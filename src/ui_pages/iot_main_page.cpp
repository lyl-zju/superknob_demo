/*
 * @Descripttion: 
 * @version: 
 * @Author: congsir
 * @Date: 2022-05-27 00:22:38
 * @LastEditors: wenzheng 565402462@qq.com
 * @LastEditTime: 2022-07-24 00:40:20
 */
#include "lvgl.h"
#include <TFT_eSPI.h>
#include <stdio.h>
#include "gui_super_knob.h"
#include <Arduino.h>
#include <motor.h>
#include <display.h>
#include "bluetooth_mouse.h"

//图片初始化
LV_IMG_DECLARE(lamp_img);
LV_IMG_DECLARE(leds_img);
LV_IMG_DECLARE(socket_img);
LV_IMG_DECLARE(computer_img);
LV_IMG_DECLARE(air_cond_img);
LV_IMG_DECLARE(sensor_img);
LV_IMG_DECLARE(fan_img);
LV_IMG_DECLARE(tomato_img);
LV_IMG_DECLARE(music_img);
LV_IMG_DECLARE(about_img);
LV_IMG_DECLARE(chip_img);
LV_IMG_DECLARE(game_title_img);
LV_IMG_DECLARE(jump_title_img);

static void scroll_event_cb(lv_event_t *e)
{
    lv_obj_t *cont = lv_event_get_target(e);

    lv_area_t cont_a;
    lv_obj_get_coords(cont, &cont_a);
    lv_coord_t cont_y_center = cont_a.y1 + lv_area_get_height(&cont_a) / 2;

    lv_coord_t r = lv_obj_get_height(cont) * 7 / 10;
    uint32_t i;
    uint32_t child_cnt = lv_obj_get_child_cnt(cont);
    for (i = 0; i < child_cnt; i++)
    {
        lv_obj_t *child = lv_obj_get_child(cont, i);
        lv_area_t child_a;
        lv_obj_get_coords(child, &child_a);

        lv_coord_t child_y_center = child_a.y1 + lv_area_get_height(&child_a) / 2;

        lv_coord_t diff_y = child_y_center - cont_y_center;
        diff_y = LV_ABS(diff_y);

        /*Get the x of diff_y on a circle.*/
        lv_coord_t x;
        /*If diff_y is out of the circle use the last point of the circle (the radius)*/
        if (diff_y >= r)
        {
            x = r;
        }
        else
        {
            /*Use Pythagoras theorem to get x from radius and y*/
            uint32_t x_sqr = r * r - diff_y * diff_y;
            lv_sqrt_res_t res;
            lv_sqrt(x_sqr, &res, 0x8000); /*Use lvgl's built in sqrt root function*/
            x = r - res.i;
        }

        /*Translate the item by the calculated X coordinate*/
        lv_obj_set_style_translate_x(child, x, 0);

        /*Use some opacity with larger translations*/
        lv_opa_t opa = lv_map(x, 0, r, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_obj_set_style_opa(child, LV_OPA_COVER - opa, 0);
    }
}

void lv_set_scroll_box(lv_obj_t* background_btn, void* image_src, const char * text_buff)
{
    /* style */
    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_bg_opa(&style_btn, 0); //设置背景透明
    lv_style_set_radius(&style_btn, 15);

    static lv_style_t style_text;
    lv_style_init(&style_text);
    lv_style_set_text_opa(&style_text, 255);
    lv_style_set_text_color(&style_text, lv_color_black());

    //lv_obj_t* normal_obj = lv_btn_create(cont);
    lv_obj_set_width(background_btn, lv_pct(100));
    lv_obj_set_height(background_btn, lv_pct(40));
    lv_obj_add_style(background_btn, &style_btn, LV_PART_MAIN);

    lv_obj_t* line1 = lv_line_create(background_btn);
    static lv_point_t line_points[] = { {70, 5}, {70, 70} };
    lv_line_set_points(line1, line_points, 2);

    lv_obj_t *img1 = lv_img_create(background_btn);
    lv_img_set_src(img1, image_src);
    lv_obj_align(img1, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* line2 = lv_line_create(background_btn);
    static lv_point_t line_points_2[] = { {80, 40}, {180, 40} };
    lv_line_set_points(line2, line_points_2, 2);

    lv_obj_t* label = lv_label_create(background_btn);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);     /*Circular scroll*/
    lv_obj_set_width(label, 150);
    LV_FONT_DECLARE(lv_font_chinese_source_20); //加载中文字体
    lv_obj_set_style_text_font(label, &lv_font_chinese_source_20, 0);
    lv_label_set_text(label, text_buff);
    lv_obj_add_style(label, &style_text, 0);
    lv_obj_align(label, LV_ALIGN_RIGHT_MID, 50, -5);

}

// 与 lv_set_scroll_box 相同，但右侧用图片代替文字（字库缺“游戏”等字时用）
void lv_set_scroll_box_img(lv_obj_t* background_btn, void* icon_src, void* text_img_src)
{
    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_bg_opa(&style_btn, 0);
    lv_style_set_radius(&style_btn, 15);

    lv_obj_set_width(background_btn, lv_pct(100));
    lv_obj_set_height(background_btn, lv_pct(40));
    lv_obj_add_style(background_btn, &style_btn, LV_PART_MAIN);

    lv_obj_t* line1 = lv_line_create(background_btn);
    static lv_point_t line_points[] = { {70, 5}, {70, 70} };
    lv_line_set_points(line1, line_points, 2);

    lv_obj_t *img1 = lv_img_create(background_btn);
    lv_img_set_src(img1, icon_src);
    lv_obj_align(img1, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* line2 = lv_line_create(background_btn);
    static lv_point_t line_points_2[] = { {80, 40}, {180, 40} };
    lv_line_set_points(line2, line_points_2, 2);

    // 原文字 label 宽 150、RIGHT_MID 偏移 +50、文字左对齐，故文字左边缘距按钮右边缘 100px。
    // 让图片左边缘对齐到同一位置（图片宽 44），偏移 = 50 - 150 + 44 = -56。
    lv_obj_t *img2 = lv_img_create(background_btn);
    lv_img_set_src(img2, text_img_src);
    lv_obj_align(img2, LV_ALIGN_RIGHT_MID, -56, -5);
}

// Jump-game menu item: draw the platform and airborne piece in LVGL so no icon
// bitmap is needed, while keeping the same layout as the other menu entries.
static void lv_set_jump_scroll_box(lv_obj_t *background_btn)
{
    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_bg_opa(&style_btn, 0);
    lv_style_set_radius(&style_btn, 15);

    lv_obj_set_width(background_btn, lv_pct(100));
    lv_obj_set_height(background_btn, lv_pct(40));
    lv_obj_add_style(background_btn, &style_btn, LV_PART_MAIN);

    lv_obj_t *divider = lv_line_create(background_btn);
    static lv_point_t divider_points[] = {{70, 5}, {70, 70}};
    lv_line_set_points(divider, divider_points, 2);

    lv_obj_t *platform_left = lv_obj_create(background_btn);
    lv_obj_set_pos(platform_left, 6, 53);
    lv_obj_set_size(platform_left, 22, 9);
    lv_obj_set_style_bg_color(platform_left, lv_color_hex(0x26A69A), 0);
    lv_obj_set_style_border_width(platform_left, 0, 0);
    lv_obj_set_style_radius(platform_left, 2, 0);

    lv_obj_t *platform_right = lv_obj_create(background_btn);
    lv_obj_set_pos(platform_right, 43, 43);
    lv_obj_set_size(platform_right, 22, 9);
    lv_obj_set_style_bg_color(platform_right, lv_color_hex(0x26A69A), 0);
    lv_obj_set_style_border_width(platform_right, 0, 0);
    lv_obj_set_style_radius(platform_right, 2, 0);

    lv_obj_t *piece = lv_obj_create(background_btn);
    lv_obj_set_pos(piece, 31, 19);
    lv_obj_set_size(piece, 12, 12);
    lv_obj_set_style_bg_color(piece, lv_color_hex(0xFFB300), 0);
    lv_obj_set_style_border_width(piece, 0, 0);
    lv_obj_set_style_radius(piece, 3, 0);

    lv_obj_t *underline = lv_line_create(background_btn);
    static lv_point_t underline_points[] = {{80, 40}, {180, 40}};
    lv_line_set_points(underline, underline_points, 2);

    lv_obj_t *title = lv_img_create(background_btn);
    lv_img_set_src(title, &jump_title_img);
    lv_obj_set_pos(title, 82, 14);
}

static void lamp_btn_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        set_super_knob_page_status(SUPER_PAGE_BUSY);
        setup_scr_screen_pointer(&super_knob_ui);
        lv_scr_load_anim(super_knob_ui.screen_iot_pointer, LV_SCR_LOAD_ANIM_FADE_ON, 100, 10, true);
        update_motor_config(2);
        update_page_status(CHECKOUT_PAGE);
    }
    else if(code == LV_EVENT_VALUE_CHANGED) {
        //LV_LOG_USER("Toggled");
    }
}

static void sensor_btn_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        update_page_status(CHECKOUT_PAGE);
        set_super_knob_page_status(SUPER_PAGE_BUSY);
        setup_scr_screen_iot_sensor(&super_knob_ui);
        lv_scr_load_anim(super_knob_ui.screen_iot_sensor, LV_SCR_LOAD_ANIM_FADE_ON, 200, 100, true);
    }
    else if(code == LV_EVENT_VALUE_CHANGED) {
        //LV_LOG_USER("Toggled");
    }
}

// 引入在 display.cpp 中定义的标志位
extern bool enter_pc_mode_flag;
extern int current_os_mode; // 引入 RTC 变量

static void sensor_computer_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
       // 1. 改变电机手感为顺滑无阻尼
        update_motor_config(0); 
        
        // 2. 将标记改为 1 (电脑系统)，并极速重启！
        current_os_mode = 1; 
        Serial.println("Switching to PC OS...");
        ESP.restart();
    }
}

static void fan_btn_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        update_page_status(CHECKOUT_PAGE);
        set_super_knob_page_status(SUPER_PAGE_BUSY);
        setup_scr_screen_smart_fan(&super_knob_ui);
        lv_scr_load_anim(super_knob_ui.screen_iot_smart_fan, LV_SCR_LOAD_ANIM_FADE_ON, 200, 100, true);
        update_motor_config(1);
    }
    else if(code == LV_EVENT_VALUE_CHANGED) {
        //LV_LOG_USER("Toggled");
    }

}

static void tomato_btn_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
       // 1. 设为忙碌
        set_super_knob_page_status(SUPER_PAGE_BUSY);
        // 2. 加载 UI
        setup_scr_screen_tomato_clock(&super_knob_ui);
        lv_scr_load_anim(super_knob_ui.screen_iot_tomato_clock, LV_SCR_LOAD_ANIM_FADE_ON, 200, 100, true);
        
        // 3. 设置电机手感 (2 表示顺滑的阻尼感，根据你的 motor.cpp 配置来定)
        update_motor_config(2);
        
        // 4. 通知电机任务
        update_page_status(CHECKOUT_PAGE);
    }
    else if(code == LV_EVENT_VALUE_CHANGED) {
        //LV_LOG_USER("Toggled");
    }

}

static void music_btn_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        set_super_knob_page_status(SUPER_PAGE_BUSY);
        setup_scr_screen_iot_music(&super_knob_ui);
        lv_scr_load_anim(super_knob_ui.screen_iot_music, LV_SCR_LOAD_ANIM_FADE_ON, 200, 100, true);
        update_page_status(MUSIC_PLAY);
    }
    else if(code == LV_EVENT_VALUE_CHANGED) {
        //LV_LOG_USER("Toggled");
    }
}

static void game_btn_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        set_super_knob_page_status(SUPER_PAGE_BUSY);
        setup_scr_screen_game(&super_knob_ui);
        lv_scr_load_anim(super_knob_ui.screen_game, LV_SCR_LOAD_ANIM_FADE_ON, 200, 100, true);
        // 手感切换放在 game_page 里：选项菜单=主界面同款档位，游戏中=无阻尼
    }
    else if(code == LV_EVENT_VALUE_CHANGED) {
        //LV_LOG_USER("Toggled");
    }
}

static void jump_btn_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        set_super_knob_page_status(SUPER_PAGE_BUSY);
        setup_scr_screen_jump(&super_knob_ui);
        lv_scr_load_anim(super_knob_ui.screen_jump, LV_SCR_LOAD_ANIM_FADE_ON, 200, 100, true);
    }
}

static void sensor_leds_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        // update_page_status(CHECKOUT_PAGE);
        // set_super_knob_page_status(SUPER_PAGE_BUSY);
        // update_motor_config(4);
        // setup_scr_screen_light_belt(&super_knob_ui);
        // lv_scr_load_anim(super_knob_ui.screen_iot_light_belt, LV_SCR_LOAD_ANIM_FADE_ON, 200, 100, true);
    }
    else if(code == LV_EVENT_VALUE_CHANGED) {
        //LV_LOG_USER("Toggled");
    }

}

static void about_btn_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        update_page_status(CHECKOUT_PAGE);
        set_super_knob_page_status(SUPER_PAGE_BUSY);
        setup_scr_screen_about(&super_knob_ui);
        lv_scr_load_anim(super_knob_ui.screen_about, LV_SCR_LOAD_ANIM_FADE_ON, 200, 100, true);
    }
    else if(code == LV_EVENT_VALUE_CHANGED) {
        //LV_LOG_USER("Toggled");
    }
}



void setup_scr_screen_iot_main(lv_ui *ui)
{
    /* style */
    // static lv_style_t style_iot_main;
    // lv_style_init(&style_iot_main);
    // lv_style_set_border_side(&style_iot_main,LV_BORDER_SIDE_NONE);

    ui->screen_iot_main_boday = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_iot_main_boday, 240, 240);

    ui->screen_iot_main = lv_obj_create(ui->screen_iot_main_boday);
    lv_obj_set_size(ui->screen_iot_main, 240, 240);
    lv_obj_center(ui->screen_iot_main);
    
    lv_obj_set_flex_flow(ui->screen_iot_main, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_event_cb(ui->screen_iot_main, scroll_event_cb, LV_EVENT_SCROLL, NULL); //每次位置变化时触发
    lv_obj_set_style_radius(ui->screen_iot_main, LV_RADIUS_CIRCLE, 0); //设置为圆形
    lv_obj_set_style_clip_corner(ui->screen_iot_main, true, 0);
    lv_obj_set_scroll_dir(ui->screen_iot_main, LV_DIR_VER); //滚动方向为上下滚动
    lv_obj_set_scroll_snap_y(ui->screen_iot_main, LV_SCROLL_SNAP_CENTER); //将子对象与滚动对象的中心对齐
    lv_obj_set_scrollbar_mode(ui->screen_iot_main, LV_SCROLLBAR_MODE_OFF); //从不显示滚动条

    //lv_obj_add_style(ui->screen_iot_main, &style_iot_main, 0);//将样式添加到对象中

    lv_obj_t* lamp_btn = lv_btn_create(ui->screen_iot_main);
    lv_obj_add_event_cb(lamp_btn, lamp_btn_event_handler, LV_EVENT_ALL, NULL);
    lv_set_scroll_box(lamp_btn, (void *)&lamp_img, "台灯");

    // lv_obj_t* leds_btn = lv_btn_create(ui->screen_iot_main);
    // lv_obj_add_event_cb(leds_btn, sensor_leds_event_handler, LV_EVENT_ALL, NULL);
    // lv_set_scroll_box(leds_btn, (void *)&leds_img, "灯带");

    lv_obj_t* fan_btn = lv_btn_create(ui->screen_iot_main);
    lv_obj_add_event_cb(fan_btn, fan_btn_event_handler, LV_EVENT_ALL, NULL);
    lv_set_scroll_box(fan_btn, (void *)&fan_img, "风扇");

    lv_obj_t* tomato_btn = lv_btn_create(ui->screen_iot_main);
    lv_obj_add_event_cb(tomato_btn, tomato_btn_event_handler, LV_EVENT_ALL, NULL);
    lv_set_scroll_box(tomato_btn, (void *)&tomato_img, "番茄");

    lv_obj_t* music_btn = lv_btn_create(ui->screen_iot_main);
    lv_obj_add_event_cb(music_btn, music_btn_event_handler, LV_EVENT_ALL, NULL);
    lv_set_scroll_box(music_btn, (void *)&music_img, "音乐");

    lv_obj_t* game_btn = lv_btn_create(ui->screen_iot_main);
    lv_obj_add_event_cb(game_btn, game_btn_event_handler, LV_EVENT_ALL, NULL);
    lv_set_scroll_box_img(game_btn, (void *)&chip_img, (void *)&game_title_img);

    lv_obj_t *jump_btn = lv_btn_create(ui->screen_iot_main);
    lv_obj_add_event_cb(jump_btn, jump_btn_event_handler, LV_EVENT_ALL, NULL);
    lv_set_jump_scroll_box(jump_btn);

    // lv_obj_t* socket_btn = lv_btn_create(ui->screen_iot_main);
    // lv_set_scroll_box(socket_btn, (void *)&socket_img, "插座");

    lv_obj_t* computer_btn = lv_btn_create(ui->screen_iot_main);
    lv_obj_add_event_cb(computer_btn, sensor_computer_event_handler, LV_EVENT_ALL, NULL);
    lv_set_scroll_box(computer_btn, (void *)&computer_img, "电脑");

    // lv_obj_t* air_cond_btn = lv_btn_create(ui->screen_iot_main);
    // lv_set_scroll_box(air_cond_btn, (void *)&air_cond_img, "空调");

    lv_obj_t* sensor_btn = lv_btn_create(ui->screen_iot_main);
    lv_obj_add_event_cb(sensor_btn, sensor_btn_event_handler, LV_EVENT_ALL, NULL);
    lv_set_scroll_box(sensor_btn, (void *)&sensor_img, "传感");

    lv_obj_t* about_btn = lv_btn_create(ui->screen_iot_main);
    lv_obj_add_event_cb(about_btn, about_btn_event_handler, LV_EVENT_ALL, NULL);
    lv_set_scroll_box(about_btn, (void *)&about_img, "关于");

    /*Update the buttons position manually for first*/
    lv_event_send(ui->screen_iot_main, LV_EVENT_SCROLL, NULL);

    /*Be sure the fist button is in the middle*/
    lv_obj_scroll_to_view(lv_obj_get_child(ui->screen_iot_main, 0), LV_ANIM_OFF);

    //刷新页面调度器
    set_super_knob_page_status(IOT_MAIN_PAGE);
}


