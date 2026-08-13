#include "display_studio/runtime/runtime_context.h"

namespace DisplayStudio::Runtime {

RuntimeContext& runtimeContext() {
    static RuntimeContext context;
    return context;
}

}
