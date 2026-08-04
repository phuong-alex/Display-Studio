#include <Arduino.h>

#include "display_studio/core/logger.h"
#include "display_studio/project/project_manager.h"
#include "display_studio/project/project_checksum.h"
#include "display_studio/project/project_model.h"
#include "display_studio/runtime/scene_runtime.h"
#include "display_studio/storage/project_storage.h"

namespace DisplayStudio::Project {
namespace {
ProjectManager instance;
}

ProjectManager& projectManager() {
    return instance;
}

void ProjectManager::begin() {
}

bool ProjectManager::load() {
    JsonDocument stored;

    if (
        !Storage::projectStorage().load(stored)
    ) {
        return false;
    }

    if (ProjectModel::isLegacyConfig(stored)) {
        Core::Logger::warning(
            "Legacy config found; migrating to Project"
        );

        JsonDocument migrated;

        if (
            !ProjectModel::migrateLegacy(
                stored,
                migrated
            )
        ) {
            Core::Logger::error(
                "Legacy migration failed"
            );
            return false;
        }

        if (!install(migrated)) {
            return false;
        }

        Core::Logger::info(
            "Legacy config migrated"
        );

        return true;
    }

    String error;

    if (
        !ProjectModel::validate(
            stored,
            error
        )
    ) {
        Core::Logger::error(
            "Stored Project invalid: " + error
        );
        return false;
    }

    project_.clear();
    project_.set(
        stored.as<JsonVariantConst>()
    );

    installed_ = true;

    Core::Logger::info(
        "Stored Project checksum: " +
        ProjectChecksum::hex(
            ProjectChecksum::calculate(
                project_.as<JsonVariantConst>()
            )
        )
    );

    const JsonVariantConst scene =
        ProjectModel::activeScene(project_);

    if (
        scene.isNull() ||
        !Runtime::sceneRuntime().configure(
            scene["config"]
        )
    ) {
        Core::Logger::error(
            "Active Scene configuration failed"
        );
        installed_ = false;
        return false;
    }

    Core::Logger::info(
        "Project loaded: " +
        String(
            static_cast<const char*>(
                project_["name"] | ""
            )
        )
    );

    return true;
}

bool ProjectManager::install(
    JsonVariantConst project
) {
    Core::Logger::info(
        "Receiving Project..."
    );

    String error;

    if (
        !ProjectModel::validate(
            project,
            error
        )
    ) {
        Core::Logger::error(
            "Project validation failed: " + error
        );
        return false;
    }

    const size_t sceneCount =
        project["scenes"]
            .as<JsonArrayConst>()
            .size();

    const uint32_t checksum =
        ProjectChecksum::calculate(project);

    Core::Logger::info(
        "Project validated"
    );

    Core::Logger::info(
        "Scene count: " +
        String(sceneCount)
    );

    Core::Logger::info(
        "Project checksum: " +
        ProjectChecksum::hex(checksum)
    );

    const JsonVariantConst scene =
        ProjectModel::activeScene(project);

    if (scene.isNull()) {
        Core::Logger::error(
            "Active Scene not found"
        );
        return false;
    }

    Core::Logger::info(
        "Configuring active Scene..."
    );

    if (
        !Runtime::sceneRuntime().configure(
            scene["config"]
        )
    ) {
        Core::Logger::error(
            "Active Scene configuration failed"
        );
        return false;
    }

    Core::Logger::info(
        "Saving Project..."
    );

    if (
        !Storage::projectStorage().save(
            project
        )
    ) {
        Core::Logger::error(
            "Project save failed"
        );
        return false;
    }

    Core::Logger::info(
        "Verifying stored Project..."
    );

    if (
        !Storage::projectStorage().verify(
            project
        )
    ) {
        Core::Logger::error(
            "Project storage verification failed"
        );
        return false;
    }

    JsonDocument installedProject;
    installedProject.set(project);

    project_.clear();
    project_.set(
        installedProject.as<JsonVariantConst>()
    );

    installed_ = true;

    Core::Logger::info(
        "Project installed and verified: " +
        String(
            static_cast<const char*>(
                project_["name"] | ""
            )
        )
    );

    return true;
}

bool ProjectManager::installLegacy(
    JsonVariantConst legacy
) {
    JsonDocument migrated;

    if (
        !ProjectModel::migrateLegacy(
            legacy,
            migrated
        )
    ) {
        return false;
    }

    return install(migrated);
}

bool ProjectManager::activateScene(
    const String& sceneId
) {
    if (!installed_) {
        return false;
    }

    JsonDocument candidate;
    candidate.set(
        project_.as<JsonVariantConst>()
    );

    if (
        !ProjectModel::setActiveScene(
            candidate,
            sceneId
        )
    ) {
        return false;
    }

    const JsonVariantConst scene =
        ProjectModel::activeScene(candidate);

    if (
        scene.isNull() ||
        !Runtime::sceneRuntime().configure(
            scene["config"]
        )
    ) {
        return false;
    }

    if (
        !Storage::projectStorage().save(
            candidate
        )
    ) {
        return false;
    }

    project_.clear();
    project_.set(
        candidate.as<JsonVariantConst>()
    );

    Core::Logger::info(
        "Active Scene changed: " +
        sceneId
    );

    return true;
}

bool ProjectManager::installed() const {
    return installed_;
}

JsonVariantConst ProjectManager::document() const {
    return project_.as<JsonVariantConst>();
}

JsonVariantConst ProjectManager::activeScene() const {
    if (!installed_) {
        return JsonVariantConst();
    }

    return ProjectModel::activeScene(project_);
}

void ProjectManager::clear() {
    project_.clear();
    installed_ = false;

    Storage::projectStorage().clear();
    Runtime::sceneRuntime().clear();
}
}
