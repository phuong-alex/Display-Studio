#include <Arduino.h>

#include "display_studio/core/application.h"

void setup() {
    DisplayStudio::Core::application().setup();
}

void loop() {
    DisplayStudio::Core::application().loop();
}
