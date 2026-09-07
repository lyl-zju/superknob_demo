
#include <main.h>
#include <display.h>
#include <motor.h>
#include "bluetooth_mouse.h"
#include <TFT_eSPI.h>
#include <esp_system.h> // 引入 ESP32 系统库以获取复位原因


//#include <tuya_control.h>
//#ifdef ENABLE_BLE_KEY_BOARD
//#endif 
//#include <ws2812_driver.h>

TaskHandle_t Task_foc_Handle;  //foc 任务
TaskHandle_t Task_lvgl_Handle; //lvgl 任务
TaskHandle_t Task_module_Handle; //子模块通讯任务
//TaskHandle_t Task_ws2812_Handle; //ws2812 任务

QueueHandle_t motor_msg_Queue;  //lvgl 接收消息队列
QueueHandle_t motor_rcv_Queue;  //motor 接收消息队列
//QueueHandle_t ws2812_rcv_Queue;  //ws2812 接收消息队列
//QueueHandle_t iot_control_Queue;  //ws2812 接收消息队列

_knob_message LVGL_MSG;
_knob_message MOTOR_MSG;
//_ws2812_message WS2812_MSG;
//_iot_control_message IOT_CONTROL_MSG;

extern TFT_eSPI tft; // 引用在 display.cpp 中定义的屏幕对象
// 1. 存入 RTC 内存，且警告编译器：无论如何绝对不许自动初始化它！
RTC_NOINIT_ATTR int current_os_mode;


void setup()
{
    pinMode(12, OUTPUT);
    digitalWrite(12, HIGH);     //设置使能引脚
    Serial.begin(115200);
    //Serial1.begin(115200);
    // 【新增】在任务启动前，趁着堆内存完整，先分配蓝牙内存
    //init_ble_at_boot();
    motor_msg_Queue = xQueueCreate(10, sizeof(struct _knob_message *));
    motor_rcv_Queue = xQueueCreate(10, sizeof(struct _knob_message *));
    //ws2812_rcv_Queue = xQueueCreate(10, sizeof(_ws2812_message *));
    //iot_control_Queue = xQueueCreate(10, sizeof(_ws2812_message *));

    // A software restart is used to enter PC mode. All hardware reset causes
    // (including the reboot button) return to the main UI.
    const esp_reset_reason_t reset_reason = esp_reset_reason();
    if (reset_reason != ESP_RST_SW) {
        current_os_mode = 0;
        Serial.println("Hardware reset: Starting at the main UI.");
    }
    else {
        // Preserve the mode requested immediately before ESP.restart().
        Serial.print("Reset reason: ");
        Serial.println(reset_reason);
        Serial.println("Software reset: Retaining requested OS mode.");
    }
    // 安全兜底：如果内存里的值因为意外变成了乱码，强制纠正为 0
    if (current_os_mode != 0 && current_os_mode != 1) {
        current_os_mode = 0; 
    }

    if(current_os_mode == 0) {
        // ========== 系统 A：智能旋钮主界面 ==========
        Serial.println("Booting System A: LVGL UI");
        xTaskCreatePinnedToCore(
            Task_lvgl, "Task_lvgl", 4096, NULL, 3, &Task_lvgl_Handle, LVGL_RUNNING_CORE);

    }
    else{
        // ========== 系统 B：电脑鼠标控制器 ==========
        Serial.println("Booting System B: PC Mode");
        
        // 1. 初始化屏幕底层的硬件，直接写字，不启动 LVGL 任务！
        tft.begin();
        tft.setRotation(0);
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE); tft.setTextSize(3);
        tft.setCursor(50, 140); tft.println("PC MODE");
        tft.setTextSize(2); tft.setTextColor(TFT_DARKGREY);
        tft.setCursor(20, 200); tft.println("Touch: Next Mode");

        // 2. 独享海量完整内存，启动蓝牙！绝对不会崩溃！
        init_pc_control();
    }

    // 电机任务是两个系统都要用的，照常启动
    xTaskCreatePinnedToCore(
        Task_foc, "Task_foc", 4096, NULL, 2, &Task_foc_Handle, ESP32_RUNNING_CORE);
    //xTaskCreatePinnedToCore(
        //Task_module, "Task_knob", 2048, NULL, 4, &Task_module_Handle, ESP32_RUNNING_CORE);
    //xTaskCreatePinnedToCore(
        //Task_ws2812, "Task_ws2812", 1024, NULL, 4, &Task_ws2812_Handle, ESP32_RUNNING_CORE);

}

void loop()
{
    delay(10000);
}



