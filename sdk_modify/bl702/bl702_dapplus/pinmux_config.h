/**
 * @file pinmux_config.h
 * @brief Pinmux configuration for bl702_dapplus (RV-DAP+ CMSIS-DAP probe)
 *
 * Pin mapping aligned with DAP/Include/DAP_config.h BL702 GPIO Pins:
 *   SWCLK=GPIO0  SWDIO=GPIO1  TDI=GPIO26  TDO=GPIO27
 *   nRESET=GPIO23  LED_CONNECTED=GPIO9  LED_RUNNING=GPIO17
 *
 * DAP controls SWCLK/SWDIO/TDI/TDO via direct GLB register access,
 * so these pins must NOT be assigned to any peripheral function.
 * UART0 (GPIO14/GPIO15) provides printf debug output.
 *
 * Copyright (c) 2021 Bouffalolab team
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 */
#ifndef _PINMUX_CONFIG_H
#define _PINMUX_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <q> GPIO0  - SWCLK (DAP direct reg control)
// <i> config gpio0 function
#define CONFIG_GPIO0_FUNC GPIO_FUN_UNUSED

// <q> GPIO1  - SWDIO (DAP direct reg control)
// <i> config gpio1 function
#define CONFIG_GPIO1_FUNC GPIO_FUN_UNUSED

// <q> GPIO2  - spare
// <i> config gpio2 function
#define CONFIG_GPIO2_FUNC GPIO_FUN_UNUSED

// <q> GPIO7  - USB DM
// <i> config gpio7 function
#define CONFIG_GPIO7_FUNC GPIO_FUN_USB

// <q> GPIO8  - USB DP
// <i> config gpio8 function
#define CONFIG_GPIO8_FUNC GPIO_FUN_USB

// <q> GPIO9  - LED_CONNECTED (active low)
// <i> config gpio9 function
#define CONFIG_GPIO9_FUNC GPIO_FUN_GPIO_OUTPUT_NONE

// <q> GPIO14 - UART0 TX (debug log printf)
// <i> config gpio14 function
#define CONFIG_GPIO14_FUNC GPIO_FUN_UART0_TX

// <q> GPIO15 - UART0 RX (debug log printf)
// <i> config gpio15 function
#define CONFIG_GPIO15_FUNC GPIO_FUN_UART0_RX

// <q> GPIO17 - LED_RUNNING (active low)
// <i> config gpio17 function
#define CONFIG_GPIO17_FUNC GPIO_FUN_GPIO_OUTPUT_NONE

// <q> GPIO23 - nRESET (DAP target reset, open-drain)
// <i> config gpio23 function
#define CONFIG_GPIO23_FUNC GPIO_FUN_GPIO_OUTPUT_NONE

// <q> GPIO24 - spare
// <i> config gpio24 function
#define CONFIG_GPIO24_FUNC GPIO_FUN_UNUSED

// <q> GPIO25 - spare
// <i> config gpio25 function
#define CONFIG_GPIO25_FUNC GPIO_FUN_UNUSED

// <q> GPIO26 - TDI (DAP JTAG, direct reg control)
// <i> config gpio26 function
#define CONFIG_GPIO26_FUNC GPIO_FUN_UNUSED

// <q> GPIO27 - TDO (DAP JTAG input, direct reg control)
// <i> config gpio27 function
#define CONFIG_GPIO27_FUNC GPIO_FUN_UNUSED

// <q> GPIO28 - spare
// <i> config gpio28 function
#define CONFIG_GPIO28_FUNC GPIO_FUN_UNUSED

#endif
