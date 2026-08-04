#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

#include "display_studio/config.h"
#include "display_studio/core/logger.h"
#include "display_studio/pins.h"
#include "display_studio/renderer/display_renderer.h"
#include "display_studio/version.h"

namespace DisplayStudio::Renderer {
namespace {
SPIClass displaySpi(FSPI);

GxEPD2_3C<
    GxEPD2_213_Z98c,
    GxEPD2_213_Z98c::HEIGHT
> eink(
    GxEPD2_213_Z98c(
        Pins::EINK_CS,
        Pins::EINK_DC,
        Pins::EINK_RST,
        Pins::EINK_BUSY
    )
);

DisplayRenderer instance;
}

DisplayRenderer& displayRenderer() {
    return instance;
}

void DisplayRenderer::centered(
    const String& text,
    int16_t baseline
) {
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t width = 0;
    uint16_t height = 0;

    eink.getTextBounds(
        text,
        0,
        baseline,
        &x1,
        &y1,
        &width,
        &height
    );

    eink.setCursor(
        (
            eink.width() -
            static_cast<int16_t>(width)
        ) / 2,
        baseline
    );

    eink.print(text);
}

void DisplayRenderer::header(
    const String& title
) {
    eink.fillScreen(GxEPD_WHITE);
    eink.setTextColor(GxEPD_RED);
    eink.setFont(&FreeSansBold12pt7b);
    centered(title, 27);

    eink.drawLine(
        8,
        37,
        eink.width() - 8,
        37,
        GxEPD_BLACK
    );
}

bool DisplayRenderer::initializeController(
    const String& reason,
    bool hardwareReset
) {
    Core::Logger::info(
        "E-ink controller init: " + reason +
        ", reset=" + String(hardwareReset ? 1 : 0) +
        ", BUSY=" + String(digitalRead(Pins::EINK_BUSY))
    );

    if (hardwareReset) {
        pinMode(Pins::EINK_CS, OUTPUT);
        digitalWrite(Pins::EINK_CS, HIGH);

        pinMode(Pins::EINK_RST, OUTPUT);
        digitalWrite(Pins::EINK_RST, LOW);
        delay(30);
        digitalWrite(Pins::EINK_RST, HIGH);
        delay(250);
    }

    eink.epd2.selectSPI(
        displaySpi,
        SPISettings(
            Config::SPI_FREQUENCY,
            MSBFIRST,
            SPI_MODE0
        )
    );

    eink.init(
        Config::SERIAL_BAUD,
        true,
        50,
        false
    );

    eink.setRotation(
        Config::DISPLAY_ROTATION
    );
    eink.setFullWindow();

    const bool ready =
        digitalRead(Pins::EINK_BUSY) == LOW;

    Core::Logger::info(
        String("E-ink controller ") +
        (ready ? "ready" : "not idle") +
        "; BUSY=" +
        String(digitalRead(Pins::EINK_BUSY))
    );

    return ready;
}

bool DisplayRenderer::begin() {
    pinMode(Pins::EINK_BUSY, INPUT);

    displaySpi.begin(
        Pins::EINK_SCK,
        Pins::EINK_MISO,
        Pins::EINK_MOSI,
        Pins::EINK_CS
    );

    return initializeController(
        "startup",
        true
    );
}

bool DisplayRenderer::recoverController() {
    Core::Logger::warning(
        "E-ink recovery requested"
    );

    delay(250);

    const bool recovered = initializeController(
        "automatic recovery",
        true
    );

    Core::Logger::info(
        recovered
            ? "E-ink recovery completed"
            : "E-ink recovery failed"
    );

    return recovered;
}

void DisplayRenderer::showBootScreen() {
    eink.setFullWindow();
    eink.firstPage();

    do {
        header("Display Studio");

        eink.setTextColor(GxEPD_BLACK);
        eink.setFont(&FreeSansBold12pt7b);
        centered("Sprint 1.1", 65);

        eink.setFont(&FreeSans9pt7b);
        centered("Connect Bluetooth", 91);
        centered(Version::FIRMWARE, 116);
    } while (eink.nextPage());
}

void DisplayRenderer::showWaitingForTime() {
    eink.setFullWindow();
    eink.firstPage();

    do {
        header("Display Studio");

        eink.setTextColor(GxEPD_BLACK);
        eink.setFont(&FreeSansBold12pt7b);
        centered("Project saved", 61);

        eink.setFont(&FreeSans9pt7b);
        centered("Connect Bluetooth", 88);
        centered("and sync time", 112);
    } while (eink.nextPage());
}

bool DisplayRenderer::showScene(
    const String& preset,
    const String& hour,
    const String& minute,
    const String& weekday,
    const String& dateText,
    const String& lunarText,
    bool redAccent
) {
    eink.setRotation(
        Config::DISPLAY_ROTATION
    );

    eink.setFullWindow();
    eink.firstPage();

    do {
        eink.fillScreen(GxEPD_WHITE);

        const bool showClock =
            preset != "calendar";

        const bool showCalendar =
            preset != "clock";

        if (showClock) {
            eink.setTextColor(
                redAccent
                    ? GxEPD_RED
                    : GxEPD_BLACK
            );

            eink.setFont(
                &FreeSansBold12pt7b
            );

            centered(
                hour + ":" + minute,
                showCalendar ? 45 : 68
            );
        }

        if (showCalendar) {
            eink.setTextColor(GxEPD_BLACK);
            eink.setFont(
                &FreeSansBold12pt7b
            );

            centered(
                weekday,
                showClock ? 74 : 47
            );

            eink.setFont(
                &FreeSans9pt7b
            );

            centered(
                dateText,
                showClock ? 98 : 78
            );

            if (!lunarText.isEmpty()) {
                centered(
                    lunarText,
                    showClock ? 119 : 108
                );
            }
        }
    } while (eink.nextPage());

    const bool ok =
        digitalRead(Pins::EINK_BUSY) == LOW;

    Core::Logger::info(
        "Display render complete; BUSY=" +
        String(digitalRead(Pins::EINK_BUSY))
    );

    return ok;
}
}
