/* 2019 04 20 */
/* By hxdyxd */

#ifndef UART_FIFO_FX_H
#define UART_FIFO_FX_H

#include "app_debug.h"
#define UART_FIFO_RX_DEBUG(...)
//#define UART_FIFO_RX_DEBUG   APP_DEBUG
#define UART_FIFO_RX_WARN    printf

#define UART_FIFO_RX_HOLD_BUG    (0)



int uart_fifo_rx_init(void);
int uart_fifo_rx_proc(void);
int uart_fifo_out(int id, void *buf, int len);
int uart_fifo_out_peek(int id, void *buf, int len);
int uart_fifo_size(int id);

void uart_fifo_rx_receive_RTS(int id, int enable);
void uart_fifo_rx_handler(UART_HandleTypeDef *huart2);

void uart_update_callback(int id);

#endif
