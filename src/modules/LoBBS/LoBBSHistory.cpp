#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSHistory.h"
#include "apps/Root.h"
#include "LoBBSModule.h"
#include "LoBBSPaging.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static bool digitsOnly(const char *s)
{
    if (!s || !*s)
        return false;
    for (const char *p = s; *p; p++) {
        if (!isdigit((unsigned char)*p))
            return false;
    }
    return true;
}

static void ensureRoot(LobbsHistory *h)
{
    if (h->depth == 0)
        lobbsRootInstall(h);
}

static LobbsFrame *top(LobbsHistory *h)
{
    if (h->depth == 0)
        return nullptr;
    return &h->stack[h->depth - 1];
}

static void pathOf(const LobbsHistory *h, char *buf, size_t cap)
{
    size_t n = 0;
    if (cap == 0)
        return;
    buf[0] = '\0';
    for (uint8_t i = 0; i < h->depth && n + 1 < cap; i++) {
        const char *tag = h->stack[i].tag ? h->stack[i].tag : "?";
        int wrote = snprintf(buf + n, cap - n, "%s%s", n ? " " : "", tag);
        if (wrote < 0)
            break;
        n += (size_t)wrote;
    }
}

void lobbsHistoryReply(LobbsHistory *h, const char *msg)
{
    if (!h->ctx || !h->ctx->mod || !h->ctx->mp || !msg)
        return;
    h->ctx->mod->sendReply(*h->ctx->mp, msg);
    h->drew = true;
}

void lobbsHistoryPush(LobbsHistory *h, const LobbsFrame &frame)
{
    if (h->depth >= LOBBS_HISTORY_DEPTH) {
        lobbsHistoryReply(h, "Too deep.");
        return;
    }
    h->stack[h->depth++] = frame;
}

void lobbsHistoryPop(LobbsHistory *h)
{
    if (h->depth <= 1)
        return;
    LobbsFrame popped = h->stack[h->depth - 1];
    h->depth--;
    if (popped.onDigit && h->ctx)
        lobbsPageClearUser(h->ctx->sessionNodeId);
}

void lobbsHistoryDraw(LobbsHistory *h)
{
    ensureRoot(h);
    LobbsFrame *frame = top(h);
    if (frame && frame->draw)
        frame->draw(h, frame);
    h->drew = true;
}

static void goHome(LobbsHistory *h)
{
    h->depth = h->depth > 0 ? 1 : 0;
    h->scratch[0] = '\0';
    h->scratch2[0] = '\0';
    if (h->ctx)
        lobbsPageClearUser(h->ctx->sessionNodeId);
    ensureRoot(h);
}

static void goBack(LobbsHistory *h)
{
    ensureRoot(h);
    if (h->depth <= 1) {
        lobbsHistoryDraw(h);
        return;
    }
    LobbsFrame *frame = top(h);
    if (frame->kind == LobbsFrameKind::Prompt && frame->onCancel) {
        uint8_t before = h->depth;
        frame->onCancel(h, frame);
        if (h->depth == before)
            lobbsHistoryPop(h);
    } else {
        lobbsHistoryPop(h);
    }
    if (!h->drew)
        lobbsHistoryDraw(h);
}

static bool tryPage(LobbsHistory *h, const char *line)
{
    int pageNum = -1;
    if (strcasecmp(line, "p") == 0)
        pageNum = 0;
    else if ((line[0] == 'p' || line[0] == 'P') && digitsOnly(line + 1))
        pageNum = atoi(line + 1);
    else
        return false;
    const char *page = nullptr;
    const char *err = nullptr;
    if (!h->ctx)
        return true;
    if (!lobbsPageFetch(h->ctx->sessionNodeId, pageNum, page, err))
        lobbsHistoryReply(h, err ? err : "Page history not available");
    else
        lobbsHistoryReply(h, page);
    return true;
}

void lobbsHistoryHandle(LobbsHistory *h, LobbsCtx *ctx, const char *line)
{
    h->ctx = ctx;
    h->drew = false;
    ensureRoot(h);
    char path[96];
    pathOf(h, path, sizeof(path));
    char mark[120];
    snprintf(mark, sizeof(mark), "hist [%s] line %s", path, line ? line : "");
    lobbsBreadcrumb(mark);

    if (!line || !line[0]) {
        lobbsHistoryDraw(h);
        return;
    }
    if (strcmp(line, "?") == 0) {
        lobbsHistoryDraw(h);
        return;
    }
    if (strcmp(line, "<<") == 0) {
        goHome(h);
        lobbsHistoryDraw(h);
        return;
    }
    if (strcmp(line, "<") == 0) {
        goBack(h);
        return;
    }
    if (tryPage(h, line))
        return;

    LobbsFrame *frame = top(h);
    if (!frame)
        return;
    if (frame->kind == LobbsFrameKind::Prompt) {
        if (frame->onSuccess)
            frame->onSuccess(h, frame, line);
    } else if (digitsOnly(line) && frame->onDigit) {
        frame->onDigit(h, frame, (uint32_t)atoi(line));
    } else if (digitsOnly(line)) {
        int pick = atoi(line);
        if (pick >= 1 && pick <= (int)frame->itemCount && frame->items[pick - 1].onPick)
            frame->items[pick - 1].onPick(h, frame);
    }
    if (!h->drew)
        lobbsHistoryDraw(h);
}

#endif
