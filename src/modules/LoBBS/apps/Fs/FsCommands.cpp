#if !MESHTASTIC_EXCLUDE_LOBBS

#include "FsCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSReply.h"
#include <lofs/LoFS.h>
#include <cstdio>
#include <cstring>

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

static void fsTrimTrailingSlash(char *dir)
{
    size_t len = strlen(dir);
    while (len > 1 && dir[len - 1] == '/') {
        dir[len - 1] = '\0';
        len--;
    }
}

static void fsSplitDirPattern(const char *path, char *dir, size_t dirCap, char *pat, size_t patCap)
{
    if (!path || !path[0] || (path[0] == '*' && path[1] == '\0')) {
        snprintf(dir, dirCap, "/");
        snprintf(pat, patCap, "*");
        return;
    }

    if (!fsLastComponentHasGlob(path)) {
        strncpy(dir, path, dirCap - 1);
        dir[dirCap - 1] = '\0';
        fsTrimTrailingSlash(dir);
        if (dir[0] == '\0') {
            dir[0] = '/';
            dir[1] = '\0';
        }
        snprintf(pat, patCap, "*");
        return;
    }

    const char *lastSlash = strrchr(path, '/');
    if (!lastSlash) {
        snprintf(dir, dirCap, "/");
        strncpy(pat, path, patCap - 1);
        pat[patCap - 1] = '\0';
        return;
    }

    if (lastSlash == path) {
        snprintf(dir, dirCap, "/");
        strncpy(pat, path + 1, patCap - 1);
        pat[patCap - 1] = '\0';
        return;
    }

    size_t dirLen = (size_t)(lastSlash - path);
    if (dirLen >= dirCap)
        dirLen = dirCap - 1;
    memcpy(dir, path, dirLen);
    dir[dirLen] = '\0';
    dir[dirCap - 1] = '\0';
    strncpy(pat, lastSlash + 1, patCap - 1);
    pat[patCap - 1] = '\0';
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
    char dirPath[256];
    char filePath[256];
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
        snprintf(mc->filePath, sizeof(mc->filePath), "%s/%s", mc->dirPath, basename);
    mc->count++;
    return true;
}

static bool fsResolveOneFilePath(const char *spec, char *dir, size_t dirCap, char *outPath, size_t outCap,
                                 LoBBSCommandCtx &ctx)
{
    if (fsGlobBeforeLastName(spec)) {
        lobbsCommandReply(ctx, "glob only on the last name");
        return false;
    }

    if (!fsLastComponentHasGlob(spec)) {
        strncpy(outPath, spec, outCap - 1);
        outPath[outCap - 1] = '\0';
        return true;
    }

    char pat[64];
    fsSplitDirPattern(spec, dir, dirCap, pat, sizeof(pat));

    FsMatchCtx m{};
    m.pattern = pat;
    strncpy(m.dirPath, dir, sizeof(m.dirPath) - 1);
    m.dirPath[sizeof(m.dirPath) - 1] = '\0';

    if (!LoFS::list(dir, &m, fsMatchFileCb)) {
        lobbsCommandReply(ctx, "No such directory.");
        return false;
    }
    if (m.count == 0) {
        lobbsCommandReply(ctx, "No match.");
        return false;
    }
    if (m.count > 1) {
        lobbsCommandReply(ctx, "Many matches.");
        return false;
    }
    strncpy(outPath, m.filePath, outCap - 1);
    outPath[outCap - 1] = '\0';
    return true;
}

static bool fsPathHasAnyGlob(const char *path)
{
    return fsHasGlobMeta(path);
}

static bool fsIsProtectedRmtreeRoot(const char *path)
{
    return strcmp(path, "/") == 0 || strcmp(path, "/internal") == 0 || strcmp(path, "/internal/") == 0 ||
           strcmp(path, "/sd") == 0 || strcmp(path, "/sd/") == 0;
}

static void handleLs(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireSysop(ctx))
        return;

    const char *spec = "/";
    const char *pathTok = lobbsArgPeek(ctx);
    if (pathTok && !lobbsTokenIsPage(pathTok))
        spec = lobbsArgShift(ctx);
    lobbsArgTakePage(ctx);

    if (fsGlobBeforeLastName(spec)) {
        lobbsCommandReply(ctx, "glob only on the last name");
        return;
    }

    char dir[256];
    char pat[64];
    fsSplitDirPattern(spec, dir, sizeof(dir), pat, sizeof(pat));

    fsLsCollectScratch = {};
    fsLsCollectScratch.pattern = pat;
    if (!LoFS::list(dir, &fsLsCollectScratch, fsLsCollectCb)) {
        lobbsCommandReply(ctx, "No such directory.");
        return;
    }

    const char *linePtrs[FS_LS_MAX_NAMES + 1];
    int lineCount = fsLsCollectScratch.count;
    for (int i = 0; i < fsLsCollectScratch.count; i++)
        linePtrs[i] = fsLsCollectScratch.names[i];
    if (fsLsCollectScratch.truncated && lineCount < FS_LS_MAX_NAMES) {
        linePtrs[lineCount] = "(list cut off)";
        lineCount++;
    }

    char buf[LOBBS_REPLY_BYTES + 1];
    const char *errEmpty = nullptr;
    const char *errBadPage = nullptr;
    if (!lobbsPagerFormatLines(buf, sizeof(buf), ctx.page, linePtrs, (uint32_t)lineCount, &errEmpty, &errBadPage)) {
        lobbsCommandReply(ctx, errEmpty ? errEmpty : (errBadPage ? errBadPage : "No such page."));
        return;
    }
    lobbsCommandReply(ctx, buf);
}

static void fsReplyFileText(LoBBSCommandCtx &ctx, const char *path, bool hex)
{
    File f = LoFS::open(path, FILE_O_READ);
    if (!f) {
        lobbsCommandReply(ctx, "No such file.");
        return;
    }
    if (f.isDirectory()) {
        f.close();
        lobbsCommandReply(ctx, "Is a directory.");
        return;
    }

    char buf[LOBBS_REPLY_BYTES + 1];
    size_t cap = sizeof(buf);
    if (hex)
        cap = LOBBS_REPLY_BYTES - 12;

    size_t out = 0;
    bool truncated = false;
    uint8_t chunk[32];

    if (hex) {
        while (true) {
            size_t n = f.read(chunk, sizeof(chunk));
            if (n == 0)
                break;
            for (size_t i = 0; i < n; i++) {
                if (out + 3 >= cap) {
                    truncated = true;
                    break;
                }
                int w = snprintf(buf + out, cap - out + 1, "%02x ", chunk[i]);
                if (w > 0)
                    out += (size_t)w;
            }
            if (truncated)
                break;
        }
    } else {
        while (true) {
            size_t n = f.read((uint8_t *)buf + out, cap - out);
            if (n == 0)
                break;
            out += n;
            if (out >= cap) {
                truncated = true;
                out = cap;
                break;
            }
        }
    }
    f.close();

    buf[out] = '\0';
    if (truncated) {
        size_t suffixLen = strlen(" truncated");
        if (out + suffixLen >= sizeof(buf))
            out = sizeof(buf) - suffixLen - 1;
        strcpy(buf + out, " truncated");
    }
    lobbsCommandReply(ctx, buf);
}

static void handleCat(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireSysop(ctx))
        return;
    const char *spec = lobbsArgShift(ctx);
    if (!spec) {
        lobbsCommandReply(ctx, "Usage: /cat path");
        return;
    }

    char dir[256];
    char path[256];
    if (!fsResolveOneFilePath(spec, dir, sizeof(dir), path, sizeof(path), ctx))
        return;
    fsReplyFileText(ctx, path, false);
}

static void handleHex(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireSysop(ctx))
        return;
    const char *spec = lobbsArgShift(ctx);
    if (!spec) {
        lobbsCommandReply(ctx, "Usage: /hex path");
        return;
    }

    char dir[256];
    char path[256];
    if (!fsResolveOneFilePath(spec, dir, sizeof(dir), path, sizeof(path), ctx))
        return;
    fsReplyFileText(ctx, path, true);
}

static void handleRm(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireSysop(ctx))
        return;
    const char *spec = lobbsArgShift(ctx);
    if (!spec) {
        lobbsCommandReply(ctx, "Usage: /rm path");
        return;
    }
    if (fsPathHasAnyGlob(spec)) {
        lobbsCommandReply(ctx, "No glob.");
        return;
    }

    File f = LoFS::open(spec, FILE_O_READ);
    if (!f) {
        lobbsCommandReply(ctx, "No such file.");
        return;
    }
    if (f.isDirectory()) {
        f.close();
        lobbsCommandReply(ctx, "Is a directory.");
        return;
    }
    f.close();

    lobbsCommandReply(ctx, LoFS::remove(spec) ? "Removed." : "Failed.");
}

static void handleRmdir(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireSysop(ctx))
        return;
    const char *spec = lobbsArgShift(ctx);
    if (!spec) {
        lobbsCommandReply(ctx, "Usage: /rmdir path");
        return;
    }
    if (fsPathHasAnyGlob(spec)) {
        lobbsCommandReply(ctx, "No glob.");
        return;
    }

    if (!LoFS::exists(spec)) {
        lobbsCommandReply(ctx, "No such directory.");
        return;
    }

    File probe = LoFS::open(spec, FILE_O_READ);
    if (!probe) {
        lobbsCommandReply(ctx, "Failed.");
        return;
    }
    if (!probe.isDirectory()) {
        probe.close();
        lobbsCommandReply(ctx, "Not a directory.");
        return;
    }
    probe.close();

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
        lobbsCommandReply(ctx, "Failed.");
        return;
    }
    if (!dec.empty) {
        lobbsCommandReply(ctx, "Not empty.");
        return;
    }

    lobbsCommandReply(ctx, LoFS::rmdir(spec, false) ? "Removed." : "Failed.");
}

static void handleRmtree(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireSysop(ctx))
        return;
    const char *a = lobbsArgShift(ctx);
    const char *b = lobbsArgShift(ctx);
    if (!a || !b || strcmp(a, b) != 0 || lobbsArgHasMore(ctx)) {
        lobbsCommandReply(ctx, "Usage: /rmtree path path");
        return;
    }

    const char *path = a;
    if (fsIsProtectedRmtreeRoot(path)) {
        lobbsCommandReply(ctx, "Refused.");
        return;
    }
    if (fsPathHasAnyGlob(path)) {
        lobbsCommandReply(ctx, "No glob.");
        return;
    }
    if (!LoFS::exists(path)) {
        lobbsCommandReply(ctx, "No such directory.");
        return;
    }

    lobbsCommandReply(ctx, LoFS::rmdir(path, true) ? "Removed." : "Failed.");
}

static const LoBBSSubHelpEntry fsHelp[] = {
    {"ls", "ls [path] [pN] — list directory (* on last name only)"},
    {"cat", "cat path — read a file (one match if glob)"},
    {"hex", "hex path — hex dump (one match if glob)"},
    {"rm", "rm path — delete a file"},
    {"rmdir", "rmdir path — remove empty directory"},
    {"rmtree", "rmtree path path — recursive delete (type path twice)"},
};

static void slashFs(LoBBSCommandCtx *ctx, const char *verb, const char *rest)
{
    if (!ctx || !verb)
        return;
    LoBBSCommandCtx &c = *ctx;
    if (rest)
        c.rest = (char *)rest;
    if (strcasecmp(verb, "ls") == 0) {
        handleLs(c);
        return;
    }
    if (strcasecmp(verb, "cat") == 0) {
        handleCat(c);
        return;
    }
    if (strcasecmp(verb, "hex") == 0) {
        handleHex(c);
        return;
    }
    if (strcasecmp(verb, "rm") == 0) {
        handleRm(c);
        return;
    }
    if (strcasecmp(verb, "rmdir") == 0) {
        handleRmdir(c);
        return;
    }
    if (strcasecmp(verb, "rmtree") == 0) {
        handleRmtree(c);
        return;
    }
}

static void filterFsRootCommands(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    if (query || !ctx || !lobbsCtxLoggedIn(*ctx) || !ctx->session.isSysop)
        return;
    lobbsRootCommandPush(lines, "ls");
    lobbsRootCommandPush(lines, "cat");
    lobbsRootCommandPush(lines, "hex");
    lobbsRootCommandPush(lines, "rm");
    lobbsRootCommandPush(lines, "rmdir");
    lobbsRootCommandPush(lines, "rmtree");
}

static void filterFsCommandHelp(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    (void)ctx;
    if (!query)
        return;
    for (size_t i = 0; i < sizeof(fsHelp) / sizeof(fsHelp[0]); i++) {
        if (fsHelp[i].verb && strcasecmp(query, fsHelp[i].verb) == 0 && fsHelp[i].line)
            lines.push_back(fsHelp[i].line);
    }
}

void lobbsFsRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashFs, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("root_commands", filterFsRootCommands, LOBBS_HOOK_PRIORITY_FEATURE);
    lobbsAddFilter("command_help", filterFsCommandHelp, LOBBS_HOOK_PRIORITY_FEATURE);
}

#endif
