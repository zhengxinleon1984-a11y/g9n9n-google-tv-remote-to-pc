# G9N9N Google TV 遥控器直连电脑

把 **G9N9N / Chromecast Remote** 接到 **ES3C28P（ESP32-S3 N16R8）**，
通过原生 USB 变成一台带屏幕诊断、网页改键和蓝牙语音输入能力的硬件桥。

当前发行版：`0.1.15`

对应固件版本：`1.2.10-g9n9n-idle-activity-fix`

> 这是一个面向具体硬件组合的定制固件，不是通用遥控器固件。使用前请先阅读
> [硬件要求](#硬件要求) 和 [安全刷写](#安全刷写)。

![Status display](docs/status-display-preview.png)

## 它能做什么

```
G9N9N 按键
  -> BLE HOGP
  -> ESP32-S3
  -> USB HID 键盘 / 多媒体键
  -> Windows

G9N9N 语音键
  -> BLE ATVV
  -> ESP32-S3 IMA ADPCM 解码、滤波、AGC
  -> USB UAC 麦克风
  -> 豆包等支持长按说话的输入法
```

这解决了三个实际问题：

- G9N9N 原本主要作为电视/盒子蓝牙遥控器使用，不容易直接变成 Windows
  快捷键键盘。
- 遥控器语音只能通过 Android TV 的 ATVV 链路传输，电脑不能直接把它当麦克风。
- 普通无线桥接方案很难同时提供低延迟音频、按键宏、状态显示和网页配置。

## 核心功能

- G9N9N 普通按键索引 `1..17` 映射为 Windows 键盘、多媒体键或组合键。
- 5 层按键配置，支持单击、长按、双击和按下动作。
- 网页端可视化改键，可从按键码表选择，也可以直接录制组合键。
- 语音键固定使用 `0xFE`，与普通按键索引分离。
- 默认语音组合键为 **右 Ctrl + 右 Alt**，适配豆包语音输入。
- ATVV 音频解码后通过 USB UAC 麦克风输出，Windows 显示为
  `RemoteMapper Wireless Microphone`。
- 320x240 LCD 状态屏显示 BLE、ATVV、音频帧、UAC、AGC、内存和运行时间。
- Wi-Fi 网页后台支持按键映射、配置导入导出、BLE 配对、Wi-Fi 配置和 OTA。
- 空闲 ATVV 健康刷新、语音超时恢复、BLE 任务心跳和主循环维护看门狗。
- 按键配置、Wi-Fi 配置和系统配置可备份；BLE 加密绑定不进入配置备份。

## 硬件要求

| 项目 | 要求 |
|---|---|
| 开发板 | ES3C28P 或同引脚 ESP32-S3 板 |
| MCU | ESP32-S3 N16R8 |
| Flash | 16MB |
| PSRAM | 8MB Octal |
| 屏幕 | 320x240 ILI9341，SPI |
| 遥控器 | G9N9N，Windows 枚举名通常为 `Chromecast Remote` |
| USB | 必须使用开发板**原生 USB** 数据口，不支持只供电线 |

当前板级 LCD 引脚：

```text
SCLK GPIO12
MOSI GPIO11
MISO GPIO13
CS   GPIO10
DC   GPIO46
BL   GPIO45
```

音频使用 ES8311 / I2S，不复用上述 LCD SPI 引脚。

## Windows 会看到什么

刷入固件并用原生 USB 连接电脑后，Windows 通常会枚举：

- `HID Keyboard Device`
- `符合 HID 标准的用户控制设备`
- `RemoteMapper Wireless Microphone`
- `USB 串行设备 (COMx)`

语音输入时，在豆包输入法的麦克风设置中选择
`RemoteMapper Wireless Microphone`。

## 快速开始

1. **备份完整 16MB Flash**。详见 [docs/SAFE_FLASHING.md](docs/SAFE_FLASHING.md)。
2. 只向两个应用槽写入 `firmware.bin`：

   ```text
   0x0020000
   0x0410000
   ```

3. 用原生 USB 数据线连接电脑，确认 Windows 出现键盘、麦克风和串口。
4. 电脑连接 Wi-Fi `RemoteMapper-AP`，打开 `http://192.168.4.1`；
   或连接家庭 Wi-Fi 后打开 `http://remotemapper.local`。
5. 在网页“蓝牙配对管理”中唤醒遥控器并扫描，连接
   `G9N9N` / `Chromecast Remote`。
6. 在豆包输入法中把麦克风设为 `RemoteMapper Wireless Microphone`，
   保持语音快捷键为右 Ctrl + 右 Alt。
7. 按住遥控器语音键说话，松开后等待文字上屏。

完整步骤、网页说明和排障见 **[USAGE.md](USAGE.md)**。

## 网页后台

默认入口：

```text
http://remotemapper.local
```

首次配网：

```text
Wi-Fi: RemoteMapper-AP
URL:   http://192.168.4.1
```

默认 AP 为开放热点，可在网页里设置 8~63 位 WPA2 密码。

网页主要区域：

- **按键映射**：5 层配置、单击/长按/双击、按键码表和组合键录制。
- **蓝牙配对管理**：扫描、绑定、重连、解除绑定。
- **配置备份**：单独备份按键，或完整备份 Wi-Fi/AP/策略/按键。
- **Wi-Fi 与系统配置**：STA、AP、节能策略、OTA、系统重启和 NVS 管理。

### 按键映射怎么改

1. 进入网页“按键映射”。
2. 选择默认层或第 1~4 层。
3. 点击遥控器按键图标，选择单击、长按或双击。
4. 选择动作模式，例如键盘直通、单次点按、多媒体控制、切换按键层或 WOL。
5. 组合键可在录制框直接按下，也可手动填写修饰键和按键码。
6. 点击“保存映射”或“保存层级配置”。

按键码悬停/对照表按钮会显示字母、数字、功能键和多媒体键码。

## 语音输入

默认链路：

```text
按住 G9N9N 语音键
  -> 持续按住右 Ctrl + 右 Alt
  -> 豆包开始录音
松开语音键
  -> 释放右 Ctrl + 右 Alt
  -> 豆包结束录音并写入当前窗口
```

注意：

- 豆包要选择 `RemoteMapper Wireless Microphone`，不能选电脑内置麦克风。
- 语音快捷键必须是右 Ctrl + 右 Alt；若改过，需要和固件映射保持一致。
- 遥控器同一时间只能连接一个主机。若已连接 Windows 蓝牙，先删除 Windows
  中的 `Chromecast Remote`。
- 状态屏或 API 的 `uac_streaming=true` 表示电脑正在从 UAC 麦克风取样。

## 状态屏与诊断

LCD 每秒刷新以下信息：

- BLE 连接、遥控器名称和电量
- ATVV 麦克风状态、帧尺寸和最近收帧时间
- 音频帧数、帧率和缓冲
- UAC 挂载、流状态和传输次数
- AGC 增益和欠载计数
- 内存、Wi-Fi IP、运行时间

在线诊断：

```text
GET http://remotemapper.local/api/status
GET http://remotemapper.local/api/logs
```

关键稳定性字段：

```text
ble_task_heartbeat_age_ms
idle_maintenance_done
idle_activity_age_ms
idle_maintenance_due_ms
atvv_activity_age_ms
voice_timeout_count
atvv_refresh_count
atvv_refresh_fail_count
ble_recovery_count
uac_mounted
uac_streaming
```

正常值通常为：

```text
ble_task_heartbeat_age_ms = 0..10
voice_timeout_count = 0
atvv_refresh_fail_count = 0
uac_mounted = true
```

## 长时间空闲自动恢复

版本 `1.2.10` 修复了一个关键问题：`NimBLEScan::start(0, false)` 会选中
阻塞重载，导致 BLE 任务永久卡在扫描调用里。扫描回调还能运行，但 BLE
状态机、语音恢复和旧版空闲维护都不会执行。

现在的策略：

- 扫描使用非阻塞 `start(0, nullptr, false)`。
- BLE 任务每次循环更新心跳。
- 主循环检测到心跳 30 秒不更新会自动重启，最多连续尝试 3 次。
- 首次 15 分钟无用户活动会做一次维护重启，之后按 60 分钟冷却。
- 语音会话或 UAC streaming 中不做空闲重启。
- HOGP 按键、语音开始/结束、SYNC 和真实音频帧才算用户活动。
- CAPS 健康响应只刷新 ATVV 活性，不会阻止空闲维护。

因此，静置约 15 分钟后设备自动重启一次是**预期的自愈行为**，不是崩溃。
网页手动重启仍然可用，但正常情况下不再需要。

详见 [IDLE_VOICE_RECOVERY.md](IDLE_VOICE_RECOVERY.md)。

## 安全刷写

当前设备沿用原厂 bootloader 和分区表。日常升级只允许写两个应用槽：

```text
0x0020000  firmware.bin
0x0410000  firmware.bin
```

不要写：

```text
0x0000000  bootloader
0x0008000  partition table
0x0009000  NVS / Wi-Fi credentials
```

完整 16MB Flash 备份包含 Wi-Fi 和 BLE 凭据，不能公开上传。

参考：

- [docs/SAFE_FLASHING.md](docs/SAFE_FLASHING.md)
- [USAGE.md#刷写与升级](USAGE.md#2-刷写与升级)

## 编译与测试

```powershell
python -m venv .venv
& '.venv\Scripts\python.exe' -m pip install platformio
& '.venv\Scripts\pio.exe' run -e esp32s3_n16r8
```

原生测试：

```powershell
C:\Python314\python.exe test\native\test_suite.py
```

如果 Xtensa 链接器在中文路径下失败，请把项目放到纯 ASCII 路径，或使用
`C:\pio-build\RemoteMapper-G9N9N` 作为 PlatformIO 构建目录。

## 串口 CLI

用原生 USB CDC 或 UART 打开对应 COM 口，波特率 `115200`：

```text
status
reconnect
reset_keys
usb reconnect
wifi on
wifi off
wifi status
log on
log off
help
```

## 目录

```text
src/                固件源码
include/            公共头文件和字体
test/native/        PC 原生测试
pc_companion/       Windows 配套脚本
tools/              字体生成、状态屏预览、发布打包
docs/               安全刷写和硬件说明
USAGE.md            完整使用手册和故障排查
IDLE_VOICE_RECOVERY.md  空闲恢复设计
CHANGELOG.md        版本变更
```

## 常见问题

### 电脑找不到 `RemoteMapper Wireless Microphone`

- 确认使用原生 USB 数据线，不是只供电线。
- 换一根数据线或 USB 口。
- 检查设备管理器的“声音、视频和游戏控制器”和“端口”。
- 看 `/api/status` 的 `uac_mounted`。

### 网页扫描不到遥控器

- 先删除 Windows 蓝牙里的 `Chromecast Remote` 配对。
- 按遥控器按键唤醒，再点网页扫描。
- 遥控器休眠时不会持续广播。

### 豆包没有反应

- 豆包麦克风必须选 `RemoteMapper Wireless Microphone`。
- 豆包语音快捷键必须是右 Ctrl + 右 Alt，或与网页语音映射一致。
- 按住遥控器语音键，不要只点一下。
- 看 `frames_decoded` 和 `uac_streaming` 是否增长。

### 设备静置后自己重启了

这是 15 分钟空闲维护，属于预期行为。重启后 BLE 会自动重连，
UAC 麦克风会重新枚举，通常等待几秒即可使用。

### Wi-Fi 连不上

- 设备会自动重新开启 `RemoteMapper-AP`。
- 连接 AP 后访问 `http://192.168.4.1` 重新配网。
- 也可以用 USB CDC/UART CLI 查看 `status` 或执行 `wifi status`。

更多问题见 [USAGE.md#故障排查](USAGE.md#11-故障排查)。

## 文档导航

- [完整使用手册](USAGE.md)
- [安全刷写](docs/SAFE_FLASHING.md)
- [状态屏设计](STATUS_DISPLAY.md)
- [空闲语音恢复](IDLE_VOICE_RECOVERY.md)
- [版本变更](CHANGELOG.md)
- [底层 USB/BLE 问题复盘](BLE_HID_UAC_BUGS_POSTMORTEM.md)
- [USB UAC 问题复盘](USB_UAC_BUG_POSTMORTEM.md)

## 已知限制

- 本固件针对 ES3C28P + G9N9N 验证，其他板级引脚和遥控器协议不保证兼容。
- 原生 USB 依赖数据线；UAC 不是无线音频，不能脱离 USB 使用。
- 遥控器加密绑定不能通过 JSON 配置文件备份或恢复。
- 5 层映射和宏能力以当前网页/固件实现为准；未完成的能力不会在网页中伪装可用。
- 本项目不是 Google、Xiaomi、ByteDance/Doubao 的官方项目。

## 来源与许可证

本项目基于：

[cuicui-V5/RemoteMapper-ESP32](https://github.com/cuicui-V5/RemoteMapper-ESP32)

上游 README 声称 MIT，但截至整理时仓库没有实际 `LICENSE` 文件。
当前仓库为私有整理版，未额外授予许可证。详见
[LICENSE_STATUS.md](LICENSE_STATUS.md) 和
[UPSTREAM_NOTICE.md](UPSTREAM_NOTICE.md)。

在许可证确认前，不要将本仓库直接改成 Public，也不要删除上游来源说明。
