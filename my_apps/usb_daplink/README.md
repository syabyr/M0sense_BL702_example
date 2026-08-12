# usb_daplink — CMSIS-DAP v2 + CDC ACM 调试记录

BL702 实现 CMSIS-DAP v2（Bulk）调试器 + CDC ACM 虚拟串口复合设备。

---

## 1. 架构

```
USB 复合设备
├─ Interface 0: CMSIS-DAP v2 (vendor class 0xFF)
│   ├─ EP1 OUT (bulk, 0x01)  ← 调试命令
│   └─ EP2 IN  (bulk, 0x82)  → 调试响应
├─ Interface 1: CDC ACM 通信 (class 0x02)
│   └─ EP3 INT  (0x83)       ← 串口状态通知
└─ Interface 2: CDC ACM 数据 (class 0x0A)
    ├─ EP4 OUT (bulk, 0x04)  ← host → UART0 TX
    └─ EP5 IN  (bulk, 0x85)  → UART0 RX → host

UART0 (GPIO14 TX / GPIO15 RX) = 虚拟串口
UART1 (GPIO25 TX)             = debug log
```

**传输类型：Bulk（CMSIS-DAP v2 协议），不是 HID（v1）。**

---

## 2. CDC 串口问题（macOS 上单向不通）

### 现象
- ✅ CDC **IN**（设备 → host）正常
- ❌ CDC **OUT**（host → 设备）失败

### 决定性验证

pyusb 同时测两个 bulk OUT 端点：

| 接口 | OUT 端点 | 结果 |
|------|---------|------|
| DAP (interface 0) | EP1 OUT | ✅ 正常（`DAP_Info` 收到响应 `0000`） |
| CDC (interface 2) | EP4 OUT | ❌ 超时（`usbd_cdc_acm_bulk_out` 回调从未触发） |

### 排查结论

DAP 和 CDC 的 bulk OUT 走完全相同的 USB 中断 → 回调代码路径，DAP 正常而 CDC 失败，
说明 **USB 栈、中断使能、端点配置、回调注册全部正确**，问题锁定在 EP4 OUT 单点。

软件层已逐项确认正确：描述符（`bDeviceClass=ef/sub=02/proto=01`、3 接口、EP4/EP5）、
回调注册、中断使能（`USB_EP4_DATA_OUT_IT`）、端点 ACK 状态、
`src_buffer` 段（`.system_ram`，修复 DMA 读 0xff）、UART0 桥接。

### 根因（推断）

**macOS 的 `AppleUSBCDCACM` 内核驱动 claim 了 CDC 接口，接管 EP4/EP5，
host→device 数据未到达设备端固件回调。**

佐证：
1. `detach_kernel_driver` 返回 `Access denied`（内核驱动占用）
2. 本仓库早期记录过同样问题，workaround 是 DAP-only 固件

### 待验证（Linux 下）

在 Linux（`cdc_acm` 驱动）测试双向串口，若正常则确认是 macOS 驱动问题。

---

## 3. SWD 烧录速度慢问题

### 现象

pyOCD 烧录 RP2040 速度仅 **13.53 kB/s**。

### 根因

速度慢**与 bulk 无关**，主因是 **SWD 软件 bit-bang**：

1. BL702 **无硬件 SWD 控制器**，SWD 时序靠 CPU 逐 bit 翻转 GPIO 寄存器
2. SWD 时钟默认仅 **1 MHz**（`DAP_DEFAULT_SWJ_CLOCK`），硬件 SWD 调试器可达 10~50 MHz
3. 1 MHz ÷ SWD 协议开销（turn-around/ACK/parity）≈ 13 KB/s，与实测吻合

### 吞吐量计算

每个 SWD 数据 bit = 2 次 GPIO 写 + 协议开销，1 MHz SWCLK → ~13 KB/s。

### 已尝试的优化

| 方案 | 效果 |
|------|------|
| `DAP_DEFAULT_SWJ_CLOCK` 1 MHz → 4 MHz | ❌ 速度基本无变化 |

**结论**：SWJ 时钟不是唯一瓶颈，bit-bang 循环本身的 CPU 指令开销是硬上限。
BL702 软件 bit-bang SWD 的吞吐天花板约在 10~20 KB/s。

### 进一步优化方向（未实施）

| 方案 | 预期 | 难度 |
|------|------|------|
| 用 BL702 硬件 SPI 模拟 SWD（SPI 位时钟 + MOSI 输出 SWDIO） | 数倍提升 | 高 |
| 换带硬件 SWD 的调试芯片 | 10~50× | 硬件改动 |
| 精简 bit-bang 循环（汇编级，减少每 bit 指令数） | 10~30% | 中 |

---

## 4. 文件说明

| 文件 | 说明 |
|------|------|
| `main.c` | CMSIS-DAP + CDC ACM 复合设备 |
| `uart_interface.c/h` | UART0 ↔ CDC 桥接（`src_buffer` 在 `.system_ram`） |
| `config/` | 板级配置（clock/peripheral/pinmux），UART0 映射 GPIO14/15 |
| `DAP/Include/DAP_config.h` | DAP 配置（SWD 引脚 GPIO0/1，SWJ 时钟） |
| `daplink_process.c` | CMSIS-DAP 命令处理 |
| `winusb.c` | WinUSB MS OS 描述符 |

---

## 5. 构建

```bash
./build.sh my_apps/usb_daplink
```

产物：`uf2_demos/usb_daplink.uf2`
