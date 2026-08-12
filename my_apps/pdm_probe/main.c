/**
 * PDM_BASE probe v2 — wider scan with I2S clock enabled.
 * Outputs continuously so USB CDC ACM timing is never an issue.
 */
#include "bflb_platform.h"
#include <usb_stdio.h>
#include "hal_mtimer.h"
#include "bl702_glb.h"
#include "pdm_reg.h"

#define N_CANDIDATES 4
static const uint32_t g_addr[N_CANDIDATES] = {
    0x4000AA00,  /* I2S_BASE — PDM might be embedded */
    0x4000AB00,  /* gap */
    0x4000AC00,  /* gap */
    0x4000AD00,  /* CAM_BASE (control) */
};
static const char *g_label[N_CANDIDATES] = {
    "I2S  (AA00)", "Gap  (AB00)", "Gap  (AC00)", "CAM  (AD00)"
};
static uint32_t g_reset[N_CANDIDATES];
static uint32_t g_write[N_CANDIDATES];
static uint32_t g_dma[N_CANDIDATES];
static uint32_t g_fifo[N_CANDIDATES];

static void do_probe(void)
{
    /* Enable I2S clock (PDM may share clock domain) */
    {
        uint32_t tmp = *(volatile uint32_t *)(GLB_BASE + 0x04);
        tmp |= (1U << 13);  /* GLB_REG_I2S0_CLK_EN */
        *(volatile uint32_t *)(GLB_BASE + 0x04) = tmp;
    }
    /* Toggle I2S M_EN so I2S block is powered */
    {
        volatile uint32_t *cfg = (volatile uint32_t *)0x4000AA00;
        *cfg |= 1U;
    }
    /* Enable PDM clock */
    GLB_Set_PDM_CLK(ENABLE, 4);

    for (int i = 0; i < N_CANDIDATES; i++) {
        volatile pdm_reg_t *p = (volatile pdm_reg_t *)g_addr[i];
        g_reset[i] = p->pdm_datapath_config.WORD;
        uint32_t new_val = g_reset[i] | 1U;
        p->pdm_datapath_config.WORD = new_val;
        g_write[i] = p->pdm_datapath_config.WORD;
        p->pdm_datapath_config.WORD = g_reset[i] & ~1U;
        g_dma[i]  = p->pdm_dma_config.WORD;
        g_fifo[i] = p->pdm_rx_fifo_status.WORD;
    }
}

int main(void)
{
    bflb_platform_init(0);
#ifdef M0SENSE_USE_USBSTDIO
    usb_stdio_init();
#endif
    do_probe();

    while (1) {
        printf("\r\n=== PDM Probe v2 ===\r\n");
        printf("  Clk: GLB_CFG1=0x%08lX  PDM_CLK_CTRL=0x%08lX\r\n\r\n",
               (unsigned long)*(volatile uint32_t *)(GLB_BASE + 0x04),
               (unsigned long)*(volatile uint32_t *)(GLB_BASE + 0x84));

        int found = -1;
        for (int i = 0; i < N_CANDIDATES; i++) {
            int stick = (g_write[i] == (g_reset[i] | 1U));
            printf("  %-14s    rst=0x%08lX  wr=0x%08lX  [%s]  dma=0x%08lX  fifo=0x%08lX\r\n",
                   g_label[i],
                   (unsigned long)g_reset[i], (unsigned long)g_write[i],
                   stick ? "STICK" : "IGNORE",
                   (unsigned long)g_dma[i], (unsigned long)g_fifo[i]);
            if (stick) found = i;
        }

        printf("\r\n");
        if (found >= 0) {
            printf("  >> PDM_BASE = 0x%08lX  CONFIRMED  (%s)\r\n",
                   (unsigned long)g_addr[found], g_label[found]);
        } else {
            printf("  >> No write confirmed. Try: enable I2S clk in GLB first?\r\n");
        }
        printf("============================================\r\n");
        mtimer_delay_ms(2000);
    }
}
