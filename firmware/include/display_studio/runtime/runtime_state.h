#pragma once

#include <stdint.h>

namespace DisplayStudio::Runtime {

enum class RuntimeState : uint8_t {
    Boot = 0,
    Idle,
    Uploading,
    Installing,
    Rendering,
    Error
};

const char* runtimeStateName(RuntimeState state);

}
