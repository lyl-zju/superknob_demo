# SuperKnob 智能旋钮

一个基于 **ESP32 + FOC 无刷电机 + AS5600 磁编码器** 的智能旋钮（smart knob）。旋钮带力反馈，可在不同档位间"咔哒"定位，也可切换成阻尼/顺滑手感；屏幕上是一套 LVGL 图形界面，通过旋转 + 按下旋钮来操作。

> 本项目是 [smartknob](https://github.com/scottbez1/smartknob) 风格硬件的自研固件实现。

## 硬件

| 部件 | 型号 / 参数 |
|------|-------------|
| 主控 | ESP32 (LOLIN32 Lite) |
| 电机 | FOC 无刷直流电机（BLDC，7 对极），SimpleFOC 驱动 |
| 编码器 | AS5600 磁编码器（I2C，地址 0x36） |
| 屏幕 | ST7789 240×320 触摸屏（TFT_eSPI） |
| UI 框架 | LVGL 8.4.0 |
| 蓝牙 | ESP32 BLE Keyboard（"电脑"模式 = 蓝牙鼠标/键盘） |

**引脚定义**（见 `src/motor.cpp`）：

| 功能 | 引脚 |
|------|------|
| 电机 PWM A / B / C | 32 / 33 / 25 |
| 电机使能 EN | 12 |
| I2C SDA / SCL（接 AS5600） | 23 / 5 |
| 旋钮按键（触摸） | GPIO0 |

## 功能

主界面（可上下滚动）包含以下入口：

- **台灯** — 亮度/开关控制
- **番茄钟** — 计时器
- **音乐** — 播放一段内置旋律（用电机发声）
- **游戏** — 钓鱼小游戏（旋钮控制捕捉框，追着鱼移动）
- **电脑** — 切换到蓝牙鼠标/键盘模式，把旋钮当电脑外设
- **传感** — 传感器页面
- **关于** — 关于页

另有欢迎页、闹钟等页面。

## 旋钮手感（电机模式）

`src/motor.cpp` 里的 `super_knob_configs[]` 定义了多种力反馈手感，例如：

| 配置 | 说明 |
|------|------|
| 0 | 无档位、顺滑 |
| 1 | 连续密集强档位（"咔哒"一下一个） |
| 2 | 精细档位（台灯/番茄钟用） |
| 3 | 精细、无档位 |
| 4 | 两档开关（强档位） |
| 5 | **纯粘性阻尼**：转动有阻力、松手停住不回弹（钓鱼游戏用） |

## 目录结构

```
src/
  main.cpp           入口、任务/队列创建
  motor.cpp/.h       FOC 电机控制、力反馈手感、音乐发声
  display.cpp/.h     屏幕初始化
  bluetooth_mouse.*  蓝牙鼠标/键盘（电脑模式）
  ui_pages/          LVGL 各页面
    iot_main_page.cpp    主界面
    iot_pointer_page.cpp 台灯
    iot_tomato_clock.cpp 番茄钟
    iot_music_page.cpp   音乐
    game_page.cpp        钓鱼游戏
    iot_sensor_page.cpp  传感
    about_page.cpp       关于
    ...
  ui_image_src/      由 PNG 生成的中文图片 C 数组（字库缺字时用）
scripts/
  gen_game_images.py 把 pictures/ 下的中文 PNG 转成 LVGL 图片 C 文件
pictures/            中文文字素材 PNG（图片生成源）
```

## 编译与烧录

用 [PlatformIO](https://platformio.org/) 打开项目（含 `platformio.ini`）。

```bash
# 编译
platformio run

# 烧录（默认 COM12，可在 platformio.ini 改 upload_port）
platformio run -t upload
```

**依赖**（`platformio.ini` 已声明）：

- `askuric/Simple FOC@2.2.1`
- `t-vk/ESP32 BLE Keyboard`
- `bodmer/TFT_eSPI`
- `lvgl/lvgl@8.4.0`

## 说明

- 中文字库 `lv_font_chinese_source_20` 只含少量常用字，缺字的地方（如"游戏"）用 `scripts/gen_game_images.py` 把 PNG 转成图片显示。
- 游戏最高分存于 RTC 内存，软重启不清零；彻底断电后为随机值，代码里做了越界兜底。
