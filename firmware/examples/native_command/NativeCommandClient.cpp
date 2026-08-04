#include "NativeCommandClient.h"

NativeCommand NativeCommandClient::poll() {
  NativeCommand result;
  HTTPClient http;

  const String url =
      serverUrl_ +
      "/api/device/command?device_id=" +
      deviceId_;

  if (!http.begin(url)) {
    return result;
  }

  const int code = http.GET();

  if (code == HTTP_CODE_NO_CONTENT) {
    http.end();
    return result;
  }

  if (code != HTTP_CODE_OK) {
    http.end();
    return result;
  }

  JsonDocument document;
  const DeserializationError error =
      deserializeJson(document, http.getString());

  http.end();

  if (error) {
    return result;
  }

  result.available = true;
  result.id =
      document["command"]["id"] | "";
  result.type =
      document["command"]["type"] | "";

  result.payload.set(
      document["command"]["payload"]
  );

  return result;
}

bool NativeCommandClient::complete(
    const String& commandId,
    bool ok,
    const String& message) {
  HTTPClient http;

  if (!http.begin(
      serverUrl_ + "/api/device/command-done")) {
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  JsonDocument document;
  document["device_id"] = deviceId_;
  document["command_id"] = commandId;
  document["ok"] = ok;
  document["message"] = message;

  String body;
  serializeJson(document, body);

  const int code = http.POST(body);
  http.end();

  return code >= 200 && code < 300;
}
