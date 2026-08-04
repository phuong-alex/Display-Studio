#pragma once

#include <Arduino.h>

namespace DisplayStudio::Renderer {
class DisplayRenderer {
public:
    bool begin();
    void showBootScreen();
    void showWaitingForTime();

    bool showScene(
        const String& preset,
        const String& hour,
        const String& minute,
        const String& weekday,
        const String& dateText,
        const String& lunarText,
        bool redAccent
    );

private:
    void centered(
        const String& text,
        int16_t baseline
    );
    void header(const String& title);
};

DisplayRenderer& displayRenderer();
}
