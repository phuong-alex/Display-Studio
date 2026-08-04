#include <Arduino.h>
#include <time.h>

#include "display.h"
#include "logger.h"
#include "native_app.h"
#include "runtime_config.h"

namespace {
uint32_t lastRenderAt = 0;

String posixTimezone(const String& timezone) {
    if (timezone == "Asia/Ho_Chi_Minh") {
        return "ICT-7";
    }

    return timezone;
}

void configureTime() {
    const String tz = posixTimezone(
        RuntimeConfig::timezone()
    );

    configTzTime(
        tz.c_str(),
        RuntimeConfig::ntpServer().c_str()
    );
}

bool localTimeParts(
    String& hour,
    String& minute,
    String& weekday,
    String& day,
    String& month,
    String& year
) {
    struct tm value;

    if (!getLocalTime(&value, 2000)) {
        Logger::warning("NTP time is not ready");
        return false;
    }

    char buffer[20];

    strftime(buffer, sizeof(buffer), "%H", &value);
    hour = buffer;

    strftime(buffer, sizeof(buffer), "%M", &value);
    minute = buffer;

    strftime(buffer, sizeof(buffer), "%A", &value);
    weekday = buffer;

    strftime(buffer, sizeof(buffer), "%d", &value);
    day = buffer;

    strftime(buffer, sizeof(buffer), "%m", &value);
    month = buffer;

    strftime(buffer, sizeof(buffer), "%Y", &value);
    year = buffer;

    return true;
}
}

namespace NativeApp {
void begin() {
    configureTime();
    lastRenderAt = 0;
}

bool showClock(JsonVariantConst payload) {
    String hour;
    String minute;
    String weekday;
    String day;
    String month;
    String year;

    if (!localTimeParts(
            hour,
            minute,
            weekday,
            day,
            month,
            year
        )) {
        hour = String(
            static_cast<const char*>(
                payload["local"]["hour"] | "--"
            )
        );
        minute = String(
            static_cast<const char*>(
                payload["local"]["minute"] | "--"
            )
        );
        weekday = String(
            static_cast<const char*>(
                payload["local"]["weekday"] | ""
            )
        );
        day = String(
            static_cast<const char*>(
                payload["local"]["day"] | "--"
            )
        );
        month = String(
            static_cast<const char*>(
                payload["local"]["month"] | "--"
            )
        );
        year = String(
            static_cast<const char*>(
                payload["local"]["year"] | "----"
            )
        );
    }

    const bool redAccent =
        String(
            static_cast<const char*>(
                payload["accent"] | "black"
            )
        ) == "red";

    lastRenderAt = millis();

    return Display::showNativeClock(
        hour,
        minute,
        weekday,
        day + "/" + month + "/" + year,
        redAccent
    );
}

bool showCalendar(JsonVariantConst payload) {
    const String day = String(
        static_cast<const char*>(
            payload["local"]["day"] | "--"
        )
    );

    const String month = String(
        static_cast<const char*>(
            payload["local"]["month"] | "--"
        )
    );

    const String year = String(
        static_cast<const char*>(
            payload["local"]["year"] | "----"
        )
    );

    const String weekday = String(
        static_cast<const char*>(
            payload["local"]["weekday"] | ""
        )
    );

    const int lunarDay = payload["lunar"]["day"] | 0;
    const int lunarMonth = payload["lunar"]["month"] | 0;
    const bool lunarLeap =
        payload["lunar"]["leap"] | false;

    String lunarText = "Lunar: ";

    if (lunarDay > 0 && lunarMonth > 0) {
        lunarText +=
            String(lunarDay) + "/" +
            String(lunarMonth);

        if (lunarLeap) {
            lunarText += " leap";
        }
    } else {
        lunarText += "--/--";
    }

    const bool redAccent =
        String(
            static_cast<const char*>(
                payload["accent"] | "black"
            )
        ) == "red";

    lastRenderAt = millis();

    return Display::showNativeCalendar(
        day,
        month,
        year,
        weekday,
        lunarText,
        redAccent
    );
}

void loop() {
    const String app = RuntimeConfig::nativeApp();

    if (app == "none") {
        return;
    }

    if (
        lastRenderAt != 0 &&
        millis() - lastRenderAt <
            RuntimeConfig::nativeRefreshIntervalMs()
    ) {
        return;
    }

    JsonDocument emptyPayload;

    if (app == "clock") {
        showClock(emptyPayload.as<JsonVariantConst>());
    }

    // Automatic calendar rendering needs a lunar payload from
    // the server, so only explicit native-calendar commands render
    // the lunar date. Clock can refresh locally from NTP.
}
}
