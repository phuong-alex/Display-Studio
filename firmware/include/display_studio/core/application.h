#pragma once

namespace DisplayStudio::Core {
class Application {
public:
    void setup();
    void loop();
};

Application& application();
}
