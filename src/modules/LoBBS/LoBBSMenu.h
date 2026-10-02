#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "MeshModule.h"
#include "lobbs.pb.h"
#include <stdint.h>

class LoBBSModule;
class LoBBSDal;
typedef struct _meshtastic_MeshPacket meshtastic_MeshPacket;
typedef struct _meshtastic_LoBBSUser meshtastic_LoBBSUser;

enum class LobbsMenuKeyResult { NotHandled, Handled };

// Exact-line ? << < before prompts. p / pN paging without clearing menu state.
LobbsMenuKeyResult lobbsMenuTryGlobalKeys(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId,
                                          LoBBSDal *dal, bool isAuth, const meshtastic_LoBBSUser *user, bool isAdmin,
                                          const char *line);

// Non-slash menu input (digits, prompt answers).
void lobbsMenuHandleLine(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId, LoBBSDal *dal,
                         bool isAuth, const meshtastic_LoBBSUser *user, bool isAdmin, const char *line);

// Same as sending ?
void lobbsMenuReprint(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId, LoBBSDal *dal,
                      bool isAuth, const meshtastic_LoBBSUser *user, bool isAdmin);

void lobbsMenuOnLogout(uint32_t sessionNodeId);

void lobbsMenuAfterMailList(uint32_t sessionNodeId, uint64_t inboxUuid);
void lobbsMenuAfterNewsList(uint32_t sessionNodeId);
void lobbsMenuAfterUserList(uint32_t sessionNodeId);

void lobbsMenuShowMailList(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId, LoBBSDal *dal,
                           const meshtastic_LoBBSUser *user, bool isAdmin, uint64_t inboxUuid);
void lobbsMenuShowMailRead(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId, LoBBSDal *dal,
                           const meshtastic_LoBBSUser *user, bool isAdmin, uint64_t inboxUuid, uint32_t idx);
void lobbsMenuShowNewsList(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId, LoBBSDal *dal,
                           const meshtastic_LoBBSUser *user, bool isAdmin);
void lobbsMenuShowNewsRead(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId, LoBBSDal *dal,
                           const meshtastic_LoBBSUser *user, bool isAdmin, uint32_t idx);

#endif
