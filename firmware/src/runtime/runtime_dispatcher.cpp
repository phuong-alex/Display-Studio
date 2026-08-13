#include <Arduino.h>
#include "display_studio/core/logger.h"
#include "display_studio/runtime/runtime_dispatcher.h"
#include "display_studio/runtime/runtime_info.h"

namespace DisplayStudio::Runtime {

RuntimeResult RuntimeDispatcher::execute(RuntimeCommand command, JsonDocument* output) {
    RuntimeContext& context = runtimeContext();
    const uint32_t startedAt = millis();
    RuntimeResult result;
    result.state = context.state;

    Core::Logger::info("[RUNTIME] Execute: " + String(runtimeCommandName(command)));

    if (command == RuntimeCommand::GetRuntimeInfo && output != nullptr) {
        buildRuntimeInfo(*output);
        result.success = true;
        result.error = RuntimeError::None;
        context.lastError = RuntimeError::None;
    } else {
        result.success = false;
        result.error = command == RuntimeCommand::GetRuntimeInfo
            ? RuntimeError::InvalidState
            : RuntimeError::Unknown;
        context.lastError = result.error;
    }

    result.state = context.state;
    result.elapsedMs = millis() - startedAt;
    Core::Logger::info(
        "[RUNTIME] Complete: " + String(runtimeCommandName(command)) +
        " ok=" + String(result.success ? "true" : "false") +
        " time=" + String(result.elapsedMs) + " ms"
    );
    return result;
}

RuntimeDispatcher& runtimeDispatcher() {
    static RuntimeDispatcher dispatcher;
    return dispatcher;
}

}
