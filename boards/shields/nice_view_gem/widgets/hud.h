#pragma once

#include <lvgl.h>
#include "util.h"

/**
 * Status screen styles 3 and 4 ("HUD"): header bar, segmented battery
 * gauges, framed layer name with auto-fitting font. Style 3 adds a WPM bar
 * graph, style 4 an animated toucan.
 */
void draw_hud(lv_obj_t *canvas, const struct status_state *state);

/* Style 4 only: toucan animation in place of the WPM graph */
void hud_anim_init(lv_obj_t *canvas);
void hud_anim_step(void);
void hud_anim_set_sleeping(bool sleeping);
