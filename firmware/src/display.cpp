#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include "config.h"
#include "display.h"
#include "logger.h"
#include "pins.h"
#include "version.h"

namespace {
SPIClass einkSpi(FSPI);
GxEPD2_3C<GxEPD2_213_Z98c, GxEPD2_213_Z98c::HEIGHT> eink(
    GxEPD2_213_Z98c(Pins::EINK_CS, Pins::EINK_DC, Pins::EINK_RST, Pins::EINK_BUSY));

void centered(const String& text, int16_t y) {
    int16_t x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;
    eink.getTextBounds(text.c_str(), 0, y, &x1, &y1, &w, &h);
    eink.setCursor((eink.width() - static_cast<int16_t>(w)) / 2, y);
    eink.print(text);
}

void header(const String& title) {
    eink.fillScreen(GxEPD_WHITE);
    eink.fillRect(0, 0, eink.width(), 25, GxEPD_RED);
    eink.setTextColor(GxEPD_WHITE);
    eink.setFont(&FreeSansBold12pt7b);
    centered(title, 20);
    eink.setTextColor(GxEPD_BLACK);
}
}

namespace Display {
bool begin() {
    // Use exactly the reset sequence that was proven to work in the
    // original calendar_lunar_display firmware.
    pinMode(Pins::EINK_BUSY, INPUT);
    einkSpi.begin(
        Pins::EINK_SCK,
        Pins::EINK_MISO,
        Pins::EINK_MOSI,
        Pins::EINK_CS
    );
    eink.epd2.selectSPI(
        einkSpi,
        SPISettings(
            Config::SPI_FREQUENCY,
            MSBFIRST,
            SPI_MODE0
        )
    );

    // serial_diag=true, initial=true, reset_duration=50 ms,
    // pulldown_rst_mode=false. The previous one-argument init() used
    // a shorter reset pulse and caused BUSY to remain asserted.
    eink.init(
        Config::SERIAL_BAUD,
        true,
        50,
        false
    );

    delay(100);

    Logger::info(
        "E-ink init complete; BUSY=" +
        String(digitalRead(Pins::EINK_BUSY))
    );

    eink.setRotation(Config::DISPLAY_ROTATION);
    eink.setFullWindow();
    return true;
}

void showBootScreen() {
    eink.firstPage();
    do {
        header("Display Studio");
        eink.setFont(&FreeSansBold12pt7b);
        centered("Alpha 2", 61);
        eink.setFont(&FreeSans9pt7b);
        centered(Version::FIRMWARE, 88);
        centered("Starting...", 112);
    } while (eink.nextPage());
}

void showConnecting(const String& ssid) {
    eink.firstPage();
    do {
        header("WiFi");
        eink.setFont(&FreeSansBold12pt7b);
        centered("Connecting", 58);
        eink.setFont(&FreeSans9pt7b);
        centered(ssid, 86);
        centered("Please wait...", 112);
    } while (eink.nextPage());
}

void showConnected(const String& ip, int32_t rssi) {
    eink.firstPage();
    do {
        header("WiFi Connected");
        eink.setFont(&FreeSansBold12pt7b);
        centered(ip, 58);
        eink.setFont(&FreeSans9pt7b);
        centered("RSSI: " + String(rssi) + " dBm", 86);
        centered("Opening service...", 112);
    } while (eink.nextPage());
}

void showPortal(const String& apName, const String& ip) {
    eink.firstPage();
    do {
        header("WiFi Setup");
        eink.setFont(&FreeSans9pt7b);
        centered("Connect to:", 50);
        eink.setFont(&FreeSansBold12pt7b);
        centered(apName, 76);
        eink.setFont(&FreeSans9pt7b);
        centered("Open " + ip, 104);
    } while (eink.nextPage());
}

void showServerStatus(const String& ip, const String& deviceId) {
    eink.firstPage();
    do {
        header("Device Online");
        eink.setFont(&FreeSansBold12pt7b);
        centered(ip, 55);
        eink.setFont(&FreeSans9pt7b);
        centered("ID: " + deviceId, 83);
        centered("Waiting image...", 109);
    } while (eink.nextPage());
}

bool showRawImage(const uint8_t* blackPlane, const uint8_t* redPlane, size_t planeBytes) {
    if (!blackPlane || !redPlane || planeBytes != Config::PLANE_BYTES) {
        Logger::error("Invalid raw image planes");
        return false;
    }

    // The server bitmap is in logical landscape coordinates: 250 x 122.
    // GxEPD2_213_Z98c low-level writeImage() expects the controller's
    // native portrait coordinates: 128 x 250. Rotation 1 maps:
    //   nativeX = 127 - logicalY
    //   nativeY = logicalX
    // The panel has 128 controller columns, of which 122 are visible.
    constexpr uint16_t nativeWidth = GxEPD2_213_Z98c::WIDTH;   // 128
    constexpr uint16_t nativeHeight = GxEPD2_213_Z98c::HEIGHT; // 250
    constexpr size_t nativePlaneBytes =
        (static_cast<size_t>(nativeWidth) * nativeHeight) / 8;

    uint8_t* nativeBlack = static_cast<uint8_t*>(calloc(nativePlaneBytes, 1));
    uint8_t* nativeRed = static_cast<uint8_t*>(calloc(nativePlaneBytes, 1));

    if (!nativeBlack || !nativeRed) {
        free(nativeBlack);
        free(nativeRed);
        Logger::error("Native display buffer allocation failed");
        return false;
    }

    uint32_t blackPixels = 0;
    uint32_t redPixels = 0;
    uint32_t checksum = 2166136261UL; // FNV-1a, diagnostic only

    for (uint16_t y = 0; y < Config::SCREEN_HEIGHT; ++y) {
        for (uint16_t x = 0; x < Config::SCREEN_WIDTH; ++x) {
            const size_t logicalIndex =
                static_cast<size_t>(y) * ((Config::SCREEN_WIDTH + 7) / 8) +
                (x / 8);
            const uint8_t logicalMask = 0x80 >> (x & 7);

            const bool isBlack = (blackPlane[logicalIndex] & logicalMask) != 0;
            const bool isRed = (redPlane[logicalIndex] & logicalMask) != 0;

            // Rotation 1 used by all existing UI screens.
            const uint16_t nativeX = nativeWidth - 1 - y;
            const uint16_t nativeY = x;
            const size_t nativeIndex =
                static_cast<size_t>(nativeY) * (nativeWidth / 8) +
                (nativeX / 8);
            const uint8_t nativeMask = 0x80 >> (nativeX & 7);

            // Black wins if both source planes accidentally contain the pixel.
            if (isBlack) {
                nativeBlack[nativeIndex] |= nativeMask;
                ++blackPixels;
            } else if (isRed) {
                nativeRed[nativeIndex] |= nativeMask;
                ++redPixels;
            }
        }
    }

    for (size_t i = 0; i < planeBytes; ++i) {
        checksum ^= blackPlane[i];
        checksum *= 16777619UL;
        checksum ^= redPlane[i];
        checksum *= 16777619UL;
    }

    Logger::info(
        "Direct EPD write: native=" + String(nativeWidth) + "x" +
        String(nativeHeight) + ", bytes=" + String(nativePlaneBytes) +
        ", black_px=" + String(blackPixels) +
        ", red_px=" + String(redPixels) +
        ", checksum=" + String(checksum, HEX)
    );

    // Use the panel driver's native two-plane API. The server planes use
    // 1 = coloured pixel, while this driver expects the opposite polarity
    // for its bitmap input, therefore invert=true is required.
    eink.epd2.writeScreenBuffer(0xFF, 0xFF);
    eink.epd2.writeImage(
        nativeBlack,
        nativeRed,
        0,
        0,
        nativeWidth,
        nativeHeight,
        true,   // invert: input has 1 = black/red pixel
        false,  // mirror_y
        false   // data is in RAM, not PROGMEM
    );

    Logger::info(
        "Controller RAM written; BUSY before refresh=" +
        String(digitalRead(Pins::EINK_BUSY))
    );

    eink.epd2.refresh(false);

    Logger::info(
        "Refresh returned; BUSY=" +
        String(digitalRead(Pins::EINK_BUSY))
    );

    eink.epd2.powerOff();
    Logger::info("Full refresh command completed");

    free(nativeBlack);
    free(nativeRed);
    return true;
}


void clear() {
    eink.setRotation(Config::DISPLAY_ROTATION);
    eink.setFullWindow();
    eink.firstPage();

    do {
        eink.fillScreen(GxEPD_WHITE);
    } while (eink.nextPage());
}

void refresh() {
    eink.refresh();
}

bool showNativeClock(
    const String& hour,
    const String& minute,
    const String& weekday,
    const String& dateText,
    bool redAccent
) {
    eink.setRotation(Config::DISPLAY_ROTATION);
    eink.setFullWindow();
    eink.firstPage();

    do {
        eink.fillScreen(GxEPD_WHITE);

        eink.setTextColor(
            redAccent ? GxEPD_RED : GxEPD_BLACK
        );
        eink.setFont(&FreeSansBold12pt7b);
        centered(hour + ":" + minute, 58);

        eink.setTextColor(GxEPD_BLACK);
        eink.setFont(&FreeSans9pt7b);
        centered(weekday, 88);
        centered(dateText, 112);
    } while (eink.nextPage());

    return true;
}

bool showNativeCalendar(
    const String& day,
    const String& month,
    const String& year,
    const String& weekday,
    const String& lunarText,
    bool redAccent
) {
    eink.setRotation(Config::DISPLAY_ROTATION);
    eink.setFullWindow();
    eink.firstPage();

    do {
        eink.fillScreen(GxEPD_WHITE);

        eink.setTextColor(
            redAccent ? GxEPD_RED : GxEPD_BLACK
        );
        eink.setFont(&FreeSansBold12pt7b);
        centered(day + "/" + month + "/" + year, 52);

        eink.setTextColor(GxEPD_BLACK);
        eink.setFont(&FreeSans9pt7b);
        centered(weekday, 82);
        centered(lunarText, 110);
    } while (eink.nextPage());

    return true;
}


bool showStudioScene(
    const String& preset,
    const String& hour,
    const String& minute,
    const String& weekday,
    const String& dateText,
    const String& lunarText,
    bool redAccent
) {
    eink.setRotation(Config::DISPLAY_ROTATION);
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

    const bool ok =
        digitalRead(Pins::EINK_BUSY) == LOW;

    Logger::info(
        "Studio display complete; BUSY=" +
        String(digitalRead(Pins::EINK_BUSY))
    );

    return ok;
}

void showStudioWaitingTime() {
    eink.setRotation(Config::DISPLAY_ROTATION);
    eink.setFullWindow();
    eink.firstPage();

    do {
        header("Display Studio");
        eink.setFont(&FreeSansBold12pt7b);
        centered("Scene saved", 61);
        eink.setFont(&FreeSans9pt7b);
        centered("Connect Bluetooth", 88);
        centered("and sync time", 112);
    } while (eink.nextPage());
}

void sleep() { eink.hibernate(); }
}
