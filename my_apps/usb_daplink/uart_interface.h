/**
 * @file uart_interface.h
 * @brief UART0 ↔ USB CDC ACM bridge for usb_daplink
 *
 * CDC ACM virtual COM port is bridged to UART0 (GPIO14 TX / GPIO15 RX),
 * while UART1 (GPIO25 TX) is reserved for debug log.
 */

#ifndef __UART_IF_H__
#define __UART_IF_H__

#include "hal_uart.h"
#include "ring_buffer.h"

extern Ring_Buffer_Type usb_rx_rb;   /* USB OUT → UART0 TX */
extern Ring_Buffer_Type uart0_rx_rb; /* UART0 RX → USB IN */

void uart0_init(void);
void uart0_config(uint32_t baudrate, uart_databits_t databits, uart_parity_t parity, uart_stopbits_t stopbits);
void uart_ringbuffer_init(void);
void uart_send_from_ringbuffer(void);

#endif
