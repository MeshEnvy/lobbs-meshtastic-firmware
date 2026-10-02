#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSModule.h"
#include "LoBBSDb.h"
#include "LoBBSDispatch.h"
#include "LoBBSPaging.h"
#include "MeshService.h"
#include <cstdio>
#include <cstring>
#include <string>

LoBBSModule::LoBBSModule()
    : SinglePortModule("LoBBS", meshtastic_PortNum_TEXT_MESSAGE_APP) {
  db = new LoBBSDb(nodeDB->getNodeNum());
}

ProcessMessage LoBBSModule::handleReceived(const meshtastic_MeshPacket &mp) {
  return lobbsDispatchReceived(this, mp);
}

void lobbsBreadcrumb(const char *step)
{
  if (!step)
    return;
#if !defined(ARCH_PORTDUINO)
  // No flush: USB CDC flush can block the main loop long enough for the watchdog.
  Serial.print("INFO  | LoBBS ");
  Serial.println(step);
#else
  LOG_INFO("LoBBS %s", step);
#endif
}

void LoBBSModule::sendReply(const meshtastic_MeshPacket &req, const char *msg)
{
  if (!msg)
    return;
  char mark[40];
  snprintf(mark, sizeof(mark), "reply alloc %uB", (unsigned)strlen(msg));
  lobbsBreadcrumb(mark);
  meshtastic_MeshPacket *reply = allocDataPacket();
  if (!reply) {
    lobbsBreadcrumb("reply alloc failed");
    LOG_WARN("LoBBS: no packet for reply (pool exhausted)");
    return;
  }
  static constexpr char truncMarker[] = "[...]";
  static constexpr size_t truncMarkerLen = sizeof(truncMarker) - 1;

  size_t msgLen = strlen(msg);
  bool isTruncated = msgLen > MAX_REPLY_BYTES;
  size_t payloadSize = isTruncated ? MAX_REPLY_BYTES : msgLen;
  reply->decoded.payload.size = payloadSize;

  if (isTruncated) {
    size_t copyLen = MAX_REPLY_BYTES > truncMarkerLen ? MAX_REPLY_BYTES - truncMarkerLen : 0;
    memcpy(reply->decoded.payload.bytes, msg, copyLen);
    memcpy(reply->decoded.payload.bytes + copyLen, truncMarker, truncMarkerLen);
  } else {
    memcpy(reply->decoded.payload.bytes, msg, payloadSize);
  }
  setReplyTo(reply, req);
  reply->decoded.want_response = false;
  snprintf(mark, sizeof(mark), "reply send %08x", reply->id);
  lobbsBreadcrumb(mark);
  service->sendToMesh(reply);
  lobbsBreadcrumb("reply out");
}

void LoBBSModule::sendPagedReply(uint32_t sessionNodeId, const meshtastic_MeshPacket &req, const char *body)
{
  const char *page = nullptr;
  const char *err = nullptr;
  if (!lobbsPageStoreAndFirst(sessionNodeId, body, page, err)) {
    sendReply(req, err ? err : "Page error");
    return;
  }
  sendReply(req, page);
}

LoBBSModule *lobbsModule;

#endif
