# SportWatch

ESP32-S3 + MPU6050 + SSD1306 智能运动手表。GIX TECHIN 515A 个人项目。

## 功能

- **活动识别**：跑步 / 走路 / 其他三分类，显示当日累计时长（基于 MPU6050 六轴 + TinyML）
- **表盘**：当前时间 + 温度，两个界面每 5 秒自动轮播
- **时间同步**：无外置 RTC —— 开机走 NTP，断网兜底，每日凌晨自动重连校时
- **无按键设计**：靠界面自动轮播 + WiFi 自动配网

## 硬件

| 模块 | 接线 | I²C 地址 |
|------|------|---------|
| ESP32-S3 | — | — |
| MPU6050 | SDA=GPIO8, SCL=GPIO9, 3.3V, GND | 0x68 |
| SSD1306 OLED (128×64) | 共享 I²C 总线 | 0x3C |

两个 I²C 设备共用一组引脚，地址不冲突。

## 仓库结构

```
SportWatch/
├── SportWatch/          ← 主固件（运动手表）
│   ├── SportWatch.ino
│   ├── config.h         常量配置（WiFi、时区、温度偏置）
│   ├── sensors.*        MPU6050 封装
│   ├── display.*        SSD1306 双界面渲染
│   ├── timekeeper.*     NTP + 内部 RTC + 编译时间兜底
│   ├── storage.*        NVS 持久化（活动统计、上次时间）
│   └── activity.*       活动识别（占位阈值，Phase B 替换为 Edge Impulse 模型）
├── Collector/           ← 数据采集固件（Phase B 用）
│   └── Collector.ino    WiFi AP + 网页控制，LittleFS 存 CSV，跑完下载
└── README.md
```

主固件和采集固件是**两个独立 Arduino sketch**，按需切换烧录。

## 时间同步方案（重点）

由于没有外置 RTC 模块，采用多层兜底：

1. **开机 NTP**（主方案）：`WiFiManager` 自动配网 → `configTime()` 走 `pool.ntp.org` 等 → 写入 ESP32 内部 RTC → 立即 `WiFi.mode(WIFI_OFF)` 断开省电
2. **编译时间**（兜底）：NTP 失败用 `__DATE__ __TIME__` 作为初值，至少不是 1970
3. **每日重连**（抗漂移）：凌晨 3:00 主动短连 WiFi 重新校时（ESP32 内部 RTC 漂移 ~10-50ppm，每天误差 < 1 分钟）
4. **断电恢复**：每分钟把当前 epoch 写 NVS，重启时可作为「上次已知时间」过渡到 NTP 完成

首次烧录后，手机连接热点 `SportWatch-Setup`（密码 `12345678`）配一次 WiFi 凭据，之后存在 NVS 不需要再配。

实现见 [SportWatch/timekeeper.cpp](SportWatch/timekeeper.cpp)。

## 依赖库（Arduino Library Manager 安装）

- `Adafruit MPU6050` + `Adafruit Unified Sensor`
- `Adafruit SSD1306` + `Adafruit GFX Library`
- `WiFiManager`（tzapu 版本）
- `Preferences`（ESP32 自带）
- LittleFS、WebServer（ESP32 自带，仅 Collector 用）
- 后续：Edge Impulse 导出的 Arduino 库（Phase B）

## 烧录配置

Arduino IDE：

- **Board**：`ESP32S3 Dev Module`（或具体的开发板型号）
- **Partition Scheme**：`Default 4MB with spiffs` 或更大（采集固件需要 LittleFS）
- **USB CDC On Boot**：Enabled（用串口调试）
- **Upload Speed**：921600

## 配置项

烧录前修改 [SportWatch/config.h](SportWatch/config.h)：

```c
#define TIMEZONE_POSIX     "CST-8"          // 国内改这个；美西 PST8PDT,M3.2.0,M11.1.0
#define MPU_TEMP_OFFSET_C  8.0f             // 实测后调整
#define DAILY_RESYNC_HOUR  3                // 每日重新对时的小时
```

## 实施阶段

- [x] **Phase A 骨架**：SSD1306 显示、MPU6050 读取、NTP 同步、界面自动轮播、NVS 统计持久化
- [ ] **Phase B 活动识别模型**：用 `Collector/` 采集数据 → Edge Impulse 训练 → 导出 Arduino 库 → 替换 [SportWatch/activity.cpp](SportWatch/activity.cpp) 中的阈值占位
- [ ] **Phase C 细节打磨**：UI 美化、断电恢复验证、长时漂移实测
- [ ] **Phase D 时间方案完善**：每日自动重连 NTP 实测

## Phase B：数据采集流程

烧录 [Collector/Collector.ino](Collector/Collector.ino) 到手表，然后：

1. 手机 WiFi 连接 `SportWatch-Collect`（密码 `12345678`）
2. 浏览器打开 `http://192.168.4.1`
3. 点对应活动的按钮（跑步 / 走路 / 其他），手机塞兜里去做该活动
4. 15 分钟后自动停止（或回来点 Stop 提前停）
5. 每个活动重复 1-2 次，建议总时长：
   - 跑步 ≥ 10 分钟
   - 走路 ≥ 10 分钟
   - 其他（静坐、打字、洗碗、爬楼…）≥ 15 分钟且场景多样
6. 重新连热点，网页下方下载所有 CSV 文件
7. 上传到 Edge Impulse Studio → Data acquisition → Upload data → CSV wizard
8. 训练完导出 Arduino Library → 引入主固件工程 → 在 [SportWatch/activity.cpp](SportWatch/activity.cpp) 中替换 `classify_threshold()` 为 `run_classifier()`

CSV 格式（已兼容 Edge Impulse）：

```
timestamp,accX,accY,accZ,gyrX,gyrY,gyrZ
0,0.12,9.81,0.05,0.01,0.02,0.00
20,...
```

## 验证

| 模块 | 验证方法 |
|------|---------|
| MPU6050 | 串口绘图（Serial Plotter）看 6 轴波形，挥手时有响应 |
| SSD1306 | 字符不闪烁，秒钟稳定刷新 |
| NTP | 显示与手机时间一致 |
| 内部 RTC 漂移 | 连续运行 24h，比对误差应 < 1 分钟 |
| 活动识别 | Edge Impulse Live Classification 准确率 > 90% |
| 时长累计 | 跑步 5 分钟后统计接近 5 分钟（±10%）|
| 断电恢复 | 拔插电后统计不丢、时间自动重新对齐 |

## License

见 [LICENSE](LICENSE)。
