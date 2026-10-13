#ifndef _APP_BT_H
#define _APP_BT_H

#include "app_btdm.h"

#include "hf_api.h"
#include "me_api.h"
#include "a2dp_api.h"
#include "hfg_api.h"
#include "avrcp_api.h"
#include "hid_api.h"
#include "spp_api.h"
#include "pbap_api.h"
#include "map_api.h"

#define NUM_STREAMS         4

extern HfChannel *hf_channel;
extern HfgChannel *hfg_channel;
extern A2dpStream *Stream;
extern AvrcpChannel *rcpCtChannel;
extern PbapClientSession   *pbap_client;
extern MapClientSession   *map_client;
extern SppDev *spp_dev;
extern HidChannel *hid_channel;

uint8_t bt_get_free_hf_channel(void);
uint8_t bt_get_free_hfg_channel(void);
uint8_t bt_get_free_a2dp_sink_stream(void);
uint8_t bt_get_free_a2dp_source_stream(void);
uint8_t bt_get_free_avrcp_channel(void);
uint8_t bt_get_free_spp_channel(void);
uint8_t bt_get_free_hid_channel(void);

void app_bt_send_sco_data(void *channel, uint8_t seq, uint8_t *data, uint16_t length);
void app_bt_init(void);
void bt_avrcp_register_notification(AvrcpChannel *chnl, uint16_t event_mask);
void bt_name_query(BD_ADDR *addr, BtPageScanInfo *psi);


#endif  // _APP_BT_H
