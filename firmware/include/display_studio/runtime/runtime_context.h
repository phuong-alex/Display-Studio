#pragma once

#include <stddef.h>
#include <stdint.h>
#include "display_studio/runtime/runtime_state.h"

namespace DisplayStudio::Runtime {

struct RuntimeStatistics {
    uint32_t uploadMs = 0;
    uint32_t installMs = 0;
    uint32_t renderMs = 0;
    size_t heapFree = 0;
    size_t heapMinimum = 0;
    size_t heapLargest = 0;
};

struct RuntimeCapabilities {
    bool dynamicSceneCount = true;
    bool streamingUpload = true;
    bool persistentProject = true;
    size_t maxProjectBytes = 0;
};

struct RuntimeProjectInfo {
    bool installed = false;
    size_t bytes = 0;
    size_t scenes = 0;
};

enum class RuntimeError : uint8_t {
    None = 0,
    InvalidState,
    InvalidProject,
    CrcMismatch,
    StorageFailure,
    RendererBusy,
    RendererFailure,
    Unknown
};

struct RuntimeContext {
    RuntimeState state = RuntimeState::Boot;
    RuntimeStatistics statistics{};
    RuntimeCapabilities capabilities{};
    RuntimeProjectInfo project{};
    RuntimeError lastError = RuntimeError::None;
};

RuntimeContext& runtimeContext();

}
