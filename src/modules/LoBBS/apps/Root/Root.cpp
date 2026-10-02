#if !MESHTASTIC_EXCLUDE_LOBBS

#include "Root.h"
#include "../AppUtil.h"
#include "../Mail/Mail.h"
#include "../News/News.h"
#include "../Auth/AuthDal.h"
#include "../Auth/Auth.h"
#include "../../LoBBSModule.h"
#include "../../LoBBSPaging.h"
#include "../../LoBBSVersion.h"
#include <cstdio>
#include <cstring>

static void rootWhoami(LobbsHistory *h, const LobbsFrame *)
{
    if (!h->ctx->isAuth || !h->ctx->user) {
        lobbsHistoryReply(h, "Not logged in.");
        h->drew = false;
        return;
    }
    char buf[96];
    snprintf(buf, sizeof(buf), "Logged in as %s.", h->ctx->user->username);
    if (h->ctx->user->is_admin) {
        size_t len = strlen(buf);
        snprintf(buf + len, sizeof(buf) - len, "\nYou are the admin.");
    }
    lobbsHistoryReply(h, buf);
    h->drew = false;
}

static void rootLogout(LobbsHistory *h, const LobbsFrame *)
{
    h->ctx->mod->auth().dal().logoutUser(h->ctx->sessionNodeId);
    h->ctx->isAuth = false;
    h->ctx->isAdmin = false;
    h->ctx->user = nullptr;
    lobbsPageClearUser(h->ctx->sessionNodeId);
    lobbsRootInstall(h);
    lobbsHistoryReply(h, "Goodbye!");
    h->drew = false;
}

static void loginPassOk(LobbsHistory *h, const LobbsFrame *, const char *line);
static void rootLogin(LobbsHistory *h, const LobbsFrame *);

static void loginUserOk(LobbsHistory *h, const LobbsFrame *, const char *line)
{
    snprintf(h->scratch, sizeof(h->scratch), "%s", line);
    lobbsHistoryPop(h);
    LobbsFrame pass = {};
    pass.kind = LobbsFrameKind::Prompt;
    pass.tag = "/login/pass";
    pass.prompt = "Password?";
    pass.draw = lobbsAppDrawPrompt;
    pass.onSuccess = loginPassOk;
    lobbsHistoryPush(h, pass);
}

static void rootLogin(LobbsHistory *h, const LobbsFrame *)
{
    LobbsFrame user = {};
    user.kind = LobbsFrameKind::Prompt;
    user.tag = "/login";
    user.prompt = "Username?";
    user.draw = lobbsAppDrawPrompt;
    user.onSuccess = loginUserOk;
    lobbsHistoryPush(h, user);
}

static void loginPassOk(LobbsHistory *h, const LobbsFrame *, const char *line)
{
    snprintf(h->scratch2, sizeof(h->scratch2), "%s", line);
    const char *username = h->scratch;
    const char *password = h->scratch2;
    AuthDal &auth = h->ctx->mod->auth().dal();
    if (!auth.isValidUsername(username)) {
        lobbsHistoryReply(h, "Invalid username.");
        h->drew = false;
        return;
    }
    if (strlen(password) < 5) {
        lobbsHistoryReply(h, "Password too short.");
        h->drew = false;
        return;
    }
    meshtastic_LoBBSUser dbUser = meshtastic_LoBBSUser_init_zero;
    if (auth.loadUserByUsername(username, &dbUser)) {
        if (!auth.verifyPassword(&dbUser, password)) {
            lobbsHistoryReply(h, "Invalid password");
            h->scratch[0] = '\0';
            lobbsHistoryPop(h);
            rootLogin(h, nullptr);
            h->drew = false;
            return;
        }
        if (!auth.loginUser(username, h->ctx->sessionNodeId)) {
            lobbsHistoryReply(h, "Error creating session");
            return;
        }
    } else if (!auth.createUser(username, password, h->ctx->sessionNodeId)) {
        lobbsHistoryReply(h, "Error creating account");
        return;
    } else {
        auth.loadUserByUsername(username, &dbUser);
    }
    h->ctx->isAuth = true;
    h->ctx->isAdmin = dbUser.is_admin;
    lobbsRootInstall(h);
    char buf[96];
    snprintf(buf, sizeof(buf), "Welcome %s!", username);
    if (dbUser.is_admin) {
        size_t len = strlen(buf);
        snprintf(buf + len, sizeof(buf) - len, "\nYou are the admin.");
    }
    lobbsHistoryReply(h, buf);
    h->drew = false;
}

static void drawRoot(LobbsHistory *h, const LobbsFrame *self)
{
    char buf[180];
    size_t n = 0;
    int w = snprintf(buf, sizeof(buf), "LoBBS v%s\n", LOBBS_VERSION_SHORT);
    if (w > 0)
        n = (size_t)w;
    for (uint8_t i = 0; i < self->itemCount && n + 1 < sizeof(buf); i++)
        lobbsAppAppendMenuLine(h, buf, sizeof(buf), &n, i, &self->items[i]);
    snprintf(buf + n, sizeof(buf) - n, "? < << p");
    lobbsHistoryReply(h, buf);
}

static void rootMail(LobbsHistory *h, const LobbsFrame *)
{
    lobbsMailPush(h);
}
static void rootNews(LobbsHistory *h, const LobbsFrame *)
{
    lobbsNewsPush(h);
}
static void rootUsers(LobbsHistory *h, const LobbsFrame *)
{
    lobbsAuthPush(h);
}

void lobbsRootInstall(LobbsHistory *h)
{
    LobbsFrame f = {};
    f.kind = LobbsFrameKind::Menu;
    f.tag = "/";
    f.draw = drawRoot;
    if (h->ctx && h->ctx->isAuth) {
        f.items[0] = {"Mail", rootMail, lobbsMailItemStatus};
        f.items[1] = {"News", rootNews, lobbsNewsItemStatus};
        f.items[2] = {"Users", rootUsers, nullptr};
        f.items[3] = {"Who am I", rootWhoami, nullptr};
        f.items[4] = {"Logout", rootLogout, nullptr};
        f.itemCount = 5;
    } else {
        f.items[0] = {"Login", rootLogin, nullptr};
        f.items[1] = {"Who am I", rootWhoami, nullptr};
        f.itemCount = 2;
    }
    h->depth = 0;
    h->scratch[0] = '\0';
    h->scratch2[0] = '\0';
    lobbsHistoryPush(h, f);
}

#endif
