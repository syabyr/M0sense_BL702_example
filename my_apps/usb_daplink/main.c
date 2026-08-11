/**
 * @file main.c
 * @brief
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
#include "hal_uart.h"
#include "hal_usb.h"
#include "usbd_core.h"
#include "usbd_winusb.h"
#include "hal_gpio.h"

#define CDC_IN_EP  0x85
#define CDC_OUT_EP 0x04
#define CDC_INT_EP 0x83

#define CMSIS_DAP_EP_RECV 0x01
#define CMSIS_DAP_EP_SEND 0x82

#define USBD_VID           0xd6e7
#define USBD_PID           0x3507
#define USBD_MAX_POWER     500
#define USBD_LANGID_STRING 1033

#define CMSIS_DAP_INTERFACE_SIZE (9 + 7 + 7)
#define USB_CONFIG_SIZE          (9 + CMSIS_DAP_INTERFACE_SIZE)

// DAP-only descriptor — no CDC ACM so macOS won't claim the device.
USB_DESC_SECTION const uint8_t rv_dap_plus_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0100, 0x01),
    /* Configuration 0 */
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    /* Interface 0 (CMSIS-DAP) */
    USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x04),
    /* Endpoint OUT 1 */
    USB_ENDPOINT_DESCRIPTOR_INIT(CMSIS_DAP_EP_RECV, USB_ENDPOINT_TYPE_BULK, 0x40, 0x00),
    /* Endpoint IN 2 */
    USB_ENDPOINT_DESCRIPTOR_INIT(CMSIS_DAP_EP_SEND, USB_ENDPOINT_TYPE_BULK, 0x40, 0x00),
    /* String 0 (LANGID) */
    USB_LANGID_INIT(USBD_LANGID_STRING),
    /* String 1 (Manufacturer) */
    0x12,                       /* bLength */
    USB_DESCRIPTOR_TYPE_STRING, /* bDescriptorType */
    'B', 0x00,                  /* wcChar0 */
    'o', 0x00,                  /* wcChar1 */
    'u', 0x00,                  /* wcChar2 */
    'f', 0x00,                  /* wcChar3 */
    'f', 0x00,                  /* wcChar4 */
    'a', 0x00,                  /* wcChar5 */
    'l', 0x00,                  /* wcChar6 */
    'o', 0x00,                  /* wcChar7 */
    /* String 2 (Product) */
    0x1a,                       // bLength
    USB_DESCRIPTOR_TYPE_STRING, // bDescriptorType
    'R', 0,                     // wcChar0
    'V', 0,                     // wcChar1
    ' ', 0,                     // wcChar2
    'C', 0,                     // wcChar3
    'M', 0,                     // wcChar4
    'S', 0,                     // wcChar5
    'I', 0,                     // wcChar6
    'S', 0,                     // wcChar7
    '-', 0,                     // wcChar8
    'D', 0,                     // wcChar9
    'A', 0,                     // wcChar10
    'P', 0,                     // wcChar11
    /* String 3 (Serial Number) */
    0x1A,                       // bLength
    USB_DESCRIPTOR_TYPE_STRING, // bDescriptorType
    '0', 0,                     // wcChar0
    '1', 0,                     // wcChar1
    '2', 0,                     // wcChar2
    '3', 0,                     // wcChar3
    '4', 0,                     // wcChar4
    '5', 0,                     // wcChar5
    'A', 0,                     // wcChar6
    'B', 0,                     // wcChar7
    'C', 0,                     // wcChar8
    'D', 0,                     // wcChar9
    'E', 0,                     // wcChar10
    'F', 0,                     // wcChar11
    /* String 4 (Interface) */
    0x1a,                       // bLength
    USB_DESCRIPTOR_TYPE_STRING, // bDescriptorType
    'R', 0,                     // wcChar0
    'V', 0,                     // wcChar1
    ' ', 0,                     // wcChar2
    'C', 0,                     // wcChar3
    'M', 0,                     // wcChar4
    'S', 0,                     // wcChar5
    'I', 0,                     // wcChar6
    'S', 0,                     // wcChar7
    '-', 0,                     // wcChar8
    'D', 0,                     // wcChar9
    'A', 0,                     // wcChar10
    'P', 0,                     // wcChar11
#ifdef CONFIG_USB_HS
    /* Device Qualifier */
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x10,
    0x02,
    0x00,
    0x00,
    0x00,
    0x40,
    0x01,
    0x00,
#endif
    /* End */
    0x00
};

struct device *usb_fs;
static usbd_class_t dap_class;
static usbd_interface_t dap_interface;

extern void usb_dap_recv_callback(uint8_t ep);
extern void usb_dap_send_callback(uint8_t ep);

static usbd_endpoint_t dap_endpoint_recv = {
    .ep_addr = CMSIS_DAP_EP_RECV,
    .ep_cb = usb_dap_recv_callback
};
static usbd_endpoint_t dap_endpoint_send = {
    .ep_addr = CMSIS_DAP_EP_SEND,
    .ep_cb = usb_dap_send_callback
};

/* Override debug UART to UART1 (GPIO25 TX for debug log output). */
enum uart_index_type board_get_debug_uart_index(void)
{
    return 1;
}

extern void gpio_init(void);
extern void usb_handle(void);
extern struct device *usb_dc_init(void);
extern struct usb_msosv1_descriptor msosv1_desc;

int main(void)
{
    bflb_platform_init(0);
    // uart_ringbuffer_init();
    // uart1_init();
    // uart1_set_dtr_rts(UART_DTR_PIN, UART_RTS_PIN);

    usbd_desc_register(rv_dap_plus_descriptor);

    usbd_msosv1_desc_register(&msosv1_desc); /*register winusb*/

    usbd_class_register(&dap_class);
    usbd_class_add_interface(&dap_class, &dap_interface);
    usbd_interface_add_endpoint(&dap_interface, &dap_endpoint_recv);
    usbd_interface_add_endpoint(&dap_interface, &dap_endpoint_send);

    // usbd_cdc_add_acm_interface(&cdc_class, &cdc_cmd_intf);
    // usbd_cdc_add_acm_interface(&cdc_class, &cdc_data_intf);
    // usbd_interface_add_endpoint(&cdc_data_intf, &cdc_out_ep);
    // usbd_interface_add_endpoint(&cdc_data_intf, &cdc_in_ep);

    gpio_init();

    usb_fs = usb_dc_init();

    if (usb_fs) {
        device_control(usb_fs, DEVICE_CTRL_SET_INT, (void *)(USB_EP1_DATA_OUT_IT | USB_EP2_DATA_IN_IT));
    }

    while (!usb_device_is_configured()) {
        // Simply do nothing
    }

    while (1) {
        usb_handle();
        // uart_send_from_ringbuffer();
    }
}
