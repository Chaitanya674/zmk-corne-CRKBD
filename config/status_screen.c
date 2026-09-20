/*
 * Corne custom status screen — stormtrooper helmet + battery.
 *
 * Handles mismatched panels: the two halves of this board have different
 * OLEDs (128x32 and 128x64), so the layout is measured from the actual
 * display resolution at runtime rather than hard-coded.
 *
 * Polarity convention on a monochrome OLED:
 *   lv_color_black() -> pixel OFF -> dark
 *   lv_color_white() -> pixel ON  -> blue
 * If this renders inverted, fix `inversion-on` in the .overlay, not here.
 *
 * LVGL 8.x API (current ZMK). On LVGL 9, rename lv_obj_clear_flag to
 * lv_obj_remove_flag; nothing else changes.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <stdio.h>

#include <lvgl.h>
#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

/* ------------------------------------------------------------------ *
 * DIAGNOSTIC SWITCH
 *
 * Set this to 1 to draw a 1px lit border around the exact edge of the
 * panel plus a corner tick. Flash it, and the frame tells you at a glance
 * whether your width/height/offset in the .overlay match the physical
 * glass:
 *
 *   clean rectangle touching all four edges  -> geometry is correct
 *   rectangle floating in a lit field        -> polarity still inverted
 *   rectangle cut off or wrapped at an edge  -> wrong width/segment-offset
 *   rectangle only fills part of the height  -> wrong height/multiplex-ratio
 *   nothing at all                           -> panel is not being driven
 *
 * Set back to 0 once the geometry is confirmed.
 * ------------------------------------------------------------------ */
#define STATUS_SCREEN_DEBUG_FRAME 1

/* ------------------------------------------------------------------ *
 * Which half am I?
 *
 * ZMK builds the left half with CONFIG_ZMK_SPLIT_ROLE_CENTRAL=y and the
 * right half without it, so this is a compile-time constant and each .uf2
 * carries its own label.
 * ------------------------------------------------------------------ */
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#define SIDE_LABEL "L"
#else
#define SIDE_LABEL "R"
#endif
#else
#define SIDE_LABEL "-"
#endif

static lv_obj_t *battery_label;

/* ------------------------------------------------------------------ *
 * Helpers
 *
 * lv_obj_remove_style_all() matters more than it looks. A bare
 * lv_obj_create() inherits the LVGL default theme — background fill,
 * border, scrollbars — and on a 1-bit panel every one of those turns into
 * lit pixels you never asked for.
 * ------------------------------------------------------------------ */
static lv_obj_t *new_shape(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, bool filled) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(o, w, h);

    if (filled) {
        lv_obj_set_style_bg_color(o, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    }
    return o;
}

/* ------------------------------------------------------------------ *
 * Stormtrooper helmet. `s` is a scale factor: 1 for a 32px-tall panel,
 * 2 for a 64px-tall one.
 * ------------------------------------------------------------------ */
static void draw_trooper(lv_obj_t *parent, int s) {
    const lv_coord_t w = 26 * s;
    const lv_coord_t h = 30 * s;

    lv_obj_t *helmet = new_shape(parent, w, h, false);
    lv_obj_set_style_bg_color(helmet, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(helmet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(helmet, 2, 0);
    lv_obj_set_style_border_color(helmet, lv_color_white(), 0);
    lv_obj_set_style_border_opa(helmet, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(helmet, 9 * s, 0);
    lv_obj_align(helmet, LV_ALIGN_LEFT_MID, 3, 0);

    /* Brow ridge */
    lv_obj_t *brow = new_shape(helmet, 18 * s, 2 * s, true);
    lv_obj_align(brow, LV_ALIGN_TOP_MID, 0, 3 * s);

    /* Eye lenses */
    lv_obj_t *eye_l = new_shape(helmet, 6 * s, 6 * s, true);
    lv_obj_set_style_radius(eye_l, 2 * s, 0);
    lv_obj_align(eye_l, LV_ALIGN_TOP_LEFT, 2 * s, 6 * s);

    lv_obj_t *eye_r = new_shape(helmet, 6 * s, 6 * s, true);
    lv_obj_set_style_radius(eye_r, 2 * s, 0);
    lv_obj_align(eye_r, LV_ALIGN_TOP_RIGHT, -2 * s, 6 * s);

    /* Centre ridge between the lenses */
    lv_obj_t *ridge = new_shape(helmet, 2 * s, 9 * s, true);
    lv_obj_align(ridge, LV_ALIGN_TOP_MID, 0, 6 * s);

    /* Mouth vent */
    lv_obj_t *vent = new_shape(helmet, 12 * s, 2 * s, true);
    lv_obj_align(vent, LV_ALIGN_BOTTOM_MID, 0, -5 * s);

    /* Cheek vents */
    lv_obj_t *cheek_l = new_shape(helmet, 2 * s, 5 * s, true);
    lv_obj_align(cheek_l, LV_ALIGN_BOTTOM_LEFT, 3 * s, -4 * s);

    lv_obj_t *cheek_r = new_shape(helmet, 2 * s, 5 * s, true);
    lv_obj_align(cheek_r, LV_ALIGN_BOTTOM_RIGHT, -3 * s, -4 * s);
}

/* ------------------------------------------------------------------ *
 * Battery
 * ------------------------------------------------------------------ */
static void set_battery_text(uint8_t level) {
    if (battery_label == NULL) {
        return;
    }
    char text[8];
    snprintf(text, sizeof(text), "%d%%", level);
    lv_label_set_text(battery_label, text);
}

static int battery_listener_cb(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    set_battery_text(zmk_battery_state_of_charge());
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(status_screen_battery, battery_listener_cb);
ZMK_SUBSCRIPTION(status_screen_battery, zmk_battery_state_changed);

/* ------------------------------------------------------------------ *
 * Screen
 * ------------------------------------------------------------------ */
lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    lv_obj_remove_style_all(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);

    /* Measure the panel instead of assuming it. This is what lets one
     * source file serve both the 128x32 and the 128x64 half. */
    const lv_coord_t ver = lv_disp_get_ver_res(NULL);
    const lv_coord_t hor = lv_disp_get_hor_res(NULL);
    const int scale = (ver >= 64) ? 2 : 1;

#if STATUS_SCREEN_DEBUG_FRAME
    lv_obj_t *frame = new_shape(screen, hor, ver, false);
    lv_obj_set_style_border_width(frame, 1, 0);
    lv_obj_set_style_border_color(frame, lv_color_white(), 0);
    lv_obj_set_style_border_opa(frame, LV_OPA_COVER, 0);
    lv_obj_align(frame, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Solid tick in the top-left corner only, so you can spot rotation
     * and mirroring as well as size. */
    lv_obj_t *tick = new_shape(screen, 8, 4, true);
    lv_obj_align(tick, LV_ALIGN_TOP_LEFT, 2, 2);

    lv_obj_t *dbg = lv_label_create(screen);
    lv_obj_set_style_text_color(dbg, lv_color_white(), 0);
    static char dbg_text[24];
    snprintf(dbg_text, sizeof(dbg_text), SIDE_LABEL " %dx%d", (int)hor, (int)ver);
    lv_label_set_text(dbg, dbg_text);
    lv_obj_align(dbg, LV_ALIGN_CENTER, 0, 0);

    return screen;
#else
    draw_trooper(screen, scale);

    const lv_coord_t split_x = 3 + 26 * scale + 7;

    /* Divider between artwork and status */
    lv_obj_t *divider = new_shape(screen, 1, ver - 8, true);
    lv_obj_align(divider, LV_ALIGN_LEFT_MID, split_x, 0);

    /* L / R marker. Doubles as a diagnostic: if the right panel ever lights
     * up showing "L", you flashed the left-half firmware onto it. */
    lv_obj_t *side = lv_label_create(screen);
    lv_obj_set_style_text_color(side, lv_color_white(), 0);
    lv_label_set_text(side, SIDE_LABEL);
    lv_obj_align(side, LV_ALIGN_LEFT_MID, split_x + 8, (scale == 2) ? -12 : 0);

    /* Battery percentage */
    battery_label = lv_label_create(screen);
    lv_obj_set_style_text_color(battery_label, lv_color_white(), 0);
    lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, -4, (scale == 2) ? 12 : 0);
    set_battery_text(zmk_battery_state_of_charge());

    return screen;
#endif
}
