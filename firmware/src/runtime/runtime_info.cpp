#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

#include "display_studio/config.h"
#include "display_studio/project/project_manager.h"
#include "display_studio/runtime/runtime_info.h"
#include "display_studio/storage/project_storage.h"
#include "display_studio/version.h"

namespace DisplayStudio::Runtime {

void buildRuntimeInfo(JsonDocument& response) {
    response.clear();
    response["type"] = "runtime_info";
    response["firmware"] = Version::FIRMWARE;
    response["release"] = Version::RELEASE;
    response["protocolVersion"] = Version::PROTOCOL;
    response["uptimeMs"] = millis();

    JsonObject heap = response["heap"].to<JsonObject>();
    heap["free"] = ESP.getFreeHeap();
    heap["largest"] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    heap["minimum"] = ESP.getMinFreeHeap();

    Storage::ProjectStorage& storage = Storage::projectStorage();
    JsonObject filesystem = response["storage"].to<JsonObject>();
    filesystem["type"] = "littlefs";
    filesystem["total"] = storage.totalBytes();
    filesystem["used"] = storage.usedBytes();
    filesystem["free"] = storage.freeBytes();
    filesystem["projectBytes"] = storage.projectBytes();

    const bool installed = Project::projectManager().installed();
    JsonObject projectInfo = response["project"].to<JsonObject>();
    projectInfo["installed"] = installed;
    projectInfo["maxBytes"] = Config::MAX_PROJECT_BYTES;

    size_t sceneCount = 0;
    size_t projectBytes = 0;
    String activeSceneId;

    if (installed) {
        const JsonVariantConst project = Project::projectManager().document();
        sceneCount = project["scenes"].as<JsonArrayConst>().size();
        projectBytes = measureJson(project);
        activeSceneId = String(
            static_cast<const char*>(project["activeSceneId"] | "")
        );
    }

    projectInfo["bytes"] = projectBytes;
    projectInfo["scenes"] = sceneCount;
    projectInfo["activeSceneId"] = activeSceneId;

    JsonObject capabilities = response["capabilities"].to<JsonObject>();
    capabilities["dynamicSceneCount"] = true;
    capabilities["streamingUpload"] = true;
    capabilities["persistentProject"] = true;
    capabilities["storage"] = "littlefs";
    capabilities["maxProjectBytes"] = Config::MAX_PROJECT_BYTES;

    JsonObject display = response["display"].to<JsonObject>();
    display["width"] = Config::SCREEN_WIDTH;
    display["height"] = Config::SCREEN_HEIGHT;
    display["colorMode"] = "bwr";
    display["partialRefresh"] = false;
}

}
