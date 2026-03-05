#include "bluetooth_mouse.h"
#include <BleCombo.h> 
// 引入 SimpleFOC，为了最后输出力矩
#include <SimpleFOC.h>

// 声明外部的 motor 对象，以便调用 motor.move()
extern BLDCMotor motor;

// ================= 全局状态变量 =================
int current_pc_mode = 0;       
float pc_enc_zero = 0;        
int last_scroll_idx = 0;    
int last_vol_idx = 0;       
int last_tab_idx = 0;       
bool is_alt_pressed = false;
int mouse_click_state = 0;  

int shake_state = 0; 
unsigned long shake_timer = 0;
bool is_pc_mode_active = false;

// 在 bluetooth.cpp 中新增一个开机初始化函数
// void init_ble_at_boot(void) {
//     Keyboard.begin();
//     Mouse.begin();
//     Serial.println("BLE Stack Initialized at Boot!");
// }

void init_pc_control(void) {
    Keyboard.begin();
    Mouse.begin();
    Serial.println("BLE Stack Initialized at Boot!");
    if (!is_pc_mode_active) {
        // 记录进入页面时的零点
        pc_enc_zero = motor.shaft_angle; 
        is_pc_mode_active = true;
        current_pc_mode = 0; // 默认进入模式0
        Serial.println("Enter PC Control Mode");
    }
}

void exit_pc_control(void) {
    if (is_pc_mode_active) {
        if (is_alt_pressed) { Keyboard.releaseAll(); is_alt_pressed = false; }
        Mouse.release(MOUSE_LEFT);
        Mouse.release(MOUSE_RIGHT);
        is_pc_mode_active = false;
        Serial.println("Exit PC Control Mode");
    }
}

// 传入 SimpleFOC 算好的角度和速度
void run_pc_mouse_logic(float raw_angle, float velocity) {
    unsigned long now = millis();
    
    // ================= 1. 手势检测：摇一摇 =================
    // 直接用 SimpleFOC 算好的 velocity，更精准平滑
    if (velocity > 15.0f && shake_state == 0) {
        shake_state = 1;         
        shake_timer = now;
    } else if (velocity < -15.0f && shake_state == 1 && (now - shake_timer < 400)) {
        // 摇一摇成功，切换模式
        current_pc_mode = (current_pc_mode + 1) % 4; 
        
        pc_enc_zero = raw_angle; // 重置该模式的零点
        last_scroll_idx = 0;
        last_vol_idx = 0;
        if (is_alt_pressed) { Keyboard.releaseAll(); is_alt_pressed = false; }
        Mouse.release(MOUSE_LEFT);
        Mouse.release(MOUSE_RIGHT);
        mouse_click_state = 0;
        
        Serial.print("摇一摇成功！当前模式: ");
        Serial.println(current_pc_mode);
        // 这里你可以加一个 motor_shake(2,2) 来做震动反馈
        
        shake_state = 0; 
    }
    
    if (now - shake_timer > 400) shake_state = 0; 

    // ================= 2. 四模态触感引擎 =================
        float angle = raw_angle - pc_enc_zero; 
        float error = angle; // 目标都在 0 位置
        static float prev_error = 0;
        static const float Kd = 2.0f;   // 微分增益，可根据调试调整

        // 假定此函数大约每 1 ms 调用一次，差分项乘以采样率以放大
        float derivative = (error - prev_error) * 1000.0f;
        prev_error = error;

        float torque = 0;

    switch (current_pc_mode) {
        case 0: { // 网页滚轮
            float scroll_sector = 0.2; 
            int scroll_idx = round(angle / scroll_sector);
            torque = 3.0 * (scroll_idx * scroll_sector - angle); 
            
            if (Keyboard.isConnected() && scroll_idx != last_scroll_idx) {
                Mouse.move(0, 0, (scroll_idx > last_scroll_idx) ? 1 : -1);
                last_scroll_idx = scroll_idx;
            }
            break;
        }
        case 1: { // 音量控制
            if (angle > 2.0) torque = -20.0 * (angle - 2.0);
            else if (angle < -2.0) torque = -20.0 * (angle + 2.0);
            else torque = 0; 
            
            // 表示每转过 0.08 弧度就触发一次音量增减
            float vol_sector = 0.08; 
            int vol_idx = round(angle / vol_sector);
            if (Keyboard.isConnected() && vol_idx != last_vol_idx) {
                if (vol_idx > last_vol_idx) Keyboard.write(KEY_MEDIA_VOLUME_UP);
                else Keyboard.write(KEY_MEDIA_VOLUME_DOWN);
                last_vol_idx = vol_idx;
            }
            break;
        }
        case 2: { // Alt+Tab
            torque = -12.0 * angle; 
            if (Keyboard.isConnected()) {
                if (abs(angle) > 0.55) { 
                    if (!is_alt_pressed) {
                        Keyboard.press(KEY_LEFT_ALT);
                        Keyboard.write(KEY_TAB); 
                        is_alt_pressed = true;
                        last_tab_idx = 0;
                    } else {
                        int tab_idx = (abs(angle) - 0.55) / 0.25; 
                        if (tab_idx > last_tab_idx) {
                            if (angle > 0) Keyboard.write(KEY_TAB); 
                            else {
                                Keyboard.press(KEY_LEFT_SHIFT); 
                                Keyboard.write(KEY_TAB); 
                                Keyboard.release(KEY_LEFT_SHIFT);
                            }
                            last_tab_idx = tab_idx;
                        }
                    }
                } 
                else if (abs(angle) < 0.5 && is_alt_pressed) {
                    Keyboard.releaseAll();
                    is_alt_pressed = false;
                }
            }
            break;
        }
        case 3: { // 鼠标左右键
            torque = -15.0 * angle; 
            if (Keyboard.isConnected()) {
                if (angle > 0.55 && mouse_click_state != 2) {
                    Mouse.press(MOUSE_RIGHT);
                    mouse_click_state = 2;
                } else if (angle < -0.55 && mouse_click_state != 1) {
                    Mouse.press(MOUSE_LEFT);
                    mouse_click_state = 1;
                } else if (abs(angle) < 0.5 && mouse_click_state != 0) {
                    Mouse.release(MOUSE_LEFT);
                    Mouse.release(MOUSE_RIGHT);
                    mouse_click_state = 0;    
                }
            }
            break;
        }
    }

    // ================= 3. 输出力矩给 SimpleFOC =================
    // 因为你在 motor.cpp 里设置了 motor.controller = MotionControlType::torque;
    // 直接传电压/力矩值即可
        // 添加微分阻尼项
        // torque += -Kd * derivative;
    motor.move(torque);
}