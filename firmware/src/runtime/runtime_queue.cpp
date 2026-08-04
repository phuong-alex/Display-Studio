#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "display_studio/core/logger.h"
#include "display_studio/runtime/runtime_queue.h"
#include "display_studio/runtime/scene_runtime.h"

namespace DisplayStudio::Runtime {
namespace {
constexpr size_t RUNTIME_QUEUE_DEPTH = 4;
constexpr uint32_t RENDER_TASK_STACK = 8192;
constexpr UBaseType_t RENDER_TASK_PRIORITY = 1;
constexpr BaseType_t RENDER_TASK_CORE = 0;
constexpr size_t REQUEST_ID_CAPACITY = 64;

struct RuntimeOperation {
    RuntimeOperationType type;
    char requestId[REQUEST_ID_CAPACITY];
};

QueueHandle_t operationQueue = nullptr;
TaskHandle_t workerTask = nullptr;
RuntimeCompletionCallback completionCallback = nullptr;
volatile RuntimeWorkerState workerState =
    RuntimeWorkerState::Stopped;

RuntimeQueue instance;
}

RuntimeQueue& runtimeQueue() {
    return instance;
}

bool RuntimeQueue::begin() {
    if (operationQueue && workerTask) {
        return true;
    }

    operationQueue = xQueueCreate(
        RUNTIME_QUEUE_DEPTH,
        sizeof(RuntimeOperation)
    );

    if (!operationQueue) {
        Core::Logger::error(
            "Runtime Queue initialization failed"
        );
        workerState = RuntimeWorkerState::Error;
        return false;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        workerEntry,
        "display-renderer",
        RENDER_TASK_STACK,
        this,
        RENDER_TASK_PRIORITY,
        &workerTask,
        RENDER_TASK_CORE
    );

    if (created != pdPASS) {
        vQueueDelete(operationQueue);
        operationQueue = nullptr;
        workerTask = nullptr;
        workerState = RuntimeWorkerState::Error;
        Core::Logger::error(
            "Renderer Worker creation failed"
        );
        return false;
    }

    workerState = RuntimeWorkerState::Idle;
    Core::Logger::info(
        "Runtime Queue ready: depth=" +
        String(RUNTIME_QUEUE_DEPTH)
    );
    return true;
}

bool RuntimeQueue::enqueueRender(
    const String& requestId
) {
    if (!operationQueue) {
        Core::Logger::error(
            "Runtime Queue is not initialized"
        );
        return false;
    }

    RuntimeOperation operation{};
    operation.type =
        RuntimeOperationType::RenderActiveScene;

    requestId.substring(
        0,
        REQUEST_ID_CAPACITY - 1
    ).toCharArray(
        operation.requestId,
        REQUEST_ID_CAPACITY
    );

    if (
        xQueueSend(
            operationQueue,
            &operation,
            0
        ) != pdTRUE
    ) {
        Core::Logger::warning(
            "Runtime Queue full; render rejected"
        );
        return false;
    }

    Core::Logger::info(
        "Runtime operation queued: render" +
        (
            requestId.isEmpty()
                ? ""
                : " [" + requestId + "]"
        )
    );
    return true;
}

void RuntimeQueue::setCompletionCallback(
    RuntimeCompletionCallback callback
) {
    completionCallback = callback;
}

RuntimeWorkerState RuntimeQueue::state() const {
    return workerState;
}

size_t RuntimeQueue::pending() const {
    return operationQueue
        ? uxQueueMessagesWaiting(operationQueue)
        : 0;
}

void RuntimeQueue::workerEntry(void* context) {
    static_cast<RuntimeQueue*>(context)
        ->workerLoop();
}

void RuntimeQueue::workerLoop() {
    RuntimeOperation operation{};

    for (;;) {
        if (
            xQueueReceive(
                operationQueue,
                &operation,
                portMAX_DELAY
            ) != pdTRUE
        ) {
            continue;
        }

        bool ok = false;

        switch (operation.type) {
            case RuntimeOperationType::RenderActiveScene:
                workerState =
                    RuntimeWorkerState::Rendering;
                Core::Logger::info(
                    "Renderer Worker started"
                );
                ok = sceneRuntime().renderImmediate();
                break;
        }

        workerState = ok
            ? RuntimeWorkerState::Idle
            : RuntimeWorkerState::Error;

        Core::Logger::info(
            ok
                ? "Renderer Worker completed"
                : "Renderer Worker failed"
        );

        if (completionCallback) {
            completionCallback(
                operation.type,
                ok,
                String(operation.requestId)
            );
        }

        // A failed render is an operation failure, not a permanent
        // runtime lock. The worker must accept later operations.
        workerState = RuntimeWorkerState::Idle;
    }
}

}
