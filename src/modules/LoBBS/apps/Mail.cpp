#if !MESHTASTIC_EXCLUDE_LOBBS

#include "Mail.h"
#include "AppUtil.h"
#include "../LoBBSModule.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

static void drawMailRead(LobbsHistory *h, const LobbsFrame *self)
{
    auto mailMessages = h->ctx->dal->getAllMailForUser(self->arg0);
    uint32_t idx = self->arg1;
    if (idx == 0 || idx > mailMessages.size()) {
        LoDb::freeRecords(mailMessages);
        lobbsHistoryReply(h, "Invalid message number");
        return;
    }
    const meshtastic_LoBBSMail *mail = (const meshtastic_LoBBSMail *)mailMessages[idx - 1];
    meshtastic_LoBBSUser sender = meshtastic_LoBBSUser_init_zero;
    lobbsAppLoadUser(h->ctx->dal, mail->from_user_uuid, &sender);
    char name[32];
    char body[120];
    char when[32];
    char reply[200];
    lobbsAppCopyCapped(name, sizeof(name), sender.username, sizeof(sender.username));
    if (!name[0])
        lobbsAppCopyCapped(name, sizeof(name), "unknown", 7);
    lobbsAppCopyCapped(body, sizeof(body), mail->message, sizeof(mail->message));
    lobbsAppTimeAgo(mail->timestamp, when, sizeof(when));
    uint64_t mailUuid = mail->uuid;
    bool own = h->ctx->user && self->arg0 == h->ctx->user->uuid;
    snprintf(reply, sizeof(reply), "From: @%s (%s)\n%s%s", name, when, body, LOBBS_HIST_NAV);
    lobbsHistoryReply(h, reply);
    if (own)
        h->ctx->dal->markMailAsRead(mailUuid);
    LoDb::freeRecords(mailMessages);
}

static void openMailIndex(LobbsHistory *h, const LobbsFrame *self, uint32_t number)
{
    LobbsFrame view = {};
    view.kind = LobbsFrameKind::Menu;
    view.tag = "/mail/read";
    view.draw = drawMailRead;
    view.arg0 = self->arg0;
    view.arg1 = number;
    lobbsHistoryPush(h, view);
}

static void drawInbox(LobbsHistory *h, const LobbsFrame *self)
{
    auto mailMessages = h->ctx->dal->getAllMailForUser(self->arg0);
    if (mailMessages.empty()) {
        LoDb::freeRecords(mailMessages);
        lobbsHistoryReply(h, "No mail");
        return;
    }
    std::string list;
    for (size_t i = 0; i < mailMessages.size(); i++) {
        const meshtastic_LoBBSMail *mail = (const meshtastic_LoBBSMail *)mailMessages[i];
        meshtastic_LoBBSUser sender = meshtastic_LoBBSUser_init_zero;
        lobbsAppLoadUser(h->ctx->dal, mail->from_user_uuid, &sender);
        char name[32];
        char when[32];
        char trunc[50];
        char line[160];
        lobbsAppCopyCapped(name, sizeof(name), sender.username, sizeof(sender.username));
        if (!name[0])
            lobbsAppCopyCapped(name, sizeof(name), "unknown", 7);
        lobbsAppTimeAgo(mail->timestamp, when, sizeof(when));
        lobbsAppTruncMsg(mail->message, trunc, sizeof(trunc), 25);
        snprintf(line, sizeof(line), "[%d]%s @%s: %s (%s)\n", (int)(i + 1), mail->read ? "" : "*", name, trunc, when);
        list += line;
    }
    LoDb::freeRecords(mailMessages);
    if (list.size() + strlen(LOBBS_HIST_NAV) < 2000)
        list += LOBBS_HIST_NAV;
    h->ctx->mod->sendPagedReply(h->ctx->sessionNodeId, *h->ctx->mp, list.c_str());
    h->drew = true;
}

void lobbsMailPushInbox(LobbsHistory *h, uint64_t inboxUuid)
{
    LobbsFrame f = {};
    f.kind = LobbsFrameKind::Menu;
    f.tag = "/mail/inbox";
    f.draw = drawInbox;
    f.arg0 = inboxUuid;
    f.onDigit = openMailIndex;
    lobbsHistoryPush(h, f);
}

void lobbsMailPushRead(LobbsHistory *h, uint64_t inboxUuid, uint32_t idx)
{
    LobbsFrame f = {};
    f.kind = LobbsFrameKind::Menu;
    f.tag = "/mail/read";
    f.draw = drawMailRead;
    f.arg0 = inboxUuid;
    f.arg1 = idx;
    lobbsHistoryPush(h, f);
}

static void mailInbox(LobbsHistory *h, const LobbsFrame *)
{
    if (!h->ctx->isAuth || !h->ctx->user) {
        lobbsHistoryReply(h, "Login required.");
        return;
    }
    lobbsMailPushInbox(h, h->ctx->user->uuid);
}

static void mailBodyOk(LobbsHistory *h, const LobbsFrame *, const char *line)
{
    uint64_t toUuid = h->ctx->dal->getUserUuidByUsername(h->scratch);
    if (toUuid == 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "User '%s' not found.", h->scratch);
        lobbsHistoryReply(h, buf);
        h->drew = false;
        return;
    }
    if (!h->ctx->isAuth || !h->ctx->user) {
        lobbsHistoryReply(h, "Login required.");
        return;
    }
    if (h->ctx->dal->sendMail(h->ctx->user->uuid, toUuid, line))
        lobbsHistoryReply(h, "Mail sent.");
    else
        lobbsHistoryReply(h, "Failed to send mail.");
    lobbsHistoryPop(h);
    lobbsHistoryPop(h);
    h->scratch[0] = '\0';
    h->drew = false;
}

static void mailToOk(LobbsHistory *h, const LobbsFrame *, const char *line)
{
    snprintf(h->scratch, sizeof(h->scratch), "%s", line);
    lobbsHistoryPop(h);
    LobbsFrame body = {};
    body.kind = LobbsFrameKind::Prompt;
    body.tag = "/mail/send/body";
    body.prompt = "Message?";
    body.draw = lobbsAppDrawPrompt;
    body.onSuccess = mailBodyOk;
    lobbsHistoryPush(h, body);
}

static void mailSend(LobbsHistory *h, const LobbsFrame *)
{
    LobbsFrame to = {};
    to.kind = LobbsFrameKind::Prompt;
    to.tag = "/mail/send";
    to.prompt = "To?";
    to.draw = lobbsAppDrawPrompt;
    to.onSuccess = mailToOk;
    lobbsHistoryPush(h, to);
}

static void mailReadOk(LobbsHistory *h, const LobbsFrame *, const char *line)
{
    if (!h->ctx->user)
        return;
    uint32_t idx = (uint32_t)atoi(line);
    uint64_t inbox = h->ctx->user->uuid;
    lobbsHistoryPop(h);
    lobbsMailPushRead(h, inbox, idx);
}

static void mailRead(LobbsHistory *h, const LobbsFrame *)
{
    LobbsFrame n = {};
    n.kind = LobbsFrameKind::Prompt;
    n.tag = "/mail/read?";
    n.prompt = "Number?";
    n.draw = lobbsAppDrawPrompt;
    n.onSuccess = mailReadOk;
    lobbsHistoryPush(h, n);
}

static void mailDelOk(LobbsHistory *h, const LobbsFrame *, const char *line)
{
    if (!h->ctx->user)
        return;
    if (h->ctx->dal->deleteMailInboxIndex(h->ctx->user->uuid, (uint32_t)atoi(line)))
        lobbsHistoryReply(h, "Deleted.");
    else
        lobbsHistoryReply(h, "Invalid message number");
    lobbsHistoryPop(h);
    h->drew = false;
}

static void mailDel(LobbsHistory *h, const LobbsFrame *)
{
    LobbsFrame n = {};
    n.kind = LobbsFrameKind::Prompt;
    n.tag = "/mail/del";
    n.prompt = "Delete #?";
    n.draw = lobbsAppDrawPrompt;
    n.onSuccess = mailDelOk;
    lobbsHistoryPush(h, n);
}

static void mailOtherOk(LobbsHistory *h, const LobbsFrame *, const char *line)
{
    uint64_t uuid = h->ctx->dal->getUserUuidByUsername(line);
    if (uuid == 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "User '%s' not found.", line);
        lobbsHistoryReply(h, buf);
        h->drew = false;
        return;
    }
    lobbsHistoryPop(h);
    lobbsMailPushInbox(h, uuid);
}

static void mailOther(LobbsHistory *h, const LobbsFrame *)
{
    if (!h->ctx->isAdmin) {
        lobbsHistoryReply(h, "Admin only.");
        return;
    }
    LobbsFrame n = {};
    n.kind = LobbsFrameKind::Prompt;
    n.tag = "/mail/other";
    n.prompt = "User?";
    n.draw = lobbsAppDrawPrompt;
    n.onSuccess = mailOtherOk;
    lobbsHistoryPush(h, n);
}

static void drawMail(LobbsHistory *h, const LobbsFrame *self)
{
    lobbsAppDrawTitled(h, self, "Mail");
}

void lobbsMailPush(LobbsHistory *h)
{
    LobbsFrame f = {};
    f.kind = LobbsFrameKind::Menu;
    f.tag = "/mail";
    f.draw = drawMail;
    f.items[0] = {"Inbox", mailInbox};
    f.items[1] = {"Send", mailSend};
    f.items[2] = {"Read", mailRead};
    f.items[3] = {"Delete", mailDel};
    f.itemCount = 4;
    if (h->ctx && h->ctx->isAdmin) {
        f.items[4] = {"Other inbox", mailOther};
        f.itemCount = 5;
    }
    lobbsHistoryPush(h, f);
}

#endif
