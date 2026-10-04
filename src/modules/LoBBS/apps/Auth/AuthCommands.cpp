#if !MESHTASTIC_EXCLUDE_LOBBS

#include "AuthCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSModule.h"
#include "AuthDal.h"
#include "mesh/NodeDB.h"
#include <cstdio>
#include <cstring>
#include <string>

static void handleWhoami(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireLogin(ctx))
        return;
    char buf[96];
    char uname[LOBBS_USERNAME_BUFFER_SIZE];
    lobbsCtxUsername(ctx, uname, sizeof(uname));
    snprintf(buf, sizeof(buf), "Logged in as %s.", uname);
    if (ctx.session.isSysop) {
        size_t len = strlen(buf);
        snprintf(buf + len, sizeof(buf) - len, "\nYou are the SysOp.");
    }
    lobbsCommandReply(ctx, buf);
}

static void handleLogout(LoBBSCommandCtx &ctx)
{
    ctx.mod->auth().dal().logoutUser(ctx.session.nodeId);
    lobbsCommandReply(ctx, "Goodbye!");
}

static void handleLogin(LoBBSCommandCtx &ctx)
{
    AuthDal &auth = ctx.mod->auth().dal();
    const char *username = lobbsArgShift(ctx);
    const char *password = lobbsArgShift(ctx);
    if (!username || !password) {
        lobbsCommandReply(ctx, "Usage: /login user password");
        return;
    }
    if (!auth.isValidUsername(username)) {
        lobbsCommandReply(ctx, "Invalid username.");
        return;
    }
    if (strlen(password) < 5 || !auth.isValidPassword(password)) {
        lobbsCommandReply(ctx, "Password too short.");
        return;
    }
    LoScalar dbUser;
    if (auth.loadUserByUsername(username, &dbUser)) {
        if (!auth.verifyPassword(&dbUser, password)) {
            lobbsCommandReply(ctx, "Invalid password");
            return;
        }
        const uint32_t sessionKey = getFrom(ctx.mp);
        if (!auth.loginUser(username, sessionKey)) {
            lobbsCommandReply(ctx, "Error creating session");
            return;
        }
    } else if (!auth.createUser(username, password, getFrom(ctx.mp))) {
        lobbsCommandReply(ctx, "Error creating account");
        return;
    } else {
        auth.loadUserByUsername(username, &dbUser);
    }
    char buf[96];
    snprintf(buf, sizeof(buf), "Welcome %s!", username);
    if (AuthDal::userIsSysop(dbUser)) {
        size_t len = strlen(buf);
        snprintf(buf + len, sizeof(buf) - len, "\nYou are the SysOp.");
    }
    lobbsCommandReply(ctx, buf);
}

static bool passwdApply(AuthDal &auth, const char *username, const char *newPass, const char *confirm, LoBBSCommandCtx &ctx)
{
    if (strcmp(newPass, confirm) != 0) {
        lobbsCommandReply(ctx, "Passwords do not match.");
        return false;
    }
    if (strlen(newPass) < 5 || !auth.isValidPassword(newPass)) {
        lobbsCommandReply(ctx, "Password too short.");
        return false;
    }
    if (!auth.setPasswordByUsername(username, newPass)) {
        lobbsCommandReply(ctx, "Error updating password.");
        return false;
    }
    lobbsCommandReply(ctx, "Password updated.");
    return true;
}

static void handlePasswd(LoBBSCommandCtx &ctx)
{
    AuthDal &auth = ctx.mod->auth().dal();
    const char *a = lobbsArgShift(ctx);
    const char *b = lobbsArgShift(ctx);
    const char *c = lobbsArgShift(ctx);
    if (a && b && !c) {
        if (!lobbsCommandRequireLogin(ctx))
            return;
        char uname[LOBBS_USERNAME_BUFFER_SIZE];
        lobbsCtxUsername(ctx, uname, sizeof(uname));
        passwdApply(auth, uname, a, b, ctx);
        return;
    }
    if (a && b && c) {
        if (!lobbsCommandRequireSysop(ctx))
            return;
        if (!auth.isValidUsername(a)) {
            lobbsCommandReply(ctx, "Invalid username.");
            return;
        }
        LoScalar dbUser;
        if (!auth.loadUserByUsername(a, &dbUser)) {
            lobbsCommandReply(ctx, "User not found.");
            return;
        }
        passwdApply(auth, a, b, c, ctx);
        return;
    }
    lobbsCommandReply(ctx, "Usage: /passwd new confirm\nUsage: /passwd user new confirm (sysop)");
}

static void usersSubList(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgPeek(ctx);
    if (sub && strcasecmp(sub, "list") == 0)
        lobbsArgShift(ctx);
    lobbsArgTakePage(ctx);
    AuthDal &auth = ctx.mod->auth().dal();
    std::string msg;
    uint32_t total = 0;
    const char *empty = nullptr;
    if (!auth.formatUserListPage(nullptr, ctx.page, msg, total, &empty)) {
        lobbsCommandReply(ctx, empty ? empty : "No such page.");
        return;
    }
    lobbsCommandReply(ctx, msg.c_str());
}

static void usersSubFind(LoBBSCommandCtx &ctx)
{
    const char *sub = lobbsArgShift(ctx);
    if (!sub || strcasecmp(sub, "find") != 0) {
        lobbsCommandReply(ctx, "Usage: /users find text [pN]");
        return;
    }
    char filterBuf[128];
    strncpy(filterBuf, lobbsArgRest(ctx), sizeof(filterBuf) - 1);
    filterBuf[sizeof(filterBuf) - 1] = '\0';
    uint32_t page = ctx.page;
    lobbsHelpStripTrailingPage(filterBuf, sizeof(filterBuf), page);
    ctx.page = page;

    AuthDal &auth = ctx.mod->auth().dal();
    std::string msg;
    uint32_t total = 0;
    const char *empty = nullptr;
    if (!auth.formatUserListPage(filterBuf[0] ? filterBuf : nullptr, ctx.page, msg, total, &empty)) {
        lobbsCommandReply(ctx, empty ? empty : "No such page.");
        return;
    }
    lobbsCommandReply(ctx, msg.c_str());
}

static void usersSubKick(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireSysop(ctx))
        return;
    const char *sub = lobbsArgShift(ctx);
    const char *user = lobbsArgShift(ctx);
    if (!sub || strcasecmp(sub, "kick") != 0 || !user) {
        lobbsCommandReply(ctx, "Usage: /users kick user");
        return;
    }
    lobbsCommandReply(ctx, ctx.mod->auth().dal().kickUserByUsername(user) ? "Sessions cleared." : "User not found.");
}

static void usersSubPromote(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireSysop(ctx))
        return;
    const char *sub = lobbsArgShift(ctx);
    const char *user = lobbsArgShift(ctx);
    if (!sub || strcasecmp(sub, "promote") != 0 || !user) {
        lobbsCommandReply(ctx, "Usage: /users promote user");
        return;
    }
    lobbsCommandReply(ctx, ctx.mod->auth().dal().setUserSysopByUsername(user, true) ? "Promoted." : "User not found.");
}

static void usersSubDemote(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireSysop(ctx))
        return;
    const char *sub = lobbsArgShift(ctx);
    const char *user = lobbsArgShift(ctx);
    if (!sub || strcasecmp(sub, "demote") != 0 || !user) {
        lobbsCommandReply(ctx, "Usage: /users demote user");
        return;
    }
    AuthDal &auth = ctx.mod->auth().dal();
    LoScalar target;
    if (!auth.loadUserByUsername(user, &target)) {
        lobbsCommandReply(ctx, "User not found.");
        return;
    }
    if (AuthDal::userIsSysop(target) && auth.countSysopUsers() <= 1) {
        lobbsCommandReply(ctx, "Cannot demote last SysOp.");
        return;
    }
    lobbsCommandReply(ctx, auth.setUserSysopByUsername(user, false) ? "Demoted." : "Failed.");
}

static const LoBBSSubHelpEntry usersHelp[] = {
    {"list", "list [pN] — all users"},
    {"find", "find text [pN] — filter by username"},
    {"kick", "kick user — sysop: clear sessions"},
    {"promote", "promote user — sysop: grant sysop"},
    {"demote", "demote user — sysop: revoke sysop"},
};

static const LoBBSSubHelpEntry whoamiHelp[] = {
    {"whoami", "whoami — show logged-in user"},
};

static const LoBBSSubHelpEntry loginHelp[] = {
    {"login", "login user pass — sign in or create account (pass min 5 chars)"},
};

static const LoBBSSubHelpEntry logoutHelp[] = {
    {"logout", "logout — end session"},
};

static const LoBBSSubHelpEntry passwdHelp[] = {
    {"passwd", "passwd new confirm — change your password"},
    {"passwd", "passwd user new confirm — sysop: reset user password"},
};

static void handleUsers(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireLogin(ctx))
        return;
    const char *sub = lobbsArgPeek(ctx);
    if (!sub || strcasecmp(sub, "list") == 0) {
        usersSubList(ctx);
        return;
    }
    if (strcasecmp(sub, "find") == 0) {
        usersSubFind(ctx);
        return;
    }
    if (strcasecmp(sub, "kick") == 0) {
        usersSubKick(ctx);
        return;
    }
    if (strcasecmp(sub, "promote") == 0) {
        usersSubPromote(ctx);
        return;
    }
    if (strcasecmp(sub, "demote") == 0) {
        usersSubDemote(ctx);
        return;
    }
    lobbsCommandReply(ctx, "Unknown command. Try /help users");
}

static void slashAuth(LoBBSCommandCtx *ctx, const char *verb, const char *rest)
{
    if (!ctx || !verb)
        return;
    LoBBSCommandCtx &c = *ctx;
    if (rest)
        c.rest = (char *)rest;
    if (strcasecmp(verb, "whoami") == 0) {
        handleWhoami(c);
        return;
    }
    if (strcasecmp(verb, "login") == 0) {
        handleLogin(c);
        return;
    }
    if (strcasecmp(verb, "logout") == 0) {
        handleLogout(c);
        return;
    }
    if (strcasecmp(verb, "passwd") == 0) {
        handlePasswd(c);
        return;
    }
    if (strcasecmp(verb, "users") == 0) {
        handleUsers(c);
        return;
    }
}

static void filterAuthRootCommands(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    if (query || !ctx)
        return;
    if (lobbsCtxLoggedIn(*ctx)) {
        lobbsRootCommandPush(lines, "whoami");
        lobbsRootCommandPush(lines, "logout");
        lobbsRootCommandPush(lines, "passwd");
        lobbsRootCommandPush(lines, "users");
    } else {
        lobbsRootCommandPush(lines, "login");
    }
}

static void filterAuthCommandHelp(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    (void)ctx;
    lobbsCommandHelpPush(lines, "whoami", whoamiHelp, sizeof(whoamiHelp) / sizeof(whoamiHelp[0]), query);
    lobbsCommandHelpPush(lines, "login", loginHelp, sizeof(loginHelp) / sizeof(loginHelp[0]), query);
    lobbsCommandHelpPush(lines, "logout", logoutHelp, sizeof(logoutHelp) / sizeof(logoutHelp[0]), query);
    lobbsCommandHelpPush(lines, "passwd", passwdHelp, sizeof(passwdHelp) / sizeof(passwdHelp[0]), query);
    lobbsCommandHelpPush(lines, "users", usersHelp, sizeof(usersHelp) / sizeof(usersHelp[0]), query);
}

static void filterAuthStatusLines(LoBBSCommandCtx *ctx, std::vector<std::string> &lines, const char *query)
{
    (void)query;
    if (!ctx || !ctx->mod)
        return;
    char line[96];
    uint32_t n = ctx->mod->auth().dal().countAllUsers();
    if (lobbsCtxLoggedIn(*ctx))
        snprintf(line, sizeof(line), "Users: %u", (unsigned)n);
    else
        snprintf(line, sizeof(line), "Users: %u (all time)", (unsigned)n);
    lines.push_back(line);
}

void lobbsAuthRegisterCommands()
{
    lobbsAddAction("slash_cmd", slashAuth, LOBBS_HOOK_PRIORITY_AUTH);
    lobbsAddFilter("root_commands", filterAuthRootCommands, LOBBS_HOOK_PRIORITY_AUTH);
    lobbsAddFilter("command_help", filterAuthCommandHelp, LOBBS_HOOK_PRIORITY_AUTH);
    lobbsAddFilter("status_lines", filterAuthStatusLines, LOBBS_HOOK_PRIORITY_AUTH);
}

#endif
