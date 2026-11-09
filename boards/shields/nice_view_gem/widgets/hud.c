#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>

#include <zmk/endpoints.h>
#include <zmk/keymap.h>

#include "hud.h"
#include "../assets/custom_fonts.h"

/*
 * Layout (144 x 168):
 *   0..15    header bar (inverted): "TOUCAN" + connection
 *   21..45   battery gauges L / R
 *   50       dotted divider
 *   56..112  layer frame with corner brackets
 *   118..166 WPM number + bar graph
 */

#define HEADER_H 16

#define BAT_Y 21
#define BAT_W 46
#define BAT_H 14

#define DIVIDER_Y 50

#define FRAME_X 4
#define FRAME_Y 56
#define FRAME_W 136
#define FRAME_H 57
#define BRACKET 9

#define WPM_Y 118
#define CHART_X 46
#define CHART_BARS 32
#define CHART_PITCH 3
#define CHART_BASE_Y 160
#define CHART_MAX_H 34
#define WPM_MAX 100

static uint8_t wpm_hist[CHART_BARS];
static uint8_t wpm_head;
static uint8_t wpm_last;

static void fill(lv_obj_t *canvas, int x, int y, int w, int h, lv_color_t color) {
    lv_draw_rect_dsc_t dsc;
    init_rect_dsc(&dsc, color);
    lv_canvas_draw_rect(canvas, x, y, w, h, &dsc);
}

static void text(lv_obj_t *canvas, int x, int y, int w, const lv_font_t *font,
                 lv_text_align_t align, lv_color_t color, const char *str) {
    lv_draw_label_dsc_t dsc;
    init_label_dsc(&dsc, color, font, align);
    lv_canvas_draw_text(canvas, x, y, w, &dsc, str);
}

static lv_coord_t text_width(const lv_font_t *font, const char *str) {
    lv_point_t size;
    lv_txt_get_size(&size, str, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return size.x;
}

static void draw_header(lv_obj_t *canvas, const struct status_state *state) {
    fill(canvas, 0, 0, SCREEN_WIDTH, HEADER_H, LVGL_FOREGROUND);
    text(canvas, 4, 4, 70, &quinquefive_8, LV_TEXT_ALIGN_LEFT, LVGL_BACKGROUND, "TOUCAN");

    char label[16];
    bool usb = state->selected_endpoint.transport == ZMK_TRANSPORT_USB;
    if (usb) {
        strcpy(label, "USB");
    } else {
        snprintf(label, sizeof(label), "BT%d", state->active_profile_index + 1);
    }
    text(canvas, 70, 4, 56, &quinquefive_8, LV_TEXT_ALIGN_RIGHT, LVGL_BACKGROUND, label);

    /* Status dot: filled = connected, ring = paired but not connected,
     * ring with center = open profile waiting for pairing */
    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.radius = LV_RADIUS_CIRCLE;
    if (usb || state->active_profile_connected) {
        dot.bg_color = LVGL_BACKGROUND;
        lv_canvas_draw_rect(canvas, 131, 4, 8, 8, &dot);
    } else {
        dot.bg_opa = LV_OPA_TRANSP;
        dot.border_color = LVGL_BACKGROUND;
        dot.border_width = 1;
        lv_canvas_draw_rect(canvas, 131, 4, 8, 8, &dot);
        if (!state->active_profile_bonded) {
            fill(canvas, 134, 7, 2, 2, LVGL_BACKGROUND);
        }
    }
}

static void draw_battery(lv_obj_t *canvas, int x0, const char *name, uint8_t level,
                         bool charging) {
    int bx = x0 + 14;

    text(canvas, x0 + 3, BAT_Y + 1, 12, &quinquefive_12, LV_TEXT_ALIGN_LEFT, LVGL_FOREGROUND,
         name);

    /* Body with 2 px outline and terminal nub */
    fill(canvas, bx, BAT_Y, BAT_W, BAT_H, LVGL_FOREGROUND);
    fill(canvas, bx + 2, BAT_Y + 2, BAT_W - 4, BAT_H - 4, LVGL_BACKGROUND);
    fill(canvas, bx + BAT_W, BAT_Y + 4, 3, BAT_H - 8, LVGL_FOREGROUND);

    /* 5 segments, 20 % each */
    int segments = level == 0 ? 0 : (level + 19) / 20;
    if (segments > 5) {
        segments = 5;
    }
    for (int i = 0; i < segments; i++) {
        fill(canvas, bx + 4 + i * 8, BAT_Y + 4, 6, BAT_H - 8, LVGL_FOREGROUND);
    }

    char pct[12];
    snprintf(pct, sizeof(pct), charging ? "+%u%%" : "%u%%", level);
    text(canvas, bx, BAT_Y + BAT_H + 3, BAT_W, &quinquefive_8, LV_TEXT_ALIGN_CENTER,
         LVGL_FOREGROUND, pct);
}

static void draw_divider(lv_obj_t *canvas) {
    for (int x = 4; x < SCREEN_WIDTH - 4; x += 2) {
        fill(canvas, x, DIVIDER_Y, 1, 1, LVGL_FOREGROUND);
    }
}

static void draw_brackets(lv_obj_t *canvas) {
    int x1 = FRAME_X, y1 = FRAME_Y;
    int x2 = FRAME_X + FRAME_W - 1, y2 = FRAME_Y + FRAME_H - 1;

    fill(canvas, x1, y1, BRACKET, 2, LVGL_FOREGROUND);
    fill(canvas, x1, y1, 2, BRACKET, LVGL_FOREGROUND);
    fill(canvas, x2 - BRACKET + 1, y1, BRACKET, 2, LVGL_FOREGROUND);
    fill(canvas, x2 - 1, y1, 2, BRACKET, LVGL_FOREGROUND);
    fill(canvas, x1, y2 - 1, BRACKET, 2, LVGL_FOREGROUND);
    fill(canvas, x1, y2 - BRACKET + 1, 2, BRACKET, LVGL_FOREGROUND);
    fill(canvas, x2 - BRACKET + 1, y2 - 1, BRACKET, 2, LVGL_FOREGROUND);
    fill(canvas, x2 - 1, y2 - BRACKET + 1, 2, BRACKET, LVGL_FOREGROUND);
}

static void draw_layer(lv_obj_t *canvas, const struct status_state *state) {
    draw_brackets(canvas);

    text(canvas, FRAME_X + 8, FRAME_Y + 6, 60, &quinquefive_8, LV_TEXT_ALIGN_LEFT,
         LVGL_FOREGROUND, "LAYER");

    /* Inverted index tag */
    char idx[8];
    snprintf(idx, sizeof(idx), "%u", state->layer_index);
    fill(canvas, FRAME_X + FRAME_W - 26, FRAME_Y + 4, 18, 12, LVGL_FOREGROUND);
    text(canvas, FRAME_X + FRAME_W - 26, FRAME_Y + 6, 18, &quinquefive_8, LV_TEXT_ALIGN_CENTER,
         LVGL_BACKGROUND, idx);

    char fallback[16];
    const char *name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(state->layer_index));
    if (name == NULL || name[0] == '\0') {
        snprintf(fallback, sizeof(fallback), "L%u", state->layer_index);
        name = fallback;
    }

    /* Largest font that fits inside the frame */
    const lv_font_t *fonts[] = {&quinquefive_24, &quinquefive_18, &quinquefive_12,
                                &quinquefive_8};
    const lv_font_t *font = fonts[ARRAY_SIZE(fonts) - 1];
    for (size_t i = 0; i < ARRAY_SIZE(fonts); i++) {
        if (text_width(fonts[i], name) <= FRAME_W - 12) {
            font = fonts[i];
            break;
        }
    }

    int area_y = FRAME_Y + 18;
    int area_h = FRAME_H - 18 - 6;
    int y = area_y + (area_h - lv_font_get_line_height(font)) / 2;
    text(canvas, FRAME_X, y, FRAME_W, font, LV_TEXT_ALIGN_CENTER, LVGL_FOREGROUND, name);
}

static void draw_wpm(lv_obj_t *canvas, const struct status_state *state) {
    if (state->wpm != wpm_last) {
        wpm_head = (wpm_head + 1) % CHART_BARS;
        wpm_hist[wpm_head] = state->wpm;
        wpm_last = state->wpm;
    }

    /* Number: largest font that fits the column left of the chart */
    char num[8];
    snprintf(num, sizeof(num), "%u", state->wpm);
    const lv_font_t *fonts[] = {&quinquefive_24, &quinquefive_18, &quinquefive_12,
                                &quinquefive_8};
    const lv_font_t *font = fonts[ARRAY_SIZE(fonts) - 1];
    for (size_t i = 0; i < ARRAY_SIZE(fonts); i++) {
        if (text_width(fonts[i], num) <= CHART_X - 6) {
            font = fonts[i];
            break;
        }
    }
    int num_y = WPM_Y + 30 - lv_font_get_line_height(font);
    text(canvas, 4, num_y, CHART_X - 4, font, LV_TEXT_ALIGN_LEFT, LVGL_FOREGROUND, num);
    text(canvas, 4, WPM_Y + 35, 40, &quinquefive_8, LV_TEXT_ALIGN_LEFT, LVGL_FOREGROUND, "WPM");

    /* Bars, oldest left, newest right */
    for (int i = 0; i < CHART_BARS; i++) {
        uint8_t wpm = wpm_hist[(wpm_head + 1 + i) % CHART_BARS];
        int h = (wpm > WPM_MAX ? WPM_MAX : wpm) * CHART_MAX_H / WPM_MAX;
        if (wpm > 0 && h == 0) {
            h = 1;
        }
        if (h > 0) {
            fill(canvas, CHART_X + i * CHART_PITCH, CHART_BASE_Y - h, 2, h, LVGL_FOREGROUND);
        }
    }

    /* Baseline with ticks */
    fill(canvas, CHART_X, CHART_BASE_Y + 1, CHART_BARS * CHART_PITCH - 1, 1, LVGL_FOREGROUND);
    for (int i = 0; i <= CHART_BARS; i += 8) {
        int x = CHART_X + i * CHART_PITCH - (i == CHART_BARS ? 2 : 0);
        fill(canvas, x, CHART_BASE_Y + 2, 1, 3, LVGL_FOREGROUND);
    }
}

void draw_hud(lv_obj_t *canvas, const struct status_state *state) {
    draw_header(canvas, state);
    draw_battery(canvas, 0, "L", state->battery, state->charging);
    /* charging_p only mirrors the left half's USB state, so no "+" on the right */
    draw_battery(canvas, 72, "R", state->battery_p, false);
    draw_divider(canvas);
    draw_layer(canvas, state);
    draw_wpm(canvas, state);
}
