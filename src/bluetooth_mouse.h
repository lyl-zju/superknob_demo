#ifndef BLUETOOTH_MOUSE_H
#define BLUETOOTH_MOUSE_H

#include <Arduino.h>

// 开机初始化蓝牙键盘鼠标
//void init_ble_at_boot(void);
// 进入pc界面初始化蓝牙键盘鼠标
void init_pc_control(void);
// 退出 PC 控制页面时的清理
void exit_pc_control(void);
// 核心逻辑
void run_pc_mouse_logic(float current_angle, float current_velocity);

#endif