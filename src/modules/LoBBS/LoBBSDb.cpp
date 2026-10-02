#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSDb.h"
#include "apps/Auth/AuthDal.h"
#include "apps/Mail/MailDal.h"
#include "apps/News/NewsDal.h"

LoBBSDb::LoBBSDb(uint32_t hostNodeId) : hostNodeId_(hostNodeId)
{
    db_ = new LoDb("lobbs");
    AuthDal::registerTables(*db_);
    MailDal::registerTables(*db_);
    NewsDal::registerTables(*db_);
}

LoBBSDb::~LoBBSDb()
{
    delete db_;
}

#endif
