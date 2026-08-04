#include <Arduino.h>
#include <sys/time.h>
#include <time.h>

#include "display_studio/core/logger.h"
#include "display_studio/runtime/time_service.h"

namespace DisplayStudio::Runtime {
namespace {
constexpr time_t VALID_EPOCH = 1700000000;
TimeService instance;
}

TimeService& timeService() {
    return instance;
}

void TimeService::begin() {
}

bool TimeService::setFromBrowser(
    int64_t epochMs,
    int timezoneOffsetMinutes
) {
    if (epochMs < 1700000000000LL) {
        Core::Logger::error(
            "Browser epoch is invalid"
        );
        return false;
    }

    timeval value;
    value.tv_sec =
        static_cast<time_t>(
            epochMs / 1000LL
        );

    value.tv_usec =
        static_cast<suseconds_t>(
            (epochMs % 1000LL) * 1000LL
        );

    if (settimeofday(&value, nullptr) != 0) {
        Core::Logger::error(
            "settimeofday failed"
        );
        return false;
    }

    offsetMinutes_ = constrain(
        timezoneOffsetMinutes,
        -720,
        840
    );

    Core::Logger::info(
        "Browser time synchronized; offset=" +
        String(offsetMinutes_) +
        " min"
    );

    return true;
}

bool TimeService::ready() const {
    return time(nullptr) > VALID_EPOCH;
}

bool TimeService::getLocalTime(
    tm& value
) const {
    if (!ready()) {
        return false;
    }

    const time_t adjusted =
        time(nullptr) +
        static_cast<time_t>(
            offsetMinutes_
        ) * 60;

    gmtime_r(&adjusted, &value);
    return true;
}

int TimeService::
timezoneOffsetMinutes() const {
    return offsetMinutes_;
}
}
