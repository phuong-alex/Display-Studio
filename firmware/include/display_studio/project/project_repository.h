#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <memory>

namespace DisplayStudio::Project {

class ProjectRepository {
public:
    bool load();
    bool adopt(std::unique_ptr<JsonDocument> project);
    bool installed() const;
    JsonVariantConst document() const;
    JsonVariantConst activeScene() const;
    JsonDocument* mutableDocument();
    void clearMemory();

private:
    std::unique_ptr<JsonDocument> project_;
};

ProjectRepository& projectRepository();

}
