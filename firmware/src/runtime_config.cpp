#include <Preferences.h>

#include "runtime_config.h"

namespace {
constexpr const char* NAMESPACE = "eink-runtime";

uint32_t configVersion = 0;
uint32_t heartbeatMs = 30000;
uint32_t jobMs = 10000;
uint32_t commandMs = 5000;
uint32_t otaMs = 60000;
uint32_t sleepAfterUpdate = 0;
uint32_t nativeRefreshMs = 60000;

String currentNativeApp = "none";
String currentNtpServer = "pool.ntp.org";
String currentTimezone = "Asia/Ho_Chi_Minh";

uint32_t secondsToMs(
    JsonVariantConst value,
    uint32_t fallbackSeconds,
    uint32_t minimumSeconds,
    uint32_t maximumSeconds
) {
    const long parsed = value | static_cast<long>(fallbackSeconds);

    if (parsed < static_cast<long>(minimumSeconds) ||
        parsed > static_cast<long>(maximumSeconds)) {
        return fallbackSeconds * 1000UL;
    }

    return static_cast<uint32_t>(parsed) * 1000UL;
}

void save() {
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    prefs.putUInt("version", configVersion);
    prefs.putUInt("heartbeat", heartbeatMs);
    prefs.putUInt("job", jobMs);
    prefs.putUInt("command", commandMs);
    prefs.putUInt("ota", otaMs);
    prefs.putUInt("sleep", sleepAfterUpdate);
    prefs.putUInt("native_ms", nativeRefreshMs);
    prefs.putString("native_app", currentNativeApp);
    prefs.putString("ntp", currentNtpServer);
    prefs.putString("timezone", currentTimezone);
    prefs.end();
}
}

namespace RuntimeConfig {
void begin() {
    Preferences prefs;
    prefs.begin(NAMESPACE, true);
    configVersion = prefs.getUInt("version", 0);
    heartbeatMs = prefs.getUInt("heartbeat", 30000);
    jobMs = prefs.getUInt("job", 10000);
    commandMs = prefs.getUInt("command", 5000);
    otaMs = prefs.getUInt("ota", 60000);
    sleepAfterUpdate = prefs.getUInt("sleep", 0);
    nativeRefreshMs = prefs.getUInt("native_ms", 60000);
    currentNativeApp = prefs.getString("native_app", "none");
    currentNtpServer = prefs.getString("ntp", "pool.ntp.org");
    currentTimezone = prefs.getString(
        "timezone",
        "Asia/Ho_Chi_Minh"
    );
    prefs.end();
}

uint32_t version() { return configVersion; }
uint32_t heartbeatIntervalMs() { return heartbeatMs; }
uint32_t jobIntervalMs() { return jobMs; }
uint32_t commandIntervalMs() { return commandMs; }
uint32_t otaIntervalMs() { return otaMs; }
uint32_t sleepAfterUpdateSeconds() {
    return sleepAfterUpdate;
}
uint32_t nativeRefreshIntervalMs() {
    return nativeRefreshMs;
}
String nativeApp() { return currentNativeApp; }
String ntpServer() { return currentNtpServer; }
String timezone() { return currentTimezone; }

bool apply(JsonVariantConst configuration) {
    if (configuration.isNull()) {
        return false;
    }

    const uint32_t incomingVersion =
        configuration["version"] | configVersion;

    if (incomingVersion <= configVersion) {
        return false;
    }

    configVersion = incomingVersion;

    heartbeatMs = secondsToMs(
        configuration["heartbeat_interval"],
        30,
        10,
        600
    );

    jobMs = secondsToMs(
        configuration["job_interval"],
        10,
        2,
        600
    );

    commandMs = secondsToMs(
        configuration["command_interval"],
        5,
        2,
        600
    );

    otaMs = secondsToMs(
        configuration["ota_interval"],
        60,
        30,
        86400
    );

    const long sleepSeconds =
        configuration["sleep_after_update"] | 0;

    sleepAfterUpdate =
        sleepSeconds >= 0 && sleepSeconds <= 86400
            ? static_cast<uint32_t>(sleepSeconds)
            : 0;

    const long nativeMinutes =
        configuration["native_refresh_minutes"] | 1;

    nativeRefreshMs =
        static_cast<uint32_t>(
            constrain(nativeMinutes, 1L, 1440L)
        ) * 60UL * 1000UL;

    currentNativeApp =
        String(
            static_cast<const char*>(
                configuration["native_app"] | "none"
            )
        );

    if (
        currentNativeApp != "none" &&
        currentNativeApp != "clock" &&
        currentNativeApp != "calendar"
    ) {
        currentNativeApp = "none";
    }

    currentNtpServer =
        String(
            static_cast<const char*>(
                configuration["ntp_server"] |
                "pool.ntp.org"
            )
        );

    currentTimezone =
        String(
            static_cast<const char*>(
                configuration["timezone"] |
                "Asia/Ho_Chi_Minh"
            )
        );

    save();
    return true;
}
}
