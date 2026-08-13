#include "display_studio/runtime/runtime_state.h"

namespace DisplayStudio::Runtime {

const char* runtimeStateName(RuntimeState state) {
    switch (state) {
        case RuntimeState::Boot: return "boot";
        case RuntimeState::Idle: return "idle";
        case RuntimeState::Uploading: return "uploading";
        case RuntimeState::Installing: return "installing";
        case RuntimeState::Rendering: return "rendering";
        case RuntimeState::Error: return "error";
        default: return "unknown";
    }
}

}
