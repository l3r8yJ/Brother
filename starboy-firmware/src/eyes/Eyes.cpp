#include "Eyes.h"
#include <lvgl.h>

Eyes& Eyes::instance() {
    static Eyes inst;
    return inst;
}

void Eyes::begin() {
    _state  = emotionTarget(Emotion::IDLE);
    _target = _state;
    _nextBlinkMs = millis() + 2000 + random(3000);

    _canvas_buf = (lv_color_t*)ps_malloc(466 * 466 * sizeof(lv_color_t));
    if (!_canvas_buf) {
        Serial.println("[Eyes] FATAL: ps_malloc failed");
        return;
    }

    _eyes_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_eyes_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_eyes_screen, LV_OPA_COVER, 0);

    _canvas = lv_canvas_create(_eyes_screen);
    lv_canvas_set_buffer(_canvas, _canvas_buf, 466, 466, LV_COLOR_FORMAT_NATIVE);
    lv_obj_set_pos(_canvas, 0, 0);

    lv_scr_load(_eyes_screen);

    _timer = lv_timer_create(timerCb, 16, nullptr);

    Serial.println("[Eyes] OK (lv_canvas 434KB PSRAM)");
}

void Eyes::showEyesScreen() {
    if (_eyes_screen) lv_scr_load(_eyes_screen);
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

// ── Timer callback ────────────────────────────────────────────────────────────

void Eyes::timerCb(lv_timer_t* t) {
    Eyes::instance().drawFrame();
}

void Eyes::drawFrame() {
    if (!_rendering || !_canvas) return;

    animateEmotion();
    animateBlink();

    if (!_blinking && millis() >= _nextBlinkMs) triggerBlink();

    lv_canvas_fill_bg(_canvas, lv_color_black(), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(_canvas, &layer);

    float gazeDx = _lookX * MAX_GAZE;
    float gazeDy = _lookY * MAX_GAZE;

    drawEye(&layer, EYE_LEFT_X,  EYE_Y, gazeDx, gazeDy);
    drawEye(&layer, EYE_RIGHT_X, EYE_Y, gazeDx, gazeDy);

    lv_canvas_finish_layer(_canvas, &layer);
}

// ── Draw one eye ──────────────────────────────────────────────────────────────

void Eyes::drawEye(lv_layer_t* layer, int16_t cx, int16_t cy, float gazeDx, float gazeDy) {
    lv_draw_rect_dsc_t rDsc;
    lv_draw_rect_dsc_init(&rDsc);

    // 1. IRIS — full circle; eyelid rects clip it
    rDsc.bg_color = lv_color_hex(C_IRIS);
    rDsc.bg_opa   = LV_OPA_COVER;
    rDsc.radius   = LV_RADIUS_CIRCLE;
    lv_area_t iris_area = {
        cx - IRIS_R,
        cy - IRIS_R,
        cx + IRIS_R - 1,
        cy + IRIS_R - 1
    };
    lv_draw_rect(layer, &rDsc, &iris_area);

    // 2. UPPER EYELID — black rect clips from top
    int16_t upper_h = (int16_t)((1.0f - _state.lidTop) * (IRIS_R * 2 + 4));
    if (upper_h > 0) {
        rDsc.bg_color = lv_color_black();
        rDsc.radius   = 0;
        lv_area_t lid_area = {
            cx - IRIS_R - 2,
            cy - IRIS_R - 2,
            cx + IRIS_R + 1,
            (int16_t)(cy - IRIS_R - 2 + upper_h)
        };
        lv_draw_rect(layer, &rDsc, &lid_area);
    }

    // 3. LOWER EYELID — black rect clips from bottom (happy squint)
    int16_t lower_h = (int16_t)(_state.lidBot * IRIS_R);
    if (lower_h > 0) {
        rDsc.bg_color = lv_color_black();
        rDsc.radius   = 0;
        lv_area_t bot_area = {
            cx - IRIS_R - 2,
            (int16_t)(cy + IRIS_R + 2 - lower_h),
            cx + IRIS_R + 1,
            cy + IRIS_R + 2
        };
        lv_draw_rect(layer, &rDsc, &bot_area);
    }

    // 4. PUPIL
    int16_t pdx = (int16_t)gazeDx;
    int16_t pdy = (int16_t)gazeDy;
    int16_t pr  = (int16_t)(PUPIL_R * _state.pupilScale);
    pr = min<int16_t>(pr, IRIS_R - 4);

    float dist    = sqrtf((float)(pdx * pdx + pdy * pdy));
    float maxDist = IRIS_R - pr - 4;
    if (dist > maxDist && dist > 0.0f) {
        pdx = (int16_t)(pdx * maxDist / dist);
        pdy = (int16_t)(pdy * maxDist / dist);
    }
    int16_t pcx = cx + pdx;
    int16_t pcy = cy + pdy;

    rDsc.bg_color = lv_color_black();
    rDsc.radius   = LV_RADIUS_CIRCLE;
    lv_area_t pupil_area = { (int16_t)(pcx - pr), (int16_t)(pcy - pr), (int16_t)(pcx + pr - 1), (int16_t)(pcy + pr - 1) };
    lv_draw_rect(layer, &rDsc, &pupil_area);

    // 5. GLINTS (only when eye mostly open)
    if (_state.lidTop > 0.3f) {
        rDsc.bg_color = lv_color_white();
        rDsc.radius   = LV_RADIUS_CIRCLE;

        int16_t gx = pcx - 20, gy = pcy - 20;
        lv_area_t g1 = { (int16_t)(gx - 7), (int16_t)(gy - 7), (int16_t)(gx + 6), (int16_t)(gy + 6) };
        lv_draw_rect(layer, &rDsc, &g1);

        gx = pcx + 12; gy = pcy - 28;
        lv_area_t g2 = { (int16_t)(gx - 3), (int16_t)(gy - 3), (int16_t)(gx + 2), (int16_t)(gy + 2) };
        lv_draw_rect(layer, &rDsc, &g2);

        gx = pcx - 28; gy = pcy + 8;
        lv_area_t g3 = { (int16_t)(gx - 3), (int16_t)(gy - 3), (int16_t)(gx + 2), (int16_t)(gy + 2) };
        lv_draw_rect(layer, &rDsc, &g3);
    }

    // 6. EYEBROW — line above iris
    lv_draw_line_dsc_t lDsc;
    lv_draw_line_dsc_init(&lDsc);
    lDsc.color       = lv_color_hex(C_BROW);
    lDsc.width       = BROW_WIDTH;
    lDsc.round_start = 1;
    lDsc.round_end   = 1;

    int16_t brow_base = cy - IRIS_R - BROW_BASE_Y;
    bool isLeft = (cx == EYE_LEFT_X);
    lDsc.p1.x = (lv_value_precise_t)(cx + (isLeft ? +BROW_INNER : -BROW_INNER));
    lDsc.p1.y = (lv_value_precise_t)(brow_base + (int16_t)_state.browInnerDY);
    lDsc.p2.x = (lv_value_precise_t)(cx + (isLeft ? -BROW_OUTER : +BROW_OUTER));
    lDsc.p2.y = (lv_value_precise_t)(brow_base + (int16_t)_state.browOuterDY);
    lv_draw_line(layer, &lDsc);
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
