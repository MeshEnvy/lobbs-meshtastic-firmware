#if !MESHTASTIC_EXCLUDE_LOBBS

#include "FsCommands.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSReply.h"
#include "../../LoBBSReplyCache.h"
#include "../../LoBBSResponse.h"
#include <cstdio>
#include <cstring>
#include <lodb/LoDB.h>
#include <lofs/LoFS.h>
#include <string>

#include "LoBBSStackGuard.h"

static constexpr int FS_LS_MAX_NAMES = 128;
static constexpr size_t FS_NAME_BYTES = 48;

static bool fsHasGlobMeta(const char *s)
{
    for (; s && *s; s++) {
        if (*s == '*' || *s == '?')
            return true;
    }
    return false;
}

static bool fsGlobMatch(const char *pat, const char *str)
{
    if (!*pat)
        return !*str;
    if (*pat == '*') {
        if (!pat[1])
            return true;
        for (; *str; str++) {
            if (fsGlobMatch(pat + 1, str))
                return true;
        }
        return fsGlobMatch(pat + 1, str);
    }
    if (*pat == '?') {
        if (!*str)
            return false;
        return fsGlobMatch(pat + 1, str + 1);
    }
    if (*pat != *str)
        return false;
    return fsGlobMatch(pat + 1, str + 1);
}

static bool fsGlobBeforeLastName(const char *path)
{
    if (!path)
        return false;
    const char *lastSlash = strrchr(path, '/');
    const char *end = lastSlash ? lastSlash : path;
    for (const char *p = path; p < end; p++) {
        if (*p == '*' || *p == '?')
            return true;
    }
    return false;
}

static bool fsLastComponentHasGlob(const char *path)
{
    if (!path)
        return false;
    const char *lastSlash = strrchr(path, '/');
    const char *last = lastSlash ? lastSlash + 1 : path;
    return fsHasGlobMeta(last);
}

struct FsLsCollect {
    const char *pattern;
    char names[FS_LS_MAX_NAMES][FS_NAME_BYTES];
    int count;
    bool truncated;
};

/** One command at a time; ~6 KiB names[] must not live on the mesh handler stack. */
static FsLsCollect fsLsCollectScratch;

static bool fsLsCollectCb(void *ctx, const char *basename, bool isDirectory)
{
    (void)isDirectory;
    auto *c = (FsLsCollect *)ctx;
    if (!fsGlobMatch(c->pattern, basename))
        return true;
    if (c->count >= FS_LS_MAX_NAMES) {
        c->truncated = true;
        return false;
    }
    strncpy(c->names[c->count], basename, FS_NAME_BYTES - 1);
    c->names[c->count][FS_NAME_BYTES - 1] = '\0';
    c->count++;
    if (c->count >= FS_LS_MAX_NAMES) {
        c->truncated = true;
        return false;
    }
    return true;
}

struct FsMatchCtx {
    const char *pattern;
    const char *dirPath;
    char *out;
    size_t outCap;
    int count;
};

static bool fsMatchFileCb(void *ctx, const char *basename, bool isDirectory)
{
    if (isDirectory)
        return true;
    auto *mc = (FsMatchCtx *)ctx;
    if (!fsGlobMatch(mc->pattern, basename))
        return true;
    if (mc->count == 0)
        snprintf(mc->out, mc->outCap, "%s/%s", mc->dirPath, basename);
    mc->count++;
    return true;
}

/** Absolute `path` with an optional glob on the last name -> one file in `out`. Splits `path` in place while listing. */
static bool fsResolveOneFilePath(char *path, char *out, size_t outCap, LoBBSCommandCtx &ctx)
{
    if (fsGlobBeforeLastName(path)) {
        lobbsCommandReplyError(ctx, "glob only on the last name");
        return false;
    }

    if (!fsLastComponentHasGlob(path)) {
        strncpy(out, path, outCap - 1);
        out[outCap - 1] = '\0';
        return true;
    }

    char *lastSlash = strrchr(path, '/');
    FsMatchCtx m{};
    m.pattern = lastSlash + 1;
    m.dirPath = lastSlash == path ? "/" : path;
    m.out = out;
    m.outCap = outCap;
    *lastSlash = '\0';
    bool listed = LoFS::list(m.dirPath, &m, fsMatchFileCb);
    *lastSlash = '/';

    if (!listed) {
        lobbsCommandReplyError(ctx, "No such directory.");
        return false;
    }
    if (m.count == 0) {
        lobbsCommandReplyError(ctx, "No match.");
        return false;
    }
    if (m.count > 1) {
        lobbsCommandReplyError(ctx, "Many matches.");
        return false;
    }
    return true;
}

/** Append `src`'s components to out[0..len), folding `.` and `..`. False if the result does not fit. */
static bool fsFoldPath(const char *src, char *out, size_t cap, size_t &len)
{
    const char *p = src;
    while (*p) {
        while (*p == '/')
            p++;
        if (!*p)
            break;
        const char *start = p;
        while (*p && *p != '/')
            p++;
        size_t n = (size_t)(p - start);
        if (n == 1 && start[0] == '.')
            continue;
        if (n == 2 && start[0] == '.' && start[1] == '.') {
            while (len > 0 && out[len - 1] != '/')
                len--;
            if (len > 0)
                len--;
            continue;
        }
        if (len + 1 + n >= cap)
            return false;
        out[len++] = '/';
        memcpy(out + len, start, n);
        len += n;
    }
    return true;
}

/** Resolve a spec against the session cwd and fold `.` and `..`. False if the result does not fit. */
static bool fsResolvePath(const LoBBSCommandCtx &ctx, const char *spec, char *out, size_t cap)
{
    size_t len = 0;
    if (spec[0] != '/' && !fsFoldPath(ctx.session.cwd, out, cap, len))
        return false;
    if (!fsFoldPath(spec, out, cap, len))
        return false;
    if (len == 0)
        out[len++] = '/';
    out[len] = '\0';
    return true;
}

static bool fsResolveOrReply(LoBBSCommandCtx &ctx, const char *spec, char *out, size_t cap)
{
    if (fsResolvePath(ctx, spec, out, cap))
        return true;
    lobbsCommandReplyError(ctx, "Path too long.");
    return false;
}

/** rm, rmdir and rmtree never resolve against cwd. */
static bool fsResolveAbsoluteOrReply(LoBBSCommandCtx &ctx, const char *spec, char *out, size_t cap)
{
    if (spec[0] != '/') {
        lobbsCommandReplyError(ctx, "Absolute path only.");
        return false;
    }
    return fsResolveOrReply(ctx, spec, out, cap);
}

static bool fsPathWithin(const char *path, const char *ancestor)
{
    if (!path || !ancestor)
        return false;
    size_t alen = strlen(ancestor);
    if (strcmp(path, ancestor) == 0)
        return true;
    if (alen == 0 || ancestor[0] != '/')
        return false;
    if (strncmp(path, ancestor, alen) != 0)
        return false;
    return path[alen] == '/' || path[alen] == '\0';
}

static void fsSessionPrep(LoBBSCommandCtx &ctx)
{
    if (LoFS::isDirectory(ctx.session.cwd))
        return;
    if (ctx.mod->auth().dal().setSessionCwd(ctx.session.nodeId, "/"))
        strncpy(ctx.session.cwd, "/", sizeof(ctx.session.cwd));
}

static bool fsRefuseCwdTarget(LoBBSCommandCtx &ctx, const char *path, const char *verbLabel)
{
    if (!fsPathWithin(path, ctx.session.cwd))
        return false;
    (void)verbLabel;
    lobbsCommandReplyError(ctx, "Cannot remove the current directory.");
    return true;
}

static void handleLs(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);

    char spec[256];
    const char *pathTok = lobbsArgShift(ctx);
    if (!fsResolveOrReply(ctx, pathTok ? pathTok : ".", spec, sizeof(spec)))
        return;
    if (fsGlobBeforeLastName(spec)) {
        lobbsCommandReplyError(ctx, "glob only on the last name");
        return;
    }

    const char *dir = spec;
    const char *pat = "*";
    if (fsLastComponentHasGlob(spec)) {
        char *lastSlash = strrchr(spec, '/');
        pat = lastSlash + 1;
        *lastSlash = '\0';
        if (lastSlash == spec)
            dir = "/";
    }

    fsLsCollectScratch = {};
    fsLsCollectScratch.pattern = pat;
    if (!LoFS::list(dir, &fsLsCollectScratch, fsLsCollectCb)) {
        lobbsCommandReplyError(ctx, "No such directory.");
        return;
    }

    LoBBSResponse resp;
    for (int i = 0; i < fsLsCollectScratch.count; i++)
        lobbsRecordPush(resp.records, fsLsCollectScratch.names[i]);
    if (fsLsCollectScratch.truncated)
        lobbsRecordPush(resp.records, "(list cut off)");
    if (resp.records.empty()) {
        lobbsResponseSetError(resp, "Empty.");
        lobbsCommandReplyResponse(ctx, resp);
        return;
    }
    lobbsCommandReplyResponse(ctx, resp);
}

static void fsReplyFileText(LoBBSCommandCtx &ctx, const char *path, bool hex)
{
    File f = LoFS::open(path, FILE_O_READ);
    if (!f) {
        lobbsCommandReplyError(ctx, "No such file.");
        return;
    }
    if (f.isDirectory()) {
        f.close();
        lobbsCommandReplyError(ctx, "Is a directory.");
        return;
    }

    std::string body;
    body.reserve(4096);
    static constexpr size_t kMaxBody = LOBBS_REPLY_CACHE_MAX_BYTES;
    uint8_t chunk[64];
    bool truncated = false;
    while (true) {
        size_t n = f.read(chunk, sizeof(chunk));
        if (n == 0)
            break;
        if (hex) {
            for (size_t i = 0; i < n; i++) {
                if (body.size() + 3 > kMaxBody) {
                    truncated = true;
                    break;
                }
                char pair[4];
                snprintf(pair, sizeof(pair), "%02x ", chunk[i]);
                body += pair;
            }
        } else {
            size_t room = kMaxBody > body.size() ? kMaxBody - body.size() : 0;
            if (n > room) {
                body.append((const char *)chunk, room);
                truncated = true;
                break;
            }
            body.append((const char *)chunk, n);
        }
        if (truncated)
            break;
    }
    if (truncated)
        body += "[...]";
    f.close();

    LoScalar rec;
    rec.setString(LODB_F_DESCRIPTION, body);
    LoBBSResponse resp;
    lobbsResponseAppendRecord(resp, rec);
    lobbsCommandReplyResponse(ctx, resp);
}

static bool fsResolveFileArg(LoBBSCommandCtx &ctx, const char *spec, char *out, size_t cap)
{
    char full[256];
    if (!fsResolveOrReply(ctx, spec, full, sizeof(full)))
        return false;
    return fsResolveOneFilePath(full, out, cap, ctx);
}

static void handleCat(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *spec = lobbsArgShift(ctx);
    if (!spec) {
        lobbsCommandReplyError(ctx, "Usage: /cat path");
        return;
    }

    char path[256];
    if (!fsResolveFileArg(ctx, spec, path, sizeof(path)))
        return;
    fsReplyFileText(ctx, path, false);
}

static void handleHex(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *spec = lobbsArgShift(ctx);
    if (!spec) {
        lobbsCommandReplyError(ctx, "Usage: /hex path");
        return;
    }

    char path[256];
    if (!fsResolveFileArg(ctx, spec, path, sizeof(path)))
        return;
    fsReplyFileText(ctx, path, true);
}

static void handleRm(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *arg = lobbsArgShift(ctx);
    if (!arg) {
        lobbsCommandReplyError(ctx, "Usage: /rm /path");
        return;
    }
    char spec[256];
    if (!fsResolveAbsoluteOrReply(ctx, arg, spec, sizeof(spec)))
        return;
    if (fsHasGlobMeta(spec)) {
        lobbsCommandReplyError(ctx, "No glob.");
        return;
    }

    File f = LoFS::open(spec, FILE_O_READ);
    if (!f) {
        lobbsCommandReplyError(ctx, "No such file.");
        return;
    }
    if (f.isDirectory()) {
        f.close();
        lobbsCommandReplyError(ctx, "Is a directory.");
        return;
    }
    f.close();

    if (LoFS::remove(spec))
        lobbsCommandReply(ctx, "Removed.");
    else
        lobbsCommandReplyError(ctx, "Failed.");
}

static void handleRmdir(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *arg = lobbsArgShift(ctx);
    if (!arg) {
        lobbsCommandReplyError(ctx, "Usage: /rmdir /path");
        return;
    }
    char spec[256];
    if (!fsResolveAbsoluteOrReply(ctx, arg, spec, sizeof(spec)))
        return;
    if (fsHasGlobMeta(spec)) {
        lobbsCommandReplyError(ctx, "No glob.");
        return;
    }
    if (LoFS::isMountPoint(spec)) {
        lobbsCommandReplyError(ctx, "Refused.");
        return;
    }
    if (fsRefuseCwdTarget(ctx, spec, "rmdir"))
        return;

    if (!LoFS::isDirectory(spec)) {
        lobbsCommandReplyError(ctx, "No such directory.");
        return;
    }

    struct DirEmptyCtx {
        bool empty;
    } dec{true};
    auto dirHasEntry = [](void *v, const char *basename, bool isDirectory) -> bool {
        (void)isDirectory;
        (void)basename;
        ((DirEmptyCtx *)v)->empty = false;
        return false;
    };
    if (!LoFS::list(spec, &dec, dirHasEntry)) {
        lobbsCommandReplyError(ctx, "Failed.");
        return;
    }
    if (!dec.empty) {
        lobbsCommandReplyError(ctx, "Not empty.");
        return;
    }

    if (LoFS::rmdir(spec, false))
        lobbsCommandReply(ctx, "Removed.");
    else
        lobbsCommandReplyError(ctx, "Failed.");
}

static void handleRmtree(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *a = lobbsArgShift(ctx);
    const char *b = lobbsArgShift(ctx);
    if (!a || !b || strcmp(a, b) != 0 || lobbsArgHasMore(ctx)) {
        lobbsCommandReplyError(ctx, "Usage: /rmtree /path /path");
        return;
    }

    char path[256];
    if (!fsResolveAbsoluteOrReply(ctx, a, path, sizeof(path)))
        return;
    if (LoFS::isMountPoint(path)) {
        lobbsCommandReplyError(ctx, "Refused.");
        return;
    }
    if (fsRefuseCwdTarget(ctx, path, "rmtree"))
        return;
    if (fsHasGlobMeta(path)) {
        lobbsCommandReplyError(ctx, "No glob.");
        return;
    }
    if (!LoFS::exists(path)) {
        lobbsCommandReplyError(ctx, "No such directory.");
        return;
    }

    if (LoFS::rmdir(path, true))
        lobbsCommandReply(ctx, "Removed.");
    else
        lobbsCommandReplyError(ctx, "Failed.");
}

static void handleMkdir(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *arg = lobbsArgShift(ctx);
    if (!arg) {
        lobbsCommandReplyError(ctx, "Usage: /mkdir path");
        return;
    }
    char path[256];
    if (!fsResolveOrReply(ctx, arg, path, sizeof(path)))
        return;
    if (LoFS::isMountPoint(path) || strcmp(path, "/") == 0) {
        lobbsCommandReplyError(ctx, "Refused.");
        return;
    }
    if (LoFS::exists(path)) {
        lobbsCommandReplyError(ctx, "Already exists.");
        return;
    }
    char *slash = strrchr(path, '/');
    if (!slash || slash == path) {
        lobbsCommandReplyError(ctx, "No parent.");
        return;
    }
    *slash = '\0';
    bool parentIsDir = LoFS::isDirectory(path);
    *slash = '/';
    if (!parentIsDir) {
        lobbsCommandReplyError(ctx, "No such directory.");
        return;
    }
    if (LoFS::mkdir(path))
        lobbsCommandReply(ctx, "Created.");
    else
        lobbsCommandReplyError(ctx, "Failed.");
}

static void handleStat(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *arg = lobbsArgShift(ctx);
    if (!arg) {
        lobbsCommandReplyError(ctx, "Usage: /stat path");
        return;
    }
    char path[256];
    if (!fsResolveOrReply(ctx, arg, path, sizeof(path)))
        return;
    uint32_t size = 0;
    bool isDir = false;
    if (!LoFS::stat(path, &size, &isDir)) {
        lobbsCommandReplyError(ctx, "Not found.");
        return;
    }
    char line[64];
    if (isDir)
        snprintf(line, sizeof(line), "dir");
    else
        snprintf(line, sizeof(line), "file %u", (unsigned)size);
    lobbsCommandReply(ctx, line);
}

static void handleDf(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    LoBBSResponse resp;
    struct Ctx {
        LoBBSResponse *resp;
    } dc{&resp};
    LoFS::eachPresentMount(
        [](void *v, const char *name) {
            auto *dc = (Ctx *)v;
            char root[16];
            snprintf(root, sizeof(root), "/%s", name);
            uint64_t total = LoFS::totalBytes(root);
            uint64_t used = LoFS::usedBytes(root);
            char line[48];
            if (total == 0)
                snprintf(line, sizeof(line), "%s ?/? KB", name);
            else
                snprintf(line, sizeof(line), "%s %llu/%llu KB", name, (unsigned long long)(used / 1024),
                         (unsigned long long)(total / 1024));
            lobbsRecordPush(dc->resp->records, line);
        },
        &dc);
    if (resp.records.empty())
        lobbsResponseSetError(resp, "No mounts.");
    lobbsCommandReplyResponse(ctx, resp);
}

/** One command at a time; keeps two path buffers off the mesh handler stack for /cp and /mv. */
static struct {
    char src[256];
    char dst[256];
} fsPairScratch;

static void handleCp(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *srcSpec = lobbsArgShift(ctx);
    const char *dstSpec = lobbsArgShift(ctx);
    if (!srcSpec || !dstSpec || lobbsArgHasMore(ctx)) {
        lobbsCommandReplyError(ctx, "Usage: /cp src dst");
        return;
    }
    if (fsHasGlobMeta(dstSpec)) {
        lobbsCommandReplyError(ctx, "No glob on destination.");
        return;
    }
    auto &src = fsPairScratch.src;
    auto &dst = fsPairScratch.dst;
    if (!fsResolveFileArg(ctx, srcSpec, src, sizeof(src)))
        return;
    if (!fsResolveOrReply(ctx, dstSpec, dst, sizeof(dst)))
        return;
    if (LoFS::isMountPoint(src)) {
        lobbsCommandReplyError(ctx, "Refused.");
        return;
    }
    bool srcIsDir = false;
    uint32_t srcSz = 0;
    if (LoFS::stat(src, &srcSz, &srcIsDir) && srcIsDir) {
        lobbsCommandReplyError(ctx, "Is a directory.");
        return;
    }
    if (LoFS::exists(dst)) {
        lobbsCommandReplyError(ctx, "Destination exists.");
        return;
    }
    if (!LoFS::copy(src, dst)) {
        lobbsCommandReplyError(ctx, "Failed.");
        return;
    }
    lobbsCommandReply(ctx, "Copied.");
}

static void handleMv(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *srcSpec = lobbsArgShift(ctx);
    const char *dstSpec = lobbsArgShift(ctx);
    if (!srcSpec || !dstSpec || lobbsArgHasMore(ctx)) {
        lobbsCommandReplyError(ctx, "Usage: /mv src dst");
        return;
    }
    if (fsHasGlobMeta(dstSpec)) {
        lobbsCommandReplyError(ctx, "No glob on destination.");
        return;
    }
    auto &src = fsPairScratch.src;
    auto &dst = fsPairScratch.dst;
    if (!fsResolveOrReply(ctx, srcSpec, src, sizeof(src)))
        return;
    if (!fsResolveOrReply(ctx, dstSpec, dst, sizeof(dst)))
        return;
    if (LoFS::isMountPoint(src) || LoFS::isMountPoint(dst)) {
        lobbsCommandReplyError(ctx, "Refused.");
        return;
    }
    if (fsRefuseCwdTarget(ctx, src, "mv"))
        return;
    if (LoFS::exists(dst)) {
        lobbsCommandReplyError(ctx, "Destination exists.");
        return;
    }

    const char *srcMount = LoFS::mountNameForPath(src);
    const char *dstMount = LoFS::mountNameForPath(dst);
    if (srcMount && dstMount && strcmp(srcMount, dstMount) == 0) {
        if (LoFS::rename(src, dst))
            lobbsCommandReply(ctx, "Moved.");
        else
            lobbsCommandReplyError(ctx, "Failed.");
        return;
    }

    bool isDir = false;
    uint32_t sz = 0;
    if (!LoFS::stat(src, &sz, &isDir) || isDir) {
        lobbsCommandReplyError(ctx, "Cross-mount directory move not supported.");
        return;
    }
    char dstRoot[16];
    snprintf(dstRoot, sizeof(dstRoot), "/%s", dstMount ? dstMount : "flash");
    uint64_t freeB = LoFS::freeBytes(dstRoot);
    uint64_t totalB = LoFS::totalBytes(dstRoot);
    if (totalB > 0 && freeB < sz) {
        lobbsCommandReplyError(ctx, "Not enough space.");
        return;
    }
    if (!LoFS::copy(src, dst)) {
        lobbsCommandReplyError(ctx, "Failed.");
        return;
    }
    if (LoFS::remove(src))
        lobbsCommandReply(ctx, "Moved.");
    else
        lobbsCommandReplyError(ctx, "Copied, but source not removed.");
}

static void handleCd(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    const char *arg = lobbsArgShift(ctx);
    char path[LOBBS_CWD_BUFFER_SIZE];
    if (!fsResolveOrReply(ctx, arg ? arg : "/", path, sizeof(path)))
        return;
    if (fsHasGlobMeta(path)) {
        lobbsCommandReplyError(ctx, "No glob.");
        return;
    }
    if (!LoFS::isDirectory(path)) {
        lobbsCommandReplyError(ctx, "No such directory.");
        return;
    }
    if (!ctx.mod->auth().dal().setSessionCwd(ctx.session.nodeId, path)) {
        lobbsCommandReplyError(ctx, "Failed.");
        return;
    }
    memcpy(ctx.session.cwd, path, strlen(path) + 1);
    lobbsCommandReply(ctx, ctx.session.cwd);
}

static void handlePwd(LoBBSCommandCtx &ctx)
{
    fsSessionPrep(ctx);
    lobbsCommandReply(ctx, ctx.session.cwd);
}

static const LoBBSVerb fsVerbs[] = {
    {"cd", handleCd, LOBBS_V_SYSOP, "cd [path] — set working directory (default /)"},
    {"pwd", handlePwd, LOBBS_V_SYSOP, "pwd — show working directory"},
    {"ls", handleLs, LOBBS_V_SYSOP, "ls [path] — list directory (default cwd; /p2 …; * on last name only)"},
    {"cat", handleCat, LOBBS_V_SYSOP, "cat path — read a file (one match if glob)"},
    {"hex", handleHex, LOBBS_V_SYSOP, "hex path — hex dump (one match if glob)"},
    {"rm", handleRm, LOBBS_V_SYSOP, "rm /path — delete a file (absolute path)"},
    {"rmdir", handleRmdir, LOBBS_V_SYSOP, "rmdir /path — remove empty directory (absolute path)"},
    {"rmtree", handleRmtree, LOBBS_V_SYSOP, "rmtree /path /path — recursive delete (absolute path, typed twice)"},
    {"mkdir", handleMkdir, LOBBS_V_SYSOP, "mkdir path — create directory"},
    {"cp", handleCp, LOBBS_V_SYSOP, "cp src dst — copy file (no overwrite)"},
    {"mv", handleMv, LOBBS_V_SYSOP, "mv src dst — move/rename (files; dirs same mount only)"},
    {"stat", handleStat, LOBBS_V_SYSOP, "stat path — file size or dir"},
    {"df", handleDf, LOBBS_V_SYSOP, "df — space per mount"},
};

static void slashFs(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    std::string v;
    if (!ctx || !args.getString(LOBBS_ARG_VERB, v))
        return;
    LoBBSCommandCtx &c = *ctx;
    const char *verb = v.c_str();
    const size_t n = sizeof(fsVerbs) / sizeof(fsVerbs[0]);
    for (size_t i = 0; i < n; i++) {
        if (!fsVerbs[i].verb || strcasecmp(verb, fsVerbs[i].verb) != 0)
            continue;
        if ((fsVerbs[i].flags & LOBBS_V_SYSOP) && !lobbsCommandRequireSysop(c))
            return;
        fsVerbs[i].fn(c);
        return;
    }
}

static void filterFsHelpTopics(LoBBSCommandCtx *ctx, std::vector<LoScalar> &topics, const LoScalar &args)
{
    (void)args;
    if (!ctx || !lobbsCtxIsSysop(*ctx))
        return;
    lobbsRecordPush(topics, "cd", "set working directory");
    lobbsRecordPush(topics, "pwd", "show working directory");
    lobbsRecordPush(topics, "ls", "list files");
    lobbsRecordPush(topics, "cat", "print a file");
    lobbsRecordPush(topics, "hex", "hex dump a file");
    lobbsRecordPush(topics, "rm", "delete a file");
    lobbsRecordPush(topics, "rmdir", "remove an empty directory");
    lobbsRecordPush(topics, "rmtree", "remove a directory tree");
    lobbsRecordPush(topics, "mkdir", "create a directory");
    lobbsRecordPush(topics, "cp", "copy a file");
    lobbsRecordPush(topics, "mv", "move or rename");
    lobbsRecordPush(topics, "stat", "file or directory info");
    lobbsRecordPush(topics, "df", "filesystem space");
}

static void filterFsHelpForTopic(LoBBSCommandCtx *ctx, LoScalar &value, const LoScalar &args)
{
    const size_t n = sizeof(fsVerbs) / sizeof(fsVerbs[0]);
    for (size_t i = 0; i < n; i++) {
        if (!fsVerbs[i].verb || !fsVerbs[i].help)
            continue;
        lobbsHelpForTable(ctx, value, args, fsVerbs[i].verb, &fsVerbs[i], 1);
    }
}

void lobbsFsRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashFs, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_topics", filterFsHelpTopics, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("help_for_topic", filterFsHelpForTopic, LOBBS_HOOK_PRIORITY_FEATURE);
}

#endif
