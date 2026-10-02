#if !MESHTASTIC_EXCLUDE_LOBBS

#include "News.h"
#include "../AppUtil.h"
#include "../../LoBBSModule.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

NewsApp::NewsApp(LoDb &lodb) : dal_(lodb) {}

static NewsDal &newsOf(LobbsHistory *h)
{
    return h->ctx->mod->news().dal();
}

void lobbsNewsItemStatus(LobbsHistory *h, char *buf, size_t cap)
{
    if (!buf || cap == 0)
        return;
    buf[0] = '\0';
    if (!h->ctx || !h->ctx->mod || !h->ctx->user)
        return;
    lobbsAppStatusCount(buf, cap, newsOf(h).countUnreadNews(h->ctx->user->uuid));
}

static void drawNewsRead(LobbsHistory *h, const LobbsFrame *self)
{
    if (!h->ctx->user) {
        lobbsHistoryReply(h, "Login required.");
        return;
    }
    auto newsItems = newsOf(h).getAllNewsForUser(h->ctx->user->uuid);
    uint32_t idx = self->arg1;
    if (idx == 0 || idx > newsItems.size()) {
        for (auto &e : newsItems)
            delete[] (uint8_t *)e.news;
        lobbsHistoryReply(h, "Invalid news number");
        return;
    }
    const meshtastic_LoBBSNews *news = newsItems[idx - 1].news;
    meshtastic_LoBBSUser author = meshtastic_LoBBSUser_init_zero;
    lobbsAppLoadUser(h->ctx->mod->auth().dal(), news->author_user_uuid, &author);
    char name[32];
    char body[120];
    char when[32];
    char reply[200];
    lobbsAppCopyCapped(name, sizeof(name), author.username, sizeof(author.username));
    if (!name[0])
        lobbsAppCopyCapped(name, sizeof(name), "unknown", 7);
    lobbsAppCopyCapped(body, sizeof(body), news->message, sizeof(news->message));
    lobbsAppTimeAgo(news->timestamp, when, sizeof(when));
    uint64_t newsUuid = news->uuid;
    snprintf(reply, sizeof(reply), "From: @%s (%s)\n%s%s", name, when, body, LOBBS_HIST_NAV);
    lobbsHistoryReply(h, reply);
    newsOf(h).markNewsAsRead(newsUuid, h->ctx->user->uuid);
    for (auto &e : newsItems)
        delete[] (uint8_t *)e.news;
}

static void openNewsIndex(LobbsHistory *h, const LobbsFrame *, uint32_t number)
{
    lobbsNewsPushRead(h, number);
}

static void drawNewsList(LobbsHistory *h, const LobbsFrame *)
{
    if (!h->ctx->user) {
        lobbsHistoryReply(h, "Login required.");
        return;
    }
    auto newsItems = newsOf(h).getAllNewsForUser(h->ctx->user->uuid);
    if (newsItems.empty()) {
        for (auto &e : newsItems)
            delete[] (uint8_t *)e.news;
        lobbsHistoryReply(h, "No news");
        return;
    }
    std::string list;
    for (size_t i = 0; i < newsItems.size(); i++) {
        const meshtastic_LoBBSNews *news = newsItems[i].news;
        meshtastic_LoBBSUser author = meshtastic_LoBBSUser_init_zero;
        lobbsAppLoadUser(h->ctx->mod->auth().dal(), news->author_user_uuid, &author);
        char name[32];
        char when[32];
        char trunc[50];
        char line[160];
        lobbsAppCopyCapped(name, sizeof(name), author.username, sizeof(author.username));
        if (!name[0])
            lobbsAppCopyCapped(name, sizeof(name), "unknown", 7);
        lobbsAppTimeAgo(news->timestamp, when, sizeof(when));
        lobbsAppTruncMsg(news->message, trunc, sizeof(trunc), 25);
        snprintf(line, sizeof(line), "[%d]%s @%s: %s (%s)\n", (int)(i + 1), newsItems[i].isRead ? "" : "*", name, trunc,
                 when);
        list += line;
    }
    for (auto &e : newsItems)
        delete[] (uint8_t *)e.news;
    if (list.size() + strlen(LOBBS_HIST_NAV) < 2000)
        list += LOBBS_HIST_NAV;
    h->ctx->mod->sendPagedReply(h->ctx->sessionNodeId, *h->ctx->mp, list.c_str());
    h->drew = true;
}

void lobbsNewsPushList(LobbsHistory *h)
{
    LobbsFrame f = {};
    f.kind = LobbsFrameKind::Menu;
    f.tag = "/news/list";
    f.draw = drawNewsList;
    f.onDigit = openNewsIndex;
    lobbsHistoryPush(h, f);
}

void lobbsNewsPushRead(LobbsHistory *h, uint32_t idx)
{
    LobbsFrame f = {};
    f.kind = LobbsFrameKind::Menu;
    f.tag = "/news/read";
    f.draw = drawNewsRead;
    f.arg1 = idx;
    lobbsHistoryPush(h, f);
}

static void newsList(LobbsHistory *h, const LobbsFrame *)
{
    lobbsNewsPushList(h);
}

static void newsPostOk(LobbsHistory *h, const LobbsFrame *, const char *line)
{
    if (!h->ctx->user) {
        lobbsHistoryReply(h, "Login required.");
        return;
    }
    if (!isalpha((unsigned char)line[0])) {
        lobbsHistoryReply(h, "Start with a letter.");
        h->drew = false;
        return;
    }
    if (newsOf(h).postNews(h->ctx->user->uuid, line))
        lobbsHistoryReply(h, "News posted.");
    else
        lobbsHistoryReply(h, "Failed to post news.");
    lobbsHistoryPop(h);
    h->drew = false;
}

static void newsPost(LobbsHistory *h, const LobbsFrame *)
{
    LobbsFrame n = {};
    n.kind = LobbsFrameKind::Prompt;
    n.tag = "/news/post";
    n.prompt = "News?";
    n.draw = lobbsAppDrawPrompt;
    n.onSuccess = newsPostOk;
    lobbsHistoryPush(h, n);
}

static void newsReadOk(LobbsHistory *h, const LobbsFrame *, const char *line)
{
    uint32_t idx = (uint32_t)atoi(line);
    lobbsHistoryPop(h);
    lobbsNewsPushRead(h, idx);
}

static void newsRead(LobbsHistory *h, const LobbsFrame *)
{
    LobbsFrame n = {};
    n.kind = LobbsFrameKind::Prompt;
    n.tag = "/news/read?";
    n.prompt = "Number?";
    n.draw = lobbsAppDrawPrompt;
    n.onSuccess = newsReadOk;
    lobbsHistoryPush(h, n);
}

static void newsDelOk(LobbsHistory *h, const LobbsFrame *, const char *line)
{
    if (!h->ctx->user)
        return;
    uint32_t idx = (uint32_t)atoi(line);
    auto newsItems = newsOf(h).getAllNewsForUser(h->ctx->user->uuid);
    if (idx == 0 || idx > newsItems.size()) {
        for (auto &e : newsItems)
            delete[] (uint8_t *)e.news;
        lobbsHistoryReply(h, "Invalid news number");
        lobbsHistoryPop(h);
        h->drew = false;
        return;
    }
    const meshtastic_LoBBSNews *news = newsItems[idx - 1].news;
    bool allowed = h->ctx->isAdmin || news->author_user_uuid == h->ctx->user->uuid;
    uint64_t newsUuid = news->uuid;
    for (auto &e : newsItems)
        delete[] (uint8_t *)e.news;
    if (!allowed)
        lobbsHistoryReply(h, "Not allowed.");
    else if (newsOf(h).deleteNewsUuid(newsUuid))
        lobbsHistoryReply(h, "Deleted.");
    else
        lobbsHistoryReply(h, "Failed to delete.");
    lobbsHistoryPop(h);
    h->drew = false;
}

static void newsDel(LobbsHistory *h, const LobbsFrame *)
{
    LobbsFrame n = {};
    n.kind = LobbsFrameKind::Prompt;
    n.tag = "/news/del";
    n.prompt = "Delete #?";
    n.draw = lobbsAppDrawPrompt;
    n.onSuccess = newsDelOk;
    lobbsHistoryPush(h, n);
}

static void drawNews(LobbsHistory *h, const LobbsFrame *self)
{
    lobbsAppDrawTitled(h, self, "News");
}

void lobbsNewsPush(LobbsHistory *h)
{
    LobbsFrame f = {};
    f.kind = LobbsFrameKind::Menu;
    f.tag = "/news";
    f.draw = drawNews;
    f.items[0] = {"List", newsList, lobbsNewsItemStatus};
    f.items[1] = {"Post", newsPost, nullptr};
    f.items[2] = {"Read", newsRead, nullptr};
    f.items[3] = {"Delete", newsDel, nullptr};
    f.itemCount = 4;
    lobbsHistoryPush(h, f);
}

#endif
