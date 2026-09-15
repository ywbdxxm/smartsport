# Smart Sport Firmware

智能运动背心的 ESP32-S3 固件工程。当前阶段在开发板基础程序上接入 JYTech ICM-45686，用于完成首版 I²C 数据采集。

## 目标硬件配置

- 开发板：芯路城 ESP32S3 开发板，板载模组为 ESP32-S3-WROOM-1-N16R8
- 目标：ESP32-S3
- Flash：16 MB Quad SPI
- PSRAM：8 MB Octal SPI，80 MHz
- 下载与日志串口：CH340，115200 波特率
- 板载 RGB：WS2812B，数据引脚为 GPIO48
- IMU：JYTech ICM-45686 纯传感器模组，I²C 接口

开发板资料位于 `../doc/硬件资料/芯路城-ESP32S3-N16R8/`。其中原理图明确标出 WS2812B 的 DIN 经焊盘 J3 连接 GPIO48；厂商说明该焊盘出厂默认已短接。GPIO43/44 上的蓝色 LED 是 CH340 串口活动指示灯，不是用户可直接控制的状态灯。

## 开发环境

使用 ESP-IDF 6.1。请先激活与该版本配套的 ESP-IDF 环境，再执行以下命令：

```powershell
idf.py build
idf.py -p <PORT> flash monitor
```

`sdkconfig.defaults` 保存板级默认配置，生成的 `sdkconfig` 不提交到版本库。

## 当前程序

`main/hello_world_main.c` 输出芯片和内存信息，初始化 ICM-45686，并每 100 ms 输出一组加速度、角速度和温度。IMU 正常时 GPIO48 上的板载 WS2812B 绿灯闪烁；初始化或读取失败时显示红灯并输出错误原因。

当前只完成了主机换算测试和 ESP-IDF 离线编译。IMU 接线、地址探测和数据读取仍需连接目标开发板后验证。

## ICM-45686 接线

首版使用 I²C 轮询，INT1、INT2、ASD 和 ASL 暂不连接。

| ICM-45686 模组 | 芯路城 ESP32-S3 | 说明 |
| --- | --- | --- |
| `VIN` | `3V3` | 模组允许 3.3–12 V 输入；使用 3V3 不依赖开发板 5V 输出焊盘 |
| `GND` | `GND` | 两块板必须共地 |
| `SDA/MOSI` | `GPIO8` | I²C SDA |
| `SCL/SCLK` | `GPIO9` | I²C SCL |

模组原理图上 `CS` 和 `AD0/MISO` 默认各由 10 kΩ 电阻上拉，因此使用 I²C 时可以不接：默认地址是 `0x69`。程序也会在 `0x69` 未响应时尝试 `0x68`。SDA 和 SCL 已各有 4.7 kΩ 上拉。不要把该模组当作 UART 姿态模块使用。

传感器配置与配套示例一致：加速度计 ±4 g、陀螺仪 ±1000 dps、两者均为 200 Hz 低噪声模式，低通带宽为 ODR/4。当前串口只以 10 Hz 显示寄存器数据；FIFO、中断采集和姿态融合将在后续数据链路阶段实现。

模组资料位于 `../doc/imu/ICM45686-Module/`。驱动组件说明见 `components/icm45686/README.md`。
