#if !MESHTASTIC_EXCLUDE_LOBBS

#include "paginate.h"
#include "LoBBSReply.h"
#include <cstdio>
#include <cstring>
#include <vector>

static constexpr size_t LOBBS_PAGER_FOOTER_MAX = 16;
static constexpr int LOBBS_PAGER_MAX_PAGES = 64;
static constexpr char LOBBS_LINE_TRUNC[] = "[...]";
static constexpr size_t LOBBS_LINE_TRUNC_LEN = sizeof(LOBBS_LINE_TRUNC) - 1;

static bool lobbsHumanTruncateLine(char *line, size_t lineCap, size_t maxBytes)
{
    size_t len = strlen(line);
    if (len <= maxBytes)
        return true;
    if (maxBytes <= LOBBS_LINE_TRUNC_LEN) {
        line[0] = '\0';
        return false;
    }
    size_t copyLen = maxBytes - LOBBS_LINE_TRUNC_LEN;
    if (copyLen >= lineCap)
        copyLen = lineCap - 1;
    line[copyLen] = '\0';
    strncat(line, LOBBS_LINE_TRUNC, lineCap - copyLen - 1);
    return true;
}

static bool lobbsHumanPackPages(const std::vector<std::string> &lines, size_t maxBytes, bool reserveFooter, uint32_t *pageStarts,
                                int &pageCountOut)
{
    pageCountOut = 0;
    if (lines.empty())
        return false;
    uint32_t idx = 0;
    while (idx < lines.size() && pageCountOut < LOBBS_PAGER_MAX_PAGES) {
        pageStarts[pageCountOut] = idx;
        size_t used = 0;
        while (idx < lines.size()) {
            char lineBuf[512];
            strncpy(lineBuf, lines[idx].c_str(), sizeof(lineBuf) - 1);
            lineBuf[sizeof(lineBuf) - 1] = '\0';
            size_t footerReserve = reserveFooter ? LOBBS_PAGER_FOOTER_MAX : 0;
            size_t maxLine = maxBytes > footerReserve ? maxBytes - footerReserve : 0;
            lobbsHumanTruncateLine(lineBuf, sizeof(lineBuf), maxLine);
            size_t lineLen = strlen(lineBuf);
            size_t add = lineLen + (used > 0 ? 1 : 0);
            if (used + add + footerReserve > maxBytes) {
                if (used > 0)
                    break;
                used += lineLen;
                idx++;
                break;
            }
            used += add;
            idx++;
            if (idx < lines.size()) {
                char nextBuf[512];
                strncpy(nextBuf, lines[idx].c_str(), sizeof(nextBuf) - 1);
                nextBuf[sizeof(nextBuf) - 1] = '\0';
                lobbsHumanTruncateLine(nextBuf, sizeof(nextBuf), maxLine);
                size_t nextLen = strlen(nextBuf);
                size_t nextAdd = nextLen + 1;
                if (used + nextAdd + footerReserve > maxBytes)
                    break;
            }
        }
        pageCountOut++;
    }
    if (pageCountOut < LOBBS_PAGER_MAX_PAGES)
        pageStarts[pageCountOut] = (uint32_t)lines.size();
    return pageCountOut > 0;
}

bool lobbsPaginatePlainText(const std::string &text, uint32_t page1, std::string &pageOut, const char **errMsg)
{
    pageOut.clear();
    if (text.empty()) {
        if (errMsg)
            *errMsg = "Empty.";
        return false;
    }

    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }

    if (page1 == 0)
        page1 = 1;
    uint32_t pageStarts[LOBBS_PAGER_MAX_PAGES + 1];
    memset(pageStarts, 0, sizeof(pageStarts));
    int pageCount = 0;
    lobbsHumanPackPages(lines, LOBBS_REPLY_BYTES, true, pageStarts, pageCount);
    if (pageCount <= 0) {
        if (errMsg)
            *errMsg = "Empty.";
        return false;
    }
    if (pageCount == 1) {
        lobbsHumanPackPages(lines, LOBBS_REPLY_BYTES, false, pageStarts, pageCount);
        pageStarts[1] = (uint32_t)lines.size();
    }
    if (page1 > (uint32_t)pageCount) {
        if (errMsg)
            *errMsg = "No such page.";
        return false;
    }

    uint32_t from = pageStarts[page1 - 1];
    uint32_t to = pageStarts[page1];
    size_t footerReserve = pageCount > 1 ? LOBBS_PAGER_FOOTER_MAX : 0;
    size_t maxLine = LOBBS_REPLY_BYTES > footerReserve ? LOBBS_REPLY_BYTES - footerReserve : 0;
    for (uint32_t i = from; i < to; i++) {
        if (i > from)
            pageOut.push_back('\n');
        char lineBuf[512];
        strncpy(lineBuf, lines[i].c_str(), sizeof(lineBuf) - 1);
        lineBuf[sizeof(lineBuf) - 1] = '\0';
        lobbsHumanTruncateLine(lineBuf, sizeof(lineBuf), maxLine);
        pageOut += lineBuf;
    }
    if ((uint32_t)pageCount > 1) {
        char footer[LOBBS_PAGER_FOOTER_MAX];
        snprintf(footer, sizeof(footer), "{p %u/%u}", page1, (uint32_t)pageCount);
        pageOut.push_back('\n');
        pageOut += footer;
    }
    return true;
}

#endif
