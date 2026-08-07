#include <Arduino.h>
#include <esp_heap_caps.h>

#include "display_studio/core/logger.h"
#include "display_studio/project/package_installer.h"
#include "display_studio/project/project_model.h"
#include "display_studio/project/project_repository.h"
#include "display_studio/runtime/scene_runtime.h"
#include "display_studio/storage/project_storage.h"

namespace DisplayStudio::Project {
namespace {
PackageInstaller instance;
String heapSnapshot() {
    return "free=" + String(ESP.getFreeHeap()) +
        ", largest=" + String(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)) +
        ", min=" + String(ESP.getMinFreeHeap());
}
}

PackageInstaller& packageInstaller() { return instance; }

bool PackageInstaller::install(std::unique_ptr<JsonDocument> project) {
    lastError_ = "";
    if (!project || project->overflowed()) {
        lastError_ = "project_missing_or_overflowed";
        return false;
    }

    const JsonVariantConst root = project->as<JsonVariantConst>();
    Core::Logger::info("[INSTALL] zero-copy begin: bytes=" + String(measureJson(root)) + ", " + heapSnapshot());

    String validationError;
    if (!ProjectModel::validate(root, validationError)) {
        lastError_ = "validation:" + validationError;
        Core::Logger::error("[INSTALL] validation failed: " + lastError_);
        return false;
    }

    const JsonVariantConst scene = ProjectModel::activeScene(root);
    if (scene.isNull() || !Runtime::sceneRuntime().configure(scene["config"])) {
        lastError_ = "active_scene_config_failed";
        return false;
    }

    if (!Storage::projectStorage().save(root)) {
        lastError_ = "storage_save_failed";
        return false;
    }

    if (!Storage::projectStorage().verify(root)) {
        lastError_ = "storage_verify_failed";
        return false;
    }

    if (!projectRepository().adopt(std::move(project))) {
        lastError_ = "repository_adopt_failed";
        return false;
    }

    Core::Logger::info("[INSTALL] zero-copy complete: " + heapSnapshot());
    return true;
}

const String& PackageInstaller::lastError() const { return lastError_; }

}
