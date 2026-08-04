#pragma once

#include <Arduino.h>

namespace DisplayStudio::Display {

enum class DisplayEngineState : uint8_t {
    Stopped = 0,
    Idle,
    CoolingDown,
    Refreshing,
    Error
};

class DisplayEngine {
public:
    bool begin();

    // Executes one physical refresh from the dedicated renderer task.
    // Calls are serialized and a recovery interval is enforced between
    // consecutive full-refresh operations.
    bool renderActiveScene();

    DisplayEngineState state() const;
    uint32_t lastRefreshFinishedAt() const;

private:
    bool waitForRecoveryWindow();
};

DisplayEngine& displayEngine();

}
