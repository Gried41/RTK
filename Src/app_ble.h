#ifndef _APP_BLE_H
#define _APP_BLE_H

#include <stdint.h>

#include "gap_api.h"

void app_ble_init(void);

void app_ble_advertising_start(uint8_t adv_chn);
void app_ble_advertising_stop(uint8_t adv_chn);

void app_ble_scan_start(void);
void app_ble_scan_stop(void);

void app_ble_conn_start(struct gap_ble_addr *addr);
void app_ble_conn_stop(void);

#endif  // _APP_BLE_H
