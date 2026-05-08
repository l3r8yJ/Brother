#include <Arduino.h>

#include "display/Display.h"

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("===================");
    Serial.println("StarBoy boot v0.0.1");
    Serial.println("===================");
    Serial.printf("CPU:   %lu MHz\n", getCpuFrequencyMhz());
    Serial.printf("PSRAM: %lu KB\n", ESP.getPsramSize() / 1024);
    Serial.printf("Heap:  %lu KB free\n", ESP.getFreeHeap() / 1024);

    if (!Display::instance().begin()) {
        Serial.println("FATAL: display init failed");
        while (true) delay(1000);
    }

    Display::instance().fillScreen(0xF800); delay(400);
    Display::instance().fillScreen(0x07E0); delay(400);
    Display::instance().fillScreen(0x001F); delay(400);
    Display::instance().fillScreen(0x0000);

    Serial.println("Boot OK");
}

void loop() {
    delay(5000);
    Serial.println("StarBoy alive");
}
