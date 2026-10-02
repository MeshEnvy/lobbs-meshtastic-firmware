#if !MESHTASTIC_EXCLUDE_LOBBS

#include "Auth.h"
#include "../AppUtil.h"
#include "../../LoBBSModule.h"
#include <cstring>
#include <string>

AuthApp::AuthApp(LoDb &lodb) : dal_(lodb) {}

static AuthDal &authOf(LobbsHistory *h)
{
    return h->ctx->mod->auth().dal();
}

static void drawUserList(LobbsHistory *h, const LobbsFrame *self)
{
    const char *filter = h->scratch[0] ? h->scratch : nullptr;
    std::string msg;
    const char *emptyReply = nullptr;
    if (!authOf(h).buildUserList(filter, msg, &emptyReply)) {
        lobbsHistoryReply(h, emptyReply);
        return;
    }
    msg += LOBBS_HIST_NAV;
    h->ctx->mod->sendPagedReply(h->ctx->sessionNodeId, *h->ctx->mp, msg.c_str());
    h->drew = true;
    (void)self;
}

static void usersList(LobbsHistory *h, const LobbsFrame *)
{
    h->scratch[0] = '\0';
    LobbsFrame f = {};
    f.kind = LobbsFrameKind::Menu;
    f.tag = "/users/list";
    f.draw = drawUserList;
    f.onDigit = nullptr;
    lobbsHistoryPush(h, f);
}

static void usersFilterOk(LobbsHistory *h, const LobbsFrame *, const char *line)
{
    snprintf(h->scratch, sizeof(h->scratch), "%s", line);
    lobbsHistoryPop(h);
    LobbsFrame f = {};
    f.kind = LobbsFrameKind::Menu;
    f.tag = "/users/list";
    f.draw = drawUserList;
    lobbsHistoryPush(h, f);
}

static void usersFilter(LobbsHistory *h, const LobbsFrame *)
{
    LobbsFrame n = {};
    n.kind = LobbsFrameKind::Prompt;
    n.tag = "/users/filter";
    n.prompt = "Filter?";
    n.draw = lobbsAppDrawPrompt;
    n.onSuccess = usersFilterOk;
    lobbsHistoryPush(h, n);
}

static void usersActOk(LobbsHistory *h, const LobbsFrame *self, const char *line)
{
    AuthDal &auth = authOf(h);
    uint32_t act = self->arg1;
    if (act == 1) {
        if (auth.kickUserByUsername(line))
            lobbsHistoryReply(h, "Sessions cleared.");
        else
            lobbsHistoryReply(h, "User not found.");
    } else if (act == 2) {
        if (auth.setUserAdminByUsername(line, true))
            lobbsHistoryReply(h, "Promoted.");
        else
            lobbsHistoryReply(h, "User not found.");
    } else {
        meshtastic_LoBBSUser target = meshtastic_LoBBSUser_init_zero;
        if (!auth.loadUserByUsername(line, &target))
            lobbsHistoryReply(h, "User not found.");
        else if (target.is_admin && auth.countAdminUsers() <= 1)
            lobbsHistoryReply(h, "Cannot demote last admin.");
        else if (auth.setUserAdminByUsername(line, false))
            lobbsHistoryReply(h, "Demoted.");
        else
            lobbsHistoryReply(h, "Failed.");
    }
    lobbsHistoryPop(h);
    h->drew = false;
}

static void usersAct(LobbsHistory *h, const LobbsFrame *self)
{
    LobbsFrame n = {};
    n.kind = LobbsFrameKind::Prompt;
    n.tag = "/users/act";
    n.prompt = self->arg1 == 1 ? "Kick who?" : self->arg1 == 2 ? "Promote who?" : "Demote who?";
    n.draw = lobbsAppDrawPrompt;
    n.onSuccess = usersActOk;
    n.arg1 = self->arg1;
    lobbsHistoryPush(h, n);
}

static void usersKick(LobbsHistory *h, const LobbsFrame *)
{
    LobbsFrame fake = {};
    fake.arg1 = 1;
    usersAct(h, &fake);
}
static void usersPromote(LobbsHistory *h, const LobbsFrame *)
{
    LobbsFrame fake = {};
    fake.arg1 = 2;
    usersAct(h, &fake);
}
static void usersDemote(LobbsHistory *h, const LobbsFrame *)
{
    LobbsFrame fake = {};
    fake.arg1 = 3;
    usersAct(h, &fake);
}

static void drawUsers(LobbsHistory *h, const LobbsFrame *self)
{
    lobbsAppDrawTitled(h, self, "Users");
}

void lobbsAuthPush(LobbsHistory *h)
{
    LobbsFrame f = {};
    f.kind = LobbsFrameKind::Menu;
    f.tag = "/users";
    f.draw = drawUsers;
    f.items[0] = {"List", usersList, nullptr};
    f.items[1] = {"Filter", usersFilter, nullptr};
    f.itemCount = 2;
    if (h->ctx && h->ctx->isAdmin) {
        f.items[2] = {"Kick", usersKick, nullptr};
        f.items[3] = {"Promote", usersPromote, nullptr};
        f.items[4] = {"Demote", usersDemote, nullptr};
        f.itemCount = 5;
    }
    lobbsHistoryPush(h, f);
}

#endif
