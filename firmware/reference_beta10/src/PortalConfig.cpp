#include "PortalConfig.h"
#include "DeviceDefaults.h"

bool PortalConfig::begin() {
  return preferences_.begin("eink-portal", false);
}

PortalValues PortalConfig::load() {
  PortalValues values;

  values.serverUrl = preferences_.getString(
    "server",
    DEFAULT_SERVER_URL
  );

  values.deviceId = preferences_.getString(
    "deviceId",
    DEFAULT_DEVICE_ID
  );

  values.deviceName = preferences_.getString(
    "deviceName",
    DEFAULT_DEVICE_NAME
  );

  return values;
}

bool PortalConfig::save(const PortalValues& values) {
  bool ok = true;

  ok &= preferences_.putString(
    "server",
    values.serverUrl
  ) > 0;

  ok &= preferences_.putString(
    "deviceId",
    values.deviceId
  ) > 0;

  ok &= preferences_.putString(
    "deviceName",
    values.deviceName
  ) > 0;

  return ok;
}
