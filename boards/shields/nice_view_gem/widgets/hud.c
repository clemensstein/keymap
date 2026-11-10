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
 *   118..167 style 3: WPM number + bar graph
 *            style 4: toucan animation
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

#if CONFIG_TOUCAN_STATUS_SCREEN == 3
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
#endif

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

#if CONFIG_TOUCAN_STATUS_SCREEN == 3
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

#endif /* CONFIG_TOUCAN_STATUS_SCREEN == 3 */

#if CONFIG_TOUCAN_STATUS_SCREEN == 4
/*
 * Toucan animation in the bottom strip. Sprites are drawn straight into the
 * canvas buffer and only the strip is invalidated, so a frame refreshes
 * 50 of 168 display rows. The timer pauses while the keyboard is idle.
 */

#define ANIM_Y 118
#define ANIM_H (SCREEN_HEIGHT - ANIM_Y)
#define ANIM_FRAME_MS 250
#define PX 2 /* sprite pixel size */

#define BIRD_W 30
#define BIRD_H 21
#define BIRD_BODY_ROWS 19
#define BIRD_Y (SCREEN_HEIGHT - 2 - BIRD_H * PX) /* feet just above the ground */
#define BIRD_X_MIN 6
#define BIRD_X_MAX (SCREEN_WIDTH - 6 - BIRD_W * PX)
#define HOP_PX 4

static const char *const bird_body[BIRD_BODY_ROWS] = {
    "..........######..............",
    "........##########............",
    ".......############...........",
    "......#####....#####..........",
    "......####..##..###########...",
    ".....#####..##..##.........##.",
    ".....######....##...........##",
    ".....#######..##.............#",
    "....########.##.......########",
    "....#######.....##########....",
    "....######.......#............",
    "...#######........#...........",
    "...#######.........#..........",
    "...#######.........#..........",
    "...########.......#...........",
    "....########.....##...........",
    ".....##########.##............",
    "......#########...............",
    "........#####.................",
};

/* Rows 4 and 5 with the eye closed */
static const char *const bird_eye_closed[2] = {
    "......####......###########...",
    ".....#####.####.##.........##.",
};

/* Leg variants (rows 19 and 20) */
static const char *const legs_stand[2] = {".........#..#", "........##.##"};
static const char *const legs_walk_a[2] = {"........#...#", ".......##...##"};
static const char *const legs_walk_b[2] = {"..........##", "..........##"};
static const char *const legs_tucked[2] = {".........####", ""};

static const char *const heart[6] = {
    ".##.##.", "#######", "#######", ".#####.", "..###..", "...#...",
};

enum bird_action { ACT_STAND, ACT_HOP, ACT_WALK };

static struct {
    lv_obj_t *canvas;
    lv_timer_t *timer;
    int x;
    bool facing_left;
    enum bird_action action;
    int t;
    int heart_t; /* frames since the heart appeared, -1 = none */
    volatile bool sleeping;
    bool drawn_asleep;
} bird = {.x = BIRD_X_MIN, .heart_t = -1};

static void px_block(lv_color_t *buf, int x, int y, int size, lv_color_t color) {
    for (int dy = 0; dy < size; dy++) {
        int yy = y + dy;
        if (yy < ANIM_Y || yy >= SCREEN_HEIGHT) {
            continue;
        }
        for (int dx = 0; dx < size; dx++) {
            int xx = x + dx;
            if (xx >= 0 && xx < SCREEN_WIDTH) {
                buf[yy * SCREEN_WIDTH + xx] = color;
            }
        }
    }
}

static void sprite_row(lv_color_t *buf, int x0, int y, const char *row, int width, int size,
                       bool mirror) {
    for (int i = 0; row[i] != '\0'; i++) {
        if (row[i] == '#') {
            int col = mirror ? width - 1 - i : i;
            px_block(buf, x0 + col * size, y, size, LVGL_FOREGROUND);
        }
    }
}

static void draw_bird(lv_color_t *buf, int x, int y, bool mirror, bool eyes_closed,
                      const char *const legs[2]) {
    for (int r = 0; r < BIRD_BODY_ROWS; r++) {
        const char *row = bird_body[r];
        if (eyes_closed && (r == 4 || r == 5)) {
            row = bird_eye_closed[r - 4];
        }
        sprite_row(buf, x, y + r * PX, row, BIRD_W, PX, mirror);
    }
    for (int r = 0; r < 2; r++) {
        sprite_row(buf, x, y + (BIRD_BODY_ROWS + r) * PX, legs[r], BIRD_W, PX, mirror);
    }
}

static void render_bird(lv_obj_t *canvas, bool invalidate) {
    lv_img_dsc_t *img = lv_canvas_get_img(canvas);
    lv_color_t *buf = (lv_color_t *)img->data;

    for (int y = ANIM_Y; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            buf[y * SCREEN_WIDTH + x] = LVGL_BACKGROUND;
        }
    }

    /* Dotted ground */
    for (int x = 2; x < SCREEN_WIDTH - 2; x += 3) {
        buf[(SCREEN_HEIGHT - 1) * SCREEN_WIDTH + x] = LVGL_FOREGROUND;
    }

    int y = BIRD_Y;
    bool eyes_closed = bird.sleeping;
    const char *const *legs = legs_stand;

    if (!bird.sleeping) {
        switch (bird.action) {
        case ACT_STAND:
            eyes_closed = (bird.t % 12) == 5;
            break;
        case ACT_HOP:
            if (bird.t % 2 == 0) {
                y -= HOP_PX;
                legs = legs_tucked;
            }
            break;
        case ACT_WALK:
            legs = (bird.t % 2 == 0) ? legs_walk_a : legs_walk_b;
            y -= (bird.t % 2 == 0) ? 0 : 1;
            break;
        }
    }

    draw_bird(buf, bird.x, y, bird.facing_left, eyes_closed, legs);

    /* Heart pops up above the beak: small first, then full size */
    if (bird.heart_t >= 0 && !bird.sleeping) {
        int size = bird.heart_t < 2 ? 1 : 2;
        int beak = bird.facing_left ? bird.x + 4 : bird.x + BIRD_W * PX - 18;
        int hx = beak + (2 - size) * 3;
        int hy = ANIM_Y + 1 + (2 - size) * 4;
        for (int r = 0; r < 6; r++) {
            sprite_row(buf, hx, hy + r * size, heart[r], 7, size, false);
        }
    }

    if (invalidate) {
        lv_area_t area;
        lv_obj_get_coords(canvas, &area);
        area.y1 += ANIM_Y;
        lv_obj_invalidate_area(canvas, &area);
    }
}

static void draw_sleep_marks(lv_obj_t *canvas) {
    /* "z Z" in the free space beside the bird, rising away from it */
    bool right = bird.x + BIRD_W * PX + 24 <= SCREEN_WIDTH;
    int x = right ? bird.x + BIRD_W * PX + 2 : bird.x - 24;
    text(canvas, right ? x : x + 14, BIRD_Y + 8, 10, &quinquefive_8, LV_TEXT_ALIGN_LEFT,
         LVGL_FOREGROUND, "z");
    text(canvas, right ? x + 9 : x, ANIM_Y + 2, 14, &quinquefive_12, LV_TEXT_ALIGN_LEFT,
         LVGL_FOREGROUND, "Z");
}

void hud_anim_step(void) {
    if (bird.canvas == NULL) {
        return;
    }
    if (bird.sleeping) {
        if (!bird.drawn_asleep) {
            render_bird(bird.canvas, false);
            draw_sleep_marks(bird.canvas);
            bird.drawn_asleep = true;
        }
        return;
    }
    bird.drawn_asleep = false;

    bird.t++;
    if (bird.heart_t >= 0 && ++bird.heart_t > 6) {
        bird.heart_t = -1;
    }

    switch (bird.action) {
    case ACT_STAND:
        if (bird.t >= 14) {
            bird.action = ACT_HOP;
            bird.t = 0;
            bird.heart_t = 0;
        }
        break;
    case ACT_HOP:
        if (bird.t >= 6) {
            bird.action = ACT_WALK;
            bird.t = 0;
        }
        break;
    case ACT_WALK:
        bird.x += bird.facing_left ? -2 : 2;
        if (bird.x <= BIRD_X_MIN || bird.x >= BIRD_X_MAX) {
            bird.x = bird.x <= BIRD_X_MIN ? BIRD_X_MIN : BIRD_X_MAX;
            bird.facing_left = !bird.facing_left;
            bird.action = ACT_STAND;
            bird.t = 0;
        }
        break;
    }

    render_bird(bird.canvas, true);
}

static void anim_timer_cb(lv_timer_t *timer) { hud_anim_step(); }

void hud_anim_init(lv_obj_t *canvas) {
    bird.canvas = canvas;
    if (bird.timer == NULL) {
        bird.timer = lv_timer_create(anim_timer_cb, ANIM_FRAME_MS, NULL);
    }
}

void hud_anim_set_sleeping(bool sleeping) { bird.sleeping = sleeping; }
#endif /* CONFIG_TOUCAN_STATUS_SCREEN == 4 */

void draw_hud(lv_obj_t *canvas, const struct status_state *state) {
    draw_header(canvas, state);
    draw_battery(canvas, 0, "L", state->battery, state->charging);
    /* charging_p only mirrors the left half's USB state, so no "+" on the right */
    draw_battery(canvas, 72, "R", state->battery_p, false);
    draw_divider(canvas);
    draw_layer(canvas, state);
#if CONFIG_TOUCAN_STATUS_SCREEN == 3
    draw_wpm(canvas, state);
#else
    render_bird(canvas, false);
    if (bird.sleeping) {
        draw_sleep_marks(canvas);
    }
#endif
}
