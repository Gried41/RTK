#include "mcu_gnss.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

static char uart2_tx_buf[128 + 2];
static char uart1_tx_buf[128 + 2];
/*
@fn:uart2 send cmd
*/
void Uart2_Send_AT_cmd(const char *cmd , struct __UART_HandleTypeDef *handle)
{
    uint8_t uart2_tx_buf_len = strlen(cmd);
    memcpy(uart2_tx_buf,cmd,uart2_tx_buf_len);
    uart2_tx_buf[uart2_tx_buf_len++] = '\r';
    uart2_tx_buf[uart2_tx_buf_len] = '\n';
    uart_transmit(handle,uart2_tx_buf,(uint32_t)(uart2_tx_buf_len+1));
    printf("Uart2 Send Cmd %s\r\n",uart2_tx_buf);
}

void Uart1_Send_AT_cmd(const char *cmd , struct __UART_HandleTypeDef *handle)
{
    uint8_t uart1_tx_buf_len = strlen(cmd);
    memcpy(uart1_tx_buf,cmd,uart1_tx_buf_len);
    uart1_tx_buf[uart1_tx_buf_len++] = '\r';
    uart1_tx_buf[uart1_tx_buf_len] = '\n';
    uart_transmit(handle,uart1_tx_buf,(uint32_t)(uart1_tx_buf_len+1));
    printf("Uart1 Send Cmd %s\r\n",uart1_tx_buf);
}
