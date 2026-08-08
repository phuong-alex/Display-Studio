#include <Arduino.h>
#include <esp_heap_caps.h>

#include "display_studio/core/logger.h"
#include "display_studio/project/project_manager.h"
#include "display_studio/project/project_checksum.h"
#include "display_studio/project/project_model.h"
#include "display_studio/runtime/scene_runtime.h"
#include "display_studio/storage/project_storage.h"

namespace DisplayStudio::Project {
namespace {
ProjectManager instance;

String heapSnapshot() {
    return "free=" + String(ESP.getFreeHeap()) +
        ", largest=" + String(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)) +
        ", min=" + String(ESP.getMinFreeHeap());
}
}

ProjectManager& projectManager() {
    return instance;
}

void ProjectManager::begin() {
}

bool ProjectManager::load() {
    lastError_ = "";
    JsonDocument stored;

    if (!Storage::projectStorage().load(stored)) {
        lastError_ = "storage_load_failed";
        return false;
    }

    if (ProjectModel::isLegacyConfig(stored)) {
        Core::Logger::warning("Legacy config found; migrating to Project");
        JsonDocument migrated;
        if (!ProjectModel::migrateLegacy(stored, migrated)) {
            lastError_ = "legacy_migration_failed";
            Core::Logger::error("Legacy migration failed");
            return false;
        }
        if (!install(migrated)) return false;
        Core::Logger::info("Legacy config migrated");
        return true;
    }

    String error;
    if (!ProjectModel::validate(stored, error)) {
        lastError_ = "validation:" + error;
        Core::Logger::error("Stored Project invalid: " + error);
        return false;
    }

    project_.clear();
    project_.set(stored.as<JsonVariantConst>());
    if (project_.overflowed()) {
        lastError_ = "stored_copy_overflow";
        Core::Logger::error("Stored Project copy overflowed");
        project_.clear();
        return false;
    }

    installed_ = true;
    Core::Logger::info(
        "Stored Project checksum: " +
        ProjectChecksum::hex(ProjectChecksum::calculate(project_.as<JsonVariantConst>()))
    );

    const JsonVariantConst scene = ProjectModel::activeScene(project_);
    if (scene.isNull() || !Runtime::sceneRuntime().configure(scene["config"])) {
        lastError_ = "active_scene_config_failed";
        Core::Logger::error("Active Scene configuration failed");
        installed_ = false;
        return false;
    }

    Core::Logger::info(
        "Project loaded: " +
        String(static_cast<const char*>(project_["name"] | "")) +
        ", " + heapSnapshot()
    );
    return true;
}

bool ProjectManager::install(JsonVariantConst project) {
    const uint32_t startedAt = millis();
    lastError_ = "";

    Core::Logger::info(
        "[INSTALL][1/6] ENTER validate: bytes=" + String(measureJson(project)) +
        ", scenes=" + String(project["scenes"].as<JsonArrayConst>().size()) +
        ", " + heapSnapshot()
    );

    String validationError;
    if (!ProjectModel::validate(project, validationError)) {
        lastError_ = "validation:" + validationError;
        Core::Logger::error("[INSTALL][1/6] FAIL " + lastError_);
        return false;
    }
    Core::Logger::info("[INSTALL][1/6] PASS validate");

    const uint32_t checksumStartedAt = millis();
    Core::Logger::info("[INSTALL][2/6] ENTER checksum");
    const uint32_t checksum = ProjectChecksum::calculate(project);
    Core::Logger::info(
        "[INSTALL][2/6] PASS checksum=" + ProjectChecksum::hex(checksum) +
        ", time=" + String(millis() - checksumStartedAt) + " ms"
    );

    Core::Logger::info("[INSTALL][3/6] ENTER active_scene_config");
    const JsonVariantConst scene = ProjectModel::activeScene(project);
    if (scene.isNull()) {
        lastError_ = "active_scene_not_found";
        Core::Logger::error("[INSTALL][3/6] FAIL " + lastError_);
        return false;
    }
    if (!Runtime::sceneRuntime().configure(scene["config"])) {
        lastError_ = "active_scene_config_failed";
        Core::Logger::error("[INSTALL][3/6] FAIL " + lastError_);
        return false;
    }
    Core::Logger::info("[INSTALL][3/6] PASS active_scene_config, " + heapSnapshot());

    const uint32_t saveStartedAt = millis();
    Core::Logger::info("[INSTALL][4/6] ENTER storage_save: " + heapSnapshot());
    if (!Storage::projectStorage().save(project)) {
        lastError_ = "storage_save_failed";
        Core::Logger::error("[INSTALL][4/6] FAIL " + lastError_ + ", " + heapSnapshot());
        return false;
    }
    Core::Logger::info(
        "[INSTALL][4/6] PASS storage_save, time=" +
        String(millis() - saveStartedAt) + " ms, " + heapSnapshot()
    );

    const uint32_t verifyStartedAt = millis();
    Core::Logger::info("[INSTALL][5/6] ENTER storage_verify: " + heapSnapshot());
    if (!Storage::projectStorage().verify(project)) {
        lastError_ = "storage_verify_failed";
        Core::Logger::error("[INSTALL][5/6] FAIL " + lastError_ + ", " + heapSnapshot());
        return false;
    }
    Core::Logger::info(
        "[INSTALL][5/6] PASS storage_verify, time=" +
        String(millis() - verifyStartedAt) + " ms, " + heapSnapshot()
    );

    const uint32_t copyStartedAt = millis();
    Core::Logger::info("[INSTALL][6/6] ENTER persistent_copy: " + heapSnapshot());
    project_.clear();
    project_.set(project);
    if (project_.overflowed()) {
        lastError_ = "persistent_copy_overflow";
        Core::Logger::error("[INSTALL][6/6] FAIL " + lastError_ + ", " + heapSnapshot());
        project_.clear();
        installed_ = false;
        return false;
    }

    installed_ = true;
    Core::Logger::info(
        "[INSTALL][6/6] PASS persistent_copy, bytes=" + String(measureJson(project_)) +
        ", time=" + String(millis() - copyStartedAt) + " ms, " + heapSnapshot()
    );
    Core::Logger::info(
        "[INSTALL] COMPLETE total=" + String(millis() - startedAt) +
        " ms, project=" + String(static_cast<const char*>(project_["name"] | ""))
    );
    return true;
}

bool ProjectManager::installLegacy(JsonVariantConst legacy) {
    JsonDocument migrated;
    if (!ProjectModel::migrateLegacy(legacy, migrated)) {
        lastError_ = "legacy_migration_failed";
        return false;
    }
    return install(migrated);
}

bool ProjectManager::activateScene(const String& sceneId) {
    lastError_ = "";
    if (!installed_) {
        lastError_ = "project_not_installed";
        return false;
    }

    JsonDocument candidate;
    candidate.set(project_.as<JsonVariantConst>());
    if (candidate.overflowed()) {
        lastError_ = "scene_activation_copy_overflow";
        Core::Logger::error("Scene activation copy overflowed");
        return false;
    }

    if (!ProjectModel::setActiveScene(candidate, sceneId)) {
        lastError_ = "scene_not_found_or_disabled";
        return false;
    }

    const JsonVariantConst scene = ProjectModel::activeScene(candidate);
    if (scene.isNull() || !Runtime::sceneRuntime().configure(scene["config"])) {
        lastError_ = "active_scene_config_failed";
        return false;
    }

    if (!Storage::projectStorage().save(candidate)) {
        lastError_ = "storage_save_failed";
        return false;
    }

    project_.clear();
    project_.set(candidate.as<JsonVariantConst>());
    if (project_.overflowed()) {
        lastError_ = "persistent_copy_overflow";
        project_.clear();
        installed_ = false;
        return false;
    }

    Core::Logger::info("Active Scene changed: " + sceneId);
    return true;
}

bool ProjectManager::installed() const { return installed_; }
JsonVariantConst ProjectManager::document() const { return project_.as<JsonVariantConst>(); }
JsonVariantConst ProjectManager::activeScene() const {
    if (!installed_) return JsonVariantConst();
    return ProjectModel::activeScene(project_);
}
const String& ProjectManager::lastError() const { return lastError_; }

void ProjectManager::clear() {
    project_.clear();
    installed_ = false;
    lastError_ = "";
    Storage::projectStorage().clear();
    Runtime::sceneRuntime().clear();
}
}
