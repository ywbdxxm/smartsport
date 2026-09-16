# Smart Sport

智能运动背心研发仓库，包含产品与硬件资料以及 ESP32-S3 固件工程。

## 目录

- `doc/`：产品方案、器件选型和项目使用的硬件资料。
- `firmware/`：基于 ESP-IDF 6.1 的 ESP32-S3 固件。

`doc/imu/ICM45686-Module/` 作为 Git 子模块管理。首次克隆后执行：

```powershell
git submodule update --init --recursive
```

## 当前固件基线

- 开发板：芯路城 ESP32S3 开发板（ESP32-S3-WROOM-1-N16R8）
- 目标芯片：ESP32-S3
- Flash：16 MB Quad SPI
- PSRAM：8 MB Octal SPI，80 MHz
- 当前程序：GPIO48 板载 WS2812B 与 JYTech ICM-45686 I²C 数据读取（待目标板实测）

具体构建和烧录方法见 `firmware/README.md`。

IMU、SD 卡和 GNSS 预留的统一接线基线见 `doc/模块接线最终规划.md`。
