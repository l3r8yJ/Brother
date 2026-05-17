#pragma once
#include <Arduino.h>

enum class Emotion { IDLE, HAPPY, ANGRY, SAD, SLEEPY, SURPRISED };

struct EyeState {
    float lidTop;       // 1.0=fully open, 0.0=fully closed from top
    float lidBot;       // 0.0=no lower clip, 1.0=full lower squint (happy)
    float pupilScale;   // 1.0=normal, 1.6=surprised
    float browInnerDY;  // brow inner edge offset (negative=up)
    float browOuterDY;  // brow outer edge offset (negative=up)
};

class Eyes {
public:
    static Eyes& instance();

    void setRendering(bool enabled) { _rendering = enabled; }
    void begin();
    void setEmotion(Emotion e);
    void lookAt(float normX, float normY);
    void triggerBlink();
    void update();

private:
    Eyes() = default;

    void clearEye(int16_t cx, int16_t cy);
    void renderEye(int16_t cx, int16_t cy, float gazeDx, float gazeDy);
    void animateEmotion();
    void animateBlink();
    static EyeState emotionTarget(Emotion e);
    static float lerp(float a, float b, float t) { return a + (b - a) * t; }

    bool _rendering = true;

    Emotion  _emotion = Emotion::IDLE;
    EyeState _state   = {1.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    EyeState _target  = {1.0f, 0.0f, 1.0f, 0.0f, 0.0f};

    float _lookX = 0.0f;
    float _lookY = 0.0f;

    bool     _blinking     = false;
    uint32_t _blinkStartMs = 0;
    uint32_t _nextBlinkMs  = 3000;

    static constexpr uint32_t BLINK_DOWN_MS = 300;
    static constexpr uint32_t BLINK_HOLD_MS = 50;
    static constexpr uint32_t BLINK_UP_MS   = 150;

    // Layout — 466x466 round AMOLED
    static constexpr int16_t EYE_LEFT_X  = 133;
    static constexpr int16_t EYE_RIGHT_X = 333;
    static constexpr int16_t EYE_Y       = 233;
    static constexpr int16_t IRIS_R      = 90;
    static constexpr int16_t PUPIL_R     = 36;
    static constexpr int16_t MAX_GAZE    = 25;
    static constexpr int16_t BROW_BASE_Y = 22;  // pixels above iris top
    static constexpr int16_t BROW_INNER  = 35;
    static constexpr int16_t BROW_OUTER  = 40;

    // Colors RGB565 — "Obsidian Mirror" amber
    static constexpr uint16_t C_IRIS  = 0xFD20;
    static constexpr uint16_t C_BG    = 0x0000;
    static constexpr uint16_t C_PUPIL = 0x0000;
    static constexpr uint16_t C_GLINT = 0xFFFF;
};
