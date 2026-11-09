#pragma once

#include <lvgl.h>
#include "util.h"

/**
 * Status screen style 3 ("HUD"): header bar, segmented battery gauges,
 * framed layer name with auto-fitting font and a WPM bar graph.
 */
void draw_hud(lv_obj_t *canvas, const struct status_state *state);
