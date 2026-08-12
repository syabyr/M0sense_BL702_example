# BL702 驱动 PDM 麦克风可行性调研

## 结论

**BL702 无独立 PDM 外设**（2026-08-12 硬件探测确认），但可通过内置 **I2S RX** 外设 + 外部 PDM→I2S 桥接芯片实现，
或直接使用内置 I2S 接口的数字 MEMS 麦克风。

`pdm_reg.h` 存在于 SDK 中，但 I2S (0x4000AA00) 和 CAM (0x4000AD00) 之间的 768 字节地址间隙均无硬件响应——PDM 寄存器定义
可能是为 BL704/BL706 或其他系列芯片准备的，BL702 硅片上未实现。

**推荐方案：PDM Mic → PDM→I2S Bridge (IA611) → BL702 I2S RX → DMA → 内存**

---

## 1. 硬件探测结果 (2026-08-12)

使用 `my_apps/pdm_probe` 固件对 I2S (0x4000AA00) 到 CAM (0x4000AD00) 之间的地址间隙进行了读写验证：

```
  I2S  (0x4000AA00)    rst=0x00005001  wr=0x00005001  [STICK]   ← 外设存在
  Gap  (0x4000AB00)    rst=0x00000000  wr=0x00000000  [IGNORE]  ← 空
  Gap  (0x4000AC00)    rst=0x00000004  wr=0x00000004  [IGNORE]  ← 空(ghost read)
  CAM  (0x4000AD00)    rst=0x4000083C  wr=0x4000083D  [STICK]   ← 外设存在
```

| 结论 | 说明 |
|---|---|
| ❌ 无独立 PDM 硬件 | 0xAB00/0xAC00 写操作不生效 |
| ❌ 软件 CIC 不可行 | CPU 不能承受 3 MHz GPIO 中断 |
| ✅ I2S 外设正常 | 写确认，DMA 通道就绪 |
| ✅ CAM 外设正常 | 写确认，对照组 |

## 2. 推荐方案：PDM→I2S 桥接芯片

### 2.1 信号通路

```
PDM Mic ──► PDM→I2S Bridge ──► BL702 I2S RX (0x4000AA00) ──► DMA CH3 ──► 内存
  CLK/DATA    (IA611等)           BCLK/FS/DI (GPIO0/1/23)      (已验证)
```

### 2.2 推荐的桥接芯片

| 芯片 | 通道数 | 接口 | 参考价格 |
|---|---|---|---|
| Knowles IA611 | 1-ch | I2S (直接输出) | ~$1.5 |
| Vesper VM3011 | 1-ch | I2S (PDM→I2S 内建) | ~$2 |
| TI TLV320ADCx140 | 4-ch | I2S/TDM | ~$5 |

### 2.3 或直接使用 I2S 数字麦克风

部分 MEMS 麦克风直接输出 I2S 格式，无需桥接转换：

| 芯片 | 输出 | 备注 |
|---|---|---|
| Knowles SPH0645LM4H | I2S 24-bit | 单声道 |
| InvenSense ICS-43432 | I2S 24-bit | 低噪声 |
| Vesper VM3000 | I2S/PCM | 超低功耗 |

如果用 I2S 数字麦克风，整个方案简化为：
```
I2S Mic ──► BL702 I2S RX ──► DMA ──► 内存
              (即插即用)
```

---

## 3. 示例工程：`my_apps/pdm_mic`

### 3.1 引脚配置

| GPIO | 功能 | I2S 子功能 | 方向 |
|---|---|---|---|
| 0 | I2S_BCLK | I2S0_BCLK | 输出 (master clock) |
| 1 | I2S_FS | I2S0_FS | 输出 (word select) |
| 23 | I2S_DI | I2S0_RCLK_O_I2S0_DI | 输入 (data) |

`config/pinmux_config.h` 覆盖了 bl702_dapplus 的默认配置，仅改以上三个 GPIO。

### 3.2 软件架构

```
main.c
  ├─ usb_stdio_init()          USB CDC ACM 输出
  ├─ i2s_register()            I2S master, 16kHz, mono, 16-bit
  ├─ dma_register(CH3)         DMA RX: I2S RDR → memory
  ├─ device_read()             启动首帧 DMA
  └─ dma_rx_cb()              中断回调：切换双缓冲 + 标记就绪
       └─ compute_peak()       计算峰值 / 削波检测
       └─ printf() via USB     每秒输出音频电平和削波警告
```

### 3.3 DMA 双缓冲

```
┌─────────┐     ┌─────────┐
│ buf[0]  │←────│ buf[1]  │←──── I2S RDR (DMA RX)
│ 处理中   │     │ 收集中   │
└────┬────┘     └────┬────┘
     │               │
     └── callback ───┘ 两帧交替，无缝。
```

### 3.4 编译

```bash
./build.sh my_apps/pdm_mic
```

产物：`uf2_demos/pdm_mic.uf2` (280 blocks, 70KB)。

### 3.5 运行输出

烧录后打开 USB CDC ACM 串口，固件每秒打印一行：

```
=== I2S PDM Mic Example ===

I2S0 opened (master, mono, 16kHz, 16-bit)
DMA CH3 opened (I2S RX -> memory)

Capturing... (printing audio level every second)
Connect PDM→I2S bridge to GPIO0/1/23
=====================================

[  1] peak= 2100 (-23.9 dBFS)  blocks=32
[  2] peak= 3400 (-19.7 dBFS)  blocks=32
[  3] peak=    0 (-90.3 dBFS)  blocks=31       ← 静音
[  4] peak=32760 ( -0.0 dBFS)  blocks=32 CLIP! ← 削波！
```

- `peak` — 秒内最大采样绝对值 (0–32767)
- `dBFS` — 相对满刻度的分贝值
- `CLIP!` — 检测到削波（信号超过 32760）

---

## 4. 参考资料

已废弃（BL702 无 PDM 外设，仅作 SDK 分析参考）：

| 文件 | 内容 |
|---|---|
| `regs/pdm_reg.h` | PDM 寄存器定义（**硅片上未实现**） |
| `glb_reg.h` `PDM_CLK_CTRL` | PDM 时钟分频器（驱动存在但无外设） |
| `glb_reg.h` `reg_i2s_clk_sel` | I2S BCLK 时钟源选择位 |

生效资料：

| 文件 | 内容 |
|---|---|
| `hal_drv/inc/hal_i2s.h` | I2S HAL 层 API（`i2s_device_t` 结构） |
| `hal_drv/src/hal_i2s.c` | I2S HAL 实现（open/close/read/write/control） |
| `std_drv/src/bl702_i2s.c` | I2S 底层寄存器驱动 |
| `std_drv/inc/bl702_i2s.h` | I2S 配置结构体 `I2S_CFG_Type` |
| `hal_drv/inc/hal_dma.h` | DMA 配置 (`DMA_REQUEST_I2S_RX=20`) |
| `examples/i2s/i2s_play_from_record/` | I2S 环回示例（SDK 官方） |
| `my_apps/pdm_probe/` | 硬件探针固件（本仓库） |
| `my_apps/pdm_mic/` | **I2S PDM 例程（本仓库）** |

关键寄存器结构：

```c
struct pdm_reg {
    union {
        struct {
            uint32_t pdm_en      : 1;  // [0]    PDM 使能
            uint32_t reserved_1  : 1;  // [1]
            uint32_t rx_sel_128fs: 1;  // [2]    RX 128fs 时钟选择
            uint32_t tx_sel_128fs: 1;  // [3]    TX 128fs 时钟选择
            uint32_t dc_mul      : 8;  // [11:4] 抽取倍数 (默认 0x64=100)
            uint32_t scale_sel   : 3;  // [14:12] 缩放选择 (默认 0x5)
            uint32_t dither_sel  : 2;  // [17:16] 抖动选择 (默认 0x1)
            uint32_t force_lr    : 1;  // [20]    强制左右通道
            uint32_t force_sel   : 1;  // [21]    强制选择
            uint32_t dsd_swap    : 1;  // [22]    DSD 交换
        } BF;
        uint32_t WORD;
    } pdm_datapath_config;           // 0x00

    union {
        struct {
            uint32_t pdm_dma_rx_en : 1;  // [0]     DMA RX 使能
            uint32_t rx_format     : 3;  // [6:4]   RX 数据格式
            uint32_t pdm_dma_tx_en : 1;  // [8]     DMA TX 使能
            uint32_t tx_format     : 3;  // [14:12] TX 数据格式
            uint32_t tx_data_shift : 5;  // [20:16] TX 数据移位
        } BF;
        uint32_t WORD;
    } pdm_dma_config;                // 0x04

    union {
        struct {
            uint32_t pdm_dma_rdata : 32; // [31:0] DMA 读出数据 (PCM)
        } BF;
        uint32_t WORD;
    } pdm_dma_rdata;                 // 0x14

    union {
        struct {
            uint32_t rx_fifo_empty : 1;   // [0]    RX FIFO 空
            uint32_t rx_fifo_full  : 1;   // [1]    RX FIFO 满
            uint32_t rx_cs         : 2;   // [3:2]  RX FIFO 计数
            uint32_t RxFifoRdPtr   : 2;   // [5:4]  RX FIFO 读指针
            uint32_t RxFifoWrPtr   : 3;   // [10:8] RX FIFO 写指针
        } BF;
        uint32_t WORD;
    } pdm_rx_fifo_status;            // 0x1C
};
```

### 1.2 硬件特性

| 特性 | 说明 |
|---|---|
| **PDM→PCM 硬件转换** | 内置 CIC 抽取滤波器，不是纯软件 bit-bang |
| **抽取倍数可配** | `dc_mul` 8-bit（默认 100），控制降采样率 |
| **立体声双通道** | TX/TX2 分离 FIFO，`force_lr` 强制左右 |
| **硬件抖动** | `dither_sel` 2-bit 噪声整形 |
| **RX FIFO** | 8 级深度的硬件 FIFO |
| **独立 DMA** | `pdm_dma_rx_en`，不占用主 DMA 外设请求 |

### 1.3 时钟系统

**Audio PLL**（`bl702_pds.h`）提供标准音频频率：

| PLL 频率 | 典型用途 |
|---|---|
| `AUDIO_PLL_12288000_HZ` | 12.288 MHz = 48 kHz × 256 |
| `AUDIO_PLL_11289600_HZ` | 11.2896 MHz = 44.1 kHz × 256 |
| `AUDIO_PLL_5644800_HZ`  | 5.6448 MHz = 44.1 kHz × 128 |
| `AUDIO_PLL_24576000_HZ` | 24.576 MHz = 48 kHz × 512 |
| `AUDIO_PLL_24000000_HZ` | 24.000 MHz |

**PDM 独立时钟分频器**（`glb_reg.h` offset 0x84）：

```c
// GLB_PDM_CLK_CTRL
BL_Err_Type GLB_Set_PDM_CLK(uint8_t enable, uint8_t div);
// pdm0_clk_div: 6-bit 分频 (0-63)
// pdm0_clk_en:  1-bit 时钟使能
```

**PDM ↔ I2S 时钟桥接**（`glb_reg.h` `clk_cfg1` offset 0x04）：

```c
// clk_cfg1 bit 12: reg_i2s_clk_sel
// 0 = I2S BCLK 使用 Audio PLL (正常 I2S 模式)
// 1 = I2S BCLK 使用 PDM 时钟源   (PDM 模式)
```

完整时钟路径：

```
Audio PLL → GLB PDM Clock Divider → PDM_CLK ──┬──→ BCLK pad (GPIO, func_sel=3)
                     (GLB 0x84)                │      ↑
                                               └── reg_i2s_clk_sel=1 ──→ I2S BCLK
                                               (clk_cfg1 bit12)
```

---

## 2. PDM 与 I2S 的关系

### 2.1 不是同一个外设，但共享物理引脚

PDM 和 I2S 是 **两个独立的硬件外设**（各有自己的寄存器文件 `pdm_reg.h` vs `i2s_reg.h`），
但 PDM 通过 **复用 I2S 的 GPIO 引脚** 来接入物理信号。

### 2.2 关键桥接：`GLB_REG_I2S_CLK_SEL`

`clk_cfg1` bit 12 是 PDM ↔ I2S 的桥接开关。整个数据流为：

```
                        GLB CLK MUX
        Audio PLL ──┐
                     ├── reg_i2s_clk_sel ──► I2S BCLK domain
        PDM CLK  ───┘      (bit 12)

  PDM DATA ◄── GPIO pad (func_sel=3, I2S0_DI 子功能)
       │
       ▼
  ┌─────────────────────┐
  │  PDM 硬件模块        │  ← 独立的 CIC 滤波 + PDM FIFO，不走 I2S FIFO
  │  CIC 抽取滤波 (dc_mul)│
  │  缩放 (scale_sel)    │
  │  抖动 (dither_sel)   │
  │  内部 8 级 RX FIFO   │
  └─────────────────────┘
       │
       ▼
  pdm_dma_rdata → 16-bit PCM → DMA → 内存
```

### 2.3 对比总结

| | PDM | I2S |
|---|---|---|
| **寄存器文件** | `pdm_reg.h`（独立） | `i2s_reg.h`（独立） |
| **基地址** | 待 Datasheet 确认 | `0x4000AA00` |
| **物理引脚** | 复用 I2S GPIO (func_sel=3) | GPIO func_sel=3 |
| **数据处理** | PDM CIC 硬件滤波 + PDM FIFO | I2S FIFO（`i2s_fifo_rdata`） |
| **时钟源** | `GLB_PDM_CLK_CTRL` (GLB 0x84) | `i2s_bclk_config` (I2S 0x10) |
| **BCLK 切换** | `reg_i2s_clk_sel = 1` | `reg_i2s_clk_sel = 0` |

---

## 3. 物理管脚配置

### 3.1 PDM 需要 2 根信号线

```
  BL702                        PDM Microphone
┌────────┐                  ┌──────────────────┐
│         │                  │                   │
│  GPIOx  ├─────────────────►│ CLK               │
│         │  (PDM_CLK)       │       (1~3 MHz)   │
│  GPIOy  │◄─────────────────┤ DATA              │
│         │  (PDM_DATA)      │     (bitstream)   │
│  VDD    ├─────────────────►│ VDD  (1.8V/3.3V)  │
│  GND    ├─────────────────►│ GND               │
└────────┘                  └──────────────────┘
```

- **PDM_CLK** — 典型频率 = 音频采样率 × 64（如 48 kHz → 3.072 MHz）
- **PDM_DATA** — 1-bit 过采样数字流
- **L/R 选择** — 单麦克风用 `force_lr=1` 强制左/右通道即可

### 3.2 GPIO 功能码

BL702 所有 GPIO 的 function code 3 都是 I2S：

```c
GPIO_FUN_I2S = 3
```

每个 GPIO 的 I2S 子功能分配如下（`bl702_gpio.h`）：

| GPIO | I2S 子功能 | PDM 用途 |
|---|---|---|
| 0, 4, 8, 12, 16, 20, 24 | **I2S0_BCLK** | **PDM_CLK** ← 选一个 |
| 1, 5, 9, 13, 17, 21, 25 | I2S0_FS | 不需要（用 force_lr） |
| 2, 6, 10, 14, 18, 22, 26 | I2S0_DIO (Data Out) | 不需要（PDM 只接收） |
| 3, 7, 11, 15, 19, 23 | **I2S0_RCLK_O_I2S0_DI** (Data In) | **PDM_DATA** ← 选一个 |

### 3.3 当前可用引脚

板上可用的 GPIO：**0, 1, 2, 23, 25**

| GPIO | I2S 子功能 (func_sel=3) | PDM 角色 |
|---|---|---|
| **0** | `I2S0_BCLK` | ✅ **PDM_CLK** 输出 → 麦克风 |
| 1 | `I2S0_FS` | ❌ 不需要 |
| 2 | `I2S0_DIO_I2S0_DO` | ❌ 不需要 |
| **23** | `I2S0_RCLK_O_I2S0_DI` | ✅ **PDM_DATA** 输入 ← 麦克风 |
| 25 | `I2S0_FS` | ❌ 不需要 |

> **完美匹配**：GPIO0 和 GPIO23 刚好凑齐 PDM 所需的 CLK + DATA 两根线。

### 3.4 pinmux_config.h 配置

```c
// PDM microphone: GPIO0=PDM_CLK, GPIO23=PDM_DATA
#define CONFIG_GPIO0_FUNC  GPIO_FUN_I2S   // I2S0_BCLK → PDM_CLK
#define CONFIG_GPIO23_FUNC GPIO_FUN_I2S   // I2S0_DI   → PDM_DATA

// 其余保持原样
#define CONFIG_GPIO1_FUNC  GPIO_FUN_UNUSED
#define CONFIG_GPIO2_FUNC  GPIO_FUN_UNUSED
#define CONFIG_GPIO25_FUNC GPIO_FUN_UNUSED
```

---

## 4. SDK 现状

### 4.1 已有资源

| 文件 | 状态 | 说明 |
|---|---|---|
| `regs/pdm_reg.h` | ✅ 完整 | 所有寄存器位定义 |
| `glb_reg.h` PDM_CLK_CTRL | ✅ 完整 | PDM 时钟分频器 (offset 0x84) |
| `glb_reg.h` clk_cfg1 | ✅ 完整 | `reg_i2s_clk_sel` 桥接位 (bit 12) |
| `bl702_glb.h` `GLB_Set_PDM_CLK()` | ✅ 完整 | PDM 时钟 API |
| `bl702_gpio.h` I2S pin mux | ✅ 完整 | func_sel=3 分配到所有 GPIO |
| `bl702_i2s.c/h` | ✅ 完整 | I2S 驱动，可参考代码结构 |

### 4.2 SDK 缺失

| 缺失项 | 说明 |
|---|---|
| **`PDM_BASE` 基地址** | `pdm_reg.h` 存在但 `bl702.h` 未定义基地址 |
| **`BL_AHB_SLAVE1_PDM` 枚举** | `bl702.h` 外设列表中无 PDM entry |
| **`bl702_pdm.c/h` 驱动** | 无可直接调用的 PDM 初始化/读取 API |
| **中断向量** | 未见 `PDM_IRQn` 中断号 |

---

## 5. 完整初始化流程（方案 A：直接 PDM 硬件）

```c
// 定义基地址（待 Datasheet 最终确认）
typedef volatile struct pdm_reg pdm_reg_t;
#define PDM_BASE    ((uint32_t)0x4000AB00)  // 推测地址
static pdm_reg_t * const PDM = (pdm_reg_t *)PDM_BASE;

void pdm_init(uint32_t sample_rate_hz)
{
    uint32_t tmp;

    // === 1. 时钟配置 ===

    // 1a. Audio PLL = 12.288 MHz (适合 48 kHz)
    PDS_Set_Audio_PLL_Freq(AUDIO_PLL_12288000_HZ);

    // 1b. I2S BCLK 时钟源切换到 PDM (clk_cfg1 bit12 = 1)
    //     这是 PDM ↔ I2S 桥接的关键步骤！
    tmp = BL_RD_REG(GLB_BASE, GLB_CLK_CFG1);
    tmp = BL_SET_REG_BIT(tmp, GLB_REG_I2S_CLK_SEL);
    BL_WR_REG(GLB_BASE, GLB_CLK_CFG1, tmp);

    // 1c. PDM 时钟使能 + 分频
    //     12.288 MHz / 3 = 4.096 MHz → 128× 倍频 → 32 kHz fs_ds
    //     12.288 MHz / 4 = 3.072 MHz → 64× 倍频  → 48 kHz (常用)
    uint8_t pdm_div = 4;  // → 3.072 MHz PDM CLK
    GLB_Set_PDM_CLK(ENABLE, pdm_div);

    // === 2. PDM 数据路径配置 ===

    // 2a. 抽取滤波参数
    PDM->pdm_datapath_config.WORD = 0;
    PDM->pdm_datapath_config.BF.dc_mul    = 64;   // 64× decimation
    PDM->pdm_datapath_config.BF.scale_sel = 5;    // 缩放
    PDM->pdm_datapath_config.BF.dither_sel = 1;   // 抖动

    // 2b. 单声道：强制左通道
    PDM->pdm_datapath_config.BF.force_lr  = 1;
    PDM->pdm_datapath_config.BF.force_sel = 0;    // 0=left

    // 2c. 128fs 时钟选择
    PDM->pdm_datapath_config.BF.rx_sel_128fs = 1;

    // 2d. DMA RX 配置
    PDM->pdm_dma_config.WORD = 0;
    PDM->pdm_dma_config.BF.rx_format = 3;          // 16-bit PCM
    PDM->pdm_dma_config.BF.pdm_dma_rx_en = 1;

    // 2e. 使能 PDM 数据通路
    PDM->pdm_datapath_config.BF.pdm_en = 1;
}

// 读取 PCM 数据
int16_t pdm_read_sample(void)
{
    while (PDM->pdm_rx_fifo_status.BF.rx_fifo_empty);
    return (int16_t)(PDM->pdm_dma_rdata.WORD & 0xFFFF);
}
```

### 5.1 信号通路总览

```
GPIO0  (func_sel=3, BCLK) ────► PDM_CLK ─────► 麦克风 CLK
GPIO23 (func_sel=3, DI)   ◄──── PDM_DATA ◄──── 麦克风 DATA
                                    │
                                    ▼
                             PDM CIC 硬件抽取滤波
                             · dc_mul=64 降采样
                             · dither 噪声整形
                                    │
                                    ▼
                             pdm_dma_rdata → 16-bit PCM → DMA → 内存
```

---

## 6. 其他实现方案

### 方案 B：GPIO + 软件 CIC 滤波

用 GPIO 中断配合 Timer 采样 DATA 线，软件实现 CIC 降采样。

| 优点 | 缺点 |
|---|---|
| 不依赖 PDM 外设 | 2.8 MHz 中断不可能（CPU 144 MHz 也撑不住） |
| 不需要 Datasheet | 需要 DMA 缓冲 + 后台处理 |
| 可立即开始 | 实现复杂 |

> **不推荐**，除非确认 PDM 外设不可用。

### 方案 C：I2S + 外部 PDM→I2S 转换芯片

用 BL702 成熟的 I2S RX 接收，外接硬件桥接芯片。

| 芯片型号 | 通道数 | 接口 |
|---|---|---|
| TI TLV320ADCx140 | 4-ch | I2S/TDM |
| TI PCMD3180 | 8-ch | I2S/TDM |
| Knowles IA611 | 1-ch | I2S |
| Vesper VM3011 | 1-ch | I2S (集成 PDM→I2S) |

| 优点 | 缺点 |
|---|---|
| BL702 I2S 驱动开箱即用 | 额外 BOM 成本 ~$2-5 |
| DMA 成熟可靠 | PCB 空间增加 |

---

## 7. 可行性评估

| 维度 | 评级 | 说明 |
|---|---|---|
| **硬件** | ⭐⭐⭐⭐ | PDM 外设确认存在，内置硬件抽取滤波 |
| **时钟** | ⭐⭐⭐⭐⭐ | Audio PLL 完整支持，独立 PDM 分频器，I2S_CLK_SEL 桥接 |
| **引脚** | ⭐⭐⭐⭐⭐ | GPIO0 (BCLK) + GPIO23 (DI) 完美匹配，2 根线无需外加芯片 |
| **SDK** | ⭐⭐⭐ | 寄存器定义完整，缺 PDM_BASE 和驱动代码 |
| **开发难度** | ⭐⭐⭐ | 可参考 I2S 驱动结构，核心初始化 ~100 行 |

---

## 8. 建议开发步骤

### 阶段 1：确认基地址
1. 获取 **BL702 Datasheet**，确认 `PDM_BASE`（推测 `0x4000AB00` 附近）
2. 参考 [Bouffalo BL602 PDM 驱动](https://github.com/bouffalolab/bl_mcu_sdk)（同系列芯片）

### 阶段 2：驱动开发（1-2 天）
1. 在 `bl702.h` 中补完 `PDM_BASE`、`BL_AHB_SLAVE1_PDM`
2. 参考 `bl702_i2s.c` 实现 `bl702_pdm.c`：
   - `PDM_Init()` — 时钟、抽取参数、DMA 配置（见 §5 完整流程）
   - `PDM_ReadFifo()` — RX FIFO 读取
   - `PDM_Start()` / `PDM_Stop()`
   - DMA 循环缓冲 + 中断回调
3. HAL 层封装 `hal_pdm.h`

### 阶段 3：示例工程（半天）
1. 创建 `my_apps/pdm_mic` 示例
2. pinmux：`GPIO0=I2S, GPIO23=I2S`
3. 验证：PDM 麦克风 → PCM 数据 → UART printf
4. 可选：USB Audio 传输到 PC

---

## 9. 参考资料

| 文件 | 内容 |
|---|---|
| `drivers/bl702_driver/regs/pdm_reg.h` | PDM 寄存器完整定义 |
| `drivers/bl702_driver/regs/i2s_reg.h` | I2S 寄存器（结构参考） |
| `drivers/bl702_driver/regs/glb_reg.h` L852 | `PDM_CLK_CTRL` 时钟分频器 |
| `drivers/bl702_driver/regs/glb_reg.h` L121-125 | `reg_i2s_clk_sel` PDM↔I2S 桥接位 |
| `drivers/bl702_driver/std_drv/src/bl702_glb.c` L2407 | `GLB_Set_PDM_CLK()` 实现 |
| `drivers/bl702_driver/std_drv/inc/bl702_gpio.h` | GPIO 功能码（func_sel=3 → I2S） |
| `drivers/bl702_driver/std_drv/inc/bl702_pds.h` | Audio PLL 频率枚举 |
| `drivers/bl702_driver/std_drv/src/bl702_i2s.c` | I2S 驱动实现（API 参考） |
| `examples/i2s/` | I2S 播放示例（DMA 用法参考） |
