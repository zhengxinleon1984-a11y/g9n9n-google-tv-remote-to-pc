# G9N9N Google TV 遥控器直连电脑：完整使用手册

本文对应发行版 `0.1.15`，对应固件：

```text
1.2.10-g9n9n-idle-activity-fix
```

适用硬件：

```text
ES3C28P / ESP32-S3 N16R8 / 16MB Flash / 8MB PSRAM
320x240 ILI9341
G9N9N（Windows 名称 Chromecast Remote）
原生 USB：HID + UAC + CDC
```

## 目录

- [1. 使用前必须知道](#1-使用前必须知道)
- [2. 刷写与升级](#2-刷写与升级)
- [3. 第一次开机](#3-第一次开机)
- [4. 配网与网页后台](#4-配网与网页后台)
- [5. 配对 G9N9N](#5-配对-g9n9n)
- [6. 豆包语音输入](#6-豆包语音输入)
- [7. 按键映射与组合键](#7-按键映射与组合键)
- [8. 配置备份与恢复](#8-配置备份与恢复)
- [9. 状态屏与状态 API](#9-状态屏与状态-api)
- [10. 长时间空闲与自动恢复](#10-长时间空闲与自动恢复)
- [11. 故障排查](#11-故障排查)
- [12. 已知限制与安全事项](#12-已知限制与安全事项)

## 1. 使用前必须知道

### 1.1 这是定制固件

本项目针对 **ES3C28P + G9N9N + 原生 USB** 组合验证。其他 ESP32-S3 板、
其他屏幕引脚或其他蓝牙遥控器不保证直接可用。

### 1.2 必须使用数据线

电脑必须连接开发板的**原生 USB 数据口**。只供电 USB 线只能充电，不能出现
HID 键盘、UAC 麦克风和 CDC 串口。

### 1.3 遥控器只能连接一个主机

G9N9N 同一时间只能连接一个蓝牙主机。若它已经连到 Windows、电视或盒子，
ESP32 将无法抢占连接。首次使用前：

1. 打开 Windows“蓝牙和其他设备”。
2. 找到 `Chromecast Remote` 或类似名称。
3. 删除/忽略该设备。
4. 再让 ESP32 扫描并绑定。

### 1.4 UAC 麦克风是 USB 麦克风

语音通过 BLE ATVV 进入 ESP32，再由 ESP32 通过原生 USB UAC 送给电脑。
它不是无线电脑麦克风，USB 断开后电脑端麦克风也会消失。

## 2. 刷写与升级

### 2.1 先判断当前分区

本设备保留原厂小智 bootloader 和分区表。日常升级只写两个应用槽：

```text
0x0020000
0x0410000
```

不要写：

```text
0x0000000  bootloader
0x0008000  partition table
0x0009000  NVS / Wi-Fi / BLE 配置
```

错误地写 bootloader 或分区表可能导致设备无法启动。

### 2.2 先做完整备份

刷写前读取完整 16MB Flash：

```powershell
python -m esptool `
  --chip esp32s3 `
  --port COM4 `
  --baud 921600 `
  read-flash 0 0x1000000 backup-16mb.bin
```

确认：

- 文件大小必须是 `16777216` 字节。
- 记录 SHA256。
- 备份可能包含 Wi-Fi 密码和蓝牙绑定信息，不要上传公开仓库。

### 2.3 写入两个应用槽

先确认设备处于 ESP32-S3 ROM 下载模式，并且芯片是 ESP32-S3、Flash 为
16MB。示例：

```powershell
python -m esptool `
  --chip esp32s3 `
  --port COM4 `
  --baud 921600 `
  write-flash `
  --flash-mode dio `
  --flash-size 16MB `
  --flash-freq 80m `
  0x20000 firmware.bin `
  0x410000 firmware.bin
```

两个地址都必须出现：

```text
Hash of data verified.
```

### 2.4 Windows 只出现复合 CDC/COM5 时

设备运行应用时，原生 USB 可能先出现 `RemoteMapper` 复合设备和 CDC 串口，
例如 `COM5`。此时 esptool 可能无法直接抢占该复合 CDC。

可先让设备进入 ROM 下载模式，确认出现另一个 USB 串口（例如 `COM4`），
再执行备份和烧录。端口号以设备管理器为准，不要照抄示例端口。

### 2.5 网页 OTA

设备联网后可在网页“Wi-Fi 与系统配置”中使用 OTA 上传 `.bin`。

OTA 特点：

- 写入非当前运行的应用槽。
- 不主动覆盖 Wi-Fi、AP、按键映射等配置。
- 升级完成会自动重启，通常等待 20~40 秒。
- 升级中断电有失败风险；重要升级前仍建议保留 USB 完整备份。

## 3. 第一次开机

### 3.1 连接原生 USB

用数据线连接电脑和开发板原生 USB。Windows 正常应出现：

```text
HID Keyboard Device
符合 HID 标准的用户控制设备
RemoteMapper Wireless Microphone
USB 串行设备 (COMx)
```

其中 `RemoteMapper Wireless Microphone` 是后续给豆包使用的麦克风。

### 3.2 查看状态屏

LCD 会显示：

```text
遥控器 BLE 状态 / 电量
ATVV 麦克风状态 / 帧尺寸
音频帧 / 帧率 / 最近收帧时间
UAC 挂载 / 流状态 / 传输量
AGC / 缓冲 / 欠载
Wi-Fi / 运行时间
```

如果屏幕不亮，但网页和 USB 仍然可用，先检查 LCD 排线和背光引脚。

## 4. 配网与网页后台

### 4.1 首次使用 AP

设备默认开启：

```text
SSID: RemoteMapper-AP
默认密码: 空，开放热点
地址: http://192.168.4.1
```

连接该热点后打开地址即可进入后台。

### 4.2 连接家庭 Wi-Fi

网页路径：

```text
Wi-Fi 与系统配置 -> Wi-Fi 网络配置
```

步骤：

1. 点“搜索 Wi-Fi”或手动填写 SSID。
2. 输入密码。
3. 点“保存并连接 Wi-Fi”。
4. 设备取得 IP 后会关闭 `RemoteMapper-AP` 以降低发热和功耗。
5. 电脑切回同一家庭网络。
6. 使用 `http://remotemapper.local` 或状态屏显示的 IP 访问。

如果家庭 Wi-Fi 断开，设备会在约 15 秒后自动恢复 AP，便于重新配网。

### 4.3 AP 安全

网页可以给 AP 设置 8~63 位 WPA2 密码。建议在公共环境设置密码。

## 5. 配对 G9N9N

### 5.1 清除其他主机占用

先从 Windows、电视、机顶盒的蓝牙列表中删除
`Chromecast Remote` / `G9N9N`。

### 5.2 通过网页扫描

路径：

```text
蓝牙配对管理 -> 周围蓝牙设备扫描与手动绑定
```

步骤：

1. 按一下遥控器按键，让它从休眠中广播。
2. 点“扫描蓝牙设备”。
3. 找到 `Chromecast Remote` 或对应设备。
4. 点“连接并绑定”。
5. 等待网页显示连接成功。

遥控器休眠时不会持续广播；扫描不到时先按键唤醒再扫描。

### 5.3 自动重连

绑定成功后，ESP32 会保存专属绑定目标。遥控器重新广播时会自动连接。

如需换遥控器，先在网页“蓝牙配对管理”里清空绑定，再重新扫描。

## 6. 豆包语音输入

### 6.1 设置豆包麦克风

在豆包输入法的麦克风选择中选中：

```text
RemoteMapper Wireless Microphone
```

不要选电脑内置麦克风、手机麦克风或 VB-CABLE 等其他设备。

### 6.2 设置豆包快捷键

默认固件把遥控器语音键映射为：

```text
长按: 右 Ctrl + 右 Alt
松开: 释放右 Ctrl + 右 Alt
```

豆包需要配置成对应的长按说话快捷键。若你改了网页里的语音映射，
豆包快捷键也要同步修改。

### 6.3 使用方法

1. 光标放在需要输入文字的窗口。
2. 按住 G9N9N 语音键。
3. 说完后松开。
4. 等待豆包把文字写入窗口。

### 6.4 如何确认音频到电脑

打开：

```text
http://remotemapper.local/api/status
```

观察：

```text
frames_decoded  持续增长
uac_streaming   true
uac_mounted     true
voice_timeout_count 保持 0
```

如果 `frames_decoded` 增长但豆包没反应，问题通常在豆包麦克风选择或快捷键，
不在 BLE 音频链路。

## 7. 按键映射与组合键

### 7.1 默认层与 5 个层

网页支持：

```text
默认层
层 1
层 2
层 3
层 4
```

默认层是基础映射；其他层未单独设置的按键会穿透继承默认层。

### 7.2 三种触发方式

普通按键支持：

```text
单击
长按
双击
```

每个触发方式可以单独设置动作。

### 7.3 动作模式

网页当前提供：

- **键盘直通**：按下/松开与遥控器同步。
- **单次点按**：触发一次键盘按键。
- **多媒体控制**：音量、播放/暂停、静音等。
- **切换按键层**：用按键切换当前层。
- **高级扩展**：WOL 等已支持能力。

语音键有独立的“长按模式”和“切换模式”：

- **长按模式**：按住发送组合键，松开释放，默认用于豆包。
- **切换模式**：按一下开始，按一下结束，适配部分切换式输入法。

语音键不能改成普通多媒体键。

### 7.4 组合键录制

1. 打开某个按键的映射。
2. 在“自定义按键 / 组合键录制”框内点击。
3. 直接按下键盘组合键。
4. 浏览器拦截的系统快捷键，可用下方预设项或手动填写。

修饰键常见值：

```text
0x01  左 Ctrl
0x02  左 Shift
0x04  左 Alt
0x08  左 Win
0x10  右 Ctrl
0x20  右 Shift
0x40  右 Alt
0x80  右 Win
```

组合键的修饰键是位掩码。例如左 Ctrl + 左 Shift：

```text
0x01 + 0x02 = 0x03
```

### 7.5 按键码

网页提供字母、数字、功能键、方向键和多媒体键对照表。

常用示例：

```text
A-Z       0x04-0x1D
1-0       0x1E-0x27
Enter     0x28
Esc       0x29
Backspace 0x2A
Tab       0x2B
Space     0x2C
```

### 7.6 保存

改完后点击：

```text
保存映射
保存层级配置
```

映射写入 NVS，设备重启后保留。

## 8. 配置备份与恢复

网页支持两类备份。

### 8.1 按键配置备份

只包含：

```text
5 个层级的按键映射
触发方式
动作模式
```

不包含：

```text
Wi-Fi
AP 密码
蓝牙配对
底层系统参数
```

适合多设备复制键位。

### 8.2 系统配置备份

包含：

```text
Wi-Fi / AP
节能策略
超时
按键映射
系统配置
```

注意：

- 脱敏备份会隐藏 Wi-Fi/AP 密码，不能直接用于完整恢复。
- 完整备份包含明文密码，必须妥善保存。
- 蓝牙绑定使用加密身份，不能通过 JSON 备份恢复；换机后需要重新配对。

### 8.3 重置

网页提供：

```text
清空按键映射（保留蓝牙与 Wi-Fi）
清空全部 NVS（出厂完全重置）
```

“清空全部 NVS”会删除网络、蓝牙绑定和按键映射，并立即重启。执行前确认
已有可恢复备份。

## 9. 状态屏与状态 API

### 9.1 API 地址

```text
GET http://remotemapper.local/api/status
GET http://remotemapper.local/api/logs
```

### 9.2 关键字段

```text
version                    当前固件版本
reset_reason               最近一次复位原因
ble_state                  BLE 状态机
ble_link_connected         GATT 链路是否连接
ble_task_heartbeat_age_ms  BLE 任务心跳年龄
atvv_audio_ready           ATVV 音频特征是否可用
last_audio_age_ms          最近音频帧年龄
frames_decoded             已解码音频帧数
uac_mounted                USB UAC 是否挂载
uac_streaming              电脑是否正在取样
uac_transfers              UAC 传输次数
agc_gain                   当前 AGC
audio_underruns            音频欠载次数
idle_maintenance_done      本轮是否已执行维护重启
idle_activity_age_ms       距最近用户活动的毫秒数
idle_maintenance_due_ms    距下次空闲维护的毫秒数
voice_timeout_count        强制释放语音会话次数
atvv_refresh_count         ATVV 健康刷新次数
atvv_refresh_fail_count    连续健康刷新失败次数
ble_recovery_count         BLE 自动恢复次数
free_heap / free_psram     可用内存
```

### 9.3 正常值

```text
ble_task_heartbeat_age_ms  0~10
uac_mounted               true
voice_timeout_count       0
atvv_refresh_fail_count   0
```

`ble_task_heartbeat_age_ms` 大于 30000 时，主循环看门狗会准备重启。

## 10. 长时间空闲与自动恢复

### 10.1 为什么需要自动恢复

G9N9N 会休眠，ATVV 订阅和 USB UAC 也可能在长时间空闲后进入异常状态。
固件保留每分钟一次轻量 ATVV 健康刷新，并增加主循环看门狗。

### 10.2 自动维护规则

- BLE 任务每次循环更新心跳。
- 心跳超过 30 秒不更新：自动重启。
- 连续自动重启上限为 3 次。
- 成功用户活动会清零重启计数。
- 首次 15 分钟无用户活动：软重启一次。
- 后续维护重启间隔至少 60 分钟。
- 语音会话或 UAC streaming 中不执行空闲重启。

### 10.3 静置后自动重启是否正常

是。静置约 15 分钟后的自动重启是维护动作。重启后：

- BLE 会自动重连；
- USB UAC 会重新枚举；
- LCD 和网页恢复；
- 通常几秒后可以继续使用。

如果你正在连续使用，按键、语音开始/结束和音频帧会阻止空闲维护。

## 11. 故障排查

### 11.1 Windows 没有麦克风

检查：

```text
是否使用原生 USB 数据口
设备管理器是否出现 RemoteMapper Wireless Microphone
/api/status 的 uac_mounted
```

处理：

1. 换数据线。
2. 换 USB 口。
3. 重新插拔。
4. 卸载未知 USB 设备后扫描硬件改动。
5. 必要时网页“系统控制 -> 重启”。

### 11.2 扫描不到 G9N9N

检查：

```text
Windows 是否仍占用 Chromecast Remote
遥控器是否休眠
是否已绑定其他 ESP32
```

处理：

1. 删除 Windows 蓝牙配对。
2. 按遥控器按键唤醒。
3. 网页重新扫描。
4. 清空旧绑定后重试。

### 11.3 语音键没有反应

按顺序检查：

1. 豆包麦克风是否为 `RemoteMapper Wireless Microphone`。
2. 豆包快捷键是否为右 Ctrl + 右 Alt。
3. 网页语音键映射是否仍为长按右 Ctrl + 右 Alt。
4. `frames_decoded` 是否增长。
5. `uac_streaming` 是否为 true。
6. `ble_task_heartbeat_age_ms` 是否正常。
7. `voice_timeout_count` 是否快速增长。

### 11.4 有音频帧但豆包不上屏

这通常不是 BLE 问题：

- 豆包选错麦克风。
- 豆包快捷键不匹配。
- 当前窗口不接受输入。
- 豆包进程没有获得输入焦点。

### 11.5 网页打不开

尝试：

```text
http://remotemapper.local
http://192.168.4.1
状态屏显示的局域网 IP
```

如果 STA 连接失败，设备会自动恢复 AP。

### 11.6 LCD 不亮

检查：

```text
3.3V / GND
SCLK/MOSI/MISO/CS/DC
背光 GPIO45
LCD 型号是否 ILI9341
```

### 11.7 设备反复重启

查看：

```text
/api/status 的 reset_reason
/api/logs
USB 串口启动日志
```

- `reset_reason=3`：软件复位，可能是网页重启、OTA 或空闲维护。
- 其他值需要结合日志判断看门狗、掉电或 panic。
- 若持续出现 `Guru Meditation` 或 `panic`，不要继续反复烧录，先保存日志。

## 12. 已知限制与安全事项

- 当前是 ES3C28P + G9N9N 的定制适配，不是通用 ESP32 蓝牙遥控器项目。
- UAC 麦克风必须通过原生 USB 数据线使用。
- BLE 加密绑定不可迁移到另一块 ESP32。
- 完整 Flash 备份和完整配置备份可能包含 Wi-Fi 密码，禁止公开上传。
- 不要覆盖 bootloader、分区表和 NVS。
- 不要把不兼容项目的合并固件直接写到 `0x0`。
- 在确认上游许可证前，本仓库不额外授予再发布许可。

## 相关文档

- [README](README.md)
- [安全刷写](docs/SAFE_FLASHING.md)
- [状态屏](STATUS_DISPLAY.md)
- [空闲语音恢复](IDLE_VOICE_RECOVERY.md)
- [版本变更](CHANGELOG.md)
- [许可证状态](LICENSE_STATUS.md)
