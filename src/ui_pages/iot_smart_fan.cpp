#include "gui_super_knob.h"

#include <Arduino.h>
#include <display.h>
#include <motor.h>

#include "fan_controller.h"

LV_FONT_DECLARE(lv_font_fan_20);
LV_FONT_DECLARE(lv_font_fan_digits_30);

namespace {

constexpr uint32_t BG = 0x071521, SURFACE = 0x102B3A, FOCUS = 0x18465B;
constexpr uint32_t CYAN = 0x35D4F4, GREEN = 0x32D583, RED = 0xEF6262;
constexpr uint32_t AMBER = 0xFFB703, MUTED = 0x86A7B6;

uint8_t displayedSpeed = 1;
int16_t pendingDirectionSteps = 0;
bool speedAdjusted = false, powerPending = false, powerTarget = false;
lv_obj_t *powerButton = nullptr, *powerLabel = nullptr;
lv_obj_t *speedMeter = nullptr, *directionArc = nullptr;
lv_meter_indicator_t *speedNeedle = nullptr, *speedArc = nullptr;

void updateSpeedDisplay();

const lv_font_t *uiFont()
{
    // Project-owned font generated from SimHei with every fan UI glyph.
    return &lv_font_fan_20;
}

void styleScreen(lv_obj_t *screen)
{
    lv_obj_set_size(screen, 240, 240);
    lv_obj_center(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
}

lv_obj_t *createMenuButton(lv_obj_t *parent, const char *text)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, 166, 39);
    lv_obj_set_style_radius(button, 13, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(SURFACE), 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0x285064), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(FOCUS), LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(button, lv_color_hex(CYAN), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(button, 4, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(button, lv_color_hex(0x176D82), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(button, 3, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(button, 2, LV_STATE_FOCUSED);
    lv_obj_t *label = lv_label_create(button);
    lv_obj_set_style_text_font(label, uiFont(), 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

void updatePowerAppearance(bool power)
{
    if (!powerButton || !powerLabel) return;
    lv_label_set_text(powerLabel, power ? "开关  ON" : "开关  OFF");
    lv_obj_set_style_bg_color(powerButton, lv_color_hex(power ? 0x145A45 : 0x3C2730), 0);
    lv_obj_set_style_border_color(powerButton, lv_color_hex(power ? GREEN : RED), 0);
    lv_obj_set_style_text_color(powerLabel, lv_color_hex(power ? GREEN : 0xFF9C9C), 0);
}

void setConnectionLabel(lv_obj_t *label, const FanControllerSnapshot &state)
{
    const char *text = "DISABLED";
    uint32_t color = MUTED;
    if (state.status == FanConnectionStatus::Connecting) { text = "CONNECTING"; color = AMBER; }
    else if (state.status == FanConnectionStatus::Online) { text = "ONLINE"; color = GREEN; }
    else if (state.status == FanConnectionStatus::Error) { text = "OFFLINE"; color = RED; }
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
}

void loadFanMenu()
{
    fan_controller_cancel_turns();
    set_super_knob_page_status(SUPER_PAGE_BUSY);
    setup_scr_screen_smart_fan(&super_knob_ui);
    lv_scr_load_anim(super_knob_ui.screen_iot_smart_fan, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, true);
    update_motor_config(KNOB_CONFIG_FAN_DETAIL);
    update_page_status(CHECKOUT_PAGE);
}

void powerButtonEvent(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    const FanControllerSnapshot state = fan_controller_snapshot();
    powerTarget = !(powerPending ? powerTarget : (state.stateValid && state.power));
    powerPending = true;
    updatePowerAppearance(powerTarget);
    fan_controller_request_power(powerTarget);
}

void speedButtonEvent(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    set_super_knob_page_status(SUPER_PAGE_BUSY);
    setup_scr_screen_fan_speed(&super_knob_ui);
    lv_scr_load_anim(super_knob_ui.screen_iot_fan_speed, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, true);
    // Config 1 has infinitely many detents (num_positions == 0), so it feels
    // like a gear but never reaches a physical end stop.  Notify the motor task
    // as well: assigning motor_config alone does not refresh PID D/center state.
    update_motor_config(KNOB_CONFIG_FAN_DETAIL);
    update_page_status(CHECKOUT_PAGE);
}

void directionButtonEvent(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    set_super_knob_page_status(SUPER_PAGE_BUSY);
    setup_scr_screen_fan_direction(&super_knob_ui);
    lv_scr_load_anim(super_knob_ui.screen_iot_fan_direction, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, true);
    update_motor_config(1);
    update_page_status(CHECKOUT_PAGE);
}

void backButtonEvent(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    set_super_knob_page_status(SUPER_PAGE_BUSY);
    setup_scr_screen_iot_main(&super_knob_ui);
    lv_scr_load_anim(super_knob_ui.screen_iot_main_boday, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, true);
    update_motor_config(1);
    update_page_status(CHECKOUT_PAGE);
}

void menuStatusTimer(lv_timer_t *timer)
{
    if (get_super_knob_page_status() != IOT_SMART_FAN_PAGE) { lv_timer_del(timer); return; }
    const FanControllerSnapshot state = fan_controller_snapshot();
    if (powerPending && state.status == FanConnectionStatus::Online && state.stateValid && state.power == powerTarget)
        powerPending = false;
    else if (powerPending && state.status == FanConnectionStatus::Error)
        powerPending = false;
    if (!powerPending && state.stateValid) updatePowerAppearance(state.power);
}

void speedStatusTimer(lv_timer_t *timer)
{
    if (get_super_knob_page_status() != IOT_FAN_SPEED_PAGE) { lv_timer_del(timer); return; }
    const FanControllerSnapshot state = fan_controller_snapshot();
    setConnectionLabel(super_knob_ui.screen_iot_fan_status_label, state);
    if (state.stateValid && !speedAdjusted && displayedSpeed != state.speed) {
        displayedSpeed = state.speed;
        updateSpeedDisplay();
    }
}

void updateSpeedDisplay()
{
    if (super_knob_ui.screen_iot_fan_value_label) {
        char value[8];
        snprintf(value, sizeof(value), "%u", displayedSpeed);
        lv_label_set_text(super_knob_ui.screen_iot_fan_value_label, value);
    }
    if (speedMeter && speedNeedle) lv_meter_set_indicator_value(speedMeter, speedNeedle, displayedSpeed);
    if (speedMeter && speedArc) lv_meter_set_indicator_end_value(speedMeter, speedArc, displayedSpeed);
}

void updateDirectionDisplay()
{
    if (directionArc) lv_arc_set_value(directionArc, pendingDirectionSteps);
    if (!super_knob_ui.screen_iot_fan_value_label) return;
    char value[24];
    if (pendingDirectionSteps == 0) snprintf(value, sizeof(value), "CENTER");
    else snprintf(value, sizeof(value), "%c  %d", pendingDirectionSteps < 0 ? 'L' : 'R', abs(pendingDirectionSteps));
    lv_label_set_text(super_knob_ui.screen_iot_fan_value_label, value);
}

} // namespace

void setup_scr_screen_smart_fan(lv_ui *ui)
{
    const FanControllerSnapshot state = fan_controller_snapshot();
    powerPending = false;
    powerTarget = state.stateValid && state.power;
    ui->screen_iot_smart_fan = lv_obj_create(nullptr);
    styleScreen(ui->screen_iot_smart_fan);
    lv_obj_set_flex_flow(ui->screen_iot_smart_fan, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui->screen_iot_smart_fan, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ui->screen_iot_smart_fan, 7, 0);

    powerButton = createMenuButton(ui->screen_iot_smart_fan, "开关  OFF");
    powerLabel = lv_obj_get_child(powerButton, 0);
    updatePowerAppearance(powerTarget);
    lv_obj_add_event_cb(powerButton, powerButtonEvent, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *speed = createMenuButton(ui->screen_iot_smart_fan, "风速");
    lv_obj_add_event_cb(speed, speedButtonEvent, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *direction = createMenuButton(ui->screen_iot_smart_fan, "左右转动");
    lv_obj_add_event_cb(direction, directionButtonEvent, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *back = createMenuButton(ui->screen_iot_smart_fan, "返回");
    lv_obj_add_event_cb(back, backButtonEvent, LV_EVENT_CLICKED, nullptr);
    lv_timer_create(menuStatusTimer, 350, nullptr);
    set_super_knob_page_status(IOT_SMART_FAN_PAGE);
}

void setup_scr_screen_fan_speed(lv_ui *ui)
{
    const FanControllerSnapshot state = fan_controller_snapshot();
    speedAdjusted = false;
    if (state.stateValid) displayedSpeed = state.speed;
    ui->screen_iot_fan_speed = lv_obj_create(nullptr);
    styleScreen(ui->screen_iot_fan_speed);

    speedMeter = lv_meter_create(ui->screen_iot_fan_speed);
    lv_obj_set_size(speedMeter, 208, 208);
    lv_obj_align(speedMeter, LV_ALIGN_CENTER, 0, 9);
    lv_obj_set_style_bg_color(speedMeter, lv_color_hex(0x0B202D), 0);
    lv_obj_set_style_border_width(speedMeter, 2, 0);
    lv_obj_set_style_border_color(speedMeter, lv_color_hex(0x17475B), 0);
    lv_obj_set_style_shadow_color(speedMeter, lv_color_hex(CYAN), 0);
    lv_obj_set_style_shadow_width(speedMeter, 18, 0);
    lv_obj_set_style_shadow_opa(speedMeter, LV_OPA_20, 0);
    lv_meter_scale_t *scale = lv_meter_add_scale(speedMeter);
    lv_meter_set_scale_range(speedMeter, scale, 1, 100, 270, 135);
    lv_meter_set_scale_ticks(speedMeter, scale, 21, 2, 8, lv_color_hex(0x4E7180));
    lv_meter_set_scale_major_ticks(speedMeter, scale, 5, 4, 14, lv_color_hex(CYAN), 10);
    speedArc = lv_meter_add_arc(speedMeter, scale, 7, lv_color_hex(CYAN), -5);
    lv_meter_set_indicator_start_value(speedMeter, speedArc, 1);
    speedNeedle = lv_meter_add_needle_line(speedMeter, scale, 4, lv_color_hex(AMBER), -24);

    ui->screen_iot_fan_status_label = lv_label_create(ui->screen_iot_fan_speed);
    lv_obj_set_style_text_font(ui->screen_iot_fan_status_label, LV_FONT_DEFAULT, 0);
    setConnectionLabel(ui->screen_iot_fan_status_label, state);
    lv_obj_align(ui->screen_iot_fan_status_label, LV_ALIGN_TOP_MID, 0, 37);
    ui->screen_iot_fan_value_label = lv_label_create(ui->screen_iot_fan_speed);
    lv_obj_set_style_text_font(ui->screen_iot_fan_value_label, &lv_font_fan_digits_30, 0);
    lv_obj_set_style_text_color(ui->screen_iot_fan_value_label, lv_color_hex(CYAN), 0);
    lv_obj_align(ui->screen_iot_fan_value_label, LV_ALIGN_CENTER, 0, 19);
    // Keep Chinese separate from the number-only 30 px font used below.
    lv_obj_t *speedCaption = lv_label_create(ui->screen_iot_fan_speed);
    lv_obj_set_style_text_font(speedCaption, uiFont(), 0);
    lv_obj_set_style_text_color(speedCaption, lv_color_white(), 0);
    lv_label_set_text(speedCaption, "风速");
    lv_obj_align(speedCaption, LV_ALIGN_CENTER, 0, -15);
    updateSpeedDisplay();
    lv_timer_create(speedStatusTimer, 400, nullptr);
    set_super_knob_page_status(IOT_FAN_SPEED_PAGE);
}

void setup_scr_screen_fan_direction(lv_ui *ui)
{
    pendingDirectionSteps = 0;
    ui->screen_iot_fan_direction = lv_obj_create(nullptr);
    styleScreen(ui->screen_iot_fan_direction);
    directionArc = lv_arc_create(ui->screen_iot_fan_direction);
    lv_obj_set_size(directionArc, 194, 194);
    lv_obj_align(directionArc, LV_ALIGN_CENTER, 0, 8);
    lv_arc_set_range(directionArc, -120, 120);
    lv_arc_set_bg_angles(directionArc, 45, 315);
    lv_obj_remove_style(directionArc, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(directionArc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(directionArc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(directionArc, lv_color_hex(0x244858), LV_PART_MAIN);
    lv_obj_set_style_arc_width(directionArc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(directionArc, lv_color_hex(AMBER), LV_PART_INDICATOR);
    lv_obj_t *title = lv_label_create(ui->screen_iot_fan_direction);
    lv_obj_set_style_text_font(title, uiFont(), 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text(title, "左右转动");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 13);
    lv_obj_t *left = lv_label_create(ui->screen_iot_fan_direction);
    lv_obj_set_style_text_font(left, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(left, lv_color_hex(MUTED), 0);
    lv_label_set_text(left, "<  L");
    lv_obj_align(left, LV_ALIGN_LEFT_MID, 27, 41);
    lv_obj_t *right = lv_label_create(ui->screen_iot_fan_direction);
    lv_obj_set_style_text_font(right, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(right, lv_color_hex(MUTED), 0);
    lv_label_set_text(right, "R  >");
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, -27, 41);
    ui->screen_iot_fan_value_label = lv_label_create(ui->screen_iot_fan_direction);
    lv_obj_set_style_text_font(ui->screen_iot_fan_value_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(ui->screen_iot_fan_value_label, lv_color_hex(AMBER), 0);
    lv_obj_align(ui->screen_iot_fan_value_label, LV_ALIGN_CENTER, 0, 8);
    lv_obj_t *hint = lv_label_create(ui->screen_iot_fan_direction);
    lv_obj_set_style_text_font(hint, uiFont(), 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(MUTED), 0);
    lv_label_set_text(hint, "轻触返回");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -23);
    updateDirectionDisplay();
    set_super_knob_page_status(IOT_FAN_DIRECTION_PAGE);
}

void smart_fan_handle_encoder_delta(int16_t delta)
{
    if (delta == 0) return;
    // Haptic pitch remains stable; only the logical value changes twice per
    // physical detent. This avoids increasing sensor-noise gain in motor PID.
    const int16_t logicalDelta = constrain(static_cast<int32_t>(delta) * 2, -16, 16);
    const SUPER_KNOB_PAGE_NUM page = get_super_knob_page_status();
    if (page == IOT_FAN_SPEED_PAGE) {
        speedAdjusted = true;
        displayedSpeed = constrain(static_cast<int>(displayedSpeed) + logicalDelta, 1, 100);
        updateSpeedDisplay();
        fan_controller_request_speed(displayedSpeed);
    } else if (page == IOT_FAN_DIRECTION_PAGE) {
        pendingDirectionSteps = constrain(pendingDirectionSteps + logicalDelta, -120, 120);
        updateDirectionDisplay();
        fan_controller_request_turn_steps(logicalDelta);
    }
}

void smart_fan_return_to_menu(void) { loadFanMenu(); }
