#include <Arduino.h>
#include <sys/time.h>
#include <time.h>

#include "logger.h"
#include "studio_time.h"

namespace {
constexpr time_t VALID_EPOCH = 1700000000;
int offsetMinutes = 420;
}

namespace StudioTime {
void begin() {
}

bool setFromBrowser(
    int64_t epochMs,
    int timezoneOffsetMinutes
) {
    if (epochMs < 1700000000000LL) {
        Logger::error("Browser epoch is invalid");
        return false;
    }

    timeval value;
    value.tv_sec =
        static_cast<time_t>(epochMs / 1000LL);
    value.tv_usec =
        static_cast<suseconds_t>(
            (epochMs % 1000LL) * 1000LL
        );

    if (settimeofday(&value, nullptr) != 0) {
        Logger::error("settimeofday failed");
        return false;
    }

    offsetMinutes = constrain(
        timezoneOffsetMinutes,
        -720,
        840
    );

    Logger::info(
        "Browser time synchronized; offset=" +
        String(offsetMinutes) + " min"
    );

    return true;
}

bool ready() {
    return time(nullptr) > VALID_EPOCH;
}

bool getLocalTime(tm& value) {
    if (!ready()) {
        return false;
    }

    const time_t adjusted =
        time(nullptr) +
        static_cast<time_t>(offsetMinutes) * 60;

    gmtime_r(&adjusted, &value);
    return true;
}

int timezoneOffsetMinutes() {
    return offsetMinutes;
}
}
