# 米家直流变频台式循环扇（实机 `xiaomi.fan.p69`）由 ESP32 直接局域网控制的可行性报告

日期：2026-09-07

## 结论

方案一可实现：ESP32 可在家庭局域网内直接向米家直流变频台式循环扇 BPLDS10DM 发送 MIoT/miIO UDP 指令，不需要虚拟机，也不需要让电脑或手机长期在线。

综合判断：风速、摆风开关和预设摆角为高可行性；单次向左/向右“点动”为中等可行性，必须先对用户的实际固件做一次验证。

## 证据链

1. 小米官方页面确认 BPLDS10DM 支持 2.4 GHz Wi-Fi、直吹模式 100 档风速、水平 120° 与垂直 100° 摆动。
   - https://www.mi.com/global/product/xiaomi-smart-desktop-air-circulation-fan/
   - https://www.mi.com/global/product/xiaomi-smart-desktop-air-circulation-fan/specs/
2. 用户实机只读查询确认内部 MIoT model 为 `xiaomi.fan.p69`、固件 `1.0.5`。一份专门针对这一型号的 SmartThings Edge 驱动标注协议证据为 confirmed，并以设备局域网 IP 与 32 位十六进制 token 进行本地通信。`p70` 是另一地区/产品条目的型号，不能代替本机实测结果。
   - https://github.com/wonjj6768/smartthings-miot-edge-driver
   - https://github.com/wonjj6768/smartthings-miot-edge-driver/tree/main/miot/xiaomi-fan-p69
3. `xiaomi.fan.p69` 的 MIoT 规范公开了所需属性与动作：风速 1–100、水平摆动、水平摆角 30/60/90/120，以及向左、向右动作。
   - https://home.miot-spec.com/spec/xiaomi.fan.p69
4. 上述精确型号驱动的源码实现了 UDP 54321、设备握手、MD5 派生、AES-128-CBC、`get_properties`、`set_properties` 和 `action`，说明它不是只经由云端转发。
   - https://github.com/wonjj6768/smartthings-miot-edge-driver/blob/main/miot/xiaomi-fan-p69/src/miot.lua
5. OpenMiHome 的协议文档和 python-miio 项目提供了相同的 miIO 局域网协议与通用 MIoT 控制工具，可用于写 ESP32 前的电脑端验证。
   - https://github.com/OpenMiHome/mihome-binary-protocol/blob/master/doc/PROTOCOL.md
   - https://github.com/rytilahti/python-miio

## 具体控制接口

设备服务 SIID 为 2：

| 功能 | 类型 | 标识 | 值 |
|---|---|---:|---|
| 电源 | 属性 | PIID 1 | true / false |
| 模式 | 属性 | PIID 3 | 0 直吹，1 自然风 |
| 无级风速 | 属性 | PIID 5 | 1–100 |
| 水平摆风 | 属性 | PIID 6 | true / false |
| 水平摆角 | 属性 | PIID 7 | 30 / 60 / 90 / 120 |
| 向左点动 | 动作 | AIID 4 | 无参数 |
| 向右点动 | 动作 | AIID 5 | 无参数 |

例如设置 60 档风速的明文 MIoT 负载为：

```json
{"id":1,"method":"set_properties","params":[{"did":"set.2.5","siid":2,"piid":5,"value":60}]}
```

负载还需按 miIO 协议使用设备 token 派生密钥与 IV，加密并封装后通过 UDP 54321 发出。

## 无虚拟机的实施方式

电脑只在首次验证时临时使用：

1. 在路由器中查到风扇 IP，并为它设置 DHCP 地址保留。
2. 用 Windows 版 Xiaomi Cloud Tokens Extractor 或其 Python 版本取得设备 token；token 属于设备凭据，不应提交到 Git。
   - https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor
3. 用 python-miio 核实 model 是 `xiaomi.fan.p70`，读取属性，然后分别测试风速、摆风、摆角与左右点动。
4. 测试通过后，将相同协议的最小实现移植到 ESP32。完成后电脑可以关机，ESP32 和风扇在同一局域网即可。

ESP32 本身具备所需的 UDP、AES 和 MD5 能力；ESP-IDF 内置 Mbed TLS 并支持 AES。
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mbedtls.html

## 风险与设计建议

- 一份 BPLDS10DM 的 Homebridge issue 报告过角度按钮和移动控制异常，因此左右点动需要实机验证；这不否定基本局域网控制，因为另一个精确型号驱动已实现风速、摆风和角度。
  - https://github.com/merdok/homebridge-miot/issues/742
- 手机和电脑不是通信中转站；正常工作链路是 `旋钮 ESP32 → 家庭路由器 → 风扇`。
- ESP32 与风扇必须处于可互访的同一局域网；访客网络、AP 隔离或 VLAN 可能阻断 UDP。
- 旋钮界面可以立即更新，但网络发送应合并/限频到约 150–300 ms，避免每个编码器脉冲都产生报文。
- 网络通信应放入独立任务或队列，避免超时重试卡住 LVGL 与电机控制。
- Wi-Fi 密码和风扇 token 应放进被 `.gitignore` 排除的本地 secrets 文件。

## 最终判定

建议继续方案一，但采用“先做电脑端最小验证，再写 ESP32”的顺序。若风速与水平控制均通过，后续不需要 Home Assistant、虚拟机或常开电脑；若仅左右点动失败，可退化为“水平摆风开关 + 30/60/90/120 摆角”，仍能满足大部分旋钮控制需求。
