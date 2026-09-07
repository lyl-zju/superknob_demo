/*
 * One-dimensional Jump game for the SuperKnob.
 * Turn the knob against the virtual spring to charge, then release to jump.
 */
#include "lvgl.h"
#include <Arduino.h>
#include <esp_random.h>
#include <math.h>
#include "gui_super_knob.h"
#include <display.h>
#include <motor.h>

LV_IMG_DECLARE(jump_retry_img);

namespace
{
constexpr int SCREEN_W = 240;
constexpr int PLATFORM_Y = 180;
constexpr int PLATFORM_H = 17;
constexpr int PIECE_SIZE = 15;
constexpr int PIECE_REST_Y = PLATFORM_Y - PIECE_SIZE;
constexpr float MAX_JUMP_DISTANCE = 175.0f;

enum JumpPhase
{
    JUMP_READY,
    JUMP_AIRBORNE,
    JUMP_LANDING,
    JUMP_FALLING,
    JUMP_GAME_OVER,
};

static JumpPhase phase = JUMP_READY;
static int score = 0;
static int centre_streak = 0;
static int current_x = 14;
static int current_w = 58;
static int target_x = 125;
static int target_w = 46;
static int last_award = 0;

static float jump_start_centre = 0.0f;
static float jump_end_centre = 0.0f;
static float jump_arc_height = 60.0f;
static float piece_centre_x = 0.0f;
static float falling_x = 0.0f;
static float falling_y = 0.0f;
static uint32_t phase_started = 0;
static uint32_t jump_duration = 650;

static lv_obj_t *score_label = nullptr;
static lv_obj_t *charge_bar = nullptr;
static lv_obj_t *charge_label = nullptr;
static lv_obj_t *current_platform = nullptr;
static lv_obj_t *target_platform = nullptr;
static lv_obj_t *piece = nullptr;
static lv_obj_t *award_label = nullptr;
static lv_obj_t *game_over_panel = nullptr;
static lv_obj_t *retry_button = nullptr;
static lv_timer_t *jump_timer = nullptr;

static int random_between(int minimum, int maximum)
{
    return minimum + static_cast<int>(esp_random() % static_cast<uint32_t>(maximum - minimum + 1));
}

static void set_piece_centre(float x, float y)
{
    piece_centre_x = x;
    lv_obj_set_pos(piece, static_cast<int>(x - PIECE_SIZE / 2.0f), static_cast<int>(y));
}

static void refresh_score()
{
    lv_label_set_text_fmt(score_label, "SCORE %d", score);
}

static void generate_target()
{
    target_w = random_between(36, 58);
    const int target_centre = random_between(125, 198);
    target_x = target_centre - target_w / 2;
}

static void refresh_platforms()
{
    lv_obj_set_pos(current_platform, current_x, PLATFORM_Y);
    lv_obj_set_size(current_platform, current_w, PLATFORM_H);
    lv_obj_set_pos(target_platform, target_x, PLATFORM_Y);
    lv_obj_set_size(target_platform, target_w, PLATFORM_H);
}

static void prepare_next_platform()
{
    // Move the landed platform to the left. Preserve the piece's exact landing
    // offset on that platform instead of snapping it back to the centre.
    const float old_target_centre = target_x + target_w / 2.0f;
    const float landing_offset = piece_centre_x - old_target_centre;
    current_w = target_w;
    current_x = 45 - current_w / 2;
    const float new_current_centre = current_x + current_w / 2.0f;
    generate_target();
    refresh_platforms();
    set_piece_centre(new_current_centre + landing_offset, PIECE_REST_Y);
    lv_obj_set_style_bg_color(piece, lv_color_hex(0xFFB300), 0);
    lv_obj_add_flag(award_label, LV_OBJ_FLAG_HIDDEN);
    phase = JUMP_READY;
}

static void reset_game()
{
    score = 0;
    centre_streak = 0;
    current_x = 14;
    current_w = 58;
    generate_target();
    refresh_score();
    refresh_platforms();
    set_piece_centre(current_x + current_w / 2.0f, PIECE_REST_Y);
    lv_obj_set_style_bg_color(piece, lv_color_hex(0xFFB300), 0);
    lv_obj_add_flag(award_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(game_over_panel, LV_OBJ_FLAG_HIDDEN);
    lv_group_remove_obj(retry_button);
    phase = JUMP_READY;

    // Re-centre the virtual spring and discard any release left by the last round.
    update_page_status(JUMP_SPRING_START);
}

static void show_game_over()
{
    phase = JUMP_GAME_OVER;
    lv_obj_clear_flag(game_over_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(game_over_panel);
    lv_group_add_obj(super_knob_ui.defult_group, retry_button);
    lv_group_focus_obj(retry_button);
}

static void begin_jump(float charge)
{
    charge = constrain(charge, 0.0f, 1.0f);
    phase = JUMP_AIRBORNE;
    phase_started = lv_tick_get();
    jump_start_centre = piece_centre_x;
    jump_end_centre = jump_start_centre + charge * MAX_JUMP_DISTANCE;
    jump_arc_height = 48.0f + charge * 48.0f;
    jump_duration = static_cast<uint32_t>(500.0f + charge * 280.0f);
}

static void finish_jump()
{
    const float target_centre = target_x + target_w / 2.0f;
    const bool landed = jump_end_centre >= target_x && jump_end_centre <= target_x + target_w;
    if (!landed)
    {
        falling_x = jump_end_centre;
        falling_y = PIECE_REST_Y;
        phase_started = lv_tick_get();
        phase = JUMP_FALLING;
        return;
    }

    const float centre_zone = fmaxf(4.0f, target_w * 0.12f);
    const bool centred = fabsf(jump_end_centre - target_centre) <= centre_zone;
    if (centred)
    {
        centre_streak++;
        last_award = centre_streak * 2;
        lv_obj_set_style_bg_color(piece, lv_color_hex(0x00C853), 0);
    }
    else
    {
        centre_streak = 0;
        last_award = 1;
        lv_obj_set_style_bg_color(piece, lv_color_hex(0x42A5F5), 0);
    }
    score += last_award;
    refresh_score();
    lv_label_set_text_fmt(award_label, "+%d", last_award);
    lv_obj_clear_flag(award_label, LV_OBJ_FLAG_HIDDEN);
    set_piece_centre(jump_end_centre, PIECE_REST_Y);
    phase_started = lv_tick_get();
    phase = JUMP_LANDING;
}

static void jump_tick(lv_timer_t *timer)
{
    (void)timer;

    const int charge_percent = static_cast<int>(get_jump_charge_percent() + 0.5f);
    lv_bar_set_value(charge_bar, charge_percent, LV_ANIM_OFF);
    lv_label_set_text_fmt(charge_label, "%d%%", charge_percent);

    if (phase == JUMP_READY)
    {
        float released_charge = 0.0f;
        if (consume_jump_release(&released_charge))
            begin_jump(released_charge);
        return;
    }

    const uint32_t elapsed = lv_tick_elaps(phase_started);
    if (phase == JUMP_AIRBORNE)
    {
        float t = static_cast<float>(elapsed) / static_cast<float>(jump_duration);
        if (t > 1.0f)
            t = 1.0f;
        const float x = jump_start_centre + (jump_end_centre - jump_start_centre) * t;
        const float y = PIECE_REST_Y - 4.0f * jump_arc_height * t * (1.0f - t);
        set_piece_centre(x, y);
        if (t >= 1.0f)
            finish_jump();
    }
    else if (phase == JUMP_LANDING)
    {
        if (elapsed >= 280)
            prepare_next_platform();
    }
    else if (phase == JUMP_FALLING)
    {
        const float seconds = elapsed / 1000.0f;
        const float y = falling_y + 25.0f * seconds + 330.0f * seconds * seconds;
        set_piece_centre(falling_x, y);
        if (y > 245.0f)
            show_game_over();
    }
}

static void retry_event_handler(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED)
        reset_game();
}

static lv_obj_t *create_platform(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *platform = lv_obj_create(parent);
    lv_obj_set_style_bg_color(platform, color, 0);
    lv_obj_set_style_bg_opa(platform, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(platform, 0, 0);
    lv_obj_set_style_radius(platform, 4, 0);
    lv_obj_clear_flag(platform, LV_OBJ_FLAG_SCROLLABLE);
    return platform;
}
} // namespace

void setup_scr_screen_jump(lv_ui *ui)
{
    ui->screen_jump = lv_obj_create(nullptr);
    lv_obj_set_size(ui->screen_jump, SCREEN_W, SCREEN_W);
    lv_obj_center(ui->screen_jump);
    lv_obj_set_style_bg_color(ui->screen_jump, lv_color_hex(0xEAF7F5), 0);
    lv_obj_set_style_bg_opa(ui->screen_jump, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ui->screen_jump, LV_OBJ_FLAG_SCROLLABLE);

    score_label = lv_label_create(ui->screen_jump);
    lv_obj_set_pos(score_label, 8, 4);
    lv_obj_set_style_text_color(score_label, lv_color_hex(0x263238), 0);

    charge_label = lv_label_create(ui->screen_jump);
    lv_obj_set_width(charge_label, 48);
    lv_obj_set_pos(charge_label, 184, 4);
    lv_obj_set_style_text_align(charge_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(charge_label, lv_color_hex(0x263238), 0);

    charge_bar = lv_bar_create(ui->screen_jump);
    lv_obj_set_pos(charge_bar, 14, 25);
    lv_obj_set_size(charge_bar, 212, 12);
    lv_bar_set_range(charge_bar, 0, 100);
    lv_bar_set_value(charge_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(charge_bar, lv_color_hex(0xCFD8DC), LV_PART_MAIN);
    lv_obj_set_style_bg_color(charge_bar, lv_color_hex(0xFFB300), LV_PART_INDICATOR);
    lv_obj_set_style_radius(charge_bar, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(charge_bar, 6, LV_PART_INDICATOR);

    lv_obj_t *hint = lv_label_create(ui->screen_jump);
    lv_label_set_text(hint, "TURN & RELEASE");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x607D8B), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -11);

    current_platform = create_platform(ui->screen_jump, lv_color_hex(0x26A69A));
    target_platform = create_platform(ui->screen_jump, lv_color_hex(0x26A69A));

    piece = lv_obj_create(ui->screen_jump);
    lv_obj_set_size(piece, PIECE_SIZE, PIECE_SIZE);
    lv_obj_set_style_bg_color(piece, lv_color_hex(0xFFB300), 0);
    lv_obj_set_style_bg_opa(piece, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(piece, 2, 0);
    lv_obj_set_style_border_color(piece, lv_color_white(), 0);
    lv_obj_set_style_radius(piece, 4, 0);
    lv_obj_clear_flag(piece, LV_OBJ_FLAG_SCROLLABLE);

    award_label = lv_label_create(ui->screen_jump);
    lv_obj_set_width(award_label, 80);
    lv_obj_set_pos(award_label, 80, 55);
    lv_obj_set_style_text_align(award_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(award_label, lv_color_hex(0x00A152), 0);
    lv_obj_add_flag(award_label, LV_OBJ_FLAG_HIDDEN);

    game_over_panel = lv_obj_create(ui->screen_jump);
    lv_obj_set_size(game_over_panel, 164, 100);
    lv_obj_align(game_over_panel, LV_ALIGN_CENTER, 0, 4);
    lv_obj_set_style_bg_color(game_over_panel, lv_color_hex(0x263238), 0);
    lv_obj_set_style_bg_opa(game_over_panel, LV_OPA_90, 0);
    lv_obj_set_style_border_width(game_over_panel, 0, 0);
    lv_obj_set_style_radius(game_over_panel, 14, 0);
    lv_obj_clear_flag(game_over_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *over_label = lv_label_create(game_over_panel);
    lv_label_set_text(over_label, "GAME OVER");
    lv_obj_set_style_text_color(over_label, lv_color_white(), 0);
    lv_obj_align(over_label, LV_ALIGN_TOP_MID, 0, -1);

    retry_button = lv_btn_create(game_over_panel);
    lv_obj_set_size(retry_button, 126, 43);
    lv_obj_align(retry_button, LV_ALIGN_BOTTOM_MID, 0, 1);
    lv_obj_set_style_bg_color(retry_button, lv_color_hex(0x00A896), 0);
    lv_obj_set_style_radius(retry_button, 9, 0);
    lv_obj_add_event_cb(retry_button, retry_event_handler, LV_EVENT_ALL, nullptr);

    lv_obj_t *retry_image = lv_img_create(retry_button);
    lv_img_set_src(retry_image, &jump_retry_img);
    lv_obj_center(retry_image);

    lv_obj_add_flag(game_over_panel, LV_OBJ_FLAG_HIDDEN);
    lv_group_remove_obj(retry_button);

    if (jump_timer == nullptr)
        jump_timer = lv_timer_create(jump_tick, 20, nullptr);

    set_super_knob_page_status(JUMP_PAGE);
    reset_game();
}
