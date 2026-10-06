#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSConfig.h"
#include <stddef.h>
#include <stdint.h>

class LoBBSModule;
typedef struct _meshtastic_MeshPacket meshtastic_MeshPacket;

enum class LoBBSInstallState : uint8_t { Blank, Ready, Offline };

static constexpr const char *LOBBS_INSTALL_MARKER_PATH = "/flash/lobbs.ls";
static constexpr uint32_t LOBBS_INSTALL_FIELD_ROOT = 0;

LoBBSInstallState lobbsInstallState(const LoBBSModule &mod);
const char *lobbsInstallRoot(const LoBBSModule &mod);
const char *lobbsInstallOfflineMount(const LoBBSModule &mod);

void lobbsInstallInit(LoBBSModule &mod);
void lobbsInstallDatabaseOpened(LoBBSModule &mod);
void lobbsInstallMountList(char *out, size_t cap);

bool lobbsInstallAuthorized(const meshtastic_MeshPacket &mp);
bool lobbsInstallWriteMarker(const char *root);
bool lobbsInstallReadMarker(char *rootOut, size_t cap);

#if LOBBS_SEED
void lobbsInstallAutoSeed(LoBBSModule &mod);
#endif

void lobbsInstallRegisterCommands();

#endif
