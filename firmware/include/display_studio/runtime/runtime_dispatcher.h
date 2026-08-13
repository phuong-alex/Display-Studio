#pragma once

#include <stdint.h>
#include <ArduinoJson.h>

#include "display_studio/runtime/runtime_command.h"
#include "display_studio/runtime/runtime_context.h"

namespace DisplayStudio::Runtime {

struct RuntimeResult {
    bool success = false;
    RuntimeError error = RuntimeError::None;
    RuntimeState state = RuntimeState::Boot;
    uint32_t elapsedMs = 0;
};

struct SetTimeCommandArgs {
    int64_t epochMs = 0;
    int timezoneOffsetMinutes = 420;
};

class RuntimeDispatcher {
public:
    RuntimeResult execute(
        RuntimeCommand command,
        JsonDocument* output = nullptr
    );

    RuntimeResult executeSetTime(
        const SetTimeCommandArgs& args
    );
};

RuntimeDispatcher& runtimeDispatcher();

}
