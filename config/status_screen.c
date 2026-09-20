/*
 * Corne custom status screen - stormtrooper helmet + battery.
 *
 * Built for ZMK with LVGL 8.x on a 1-bit OLED.
 *
 * Polarity convention on a monochrome OLED:
 *   lv_color_black() -> pixel OFF -> dark
 *   lv_color_white() -> pixel ON  -> blue (or whatever your panel emits)
 * So: background black, artwork white. If it renders inverted, the problem
 * is the `inversion-on` property in devicetree, NOT this file.
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
 * Which half am I?
 *
 * On a ZMK split, the left half is built with CONFIG_ZMK_SPLIT_ROLE_CENTRAL=y
 * and the right half without it. This is a compile-time constant, so each
 * .uf2 gets its own label with zero runtime cost.
 * ------------------------------------------------------------------ */
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#define SIDE_LABEL "L"
#define IS_LEFT_HALF 1
#else
#define SIDE_LABEL "R"
#define IS_LEFT_HALF 0
#endif
#else
#define SIDE_LABEL "-"
#define IS_LEFT_HALF 1
#endif

static lv_obj_t *battery_label;

/* ------------------------------------------------------------------ *
 * Helpers
 * ------------------------------------------------------------------ */

/*
 * lv_obj_remove_style_all() is the important call here. A bare
 * lv_obj_create() inherits the LVGL default theme, which carries a
 * background fill, a border and scrollbars. On a 1-bit panel every one of
 * those becomes lit pixels, which is why hand-rolled widgets tend to come
 * out as blocky smears. Stripping the theme first gives a genuinely blank
 * object that only shows what you explicitly draw.
 */
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
 * Stormtrooper helmet, 26 x 30, drawn as lit shapes on black
 * ------------------------------------------------------------------ */
static void draw_trooper(lv_obj_t *parent) {
    /* Helmet shell: white outline, black interior */
    lv_obj_t *helmet = new_shape(parent, 26, 30, false);
    lv_obj_set_style_bg_color(helmet, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(helmet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(helmet, 2, 0);
    lv_obj_set_style_border_color(helmet, lv_color_white(), 0);
    lv_obj_set_style_border_opa(helmet, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(helmet, 9, 0);
    lv_obj_align(helmet, LV_ALIGN_LEFT_MID, 3, 0);

    /* Brow ridge across the top of the visor */
    lv_obj_t *brow = new_shape(helmet, 18, 2, true);
    lv_obj_align(brow, LV_ALIGN_TOP_MID, 0, 3);

    /* Eye lenses */
    lv_obj_t *eye_l = new_shape(helmet, 6, 6, true);
    lv_obj_set_style_radius(eye_l, 2, 0);
    lv_obj_align(eye_l, LV_ALIGN_TOP_LEFT, 2, 6);

    lv_obj_t *eye_r = new_shape(helmet, 6, 6, true);
    lv_obj_set_style_radius(eye_r, 2, 0);
    lv_obj_align(eye_r, LV_ALIGN_TOP_RIGHT, -2, 6);

    /* Centre ridge between the lenses */
    lv_obj_t *ridge = new_shape(helmet, 2, 9, true);
    lv_obj_align(ridge, LV_ALIGN_TOP_MID, 0, 6);

    /* Mouth vent */
    lv_obj_t *vent = new_shape(helmet, 12, 2, true);
    lv_obj_align(vent, LV_ALIGN_BOTTOM_MID, 0, -5);

    /* Cheek vents */
    lv_obj_t *cheek_l = new_shape(helmet, 2, 5, true);
    lv_obj_align(cheek_l, LV_ALIGN_BOTTOM_LEFT, 3, -4);

    lv_obj_t *cheek_r = new_shape(helmet, 2, 5, true);
    lv_obj_align(cheek_r, LV_ALIGN_BOTTOM_RIGHT, -3, -4);
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

    /* Strip the theme, then force an opaque black background so every
     * pixel we do not draw on is genuinely off. */
    lv_obj_remove_style_all(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);

    draw_trooper(screen);

    /* Divider between artwork and status */
    lv_obj_t *divider = new_shape(screen, 1, 22, true);
    lv_obj_align(divider, LV_ALIGN_LEFT_MID, 36, 0);

    /* L / R marker. This is also your diagnostic: if the right OLED ever
     * lights up at all, it will say R. If it shows L, you flashed the
     * left-half firmware onto the right controller. */
    lv_obj_t *side = lv_label_create(screen);
    lv_obj_set_style_text_color(side, lv_color_white(), 0);
    lv_label_set_text(side, SIDE_LABEL);
    lv_obj_align(side, LV_ALIGN_LEFT_MID, 44, 0);

    /* Battery percentage, right aligned */
    battery_label = lv_label_create(screen);
    lv_obj_set_style_text_color(battery_label, lv_color_white(), 0);
    lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, -4, 0);
    set_battery_text(zmk_battery_state_of_charge());

    return screen;
}
