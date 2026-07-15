#include "lvgl.h"
#include "gui_super_knob.h"

void setup_scr_screen_iot_music(lv_ui *ui)
{
    ui->screen_iot_music = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_iot_music, 240, 240);
    lv_obj_clear_flag(ui->screen_iot_music, LV_OBJ_FLAG_SCROLLABLE);

    set_super_knob_page_status(IOT_MUSIC_PAGE);
}
