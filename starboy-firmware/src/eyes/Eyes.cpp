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
    Serial.println("[Eyes] OK (GFX)");
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
    if (!_rendering) return;

    auto* gfx = Display::instance().gfx();
    if (!gfx) return;

    animateEmotion();
    animateBlink();

    if (!_blinking && millis() >= _nextBlinkMs) triggerBlink();

    float gazeDx = _lookX * MAX_GAZE;
    float gazeDy = _lookY * MAX_GAZE;

    clearEye(EYE_LEFT_X,  EYE_Y);
    clearEye(EYE_RIGHT_X, EYE_Y);
    renderEye(EYE_LEFT_X,  EYE_Y, gazeDx, gazeDy);
    renderEye(EYE_RIGHT_X, EYE_Y, gazeDx, gazeDy);

    Display::instance().flush();
}

// ── Private ───────────────────────────────────────────────────────────────────

void Eyes::clearEye(int16_t cx, int16_t cy) {
    auto* gfx = Display::instance().gfx();
    // Clear iris area + brow area above
    int16_t y_top = cy - IRIS_R - BROW_BASE_Y - 10;
    int16_t x     = max<int16_t>(0, (int16_t)(cx - IRIS_R - 4));
    y_top         = max<int16_t>(0, y_top);
    int16_t w     = min<int16_t>((IRIS_R + 4) * 2, (int16_t)(DISPLAY_WIDTH  - x));
    int16_t h     = min<int16_t>((int16_t)(IRIS_R * 2 + BROW_BASE_Y + 18), (int16_t)(DISPLAY_HEIGHT - y_top));
    gfx->fillRect(x, y_top, w, h, C_BG);
}

void Eyes::renderEye(int16_t cx, int16_t cy, float gazeDx, float gazeDy) {
    auto* gfx = Display::instance().gfx();

    // 1. IRIS — full circle
    gfx->fillCircle(cx, cy, IRIS_R, C_IRIS);

    // 2. UPPER EYELID — black rect clips from top
    int16_t upper_h = (int16_t)((1.0f - _state.lidTop) * (IRIS_R * 2 + 4));
    if (upper_h > 0) {
        gfx->fillRect(cx - IRIS_R - 2, cy - IRIS_R - 2, (IRIS_R + 2) * 2, upper_h, C_BG);
    }

    // 3. LOWER EYELID — black rect clips from bottom (happy squint)
    int16_t lower_h = (int16_t)(_state.lidBot * IRIS_R);
    if (lower_h > 0) {
        int16_t bot_y = (int16_t)(cy + IRIS_R + 2 - lower_h);
        gfx->fillRect(cx - IRIS_R - 2, bot_y, (IRIS_R + 2) * 2, lower_h, C_BG);
    }

    // 4. PUPIL
    int16_t pdx = (int16_t)gazeDx;
    int16_t pdy = (int16_t)gazeDy;
    int16_t pr  = (int16_t)(PUPIL_R * _state.pupilScale);
    pr = min<int16_t>(pr, (int16_t)(IRIS_R - 4));

    float dist    = sqrtf((float)(pdx * pdx + pdy * pdy));
    float maxDist = IRIS_R - pr - 4;
    if (dist > maxDist && dist > 0.0f) {
        pdx = (int16_t)(pdx * maxDist / dist);
        pdy = (int16_t)(pdy * maxDist / dist);
    }
    int16_t pcx = cx + pdx;
    int16_t pcy = cy + pdy;
    gfx->fillCircle(pcx, pcy, pr, C_PUPIL);

    // 5. GLINTS (when eye mostly open)
    if (_state.lidTop > 0.3f) {
        gfx->fillCircle(pcx - 20, pcy - 20, 7, C_GLINT);
        gfx->fillCircle(pcx + 12, pcy - 28, 3, C_GLINT);
        gfx->fillCircle(pcx - 28, pcy + 8,  3, C_GLINT);
    }

    // 6. EYEBROW — 5 parallel lines for thickness
    int16_t brow_base = cy - IRIS_R - BROW_BASE_Y;
    bool    isLeft    = (cx == EYE_LEFT_X);
    int16_t inner_x   = cx + (isLeft ? +BROW_INNER : -BROW_INNER);
    int16_t inner_y   = brow_base + (int16_t)_state.browInnerDY;
    int16_t outer_x   = cx + (isLeft ? -BROW_OUTER : +BROW_OUTER);
    int16_t outer_y   = brow_base + (int16_t)_state.browOuterDY;
    for (int t = -2; t <= 2; t++) {
        gfx->drawLine(inner_x, inner_y + t, outer_x, outer_y + t, C_IRIS);
    }
}

// ── Animation ─────────────────────────────────────────────────────────────────

void Eyes::animateEmotion() {
    constexpr float k = 0.12f;
    _state.lidTop      = lerp(_state.lidTop,      _target.lidTop,      k);
    _state.lidBot      = lerp(_state.lidBot,       _target.lidBot,      k);
    _state.pupilScale  = lerp(_state.pupilScale,   _target.pupilScale,  k);
    _state.browInnerDY = lerp(_state.browInnerDY,  _target.browInnerDY, k);
    _state.browOuterDY = lerp(_state.browOuterDY,  _target.browOuterDY, k);
}

void Eyes::animateBlink() {
    if (!_blinking) return;
    uint32_t t = millis() - _blinkStartMs;

    if (t < BLINK_DOWN_MS) {
        _state.lidTop = _target.lidTop * (1.0f - (float)t / BLINK_DOWN_MS);
    } else if (t < BLINK_DOWN_MS + BLINK_HOLD_MS) {
        _state.lidTop = 0.0f;
    } else {
        uint32_t upT = t - BLINK_DOWN_MS - BLINK_HOLD_MS;
        _state.lidTop = _target.lidTop * ((float)upT / BLINK_UP_MS);
        if (_state.lidTop >= _target.lidTop) {
            _state.lidTop = _target.lidTop;
            _blinking     = false;
            _nextBlinkMs  = millis() + 2000 + random(3000);
        }
    }
}

EyeState Eyes::emotionTarget(Emotion e) {
    // { lidTop, lidBot, pupilScale, browInnerDY, browOuterDY }
    switch (e) {
        case Emotion::HAPPY:     return { 0.50f, 0.40f, 0.80f,  -8.0f, -12.0f };
        case Emotion::ANGRY:     return { 0.70f, 0.00f, 0.70f,  12.0f,  -8.0f };
        case Emotion::SAD:       return { 0.75f, 0.00f, 0.75f, -10.0f,   8.0f };
        case Emotion::SLEEPY:    return { 0.20f, 0.00f, 0.60f,   4.0f,   8.0f };
        case Emotion::SURPRISED: return { 1.00f, 0.00f, 1.60f, -20.0f, -20.0f };
        default:                 return { 1.00f, 0.00f, 1.00f,   0.0f,   0.0f };
    }
}
