/**
 * @file pinmux_config.h
 * @brief Pinmux for INMP441 via I2S RX
 *
 * Overrides bl702_dapplus defaults — must include ALL needed GPIOs
 * because #pragma once suppresses the SDK board config.
 *
 *   GPIO0  = I2S_BCLK  → INMP441 SCK
 *   GPIO1  = I2S_FS    → INMP441 WS
 *   GPIO23 = I2S_DI    ← INMP441 SD
 */
#ifndef _PINMUX_CONFIG_H
#define _PINMUX_CONFIG_H

/* ---- I2S for INMP441 ---- */
#define CONFIG_GPIO0_FUNC  GPIO_FUN_I2S       /* I2S0_BCLK → SCK */
#define CONFIG_GPIO1_FUNC  GPIO_FUN_I2S       /* I2S0_FS   → WS  */
#define CONFIG_GPIO23_FUNC GPIO_FUN_I2S       /* I2S0_DI   ← SD  */

/* ---- USB (critical — GPIO7/8 MUST be USB for CDC ACM to work!) ---- */
#define CONFIG_GPIO7_FUNC  GPIO_FUN_USB       /* USB DM */
#define CONFIG_GPIO8_FUNC  GPIO_FUN_USB       /* USB DP */

/* ---- UART0 for debug ---- */
#define CONFIG_GPIO14_FUNC GPIO_FUN_UART0_TX
#define CONFIG_GPIO15_FUNC GPIO_FUN_UART0_RX

/* ---- LEDs and DAP pins (from bl702_dapplus defaults) ---- */
#define CONFIG_GPIO9_FUNC  GPIO_FUN_GPIO_OUTPUT_NONE
#define CONFIG_GPIO17_FUNC GPIO_FUN_GPIO_OUTPUT_NONE

/* ---- Unused (DAP JTAG pins + spare) ---- */
#define CONFIG_GPIO2_FUNC  GPIO_FUN_UNUSED
#define CONFIG_GPIO24_FUNC GPIO_FUN_UNUSED
#define CONFIG_GPIO25_FUNC GPIO_FUN_UNUSED
#define CONFIG_GPIO26_FUNC GPIO_FUN_UNUSED
#define CONFIG_GPIO27_FUNC GPIO_FUN_UNUSED
#define CONFIG_GPIO28_FUNC GPIO_FUN_UNUSED

#endif
