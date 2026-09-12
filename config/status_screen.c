#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <lvgl.h>
#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

static lv_obj_t *battery_label;

static void set_battery_text(uint8_t level) {
    if (battery_label == NULL) {
        return;
    }
    char text[6];
    snprintf(text, sizeof(text), "%d%%", level);
    lv_label_set_text(battery_label, text);
}

static int battery_listener_cb(const zmk_event_t *eh) {
    set_battery_text(zmk_battery_state_of_charge());
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(status_screen_battery, battery_listener_cb);
ZMK_SUBSCRIPTION(status_screen_battery, zmk_battery_state_changed);

static void draw_face(lv_obj_t *parent) {
    lv_obj_t *face = lv_obj_create(parent);
    lv_obj_set_size(face, 28, 28);
    lv_obj_set_style_radius(face, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(face, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(face, 2, 0);
    lv_obj_set_style_border_color(face, lv_color_white(), 0);
    lv_obj_align(face, LV_ALIGN_LEFT_MID, 2, 0);

    lv_obj_t *eye_l = lv_obj_create(face);
    lv_obj_set_size(eye_l, 6, 8);
    lv_obj_set_style_radius(eye_l, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(eye_l, lv_color_white(), 0);
    lv_obj_align(eye_l, LV_ALIGN_LEFT_MID, 4, -2);

    lv_obj_t *eye_r = lv_obj_create(face);
    lv_obj_set_size(eye_r, 6, 8);
    lv_obj_set_style_radius(eye_r, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(eye_r, lv_color_white(), 0);
    lv_obj_align(eye_r, LV_ALIGN_RIGHT_MID, -4, -2);

    lv_obj_t *blush = lv_obj_create(face);
    lv_obj_set_size(blush, 4, 2);
    lv_obj_set_style_radius(blush, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(blush, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(blush, LV_OPA_50, 0);
    lv_obj_align(blush, LV_ALIGN_LEFT_MID, 2, 6);
}

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    draw_face(screen);

    battery_label = lv_label_create(screen);
    lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, -4, 0);
    set_battery_text(zmk_battery_state_of_charge());

    return screen;
}