#pragma once
#include <Arduino.h>

enum class Emotion { IDLE, HAPPY, ANGRY, SAD, SLEEPY, SURPRISED };

struct EyeState {
    float lidOpen;    // 0.0=closed, 1.0=fully open
    float irisScaleY; // 1.0=circle, <1=squint vertically
    float pupilScale; // 1.0=normal, >1=dilated
};

class Eyes {
public:
    static Eyes& instance();

    void begin();
    void setEmotion(Emotion e);
    void lookAt(float normX, float normY); // -1..1 normalized
    void triggerBlink();
    void update(); // call at ~30fps from FreeRTOS task

private:
    Eyes() = default;

    void renderEye(int16_t cx, int16_t cy, float pupilDx, float pupilDy);
    void clearEye(int16_t cx, int16_t cy);
    void animateBlink();
    void animateEmotion();
    static EyeState emotionTarget(Emotion e);
    static float lerp(float a, float b, float t) { return a + (b - a) * t; }

    Emotion   _emotion = Emotion::IDLE;
    EyeState  _state   = {1.0f, 1.0f, 1.0f};
    EyeState  _target  = {1.0f, 1.0f, 1.0f};

    float _lookX = 0.0f;
    float _lookY = 0.0f;

    bool     _blinking     = false;
    uint32_t _blinkStartMs = 0;
    uint32_t _nextBlinkMs  = 3000;

    static constexpr uint32_t BLINK_DOWN_MS  = 120;
    static constexpr uint32_t BLINK_HOLD_MS  = 30;
    static constexpr uint32_t BLINK_UP_MS    = 150;

    // Layout constants — tuned for 466×466 round AMOLED
    static constexpr int16_t  EYE_LEFT_X  = 158;
    static constexpr int16_t  EYE_RIGHT_X = 308;
    static constexpr int16_t  EYE_Y       = 233;
    static constexpr int16_t  IRIS_R      = 65;
    static constexpr int16_t  PUPIL_R     = 26;
    static constexpr int16_t  MAX_GAZE    = 18; // max pupil offset px
    static constexpr int16_t  GLINT_DX    = -20;
    static constexpr int16_t  GLINT_DY    = -20;
    static constexpr int16_t  GLINT_R     = 5;

    // Colors (RGB565) — "Obsidian Mirror" amber eyes
    static constexpr uint16_t C_IRIS  = 0xFD20; // amber ~255,165,0
    static constexpr uint16_t C_PUPIL = 0x0000; // black
    static constexpr uint16_t C_GLINT = 0xFFFF; // white
    static constexpr uint16_t C_BG    = 0x0000; // black
};
