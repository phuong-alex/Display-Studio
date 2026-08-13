#include "display_studio/runtime/runtime_command.h"

namespace DisplayStudio::Runtime {

const char* runtimeCommandName(RuntimeCommand command) {
    switch (command) {
        case RuntimeCommand::None: return "none";
        case RuntimeCommand::BeginUpload: return "begin_upload";
        case RuntimeCommand::CommitUpload: return "commit_upload";
        case RuntimeCommand::AbortUpload: return "abort_upload";
        case RuntimeCommand::Apply: return "apply";
        case RuntimeCommand::RenderActiveScene: return "render_active_scene";
        case RuntimeCommand::SetTime: return "set_time";
        case RuntimeCommand::GetRuntimeInfo: return "get_runtime_info";
        case RuntimeCommand::Reboot: return "reboot";
        case RuntimeCommand::FactoryReset: return "factory_reset";
        default: return "unknown";
    }
}

}
