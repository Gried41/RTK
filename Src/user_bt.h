#ifndef __USER_BT_H
#define __USER_BT_H

#include <stdint.h>
#include "app_config.h"
#include "app_bt.h"
#include "bt_types.h"
#include "btconfig.h"
#include "co_list.h"

///enable_profiles
#define ENABLE_PROFILE_HF               BIT0
#define ENABLE_PROFILE_A2DP_SINK        BIT1    //if a2dp is enabled, avrcp is enabled automatically
#define ENABLE_PROFILE_A2DP_SOURCE      BIT2
#define ENABLE_PROFILE_HID              BIT3
#define ENABLE_PROFILE_PBAP             BIT4
#define ENABLE_PROFILE_SPP              BIT5
#define ENABLE_PROFILE_HFG              BIT6
#define ENABLE_PROFILE_MAP              BIT7
#define ENABLE_PROFILE_ALL              0xff

//conn flags
#define LINK_STATUS_HF_CONNECTED    (1<<0)
#define LINK_STATUS_AV_CONNECTED    (1<<1)
#define LINK_STATUS_AVC_CONNECTED   (1<<2)
#define LINK_STATUS_MEDIA_PLAYING   (1<<3)
#define LINK_STATUS_SCO_CONNECTED   (1<<4)  
#define LINK_STATUS_HID_CONNECTED   (1<<5)
#define LINK_STATUS_PBAP_CONNECTED  (1<<6)
#define LINK_STATUS_SPP_CONNECTED   (1<<7)
#define LINK_STATUS_HFG_CONNECTED   (1<<8)


enum bt_state_t{
    BT_STATE_IDLE,                          //IDLE
    BT_STATE_CONNECTING,                    //CONNECTING, connect is ongoing
	BT_STATE_DISCONNECTING,                 //disconnecting 
    BT_STATE_CONNECTED,                     //CONNECTED, connected,no call and music
    BT_STATE_HFP_INCOMMING,                 //INCOMMING CALL
    BT_STATE_HFP_OUTGOING,                  //OUTGOING CALL
    BT_STATE_HFP_CALLACTIVE,                //CALL IS ACTIVE
    BT_STATE_MEDIA_PLAYING,                 //MUSIC PLAYING
    BT_STATE_MAX,                           
};

//enum bt_state_t{
//    BT_STATE_IDLE,                          //IDLE
//    BT_STATE_PAIRING,                       //PAIRING
//    BT_STATE_CONNECTED,                     //CONNECTED, connected,no call and music
//    BT_STATE_BOTH_CONNECTED,                //used in connecting with two device
//    BT_STATE_MAX,                           
//};

enum bt_link_state_t{
    BT_LINK_STATE_IDLE,
    BT_LINK_STATE_CONNECTING,
    BT_LINK_STATE_DISCONNECTING,
    BT_LINK_STATE_ACL_CONNECTED,
    BT_LINK_STATE_PROFILE_CONNECTED,
};

enum app_bt_access_state_t{
    ACCESS_IDLE = 0x00,
    ACCESS_PAIRING,
};

enum bt_connect_action_t{
    BT_ACTION_NULL,
    BT_ACTION_CONNECT,
    BT_ACTION_DISCONNECT,
    BT_ACTION_ACCESS,
};

enum bt_connect_event_t{
    BT_EVENT_CON_IND,
    BT_EVENT_CON_CNF,
    BT_EVENT_PROFILE_CONNECT,
    BT_EVENT_DISCONNECT,
    BT_EVENT_ACC_CHG,
};

enum bt_profile_event_t{
    BT_PROFILE_HF_CONN_REQ,
    BT_PROFILE_HF_CONN,
    BT_PROFILE_HF_DISCONN,
    BT_PROFILE_HF_AUDIO_CONN,
    BT_PROFILE_HF_AUDIO_DISCONN,
    
    BT_PROFILE_HF_CALL,     //5
    BT_PROFILE_HF_CALLSETUP,
    BT_PROFILE_HF_SPK_VOL,
    BT_PROFILE_HF_CURRENT_CALL,
    BT_PROFILE_HF_AT_RESULT,
    BT_PROFILE_HF_RING,     //10
    BT_PROFILE_HF_AUDIO_DATA,
    BT_PROFILE_HF_MAX,      
    
    BT_PROFILE_A2DP_CONN,
    BT_PROFILE_A2DP_DISCONN,
    BT_PROFILE_A2DP_PLAYING,//15
    BT_PROFILE_A2DP_SUSPEND,
    BT_PROFILE_A2DP_OPEN_IND,
    BT_PROFILE_A2DP_STREAM_DATA,
    BT_PROFILE_A2DP_SBC_PKT_SENT,
    BT_PROFILE_A2DP_MAX,//20
    
    BT_PROFILE_AVRCP_CONN,
    BT_PROFILE_AVRCP_DISCONN,
    BT_PROFILE_AVRCP_TRACK_CHANGED,
    BT_PROFILE_AVRCP_MEDIA_STATUS,
    BT_PROFILE_AVRCP_MEDIA_INFO,//25
    BT_PROFILE_AVRCP_MEDIA_POS,
    BT_PROFILE_AVRCP_EVENT_SUPPORT,
    BT_PROFILE_AVRCP_MAX,
    
    BT_PROFILE_PBAP_CONN,
    BT_PROFILE_PBAP_DISCONN,//30
    BT_PROFILE_PBAP_DATA_IND,
    BT_PROFILE_PBAP_COMP,
    BT_PROFILE_PBAP_MAX,
    
    BT_PROFILE_HFG_CONN_REQ,
    BT_PROFILE_HFG_CONN,//35
    BT_PROFILE_HFG_DISCONN,
    BT_PROFILE_HFG_AUDIO_CONN,
    BT_PROFILE_HFG_AUDIO_DISCONN,
    BT_PROFILE_HFG_AUDIO_DATA,
    BT_PROFILE_HFG_MAX,//40
    
    BT_PROFILE_SPP_CONN,
    BT_PROFILE_SPP_DISCONN,
    BT_PROFILE_SPP_MAX,
    
    BT_PROFILE_HID_CONN,
    BT_PROFILE_HID_DISCONN,
    BT_PROFILE_HID_MAX,
    
    BT_INFO_INQ_RESULT,
    BT_INFO_INQ_CANCEL,
    BT_INFO_INQ_COMP,
    BT_INFO_MAX,
};

struct bt_connect_param_t{
    uint16_t page_timeout;     //unit: 625us
    uint8_t connect_times;     
//    uint8_t connect_interval; //unit: 1s , interval shall larger than page_timeout
};

struct bt_connect_elt_t{
    struct co_list_hdr hdr;
    BD_ADDR addr;
    uint8_t action;
    uint8_t profiles;
//    struct bt_connect_param_t param;
}; 

/* Device context */ 
typedef struct app_device_t {
    BD_ADDR         remote_bd;
    uint16_t         conFlags;//avrcp,a2dp,hfp conn status
    uint8_t         prf_all_connected;
    uint8_t         responder;
    uint8_t         state;
    
    uint8_t         connect_profile;
    uint8_t         mode;//active or sniff
    #if BTDM_STACK_ENABLE_HF || BTDM_STACK_ENABLE_AG
    HfCallSetupState setup_state;
    HfCallActiveState active;
    HfCallHeldState call_held;
    #endif
    BtRemoteDevice *remDev;
    #if BTDM_STACK_ENABLE_HF
    HfChannel *hf_chan;
    #endif
    #if BTDM_STACK_ENABLE_AG
    HfgChannel *hfg_chan;
    #endif
    #if BTDM_STACK_ENABLE_AVRCP
    AvrcpChannel *rcp_chan;
    #endif
    #if BTDM_STACK_ENABLE_A2DP_SNK || BTDM_STACK_ENABLE_A2DP_SRC
    A2dpStream *pstream;
    #endif
    #if BTDM_STACK_ENABLE_HID
    HidChannel *hid_chan;
    #endif
    #if BTDM_STACK_ENABLE_PBAP
    PbapClientSession   *pbap_client;
    #endif
    #if BTDM_STACK_ENABLE_SPP
    SppDev *spp_dev;
    #endif
    
} APP_DEVICE;

struct user_bt_env_t {
	uint8_t enable_profiles;
	uint8_t last_active_index;
    uint8_t access_state;
    uint16_t page_timeout;     //unit: 625us
    uint8_t connect_times; 
	BD_ADDR last_dev_addr;
    uint8_t last_connect_profile;
    uint8_t bt_source_sbc_pkt_num;
    uint8_t cur_action;
    uint8_t action_cnt;
    bool enable_local_ring;
    struct co_list op_list;
//    struct bt_connect_param_t connect_param;
	void (*bt_disconnect_cb)(BD_ADDR *addr, uint8_t errcode);
	void (*bt_connect_cb)(uint8_t type, BD_ADDR *addr, uint8_t errcode);
	void (*bt_access_change_cb)(uint8_t mode);
	APP_DEVICE dev[NUM_BT_DEVICES];
};

struct sbc_info_t{
    uint8_t elem[4];
    SbcStreamInfo info;
};

extern struct user_bt_env_t user_bt_env;
extern struct sbc_info_t sbcinfo[];

BtStatus bt_connect(BD_ADDR *addr, uint8_t profiles);

BtStatus bt_disconnect(BD_ADDR *addr, uint8_t force_disconnect);

BtStatus bt_poweroff(void);

void bt_connect_act_cmp(uint8_t evt,uint8_t error,BtRemoteDevice *rem_dev);

void bt_update_conn_status(uint8_t event, void *chan, const void *param);

uint8_t user_bt_get_state(uint8_t dev_index);

void user_bt_init(void);


BtStatus bt_answer_call(uint8_t dev_index);

BtStatus bt_dial_number(uint8_t dev_index, uint8_t *number, uint16_t len);

BtStatus bt_redial(uint8_t dev_index);

BtStatus bt_call_hold(uint8_t dev_index, HfHoldAction action, uint8_t index);
                     
BtStatus bt_hang_up(uint8_t dev_index);

BtStatus bt_list_current_calls(uint8_t dev_index);

BtStatus bt_transfer_sco(uint8_t dev_index);

BtStatus bt_send_dtmf(uint8_t dev_index, uint8_t dtmf);

BtStatus bt_report_mic_volume(uint8_t dev_index, uint8_t vol);

BtStatus bt_report_spk_volume(uint8_t dev_index, uint8_t vol);

BtStatus bt_send_hf_cmd(uint8_t dev_index, const uint8_t *at_str);

BtStatus bt_enable_voice_recognition(uint8_t dev_index, uint8_t enabled);

BtStatus bt_enable_caller_id_notify(uint8_t dev_index, uint8_t enabled);

uint8_t bt_is_voice_rec_active(uint8_t dev_index);

BtStatus bt_get_media_info(uint8_t dev_index,AvrcpMediaAttrIdMask mediaMask);

BtStatus bt_get_playstatus(uint8_t dev_index);

BtStatus bt_set_media_volume(uint8_t dev_index, uint8_t volume);

BtStatus bt_enter_pairing(uint8_t access, BtAccessModeInfo *info);

BtStatus bt_exit_pairing(void);

#if AVRCP_BROWSING_CONTROLLER == XA_ENABLED
BtStatus bt_get_mediaplayer(uint8_t dev_index);
#endif

#endif