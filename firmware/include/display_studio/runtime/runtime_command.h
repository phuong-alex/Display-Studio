#pragma once

#include <stdint.h>

namespace DisplayStudio::Runtime {

enum class RuntimeCommand : uint8_t {
    None = 0,
    BeginUpload,
    CommitUpload,
    AbortUpload,
    Apply,
    RenderActiveScene,
    SetTime,
    GetRuntimeInfo,
    Reboot,
    FactoryReset
};

const char* runtimeCommandName(RuntimeCommand command);

}
