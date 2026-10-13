#ifndef _MCU_GNSS_H
#define _MCU_GNSS_H


#include "driver_uart.h"
#include <stdint.h>


void Uart2_Send_AT_cmd(const char *cmd , struct __UART_HandleTypeDef *handle);
void Uart1_Send_AT_cmd(const char *cmd , struct __UART_HandleTypeDef *handle);
#endif