#include "gui_super_knob.h"

#include <Arduino.h>
#include <display.h>
#include <motor.h>

#include "fan_controller.h"

LV_FONT_DECLARE(lv_font_chinese_source_20);
LV_FONT_DECLARE(lv_font_super_knob_30);
LV_FONT_DECLARE(lv_font_simsun_16_cjk);

namespace {

uint8_t displayedSpeed = 1;
int16_t pendingDirectionSteps = 0;
bool speedAdjusted = false;

void updateSpeedLabel();

void styleScreen(lv_obj_t *screen)
{
    lv_obj_set_size(screen, 240, 240);
    lv_obj_center(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
}

lv_obj_t *createMenuButton(lv_obj_t *parent, const char *text, const lv_font_t *font)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, 170, 52);
    lv_obj_set_style_radius(button, 18, 0);
    lv_obj_t *label = lv_label_create(button);
    lv_obj_set_style_text_font(label, font, 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

void loadFanMenu()
{
    fan_controller_cancel_turns();
    set_super_knob_page_status(SUPER_PAGE_BUSY);
    setup_scr_screen_smart_fan(&super_knob_ui);
    lv_scr_load_anim(super_knob_ui.screen_iot_smart_fan,
                     LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, true);
    update_motor_config(1);
    update_page_status(CHECKOUT_PAGE);
}

void speedButtonEvent(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    set_super_knob_page_status(SUPER_PAGE_BUSY);
    setup_scr_screen_fan_speed(&super_knob_ui);
    lv_scr_load_anim(super_knob_ui.screen_iot_fan_speed,
                     LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, true);
    update_motor_config(2);
}

void directionButtonEvent(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    set_super_knob_page_status(SUPER_PAGE_BUSY);
    setup_scr_screen_fan_direction(&super_knob_ui);
    lv_scr_load_anim(super_knob_ui.screen_iot_fan_direction,
                     LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, true);
    update_motor_config(2);
}

void backButtonEvent(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    set_super_knob_page_status(SUPER_PAGE_BUSY);
    setup_scr_screen_iot_main(&super_knob_ui);
    lv_scr_load_anim(super_knob_ui.screen_iot_main_boday,
                     LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, true);
    update_motor_config(1);
    update_page_status(CHECKOUT_PAGE);
}

void setConnectionLabel(lv_obj_t *label, const FanControllerSnapshot &state)
{
    const char *text = "DISABLED";
    switch (state.status) {
        case FanConnectionStatus::Connecting: text = "CONNECTING"; break;
        case FanConnectionStatus::Online: text = "ONLINE"; break;
        case FanConnectionStatus::Error: text = "ERROR"; break;
        case FanConnectionStatus::Disabled: break;
    }
    lv_label_set_text(label, text);
}

void statusTimer(lv_timer_t *timer)
{
    const SUPER_KNOB_PAGE_NUM page = get_super_knob_page_status();
    if (page != IOT_FAN_SPEED_PAGE) {
        lv_timer_del(timer);
        return;
    }
    FanControllerSnapshot state = fan_controller_snapshot();
    setConnectionLabel(super_knob_ui.screen_iot_fan_status_label, state);
    if (state.stateValid && !speedAdjusted && displayedSpeed != state.speed) {
        displayedSpeed = state.speed;
        updateSpeedLabel();
    }
}

void updateSpeedLabel()
{
    if (super_knob_ui.screen_iot_fan_value_label == nullptr) return;
    char value[8];
    snprintf(value, sizeof(value), "%u", displayedSpeed);
    lv_label_set_text(super_knob_ui.screen_iot_fan_value_label, value);
}

void updateDirectionLabel()
{
    if (super_knob_ui.screen_iot_fan_value_label == nullptr) return;
    char value[32];
    if (pendingDirectionSteps == 0) {
        snprintf(value, sizeof(value), "--");
    } else {
        snprintf(value, sizeof(value), "%s %d", pendingDirectionSteps < 0 ? "LEFT" : "RIGHT",
                 abs(pendingDirectionSteps));
    }
    lv_label_set_text(super_knob_ui.screen_iot_fan_value_label, value);
}

} // namespace

void setup_scr_screen_smart_fan(lv_ui *ui)
{
    ui->screen_iot_smart_fan = lv_obj_create(nullptr);
    styleScreen(ui->screen_iot_smart_fan);
    lv_obj_set_flex_flow(ui->screen_iot_smart_fan, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui->screen_iot_smart_fan, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ui->screen_iot_smart_fan, 8, 0);

    lv_obj_t *speed = createMenuButton(ui->screen_iot_smart_fan, "风速", &lv_font_chinese_source_20);
    lv_obj_add_event_cb(speed, speedButtonEvent, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *direction = createMenuButton(ui->screen_iot_smart_fan, "左右", &lv_font_simsun_16_cjk);
    lv_obj_add_event_cb(direction, directionButtonEvent, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *back = createMenuButton(ui->screen_iot_smart_fan, "返回", &lv_font_simsun_16_cjk);
    lv_obj_add_event_cb(back, backButtonEvent, LV_EVENT_CLICKED, nullptr);

    set_super_knob_page_status(IOT_SMART_FAN_PAGE);
}

void setup_scr_screen_fan_speed(lv_ui *ui)
{
    FanControllerSnapshot state = fan_controller_snapshot();
    speedAdjusted = false;
    if (state.stateValid) displayedSpeed = state.speed;

    ui->screen_iot_fan_speed = lv_obj_create(nullptr);
    styleScreen(ui->screen_iot_fan_speed);

    lv_obj_t *title = lv_label_create(ui->screen_iot_fan_speed);
    lv_obj_set_style_text_font(title, &lv_font_chinese_source_20, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text(title, "风速");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 34);

    ui->screen_iot_fan_value_label = lv_label_create(ui->screen_iot_fan_speed);
    lv_obj_set_style_text_font(ui->screen_iot_fan_value_label, &lv_font_super_knob_30, 0);
    lv_obj_set_style_text_color(ui->screen_iot_fan_value_label, lv_color_hex(0x48CAE4), 0);
    lv_obj_center(ui->screen_iot_fan_value_label);
    updateSpeedLabel();

    ui->screen_iot_fan_status_label = lv_label_create(ui->screen_iot_fan_speed);
    lv_obj_set_style_text_font(ui->screen_iot_fan_status_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(ui->screen_iot_fan_status_label, lv_color_hex(0xA8DADC), 0);
    setConnectionLabel(ui->screen_iot_fan_status_label, state);
    lv_obj_align(ui->screen_iot_fan_status_label, LV_ALIGN_BOTTOM_MID, 0, -32);

    lv_timer_create(statusTimer, 400, nullptr);

    set_super_knob_page_status(IOT_FAN_SPEED_PAGE);
}

void setup_scr_screen_fan_direction(lv_ui *ui)
{
    pendingDirectionSteps = 0;
    ui->screen_iot_fan_direction = lv_obj_create(nullptr);
    styleScreen(ui->screen_iot_fan_direction);

    lv_obj_t *title = lv_label_create(ui->screen_iot_fan_direction);
    lv_obj_set_style_text_font(title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text(title, "左右");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 34);

    ui->screen_iot_fan_value_label = lv_label_create(ui->screen_iot_fan_direction);
    lv_obj_set_style_text_font(ui->screen_iot_fan_value_label, &lv_font_super_knob_30, 0);
    lv_obj_set_style_text_color(ui->screen_iot_fan_value_label, lv_color_hex(0xFFB703), 0);
    lv_obj_center(ui->screen_iot_fan_value_label);
    updateDirectionLabel();

    lv_obj_t *hint = lv_label_create(ui->screen_iot_fan_direction);
    lv_obj_set_style_text_font(hint, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xA8DADC), 0);
    lv_label_set_text(hint, "待定");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -32);

    set_super_knob_page_status(IOT_FAN_DIRECTION_PAGE);
}

void smart_fan_handle_encoder_delta(int16_t delta)
{
    if (delta == 0) return;
    const SUPER_KNOB_PAGE_NUM page = get_super_knob_page_status();
    if (page == IOT_FAN_SPEED_PAGE) {
        speedAdjusted = true;
        displayedSpeed = constrain(static_cast<int>(displayedSpeed) + delta, 1, 100);
        updateSpeedLabel();
        fan_controller_request_speed(displayedSpeed);
    } else if (page == IOT_FAN_DIRECTION_PAGE) {
        pendingDirectionSteps = constrain(pendingDirectionSteps + delta, -120, 120);
        updateDirectionLabel();
        // Deliberately no network action before real-device calibration.
    }
}

void smart_fan_return_to_menu(void)
{
    loadFanMenu();
}
