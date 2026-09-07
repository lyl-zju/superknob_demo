#include "display.h"
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <main.h>
#include <motor.h>
#include "ui_pages/gui_super_knob.h"
#include "esp_heap_caps.h"



TimerHandle_t poweron_tmr;
static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 320;
static const uint16_t drawBufferLines = 40;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf_1 = NULL;
static bool tft_dma_enabled = false;
lv_ui super_knob_ui;



TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */
/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    //计算刷新区域大小
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setSwapBytes(true);
    if (tft_dma_enabled) {
        tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
        tft.dmaWait();
    } else {
        tft.pushImage(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
    }
    tft.endWrite();

    lv_disp_flush_ready(disp);  //开始下一帧绘制
}
/*-----------------------------------------------------------------------*/

static bool touch_pad_press(int gpio)
{
    static uint8_t press_cnt = 0;
    static bool press_latched = false;
    const bool touched = touchRead(gpio) < 30;
    if (!touched) {
        press_cnt = 0;
        press_latched = false;
        return false;
    }
    if (press_cnt < 5) ++press_cnt;
    if (press_cnt >= 4 && !press_latched) {
        press_latched = true;
        return true;
    }
    return false;
}


//检测页面退出逻辑
static void return_to_iot_main_page(void)
{
    setup_scr_screen_iot_main(&super_knob_ui);
    lv_scr_load_anim(super_knob_ui.screen_iot_main_boday, LV_SCR_LOAD_ANIM_FADE_ON, 100, 10, false);
    set_super_knob_page_status(SUPER_PAGE_BUSY);
    update_motor_config(1);
}

void page_status_check(bool touch_pressed)
{
    if (!touch_pressed) return;
    SUPER_KNOB_PAGE_NUM now_page = get_super_knob_page_status();
    switch (now_page)
    {
    // case WELCOME_PAGE:
    //     break;
    // case WELCOME_PAGE:
    //     break;
    // case IOT_LIGHT_BELT_PAGE:
    //     if(touch_pad_press(ESP32_TOUCH_PIN1)){
    //         ext_iot_light_belt_page();
    //         setup_scr_screen_iot_main(&super_knob_ui);
    //         lv_scr_load_anim(super_knob_ui.screen_iot_main_boday, LV_SCR_LOAD_ANIM_FADE_ON, 100, 10, false);
    //         set_super_knob_page_status(SUPER_PAGE_BUSY);
    //         update_motor_config(1);
    //         update_page_status(CHECKOUT_PAGE);
    //     }
    //     break;
    case IOT_SENSOR_PAGE:
    case IOT_POINTER_PAGE:
        setup_scr_screen_iot_main(&super_knob_ui);
        lv_scr_load_anim(super_knob_ui.screen_iot_main_boday, LV_SCR_LOAD_ANIM_FADE_ON, 100, 10, false);
        set_super_knob_page_status(SUPER_PAGE_BUSY);
        update_motor_config(1);
        update_page_status(CHECKOUT_PAGE);
        break;
    case IOT_MUSIC_PAGE:
        update_page_status(MUSIC_STOP);
        break;
    case IOT_COMPUTER_PAGE: 
        Serial.println("Rebooting to exit PC mode...");
        ESP.restart();
        break;
    case IOT_FAN_SPEED_PAGE:
    case IOT_FAN_DIRECTION_PAGE:
        smart_fan_return_to_menu();
        break;
    default:
        break;
    }

}

/*Will be called by the library to read the encoder*/
static void encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static int now_num = 0;
    static int old_num = 0;
    now_num = get_motor_position();

    data->enc_diff = 0;
    if (now_num > old_num)
    {
        data->enc_diff++;
        old_num = get_motor_position();
        //update_ws2812_status(WS2812_ROLL, 10);
    }
    else if (now_num < old_num)
    {
        data->enc_diff--;
        old_num = get_motor_position();
        //update_ws2812_status(WS2812_ROLL, 10);
    }

    const SUPER_KNOB_PAGE_NUM page = get_super_knob_page_status();
    const bool fan_detail_page = page == IOT_FAN_SPEED_PAGE || page == IOT_FAN_DIRECTION_PAGE;
    const bool touch_pressed = touch_pad_press(ESP32_TOUCH_PIN1);
    if(touch_pressed && !fan_detail_page){
        //update_ws2812_status(WS2812_METEOR_OVERTURN, 10);
        data->state = LV_INDEV_STATE_PR;
    }else{
        data->state = LV_INDEV_STATE_REL;
    }

    if ((page == IOT_FAN_SPEED_PAGE || page == IOT_FAN_DIRECTION_PAGE) && data->enc_diff != 0) {
        smart_fan_handle_encoder_delta(data->enc_diff);
        data->enc_diff = 0;
    }

    page_status_check(touch_pressed);
}

void poweron_timeout(TimerHandle_t pxTimer)
{
    int32_t lArrayIndex;
    configASSERT(pxTimer);
    // 读取超时定时器的ID
    lArrayIndex = (int32_t)pvTimerGetTimerID(pxTimer);
    Serial.print("[E]network_connect_timeout----");
    Serial.println(lArrayIndex);

    xTimerStop(pxTimer, 0);
}



void check_timerout(lv_timer_t *timer)
{
    setup_scr_screen_iot_main(&super_knob_ui);
    //加载动画
    lv_scr_load_anim(super_knob_ui.screen_iot_main_boday, LV_SCR_LOAD_ANIM_OVER_TOP, 200, 50, true);
}

void update_page_status(int page_status)
{
    struct _knob_message *send_message;
    send_message = &LVGL_MSG;   //获取全局变量地址
    send_message->ucMessageID = page_status;    //设置消息ID
    xQueueSend(motor_rcv_Queue, &send_message, (TickType_t)0);
    
    Serial.print("-------------------->>>>");
    Serial.println(send_message->ucMessageID);
}


void Task_lvgl(void *pvParameters)
{
    (void)pvParameters;

    /*初始化显示*/
    lv_init();

    tft.begin();        /* ST7789 init and backlight enable */
    tft_dma_enabled = tft.initDMA();
    tft.setRotation(0); /* Landscape orientation, flipped */

    // A single 40-line buffer uses 19.2 KB instead of two 1/4-screen buffers
    // (76.8 KB). It is sufficient because LVGL flushes the display in strips.
    const uint32_t buf_size = screenWidth * drawBufferLines;
    buf_1 = (lv_color_t *)heap_caps_malloc(
        buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (buf_1 == NULL) {
        Serial.println("[Fatal] LVGL DMA buffer allocation failed; display task stopped.");
        vTaskDelete(NULL);
        return;
    }
    lv_disp_draw_buf_init(&draw_buf, buf_1, NULL, buf_size);
    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;      //创建显示驱动变量
    lv_disp_drv_init(&disp_drv);        //初始化显示驱动
    /*Change the following line to your display resolution*/
    disp_drv.hor_res = screenWidth;     //水平分辨率
    disp_drv.ver_res = screenHeight;    //垂直分辨率
    disp_drv.flush_cb = my_disp_flush;  //设置刷新回调函数
    disp_drv.draw_buf = &draw_buf;      //设置显示缓冲
    lv_disp_drv_register(&disp_drv); //注册显示屏

    /*加载输入设备  按键或者键盘 编码器 或者触摸*/
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_ENCODER; //设置输入设备类型为编码器
    indev_drv.read_cb = encoder_read;       //设置读取回调函数
    super_knob_ui.indev_encoder = lv_indev_drv_register(&indev_drv);
    //想要和按键交互必须创建一个对象组： 并且必须使用以下命令将对象添加到其中
    
    super_knob_ui.defult_group = lv_group_create();     //创建默认组
    lv_group_set_default(super_knob_ui.defult_group);    //设置为默认组
    lv_indev_set_group(super_knob_ui.indev_encoder, super_knob_ui.defult_group);    //将输入设备与组关联

    setup_ui(&super_knob_ui);
    
    update_motor_config(1);
    update_page_status(0);

    //创建开机页面超时定时器
    // poweron_tmr = xTimerCreate("poweron_Timer", (400), pdTRUE, (void *)0, poweron_timeout);
    // xTimerStart(poweron_tmr, 0); //开启倒计时定时器

    for (;;)
    {
            //监听电机运行状态
            struct _knob_message *motor_message;
            if (xQueueReceive(motor_msg_Queue, &(motor_message), (TickType_t)1))
            {
                Serial.print("lvgl_msg_Queue --->");
                Serial.println(motor_message->ucMessageID);
                switch(motor_message->ucMessageID){
                    case MOTOR_INIT:
                        if(super_knob_ui.power_on_bar)
                        lv_bar_set_value(super_knob_ui.power_on_bar, 30, LV_ANIM_ON);
                    break;
                    case MOTOR_INIT_SUCCESS:
                        if(super_knob_ui.power_on_bar)
                        lv_bar_set_value(super_knob_ui.power_on_bar, 75, LV_ANIM_ON);
                    break;
                    case MOTOR_INIT_END:
                    {
                        if(super_knob_ui.power_on_bar)
                        lv_bar_set_value(super_knob_ui.power_on_bar, 100, LV_ANIM_ON);
                        
                        lv_timer_t *_check_timer = lv_timer_create(check_timerout, 800, NULL);  //创建定时器
                        lv_timer_set_repeat_count(_check_timer, 1);     //设置定时器只运行一次
                    }    
                    break;
                    case MOTOR_MUSIC_END:
                    {
                        if(get_super_knob_page_status() == IOT_MUSIC_PAGE)
                        {
                            return_to_iot_main_page();
                        }
                    }
                    break;
                    default:
                    break;
                }
            }

            lv_task_handler(); /* let the GUI do its work */
            vTaskDelay(1);
    }
}
