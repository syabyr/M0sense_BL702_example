/**
 * @file main.c
 * @brief CMSIS-DAP v2 + CDC ACM composite device (usb_daplink)
 *
 * Interface 0 : CMSIS-DAP (vendor 0xFF, bulk EP1 OUT / EP2 IN)
 * Interface 1 : CDC ACM communication (IAD, interrupt EP3)
 * Interface 2 : CDC ACM data (bulk EP4 OUT / EP5 IN), bridged to UART0
 *
 * UART0 (GPIO14 TX / GPIO15 RX) is the virtual COM port.
 * UART1 (GPIO25 TX) is reserved for debug log.
 *
 * Copyright (c) 2021 Bouffalolab team
 */
#include "hal_uart.h"
#include "hal_usb.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_winusb.h"
#include "uart_interface.h"
#include "hal_gpio.h"
#include "bl702_sec_dbg.h"

#define CDC_IN_EP  0x85
#define CDC_OUT_EP 0x04
#define CDC_INT_EP 0x83

#define CMSIS_DAP_EP_RECV 0x01
#define CMSIS_DAP_EP_SEND 0x82

#define USBD_VID           0xd6e7
#define USBD_PID           0x3507
#define USBD_MAX_POWER     500
#define USBD_LANGID_STRING 1033

#define CMSIS_DAP_INTERFACE_SIZE (9 + 7 + 7) /* interface + 2 endpoints */
#define USB_CONFIG_SIZE          (9 + CMSIS_DAP_INTERFACE_SIZE + CDC_ACM_DESCRIPTOR_LEN)

/* Serial number string data offset inside rv_dap_plus_descriptor[]:
 *   device(18) + config(9) + DAP iface(9) + EP(7) + EP(7) + CDC(66)
 *   + LANGID(4) + mfr(18) + product(26) + 2 (bLength,bType) = 166 */
#define SERIAL_STR_DATA_OFFSET 166

/* Non-const: serial number is patched at runtime from unique chip ID. */
static uint8_t rv_dap_plus_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0100, 0x01),
    /* Configuration 0 */
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x03, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    /* Interface 0 (CMSIS-DAP) */
    USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x04),
    /* Endpoint OUT 1 */
    USB_ENDPOINT_DESCRIPTOR_INIT(CMSIS_DAP_EP_RECV, USB_ENDPOINT_TYPE_BULK, 0x40, 0x00),
    /* Endpoint IN 2 */
    USB_ENDPOINT_DESCRIPTOR_INIT(CMSIS_DAP_EP_SEND, USB_ENDPOINT_TYPE_BULK, 0x40, 0x00),
    /* Interface 1/2 (CDC ACM) */
    CDC_ACM_DESCRIPTOR_INIT(0x01, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, 0x00),
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
    /* String 3 (Serial Number) — patched from unique chip ID at init */
    0x22,                       // bLength (34 = 2 header + 32 data for 16 hex chars)
    USB_DESCRIPTOR_TYPE_STRING, // bDescriptorType
    '0', 0,                     // wcChar0
    '1', 0,                     // wcChar1
    '2', 0,                     // wcChar2
    '3', 0,                     // wcChar3
    '4', 0,                     // wcChar4
    '5', 0,                     // wcChar5
    '6', 0,                     // wcChar6
    '7', 0,                     // wcChar7
    '8', 0,                     // wcChar8
    '9', 0,                     // wcChar9
    'A', 0,                     // wcChar10
    'B', 0,                     // wcChar11
    'C', 0,                     // wcChar12
    'D', 0,                     // wcChar13
    'E', 0,                     // wcChar14
    'F', 0,                     // wcChar15
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

/* ---- CDC ACM: virtual COM port bridged to UART0 ---- */
static usbd_class_t cdc_class;
static usbd_interface_t cdc_cmd_intf;
static usbd_interface_t cdc_data_intf;

static void usbd_cdc_acm_bulk_out(uint8_t ep)
{
    usb_dc_receive_to_ringbuffer(usb_fs, &usb_rx_rb, ep);
}

static void usbd_cdc_acm_bulk_in(uint8_t ep)
{
    usb_dc_send_from_ringbuffer(usb_fs, &uart0_rx_rb, ep);
}

static usbd_endpoint_t cdc_out_ep = {
    .ep_addr = CDC_OUT_EP,
    .ep_cb = usbd_cdc_acm_bulk_out
};

static usbd_endpoint_t cdc_in_ep = {
    .ep_addr = CDC_IN_EP,
    .ep_cb = usbd_cdc_acm_bulk_in
};

/* Weak-overridable CDC callbacks (defined in usbd_cdc.c as __weak). */
void usbd_cdc_acm_set_line_coding(uint32_t baudrate, uint8_t databits, uint8_t parity, uint8_t stopbits)
{
    uart0_config(baudrate, (uart_databits_t)databits, (uart_parity_t)parity, (uart_stopbits_t)stopbits);
}

void usbd_cdc_acm_set_dtr(bool dtr)
{
    (void)dtr;
    /* no hardware flow control on DAP COM port */
}

void usbd_cdc_acm_set_rts(bool rts)
{
    (void)rts;
    /* no hardware flow control on DAP COM port */
}

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

    /* Patch USB serial number with unique chip ID (8 bytes → 16 hex chars). */
    {
        uint8_t chip_id[8];
        static const char hex[] = "0123456789ABCDEF";
        Sec_Dbg_Read_Chip_ID(chip_id);
        for (int i = 0; i < 8; i++) {
            rv_dap_plus_descriptor[SERIAL_STR_DATA_OFFSET + i * 4 + 0] = hex[(chip_id[i] >> 4) & 0x0F];
            rv_dap_plus_descriptor[SERIAL_STR_DATA_OFFSET + i * 4 + 2] = hex[ chip_id[i]       & 0x0F];
        }
    }

    /* UART0 bridge for CDC ACM */
    uart_ringbuffer_init();
    uart0_init();

    usbd_desc_register(rv_dap_plus_descriptor);

    usbd_msosv1_desc_register(&msosv1_desc); /*register winusb*/

    /* CMSIS-DAP interface */
    usbd_class_register(&dap_class);
    usbd_class_add_interface(&dap_class, &dap_interface);
    usbd_interface_add_endpoint(&dap_interface, &dap_endpoint_recv);
    usbd_interface_add_endpoint(&dap_interface, &dap_endpoint_send);

    /* CDC ACM interface */
    usbd_cdc_add_acm_interface(&cdc_class, &cdc_cmd_intf);
    usbd_cdc_add_acm_interface(&cdc_class, &cdc_data_intf);
    usbd_interface_add_endpoint(&cdc_data_intf, &cdc_out_ep);
    usbd_interface_add_endpoint(&cdc_data_intf, &cdc_in_ep);

    gpio_init();

    usb_fs = usb_dc_init();

    if (usb_fs) {
        /* EP1 OUT (DAP) | EP2 IN (DAP) | EP4 OUT (CDC) | EP5 IN (CDC) */
        device_control(usb_fs, DEVICE_CTRL_SET_INT,
                       (void *)(USB_EP1_DATA_OUT_IT | USB_EP2_DATA_IN_IT |
                                USB_EP4_DATA_OUT_IT | USB_EP5_DATA_IN_IT));
    }

    while (!usb_device_is_configured()) {
        // Simply do nothing
    }

    while (1) {
        usb_handle();
        uart_send_from_ringbuffer();
    }
}
