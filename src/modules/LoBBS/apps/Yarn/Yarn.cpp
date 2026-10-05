#if !MESHTASTIC_EXCLUDE_LOBBS

#include "Yarn.h"

#include "LoBBSStackGuard.h"

YarnApp::YarnApp(LoDb &lodb) : dal_(lodb) {}

#endif
