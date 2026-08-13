#include <Arduino.h>
#include "display_studio/core/logger.h"
#include "display_studio/runtime/runtime_dispatcher.h"
#include "display_studio/runtime/runtime_info.h"
#include "display_studio/runtime/scene_runtime.h"
#include "display_studio/runtime/time_service.h"

namespace DisplayStudio::Runtime {

namespace {
RuntimeResult finishResult(
    RuntimeCommand command,
    RuntimeContext& context,
    bool success,
    RuntimeError error,
    uint32_t startedAt
) {
    RuntimeResult result;
    result.success = success;
    result.error = error;
    context.lastError = error;
    result.state = context.state;
    result.elapsedMs = millis() - startedAt;

    Core::Logger::info(
        "[RUNTIME] Complete: " + String(runtimeCommandName(command)) +
        " ok=" + String(result.success ? "true" : "false") +
        " time=" + String(result.elapsedMs) + " ms"
    );
    return result;
}
}

RuntimeResult RuntimeDispatcher::execute(RuntimeCommand command, JsonDocument* output) {
    RuntimeContext& context = runtimeContext();
    const uint32_t startedAt = millis();

    Core::Logger::info("[RUNTIME] Execute: " + String(runtimeCommandName(command)));

    if (command == RuntimeCommand::GetRuntimeInfo && output != nullptr) {
        buildRuntimeInfo(*output);
        return finishResult(command, context, true, RuntimeError::None, startedAt);
    }

    return finishResult(
        command,
        context,
        false,
        command == RuntimeCommand::GetRuntimeInfo ? RuntimeError::InvalidState : RuntimeError::Unknown,
        startedAt
    );
}

RuntimeResult RuntimeDispatcher::executeSetTime(const SetTimeCommandArgs& args) {
    RuntimeContext& context = runtimeContext();
    const uint32_t startedAt = millis();
    const RuntimeCommand command = RuntimeCommand::SetTime;

    Core::Logger::info("[RUNTIME] Execute: " + String(runtimeCommandName(command)));

    const bool ok = timeService().setFromBrowser(args.epochMs, args.timezoneOffsetMinutes);
    return finishResult(
        command,
        context,
        ok,
        ok ? RuntimeError::None : RuntimeError::Unknown,
        startedAt
    );
}

RuntimeResult RuntimeDispatcher::executeApply() {
    RuntimeContext& context = runtimeContext();
    const uint32_t startedAt = millis();
    const RuntimeCommand command = RuntimeCommand::Apply;

    Core::Logger::info("[RUNTIME] Execute: " + String(runtimeCommandName(command)));

    const bool rendered = sceneRuntime().renderNow();
    return finishResult(
        command,
        context,
        rendered,
        rendered ? RuntimeError::None : RuntimeError::RendererFailure,
        startedAt
    );
}

RuntimeDispatcher& runtimeDispatcher() {
    static RuntimeDispatcher dispatcher;
    return dispatcher;
}

}
