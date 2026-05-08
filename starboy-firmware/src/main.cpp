#include <Arduino.h>

// Display module requires arduino-esp32 3.x + pioarduino platform.
// Enable after platform migration when board arrives.
// See docs/display-driver-research.md for migration plan.
// #include "display/Display.h"

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("===================");
    Serial.println("StarBoy boot v0.0.1");
    Serial.println("===================");
    Serial.printf("CPU:   %lu MHz\n", getCpuFrequencyMhz());
    Serial.printf("PSRAM: %lu KB\n", ESP.getPsramSize() / 1024);
    Serial.printf("Heap:  %lu KB free\n", ESP.getFreeHeap() / 1024);

    Serial.println("Boot OK");
    Serial.println("NOTE: display init deferred — needs arduino-esp32 3.x");
}

void loop() {
    delay(5000);
    Serial.println("StarBoy alive");
}
