/*
 * @Descripttion: 游戏页面（钓鱼游戏）
 * @version:
 * @Author: cong（集成）
 * @Date: 2026-09-06
 *
 * 主界面点“游戏”进入。顶部一排选项（开始 / 难度 / 最高分），
 * 旋转旋钮切换焦点，触摸（编码器按下）确认。
 * 返回主界面：直接按 ESP32 上的 reboot 键即可（无需触摸 GPIO0）。
 *
 * 中文文字用 pictures/ 里的 PNG 转成的图片显示（字库缺字）。
 */
#include "lvgl.h"
#include <stdio.h>
#include <math.h>
#include <esp_system.h>
#include <esp_attr.h>
#include <esp_random.h>
#include "gui_super_knob.h"
#include <motor.h>
#include <display.h>

// 中文词图片
LV_IMG_DECLARE(game_title_img);
LV_IMG_DECLARE(game_start_img);
LV_IMG_DECLARE(game_diff_img);
LV_IMG_DECLARE(game_highscore_img);
LV_IMG_DECLARE(game_easy_img);
LV_IMG_DECLARE(game_medium_img);
LV_IMG_DECLARE(game_hard_img);
LV_IMG_DECLARE(game_caught_img);
LV_IMG_DECLARE(game_missed_img);

// ==================== 布局（240x240 屏） ====================
#define AREA_L 8
#define AREA_R 185
#define AREA_T 92
#define AREA_B 228
#define AREA_CX ((AREA_L + AREA_R) / 2)
#define BAR_H 32
#define FISH_R 8
#define PROG_L 196
#define PROG_R 224
#define PROG_T AREA_T
#define PROG_B AREA_B
#define PI_F 3.14159265f

// ==================== 游戏状态 ====================
typedef enum
{
    GAME_MENU,
    GAME_PLAYING,
    GAME_RESULT,
} GamePhase;

static GamePhase phase = GAME_MENU;

// 难度：0 简单 / 1 中等 / 2 困难
static const lv_img_dsc_t *DIFF_IMGS[] = {&game_easy_img, &game_medium_img, &game_hard_img};
static const float DIFF_SPEED[] = {0.7f, 1.0f, 1.5f};
static int difficulty = 0;

// 最高分：存 RTC 内存，软重启不清零（只有彻底断电才会变成随机值）
static RTC_NOINIT_ATTR int game_high_score;

static int session_score = 0;
static float fishY, fishV, barY, progress;
static float last_angle;
static int catch_flash_ticks = 0;

// ==================== UI 对象 ====================
static lv_obj_t *btn_start = NULL;
static lv_obj_t *btn_diff = NULL;
static lv_obj_t *btn_score = NULL;
static lv_obj_t *diff_img = NULL;
static lv_obj_t *high_score_label = NULL;
static lv_obj_t *session_score_label = NULL;
static lv_obj_t *result_img = NULL;
static lv_obj_t *fish_obj = NULL;
static lv_obj_t *bar_obj = NULL;
static lv_obj_t *prog_fill = NULL;
static lv_timer_t *game_timer = NULL;

// ==================== 最高分读写（越界兜底） ====================
static int get_high_score(void)
{
    if (game_high_score < 0 || game_high_score > 9999)
        return 0;
    return game_high_score;
}

static void set_high_score(int v)
{
    if (v < 0)
        v = 0;
    if (v > 9999)
        v = 9999;
    game_high_score = v;
}

// ==================== 旋钮相对角度 ====================
static float get_angle_delta(void)
{
    float angle = get_motor_shaft_angle();
    float delta = angle - last_angle;
    if (delta > PI_F)
        delta -= 2.0f * PI_F;
    else if (delta < -PI_F)
        delta += 2.0f * PI_F;
    last_angle = angle;
    return delta;
}

// ==================== 状态切换 ====================
static void set_menu_mode(void)
{
    phase = GAME_MENU;

    // 选选项时用主界面同款手感（连续密集强档位），转一下咔一下
    update_motor_config(1);
    update_page_status(CHECKOUT_PAGE);

    // 重新把三个选项按钮加回焦点组，让旋钮可以切换
    lv_group_t *g = super_knob_ui.defult_group;
    lv_group_add_obj(g, btn_start);
    lv_group_add_obj(g, btn_diff);
    lv_group_add_obj(g, btn_score);

    // 隐藏游戏元素
    lv_obj_add_flag(fish_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(bar_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(prog_fill, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(session_score_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(result_img, LV_OBJ_FLAG_HIDDEN);
}

static void result_timeout(lv_timer_t *timer)
{
    (void)timer;
    set_menu_mode();
}

static void start_game(void)
{
    phase = GAME_PLAYING;
    session_score = 0;

    // 游戏时旋钮用阻尼手感（动时有阻力、松手停住不回弹），交给捕捉框控制
    update_motor_config(5);
    update_page_status(CHECKOUT_PAGE);

    // 从焦点组移除选项按钮，旋钮交给游戏控制
    lv_group_remove_obj(btn_start);
    lv_group_remove_obj(btn_diff);
    lv_group_remove_obj(btn_score);

    fishY = (AREA_T + AREA_B) / 2.0f;
    fishV = 1.0f;
    barY = fishY;
    progress = 50;
    last_angle = get_motor_shaft_angle();
    catch_flash_ticks = 0;

    lv_label_set_text(session_score_label, "0");
    lv_obj_add_flag(result_img, LV_OBJ_FLAG_HIDDEN);

    // 显示游戏元素
    lv_obj_clear_flag(fish_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(bar_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(prog_fill, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(session_score_label, LV_OBJ_FLAG_HIDDEN);

    // 初始位置
    lv_obj_set_pos(fish_obj, AREA_CX - FISH_R, (int)fishY - FISH_R);
    lv_obj_set_pos(bar_obj, AREA_L + 1, (int)barY - BAR_H / 2);
    lv_obj_set_pos(prog_fill, PROG_L, PROG_B);
    lv_obj_set_size(prog_fill, PROG_R - PROG_L, 0);
}

// ==================== 游戏逻辑 ====================
static void update_fish(float speed_mult)
{
    static int burstTimer = 0;
    float accel;
    if (burstTimer <= 0)
    {
        if (esp_random() % 100 < 3)
            burstTimer = 20;
        accel = ((int)(esp_random() % 200) - 100) / 100.0f * 0.3f;
    }
    else
    {
        burstTimer--;
        accel = ((int)(esp_random() % 200) - 100) / 100.0f * 1.6f;
    }
    fishV += accel * speed_mult;
    float vmax = 3.5f * speed_mult;
    fishV = constrain(fishV, -vmax, vmax);
    fishY += fishV;
    if (fishY < AREA_T + FISH_R)
    {
        fishY = AREA_T + FISH_R;
        fishV = fabsf(fishV);
    }
    if (fishY > AREA_B - FISH_R)
    {
        fishY = AREA_B - FISH_R;
        fishV = -fabsf(fishV);
    }
}

static void on_escape(void)
{
    phase = GAME_RESULT;

    if (session_score > get_high_score())
        set_high_score(session_score);

    lv_img_set_src(result_img, &game_missed_img);
    lv_obj_clear_flag(result_img, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_fmt(high_score_label, "%d", get_high_score());

    update_page_status(BUTTON_CLICK); // 震动反馈

    // 1.5 秒后自动回到选项菜单
    lv_timer_t *t = lv_timer_create(result_timeout, 1500, NULL);
    lv_timer_set_repeat_count(t, 1);
}

static void game_tick(lv_timer_t *timer)
{
    (void)timer;
    if (phase != GAME_PLAYING)
        return;

    float speed_mult = DIFF_SPEED[difficulty];

    update_fish(speed_mult);

    // 旋钮控制捕捉框
    float delta = get_angle_delta();
    barY -= delta / (2.0f * PI_F) * (AREA_B - AREA_T);
    if (barY < AREA_T + BAR_H / 2.0f)
        barY = AREA_T + BAR_H / 2.0f;
    if (barY > AREA_B - BAR_H / 2.0f)
        barY = AREA_B - BAR_H / 2.0f;

    // 进度：鱼在捕捉框里则增长，否则下降
    if (fabsf(fishY - barY) <= BAR_H / 2.0f + FISH_R)
        progress += 0.6f;
    else
        progress -= 0.4f;
    if (progress > 100)
        progress = 100;
    if (progress < 0)
        progress = 0;

    // 渲染
    lv_obj_set_pos(fish_obj, AREA_CX - FISH_R, (int)fishY - FISH_R);
    lv_obj_set_pos(bar_obj, AREA_L + 1, (int)barY - BAR_H / 2);
    int fillH = (int)(progress / 100.0f * (PROG_B - PROG_T));
    lv_obj_set_pos(prog_fill, PROG_L, PROG_B - fillH);
    lv_obj_set_size(prog_fill, PROG_R - PROG_L, fillH);
    lv_label_set_text_fmt(session_score_label, "%d", session_score);

    // “钓到了”短暂提示
    if (catch_flash_ticks > 0)
    {
        catch_flash_ticks--;
        if (catch_flash_ticks == 0)
            lv_obj_add_flag(result_img, LV_OBJ_FLAG_HIDDEN);
    }

    // 钓到一条
    if (progress >= 100)
    {
        session_score++;
        catch_flash_ticks = 10; // 约 0.3 秒
        lv_img_set_src(result_img, &game_caught_img);
        lv_obj_clear_flag(result_img, LV_OBJ_FLAG_HIDDEN);
        update_page_status(BUTTON_CLICK); // 震动反馈
        // 放一条新鱼
        fishY = (AREA_T + AREA_B) / 2.0f;
        fishV = 1.0f;
        progress = 50;
    }
    // 鱼跑了，结束
    else if (progress <= 0)
    {
        on_escape();
    }
}

// ==================== 按钮回调 ====================
static void start_btn_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        start_game();
}

static void diff_btn_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        difficulty = (difficulty + 1) % 3;
        lv_img_set_src(diff_img, DIFF_IMGS[difficulty]);
    }
}

static void score_btn_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        // 最高分一直显示在右侧，点一下给个震动反馈
        lv_label_set_text_fmt(high_score_label, "%d", get_high_score());
        update_page_status(BUTTON_CLICK);
    }
}

static lv_obj_t *create_btn(lv_obj_t *parent, lv_event_cb_t cb, const lv_img_dsc_t *img, int x, int y)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_ALL, NULL);
    lv_obj_set_size(btn, 74, 34);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_radius(btn, 8, 0);

    lv_obj_t *img_obj = lv_img_create(btn);
    lv_img_set_src(img_obj, img);
    lv_obj_center(img_obj);
    return btn;
}

// ==================== 页面初始化 ====================
void setup_scr_screen_game(lv_ui *ui)
{
    // 上电随机值纠正为 0
    if (game_high_score < 0 || game_high_score > 9999)
        game_high_score = 0;

    ui->screen_game = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_game, 240, 240);
    lv_obj_center(ui->screen_game);
    lv_obj_set_style_bg_color(ui->screen_game, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(ui->screen_game, LV_OPA_COVER, 0);

    // 标题「游戏」
    lv_obj_t *title = lv_img_create(ui->screen_game);
    lv_img_set_src(title, &game_title_img);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    // 三个选项按钮
    btn_start = create_btn(ui->screen_game, start_btn_event_handler, &game_start_img, 3, 28);
    btn_diff = create_btn(ui->screen_game, diff_btn_event_handler, &game_diff_img, 83, 28);
    btn_score = create_btn(ui->screen_game, score_btn_event_handler, &game_highscore_img, 163, 28);

    // 当前难度（难度按钮下方）
    diff_img = lv_img_create(ui->screen_game);
    lv_img_set_src(diff_img, DIFF_IMGS[difficulty]);
    lv_obj_set_pos(diff_img, 83 + (74 - 30) / 2, 66);

    // 最高分数字（最高分按钮下方）
    high_score_label = lv_label_create(ui->screen_game);
    lv_obj_set_style_text_color(high_score_label, lv_color_black(), 0);
    lv_label_set_text_fmt(high_score_label, "%d", get_high_score());
    lv_obj_set_width(high_score_label, 74);
    lv_obj_set_style_text_align(high_score_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(high_score_label, 163, 70);

    // 结果提示（开始按钮下方，钓到了/没钓到）
    result_img = lv_img_create(ui->screen_game);
    lv_img_set_src(result_img, &game_missed_img);
    lv_obj_add_flag(result_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(result_img, 8, 66);

    // 游戏区域（黑色背景 + 白色边框）
    lv_obj_t *play_area = lv_obj_create(ui->screen_game);
    lv_obj_set_pos(play_area, AREA_L, AREA_T);
    lv_obj_set_size(play_area, AREA_R - AREA_L + 1, AREA_B - AREA_T + 1);
    lv_obj_set_style_bg_color(play_area, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(play_area, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(play_area, 2, 0);
    lv_obj_set_style_border_color(play_area, lv_color_white(), 0);
    lv_obj_set_style_radius(play_area, 0, 0);

    // 进度条底（右侧，深灰）
    lv_obj_t *prog_bg = lv_obj_create(ui->screen_game);
    lv_obj_set_pos(prog_bg, PROG_L, PROG_T);
    lv_obj_set_size(prog_bg, PROG_R - PROG_L, PROG_B - PROG_T);
    lv_obj_set_style_bg_color(prog_bg, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(prog_bg, 0, 0);
    lv_obj_set_style_border_width(prog_bg, 0, 0);

    // 进度条填充（绿色，自下而上）
    prog_fill = lv_obj_create(ui->screen_game);
    lv_obj_set_pos(prog_fill, PROG_L, PROG_B);
    lv_obj_set_size(prog_fill, PROG_R - PROG_L, 0);
    lv_obj_set_style_bg_color(prog_fill, lv_color_hex(0x00C853), 0);
    lv_obj_set_style_radius(prog_fill, 0, 0);
    lv_obj_set_style_border_width(prog_fill, 0, 0);

    // 捕捉框（绿色横条）
    bar_obj = lv_obj_create(ui->screen_game);
    lv_obj_set_size(bar_obj, AREA_R - AREA_L - 1, BAR_H);
    lv_obj_set_style_bg_color(bar_obj, lv_color_hex(0x00C853), 0);
    lv_obj_set_style_radius(bar_obj, 4, 0);
    lv_obj_set_style_border_width(bar_obj, 0, 0);

    // 鱼（黄色圆点）
    fish_obj = lv_obj_create(ui->screen_game);
    lv_obj_set_size(fish_obj, FISH_R * 2, FISH_R * 2);
    lv_obj_set_style_radius(fish_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(fish_obj, lv_color_hex(0xFFD600), 0);
    lv_obj_set_style_border_width(fish_obj, 2, 0);
    lv_obj_set_style_border_color(fish_obj, lv_color_white(), 0);

    // 本局得分（游戏区左上角，白色数字）
    session_score_label = lv_label_create(ui->screen_game);
    lv_obj_set_style_text_color(session_score_label, lv_color_white(), 0);
    lv_label_set_text(session_score_label, "0");
    lv_obj_set_pos(session_score_label, AREA_L + 4, AREA_T + 2);

    // 游戏定时器（常驻，非 PLAYING 时直接返回）
    if (game_timer == NULL)
        game_timer = lv_timer_create(game_tick, 30, NULL);

    // 初始进入选项菜单状态
    set_menu_mode();

    // 刷新页面调度器
    set_super_knob_page_status(GAME_PAGE);
}
