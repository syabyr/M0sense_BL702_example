/**
 * INMP441 Diag v5 — Final
 *
 * Root cause: bl702_dapplus Audio PLL defaults to 24 MHz which cannot
 * generate exactly 64 BCLK/WS for 16 kHz. INMP441 requires exactly 64×.
 * Fix: PDS_Set_Audio_PLL_Freq(AUDIO_PLL_12288000_HZ).
 *
 * Wiring:
 *   BL702                INMP441
 *   GPIO0  (BCLK) ──────► SCK
 *   GPIO1  (WS)   ──────► WS
 *   GPIO23 (DI)   ◄────── SD
 *   3.3V            ────► VDD
 *   GND             ────► GND, L/R
 */
#include "bflb_platform.h"
#include <usb_stdio.h>
#include "hal_i2s.h"
#include "hal_dma.h"
#include "hal_mtimer.h"
#include "hal_gpio.h"
#include "bl702_glb.h"
#include "bl702_pds.h"

/* ---- Globals for DMA double-buffer ---- */
#define BUF_BYTES    1024
#define BUF_WORDS    (BUF_BYTES / 4)

static int32_t g_buf[2][BUF_WORDS] __attribute__((aligned(4)));
static volatile int g_ready[2];
static volatile int g_idx;
static volatile int g_irq_count;
static volatile int g_overflow_count;

/* Stored results */
static int32_t g_best_sample[8];   /* first 8 samples from whichever trial worked */
static int     g_trial_dma[4];     /* DMA irqs per trial */
static int     g_trial_max[4];     /* peak per trial */
static const char *g_trial_name[4];
static int     g_num_trials;

static void rx_callback(struct device *dev, void *args, uint32_t size, uint32_t state)
{
    (void)dev; (void)args; (void)size; (void)state;
    g_irq_count++;

    int cur = g_idx;
    g_ready[cur] = 1;
    g_idx = !cur;

    struct device *i2s = device_find("i2s");
    if (i2s) {
        device_read(i2s, 0, g_buf[g_idx], BUF_BYTES);
    }
}

static void try_one(const char *name, int iface_mode,
                    int channels, int frame, int data,
                    uint32_t pll_freq, int sample_rate)
{
    printf("\r\n  [%s]: ch=%d frame=%d data=%d rate=%d pll=%lu\r\n",
           name, channels, frame, data, sample_rate, (unsigned long)pll_freq);

    /* Store name */
    int t = g_num_trials;
    g_trial_name[t] = name;

    /* Reset */
    GLB_AHB_Slave1_Reset(BL_AHB_SLAVE1_I2S);
    for (int i = 0; i < 2; i++) { g_ready[i] = 0; g_buf[0][i*32] = 0; }
    g_idx = 0; g_irq_count = 0; g_overflow_count = 0;

    /* Audio PLL */
    PDS_Set_Audio_PLL_Freq(pll_freq);

    /* I2S */
    i2s_register(I2S0_INDEX, "i2s");
    struct device *i2s = device_find("i2s");
    if (!i2s) { printf("    I2S fail\r\n"); g_num_trials++; return; }

    I2S_DEV(i2s)->iis_mode       = I2S_MODE_MASTER;
    I2S_DEV(i2s)->interface_mode = iface_mode;
    I2S_DEV(i2s)->sampl_freq_hz  = sample_rate;
    I2S_DEV(i2s)->channel_num    = channels;
    I2S_DEV(i2s)->frame_size     = frame;
    I2S_DEV(i2s)->data_size      = data;
    I2S_DEV(i2s)->fifo_threshold = 3;

    if (device_open(i2s, DEVICE_OFLAG_DMA_RX)) {
        printf("    I2S open fail\r\n"); g_num_trials++; return;
    }

    /* DMA */
    dma_register(DMA0_CH3_INDEX, "dma3");
    struct device *dma = device_find("dma3");
    if (!dma) { printf("    DMA fail\r\n"); g_num_trials++; return; }

    DMA_DEV(dma)->direction      = DMA_PERIPH_TO_MEMORY;
    DMA_DEV(dma)->transfer_mode  = DMA_LLI_ONCE_MODE;
    DMA_DEV(dma)->src_req        = DMA_REQUEST_I2S_RX;
    DMA_DEV(dma)->dst_req        = DMA_REQUEST_NONE;
    DMA_DEV(dma)->src_addr_inc   = DMA_ADDR_INCREMENT_DISABLE;
    DMA_DEV(dma)->dst_addr_inc   = DMA_ADDR_INCREMENT_ENABLE;
    DMA_DEV(dma)->src_burst_size = DMA_BURST_INCR4;
    DMA_DEV(dma)->dst_burst_size = DMA_BURST_INCR4;
    DMA_DEV(dma)->src_width      = DMA_TRANSFER_WIDTH_32BIT;
    DMA_DEV(dma)->dst_width      = DMA_TRANSFER_WIDTH_32BIT;

    if (device_open(dma, 0)) { printf("    DMA open fail\r\n"); g_num_trials++; return; }

    device_control(i2s, DEVICE_CTRL_ATTACH_RX_DMA, (void *)dma);
    device_set_callback(I2S_DEV(i2s)->rx_dma, rx_callback);
    device_control(I2S_DEV(i2s)->rx_dma, DEVICE_CTRL_SET_INT, NULL);

    /* Start → uses i2s_read() → dma_reload + dma_channel_start on rx_dma */
    device_read(i2s, 0, g_buf[0], BUF_BYTES);

    /* Wait */
    mtimer_delay_ms(2000);

    /* Check results */
    volatile uint32_t *i2s_regs = (volatile uint32_t *)0x4000AA00;
    uint32_t fcfg0 = i2s_regs[32];
    uint32_t fcfg1 = i2s_regs[33];
    uint32_t intst = i2s_regs[1];
    int rx_cnt  = (fcfg1 >> 8) & 0x1F;
    int ovf     = !!(fcfg0 & 0x40);
    int fer     = !!(intst & 4);

    int max_val = 0;
    for (int i = 0; i < 8; i++) {
        int v = g_buf[0][i] >> 8;
        if (v < 0) v = -v;
        if (v > max_val) max_val = v;
    }

    printf("    dma_irqs=%d fifo_cnt=%d ovf=%d fer=%d max=%ld\r\n",
           g_irq_count, rx_cnt, ovf, fer, (long)max_val);
    printf("    buf[0..7]:");
    for (int i = 0; i < 8; i++) printf(" %08lX", (unsigned long)g_buf[0][i]);

    if (max_val > 0) {
        printf("  ← DATA!");
        for (int i = 0; i < 8; i++) g_best_sample[i] = g_buf[0][i];
    }
    printf("\r\n");

    g_trial_dma[t] = g_irq_count;
    g_trial_max[t] = max_val;

    device_close(i2s);
    device_close(dma);
    g_num_trials++;
}

int main(void)
{
    bflb_platform_init(0);
    gpio_write(9, 1);
    gpio_set_mode(9, GPIO_OUTPUT_MODE);

#ifdef M0SENSE_USE_USBSTDIO
    usb_stdio_init();
#endif

    /* Pinmux check */
    int p0  = (*(volatile uint32_t *)(GLB_BASE + 0x100) >> 8)  & 0x1F;
    int p1  = (*(volatile uint32_t *)(GLB_BASE + 0x100) >> 24) & 0x1F;
    int p23 = (*(volatile uint32_t *)(GLB_BASE + 0x12C) >> 24) & 0x1F;

    /* ---- Run all trials ---- */

    /* Trial A: 12.288 MHz PLL, I2S STD, 16 kHz, stereo 32-bit (64 BCLK/WS) */
    try_one("A: 12M_STD_16k",  I2S_MODE_STD,
            I2S_FS_CHANNELS_NUM_2, I2S_FRAME_LEN_32, I2S_DATA_LEN_24,
            AUDIO_PLL_12288000_HZ, 16000);

    /* Trial B: 12.288 MHz PLL, LEFT-justified, 16 kHz */
    try_one("B: 12M_LEFT_16k", I2S_MODE_LEFT,
            I2S_FS_CHANNELS_NUM_2, I2S_FRAME_LEN_32, I2S_DATA_LEN_24,
            AUDIO_PLL_12288000_HZ, 16000);

    /* Trial C: 12.288 MHz PLL, I2S STD, 16 kHz, mono 32-bit (32 BCLK/WS)
     * INMP441 probably won't like this but let's confirm */
    try_one("C: 12M_STD_16k_mono", I2S_MODE_STD,
            I2S_FS_CHANNELS_NUM_MONO, I2S_FRAME_LEN_32, I2S_DATA_LEN_24,
            AUDIO_PLL_12288000_HZ, 16000);

    /* Trial D: 24 MHz PLL (default), I2S STD, 16 kHz stereo
     * This should fail with fer=1, confirming the PLL theory */
    try_one("D: 24M_STD_16k_fer?", I2S_MODE_STD,
            I2S_FS_CHANNELS_NUM_2, I2S_FRAME_LEN_32, I2S_DATA_LEN_24,
            AUDIO_PLL_24000000_HZ, 16000);

    /* ---- Loop-print results forever ---- */
    int loop = 0;
    while (1) {
        gpio_write(9, loop & 1);

        printf("\r\n=== INMP441 Diag v5 [t=%d] ===\r\n", loop);
        printf("Pinmux: GPIO0=%d(%s) GPIO1=%d(%s) GPIO23=%d(%s)\r\n",
               p0, p0==3?"I2S":"ERR", p1, p1==3?"I2S":"ERR", p23, p23==3?"I2S":"ERR");

        /* Live I2S snapshot */
        volatile uint32_t *ir = (volatile uint32_t *)0x4000AA00;
        uint32_t fcfg0 = ir[32], fcfg1 = ir[33], intst = ir[1];
        printf("Live: fifo_cnt=%lu fer=%lu ovf=%lu\r\n",
               (unsigned long)((fcfg1>>8)&0x1F),
               (unsigned long)(!!(intst&4)),
               (unsigned long)(!!(fcfg0&0x40)));

        /* Trial results */
        for (int t = 0; t < g_num_trials; t++) {
            printf("  %-22s  dma=%d  max=%d  %s\r\n",
                   g_trial_name[t], g_trial_dma[t], g_trial_max[t],
                   g_trial_max[t] > 0 ? "← DATA!" : "");
        }

        /* Best sample data */
        int any_data = 0;
        for (int t = 0; t < g_num_trials; t++) {
            if (g_trial_max[t] > 0) any_data = 1;
        }
        if (any_data) {
            printf("  Best samples:");
            for (int i = 0; i < 8; i++) printf(" %08lX", (unsigned long)g_best_sample[i]);
            printf("\r\n");
        }

        printf("\r\nWiring: GPIO0→SCK  GPIO1→WS  GPIO23←SD  L/R→GND\r\n");
        printf("VDD→3.3V  GND→GND\r\n");
        printf("============================================\r\n");

        mtimer_delay_ms(2000);
        loop++;
    }
}
