#include <Arduino.h>
#include <memory>

#include "display_studio/core/logger.h"
#include "display_studio/project/project_model.h"
#include "display_studio/project/project_repository.h"
#include "display_studio/runtime/scene_runtime.h"
#include "display_studio/storage/project_storage.h"

namespace DisplayStudio::Project {
namespace {
ProjectRepository instance;
}

ProjectRepository& projectRepository() { return instance; }

bool ProjectRepository::load() {
    auto loaded = std::make_unique<JsonDocument>();
    if (!Storage::projectStorage().load(*loaded)) return false;

    String error;
    if (!ProjectModel::validate(loaded->as<JsonVariantConst>(), error)) {
        Core::Logger::error("[REPOSITORY] Stored Project invalid: " + error);
        return false;
    }

    const JsonVariantConst scene = ProjectModel::activeScene(loaded->as<JsonVariantConst>());
    if (scene.isNull() || !Runtime::sceneRuntime().configure(scene["config"])) {
        Core::Logger::error("[REPOSITORY] Active Scene configuration failed");
        return false;
    }

    project_ = std::move(loaded);
    Core::Logger::info("[REPOSITORY] Project loaded without Project copy");
    return true;
}

bool ProjectRepository::adopt(std::unique_ptr<JsonDocument> project) {
    if (!project || project->isNull() || project->overflowed()) return false;
    project_ = std::move(project);
    Core::Logger::info("[REPOSITORY] Project ownership adopted");
    return true;
}

bool ProjectRepository::installed() const { return static_cast<bool>(project_); }

JsonVariantConst ProjectRepository::document() const {
    return project_ ? project_->as<JsonVariantConst>() : JsonVariantConst();
}

JsonVariantConst ProjectRepository::activeScene() const {
    return project_ ? ProjectModel::activeScene(project_->as<JsonVariantConst>()) : JsonVariantConst();
}

JsonDocument* ProjectRepository::mutableDocument() { return project_.get(); }

void ProjectRepository::clearMemory() {
    project_.reset();
}

}
