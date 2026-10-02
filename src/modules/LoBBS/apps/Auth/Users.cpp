#if !MESHTASTIC_EXCLUDE_LOBBS

#include "Users.h"
#include "AuthDal.h"
#include "../AppUtil.h"
#include "../../LoBBSModule.h"
#include <cctype>
#include <cstring>
#include <string>

static AuthDal authOf(LobbsHistory *h)
{
    return AuthDal(*h->ctx->db);
}

static int cmpUser(const void *a, const void *b)
{
    return strcasecmp(((const meshtastic_LoBBSUser *)a)->username, ((const meshtastic_LoBBSUser *)b)->username);
}

static void drawUserList(LobbsHistory *h, const LobbsFrame *self)
{
    const char *filter = h->scratch;
    auto users = h->ctx->db->getDb()->select(
        "users",
        [filter](const void *rec) -> bool {
            const meshtastic_LoBBSUser *u = (const meshtastic_LoBBSUser *)rec;
            if (!filter || !filter[0])
                return true;
            const char *hay = u->username;
            for (; *hay; hay++) {
                const char *a = hay;
                const char *b = filter;
                while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
                    a++;
                    b++;
                }
                if (!*b)
                    return true;
            }
            return false;
        },
        cmpUser);
    if (users.empty()) {
        LoDb::freeRecords(users);
        lobbsHistoryReply(h, filter[0] ? "No users match filter." : "No users found");
        return;
    }
    std::string msg = "Users:\n";
    for (size_t i = 0; i < users.size(); i++) {
        const meshtastic_LoBBSUser *u = (const meshtastic_LoBBSUser *)users[i];
        if (i > 0)
            msg += ", ";
        msg += u->username;
        if (u->is_admin)
            msg += "*";
    }
    LoDb::freeRecords(users);
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
    AuthDal auth = authOf(h);
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

void lobbsUsersPush(LobbsHistory *h)
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
