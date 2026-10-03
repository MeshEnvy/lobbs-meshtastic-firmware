#if !MESHTASTIC_EXCLUDE_LOBBS

#include "NewsDal.h"
#include "configuration.h"
#include "gps/RTC.h"
#include <algorithm>
#include <cstring>

NewsDal::NewsDal(LoDb &lodb) : lodb_(lodb)
{
    lodb_.registerTable("news");
    lodb_.registerTable("news_reads");
}

uint64_t NewsDal::newsUuid(const LoScalar &n)
{
    uint64_t v = 0;
    n.getUint64(1, v);
    return v;
}

bool NewsDal::newsMessage(const LoScalar &n, char *buf, size_t bufCap)
{
    std::string s;
    if (!n.getString(3, s) || bufCap == 0)
        return false;
    strncpy(buf, s.c_str(), bufCap - 1);
    buf[bufCap - 1] = '\0';
    return true;
}

uint64_t NewsDal::newsAuthorUuid(const LoScalar &n)
{
    uint64_t v = 0;
    n.getUint64(4, v);
    return v;
}

uint32_t NewsDal::newsTimestamp(const LoScalar &n)
{
    uint32_t v = 0;
    n.getUint32(5, v);
    return v;
}

static void buildNewsReadKey(uint64_t newsUuid, uint64_t userUuid, char *out, size_t outSize)
{
    char newsHex[17];
    char userHex[17];
    lodb_uuid_to_hex(newsUuid, newsHex);
    lodb_uuid_to_hex(userUuid, userHex);
    snprintf(out, outSize, "%s:%s", newsHex, userHex);
}

static constexpr uint32_t LOBBS_MAX_LIST_ROWS = 256;
static constexpr size_t LOBBS_NEWS_MSG_MAX = 200;

bool NewsDal::postNews(uint64_t authorUserUuid, const char *message)
{
    lodb_uuid_t newsUuidVal = lodb_new_uuid(nullptr, authorUserUuid ^ (uint64_t)getTime());

    LoScalar news;
    news.setUint64(1, newsUuidVal);
    char msgBuf[LOBBS_NEWS_MSG_MAX + 1];
    strncpy(msgBuf, message, LOBBS_NEWS_MSG_MAX);
    msgBuf[LOBBS_NEWS_MSG_MAX] = '\0';
    news.setString(3, msgBuf);
    news.setUint64(4, authorUserUuid);
    news.setUint32(5, getTime());

    LoDbError err = lodb_.insert("news", newsUuidVal, news);
    return err == LODB_OK;
}

bool NewsDal::isNewsReadByUser(uint64_t newsUuidVal, uint64_t userUuid)
{
    char key[35];
    buildNewsReadKey(newsUuidVal, userUuid, key, sizeof(key));
    lodb_uuid_t readUuid = lodb_new_uuid(key, 0);

    LoScalar readRecord;
    LoDbError err = lodb_.get("news_reads", readUuid, readRecord);
    return err == LODB_OK;
}

bool NewsDal::markNewsAsRead(uint64_t newsUuidVal, uint64_t userUuid)
{
    if (isNewsReadByUser(newsUuidVal, userUuid))
        return true;

    char key[35];
    buildNewsReadKey(newsUuidVal, userUuid, key, sizeof(key));
    lodb_uuid_t readUuid = lodb_new_uuid(key, 0);

    LoScalar readRecord;
    readRecord.setUint64(1, newsUuidVal);
    readRecord.setUint64(4, userUuid);
    readRecord.setUint32(5, getTime());

    return lodb_.insert("news_reads", readUuid, readRecord) == LODB_OK;
}

bool NewsDal::markNewsAsUnread(uint64_t newsUuidVal, uint64_t userUuid)
{
    if (!isNewsReadByUser(newsUuidVal, userUuid))
        return true;

    char key[35];
    buildNewsReadKey(newsUuidVal, userUuid, key, sizeof(key));
    lodb_uuid_t readUuid = lodb_new_uuid(key, 0);
    return lodb_.deleteRecord("news_reads", readUuid) == LODB_OK;
}

std::vector<LoBBSNewsEntry> NewsDal::getNewsForUser(uint64_t userUuid, uint32_t offset, uint32_t limit)
{
    auto allNews = lodb_.select("news", LoDbFilter(), LoDbComparator());

    std::vector<LoBBSNewsEntry> newsWithStatus;
    newsWithStatus.reserve(allNews.size());
    for (const auto &newsRec : allNews) {
        uint64_t uuid = newsUuid(newsRec);
        newsWithStatus.push_back({newsRec, isNewsReadByUser(uuid, userUuid)});
    }

    std::sort(newsWithStatus.begin(), newsWithStatus.end(), [](const LoBBSNewsEntry &a, const LoBBSNewsEntry &b) {
        if (a.isRead != b.isRead)
            return !a.isRead;
        return newsTimestamp(a.news) > newsTimestamp(b.news);
    });

    std::vector<LoBBSNewsEntry> result;
    for (size_t i = offset; i < newsWithStatus.size() && i < offset + limit; i++)
        result.push_back(newsWithStatus[i]);
    return result;
}

std::vector<LoBBSNewsEntry> NewsDal::getAllNewsForUser(uint64_t userUuid)
{
    return getNewsForUser(userUuid, 0, LOBBS_MAX_LIST_ROWS);
}

uint32_t NewsDal::countAllNews()
{
    int n = lodb_.count("news");
    return n < 0 ? 0 : (uint32_t)n;
}

uint16_t NewsDal::countUnreadNews(uint64_t userUuid)
{
    auto allNews = lodb_.select("news", LoDbFilter(), LoDbComparator());
    uint16_t count = 0;
    for (const auto &newsRec : allNews) {
        if (!isNewsReadByUser(newsUuid(newsRec), userUuid)) {
            if (count < 0xffff)
                count++;
        }
    }
    return count;
}

bool NewsDal::deleteNewsUuid(uint64_t newsUuidVal)
{
    return lodb_.deleteRecord("news", newsUuidVal) == LODB_OK;
}

bool NewsDal::deleteNewsListIndex(uint64_t readerUuid, uint32_t oneBasedIndex)
{
    if (oneBasedIndex == 0)
        return false;
    auto newsItems = getAllNewsForUser(readerUuid);
    if (oneBasedIndex > newsItems.size())
        return false;
    uint64_t uuid = newsUuid(newsItems[oneBasedIndex - 1].news);
    return deleteNewsUuid(uuid);
}

#endif
