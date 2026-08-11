# pyOCD 使用文档

## 环境

- 工具链: pyocd 0.44.1 (@ `/opt/miniconda3/bin/pyocd`)
- 调试器: **Bouffalo RV CMSIS-DAP** (VID 0xd6e7, PID 0x3507)
- 目标: RP2040 / Raspberry Pi Pico

---

## 1. 列出连接的调试器

```bash
pyocd list
```

输出示例:
```
  #   Probe/Board             Unique ID      Target
-----------------------------------------------------
  0   Bouffalo RV CMSIS-DAP   012345ABCDEF   n/a
```

列出支持的目标:
```bash
pyocd list --targets | grep rp2040
```

---

## 2. 交互式命令 (commander)

### 连接并检查

```bash
# -N: 不初始化目标 (no init)，仅连接 DP
pyocd commander -t rp2040 -N

# 常用命令 (在 commander 内):
init                    # 初始化目标
halt                    # 暂停 CPU
status                  # 查看核心状态
show target             # 显示目标信息 (DAP IDCODE 等)
reg                     # 读取所有寄存器
reg pc sp               # 读取特定寄存器
write32 0x4000c000 0x1  # 写32位
read32 0x4000c000       # 读32位
exit                    # 退出
```

### 带选项连接

```bash
# 慢速连接 (解决 WAIT ACK 问题)
pyocd commander -t rp2040 -O frequency=100000

# 复位后连接
pyocd commander -t rp2040 -O connect_mode=under-reset
```

---

## 3. 读取/写入内存

```bash
# commander 模式下:
write32 <addr> <value>    # 写 32 位
write16 <addr> <value>    # 写 16 位
write8  <addr> <value>    # 写 8 位
read32  <addr>            # 读 32 位
read16  <addr>            # 读 16 位
read8   <addr>            # 读 8 位
```

---

## 4. 烧录固件

```bash
pyocd flash -t rp2040 firmware.elf
pyocd flash -t rp2040 firmware.hex
pyocd flash -t rp2040 firmware.bin --base-address 0x10000000
```

---

## 5. GDB 调试

```bash
# 启动 gdbserver
pyocd gdb -t rp2040

# 另一个终端连接:
arm-none-eabi-gdb firmware.elf
(gdb) target remote :3333
(gdb) load
(gdb) continue
```

---

## 6. Python API

```python
from pyocd.core.helpers import ConnectHelper

session = ConnectHelper.session_with_chosen_probe(
    target_override='rp2040_core0',
    options={'frequency': 100000}
)

try:
    session.open()  # init_board=True (默认) 会初始化目标
    target = session.board.target

    # 暂停
    target.halt()
    pc = target.read_core_register('pc')
    print(f"PC = 0x{pc:08X}")

    # 写寄存器
    target.write32(0x4000c000, 0xffffffff)  # 地址, 值
    val = target.read32(0x4000c000)
    print(f"Value = 0x{val:08X}")

finally:
    session.close()
```

### 初始化选项

```python
# 不初始化板子 (手动控制 DP/AP)
session = ConnectHelper.session_with_chosen_probe(
    target_override='rp2040_core0',
    options={
        'connect_mode': 'attach',  # or 'under-reset'
        'frequency': 100000,
    }
)
session.open(init_board=False)
```

---

## 7. RP2040 关键寄存器

### RESETS (解除外设复位)
| 寄存器 | 地址 | 说明 |
|--------|------|------|
| RESET | 0x4000C000 | 写 0 复位，写 1 释放 |
| RESET_DONE | 0x4000C008 | 读位为1表示已释放 |

### IO_BANK0 (管脚功能选择)
| 寄存器 | 地址 | 说明 |
|--------|------|------|
| GPIO0_CTRL | 0x40014004 | GPIO0 功能选择 |
| GPIO25_CTRL | 0x400140CC | GPIO25 功能选择 |
| 功能值 5 = SIO | | 软件 IO 控制 |

### SIO (单周期 IO)
| 寄存器 | 地址 | 说明 |
|--------|------|------|
| GPIO_IN | 0xD0000004 | 输入值 |
| GPIO_OUT | 0xD0000010 | 输出值 |
| GPIO_OUT_SET | 0xD0000014 | 写1置高 |
| GPIO_OUT_CLR | 0xD0000018 | 写1清零 |
| GPIO_OE | 0xD0000020 | 输出使能 |
| GPIO_OE_SET | 0xD0000024 | 写1使能输出 |

### 点亮 Pico LED (GPIO25)
```bash
pyocd commander -t rp2040 <<'EOF'
reinit
halt
write32 0x4000c000 0xffffffff    # 解除复位
write32 0x400140cc 0x00000005    # GPIO25 → SIO
write32 0xd0000024 0x02000000    # 输出使能
write32 0xd0000014 0x02000000    # LED ON
exit
EOF
```

---

## 8. 常见问题

### `SWD/JTAG communication failure (WAIT ACK)`
- 降低频率: `-O frequency=100000`
- 目标未供电 / 复位未释放
- AP 访问前需先配置 DP CTRL/STAT (CDBGPWRUPREQ + CSYSPWRUPREQ)

### `SWD/JTAG communication failure (NO ACK)`
- SWCLK/SWDIO 物理连接问题
- 检查接线和共地

### `SWD/JTAG communication failure (FAULT ACK)`
- RP2040 DPv2 多目标选择问题
- 需要先通过 TARGETSEL 选择正确的 DP
- pyOCD 的 `rp2040_core0` 目标类型会自动处理

### macOS CDC ACM 抢占问题
- 如果设备有 CDC ACM 接口，macOS 会抢占
- 用 DAP-only 固件 (不含 CDC) 可避免

---

## 9. 参考链接

- pyOCD 文档: https://pyocd.io/
- RP2040 数据手册: https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
- CMSIS-DAP 协议: https://arm-software.github.io/CMSIS_5/DAP/html/index.html
