#if !MESHTASTIC_EXCLUDE_LOBBS

#include "MailDal.h"
#include "mail.pb.h"
#include "configuration.h"
#include "gps/RTC.h"
#include <cstring>

MailDal::MailDal(LoDb &lodb) : lodb_(lodb)
{
    lodb_.registerTable("mail", &meshtastic_LoBBSMail_msg, sizeof(meshtastic_LoBBSMail));
}

static int compareMailByTimestamp(const void *a, const void *b)
{
    const meshtastic_LoBBSMail *m1 = (const meshtastic_LoBBSMail *)a;
    const meshtastic_LoBBSMail *m2 = (const meshtastic_LoBBSMail *)b;
    if (m2->timestamp > m1->timestamp)
        return 1;
    if (m2->timestamp < m1->timestamp)
        return -1;
    return 0;
}

static constexpr uint32_t LOBBS_MAX_LIST_ROWS = 256;

bool MailDal::sendMail(uint64_t fromUserUuid, uint64_t toUserUuid, const char *message)
{
    lodb_uuid_t mailUuid = lodb_new_uuid((const char *)&toUserUuid, getTime());

    meshtastic_LoBBSMail mail = meshtastic_LoBBSMail_init_zero;
    mail.uuid = mailUuid;
    mail.from_user_uuid = fromUserUuid;
    mail.to_user_uuid = toUserUuid;
    strncpy(mail.message, message, sizeof(mail.message) - 1);
    mail.message[sizeof(mail.message) - 1] = '\0';
    mail.timestamp = getTime();
    mail.read = false;

    LoDbError err = lodb_.insert("mail", mailUuid, &mail);
    if (err != LODB_OK) {
        LOG_ERROR("Failed to send mail");
        return false;
    }
    return true;
}

std::vector<void *> MailDal::getMailForUser(uint64_t userUuid, uint32_t offset, uint32_t limit)
{
    auto mail_filter = [userUuid](const void *rec) -> bool {
        const meshtastic_LoBBSMail *m = (const meshtastic_LoBBSMail *)rec;
        return m->to_user_uuid == userUuid;
    };

    auto allMail = lodb_.select("mail", mail_filter, compareMailByTimestamp);

    std::vector<void *> result;
    for (size_t i = offset; i < allMail.size() && i < offset + limit; i++)
        result.push_back(allMail[i]);

    for (size_t i = 0; i < allMail.size(); i++) {
        if (i < offset || i >= offset + limit)
            delete[] (uint8_t *)allMail[i];
    }
    return result;
}

std::vector<void *> MailDal::getAllMailForUser(uint64_t userUuid)
{
    return getMailForUser(userUuid, 0, LOBBS_MAX_LIST_ROWS);
}

bool MailDal::markMailAsRead(uint64_t mailUuid)
{
    meshtastic_LoBBSMail mail = meshtastic_LoBBSMail_init_zero;
    LoDbError err = lodb_.get("mail", mailUuid, &mail);
    if (err != LODB_OK)
        return false;

    mail.read = true;
    lodb_.deleteRecord("mail", mailUuid);
    err = lodb_.insert("mail", mailUuid, &mail);
    return err == LODB_OK;
}

bool MailDal::markMailAsUnread(uint64_t mailUuid)
{
    meshtastic_LoBBSMail mail = meshtastic_LoBBSMail_init_zero;
    LoDbError err = lodb_.get("mail", mailUuid, &mail);
    if (err != LODB_OK)
        return false;

    mail.read = false;
    lodb_.deleteRecord("mail", mailUuid);
    err = lodb_.insert("mail", mailUuid, &mail);
    return err == LODB_OK;
}

uint32_t MailDal::countAllMail()
{
    int n = lodb_.count("mail");
    return n < 0 ? 0 : (uint32_t)n;
}

uint16_t MailDal::countUnreadMail(uint64_t userUuid)
{
    auto mail_filter = [userUuid](const void *rec) -> bool {
        const meshtastic_LoBBSMail *m = (const meshtastic_LoBBSMail *)rec;
        return m->to_user_uuid == userUuid && !m->read;
    };
    auto rows = lodb_.select("mail", mail_filter, nullptr);
    uint16_t count = (uint16_t)(rows.size() > 0xffff ? 0xffff : rows.size());
    LoDb::freeRecords(rows);
    return count;
}

bool MailDal::deleteMailUuid(uint64_t mailUuid)
{
    return lodb_.deleteRecord("mail", mailUuid) == LODB_OK;
}

bool MailDal::deleteMailInboxIndex(uint64_t inboxOwnerUuid, uint32_t oneBasedIndex)
{
    if (oneBasedIndex == 0)
        return false;
    auto mail = getAllMailForUser(inboxOwnerUuid);
    if (oneBasedIndex > mail.size()) {
        LoDb::freeRecords(mail);
        return false;
    }
    const meshtastic_LoBBSMail *m = (const meshtastic_LoBBSMail *)mail[oneBasedIndex - 1];
    uint64_t uuid = m->uuid;
    LoDb::freeRecords(mail);
    return deleteMailUuid(uuid);
}

#endif
