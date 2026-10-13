#ifndef _MCU_IMU_H
#define _MCU_IMU_H


#include "driver_uart.h"
#include <stdint.h>


typedef enum
{
    AT_CMD_INVALID = 0,
    AT_CMD_MEMS_ON,
    AT_CMD_MEMS_OFF,
    AT_CMD_MEMS_RATE,
    AT_CMD_ACTIVATE_KEY,
    AT_CMD_LOOP_BACK,
    AT_CMD_VERSION
} AT_CmdType;

AT_CmdType AT_Cmd_Parse(const char *cmd);
void Uart4_Send_AT_cmd(const char *cmd , struct __UART_HandleTypeDef *handle);





#endif