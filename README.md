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
- **风扇** — 通过局域网控制米家循环扇的开关、风速和方向
- **游戏** — 钓鱼小游戏（旋钮控制捕捉框，追着鱼移动）
- **跳一跳** — 一维跳跃小游戏（旋钮弹簧蓄力，松手后起跳）
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
| 6 | 风扇详情页档位（物理档距稳定，界面软件侧 2 倍灵敏度） |

跳一跳不使用上述预设配置，而是在 FOC 任务中使用独立的弹簧控制模式。主要参数位于 `src/motor.cpp`：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `JUMP_MAX_ANGLE_RAD` | 100° | 100% 蓄力对应的最大旋转角度 |
| `JUMP_MIN_RELEASE_RAD` | 8° | 可触发跳跃的最小蓄力角度 |
| `JUMP_RELEASE_DROP_RAD` | 3° | 判断旋钮已经松手的回弹角度 |
| `JUMP_SPRING_STRENGTH` | 3.2 | 弹簧回中力度 |
| `JUMP_SPRING_DAMPING` | 0.08 | 回中过程的阻尼强度 |

## 目录结构

```
src/
  main.cpp           入口、任务/队列创建
  motor.cpp/.h       FOC 电机控制、力反馈手感、音乐发声
  display.cpp/.h     屏幕初始化
  bluetooth_mouse.*  蓝牙鼠标/键盘（电脑模式）
  fan_controller.*   米家风扇 UDP/MIoT 控制
  miot/              miIO 协议实现
  ui_pages/          LVGL 各页面
    iot_main_page.cpp    主界面
    iot_pointer_page.cpp 台灯
    iot_tomato_clock.cpp 番茄钟
    iot_music_page.cpp   音乐
    iot_smart_fan.cpp    风扇控制
    game_page.cpp        钓鱼游戏
    jump_page.cpp        一维跳一跳游戏
    iot_sensor_page.cpp  传感
    about_page.cpp       关于
    ...
  ui_image_src/      由 PNG 生成的中文图片 C 数组（字库缺字时用）
scripts/
  gen_game_images.py  把 pictures/ 下的钓鱼游戏中文 PNG 转成 LVGL 图片 C 文件
  gen_jump_images.py  生成跳一跳标题和“再来一局”LVGL 图片 C 文件
pictures/            中文文字素材 PNG（图片生成源）
```

## 游戏说明

- 中文字库 `lv_font_chinese_source_20` 只含少量常用字。钓鱼游戏缺少的文字由 `scripts/gen_game_images.py` 转换；跳一跳的标题和“再来一局”由 `scripts/gen_jump_images.py` 生成。
- 钓鱼游戏最高分存于 RTC 内存，软重启不清零；彻底断电后为随机值，代码里做了越界兜底。跳一跳当前只显示本局实时得分，不保存最高分。
## 米家智能风扇

风扇功能通过家庭局域网直接控制米家直流变频台式循环扇。

已验证的风扇：

- 产品型号：BPLDS10DM
- MIoT 型号：`xiaomi.fan.p69`
- 已验证固件：`1.0.5`
- 通信：ESP32 → 家庭 Wi-Fi → 风扇，UDP `54321`

手机和电脑不参与日常转发；电脑端工具只用于首次取得 token 和诊断。

## 1. 准备环境

推荐使用 Windows 10/11、VS Code、PlatformIO IDE 扩展、支持数据传输的 USB 线。系统不能识别开发板时安装 CH340/CH341 USB 串口驱动。也可以安装 [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) 后只使用命令行。

项目所需的 ESP32 平台和 Arduino 库全部写在 `platformio.ini` 中。首次构建时 PlatformIO 会自动下载：

- Espressif32 Arduino 框架
- Simple FOC `2.2.1`
- LVGL `8.4.0`
- TFT_eSPI `2.5.0`
- ESP32 BLE Keyboard `0.3.2`

`lib/ESP32-BLE-Combo-main` 是项目内置库，会随仓库一起下载，不需要另外安装。

## 2. 克隆并配置私密信息

克隆仓库后，将模板复制为本地配置：

```powershell
Copy-Item include/secrets.example.h include/secrets.h
```

Linux/macOS：

```bash
cp include/secrets.example.h include/secrets.h
```

然后编辑 `include/secrets.h`：

```cpp
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define FAN_IP "192.168.1.100"
#define FAN_TOKEN "PUT_32_HEX_TOKEN_HERE"
```

- `FAN_TOKEN` 必须是 32 位十六进制字符。
- ESP32 和风扇必须能通过同一局域网互相访问。
- 建议在路由器中为风扇设置 DHCP 地址保留，避免 `FAN_IP` 变化。
- `include/secrets.h` 已被 Git 忽略，禁止强制添加或上传。
- 仓库只提交不含真实凭据的 `include/secrets.example.h`。

## 3. 获取 FAN_TOKEN

### 推荐：二维码登录

使用开源的 [Xiaomi Cloud Tokens Extractor](https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor)。Windows 可从 [最新版 Releases](https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor/releases/latest) 下载 `token_extractor.exe`。

1. 启动工具并选择二维码登录（`q`）。
2. 使用米家/小米账号扫码并确认登录。
3. 中国大陆米家账号选择区域 `cn`；其他账号选择设备实际所在区域，也可以留空检查全部区域。
4. 在设备列表中找到 `MODEL: xiaomi.fan.p69`。
5. 将其 `IP` 写入 `FAN_IP`，将 32 位 `TOKEN` 写入 `FAN_TOKEN`。
6. 关闭工具并删除本地输出记录；不要截图、提交或分享 token。

二维码方式可以避免直接把小米账号密码输入第三方命令行工具。提取器、登录缓存和输出文件只应保存在本机，不属于仓库内容。

### 可选：python-miio

`python-miio` 是电脑端诊断工具，不是固件依赖。此前验证本项目时显示的版本是 `0.6.0.dev0`；这是开发版标识，不应把它误写成固定 PyPI 发布版本。

在独立虚拟环境中安装当前开发版：

```powershell
py -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip
.\.venv\Scripts\python.exe -m pip install "git+https://github.com/rytilahti/python-miio.git"
.\.venv\Scripts\miiocli.exe --help
```

也可以安装预发布版：

```powershell
py -m pip install --pre python-miio
```

已有 IP 和 token 后可做只读连通测试：

```powershell
miiocli device --ip 192.168.1.100 --token YOUR_32_HEX_TOKEN info
```

不要把含 token 的命令写入脚本、README、Issue 或终端输出文件。部分开发版曾存在 CLI/Click 兼容问题；若命令行入口异常，优先使用最新版开发分支，或仅使用二维码提取器取得 token。

## 4. 构建与烧录

VS Code：打开仓库根目录，等待 PlatformIO 安装平台和依赖，连接 LOLIN32 Lite，然后依次执行 Build 和 Upload。

命令行：

```powershell
pio run
pio run --target upload
pio device monitor --baud 115200
```

`platformio.ini` 不固定串口号，由 PlatformIO 自动探测。若有多个串口，可临时指定：

```powershell
pio run --target upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

如果提示端口被占用，请先关闭 PlatformIO Serial Monitor，再重新上传。

## 5. 版本库边界

应提交：源码、公开头文件、`include/secrets.example.h`、项目内置库、`platformio.ini`、`scripts/`、`docs/`、字体和图像资源。

不得提交：

- `include/secrets.h`，以及真实 Wi-Fi 名称、密码、风扇 IP/token
- `.pio/`、`.tmp/`、`tmp/` 等构建或渲染产物
- `.venv/`、Python 缓存
- `token_extractor.exe`、二维码、登录缓存和 token 输出
- 本机 IDE 数据库与串口日志

## 6. 常见问题

- `TOUCH_CS pin not defined`：项目不使用 TFT_eSPI 的电阻触摸接口，这是警告，不影响当前 GPIO 触摸键。
- 找不到 `secrets.h`：从 `include/secrets.example.h` 复制后填写；真实文件不会由 Git 提供。
- 风扇离线：确认 IP 未变化、token 属于当前设备、网络互通，且路由器未启用客户端隔离。
- COM 端口忙：关闭串口监视器或其他占用该端口的软件。

实现状态见 [风扇功能实施计划](docs/fan-feature-plan.md)，协议背景见 [可行性报告](docs/report-source.md)。
