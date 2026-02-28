/*
 * 极简版电脑控制页面 (PC Control Mode)
 */
#include "lvgl.h"
#include "gui_super_knob.h"

void setup_scr_screen_player(lv_ui *ui)
{
    // 1. 创建页面的基础容器
    ui->screen_iot_player = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_iot_player, 240, 240);
    lv_obj_center(ui->screen_iot_player);

    // 2. 设置深色背景 (可选，让界面看起来酷一点)
    lv_obj_set_style_bg_color(ui->screen_iot_player, lv_color_hex(0x1e2732), 0);

    // 3. 添加主标题文字
    lv_obj_t* label_title = lv_label_create(ui->screen_iot_player);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(label_title, "PC Mode");
    
    // 如果你有大字体，可以用这行（没有的话注释掉）：
    // LV_FONT_DECLARE(lv_font_super_knob_30); 
    // lv_obj_set_style_text_font(label_title, &lv_font_super_knob_30, 0);
    
    lv_obj_align(label_title, LV_ALIGN_CENTER, 0, -20); // 居中偏上

    // 4. 添加操作提示文字
    lv_obj_t* label_hint = lv_label_create(ui->screen_iot_player);
    lv_obj_set_style_text_color(label_hint, lv_color_hex(0x888888), 0);
    lv_label_set_text(label_hint, "Shake to switch mode\nTouch to exit");
    lv_obj_set_style_text_align(label_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_hint, LV_ALIGN_CENTER, 0, 30); // 居中偏下

    // 5. 【最核心逻辑】设置系统页面状态，这一步是触发 motor.cpp 电脑逻辑的唯一凭证！
    set_super_knob_page_status(IOT_COMPUTER_PAGE);
}