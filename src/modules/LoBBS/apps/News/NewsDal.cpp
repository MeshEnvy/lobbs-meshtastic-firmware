#if !MESHTASTIC_EXCLUDE_LOBBS

#include "NewsDal.h"
#include "configuration.h"
#include "gps/RTC.h"
#include <algorithm>
#include <cstring>

NewsDal::NewsDal(LoDb &lodb) : lodb_(lodb)
{
    lodb_.registerTable("news", &meshtastic_LoBBSNews_msg, sizeof(meshtastic_LoBBSNews));
    lodb_.registerTable("news_reads", &meshtastic_LoBBSNewsRead_msg, sizeof(meshtastic_LoBBSNewsRead));
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

bool NewsDal::postNews(uint64_t authorUserUuid, const char *message)
{
    lodb_uuid_t newsUuid = lodb_new_uuid(nullptr, authorUserUuid ^ (uint64_t)getTime());

    meshtastic_LoBBSNews news = meshtastic_LoBBSNews_init_zero;
    news.uuid = newsUuid;
    news.author_user_uuid = authorUserUuid;
    strncpy(news.message, message, sizeof(news.message) - 1);
    news.message[sizeof(news.message) - 1] = '\0';
    news.timestamp = getTime();

    LoDbError err = lodb_.insert("news", newsUuid, &news);
    return err == LODB_OK;
}

bool NewsDal::isNewsReadByUser(uint64_t newsUuid, uint64_t userUuid)
{
    char key[35];
    buildNewsReadKey(newsUuid, userUuid, key, sizeof(key));
    lodb_uuid_t readUuid = lodb_new_uuid(key, 0);

    meshtastic_LoBBSNewsRead readRecord = meshtastic_LoBBSNewsRead_init_zero;
    LoDbError err = lodb_.get("news_reads", readUuid, &readRecord);
    return err == LODB_OK;
}

bool NewsDal::markNewsAsRead(uint64_t newsUuid, uint64_t userUuid)
{
    if (isNewsReadByUser(newsUuid, userUuid))
        return true;

    char key[35];
    buildNewsReadKey(newsUuid, userUuid, key, sizeof(key));
    lodb_uuid_t readUuid = lodb_new_uuid(key, 0);

    meshtastic_LoBBSNewsRead readRecord = meshtastic_LoBBSNewsRead_init_zero;
    readRecord.news_uuid = newsUuid;
    readRecord.user_uuid = userUuid;
    readRecord.read_timestamp = getTime();

    return lodb_.insert("news_reads", readUuid, &readRecord) == LODB_OK;
}

bool NewsDal::markNewsAsUnread(uint64_t newsUuid, uint64_t userUuid)
{
    if (!isNewsReadByUser(newsUuid, userUuid))
        return true;

    char key[35];
    buildNewsReadKey(newsUuid, userUuid, key, sizeof(key));
    lodb_uuid_t readUuid = lodb_new_uuid(key, 0);
    return lodb_.deleteRecord("news_reads", readUuid) == LODB_OK;
}

std::vector<LoBBSNewsEntry> NewsDal::getNewsForUser(uint64_t userUuid, uint32_t offset, uint32_t limit)
{
    auto allNews = lodb_.select("news", LoDbFilter(), LoDbComparator());

    std::vector<LoBBSNewsEntry> newsWithStatus;
    newsWithStatus.reserve(allNews.size());
    for (auto *newsPtr : allNews) {
        auto *news = (meshtastic_LoBBSNews *)newsPtr;
        newsWithStatus.push_back({news, isNewsReadByUser(news->uuid, userUuid)});
    }

    std::sort(newsWithStatus.begin(), newsWithStatus.end(), [](const LoBBSNewsEntry &a, const LoBBSNewsEntry &b) {
        if (a.isRead != b.isRead)
            return !a.isRead;
        return a.news->timestamp > b.news->timestamp;
    });

    std::vector<LoBBSNewsEntry> result;
    for (size_t i = offset; i < newsWithStatus.size() && i < offset + limit; i++)
        result.push_back(newsWithStatus[i]);

    for (size_t i = 0; i < newsWithStatus.size(); i++) {
        if (i < offset || i >= offset + limit)
            delete[] (uint8_t *)newsWithStatus[i].news;
    }
    return result;
}

std::vector<LoBBSNewsEntry> NewsDal::getAllNewsForUser(uint64_t userUuid)
{
    return getNewsForUser(userUuid, 0, LOBBS_MAX_LIST_ROWS);
}

uint16_t NewsDal::countUnreadNews(uint64_t userUuid)
{
    auto allNews = lodb_.select("news", LoDbFilter(), nullptr);
    uint16_t count = 0;
    for (void *newsPtr : allNews) {
        const meshtastic_LoBBSNews *news = (const meshtastic_LoBBSNews *)newsPtr;
        if (!isNewsReadByUser(news->uuid, userUuid)) {
            if (count < 0xffff)
                count++;
        }
    }
    LoDb::freeRecords(allNews);
    return count;
}

bool NewsDal::deleteNewsUuid(uint64_t newsUuid)
{
    return lodb_.deleteRecord("news", newsUuid) == LODB_OK;
}

bool NewsDal::deleteNewsListIndex(uint64_t readerUuid, uint32_t oneBasedIndex)
{
    if (oneBasedIndex == 0)
        return false;
    auto newsItems = getAllNewsForUser(readerUuid);
    if (oneBasedIndex > newsItems.size()) {
        for (auto &entry : newsItems)
            delete[] (uint8_t *)entry.news;
        return false;
    }
    uint64_t uuid = newsItems[oneBasedIndex - 1].news->uuid;
    for (auto &entry : newsItems)
        delete[] (uint8_t *)entry.news;
    return deleteNewsUuid(uuid);
}

#endif
