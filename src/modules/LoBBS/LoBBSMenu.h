#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "apps/Auth/auth.pb.h"
#include <stdint.h>

class LoBBSModule;
typedef struct _meshtastic_MeshPacket meshtastic_MeshPacket;
typedef struct _meshtastic_LoBBSUser meshtastic_LoBBSUser;

enum class LobbsMenuKeyResult { NotHandled, Handled };

LobbsMenuKeyResult lobbsMenuTryGlobalKeys(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId,
                                          bool isAuth, const meshtastic_LoBBSUser *user, bool isAdmin, const char *line);

void lobbsMenuHandleLine(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId, bool isAuth,
                         const meshtastic_LoBBSUser *user, bool isAdmin, const char *line);

void lobbsMenuReprint(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId, bool isAuth,
                      const meshtastic_LoBBSUser *user, bool isAdmin);

void lobbsMenuOnLogout(uint32_t sessionNodeId);

void lobbsMenuAfterMailList(uint32_t sessionNodeId, uint64_t inboxUuid);
void lobbsMenuAfterNewsList(uint32_t sessionNodeId);
void lobbsMenuAfterUserList(uint32_t sessionNodeId);

void lobbsMenuShowMailList(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId,
                           const meshtastic_LoBBSUser *user, bool isAdmin, uint64_t inboxUuid);
void lobbsMenuShowMailRead(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId,
                           const meshtastic_LoBBSUser *user, bool isAdmin, uint64_t inboxUuid, uint32_t idx);
void lobbsMenuShowNewsList(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId,
                           const meshtastic_LoBBSUser *user, bool isAdmin);
void lobbsMenuShowNewsRead(LoBBSModule *mod, const meshtastic_MeshPacket &mp, uint32_t sessionNodeId,
                           const meshtastic_LoBBSUser *user, bool isAdmin, uint32_t idx);

#endif
