#ifndef _APP_AT_H
#define _APP_AT_H

#include <stdint.h>

#include "driver_uart.h"

#define ENABLE_GNSS_CHECK_LOG       0

void app_at_cmd_recv_handler(uint8_t *data, uint16_t length);

void app_at_rx_done(struct __UART_HandleTypeDef *handle);

void rx_to_spp_done(struct __UART_HandleTypeDef *handle);

void rx_to_imu_done(struct __UART_HandleTypeDef *handle);

void Uart_Recv_IT(struct __UART_HandleTypeDef *handle);

void uart_spp_task_init(void);

void rx_to_gnss_done(struct __UART_HandleTypeDef *handle);

void rx_to_imu_task(void *arg);
void rx_to_imu_task_init(UART_HandleTypeDef *handle);

void imu_to_rx_done(struct __UART_HandleTypeDef *handle);
void imu_to_rx_task(void *arg);
void imu_to_rx_task_init(void);

void rx_to_gnss_uart3_task(void *arg);
void rx_to_gnss_uart3_task_init(UART_HandleTypeDef *handle);

void gnss_uart3_to_rx_done(struct __UART_HandleTypeDef *handle);
void gnss_uart3_to_rx_task(void *arg);
void gnss_uart3_to_rx_task_init(void);

void rx_to_gnss_uart2_task(void *arg);
void rx_to_gnss_uart2_task_init(UART_HandleTypeDef *handle);

void gnss_uart2_to_rx_done(struct __UART_HandleTypeDef *handle);
void gnss_uart2_to_rx_task(void *arg);
void gnss_uart2_to_rx_task_init(void);

static int hex_to_value(char c);
static int nmea_checksum_check(const char *line);

void uart_printf_mutex_init(void);
#endif  // _APP_AT_H
