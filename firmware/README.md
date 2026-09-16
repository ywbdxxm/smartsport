# Smart Sport Firmware

智能运动背心的 ESP32-S3 固件工程。当前阶段已接入 JYTech ICM-45686，并加入 SD 卡挂载与 32 KiB 写入/同步/读回冒烟测试，为后续统一记录 IMU、定位和心率数据建立存储边界。

## 目标硬件配置

- 开发板：芯路城 ESP32S3 开发板，板载模组为 ESP32-S3-WROOM-1-N16R8
- 目标：ESP32-S3
- Flash：16 MB Quad SPI
- PSRAM：8 MB Octal SPI，80 MHz
- 下载与日志串口：CH340，115200 波特率
- 板载 RGB：WS2812B，数据引脚为 GPIO48
- IMU：JYTech ICM-45686 纯传感器模组，I²C 接口
- SD：Waveshare Micro SD Storage Board（SKU 3947），SPI2 接口，只能使用 3.3 V

开发板资料位于 `../doc/硬件资料/芯路城-ESP32S3-N16R8/`。其中原理图明确标出 WS2812B 的 DIN 经焊盘 J3 连接 GPIO48；厂商说明该焊盘出厂默认已短接。GPIO43/44 上的蓝色 LED 是 CH340 串口活动指示灯，不是用户可直接控制的状态灯。

## 开发环境

使用 ESP-IDF 6.1。请先激活与该版本配套的 ESP-IDF 环境，再执行以下命令：

```powershell
idf.py build
idf.py -p <PORT> flash monitor
```

`sdkconfig.defaults` 保存板级默认配置，生成的 `sdkconfig` 不提交到版本库。

## 当前程序

`main/hello_world_main.c` 输出芯片和内存信息，先执行一次非致命 SD 冒烟测试，再初始化 ICM-45686。IMU 每 100 ms 读取一组加速度、角速度和温度，成功样本日志节流为每 1 s 一条。IMU 正常时 GPIO48 上的板载 WS2812B 绿灯闪烁；初始化或读取失败时显示红灯并输出错误原因。

SD 失败不会阻止 IMU 启动。当前 SD 路径只做启动诊断，不会持续保存运动数据；统一会话记录任务和二进制记录格式见 `../doc/数据采集与存储架构.md`。

## SD 卡启动冒烟测试

SD 使用 SPI2：GPIO10 CS、GPIO11 MOSI、GPIO12 SCLK、GPIO13 MISO，GPIO14 只读取并打印卡检测原始电平。卡检测的插卡有效电平需要实板记录后再启用，当前不会用它阻止挂载。

每次启动只尝试一次：

1. 以 10 MHz 挂载 FAT 文件系统到 `/sdcard`，并打印卡名称、类型、容量和实际时钟。
2. 写入 `/sdcard/smartsport_sd_test.bin` 共 32 KiB，执行 `fflush()` 和 `fsync()`。
3. 关闭后重新打开，逐字节校验并确认文件长度正好为 32 KiB。
4. 删除且只删除该诊断文件，然后卸载文件系统并释放 SPI2。

挂载配置固定为 `format_if_mount_failed = false`，固件不会自动格式化卡。首次测试请使用已备份且已格式化为 FAT 的测试卡。板子重新接到 COM3 后可执行：

```powershell
idf.py -p COM3 flash monitor
```

成功日志包含 `SD 32 KiB write/sync/readback test passed` 和 `SD startup smoke test completed successfully`。失败日志会说明挂载、文件 I/O、校验或清理阶段，随后仍会进入 `ICM-45686 I2C` 初始化。

## ICM-45686 接线

首版仍使用 I²C 轮询；`INT1` 接到 GPIO7 为后续数据就绪中断预留，当前固件尚未启用。`INT2`、`ASD` 和 `ASL` 暂不连接。

| ICM-45686 模组 | 芯路城 ESP32-S3 | 说明 |
| --- | --- | --- |
| `VIN` | 底板红色 `5V` | 模组允许 3.3–12 V 输入，厂商推荐 5 V；不要把信号线接到 5 V 排 |
| `GND` | `GND` | 两块板必须共地 |
| `SDA/MOSI` | `GPIO8` | I²C SDA |
| `SCL/SCLK` | `GPIO9` | I²C SCL |
| `INT1` | `GPIO7` | 当前仅接线预留，固件仍轮询 |

模组原理图上 `CS` 和 `AD0/MISO` 默认各由 10 kΩ 电阻上拉，因此使用 I²C 时可以不接：默认地址是 `0x69`。程序也会在 `0x69` 未响应时尝试 `0x68`。SDA 和 SCL 已各有 4.7 kΩ 上拉。不要把该模组当作 UART 姿态模块使用。

传感器配置为加速度计 ±4 g、陀螺仪 ±1000 dps、两者均为 200 Hz 低噪声模式，低通带宽为 ODR/4。当前应用每 100 ms 轮询一次，串口成功样本日志每 1 s 输出一次；FIFO、中断采集和姿态融合将在后续数据链路阶段实现。

模组资料位于 `../doc/imu/ICM45686-Module/`。驱动组件说明见 `components/icm45686/README.md`。

IMU、SD 卡和 GNSS 的完整 GPIO 分配、电源限制与接线顺序见 `../doc/模块接线最终规划.md`。
