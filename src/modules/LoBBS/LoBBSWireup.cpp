#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSWireup.h"
#include "LoBBSHooks.h"
#include "LoBBSCommandRegistry.h"
#include "apps/Auth/AuthCommands.h"
#include "apps/Help/HelpCommands.h"
#include "apps/Mail/MailCommands.h"
#include "apps/News/NewsCommands.h"
#include "apps/Yarn/YarnCommands.h"
#include "apps/Wall/WallCommands.h"
#include "apps/Status/StatusCommands.h"

void lobbsWireup()
{
    lobbsFiltersReset();
    lobbsHelpRegisterCommands();
    lobbsAuthRegisterCommands();
    lobbsMailRegisterCommands();
    lobbsNewsRegisterCommands();
    lobbsYarnRegisterCommands();
    lobbsWallRegisterCommands();
    lobbsAuthRegisterHelpTopicsAfterApps();
    lobbsStatusRegisterCommands();

    LoBBSFilterCommands cmds{};
    lobbsApplyFilters("commands", &cmds, nullptr);
    lobbsCommandsInstall(cmds);
}

#endif
