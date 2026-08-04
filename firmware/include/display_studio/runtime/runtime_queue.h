#pragma once

#include <Arduino.h>

namespace DisplayStudio::Runtime {

enum class RuntimeOperationType : uint8_t {
    RenderActiveScene = 1
};

enum class RuntimeWorkerState : uint8_t {
    Stopped = 0,
    Idle,
    Rendering,
    Error
};

using RuntimeCompletionCallback = void (*)(
    RuntimeOperationType type,
    bool ok,
    const String& requestId
);

class RuntimeQueue {
public:
    bool begin();

    bool enqueueRender(
        const String& requestId = ""
    );

    void setCompletionCallback(
        RuntimeCompletionCallback callback
    );

    RuntimeWorkerState state() const;
    size_t pending() const;

private:
    static void workerEntry(void* context);
    void workerLoop();
};

RuntimeQueue& runtimeQueue();

}
