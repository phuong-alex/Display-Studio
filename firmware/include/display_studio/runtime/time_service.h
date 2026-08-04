#pragma once

#include <Arduino.h>

namespace DisplayStudio::Runtime {
class TimeService {
public:
    void begin();

    bool setFromBrowser(
        int64_t epochMs,
        int timezoneOffsetMinutes
    );

    bool ready() const;
    bool getLocalTime(tm& value) const;
    int timezoneOffsetMinutes() const;

private:
    int offsetMinutes_ = 420;
};

TimeService& timeService();
}
