/**
 * @file uart_interface.c
 * @brief UART0 ↔ USB CDC ACM bridge for usb_daplink
 *
 * CDC ACM virtual COM port bridges to UART0 (GPIO14 TX / GPIO15 RX).
 * Adapted from the SDK's bsp/bsp_common/usb/uart_interface.c (which targets
 * UART1), retargeted to UART0 / DMA CH1.
 */
#include "bflb_platform.h"
#include "hal_gpio.h"
#include "uart_interface.h"
#include "hal_usb.h"
#include "hal_dma.h"
#include "bl702_glb.h"

#define USB_OUT_RINGBUFFER_SIZE (8 * 1024)
#define UART_RX_RINGBUFFER_SIZE (8 * 1024)
#define UART_TX_DMA_SIZE        (4095)

uint8_t usb_rx_mem[USB_OUT_RINGBUFFER_SIZE] __attribute__((section(".system_ram")));
uint8_t uart_rx_mem[UART_RX_RINGBUFFER_SIZE] __attribute__((section(".system_ram")));

/* DMA source buffer must be in .system_ram (not .tcm_code) — the DMA master
 * does not go through the CPU's TCM remap, so a .tcm_code buffer is read as
 * 0xff garbage. */
uint8_t src_buffer[UART_TX_DMA_SIZE] __attribute__((section(".system_ram")));

struct device *uart0;
struct device *dma_ch1;

Ring_Buffer_Type usb_rx_rb;
Ring_Buffer_Type uart0_rx_rb;

static void uart_irq_callback(struct device *dev, void *args, uint32_t size, uint32_t state)
{
    (void)dev;

    if (state == UART_EVENT_RX_FIFO || state == UART_EVENT_RTO) {
        if (size && size < Ring_Buffer_Get_Empty_Length(&uart0_rx_rb)) {
            /* LED1 (GPIO17) toggles each time UART0 RX data arrives (loopback). */
            gpio_write(17, !gpio_read(17));
            Ring_Buffer_Write(&uart0_rx_rb, (uint8_t *)args, size);
        }
    }
}

void uart0_init(void)
{
    /* Explicitly route UART0 TX/RX to GPIO14/GPIO15.
     *
     * board_pin_mux_init() (run earlier by bflb_platform_init) handles UART
     * function codes via (func & 0x07), which for GPIO_FUN_UART0_TX (0xF2)
     * yields 2 = UART1_RTS — i.e. it MISROUTES UART0 TX to the wrong GLB UART
     * signal slot, so UART0 never reaches the physical pins. This is the same
     * proven setup the BL702 bootloader (hal_boot2_uart_gpio_init) uses for
     * these exact pins, and runs after board_init so it overrides the bad
     * routing. SIG slot = pin % 8: GPIO14->SIG6, GPIO15->SIG7. */
    GLB_GPIO_Type uart0_pins[] = { GPIO_PIN_14, GPIO_PIN_15 };
    GLB_GPIO_Func_Init(GPIO_FUN_UART, uart0_pins, 2);
    GLB_UART_Fun_Sel((GPIO_PIN_14 % 8), GLB_UART_SIG_FUN_UART0_TXD);
    GLB_UART_Fun_Sel((GPIO_PIN_15 % 8), GLB_UART_SIG_FUN_UART0_RXD);

    uart_register(UART0_INDEX, "uart0");
    uart0 = device_find("uart0");

    dma_register(DMA0_CH1_INDEX, "ch1");
    dma_ch1 = device_find("ch1");

    if (dma_ch1) {
        DMA_DEV(dma_ch1)->direction      = DMA_MEMORY_TO_PERIPH;
        DMA_DEV(dma_ch1)->transfer_mode  = DMA_LLI_ONCE_MODE;
        DMA_DEV(dma_ch1)->src_req        = DMA_REQUEST_NONE;
        DMA_DEV(dma_ch1)->dst_req        = DMA_REQUEST_UART0_TX;
        DMA_DEV(dma_ch1)->src_addr_inc   = DMA_ADDR_INCREMENT_ENABLE;
        DMA_DEV(dma_ch1)->dst_addr_inc   = DMA_ADDR_INCREMENT_DISABLE;
        DMA_DEV(dma_ch1)->src_burst_size = DMA_BURST_INCR1;
        DMA_DEV(dma_ch1)->dst_burst_size = DMA_BURST_INCR1;
        DMA_DEV(dma_ch1)->src_width      = DMA_TRANSFER_WIDTH_8BIT;
        DMA_DEV(dma_ch1)->dst_width      = DMA_TRANSFER_WIDTH_8BIT;
        device_open(dma_ch1, 0);
    }
}

void uart0_config(uint32_t baudrate, uart_databits_t databits, uart_parity_t parity, uart_stopbits_t stopbits)
{
    if (!uart0) {
        return;
    }

    device_close(uart0);
    UART_DEV(uart0)->baudrate = baudrate;
    UART_DEV(uart0)->stopbits = stopbits;
    UART_DEV(uart0)->parity   = parity;
    UART_DEV(uart0)->databits = (databits - 5);
    device_open(uart0, DEVICE_OFLAG_DMA_TX | DEVICE_OFLAG_INT_RX);
    device_set_callback(uart0, uart_irq_callback);
    device_control(uart0, DEVICE_CTRL_SET_INT, (void *)(UART_RX_FIFO_IT | UART_RTO_IT));
    Ring_Buffer_Reset(&usb_rx_rb);
    Ring_Buffer_Reset(&uart0_rx_rb);
}

static void ringbuffer_lock(void)
{
    cpu_global_irq_disable();
}

static void ringbuffer_unlock(void)
{
    cpu_global_irq_enable();
}

void uart_ringbuffer_init(void)
{
    memset(usb_rx_mem, 0, USB_OUT_RINGBUFFER_SIZE);
    memset(uart_rx_mem, 0, UART_RX_RINGBUFFER_SIZE);

    Ring_Buffer_Init(&usb_rx_rb, usb_rx_mem, USB_OUT_RINGBUFFER_SIZE, ringbuffer_lock, ringbuffer_unlock);
    Ring_Buffer_Init(&uart0_rx_rb, uart_rx_mem, UART_RX_RINGBUFFER_SIZE, ringbuffer_lock, ringbuffer_unlock);
}

static dma_control_data_t uart_dma_ctrl_cfg = {
    .bits.fix_cnt      = 0,
    .bits.dst_min_mode = 0,
    .bits.dst_add_mode = 0,
    .bits.SI           = 1,
    .bits.DI           = 0,
    .bits.SWidth       = DMA_TRANSFER_WIDTH_8BIT,
    .bits.DWidth       = DMA_TRANSFER_WIDTH_8BIT,
    .bits.SBSize       = 0,
    .bits.DBSize       = 0,
    .bits.I            = 0,
    .bits.TransferSize = 4095
};

static dma_lli_ctrl_t uart_lli_list = {
    .src_addr = (uint32_t)src_buffer,
    .dst_addr = DMA_ADDR_UART0_TDR,
    .nextlli  = 0
};

void uart_send_from_ringbuffer(void)
{
    if (!dma_ch1) {
        return;
    }

    if (Ring_Buffer_Get_Length(&usb_rx_rb)) {
        if (!dma_channel_check_busy(dma_ch1)) {
            uint32_t availCnt = Ring_Buffer_Read(&usb_rx_rb, src_buffer, UART_TX_DMA_SIZE);

            if (availCnt) {
                dma_channel_stop(dma_ch1);
                uart_dma_ctrl_cfg.bits.TransferSize = availCnt;
                memcpy(&uart_lli_list.cfg, &uart_dma_ctrl_cfg, sizeof(dma_control_data_t));
                dma_channel_update(dma_ch1, (void *)((uint32_t)&uart_lli_list));
                dma_channel_start(dma_ch1);
            }
        }
    }
}
