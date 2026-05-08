#include "Eyes.h"
#include "display/Display.h"

Eyes& Eyes::instance() {
    static Eyes inst;
    return inst;
}

void Eyes::begin() {
    _state  = emotionTarget(Emotion::IDLE);
    _target = _state;
    _nextBlinkMs = millis() + 2000 + random(3000);
    Serial.println("[Eyes] OK");
}

void Eyes::setEmotion(Emotion e) {
    _emotion = e;
    _target  = emotionTarget(e);
}

void Eyes::lookAt(float normX, float normY) {
    _lookX = constrain(normX, -1.0f, 1.0f);
    _lookY = constrain(normY, -1.0f, 1.0f);
}

void Eyes::triggerBlink() {
    if (_blinking) return;
    _blinking     = true;
    _blinkStartMs = millis();
}

void Eyes::update() {
    animateEmotion();
    animateBlink();

    auto* gfx = Display::instance().gfx();
    if (!gfx) return;

    float pupilDx = _lookX * MAX_GAZE;
    float pupilDy = _lookY * MAX_GAZE;

    clearEye(EYE_LEFT_X,  EYE_Y);
    clearEye(EYE_RIGHT_X, EYE_Y);
    renderEye(EYE_LEFT_X,  EYE_Y, pupilDx, pupilDy);
    renderEye(EYE_RIGHT_X, EYE_Y, pupilDx, pupilDy);

    // Auto-blink scheduling
    if (!_blinking && millis() >= _nextBlinkMs) {
        triggerBlink();
    }
}

// ── Private ──────────────────────────────────────────────────────────────────

void Eyes::clearEye(int16_t cx, int16_t cy) {
    auto* gfx = Display::instance().gfx();
    int16_t margin = 4;
    int16_t size   = (IRIS_R + margin) * 2;
    gfx->fillRect(cx - IRIS_R - margin, cy - IRIS_R - margin, size, size, C_BG);
}

void Eyes::renderEye(int16_t cx, int16_t cy, float pupilDx, float pupilDy) {
    auto* gfx = Display::instance().gfx();

    // Iris — amber ellipse (apply vertical squint scale)
    int16_t ry = (int16_t)(IRIS_R * _state.irisScaleY);
    gfx->fillEllipse(cx, cy, IRIS_R, ry, C_IRIS);

    // Pupil — black circle offset by gaze
    int16_t pr  = (int16_t)(PUPIL_R * _state.pupilScale);
    int16_t pdx = (int16_t)pupilDx;
    int16_t pdy = (int16_t)pupilDy;
    // Clamp pupil inside iris
    float dist = sqrtf(pdx * pdx + pdy * pdy);
    float maxDist = IRIS_R - pr - 4;
    if (dist > maxDist && dist > 0) {
        pdx = (int16_t)(pdx * maxDist / dist);
        pdy = (int16_t)(pdy * maxDist / dist);
    }
    gfx->fillCircle(cx + pdx, cy + pdy, pr, C_PUPIL);

    // Eyelid — large black circle offset above iris
    // When lidOpen=1.0: center far above → no coverage
    // When lidOpen=0.0: center at cy → fully covers iris
    int16_t lidCY = (int16_t)(cy - IRIS_R * 2 * _state.lidOpen);
    gfx->fillCircle(cx, lidCY, IRIS_R + 2, C_BG);

    // Glint — white highlight dot (only when eye is mostly open)
    if (_state.lidOpen > 0.5f) {
        gfx->fillCircle(cx + GLINT_DX, cy + GLINT_DY, GLINT_R, C_GLINT);
    }
}

void Eyes::animateEmotion() {
    constexpr float k = 0.12f;
    _state.lidOpen    = lerp(_state.lidOpen,    _target.lidOpen,    k);
    _state.irisScaleY = lerp(_state.irisScaleY, _target.irisScaleY, k);
    _state.pupilScale = lerp(_state.pupilScale, _target.pupilScale, k);
}

void Eyes::animateBlink() {
    if (!_blinking) return;

    uint32_t t = millis() - _blinkStartMs;

    if (t < BLINK_DOWN_MS) {
        _state.lidOpen = 1.0f - (float)t / BLINK_DOWN_MS;
    } else if (t < BLINK_DOWN_MS + BLINK_HOLD_MS) {
        _state.lidOpen = 0.0f;
    } else {
        uint32_t upT = t - BLINK_DOWN_MS - BLINK_HOLD_MS;
        _state.lidOpen = (float)upT / BLINK_UP_MS;
        if (_state.lidOpen >= 1.0f) {
            _state.lidOpen = 1.0f;
            _blinking      = false;
            _nextBlinkMs   = millis() + 2000 + random(3000);
        }
    }
}

EyeState Eyes::emotionTarget(Emotion e) {
    switch (e) {
        case Emotion::HAPPY:     return {0.75f, 0.80f, 0.90f};
        case Emotion::ANGRY:     return {0.85f, 0.90f, 0.80f};
        case Emotion::SAD:       return {0.90f, 0.85f, 0.85f};
        case Emotion::SLEEPY:    return {0.45f, 0.70f, 0.70f};
        case Emotion::SURPRISED: return {1.00f, 1.15f, 1.30f};
        default:                 return {1.00f, 1.00f, 1.00f}; // IDLE
    }
}
