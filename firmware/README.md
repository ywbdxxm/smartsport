# Smart Sport Firmware

智能运动背心的 ESP32-S3 固件工程。当前阶段保留最小启动程序，用于验证开发板、工具链和串口链路。

## 已验证硬件

- 开发板：芯路城 ESP32S3 开发板，板载模组为 ESP32-S3-WROOM-1-N16R8
- 目标：ESP32-S3
- Flash：16 MB Quad SPI
- PSRAM：8 MB Octal SPI，80 MHz
- 下载与日志串口：CH340，115200 波特率
- 板载 RGB：WS2812B，数据引脚为 GPIO48

开发板资料位于 `../doc/硬件资料/芯路城-ESP32S3-N16R8/`。其中原理图明确标出 WS2812B 的 DIN 经焊盘 J3 连接 GPIO48；厂商说明该焊盘出厂默认已短接。GPIO43/44 上的蓝色 LED 是 CH340 串口活动指示灯，不是用户可直接控制的状态灯。

## 开发环境

使用 ESP-IDF 6.1。请先激活与该版本配套的 ESP-IDF 环境，再执行以下命令：

```powershell
idf.py build
idf.py -p <PORT> flash monitor
```

`sdkconfig.defaults` 保存板级默认配置，生成的 `sdkconfig` 不提交到版本库。

## 当前程序

`main/hello_world_main.c` 输出芯片和内存信息，并让 GPIO48 上的板载 WS2812B 以 500 ms 亮、500 ms 灭的节奏闪烁。串口每完成一次闪烁周期输出一次计数。GNSS、IMU、BLE 心率、本地存储和蜂窝通信尚未实现。
