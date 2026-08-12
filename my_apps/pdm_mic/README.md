# INMP441 I2S 麦克风调试记录

## 问题

INMP441 麦克风连接 BL702 I2S RX 接口后无数据。

## 硬件接线

```
BL702                  INMP441
GPIO0  (I2S_BCLK) ───► SCK
GPIO1  (I2S_FS)   ───► WS
GPIO23 (I2S_DI)   ◄─── SD
3.3V               ───► VDD
GND                ───► GND, L/R (左通道)
```

验证手段：
- GPIO7/8 必须配置为 USB 功能（`GPIO_FUN_USB`），否则 CDC ACM 无输出
- pinmux 已确认 GPIO0/1/23 = function 3 (I2S) ✅
- GPIO17 LED 心跳可证明固件在运行

## 诊断证据

v2 诊断输出关键发现：

```
rx_fifo_cnt = 16    ← INMP441 确实在发送数据！
overflow    = 1     ← FIFO 溢出（数据到达但未被 DMA 取走）
fer         = 1     ← I2S 帧错误
DMA buf     = 全零  ← DMA 没有把 FIFO 数据搬到内存
```

**结论**：INMP441 在输出数据，数据进入了 BL702 I2S RX FIFO（累计到 16），
但 DMA 没有触发传输，FIFO 溢出。帧错误 (fer=1) 可能是根本原因。

## 诊断结果 (v5)

v5 用 12.288 MHz PLL（可精确产生 1.024 MHz = 64×16kHz），
4 组试验（STD/LEFT × stereo/mono），全部结果：

```
fifo_cnt=0  fer=0  ovf=0  ← I2S 时钟正确，但 INMP441 无数据输出
dma=0  max=0  (所有组)
```

### 关键对比：v2 vs v5

| 指标 | v2 (24 MHz PLL) | v5 (12.288 MHz PLL) |
|------|-----------------|----------------------|
| fer (帧错误) | **1** | **0** ✅ |
| rx_fifo_cnt | 16 (glitch) | 0 |
| overflow | 1 | 0 |
| DMA data | 全零 | 全零 |

**解释**：
- v2 的 `fer=1, fifo_cnt=16` 是 **错误时钟导致的 glitch 数据**——INMP441 收到不匹配的 BCLK/WS 比率后进入异常状态，SD 线上有非有效数据
- v5 时钟正确（fer=0），INMP441 **完全不输出数据** —— 这是真正的问题

### 确定的根因

**INMP441 SD 线上无有效数据**。所有配置组合（PLL × 格式 × 通道）均无法让 INMP441 输出。

最可能的原因（按概率排序）：

1. **INMP441 模块硬件故障或焊接问题** —— 换一块模块验证
2. **BL702 I2S BCLK/WS 波形本身有问题** —— 需要示波器确认 GPIO0/1 上的实际频率和电平
3. **INMP441 与 BL702 之间的电平不匹配** —— INMP441 是 3.3V，但某些模块可能带 LDO

## 下一步

1. **用示波器/逻辑分析仪确认 GPIO0/1 上的 BCLK/WS 波形**
   - BCLK 频率应 ≈ 1.024 MHz
   - WS 频率应 = 16 kHz
   - WS 和 BCLK 应完全同步

2. **确认 GPIO23 (SD) 有数据输出**
   - 排除 INMP441 硬件故障
   - 排除焊接问题

3. **如果波形正确但仍无数据，尝试不同的 I2S 配置组合**：
   - frame_size: 16/24/32
   - data_size: 16/24
   - mode: STD/LEFT
   - BCLK invert / FS invert

4. **对比测试**：如果有多余的 INMP441 模块，换一块排除硬件故障

## 备选方案

如果 I2S 方案始终无法工作：

- **SPI ADC 麦克风**（如 MAX9814 模拟输出 + BL702 ADC）
- **UART 音频模块**（如 BY8001、DFPlayer）
- **I2S 音频 ADC 芯片**（如 ES8388 Codec，SDK 已有完整驱动）

## 文件

| 文件 | 说明 |
|------|------|
| `main.c` | I2S 麦克风诊断固件（v5） |
| `config/pinmux_config.h` | GPIO 配置 |
| `CMakeLists.txt` | 构建配置 |

## 构建

```bash
./build.sh my_apps/pdm_mic
```

产物：`uf2_demos/pdm_mic.uf2`
