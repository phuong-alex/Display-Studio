#include <Arduino.h>
#include <ArduinoJson.h>

#include "display_studio/project/project_checksum.h"

namespace DisplayStudio::Project {
namespace {
uint32_t crc32(
    const uint8_t* data,
    size_t length
) {
    uint32_t crc = 0xFFFFFFFFUL;

    for (size_t index = 0; index < length; index++) {
        crc ^= data[index];

        for (uint8_t bit = 0; bit < 8; bit++) {
            const bool carry = crc & 1UL;
            crc >>= 1;

            if (carry) {
                crc ^= 0xEDB88320UL;
            }
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}
}

uint32_t ProjectChecksum::calculate(
    JsonVariantConst project
) {
    String serialized;
    serializeJson(project, serialized);

    return crc32(
        reinterpret_cast<const uint8_t*>(
            serialized.c_str()
        ),
        serialized.length()
    );
}

String ProjectChecksum::hex(
    uint32_t value
) {
    char buffer[11];

    snprintf(
        buffer,
        sizeof(buffer),
        "0x%08lX",
        static_cast<unsigned long>(value)
    );

    return String(buffer);
}
}
