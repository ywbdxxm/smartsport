# Smart Sport Firmware

智能运动背心的 ESP32-S3 固件工程。当前阶段保留最小启动程序，用于验证开发板、工具链和串口链路。

## 已验证硬件

- 目标：ESP32-S3
- Flash：16 MB Quad SPI
- PSRAM：8 MB Octal SPI，80 MHz
- 下载与日志串口：CH340，115200 波特率

开发板完整模组丝印和板卡修订版本仍需在接入外设前核对。项目内硬件资料位于 `../doc/硬件资料/ESP32-S3-DevKitC-1/`。

## 开发环境

使用 ESP-IDF 6.1。请先激活与该版本配套的 ESP-IDF 环境，再执行以下命令：

```powershell
idf.py build
idf.py -p <PORT> flash monitor
```

`sdkconfig.defaults` 保存板级默认配置，生成的 `sdkconfig` 不提交到版本库。

## 当前程序

`main/hello_world_main.c` 输出芯片和内存信息，然后倒计时并主动重启。GNSS、IMU、BLE 心率、本地存储和蜂窝通信尚未实现。
