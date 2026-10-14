#include "mcu_imu.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define MEMS_Original_Data          "AT+MEMS_OUTPUT=UART1,ON/OFF/RATE"  
#define Device_Active               "AT+ACTIVATE_KEY=KEY"               
#define Uart1_Local_LoopBack        "AT+LOOP_BACK=UART1"                //MCU向IMU发送的消息都会被回发给MCU     UART4->UART1->UART4

static char uart4_tx_buf[128 + 2];

/*

*/
AT_CmdType AT_Cmd_Parse(const char *cmd)
{
    if(cmd == NULL)
    {
        return AT_CMD_INVALID;
    }
    else if(strcmp(cmd, "AT+MEMS_OUTPUT=UART1,ON") == 0) 
    {
        return AT_CMD_MEMS_ON;
    }
    else if(strcmp(cmd, "AT+MEMS_OUTPUT=UART1,OFF") == 0)
    {
        return AT_CMD_MEMS_OFF;
    }
    else if(strcmp(cmd, "AT+ACTIVATE_KEY=KEY") == 0)
    {
        return AT_CMD_ACTIVATE_KEY;
    }
    else if(strcmp(cmd, "AT+LOOP_BACK=UART1") == 0)
    {
        return AT_CMD_LOOP_BACK;
    }
    else if(strcmp(cmd, "AT+VERSION") == 0)
    {
        return AT_CMD_VERSION;
    }
    else
    {
        return AT_CMD_INVALID;
    }
}
/*

*/
void Uart4_Send_AT_cmd(const char *cmd , struct __UART_HandleTypeDef *handle)
{
    /*
    switch(AT_Cmd_Parse(cmd))
    {
        case AT_CMD_MEMS_ON:
            printf("MEMS Original_Data_Output_Enable\r\n");
            break;
        case AT_CMD_MEMS_OFF:
            printf("MEMS Original_Data_Output_Disable\r\n");
            break;
        case AT_CMD_ACTIVATE_KEY:
            printf("Device Active...\r\n");
            break;
        case AT_CMD_LOOP_BACK:
            printf("Device LOOP_BACK_Start\r\n");
            break;
        case AT_CMD_VERSION:
            printf("Get Device Version\r\n");
            break;
        default:
            printf("INVALID CMD...\r\n");
            return;
    }
    */
    uint8_t uart4_tx_buf_len = strlen(cmd);
    memcpy(uart4_tx_buf,cmd,uart4_tx_buf_len);
    uart4_tx_buf[uart4_tx_buf_len++] = '\r';
    uart4_tx_buf[uart4_tx_buf_len] = '\n';
    uart_transmit(handle,uart4_tx_buf,(uint32_t)(uart4_tx_buf_len+1));
}


/*
@fn:通过串口发送指令


static bool Uart4_Send_AT_cmd(const char *cmd)
{
    switch
}
*/