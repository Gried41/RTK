#ifndef _APP_BTDM_H
#define _APP_BTDM_H

#include <stdint.h>
#include <stdbool.h>

enum {
    APP_BTDM_EVT_BLE_CONNECTED,
    APP_BTDM_EVT_BLE_DISCONNECTED,
    
    APP_BTDM_EVT_BT_CONNECTED,
    APP_BTDM_EVT_BT_DISCONNECTED,
    APP_BTDM_EVT_HF_INCOMING,
    APP_BTDM_EVT_HF_CALL_ACTIVE,
    APP_BTDM_EVT_HF_CALL_REMOVE,
    APP_BTDM_EVT_A2DP_STREAM_STARTED,
    APP_BTDM_EVT_A2DP_STREAM_STOPPED,
    APP_BTDM_EVT_A2DP_STREAM_DATA,
    APP_BTDM_EVT_AVRCP_VOL_CHANGE,
    APP_BTDM_EVT_AVRCP_NEXT,
    APP_BTDM_EVT_AVRCP_PREV,
    APP_BTDM_EVT_AVRCP_PAUSE,
    APP_BTDM_EVT_AVRCP_FAST_FORWARD,
    APP_BTDM_EVT_AVRCP_FAST_BACKWARD,
    APP_BTDM_EVT_SCO_CREATED,
    APP_BTDM_EVT_SCO_REMOVED,
    APP_BTDM_EVT_SCO_DATA,
};

enum {
    APP_BTDM_CODEC_SBC,
    APP_BTDM_CODEC_AAC,
    APP_BTDM_CODEC_mSBC,
    APP_BTDM_CODEC_PCM,
    APP_BTDM_CODEC_UNKNOWN,
};

struct app_btdm_event_t {
    uint8_t event;
    union {
        struct {
            uint8_t *buffer;
            uint32_t length;
        } a2dp_data;
        struct {
            uint8_t codec_type;
            uint32_t sample_rate;
        } a2dp_codec;
        struct {
            bool valid;
            uint8_t codec_type;
            uint8_t *buffer;
            uint32_t length;
        } sco_data;
        struct {
            void *hf_channel;
            uint8_t codec_type;
        } sco_codec;
    } param;
};

typedef void (*app_btdm_callback_t)(struct app_btdm_event_t *e);

void app_btdm_start(void);
void app_btdm_init(void);

#endif  // _APP_BTDM_H
