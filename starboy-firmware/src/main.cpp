#include <Arduino.h>
#include "display/Display.h"
#include "eyes/Eyes.h"
#include "audio/Audio.h"
#include "menu/Menu.h"
#include "sensors/Touch.h"
#include <lvgl.h> 

// ============================================================================
// StarBoy — main.cpp
// ============================================================================

// === BOOT button (GPIO0) — emotion cycling ===
static constexpr int     PIN_BOOT          = 0;
static constexpr uint32_t DEBOUNCE_MS      = 50;
static constexpr uint32_t LONG_PRESS_MS    = 1000;
static constexpr uint32_t SHORT_PRESS_MAX  = 500;

// === PWR button (GPIO17) — menu ===
static constexpr int PWR_PIN = 17;

// === Shared flag: LVGL active (Eyes pause while menu open) ===
volatile bool g_menuActive = false;

// ── BOOT button state ─────────────────────────────────────────────────────────
static int      s_bootRaw        = HIGH;
static int      s_bootStable     = HIGH;
static uint32_t s_bootDebounce   = 0;
static uint32_t s_bootPressStart = 0;

// ── PWR button state ──────────────────────────────────────────────────────────
static int      s_pwrRaw        = HIGH;
static int      s_pwrStable     = HIGH;
static uint32_t s_pwrDebounce   = 0;
static uint32_t s_pwrPressStart = 0;
static bool     s_pwrLongFired  = false;

// ── Emotion cycling ───────────────────────────────────────────────────────────
static constexpr Emotion kEmotions[] = {
    Emotion::IDLE, Emotion::HAPPY, Emotion::ANGRY,
    Emotion::SAD,  Emotion::SLEEPY, Emotion::SURPRISED
};
static constexpr int kNumEmotions = sizeof(kEmotions) / sizeof(kEmotions[0]);
static int s_emotionIdx = 0;

// ============================================================================
// Eyes task — Core 0
// ============================================================================
static void eyesTask(void*) {
    for (;;) {
        if (!g_menuActive) Eyes::instance().update();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ============================================================================
// Button helpers
// ============================================================================
static void handleBoot(uint32_t now) {
    if (g_menuActive) return; // BOOT ignored while menu open

    int reading = digitalRead(PIN_BOOT);
    if (reading != s_bootRaw) { s_bootRaw = reading; s_bootDebounce = now; }

    if ((now - s_bootDebounce) > DEBOUNCE_MS && reading != s_bootStable) {
        s_bootStable = reading;
        if (s_bootStable == LOW) {
            s_bootPressStart = now;
        } else {
            uint32_t dur = now - s_bootPressStart;
            if (dur >= LONG_PRESS_MS) {
                Serial.println("[BOOT] long → reboot");
                Display::instance().fillScreen(0x001F);
                delay(200);
                ESP.restart();
            } else if (dur <= SHORT_PRESS_MAX) {
                s_emotionIdx = (s_emotionIdx + 1) % kNumEmotions;
                Eyes::instance().setEmotion(kEmotions[s_emotionIdx]);
                const char* names[] = {"IDLE","HAPPY","ANGRY","SAD","SLEEPY","SURPRISED"};
                Serial.printf("[Emotion] → %s\n", names[s_emotionIdx]);
            }
        }
    }
}

static void handlePwr(uint32_t now) {
    int reading = digitalRead(PWR_PIN);
    if (reading != s_pwrRaw) { s_pwrRaw = reading; s_pwrDebounce = now; }

    if ((now - s_pwrDebounce) <= DEBOUNCE_MS || reading == s_pwrStable) return;
    s_pwrStable = reading;

    if (s_pwrStable == LOW) {
        s_pwrPressStart = now;
        s_pwrLongFired  = false;
        return;
    }

    // Button released
    uint32_t dur = now - s_pwrPressStart;

    if (dur >= LONG_PRESS_MS && !s_pwrLongFired) {
        if (g_menuActive) {
            Menu::instance().onLongPress();
            g_menuActive = Menu::instance().isOpen();
            if (!g_menuActive) Eyes::instance().setRendering(true);
        }
    } else if (dur <= SHORT_PRESS_MAX) {
        if (!g_menuActive) {
            Eyes::instance().setRendering(false);
            g_menuActive = true;
            Menu::instance().open();
        } else {
            Menu::instance().onShortPress();
            if (!Menu::instance().isOpen()) {
                g_menuActive = false;
                Eyes::instance().setRendering(true);
            }
        }
    }
}

static void pollPwrLong(uint32_t now) {
    if (s_pwrStable == LOW && !s_pwrLongFired) {
        if (now - s_pwrPressStart >= LONG_PRESS_MS) {
            s_pwrLongFired = true;
            if (g_menuActive) {
                Menu::instance().onLongPress();
                g_menuActive = Menu::instance().isOpen();
                if (!g_menuActive) Eyes::instance().setRendering(true);
            }
        }
    }
}

// ============================================================================
// setup()
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(2000);

    pinMode(PIN_BOOT, INPUT_PULLUP);
    pinMode(PWR_PIN,  INPUT_PULLUP);

    Serial.println("\n===================");
    Serial.println("StarBoy boot v0.0.2");
    Serial.println("===================");
    Serial.printf("CPU:   %lu MHz\n", getCpuFrequencyMhz());
    Serial.printf("PSRAM: %lu KB\n",  ESP.getPsramSize() / 1024);
    Serial.printf("Heap:  %lu KB\n",  ESP.getFreeHeap() / 1024);

    if (!Display::instance().begin()) {
        Serial.println("FATAL: display init failed");
        while (true) delay(1000);
    }

    Display::instance().lvglInit();

    // Color test
    Display::instance().fillScreen(0xF800); delay(200);
    Display::instance().fillScreen(0x07E0); delay(200);
    Display::instance().fillScreen(0x001F); delay(200);
    Display::instance().fillScreen(0x0000);

    Eyes::instance().begin();
    Eyes::instance().setEmotion(Emotion::IDLE);

    Audio::instance().begin();
    Touch::instance().begin();
    Menu::instance().begin();

    xTaskCreatePinnedToCore(eyesTask, "eyes", 8192, nullptr, 5, nullptr, 0);

    Serial.println("Boot OK");
    Serial.println("BOOT=cycle emotion | PWR=menu");
}

// ============================================================================
// loop()
// ============================================================================
void loop() {
    uint32_t now = millis();

    handleBoot(now);
    handlePwr(now);
    pollPwrLong(now);

    if (!g_menuActive) {
        Touch::instance().update();
    }

    if (g_menuActive) {
        lv_tick_inc(5);
        lv_task_handler();
    }

    // Telemetry every second
    static uint32_t s_lastLog = 0;
    if (now - s_lastLog > 1000) {
        Serial.printf("[loop] heap=%lu KB menu=%d emo=%d\n",
                      ESP.getFreeHeap() / 1024, (int)g_menuActive, s_emotionIdx);
        s_lastLog = now;
    }

    delay(5);
}
