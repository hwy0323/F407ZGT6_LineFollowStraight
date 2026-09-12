# F407ZGT6 直线循迹

这是一个只做黑线直线循迹的 STM32F407ZGT6 工程。程序不使用 PID、编码器、视觉、转弯任务或复杂状态机。

## 接线

| 模块信号 | F407 引脚 |
|---|---|
| 灰度模块 D0～D7 | PE0～PE7 |
| TB6612 PWMA、PWMB | PB0、PB1 |
| TB6612 AIN1、AIN2 | PD0、PD1 |
| TB6612 BIN1、BIN2 | PD2、PD3 |
| TB6612 STBY | PD4 |
| 板载 LED，低电平点亮 | PC13 |

保留 PA13/PA14 供 ST-LINK 的 SWD 使用，PH0/PH1 供外部 8 MHz 晶振使用。灰度模块到 F407 的输出必须是 3.3 V 逻辑；不要直接将 5 V 信号接入 GPIO。

## 控制规则

- PE3 或 PE4 识别到黑线：两侧电机同速前进。
- PE0～PE2 识别到黑线：左轮减速，向左轻微修正。
- PE5～PE7 识别到黑线：右轮减速，向右轻微修正。
- 八个传感器都未识别黑线：立即停机并拉低 TB6612 的 STBY。

默认按灰度模块“检测到黑线输出低电平”处理。若你的模块输出逻辑相反，将 `SENSOR_ACTIVE_LOW` 改为 `0`。

## 编译

在工程目录执行 `make`，产物在 `Debug/F407ZGT6_LineFollowStraight.elf` 和 `Debug/F407ZGT6_LineFollowStraight.bin`。本次只编译，不烧录。
