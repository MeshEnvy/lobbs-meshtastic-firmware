#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "../LoBBSHistory.h"

// Replace the stack with the guest or logged-in root menu. Does not draw.
void lobbsRootInstall(LobbsHistory *h);

#endif
