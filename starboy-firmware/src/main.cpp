#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("===================");
    Serial.println("StarBoy boot v0.0.1");
    Serial.println("===================");
    Serial.printf("CPU freq: %lu MHz\n", getCpuFrequencyMhz());
    Serial.printf("PSRAM size: %lu bytes\n", ESP.getPsramSize());
    Serial.printf("Free heap: %lu bytes\n", ESP.getFreeHeap());
}

void loop() {
    Serial.println("StarBoy alive");
    delay(5000);
}
