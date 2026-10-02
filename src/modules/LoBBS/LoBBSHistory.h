#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSConfig.h"
#include <stddef.h>
#include <stdint.h>

class LoBBSModule;
struct LobbsHistory;
struct LobbsFrame;
typedef struct _meshtastic_MeshPacket meshtastic_MeshPacket;
typedef struct _meshtastic_LoBBSUser meshtastic_LoBBSUser;

// Valid only for the duration of lobbsHistoryHandle / a door callback.
struct LobbsCtx {
    LoBBSModule *mod = nullptr;
    const meshtastic_MeshPacket *mp = nullptr;
    uint32_t sessionNodeId = 0;
    bool isAuth = false;
    const meshtastic_LoBBSUser *user = nullptr;
    bool isAdmin = false;
};

enum class LobbsFrameKind : uint8_t { Menu, Prompt };

struct LobbsItem {
    const char *label = nullptr;
    void (*onPick)(LobbsHistory *h, const LobbsFrame *self) = nullptr;
    // Writes a suffix into buf (include leading space). Empty means no mark.
    void (*status)(LobbsHistory *h, char *buf, size_t cap) = nullptr;
};

static constexpr uint8_t LOBBS_HISTORY_DEPTH = 8;
static constexpr uint8_t LOBBS_HISTORY_ITEMS = 9;

struct LobbsFrame {
    LobbsFrameKind kind = LobbsFrameKind::Menu;
    const char *tag = "";
    void (*draw)(LobbsHistory *h, const LobbsFrame *self) = nullptr;
    uint64_t arg0 = 0;
    uint32_t arg1 = 0;
    LobbsItem items[LOBBS_HISTORY_ITEMS] = {};
    uint8_t itemCount = 0;
    // Set on a list menu. A number calls this instead of items[].
    void (*onDigit)(LobbsHistory *h, const LobbsFrame *self, uint32_t number) = nullptr;
    const char *prompt = nullptr;
    void (*onSuccess)(LobbsHistory *h, const LobbsFrame *self, const char *line) = nullptr;
    void (*onCancel)(LobbsHistory *h, const LobbsFrame *self) = nullptr;
};

struct LobbsHistory {
    LobbsFrame stack[LOBBS_HISTORY_DEPTH] = {};
    uint8_t depth = 0;
    char scratch[LOBBS_USERNAME_BUFFER_SIZE] = {};
    char scratch2[LOBBS_USERNAME_BUFFER_SIZE] = {};
    LobbsCtx *ctx = nullptr;
    bool drew = false;
};

static const char *LOBBS_HIST_NAV = "\n? < << p";

void lobbsHistoryPush(LobbsHistory *h, const LobbsFrame &frame);
void lobbsHistoryPop(LobbsHistory *h);
void lobbsHistoryDraw(LobbsHistory *h);
void lobbsHistoryReply(LobbsHistory *h, const char *msg);

// Interprets ?, <, <<, p, menu numbers, and prompt text.
void lobbsHistoryHandle(LobbsHistory *h, LobbsCtx *ctx, const char *line);

#endif
