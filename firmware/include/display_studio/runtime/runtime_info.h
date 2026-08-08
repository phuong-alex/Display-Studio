#pragma once

#include <ArduinoJson.h>

namespace DisplayStudio::Runtime {

// Fills a JSON document with live Runtime capability/diagnostic information.
// The transport layer only sends this payload; it does not calculate metrics.
void buildRuntimeInfo(JsonDocument& response);

}
