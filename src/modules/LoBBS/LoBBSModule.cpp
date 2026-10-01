#if !MESHTASTIC_EXCLUDE_LOBBS

#include "LoBBSModule.h"
#include "LoBBSDal.h"
#include "LoBBSDispatch.h"
#include "LoBBSPaging.h"
#include "MeshService.h"
#include <cstring>
#include <string>

LoBBSModule::LoBBSModule()
    : SinglePortModule("LoBBS", meshtastic_PortNum_TEXT_MESSAGE_APP) {
  dal = new LoBBSDal(nodeDB->getNodeNum());
}

ProcessMessage LoBBSModule::handleReceived(const meshtastic_MeshPacket &mp) {
  return lobbsDispatchReceived(this, mp);
}

void LoBBSModule::sendReply(const meshtastic_MeshPacket &req, const std::string &msg)
{
  meshtastic_MeshPacket *reply = allocDataPacket();
  static constexpr char truncMarker[] = "[...]";
  static constexpr size_t truncMarkerLen = sizeof(truncMarker) - 1;

  bool isTruncated = msg.size() > MAX_REPLY_BYTES;
  size_t payloadSize = isTruncated ? MAX_REPLY_BYTES : msg.size();
  reply->decoded.payload.size = payloadSize;

  if (isTruncated) {
    size_t copyLen = MAX_REPLY_BYTES > truncMarkerLen ? MAX_REPLY_BYTES - truncMarkerLen : 0;
    memcpy(reply->decoded.payload.bytes, msg.c_str(), copyLen);
    memcpy(reply->decoded.payload.bytes + copyLen, truncMarker, truncMarkerLen);
  } else {
    memcpy(reply->decoded.payload.bytes, msg.c_str(), payloadSize);
  }
  setReplyTo(reply, req);
  reply->decoded.want_response = false;
  service->sendToMesh(reply);
}

void LoBBSModule::sendPagedReply(uint32_t sessionNodeId, const meshtastic_MeshPacket &req, const std::string &body)
{
  std::string page;
  std::string err;
  if (!lobbsPageStoreAndFirst(sessionNodeId, body, page, err)) {
    sendReply(req, err);
    return;
  }
  sendReply(req, page);
}

LoBBSModule *lobbsModule;

#endif
