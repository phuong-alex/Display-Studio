#include <Arduino.h>
#include <memory>

#include "display_studio/core/logger.h"
#include "display_studio/project/package_installer.h"
#include "display_studio/project/project_manager.h"
#include "display_studio/project/project_model.h"
#include "display_studio/project/project_repository.h"
#include "display_studio/runtime/scene_runtime.h"
#include "display_studio/storage/project_storage.h"

namespace DisplayStudio::Project {
namespace {
ProjectManager instance;
}

ProjectManager& projectManager() { return instance; }
void ProjectManager::begin() {}

bool ProjectManager::load() {
    lastError_ = "";
    if (projectRepository().load()) return true;

    // Compatibility migration path only. Normal Project V1 boot uses one document.
    auto stored = std::make_unique<JsonDocument>();
    if (!Storage::projectStorage().load(*stored)) {
        lastError_ = "storage_load_failed";
        return false;
    }
    if (!ProjectModel::isLegacyConfig(stored->as<JsonVariantConst>())) {
        lastError_ = "repository_load_failed";
        return false;
    }

    auto migrated = std::make_unique<JsonDocument>();
    if (!ProjectModel::migrateLegacy(stored->as<JsonVariantConst>(), *migrated)) {
        lastError_ = "legacy_migration_failed";
        return false;
    }
    stored.reset();

    if (!packageInstaller().install(std::move(migrated))) {
        lastError_ = packageInstaller().lastError();
        return false;
    }
    return true;
}

bool ProjectManager::install(JsonVariantConst project) {
    // Compatibility path for old set_project clients. Streaming upload bypasses
    // this copy and transfers its parsed JsonDocument directly to PackageInstaller.
    auto owned = std::make_unique<JsonDocument>();
    owned->set(project);
    if (owned->overflowed()) {
        lastError_ = "compatibility_copy_overflow";
        return false;
    }
    if (!packageInstaller().install(std::move(owned))) {
        lastError_ = packageInstaller().lastError();
        return false;
    }
    lastError_ = "";
    return true;
}

bool ProjectManager::installLegacy(JsonVariantConst legacy) {
    auto migrated = std::make_unique<JsonDocument>();
    if (!ProjectModel::migrateLegacy(legacy, *migrated)) {
        lastError_ = "legacy_migration_failed";
        return false;
    }
    if (!packageInstaller().install(std::move(migrated))) {
        lastError_ = packageInstaller().lastError();
        return false;
    }
    lastError_ = "";
    return true;
}

bool ProjectManager::activateScene(const String& sceneId) {
    lastError_ = "";
    JsonDocument* document = projectRepository().mutableDocument();
    if (!document) {
        lastError_ = "project_not_installed";
        return false;
    }

    const String previousId = String(static_cast<const char*>((*document)["activeSceneId"] | ""));
    if (!ProjectModel::setActiveScene(*document, sceneId)) {
        lastError_ = "scene_not_found_or_disabled";
        return false;
    }

    const JsonVariantConst scene = ProjectModel::activeScene(document->as<JsonVariantConst>());
    if (scene.isNull() || !Runtime::sceneRuntime().configure(scene["config"])) {
        ProjectModel::setActiveScene(*document, previousId);
        lastError_ = "active_scene_config_failed";
        return false;
    }

    if (!Storage::projectStorage().save(document->as<JsonVariantConst>()) ||
        !Storage::projectStorage().verify(document->as<JsonVariantConst>())) {
        ProjectModel::setActiveScene(*document, previousId);
        const JsonVariantConst oldScene = ProjectModel::activeScene(document->as<JsonVariantConst>());
        if (!oldScene.isNull()) Runtime::sceneRuntime().configure(oldScene["config"]);
        lastError_ = "storage_save_or_verify_failed";
        return false;
    }

    Core::Logger::info("Active Scene changed without Project copy: " + sceneId);
    return true;
}

bool ProjectManager::installed() const { return projectRepository().installed(); }
JsonVariantConst ProjectManager::document() const { return projectRepository().document(); }
JsonVariantConst ProjectManager::activeScene() const { return projectRepository().activeScene(); }
const String& ProjectManager::lastError() const { return lastError_; }

void ProjectManager::clear() {
    projectRepository().clearMemory();
    lastError_ = "";
    Storage::projectStorage().clear();
    Runtime::sceneRuntime().clear();
}

}
