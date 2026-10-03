#if !MESHTASTIC_EXCLUDE_LOBBS

#include "AuthCommands.h"
#include "../../LoBBSHooks.h"
#include "../../LoBBSCommandRegistry.h"
#include "../../LoBBSModule.h"
#include "AuthDal.h"
#include "auth.pb.h"
#include <cstdio>
#include <cstring>
#include <string>

static void handleWhoami(LoBBSCommandCtx &ctx)
{
    if (!ctx.isAuth || !ctx.user) {
        lobbsCommandReply(ctx, "Not logged in.");
        return;
    }
    char buf[96];
    snprintf(buf, sizeof(buf), "Logged in as %s.", ctx.user->username);
    if (ctx.user->is_admin) {
        size_t len = strlen(buf);
        snprintf(buf + len, sizeof(buf) - len, "\nYou are the admin.");
    }
    lobbsCommandReply(ctx, buf);
}

static void handleLogout(LoBBSCommandCtx &ctx)
{
    ctx.mod->auth().dal().logoutUser(ctx.sessionNodeId);
    lobbsCommandReply(ctx, "Goodbye!");
}

static void handleLogin(LoBBSCommandCtx &ctx)
{
    AuthDal &auth = ctx.mod->auth().dal();
    if (ctx.argc < 3) {
        lobbsCommandReply(ctx, "Usage: /login user password");
        return;
    }
    const char *username = ctx.argv[1];
    const char *password = ctx.argv[2];
    if (!auth.isValidUsername(username)) {
        lobbsCommandReply(ctx, "Invalid username.");
        return;
    }
    if (strlen(password) < 5 || !auth.isValidPassword(password)) {
        lobbsCommandReply(ctx, "Password too short.");
        return;
    }
    meshtastic_LoBBSUser dbUser = meshtastic_LoBBSUser_init_zero;
    if (auth.loadUserByUsername(username, &dbUser)) {
        if (!auth.verifyPassword(&dbUser, password)) {
            lobbsCommandReply(ctx, "Invalid password");
            return;
        }
        if (!auth.loginUser(username, ctx.sessionNodeId)) {
            lobbsCommandReply(ctx, "Error creating session");
            return;
        }
    } else if (!auth.createUser(username, password, ctx.sessionNodeId)) {
        lobbsCommandReply(ctx, "Error creating account");
        return;
    } else {
        auth.loadUserByUsername(username, &dbUser);
    }
    char buf[96];
    snprintf(buf, sizeof(buf), "Welcome %s!", username);
    if (dbUser.is_admin) {
        size_t len = strlen(buf);
        snprintf(buf + len, sizeof(buf) - len, "\nYou are the admin.");
    }
    lobbsCommandReply(ctx, buf);
}

static void usersSubList(LoBBSCommandCtx &ctx)
{
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
    if (!lobbsCommandNeedArgc(ctx, 3, "Usage: /users find text [pN]"))
        return;
    AuthDal &auth = ctx.mod->auth().dal();
    char filter[128];
    lobbsCommandJoinArgs(ctx, 2, ctx.argc, filter, sizeof(filter));
    std::string msg;
    uint32_t total = 0;
    const char *empty = nullptr;
    if (!auth.formatUserListPage(filter[0] ? filter : nullptr, ctx.page, msg, total, &empty)) {
        lobbsCommandReply(ctx, empty ? empty : "No such page.");
        return;
    }
    lobbsCommandReply(ctx, msg.c_str());
}

static void usersSubKick(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireAdmin(ctx))
        return;
    if (!lobbsCommandNeedArgc(ctx, 3, "Usage: /users kick user"))
        return;
    lobbsCommandReply(ctx, ctx.mod->auth().dal().kickUserByUsername(ctx.argv[2]) ? "Sessions cleared." : "User not found.");
}

static void usersSubPromote(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireAdmin(ctx))
        return;
    if (!lobbsCommandNeedArgc(ctx, 3, "Usage: /users promote user"))
        return;
    lobbsCommandReply(ctx, ctx.mod->auth().dal().setUserAdminByUsername(ctx.argv[2], true) ? "Promoted." : "User not found.");
}

static void usersSubDemote(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireAdmin(ctx))
        return;
    if (!lobbsCommandNeedArgc(ctx, 3, "Usage: /users demote user"))
        return;
    AuthDal &auth = ctx.mod->auth().dal();
    meshtastic_LoBBSUser target = meshtastic_LoBBSUser_init_zero;
    if (!auth.loadUserByUsername(ctx.argv[2], &target)) {
        lobbsCommandReply(ctx, "User not found.");
        return;
    }
    if (target.is_admin && auth.countAdminUsers() <= 1) {
        lobbsCommandReply(ctx, "Cannot demote last admin.");
        return;
    }
    lobbsCommandReply(ctx, auth.setUserAdminByUsername(ctx.argv[2], false) ? "Demoted." : "Failed.");
}

static const LoBBSSubcommand usersSubs[] = {
    {"list", usersSubList},
    {"find", usersSubFind},
    {"kick", usersSubKick},
    {"promote", usersSubPromote},
    {"demote", usersSubDemote},
};

static const LoBBSSubHelpEntry usersHelp[] = {
    {"list", "list [pN] — all users"},
    {"find", "find text [pN] — filter by username"},
    {"kick", "kick user — admin: clear sessions"},
    {"promote", "promote user — admin: grant admin"},
    {"demote", "demote user — admin: revoke admin"},
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

static void handleUsers(LoBBSCommandCtx &ctx)
{
    if (!lobbsCommandRequireLogin(ctx))
        return;
    if (lobbsCommandTrySubHelp(ctx, "Users Help", usersHelp, sizeof(usersHelp) / sizeof(usersHelp[0])))
        return;
    lobbsCommandDispatchSub(ctx, usersSubs, sizeof(usersSubs) / sizeof(usersSubs[0]), "list",
                            "Unknown command. Try /help users");
}

static void filterAuthCommands(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    auto *cmds = (LoBBSFilterCommands *)value;
    lobbsFilterCommandsAdd(*cmds, "whoami", handleWhoami);
    lobbsFilterCommandsAdd(*cmds, "login", handleLogin);
    lobbsFilterCommandsAdd(*cmds, "logout", handleLogout);
    lobbsFilterCommandsAdd(*cmds, "users", handleUsers);
}

static void filterAuthHelpTopicsEarly(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    auto *topics = (LoBBSFilterHelpTopics *)value;
    lobbsFilterHelpTopicAdd(*topics, "whoami", "Whoami Help", whoamiHelp, sizeof(whoamiHelp) / sizeof(whoamiHelp[0]));
    lobbsFilterHelpTopicAdd(*topics, "login", "Login Help", loginHelp, sizeof(loginHelp) / sizeof(loginHelp[0]));
    lobbsFilterHelpTopicAdd(*topics, "logout", "Logout Help", logoutHelp, sizeof(logoutHelp) / sizeof(logoutHelp[0]));
}

static void filterAuthHelpTopicsUsers(void *value, LoBBSCommandCtx *ctx)
{
    (void)ctx;
    lobbsFilterHelpTopicAdd(*(LoBBSFilterHelpTopics *)value, "users", "Users Help", usersHelp,
                            sizeof(usersHelp) / sizeof(usersHelp[0]));
}

static void filterAuthStatusLines(void *value, LoBBSCommandCtx *ctx)
{
    if (!ctx || !ctx->mod)
        return;
    char line[LOBBS_FILTER_LINE_BYTES];
    uint32_t n = ctx->mod->auth().dal().countAllUsers();
    snprintf(line, sizeof(line), "Users: %u", (unsigned)n);
    lobbsFilterLinesPush(*(LoBBSFilterLines *)value, line);
}

static void filterAuthHelpIndexEarly(void *value, LoBBSCommandCtx *ctx)
{
    if (!ctx)
        return;
    auto *index = (LoBBSFilterLines *)value;
    if (ctx->isAuth) {
        lobbsFilterLinesPush(*index, "whoami");
        lobbsFilterLinesPush(*index, "logout");
    } else {
        lobbsFilterLinesPush(*index, "login");
    }
}

static void filterAuthHelpIndexUsers(void *value, LoBBSCommandCtx *ctx)
{
    if (ctx && ctx->isAuth)
        lobbsFilterLinesPush(*(LoBBSFilterLines *)value, "users");
}

void lobbsAuthRegisterCommands()
{
    lobbsRegisterFilter("commands", filterAuthCommands);
    lobbsRegisterFilter("status_lines", filterAuthStatusLines);
    lobbsRegisterFilter("help_topics", filterAuthHelpTopicsEarly);
    lobbsRegisterFilter("help_index", filterAuthHelpIndexEarly);
}

void lobbsAuthRegisterHelpTopicsAfterApps()
{
    lobbsRegisterFilter("help_topics", filterAuthHelpTopicsUsers);
    lobbsRegisterFilter("help_index", filterAuthHelpIndexUsers);
}

#endif
