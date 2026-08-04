#pragma once
#include <Arduino.h>
namespace Display {
bool begin();
void showBootScreen();
void showConnecting(const String& ssid);
void showConnected(const String& ip, int32_t rssi);
void showPortal(const String& apName, const String& ip);
void showServerStatus(const String& ip, const String& deviceId);
bool showRawImage(const uint8_t* blackPlane, const uint8_t* redPlane, size_t planeBytes);
void clear();
void refresh();
bool showNativeClock(const String& hour, const String& minute, const String& weekday,
                     const String& dateText, bool redAccent);
bool showNativeCalendar(const String& day, const String& month, const String& year,
                        const String& weekday, const String& lunarText,
                        bool redAccent);
bool showStudioScene(
    const String& preset,
    const String& hour,
    const String& minute,
    const String& weekday,
    const String& dateText,
    const String& lunarText,
    bool redAccent
);
void showStudioWaitingTime();
void sleep();
}
