#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSInstall.h"
#include "LoBBSCommandRegistry.h"
#include "LoBBSConfig.h"
#include "LoBBSHooks.h"
#include "LoBBSModule.h"
#include "apps/AppUtil.h"
#include "apps/Auth/AuthDal.h"
#include "configuration.h"
#include "mesh/NodeDB.h"
#include "mesh/Router.h"
#include <cstdio>
#include <cstring>
#include <lodb/LoDB.h>
#include <lofs/LoFS.h>
#include <loscalar/LoScalar.h>

#include "LoBBSStackGuard.h"

static LoBBSInstallState gInstallState = LoBBSInstallState::Blank;
static char gInstallRoot[32] = {0};
static char gOfflineMount[16] = {0};

LoBBSInstallState lobbsInstallState(const LoBBSModule &mod)
{
    (void)mod;
    return gInstallState;
}

const char *lobbsInstallRoot(const LoBBSModule &mod)
{
    (void)mod;
    return gInstallRoot;
}

const char *lobbsInstallOfflineMount(const LoBBSModule &mod)
{
    (void)mod;
    return gOfflineMount;
}

void lobbsInstallMountList(char *out, size_t cap)
{
    if (!out || cap == 0)
        return;
    struct Ctx {
        char *out;
        size_t cap;
        bool first;
    } c{out, cap, true};
    out[0] = '\0';
    LoFS::eachPresentMount(
        [](void *v, const char *name) {
            auto *c = (Ctx *)v;
            size_t len = strlen(c->out);
            if (!c->first && len + 1 < c->cap)
                strncat(c->out, "|", c->cap - len - 1);
            c->first = false;
            len = strlen(c->out);
            strncat(c->out, name, c->cap - len - 1);
        },
        &c);
}

bool lobbsInstallReadMarker(char *rootOut, size_t cap)
{
    if (!rootOut || cap == 0)
        return false;
    rootOut[0] = '\0';
    if (!LoFS::exists(LOBBS_INSTALL_MARKER_PATH))
        return false;

    File f = LoFS::open(LOBBS_INSTALL_MARKER_PATH, FILE_O_READ);
    if (!f)
        return false;
    size_t n = f.size();
    if (n == 0 || n > 512) {
        f.close();
        return false;
    }
    std::string buf(n, '\0');
    size_t rd = f.read((uint8_t *)&buf[0], n);
    f.close();
    if (rd != n)
        return false;
    LoScalar rec;
    if (!rec.decode(buf.c_str(), rd))
        return false;
    std::string root;
    if (!rec.getString(LOBBS_INSTALL_FIELD_ROOT, root) || root.empty() || root[0] != '/')
        return false;
    strncpy(rootOut, root.c_str(), cap - 1);
    rootOut[cap - 1] = '\0';
    return true;
}

bool lobbsInstallWriteMarker(const char *root)
{
    if (!root || root[0] != '/')
        return false;
    LoScalar rec;
    rec.setString(LOBBS_INSTALL_FIELD_ROOT, root);
    std::string line;
    if (!rec.encode(line, 256))
        return false;

    File f = LoFS::open(LOBBS_INSTALL_MARKER_PATH, FILE_O_WRITE);
    if (!f)
        return false;
    size_t w = f.write((const uint8_t *)line.data(), line.size());
    f.flush();
    f.close();
    return w == line.size();
}

static bool lobbsRootMountPresent(const char *root)
{
    if (!root || root[0] != '/')
        return false;
    const char *name = root + 1;
    return LoFS::mountPresent(name);
}

static bool lobbsTryOpenDb(LoBBSModule &mod, const char *root)
{
    LoDb *db = mod.lodb();
    if (!db)
        return false;
    return db->open(root) == LODB_OK;
}

void lobbsInstallInit(LoBBSModule &mod)
{
    gInstallState = LoBBSInstallState::Blank;
    gInstallRoot[0] = '\0';
    gOfflineMount[0] = '\0';

    char root[32];
    if (!lobbsInstallReadMarker(root, sizeof(root))) {
        LOG_INFO("LoBBS: blank (no install marker)");
        return;
    }

    if (!lobbsRootMountPresent(root)) {
        gInstallState = LoBBSInstallState::Offline;
        const char *name = LoFS::mountNameForPath(root);
        if (name)
            strncpy(gOfflineMount, name, sizeof(gOfflineMount) - 1);
        else
            strncpy(gOfflineMount, root, sizeof(gOfflineMount) - 1);
        LOG_ERROR("LoBBS offline: install root %s not mounted", root);
        return;
    }

    if (!lobbsTryOpenDb(mod, root)) {
        gInstallState = LoBBSInstallState::Offline;
        strncpy(gOfflineMount, root, sizeof(gOfflineMount) - 1);
        LOG_ERROR("LoBBS offline: failed to open database at %s", root);
        return;
    }

    strncpy(gInstallRoot, root, sizeof(gInstallRoot) - 1);
    gInstallState = LoBBSInstallState::Ready;
    lobbsInstallDatabaseOpened(mod);
    LOG_INFO("LoBBS ready at %s", root);
}

void lobbsInstallDatabaseOpened(LoBBSModule &mod)
{
    mod.auth().dal().clearSessions();
    mod.config().dal().notifyDatabaseOpened(mod);
}

bool lobbsInstallAuthorized(const meshtastic_MeshPacket &mp)
{
    if (mp.from == 0)
        return true;
#if !MESHTASTIC_EXCLUDE_PKI
    if (mp.pki_encrypted) {
        if ((config.security.admin_key[0].size == 32 &&
             memcmp(mp.public_key.bytes, config.security.admin_key[0].bytes, 32) == 0) ||
            (config.security.admin_key[1].size == 32 &&
             memcmp(mp.public_key.bytes, config.security.admin_key[1].bytes, 32) == 0) ||
            (config.security.admin_key[2].size == 32 && memcmp(mp.public_key.bytes, config.security.admin_key[2].bytes, 32) == 0))
            return true;
    }
#endif
    return false;
}

static bool lobbsInstallLocToRoot(const char *loc, char *rootOut, size_t cap)
{
    if (!loc || !rootOut)
        return false;
    if (strcmp(loc, "flash") == 0 || strcmp(loc, "sd") == 0 || strcmp(loc, "extra") == 0) {
        snprintf(rootOut, cap, "/%s", loc);
        return LoFS::mountPresent(loc);
    }
    return false;
}

static void handleInstall(LoBBSCommandCtx &ctx)
{
    if (gInstallState != LoBBSInstallState::Blank) {
        lobbsCommandReplyError(ctx, "Already installed.");
        return;
    }
    if (!lobbsInstallAuthorized(*ctx.mp)) {
        lobbsCommandReplyError(ctx, "Not authorized.");
        return;
    }

    const char *loc = lobbsArgShift(ctx);
    const char *user = lobbsArgShift(ctx);
    const char *pass = lobbsArgShift(ctx);
    if (!loc || !user || !pass || lobbsArgHasMore(ctx)) {
        lobbsCommandReplyError(ctx, "Usage: /install <flash|extra|sd> <user> <pass>");
        return;
    }

    char root[16];
    if (!lobbsInstallLocToRoot(loc, root, sizeof(root))) {
        lobbsCommandReplyError(ctx, "Mount not available.");
        return;
    }

    LoDb *db = ctx.mod->lodb();
    if (db->open(root) != LODB_OK) {
        lobbsCommandReplyError(ctx, "Failed to open database.");
        return;
    }

    AuthDal &auth = ctx.mod->auth().dal();
    lobbsInstallDatabaseOpened(*ctx.mod);
    const bool hasUsers = auth.countAllUsers() > 0;
    if (hasUsers) {
        LoScalar row;
        if (!auth.loadUserByUsername(user, &row) || !AuthDal::userIsSysop(row) || !auth.verifyPassword(&row, pass)) {
            lobbsCommandReplyError(ctx, "Existing database: sysop credentials required.");
            return;
        }
        auth.loginUser(user, getFrom(ctx.mp));
    } else if (LoDbError err = auth.createUser(user, pass, getFrom(ctx.mp), true)) {
        lobbsCommandReplyError(ctx, lobbsDbErrorText(err, "Failed to create sysop."));
        return;
    }

    if (!lobbsInstallWriteMarker(root)) {
        lobbsCommandReplyError(ctx, "Failed to write install marker.");
        return;
    }

    strncpy(gInstallRoot, root, sizeof(gInstallRoot) - 1);
    gInstallState = LoBBSInstallState::Ready;
    lobbsCommandReply(ctx, "Installed.");
}

static void slashInstall(LoBBSCommandCtx *ctx, const LoScalar &args)
{
    std::string v;
    if (!ctx || !args.getString(LOBBS_ARG_VERB, v))
        return;
    if (strcasecmp(v.c_str(), "install") != 0)
        return;
    handleInstall(*ctx);
}

void lobbsInstallRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashInstall, LOBBS_HOOK_PRIORITY_AUTH - 1);
}

#if LOBBS_SEED
void lobbsInstallAutoSeed(LoBBSModule &mod)
{
    const char *root = "/flash";
    gInstallState = LoBBSInstallState::Blank;
    gInstallRoot[0] = '\0';
    gOfflineMount[0] = '\0';
    LoFS::remove(LOBBS_INSTALL_MARKER_PATH);
    LoFS::rmdir("/flash/lodb/lobbs", true);
    if (mod.lodb()->open(root) != LODB_OK || !lobbsInstallWriteMarker(root)) {
        LOG_ERROR("LoBBS seed install failed at %s", root);
        return;
    }
    lobbsInstallDatabaseOpened(mod);
    strncpy(gInstallRoot, root, sizeof(gInstallRoot) - 1);
    gInstallState = LoBBSInstallState::Ready;
    LOG_INFO("LoBBS seed install at %s", root);
}
#endif

#endif
