#include <Arduino.h>
#include "display/Display.h"
#include "eyes/Eyes.h"

// Baseline binary size (2026-05-08): Flash 13.2%, RAM 7.4%

static void eyesTask(void*) {
    const TickType_t period = pdMS_TO_TICKS(33); // ~30fps
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        Eyes::instance().update();
        vTaskDelayUntil(&last, period);
    }
}

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

    // Color flash self-test
    Display::instance().fillScreen(0xF800); delay(400);
    Display::instance().fillScreen(0x07E0); delay(400);
    Display::instance().fillScreen(0x001F); delay(400);
    Display::instance().fillScreen(0x0000);

    Eyes::instance().begin();
    Eyes::instance().setEmotion(Emotion::IDLE);

    xTaskCreatePinnedToCore(eyesTask, "eyes", 4096, nullptr, 5, nullptr, 1);

    Serial.println("Boot OK");
}

void loop() {
    delay(5000);
    Serial.println("StarBoy alive");
}
