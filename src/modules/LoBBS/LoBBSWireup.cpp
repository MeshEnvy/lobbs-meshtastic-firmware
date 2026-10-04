#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSWireup.h"
#include "LoBBSHooks.h"
#include "apps/Auth/AuthCommands.h"
#include "apps/Fs/FsCommands.h"
#include "apps/Help/HelpCommands.h"
#include "apps/Mail/MailCommands.h"
#include "apps/News/NewsCommands.h"
#include "apps/Status/StatusCommands.h"
#include "apps/Time/TimeCommands.h"
#include "apps/Wall/WallCommands.h"
#include "apps/Yarn/YarnCommands.h"

void lobbsWireup()
{
    lobbsHooksReset();
    lobbsHelpRegisterCommands();
    lobbsAuthRegisterCommands();
    lobbsMailRegisterCommands();
    lobbsNewsRegisterCommands();
    lobbsYarnRegisterCommands();
    lobbsWallRegisterCommands();
    lobbsStatusRegisterCommands();
    lobbsTimeRegisterCommands();
    lobbsFsRegisterCommands();
}

#endif
