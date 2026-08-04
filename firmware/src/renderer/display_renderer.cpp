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

constexpr uint32_t PANEL_READY_TIMEOUT_MS = 2000;

bool panelIdle() {
    return digitalRead(Pins::EINK_BUSY) == LOW;
}

bool waitForPanelIdle(uint32_t timeoutMs) {
    const uint32_t startedAt = millis();

    while (!panelIdle()) {
        if (millis() - startedAt >= timeoutMs) {
            return false;
        }
        delay(10);
        yield();
    }

    return true;
}

void configurePanel() {
    eink.setRotation(Config::DISPLAY_ROTATION);
    eink.setFullWindow();
}

bool initializeController(const String& reason) {
    Core::Logger::info(
        "E-ink controller init: " + reason +
        ", BUSY=" + String(digitalRead(Pins::EINK_BUSY))
    );

    // Always start a render transaction from a known controller state.
    // A LOW BUSY pin only reports the external pin level; it does not
    // guarantee that the controller's power/LUT state is still valid.
    eink.init(
        Config::SERIAL_BAUD,
        true,
        50,
        false
    );
    configurePanel();

    const bool ready =
        waitForPanelIdle(PANEL_READY_TIMEOUT_MS);

    Core::Logger::info(
        ready
            ? "E-ink controller ready"
            : "E-ink controller init failed; BUSY remains active"
    );

    return ready;
}

void finishTransaction(bool success) {
    if (!success) {
        return;
    }

    // Power down the panel after a completed full refresh. The next
    // render explicitly initializes it again, preventing stale state
    // from accumulating across boot, waiting and Scene refreshes.
    eink.hibernate();
    Core::Logger::info("E-ink controller hibernated");
}
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

bool DisplayRenderer::begin() {
    pinMode(Pins::EINK_BUSY, INPUT);

    displaySpi.begin(
        Pins::EINK_SCK,
        Pins::EINK_MISO,
        Pins::EINK_MOSI,
        Pins::EINK_CS
    );

    eink.epd2.selectSPI(
        displaySpi,
        SPISettings(
            Config::SPI_FREQUENCY,
            MSBFIRST,
            SPI_MODE0
        )
    );

    const bool ready = initializeController("startup");

    Core::Logger::info(
        "E-ink init complete; BUSY=" +
        String(digitalRead(Pins::EINK_BUSY))
    );

    return ready;
}

void DisplayRenderer::showBootScreen() {
    if (!initializeController("boot screen")) {
        return;
    }

    eink.firstPage();
    do {
        header("Display Studio");
        eink.setTextColor(GxEPD_BLACK);
        eink.setFont(&FreeSansBold12pt7b);
        centered("Sprint 1.5.3", 65);
        eink.setFont(&FreeSans9pt7b);
        centered("Connect Bluetooth", 91);
        centered(Version::FIRMWARE, 116);
    } while (eink.nextPage());

    const bool ok = panelIdle();
    finishTransaction(ok);
}

void DisplayRenderer::showWaitingForTime() {
    if (!initializeController("waiting screen")) {
        return;
    }

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

    const bool ok = panelIdle();
    finishTransaction(ok);
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
    if (!initializeController("Scene apply")) {
        Core::Logger::error(
            "Scene render aborted: controller not ready"
        );
        return false;
    }

    Core::Logger::info(
        "Display refresh starting; BUSY=" +
        String(digitalRead(Pins::EINK_BUSY))
    );

    eink.firstPage();
    do {
        eink.fillScreen(GxEPD_WHITE);

        const bool showClock = preset != "calendar";
        const bool showCalendar = preset != "clock";

        if (showClock) {
            eink.setTextColor(
                redAccent ? GxEPD_RED : GxEPD_BLACK
            );
            eink.setFont(&FreeSansBold12pt7b);
            centered(
                hour + ":" + minute,
                showCalendar ? 45 : 68
            );
        }

        if (showCalendar) {
            eink.setTextColor(GxEPD_BLACK);
            eink.setFont(&FreeSansBold12pt7b);
            centered(
                weekday,
                showClock ? 74 : 47
            );

            eink.setFont(&FreeSans9pt7b);
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

    const bool ok = panelIdle();

    Core::Logger::info(
        "Display render complete; BUSY=" +
        String(digitalRead(Pins::EINK_BUSY)) +
        (ok ? "" : "; controller refresh timeout")
    );

    finishTransaction(ok);
    return ok;
}
}
