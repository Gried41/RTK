#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "btdm_host.h"
#include "btdm_utils.h"
#include "pan_api.h"
#include "fdb_app.h"

#include "app_config.h"
#include "app_bt.h"
#include "user_bt.h"
#include "FreeRTOS.h"
#include "app_hid_sdp.h"

#if BTDM_STACK_ENABLE_BT

#if BTDM_STACK_ENABLE_PAN
#include "ethernetif.h"
#endif

#ifndef LOG_ENABLE
#define LOG_ENABLE
#endif
#include "co_log.h"
#ifdef LOG_LOCAL_LEVEL
#undef LOG_LOCAL_LEVEL
#endif
#define LOG_LOCAL_LEVEL             LOG_LEVEL_INFO

#define max(a, b)   ((a) > (b) ? (a) : (b))
#define min(a, b)   ((a) > (b) ? (b) : (a))

uint8_t spp_send_test = 8;

const BtAccessModeInfo access_mode_nc = {
    .inqInterval = 0x800,
    .inqWindow = 0x12,
    .pageInterval = 0x800,
    .pageWindow = 0x12,
};


static BtHandler securityHandler;
static BtHandler globalHandler;
static BtSecurityParms secParms;

uint8_t default_name[] = "dmy spp test"; //default_name = FR30xx_0000

#if BTDM_STACK_ENABLE_HF
static char hf_codec[2];
HfChannel *hf_channel;
#endif
#if BTDM_STACK_ENABLE_AG
HfgChannel *hfg_channel;
#endif
#if BTDM_STACK_ENABLE_A2DP_SNK || BTDM_STACK_ENABLE_A2DP_SRC
A2dpStream *Stream;
#endif
#if BTDM_STACK_ENABLE_AVRCP
AvrcpChannel *rcpCtChannel;
#endif
#if BTDM_STACK_ENABLE_PBAP
PbapClientSession   *pbap_client;     /* PBAP client */
#endif
#if BTDM_STACK_ENABLE_SPP
SppDev *spp_dev;
#endif
#if BTDM_STACK_ENABLE_MAP
MapClientSession *map_client;
#endif
#if BTDM_STACK_ENABLE_HID
HidChannel *hid_channel;
#endif
#if BTDM_STACK_ENABLE_A2DP_SNK || BTDM_STACK_ENABLE_A2DP_SRC
uint8_t SbcSnkElements[]={
    A2DP_SBC_CODEC_INF_ELEMENT(
        /* 48000 and 44100 are required for SNK */ 
       A2DP_SBC_CODEC_FREQ_48000  | 
       A2DP_SBC_CODEC_FREQ_44100  | 
       A2DP_SBC_CODEC_FREQ_32000  | 
       A2DP_SBC_CODEC_FREQ_16000,

       /* All modes required for SNK */ 
       A2DP_SBC_CODEC_CHNL_MODE_MONO    | 
       A2DP_SBC_CODEC_CHNL_MODE_DUAL    | 
       A2DP_SBC_CODEC_CHNL_MODE_STEREO  | 
       A2DP_SBC_CODEC_CHNL_MODE_JOINT,

       /* One block size must be supported */ 
       A2DP_SBC_CODEC_BLOCKS_16  | 
       A2DP_SBC_CODEC_BLOCKS_12  | 
       A2DP_SBC_CODEC_BLOCKS_8   | 
       A2DP_SBC_CODEC_BLOCKS_4,

       /* Both 8 and 4 must be supported */ 
       A2DP_SBC_CODEC_SUBBANDS_8 | 
       A2DP_SBC_CODEC_SUBBANDS_4,

       /* Both allocation methods must  be supported in SNK */ 
       A2DP_SBC_CODEC_ALLOCATION_LOUDNESS | 
       A2DP_SBC_CODEC_ALLOCATION_SNR,

       /* Minium bitpool */
       2,

       /* Maximum bitpool */
       53)

    };

uint8_t AacSnkElements[] = {
    A2DP_AAC_CODEC_MPEG2_LC     |
    A2DP_AAC_CODEC_MPEG4_LC,
    
    A2DP_AAC_CODEC_FREQ_44100, 
    
    A2DP_AAC_CODEC_FREQ_48000   |
    A2DP_AAC_CODEC_CHNL_MONO    |
    A2DP_AAC_CODEC_CHNL_STEREO, 
    
    A2DP_AAC_CODEC_VBR,
    
    0x00, 0x00
    };

/* Source SBC Elements */ 
static uint8_t SbcSrcElements[] = {
    A2DP_SBC_CODEC_INF_ELEMENT(/* 48000 or 44100 is required for SRC  */ 
                               /*A2DP_SBC_CODEC_FREQ_48000 | */
                               A2DP_SBC_CODEC_FREQ_44100 /*| 
                               A2DP_SBC_CODEC_FREQ_32000 | 
                               A2DP_SBC_CODEC_FREQ_16000*/,

                               /* MONO and one other required for SRC */ 
                               A2DP_SBC_CODEC_CHNL_MODE_MONO    | 
                               A2DP_SBC_CODEC_CHNL_MODE_DUAL    | 
                               A2DP_SBC_CODEC_CHNL_MODE_STEREO  | 
                               A2DP_SBC_CODEC_CHNL_MODE_JOINT,

                               /* One block size must be supported */ 
                               A2DP_SBC_CODEC_BLOCKS_16 | 
                               A2DP_SBC_CODEC_BLOCKS_12 | 
                               A2DP_SBC_CODEC_BLOCKS_8  | 
                               A2DP_SBC_CODEC_BLOCKS_4,

                               /* SRC must support 8 subbands */ 
                               A2DP_SBC_CODEC_SUBBANDS_8 |
                               A2DP_SBC_CODEC_SUBBANDS_4,

                               /* SRC must support LOUDNESS */ 
                               A2DP_SBC_CODEC_ALLOCATION_LOUDNESS | 
                               A2DP_SBC_CODEC_ALLOCATION_SNR,

                               /* Minium bitpool */
                               2,

                               /* Maximum bitpool */
                               53)
};

AvdtpCodec sbcSnkCodec1;         //local codec information
AvdtpCodec aacSnkCodec1;
AvdtpCodec sbcSnkCodec2;         //local codec information
AvdtpCodec aacSnkCodec2;

AvdtpCodec          sbcSrcCodec1;
AvdtpCodec          sbcSrcCodec2;

static AvdtpCodec          cfgCodec;
static uint8_t             cfgElements[4];
#endif
BtRemoteDevice *last_rem_dev = NULL;

MeCommandToken *name_token;

static void name_result_handler(const BtEvent *event)
{
    printf("name result: %d,%d\r\n",event->eType,event->errCode);
	if((event->eType != BTEVENT_NAME_RESULT) || (event->p.meToken != name_token)){
        return;
    }
    if(event->errCode == 0){
        printf("$$$$$addr:%x,%x,name:%d,%s\r\n",event->p.meToken->p.name.bdAddr.A[0],event->p.meToken->p.name.bdAddr.A[1],event->p.meToken->p.name.io.out.len,event->p.meToken->p.name.io.out.name);
    }
    if(name_token){
        btdm_free(name_token);
        name_token = 0;
    }
}

void bt_name_query(BD_ADDR *addr, BtPageScanInfo *psi)
{
    BtStatus         status;

    ///doing name request
    if(name_token){
        return;
    }
    name_token = (MeCommandToken *)btdm_malloc(sizeof(MeCommandToken));
    memcpy(&name_token->p.name.bdAddr,addr,6);
    memcpy(&name_token->p.name.io.in.psi,psi,sizeof(BtPageScanInfo));
    //memset(&name_token.p.name.io.in.psi,0,sizeof(BtPageScanInfo));

    name_token->callback = name_result_handler;
    
    status = ME_GetRemoteDeviceName(name_token);
    if(status != BT_STATUS_PENDING){
        btdm_free(name_token);
        name_token = 0;
    }
    printf("bt_name_query: %x,%x\r\n",addr->A[0],addr->A[1]);
}
#if BTDM_STACK_ENABLE_HF
uint8_t bt_get_free_hf_channel(void)
{
    uint8_t index = 0;
    for(index = 0; index < NUM_BT_DEVICES; index++){
        if(hf_channel[index].state == HF_STATE_CLOSED){
            break;
        }
    }
    return index;
}
#endif
#if BTDM_STACK_ENABLE_AG
uint8_t bt_get_free_hfg_channel(void)
{
    uint8_t index = 0;
    for(index = 0; index < NUM_BT_DEVICES; index++){
        if(hfg_channel[index].state == HFG_STATE_CLOSED){
            break;
        }
    }
    return index;
}
#endif
#if BTDM_STACK_ENABLE_A2DP_SNK
uint8_t bt_get_free_a2dp_sink_stream(void)
{
    uint8_t index = 0;
    for(index = 0; index < NUM_BT_DEVICES; index++){
        //if(Is_a2dp_stream_registered(&Stream[index]) && (Stream[index].device->state == A2DP_DEV_STATE_DISCONNECTED)){
        if(Is_a2dp_stream_registered(&Stream[index]) && (Stream[index].device == NULL)){
            break;
        }
    }
    return index;
}
#endif
#if BTDM_STACK_ENABLE_A2DP_SRC
uint8_t bt_get_free_a2dp_source_stream(void)
{
    uint8_t index = 0;
    for(index = 2; index < NUM_STREAMS; index++){
        //if(Is_a2dp_stream_registered(&Stream[index]) && (Stream[index].device->state == A2DP_DEV_STATE_DISCONNECTED)){
        if(Is_a2dp_stream_registered(&Stream[index]) && (Stream[index].device == NULL)){
            break;
        }
    }
    return index;
}
#endif
#if BTDM_STACK_ENABLE_SPP
uint8_t bt_get_free_spp_channel(void)
{
    uint8_t index = 0;
    for(index = 0; index < NUM_BT_DEVICES; index++){
        if(spp_dev[index].state == DEVICE_STATE_DISCONNECTED){
            break;
        }
    }
    return index;
}
#endif
#if BTDM_STACK_ENABLE_HID
uint8_t bt_get_free_hid_channel(void)
{
    uint8_t index = 0;
    for(index = 0; index < NUM_BT_DEVICES; index++){
        if(hid_channel[index].state == HID_STATE_CLOSED){
            break;
        }
    }
    return index;
}
#endif

#if BTDM_STACK_ENABLE_AVRCP
uint8_t bt_get_free_avrcp_channel(void)
{
    uint8_t index = 0;
    for(index = 0; index < NUM_BT_DEVICES; index++){
        if(rcpCtChannel[index].chnl.conn.state == 0){
            break;
        }
    }
    return index;
}

void bt_avrcp_register_notification(AvrcpChannel *chnl, uint16_t event_mask)
{
    AvrcpAdvancedPdu *avrcp_cmd;
    BtStatus status;
    printf("avrcp register event:%x\r\n",event_mask);
    if(event_mask & AVRCP_ENABLE_PLAY_STATUS_CHANGED){
        ///注册播放状态改变事件，当TG播放状态改变时，产生AVRCP_EVENT_ADV_NOTIFY事件
        avrcp_cmd = (AvrcpAdvancedPdu *)btdm_malloc(sizeof(AvrcpAdvancedPdu));
        avrcp_cmd->parms = (uint8_t *)btdm_malloc(64);
        status = AVRCP_CtRegisterNotification(chnl, avrcp_cmd, AVRCP_EID_MEDIA_STATUS_CHANGED, 0);
        if (status != BT_STATUS_PENDING) {
            btdm_free(avrcp_cmd->parms);
            btdm_free(avrcp_cmd);
        }
    }
    if(event_mask & AVRCP_ENABLE_TRACK_CHANGED){
        ///注册歌曲改变事件，当TG播放状态改变时，产生AVRCP_EVENT_ADV_NOTIFY事件
        avrcp_cmd = (AvrcpAdvancedPdu *)btdm_malloc(sizeof(AvrcpAdvancedPdu));
        avrcp_cmd->parms = (uint8_t *)btdm_malloc(64);
        status = AVRCP_CtRegisterNotification(chnl, avrcp_cmd, AVRCP_EID_TRACK_CHANGED, 0);
        if (status != BT_STATUS_PENDING) {
            btdm_free(avrcp_cmd->parms);
            btdm_free(avrcp_cmd);
        }
    }
    if(event_mask & AVRCP_ENABLE_VOLUME_CHANGED){
        ///注册歌曲改变事件，当TG播放状态改变时，产生AVRCP_EVENT_ADV_NOTIFY事件
        avrcp_cmd = (AvrcpAdvancedPdu *)btdm_malloc(sizeof(AvrcpAdvancedPdu));
        avrcp_cmd->parms = (uint8_t *)btdm_malloc(64);
        status = AVRCP_CtRegisterNotification(chnl, avrcp_cmd, AVRCP_EID_VOLUME_CHANGED, 0);
        if (status != BT_STATUS_PENDING) {
            btdm_free(avrcp_cmd->parms);
            btdm_free(avrcp_cmd);
        }
    }
}
#endif

#if BTDM_STACK_ENABLE_HF | BTDM_STACK_ENABLE_AG
extern void CMGR_SetAudioVoiceSettings(BtScoAudioSettings settings);
/// 0--- not supported type, 1---cvsd, 2---msbc, 3---cvsd&msbc
static uint8_t bt_parse_hf_codec_type(uint8_t *data)
{
    uint8_t ret = 0;
    uint8_t index,len;
    len = strlen((void *)data);
    
    for(uint8_t i = 0; i < len; i++)
    {
        if((data[i] > '9') || (data[i] < '1')){
            continue;
        }
        if(ret == 0){
            if(data[i] == '1'){
                ret = 1;
            }
            else if(data[i] == '2'){
                ret = 2;
            }
            else{
                ret = 0;
                break;
            }
        }
        else if(ret == 1){
            if(data[i] == '2'){
                ret = 3;
            }
            else{
                ret = 0;
                break;
            }
        }
        else if(ret == 2){
            if(data[i] == '1'){
                ret = 3;
            }
            else{
                ret = 0;
                break;
            }
        }       
    }
    
    return ret;
}
#endif
/*      连接时只在第一次不作为信任设备的时候触发        */
static void security_event_handler(const BtEvent *event)
{
    //BtDeviceContext  *bdc;
    printf("entry security_event_handler");
    switch(event->eType) {
        case BTEVENT_PIN_REQ:
            SEC_SetPin(event->p.remDev, (const uint8_t *)"0000", 4, BPT_SAVE_TRUSTED);
            break;
        case BTEVENT_AUTHORIZATION_REQ:
            SEC_Authorize(event->p.remDev, TRUE, TRUE);
            break;
        //case BTEVENT_PAIRING_COMPLETE:
        //    bdc = DS_FindDevice(&event->p.pairingInfo.remDev->bdAddr);
        //    break;
        default:
            //co_printf("SecurityEventHandler: event--0x%02x is not implemented.\r\n", event->eType);
            break;
    }
}

static void me_callback(const BtEvent *event)
{
    LOG_INFO(NULL, "me_callback :%d,%d\r\n",event->eType,event->errCode);

    switch (event->eType) 
    {
        case BTEVENT_HCI_INITIALIZED:
            break;
        
        case BTEVENT_INQUIRY_CANCELED:
            //printf("inquiry canceled\r\n");
            bt_update_conn_status(BT_INFO_INQ_CANCEL, NULL, NULL);
            break;
        
        case BTEVENT_INQUIRY_RESULT:
//        {
//            uint8_t name[MAX_NAME_SIZE];
//            uint8_t len;
//            printf("inq result: %x\r\n",event->p.inqResult.classOfDevice);
//            printf("addr:0x%02x%02x%02x%02x%02x%02x\r\n",event->p.inqResult.bdAddr.A[0],event->p.inqResult.bdAddr.A[1],event->p.inqResult.bdAddr.A[2],
//                    event->p.inqResult.bdAddr.A[3],event->p.inqResult.bdAddr.A[4],event->p.inqResult.bdAddr.A[5]);
//            printf("rssi:%d,name: ",event->p.inqResult.rssi);
//            bt_parse_inq_result((uint8_t *)event->p.inqResult.extInqResp,name,&len);   
//            for(uint8_t i = 0; i < len; i++)
//            {
//                printf("%c",name[i]);
//            }
//            printf("\r\n");
//        }
            bt_update_conn_status(BT_INFO_INQ_RESULT, NULL, &event->p.inqResult);
            break;
        
        case BTEVENT_INQUIRY_COMPLETE:
            bt_update_conn_status(BT_INFO_INQ_COMP, NULL, NULL);
            //ME_Inquiry(BT_IAC_GIAC, 5, 5);
            break;
            /*  收到连接请求 */
        case BTEVENT_LINK_CONNECT_REQ:
            ME_AcceptIncomingLink(event->p.remDev, BCR_SLAVE);

            break;
        case BTEVENT_LINK_CONNECT_IND:
//            if(user_bt_env.bt_connect_cb){
//                user_bt_env.bt_connect_cb(BTEVENT_LINK_CONNECT_IND, &event->p.remDev->bdAddr, event->errCode);
//            }
            last_rem_dev = event->p.remDev;
            bt_connect_act_cmp(BT_EVENT_CON_IND, event->errCode, event->p.remDev);
            break;

        case BTEVENT_LINK_CONNECT_CNF:
//            if(user_bt_env.bt_connect_cb){
//                user_bt_env.bt_connect_cb(BTEVENT_LINK_CONNECT_CNF, &event->p.remDev->bdAddr, event->errCode);
//            }
            last_rem_dev = event->p.remDev;
            bt_connect_act_cmp(BT_EVENT_CON_CNF, event->errCode, event->p.remDev);
            break;
            
        case BTEVENT_LINK_DISCONNECT:
//            if(user_bt_env.bt_disconnect_cb){
//                user_bt_env.bt_disconnect_cb(&event->p.bdAddr, event->errCode);
//            }
            bt_connect_act_cmp(BT_EVENT_DISCONNECT, event->errCode, event->p.remDev);

            break;

        case BTEVENT_ACCESSIBLE_CHANGE:
            printf("access state = %d\r\n",event->p.aMode);
//            if(user_bt_env.bt_access_change_cb){
//                user_bt_env.bt_access_change_cb(event->p.aMode);
//            }
            bt_connect_act_cmp(BT_EVENT_ACC_CHG, event->p.aMode, NULL);
            break;

        case BTEVENT_MODE_CHANGE:
            if(event->p.modeChange.curMode == 0){
                printf("Active Mode.\r\n");
            }else{
                printf("Sniff Mode.\r\n");
            }
            break;
            
        case BTEVENT_ROLE_CHANGE:
            printf("role = %d\r\n",event->p.roleChange.newRole);
            break;
        case BTEVENT_HCI_COMMAND_SENT:
            break;
        default:
            break;
    }
}
#if BTDM_STACK_ENABLE_HF
static void hf_callback(HfChannel *Chan, HfCallbackParms *Info)
{
//    static bool scoPacketSent = true;

    if ((Info->event != HF_EVENT_AUDIO_DATA)
        && (Info->event != HF_EVENT_AUDIO_DATA_SENT)) {
        printf("hf_callback: event = %d.\r\n", Info->event);
    }

    switch(Info->event){
        case HF_EVENT_GATEWAY_FEATURES:
            //printf("gw feature: %x\r\n",Info->p.features);
//            if((Info->p.features & HFG_FEATURE_CODEC_NEGOTIATON) && (HF_GetSupportedFeature() & HF_FEATURE_CODEC_NEGOTIATION_CFG)){
//                printf("hf set voice setting: msbc\r\n");
//                CMGR_SetAudioVoiceSettings(0x63);
//            }else{
//                printf("hf set voice setting: cvsd\r\n");
//                CMGR_SetAudioVoiceSettings(0x60);
//            }
            break;
        case HF_EVENT_SERVICE_CONNECT_REQ:
            bt_update_conn_status(BT_PROFILE_HF_CONN_REQ, Chan, Info);
            break;
    
        case HF_EVENT_SERVICE_CONNECTED:
            LOG_INFO(NULL,"hf connected\r\n");
            //A2DP_OpenStream(&Stream[0], &Info->p.remDev->bdAddr);
            bt_update_conn_status(BT_PROFILE_HF_CONN, Chan, Info);
            break;
            
        case HF_EVENT_SERVICE_DISCONNECTED:
            LOG_INFO(NULL,"hf disconnected %d\r\n",Info->errCode);
            bt_update_conn_status(BT_PROFILE_HF_DISCONN, Chan, Info);
            break;

        case HF_EVENT_CALL_IND:
            bt_update_conn_status(BT_PROFILE_HF_CALL, Chan, Info);

            break;
            
        case HF_EVENT_CALLSETUP_IND:
            bt_update_conn_status(BT_PROFILE_HF_CALLSETUP, Chan, Info);
            break;
            
        case HF_EVENT_CALLHELD_IND:
            break;
            
        case HF_EVENT_RING_IND:
            bt_update_conn_status(BT_PROFILE_HF_RING, Chan, Info);
            break;
            
        case HF_EVENT_CALLER_ID_NOTIFY:
            break;
            
        case HF_EVENT_SPEAKER_VOLUME:
            bt_update_conn_status(BT_PROFILE_HF_SPK_VOL, Chan, Info);
            break;
        case HF_EVENT_CURRENT_CALL_STATE:
            bt_update_conn_status(BT_PROFILE_HF_CURRENT_CALL, Chan, Info);            
            break;
        case HF_EVENT_AT_RESULT_DATA:
            bt_update_conn_status(BT_PROFILE_HF_AT_RESULT, Chan, Info);
            break;
        case HF_EVENT_AUDIO_CONNECTED:
            bt_update_conn_status(BT_PROFILE_HF_AUDIO_CONN, Chan, Info);
            break;
            
        case HF_EVENT_AUDIO_DISCONNECTED:
            bt_update_conn_status(BT_PROFILE_HF_AUDIO_DISCONN, Chan, Info);
            break;
        
        case HF_EVENT_AUDIO_DATA:
            bt_update_conn_status(BT_PROFILE_HF_AUDIO_DATA, Chan, Info);
            break;
        
        case HF_EVENT_AUDIO_DATA_SENT:
//            scoPacketSent = true;
            btdm_free(Info->p.audioPacket->data);
            btdm_free(Info->p.audioPacket);
            break;

#if HF_CODEC_NEG == XA_ENABLED
        case HF_EVENT_CODEC_NEGOTIATION:
            {
                HfCommand *cmd;
//                if(Info->p.codecID == 1){
//                    //cvsd
//                    CMGR_SetAudioVoiceSettings(0x60);
//                }else if(Info->p.codecID == 2){
//                    //msbc
//                    CMGR_SetAudioVoiceSettings(0x63);
//                }
                BtStatus ret = BT_STATUS_NO_RESOURCES;
                cmd = (HfCommand *)btdm_malloc(sizeof(HfCommand));
                if(cmd != NULL){
                    hf_codec[0] = Info->p.codecID + '0';
                    hf_codec[1] = 0;
                    ret = HF_CodecConnectionSetup((HfChannel *)Chan, hf_codec, cmd);
                }
                if(ret != BT_STATUS_PENDING){
                    btdm_free((void *)cmd);
                }
            }
            break;
#endif

        case HF_EVENT_COMMAND_COMPLETE:
            btdm_free((uint8_t *)Info->p.command);
            break;
        
        default:
            break;
    }
}
#endif

#if BTDM_STACK_ENABLE_AG
static void hfg_callback(HfgChannel *Chan, HfgCallbackParms *Info)
{
    BtStatus status;
    HfgResponse *rsp;
    uint8_t codec_type;
    
    if ((Info->hfgEvent != HFG_EVENT_AUDIO_DATA)
        && (Info->hfgEvent != HFG_EVENT_AUDIO_DATA_SENT)) {
        LOG_INFO(NULL, "hfg_callback :%d\r\n",Info->hfgEvent);
    }
    switch (Info->hfgEvent) {
        case HFG_EVENT_HANDSFREE_FEATURES:
//            printf("remote hf feature: %x\r\n",Info->p.features);
//            if((Info->p.features & HF_FEATURE_CODEC_NEGOTIATION_CFG) && (HFG_GetSupportedFeature() & HFG_FEATURE_CODEC_NEGOTIATON)){
//                printf("hfg set voice setting: msbc\r\n");
//                CMGR_SetAudioVoiceSettings(0x63);
//            }else{
//                printf("hfg set voice setting: cvsd\r\n");
//                CMGR_SetAudioVoiceSettings(0x60);
//            }
            
            break;
        case HFG_EVENT_SERVICE_CONNECT_REQ:
            bt_update_conn_status(BT_PROFILE_HFG_CONN_REQ, Chan, Info);
            break;

        case HFG_EVENT_SERVICE_CONNECTED:
            bt_update_conn_status(BT_PROFILE_HFG_CONN, Chan, Info);
            break;
        case HFG_EVENT_SERVICE_DISCONNECTED:
            bt_update_conn_status(BT_PROFILE_HFG_DISCONN, Chan, Info);
            break;
        case HFG_EVENT_AUDIO_CONNECTED:
            {
                HfgResponse *rsp,*rsp1;
                rsp = (HfgResponse *)btdm_malloc(sizeof(HfgResponse));
                HFG_SetIndicatorValue(Chan, HFG_IND_CALL, TRUE, rsp);
                
                rsp1 = (HfgResponse *)btdm_malloc(sizeof(HfgResponse));
                HFG_SendMicVolume(Chan,0x08,rsp1);
                bt_update_conn_status(BT_PROFILE_HFG_AUDIO_CONN, Chan, Info);        
            }
            break;
        case HFG_EVENT_AUDIO_DISCONNECTED:
            {
                HfgResponse *rsp;
                rsp = (HfgResponse *)btdm_malloc(sizeof(HfgResponse));
                HFG_SetIndicatorValue(Chan, HFG_IND_CALL, FALSE, rsp);
                bt_update_conn_status(BT_PROFILE_HFG_AUDIO_DISCONN, Chan, Info);        
            }
            break;        
        case HFG_EVENT_AT_COMMAND_DATA:
            for(uint8_t i = 0; i < Info->p.data->dataLen; i++)
                printf("%c",Info->p.data->rawData[i]);
            printf("\r\n");
        
            rsp = (HfgResponse *)btdm_malloc(sizeof(HfgResponse));
            status = HFG_SendOK(Chan,rsp);
            if(status != BT_STATUS_PENDING){
                btdm_free((void *)rsp);
            }
//            if(memcmp(Info->p.data->rawData,"AT+IPHONEACCEV=1,1,9",sizeof("AT+IPHONEACCEV=1,1,9")) == 0)
//            {
//                status = BT_STATUS_NO_RESOURCES;
//                rsp = (HfgResponse *)btdm_malloc(sizeof(HfgResponse));
//                if(rsp != NULL){
//                    status = HFG_CreateCodecConnection(&hfg_channel[0], 1, rsp);
//                }
//                if(status != BT_STATUS_PENDING){
//                    btdm_free((void *)rsp);
//                }
//                printf("status = %d\r\n",status);
//            }
            break;
        case HFG_EVENT_SUPPORTED_CODEC:
//            codec_type = bt_parse_hf_codec_type(Info->p.ptr);
//            if(codec_type >= 2){
//                CMGR_SetAudioVoiceSettings(0x63);
//            }else{
//                CMGR_SetAudioVoiceSettings(0x60);
//            }
            break;
        case HFG_EVENT_LIST_CURRENT_CALLS:
            rsp = (HfgResponse *)btdm_malloc(sizeof(HfgResponse));
            uint8_t number[] = ""; 
            HfgCallListParms call_list_param = {1,0,0,0,0,(void *)number,0};
            if(Chan->call){
                //have call
                call_list_param.state = 0;
            }
            else{
                //no call
                call_list_param.state = 0xff;
            }
            status = HFG_SendCallListRsp(Chan,&call_list_param,rsp,TRUE);
            break;
        case HFG_EVENT_AUDIO_DATA:
            {

//                if(Info->p.audioData->len == 120){
//                    fputc('M',NULL);
//                }else if(Info->p.audioData->len == 60){
//                    fputc('m',NULL);
//                }
//                printf("audio data: ");
//                for(uint8_t i = 0; i < Info->p.audioData->len; i++)
//                {
//                    printf("%02x ",Info->p.audioData->data[i]);
//                }
//                printf("\r\n");
                bt_update_conn_status(BT_PROFILE_HFG_AUDIO_DATA, Chan, Info);                        
            }
            break;
        
        case HFG_EVENT_AUDIO_DATA_SENT:
            btdm_free((void *)Info->p.audioPacket->data);
            btdm_free((void *)Info->p.audioPacket);
            break;

        case HFG_EVENT_CODEC_CONNECTION_RSP:
            codec_type = bt_parse_hf_codec_type(Info->p.ptr);
            if(codec_type >= 2){
                CMGR_SetAudioVoiceSettings(0x63);
            }else{
                CMGR_SetAudioVoiceSettings(0x60);
            }
            HFG_CreateAudioLink(Chan,HFG_AR_LOCAL_USER_ACTION);
            break;
        case HFG_EVENT_RESPONSE_COMPLETE:
            
            btdm_free((void *)Info->p.response);
            break;

    }
}
#endif

#if BTDM_STACK_ENABLE_A2DP_SNK | BTDM_STACK_ENABLE_A2DP_SRC
static void a2dp_callback(A2dpStream *cbStream, const A2dpCallbackParms *Parms)
{
    AvdtpCodec       *codec;
    uint8_t               *elements;
    uint8_t               *reqElements;
    uint8_t error = A2DP_ERR_NO_ERROR;

    if((Parms->event != A2DP_EVENT_STREAM_DATA_IND ) && (Parms->event != A2DP_EVENT_STREAM_SBC_PACKET_SENT)) {
        LOG_INFO(NULL, "a2dp callback event=%d,%x,%x\r\n", Parms->event,Parms->error,Parms->discReason);
    }
    switch(Parms->event) {
        case A2DP_EVENT_STREAM_OPEN_IND:
            bt_update_conn_status(BT_PROFILE_A2DP_OPEN_IND, cbStream, Parms);
            if (AVDTP_CODEC_TYPE_SBC == Parms->p.configReq->codec.codecType) {
                codec = A2DP_GetRegisteredCodec(cbStream);
                elements = codec->elements;
                reqElements = Parms->p.configReq->codec.elements;
                printf("a2dp local codec: %x,%x,%x,%x\r\n",elements[0],elements[1],elements[2],elements[3]);
                printf("a2dp remote codec: %x,%x,%x,%x\r\n",reqElements[0],reqElements[1],reqElements[2],reqElements[3]);

                /* Check requested elements with registered elements */ 
                if (!(reqElements[0] & (elements[0] & 0xF0))) {
                    error = A2DP_ERR_NOT_SUPPORTED_SAMP_FREQ;
                } else if (!(reqElements[0] & (elements[0] & 0x0F))) {
                    error = A2DP_ERR_NOT_SUPPORTED_CHANNEL_MODE;
                } else if (!(reqElements[1] & (elements[1] & 0x0C))) {
                    error = A2DP_ERR_NOT_SUPPORTED_SUBBANDS;
                } else if (!(reqElements[1] & (elements[1] & 0x03))) {
                    error = A2DP_ERR_NOT_SUPPORTED_ALLOC_METHOD;
                } else if (reqElements[2] < elements[2]) {
                    error = A2DP_ERR_NOT_SUPPORTED_MIN_BITPOOL_VALUE;
                } else if (reqElements[3] > elements[3]) {
                    error = A2DP_ERR_NOT_SUPPORTED_MAX_BITPOOL_VALUE;
                }
                
//                if(error == A2DP_ERR_NO_ERROR){
//                    
//                    sbcinfo.bitPool = reqElements[3];
//                    
//                    if(reqElements[0]&0x20){
//                        sbcinfo.sampleFreq = 2;//44100
//                    }
//                    else{
//                        sbcinfo.sampleFreq = 3;//48000
//                    }
//                    
//                    if(reqElements[1]&0x01){
//                        sbcinfo.allocMethod = 0;//loudness
//                    }
//                    else{
//                        sbcinfo.allocMethod = 1;//snr
//                    }
//                }
//                codec->freq = (reqElements[0] & 0xF0) >> 5;
                A2DP_OpenStreamRspWithSinkDelay(cbStream, error, 
                                                         AVDTP_SRV_CAT_MEDIA_CODEC,
                                                         0);
            } 
            else if(AVDTP_CODEC_TYPE_MPEG2_4_AAC== Parms->p.configReq->codec.codecType) {
                codec = A2DP_GetRegisteredCodec(cbStream);
                elements = codec->elements;
                reqElements = Parms->p.configReq->codec.elements;    
                A2DP_OpenStreamRspWithSinkDelay(cbStream, error, 
                                                         AVDTP_SRV_CAT_MEDIA_CODEC,
                                                         0);
            }
            else {
                /* Refuse to accept incoming con to MP3 stream for now */
                A2DP_OpenStreamRspWithSinkDelay(cbStream, 
                                                         AVRCP_ERR_UNKNOWN_ERROR, 
                                                         AVDTP_SRV_CAT_MEDIA_CODEC,
                                                         0);
            }
            break;
        case A2DP_EVENT_CODEC_INFO:
            printf("codec info: %x,%x,%x,%x\r\n",Parms->p.codec->elements[0],Parms->p.codec->elements[1],Parms->p.codec->elements[2],Parms->p.codec->elements[3]);
            /* Found a matching codec */ 
            codec = A2DP_GetRegisteredCodec(cbStream);
            elements = codec->elements;
            
            /* Save the remote codec information.  Selection 
             * of capabilities will be made from the UI.  
             */ 
            cfgCodec.codecType = Parms->p.codec->codecType;
            cfgCodec.elemLen = Parms->p.codec->elemLen;
            
            cfgCodec.elements = cfgElements;
            
            ///config bitpool to MIN(earpods,local)
//            if (cfgCodec.codecType == AVDTP_CODEC_TYPE_SBC) {
//                if(cfgCodec.elements[3] > Parms->p.codec->elements[3]){
//                    cfgCodec.elements[3] = Parms->p.codec->elements[3];
//                }
//            }
        
//            if (cfgCodec.codecType == AVDTP_CODEC_TYPE_SBC) {
//            
//                /* Only the matching codec information 
//                 * elements can be selected.  
//                 */ 
//                cfgCodec.elements[0] 
//                    = (uint8_t)(Parms->p.codec->elements[0] & elements[0]);
//                cfgCodec.elements[1] 
//                    = (uint8_t)(Parms->p.codec->elements[1] & elements[1]);
//                cfgCodec.elements[2] 
//                    = (uint8_t)(max(Parms->p.codec->elements[2], elements[2]));
//                cfgCodec.elements[3] 
//                    = (uint8_t)(min(Parms->p.codec->elements[3], elements[3]));

//            }
            break;
        case A2DP_EVENT_GET_CONFIG_IND:
            LOG_INFO(NULL, "codec type=%d,%x,%x,%x,%x\r\n",cfgCodec.codecType,cfgCodec.elements[0],cfgCodec.elements[1],cfgCodec.elements[2],cfgCodec.elements[3]);
            
            /* Make sure something valid is configured for each field */ 
            if (cfgCodec.codecType == AVDTP_CODEC_TYPE_SBC) {
                A2DP_SetStreamConfigWithSinkDelay(cbStream, &cfgCodec, 0, 0);
            }
            else{
                A2DP_CloseStream(cbStream);
            }
            break;
        case A2DP_EVENT_DELAY_REPORT_IND:
            printf("delay:%d\r\n",Parms->p.delayMs);
            break;
        case A2DP_EVENT_STREAM_RECONFIG_CNF:
            printf("reconfig cnf:%x,%x,%x,%x,%x\r\n",Parms->error,Parms->p.configReq->codec.elements[0],Parms->p.configReq->codec.elements[1],Parms->p.configReq->codec.elements[2],Parms->p.configReq->codec.elements[3]);
            break;
        case A2DP_EVENT_STREAM_OPEN:
            LOG_INFO(NULL,"a2dp connected\r\n");
            bt_update_conn_status(BT_PROFILE_A2DP_CONN, cbStream, Parms);
            AvdtpConfigRequest *req = ((A2dpCallbackParms *)Parms)->p.configReq;
            cfgElements[0] = req->codec.elements[0];
            cfgElements[1] = req->codec.elements[1];
            cfgElements[2] = req->codec.elements[2];
            cfgElements[3] = req->codec.elements[3];
            break;
        case A2DP_EVENT_STREAM_START_IND:
            A2DP_StartStreamRsp(cbStream, A2DP_ERR_NO_ERROR);
            break;
        case A2DP_EVENT_STREAM_STARTED:
            bt_update_conn_status(BT_PROFILE_A2DP_PLAYING, cbStream, Parms);
            break;
        case A2DP_EVENT_STREAM_SUSPENDED:
            bt_update_conn_status(BT_PROFILE_A2DP_SUSPEND, cbStream, Parms);
            break;
        case A2DP_EVENT_STREAM_CLOSED:
            bt_update_conn_status(BT_PROFILE_A2DP_DISCONN, cbStream, Parms);
            break;
        case A2DP_EVENT_STREAM_DATA_IND:
            bt_update_conn_status(BT_PROFILE_A2DP_STREAM_DATA, cbStream, Parms);
            break;
        case A2DP_EVENT_STREAM_SBC_PACKET_SENT:
            bt_update_conn_status(BT_PROFILE_A2DP_SBC_PKT_SENT, cbStream, Parms);
            break;
        default:
            break;
    }
}
#endif

#if BTDM_STACK_ENABLE_AVRCP
static void avrcp_callback(AvrcpChannel *chnl, const AvrcpCallbackParms *Parms)
{
    AvrcpAdvancedPdu *avrcp_cmd;
    BtStatus status;

    LOG_INFO(NULL, "avrcp callback event=%d, status=%d, chnl=%x\r\n",Parms->event, Parms->status, chnl);


    switch (Parms->event) {
        case AVRCP_EVENT_CONNECT_IND:
            AVRCP_ConnectRsp(chnl,TRUE);
            break;
        case AVRCP_EVENT_CONNECT:
            {
                avrcp_cmd = btdm_malloc(sizeof(AvrcpAdvancedPdu));
                avrcp_cmd->parms = (uint8_t *)btdm_malloc(64);
                status = AVRCP_CtGetCapabilities((AvrcpChannel *)chnl, avrcp_cmd, AVRCP_CAPABILITY_EVENTS_SUPPORTED);
                if (status != BT_STATUS_PENDING) {
                    btdm_free(avrcp_cmd->parms);
                    btdm_free(avrcp_cmd);
                }
//                AVRCP_TgSetEventMask(chnl,AVRCP_ENABLE_VOLUME_CHANGED);
            }
            LOG_INFO(NULL, "avrcp connected\r\n");
            bt_update_conn_status(BT_PROFILE_AVRCP_CONN, chnl, Parms);
            break;
        case AVRCP_EVENT_DISCONNECT:
            LOG_INFO(NULL, "avrcp disconnected\r\n");
            bt_update_conn_status(BT_PROFILE_AVRCP_DISCONN, chnl, Parms);
            break;
        
        case AVRCP_EVENT_ADV_INFO:
            if(Parms->advOp == AVRCP_OP_SET_ABSOLUTE_VOLUME) {
                //LOG_INFO("SET_ABSOLUTE_VOLUME is %d.\r\n", event->param.adv.info.volume);
                printf("+VOL:%02x\r\n",Parms->p.adv.info.volume); 
            }
            break;
        case AVRCP_EVENT_ADV_RESPONSE:
            ///本地查询TG支持事件，返回消�?
            /**/
            if((Parms->advOp == AVRCP_OP_GET_CAPABILITIES)
                &&(Parms->p.adv.rsp.capability.type == AVRCP_CAPABILITY_EVENTS_SUPPORTED)){
                //printf("cap eventmaskk = %x\r\n",event->param.adv.rsp.capability.info.eventMask);
                uint16_t event_mask = Parms->p.adv.rsp.capability.info.eventMask;
                bt_update_conn_status(BT_PROFILE_AVRCP_EVENT_SUPPORT, chnl, &event_mask);
            }
                
            ///本地注册事件，返回的当前状�?
            if(Parms->advOp == AVRCP_OP_REGISTER_NOTIFY) {
                ///播放状态改变通知的当前状�?
                if(Parms->p.adv.notify.event == AVRCP_EID_MEDIA_STATUS_CHANGED) {

                }
                else if(Parms->p.adv.notify.event == AVRCP_EID_TRACK_CHANGED){
                    //user_bt_get_media_info();
                }
            }
            
            if(Parms->advOp == AVRCP_OP_GET_MEDIA_INFO){
                  bt_update_conn_status(BT_PROFILE_AVRCP_MEDIA_INFO, chnl, &Parms->p.adv.rsp.element);
//                uint16_t len0 = Parms->p.adv.rsp.element.txt[0].length;
//                uint16_t len1 = Parms->p.adv.rsp.element.txt[1].length;
//                printf("media info: %d,%s,%d,%s\r\n",len0,Parms->p.adv.rsp.element.txt[0].string,len1,Parms->p.adv.rsp.element.txt[1].string);
            }
            
            if(Parms->advOp == AVRCP_OP_GET_PLAY_STATUS){
                bt_update_conn_status(BT_PROFILE_AVRCP_MEDIA_POS, chnl, &Parms->p.adv.rsp.playStatus);
                //printf("media length:%d,pos:%d,status:%d\r\n",Parms->p.adv.rsp.playStatus.length,Parms->p.adv.rsp.playStatus.position,Parms->p.adv.rsp.playStatus.mediaStatus);
            }           

        break;        
        
        case AVRCP_EVENT_PANEL_CNF:
            if(Parms->p.panelCnf.press == TRUE){
                switch(Parms->p.panelCnf.operation) {
                    case AVRCP_POP_PAUSE:
                    AVRCP_SetPanelKey(chnl, AVRCP_POP_PAUSE, FALSE);
                    break;
                    case AVRCP_POP_PLAY:
                    AVRCP_SetPanelKey(chnl, AVRCP_POP_PLAY, FALSE);
                    break;
                    case AVRCP_POP_FORWARD:
                    AVRCP_SetPanelKey(chnl, AVRCP_POP_FORWARD, FALSE);
                    break;
                    case AVRCP_POP_BACKWARD:
                    AVRCP_SetPanelKey(chnl, AVRCP_POP_BACKWARD, FALSE);
                    break;
                    case AVRCP_POP_STOP:
                    AVRCP_SetPanelKey(chnl, AVRCP_POP_STOP, FALSE);
                    break;
                }
            }
        break;
            
        case AVRCP_EVENT_PANEL_PRESS:
            printf("panel press %x\r\n",Parms->p.panelInd.operation);
            break;
        case AVRCP_EVENT_PANEL_RELEASE:
            printf("panel release %x\r\n",Parms->p.panelInd.operation);
//            switch(Parms->p.panelInd.operation){
//                case AVRCP_POP_PAUSE:
//                    break;
//                case AVRCP_POP_PLAY:
//                    break;      
//                default:
//                    break;
//            }
            break;
            
        case AVRCP_EVENT_ADV_NOTIFY:
            {
                const AvrcpAdvNotifyParms *avrcp_ntf = &Parms->p.adv.notify;
                if(Parms->errorCode == AVRCP_ERR_NO_ERROR) {
                    switch(avrcp_ntf->event) {
                        case AVRCP_EID_TRACK_CHANGED:
                        { 
                            bt_update_conn_status(BT_PROFILE_AVRCP_TRACK_CHANGED, chnl, NULL);
                            avrcp_cmd = (AvrcpAdvancedPdu *)btdm_malloc(sizeof(AvrcpAdvancedPdu));
                            avrcp_cmd->parms = (uint8_t *)btdm_malloc(64);
                            status = AVRCP_CtRegisterNotification(chnl, avrcp_cmd, AVRCP_EID_TRACK_CHANGED, 0);
                            if (status != BT_STATUS_PENDING) {
                                btdm_free(avrcp_cmd->parms);
                                btdm_free(avrcp_cmd);
                            }                        
                        }
                        break;

                        case AVRCP_EID_MEDIA_STATUS_CHANGED:
                        {
                            ///重新注册播放状态改变通知消息
                            uint8_t media_status = Parms->p.adv.notify.p.mediaStatus;
                            bt_update_conn_status(BT_PROFILE_AVRCP_MEDIA_STATUS, chnl, &media_status);
                            
                            avrcp_cmd = (AvrcpAdvancedPdu *)btdm_malloc(sizeof(AvrcpAdvancedPdu));
                            avrcp_cmd->parms = (uint8_t *)btdm_malloc(64);
                            status = AVRCP_CtRegisterNotification(chnl, avrcp_cmd, AVRCP_EID_MEDIA_STATUS_CHANGED, 0);
                            if (status != BT_STATUS_PENDING) {
                                btdm_free(avrcp_cmd->parms);
                                btdm_free(avrcp_cmd);
                            }                        
                        }
                        break;     
                    }
                }
            }
            break;
        case AVRCP_EVENT_ADV_TX_DONE:
            btdm_free(Parms->p.adv.txPdu->parms);
            btdm_free(Parms->p.adv.txPdu);
            break;
        #if AVRCP_BROWSING_CONTROLLER == XA_ENABLED
        case AVRCP_EVENT_BROWSE_RESPONSE:
            printf("adv op:%d,%d\r\n",Parms->advOp,Parms->p.adv.browseRsp.fldrItems.numItems);
            printf("%x,%x,%x ",Parms->p.adv.browseRsp.fldrItems.list[30],Parms->p.adv.browseRsp.fldrItems.list[31],Parms->p.adv.browseRsp.fldrItems.list[32]);
            printf("\r\n");
            break;
        case AVRCP_EVENT_BROWSE_TX_DONE:
            printf("free %x\r\n",Parms->p.adv.txPdu->parms);
            btdm_free(Parms->p.adv.txPdu->parms);
            btdm_free(Parms->p.adv.txPdu);
            break;
        #endif
        default:
            break;
    }
}
#endif
#if BTDM_STACK_ENABLE_PBAP
static void pbap_callback(PbapClientCallbackParms *Parms)
{
    printf("pbap callback event = %d\r\n",Parms->event);
    switch (Parms->event) {
        case PBAP_EVENT_PARAMS_RX:
        {
            if ((Parms->oper == PBAPOP_PULL_PHONEBOOK) ||
                (Parms->oper == PBAPOP_PULL_VCARD_LISTING)) {
                    printf("miss calls: %d,pb size=%d\r\n",Parms->u.paramsRx.newMissedCalls,Parms->u.paramsRx.phonebookSize);
            }
        }
            break;
        case PBAP_EVENT_DATA_IND:
            bt_update_conn_status(BT_PROFILE_PBAP_DATA_IND, NULL, Parms);
            break;
        case PBAP_EVENT_TP_CONNECTED:
            bt_update_conn_status(BT_PROFILE_PBAP_CONN, NULL, Parms);
            break;
        case PBAP_EVENT_TP_DISCONNECTED:
            bt_update_conn_status(BT_PROFILE_PBAP_DISCONN, NULL, Parms);
            break;
        case PBAP_EVENT_CONTINUE:
            /* Always call continue to keep the commands flowing */
            PBAP_ClientContinue(Parms->client);
            break;
        case PBAP_EVENT_ABORTED:
            /* The requested operation was aborted. */
            break;

        case PBAP_EVENT_COMPLETE:
            bt_update_conn_status(BT_PROFILE_PBAP_COMP, NULL, Parms);
            break;
        default:
            break;
    }
}
#endif

#if BTDM_STACK_ENABLE_PAN
static void HandleFilters(PanUser *pan, uint16_t *range, uint16_t numRanges)
{
    BtStatus            status;
    uint8_t             i;
    PanControlRespPkt  *controlRespPkt;

    LOG_INFO(NULL, "Pan: Rx'd BNEP_CONTROL:NET_TYPE_SET %d Filter ranges\r\n", numRanges);
    for (i = 0; i < numRanges*2; i+=2) {
        LOG_INFO(NULL, "   ==>Range start: %#.4x\r\n", BEtoHost16((uint8_t *)&range[i]));
        LOG_INFO(NULL, "   ==>Range end: %#.4x\r\n", BEtoHost16((uint8_t *)&range[i+1]));
    }

    controlRespPkt = pvPortMalloc(sizeof(PanControlRespPkt));
    if (controlRespPkt == 0) {
        LOG_INFO(NULL, "PanApp: No Resources\r\n");
        return;
    }

    /* Set the response message */
    StoreBE16(controlRespPkt->message, BNEP_FR_OPERATION_SUCCESS);

    /* Send the Control Response packet */
    status = PAN_ControlResponse(pan, BNEP_FILTER_NET_TYPE_RESPONSE_MSG, controlRespPkt);
    LOG_INFO(NULL, "PanApp: PAN_ControlResponse() Returned 0x%04x\r\n", status);
    if (status != BT_STATUS_PENDING) {
        vPortFree(controlRespPkt);
    }
}

static void HandleAddrs(PanUser *pan, BNEP_ADDR *addr, uint16_t numRanges)
{
    BtStatus            status;
    uint8_t             i;
    PanControlRespPkt  *controlRespPkt;
    uint8_t             buffer[40];

    LOG_INFO(NULL, "Pan: Rx'd BNEP_CONTROL:MULTI_ADDR_SET %d Address ranges\r\n", numRanges);
    for (i = 0; i < numRanges*2; i+=2) {
        bnep_addr_ntoa((BNEP_ADDR *)(&addr[i]), (char *)buffer);
        LOG_INFO(NULL, "   ==>Address start: %s\r\n", buffer);
        bnep_addr_ntoa((BNEP_ADDR *)(&addr[i+1]), (char *)buffer);
        LOG_INFO(NULL, "   ==>Address end: %s\r\n", buffer);
    }

    controlRespPkt = pvPortMalloc(sizeof(PanControlRespPkt));
    if (controlRespPkt == 0) {
        LOG_INFO(NULL, "PanApp: No Resources\r\n");
        return;
    }

    /* Set the response message */
    StoreBE16(controlRespPkt->message, BNEP_FR_OPERATION_SUCCESS);

    /* Send the Control Response packet */
    status = PAN_ControlResponse(pan, BNEP_FILTER_MULTI_ADDR_RESPONSE_MSG, controlRespPkt);
    LOG_INFO(NULL, "PanApp: PAN_ControlResponse() Returned 0x%04x\r\n", status);
    if (status != BT_STATUS_PENDING) {
        vPortFree(controlRespPkt);
    }
}

/* ---------------------------------------------------------------------------
 *                      PAN PANU Packet handler
 * ---------------------------------------------------------------------------
 */
static void HandlePanuPacket(PanCallbackParms *Info)
{
    bool DisplayUnknownPackets = true;
    uint16_t    bodyLen, ipLen, ipTotLen;
    BtStatus    status;
    ArpHeader  *arpHdrIn;
    IpHeader   *ipHdr;
    UdpHeader  *udpHdr;
    DhcpHeader *dhcpHdr;
    IcmpHeader *icmpHdr;
    PanPacket  *panPkt = 0;
    BNEP_ADDR  *macAddr = 0;
    uint8_t     buff[1024];
    uint32_t    myIpAddr, dstIP;
    PanPacketType panPktType;

    switch (Info->type) {
    case PANPKT_ARP_REQUEST:
        LOG_INFO(NULL, "Received ARP Request\r\n");
        arpHdrIn = (ArpHeader *)(Info->pkt + ETHER_HDR_SIZE);
        /* The reply is already set up. Just forward it. */
        PAN_ForwardPkt(Info->pan, Info->pkt, Info->rxPktLen);
        break;

    case PANPKT_ARP_REPLY:
        LOG_INFO(NULL, "Received ARP Reply\r\n");
        arpHdrIn = (ArpHeader *)Info->pkt;
        myIpAddr = PAN_GetLocalIpAddr_v2();
        if (myIpAddr == BEtoHost32((uint8_t *)&arpHdrIn->dstIP) &&
            PAN_GetRemoteIpAddr_v2(Info->pan) == BEtoHost32((uint8_t *)&arpHdrIn->srcIP)) {
            /* This is a response to my arp request */
            PAN_SetRemoteMacAddr(Info->pan, arpHdrIn->srcMac);
            LOG_INFO(NULL, "Destination MAC address = %02x:%02x:%02x:%02x:%02x:%02x\r\n", 
                arpHdrIn->srcMac[0], arpHdrIn->srcMac[1], arpHdrIn->srcMac[2],
                arpHdrIn->srcMac[3], arpHdrIn->srcMac[4], arpHdrIn->srcMac[5]);
        }
        break;

    case PANPKT_IP:
        if (DisplayUnknownPackets == true) {
            LOG_INFO(NULL, "Received IPv4 Packet\r\n");
//            SOCKADDR_IN from;
//            ipHdr = (IpHeader *)Info->pkt;
//            memset((uint8_t*)&from, 0, sizeof(from));
//            from.sin_addr.S_un.S_addr = ipHdr->srcIP;
//            from.sin_family = AF_INET;
//            from.sin_port = 0;
//            LOG_INFO(NULL, "Remote IP address: %s, protocol: %d, Length: %d\r\n",
//                inet_ntoa(from.sin_addr), ipHdr->proto, ipHdr->totalLen);
        }
        break;

    case PANPKT_TCP:
        if (DisplayUnknownPackets == true) {
            LOG_INFO(NULL, "Received TCP Packet\r\n");
            ipHdr = (IpHeader *)Info->pkt;
        }
        break;

    case PANPKT_UDP:
        if (DisplayUnknownPackets == true) {
            LOG_INFO(NULL, "Received UDP Packet\r\n");
            ipHdr = (IpHeader *)Info->pkt;
            udpHdr = (UdpHeader *)((uint8_t *)ipHdr + sizeof(IpHeader));
        }
        break;

    case PANPKT_DHCP_REQUEST:
        LOG_INFO(NULL, "Received DHCP Request Packet\r\n");
        LOG_INFO(NULL, "    Not valid for a PANU!\r\n");
        break;

    case PANPKT_DHCP_REPLY:
        /* This is for me */
        {
            LOG_INFO(NULL, "Received my DHCP Reply Packet\r\n");
//            SOCKADDR_IN sAddr = {AF_INET, 0, 0, 0};
//            dhcpHdr = (DhcpHeader *)(Info->pkt + sizeof(IpHeader) + sizeof(UdpHeader));
//            if (dhcpHdr->options[2] == DHCPOFFER) {
//                sAddr.sin_addr.S_un.S_addr = dhcpHdr->yiaddr;
//                LOG_INFO(NULL, "DHCP server offered IP address %s\r\n", inet_ntoa(sAddr.sin_addr));

//                /* Set the device addresses used to make the DHCPREQUEST packet */
//                PAN_SetLocalIpAddr_v2(BEtoHost32((uint8_t *)&dhcpHdr->yiaddr));
//                PAN_SetDhcpAddr_v2(BEtoHost32((uint8_t *)&dhcpHdr->siaddr));
//                bodyLen = MakeDhcpPacket(INADDR_BROADCAST, &panPkt, BOOTP_CLIENT, DHCPREQUEST);
//                status = PAN_SendPkt(Info->pan, panPkt, PANPKT_DHCP_REQUEST, bodyLen);
//                LOG_INFO(NULL, "PanApp: PAN_SendPkt() Returned 0x%04x\r\n", status);
//            }
//            else if (dhcpHdr->options[2] == DHCPACK) {
//                sAddr.sin_addr.S_un.S_addr = dhcpHdr->yiaddr;
//                LOG_INFO(NULL, "DHCP server ACK'd IP address %s\r\n", inet_ntoa(sAddr.sin_addr));
//            }
        }
        break;

    case PANPKT_ETHER_PKT:
        PAN_ForwardPkt(Info->pan, Info->pkt, Info->rxPktLen);
        break;

    default:
        LOG_INFO(NULL, "PanApp: Rx'd unrecognized packet type %d\r\n", Info->type);
        break;
    }
}

void pan_callback(PanCallbackParms *Params)
{
    BtStatus            status;
    PanPacket          *panPkt = 0;
    ArpHeader          *arpHdr;
    uint16_t            bodyLen;
    PanSetupConnPkt    *setupPkt;
    PanControlRespPkt  *setupRespPkt;
    uint16_t            numRanges;
    PanPacketType       panPktType;

    switch (Params->event) {
    case PANEVENT_OPEN:
        LOG_INFO(NULL, "PanApp: Received PANEVENT_OPEN event\r\n");

#if 1
        BD_ADDR addr;
        BNEP_ADDR addr_local, addr_remote;

        ME_ReadLocalBdAddr(&addr);
        memcpy(&addr_local.addr[0], &addr.A[0], BNEP_ADDR_SIZE);
        addr_remote = Params->pan->bnepPanu.bnepAddr;

        /* notify LWIP data link layer is established */
        bnep_lwip_netif_up(Params->pan, &addr_local, &addr_remote);
#else
        /* ARP to verify my Link Local address */
        panPktType = PANPKT_ARP_REQUEST;
        bodyLen = sizeof(ArpHeader);
        panPkt = pvPortMalloc(sizeof(PanPacket) + bodyLen);
        arpHdr = (ArpHeader *)panPkt->body;

        /* Set the target address to my initial Link Local address */
        PAN_MakeArpHdr(arpHdr, PAN_GetLocalIpAddr_v2());

        status = PAN_SendPkt(Params->pan, panPkt, panPktType, bodyLen);
        if (status != BT_STATUS_PENDING) {
            LOG_INFO(NULL, "PanApp: Send failed\r\n");
            vPortFree(panPkt);
            return;
        }
        LOG_INFO(NULL, "PanApp: Sent ARP to validate Link Local Address\r\n");
#endif
        break;

    case PANEVENT_SETUP_CONNECTION:
        LOG_INFO(NULL, "PanApp: Received PANEVENT_SETUP_CONNECTION event\r\n");
        /* Complete the open with a Setup Connection Request */
        setupPkt = pvPortMalloc(sizeof(PanSetupConnPkt));
        if (setupPkt == 0) {
            LOG_INFO(NULL, "PanApp: No Resources");
            return;
        }

        status = PAN_SetupConnection(Params->pan, setupPkt);
        LOG_INFO(NULL, "PanApp: PAN_SetupConnection() Returned 0x%04x\r\n", status);
        if (status != BT_STATUS_PENDING) {
            vPortFree(setupPkt);
        }
        break;

    case PANEVENT_SETUP_CONN_REQ:
        LOG_INFO(NULL, "PanApp: Received PANEVENT_SETUP_CONN_REQ event\r\n");
        /* Complete the open with a Setup Connection Response */
        setupRespPkt = pvPortMalloc(sizeof(PanControlRespPkt));
        if (setupRespPkt == 0) {
            LOG_INFO(NULL, "PanApp: No Resources\r\n");
            return;
        }

        /* The setup connection message has already been validated. The
         * status parameter contains the validation results of type
         * BnepSetupResponseMsg. Just copy the status into my response
         * and send it back.
         */
        StoreBE16(setupRespPkt->message, Params->status);
        status = PAN_ControlResponse(Params->pan, 
            BNEP_SETUP_CONNECTION_RESPONSE_MSG, setupRespPkt);
        LOG_INFO(NULL, "PanApp: PAN_ControlResponse() Returned 0x%04x\r\n", status);
        if (status != BT_STATUS_PENDING) {
            btdm_free(setupRespPkt);
        }
        break;

    case PANEVENT_CONTROL_IND:
        switch (Params->pkt[1]) {
        case BNEP_FILTER_NET_TYPE_SET_MSG:
            numRanges = Params->rxPktLen - BNEP_CONTROL_HDR_SIZE;
            numRanges /= ETHER_RANGE_SIZE;
            HandleFilters(Params->pan, (uint16_t *)(Params->pkt + 2), numRanges);
            break;

        case BNEP_FILTER_NET_TYPE_RESPONSE_MSG:
            LOG_INFO(NULL, "PanApp: Received FILTER_NET_TYPE_RESPONSE event\r\n");
            break;

        case BNEP_FILTER_MULTI_ADDR_SET_MSG:
            numRanges = Params->rxPktLen - BNEP_CONTROL_HDR_SIZE;
            numRanges /= BNEP_ADDR_SIZE * 2;
            HandleAddrs(Params->pan, (BNEP_ADDR *)(Params->pkt + 2), numRanges);
            break;

        case BNEP_FILTER_MULTI_ADDR_RESPONSE_MSG:
            LOG_INFO(NULL, "PanApp: Received MULTI_ADDR_RESPONSE event\r\n");
            break;

        default:
            break;
        }
        break;

    case PANEVENT_CLOSED:
        LOG_INFO(NULL, "PanApp: Received PANEVENT_CLOSED event\r\n");
        bnep_lwip_netif_down();
        break;

    case PANEVENT_DATA_IND:
        LOG_INFO(NULL, "PanApp: Received PANEVENT_DATA_IND event\r\n");
        HandlePanuPacket(Params);
        break;

    case PANEVENT_DATA_HANDLED:
        LOG_INFO(NULL, "PanApp: Received PANEVENT_DATA_HANDLED event\r\n");
        vPortFree(Params->pkt);
        break;

    case PANEVENT_SDP_FAILED:
        LOG_INFO(NULL, "PanApp: Received PANEVENT_SDP_FAILED event\r\n");
        break;

    default:
        LOG_INFO(NULL, "PanApp: Received Unknown Event %d with status 0x%04x\r\n", Params->event,
                                                            Params->status);
        break;
    }
}
#endif

#if BTDM_STACK_ENABLE_SPP
void spp_callback(SppDev *locDev, SppCallbackParms *Info)
{
    printf("spp callback, event = %d\r\n",Info->event);
    switch(Info->event) {
        case SPP_EVENT_REMDEV_CONNECTED:
            bt_update_conn_status(BT_PROFILE_SPP_CONN, locDev, Info);
            printf("spp connected\r\n");
            user_bt_env.dev[user_bt_env.last_active_index].spp_dev = locDev;
            break;
        case SPP_EVENT_REMDEV_DISCONNECTED:
            bt_update_conn_status(BT_PROFILE_SPP_DISCONN, locDev, Info);
            printf("spp disconnected\r\n");
            break;
        case SPP_EVENT_DATA_IND:
            printf("spp recv: ");
            for(uint8_t i = 0; i < Info->p.pkt->dataLen; i++)
            {
                printf("%c",Info->p.pkt->data[i]);
            }
            //增加spp_send的测试,spp_send是可行的
            //spp_send(user_bt_env.dev[user_bt_env.last_active_index].spp_dev, &spp_send_test, 1);
            printf("\r\n");
            break;
        case SPP_EVENT_SEND_COMPLETE:
//            btdm_free(Info->p.pkt->data);
//            btdm_free((uint8_t *)Info->p.pkt);
            break;
    }

}
#endif

#if BTDM_STACK_ENABLE_MAP
void map_callback(MapClientCallbackParms *Parms)
{
    printf("map client cb: %d\r\n",Parms->event);
    switch(Parms->event){
        case MAP_EVENT_TP_CONNECTED:
            break;
        case MAP_EVENT_TP_DISCONNECTED:
            printf("disconnect reason %d\r\n",Parms->u.discReason);
            break;
        case MAP_EVENT_CONNECTED:
            printf("map connected\r\n");
            break;
        case MAP_EVENT_PARAMS_RX:
            printf("map rarams rx: %d,%d,%d\r\n",Parms->u.paramsRx.listing_size,Parms->u.paramsRx.new_msg,Parms->u.paramsRx.mse_time_len);
            printf("time: %s\r\n",Parms->u.paramsRx.mse_time);
            break;
        case MAP_EVENT_CONTINUE:
            /* Always call continue to keep the commands flowing */
            MAP_ClientContinue(Parms->client);
            break;
        case MAP_EVENT_DATA_IND:
            printf("map data len: %d\r\n",Parms->u.dataInd.len);
            for(uint16_t i=0;i<Parms->u.dataInd.len;i++)
                printf("%c", Parms->u.dataInd.buffer[i]);
            printf("\r\n");
            break;
        default:
            break;
    }
}
#endif

#if BTDM_STACK_ENABLE_HID
void hid_callback(HidChannel *Channel, HidCallbackParms *event)
{
    //printf("hid_callback:%d,%x\r\n",event->event,Channel->cmgrHandler.flags);
    switch (event->event) {
        case  HIDEVENT_OPEN_IND:
            HID_AcceptConnection(Channel);
            break;
        case  HIDEVENT_OPEN:
            printf("Channel opened\r\n");
            bt_update_conn_status(BT_PROFILE_HID_CONN, Channel, event);
            break;
        case  HIDEVENT_CLOSE_IND:
            break;
        case HIDEVENT_CLOSED:
            bt_update_conn_status(BT_PROFILE_HID_DISCONN, Channel, event);
            break;
        case  HIDEVENT_TRANSACTION_IND:
            {
                HidTransaction *Trans;
                
                Trans = (void *)btdm_malloc(sizeof(HidTransaction));
                memcpy((void *)Trans,event->ptrs.trans,sizeof(HidTransaction));
                
                Trans->resultCode = HID_RESULT_SUCCESS;
                Trans->type = HID_TRANS_SET_PROTOCOL_RSP; 
                HID_DeviceSetProtocolRsp(Channel, Trans);
            }            
            break;
        case  HIDEVENT_TRANSACTION_RSP:
            break;
        case  HIDEVENT_TRANSACTION_COMPLETE:
            btdm_free((void *)(event->ptrs.trans));
            break;
        case  HIDEVENT_INTERRUPT:
            break;
        case  HIDEVENT_INTERRUPT_COMPLETE:
            btdm_free((void*)event->ptrs.intr->data);
            btdm_free((event->ptrs.intr));
            break;
        
        default:
        break;
                 
    }
}
#endif
//void app_btdm_struct_size_dump(void)
//{
//    printf("BTDM STRUCT SIZE: defined in stack.\r\n");
//    extern void btdm_struct_size_dump(void);
//    btdm_struct_size_dump();
//    
//    printf("BTDM STRUCT SIZE: defined in api.\r\n");
//    printf("BtEvent is %d\r\n", sizeof(BtEvent));
//    printf("BtRemoteDevice is %d\r\n", sizeof(BtRemoteDevice));

//    printf("RfDeferEvent is %d\r\n", sizeof(RfDeferEvent));
//    printf("RfChannel is %d\r\n", sizeof(RfChannel));

//    printf("HfChannel is %d\r\n", sizeof(HfChannel));
//    printf("HfCommand is %d\r\n", sizeof(HfCommand));
//    printf("HfCallbackParms is %d\r\n", sizeof(HfCallbackParms));
//    printf("HfgChannel is %d\r\n", sizeof(HfgChannel));
//    printf("HfgCallbackParms is %d\r\n", sizeof(HfgCallbackParms));

//    printf("A2dpStream is %d\r\n", sizeof(A2dpStream));
//    printf("A2dpCallbackParms is %d\r\n", sizeof(A2dpCallbackParms));

//    printf("AvdtpStream is %d\r\n", sizeof(AvdtpStream));
//    printf("AvdtpConn is %d\r\n", sizeof(AvdtpConn));
//    printf("AvdtpCallbackParms is %d\r\n", sizeof(AvdtpCallbackParms));

//    printf("AvctpCallbackParms is %d\r\n", sizeof(AvctpCallbackParms));
//    printf("AvctpChannel is %d\r\n", sizeof(AvctpChannel));
//    printf("AvctpCmdFrame is %d\r\n", sizeof(AvctpCmdFrame));
//    printf("AvctpRspFrame is %d\r\n", sizeof(AvctpRspFrame));

//    printf("AvrcpChannel is %d\r\n", sizeof(AvrcpChannel));
//    printf("AvrcpCallbackParms is %d\r\n", sizeof(AvrcpCallbackParms));
//    printf("AvrcpAdvancedPdu is %d\r\n", sizeof(AvrcpAdvancedPdu));
//    printf("AvrcpPlayerSetting is %d\r\n", sizeof(AvrcpPlayerSetting));
//    printf("AvrcpPlayerStrings is %d\r\n", sizeof(AvrcpPlayerStrings));
//    printf("AvrcpEqString is %d\r\n", sizeof(AvrcpEqString));
//    printf("AvrcpRepeatString is %d\r\n", sizeof(AvrcpRepeatString));
//    printf("AvrcpShuffleString is %d\r\n", sizeof(AvrcpShuffleString));
//    printf("AvrcpScanString is %d\r\n", sizeof(AvrcpScanString));
//    printf("AvrcpExtString is %d\r\n", sizeof(AvrcpExtString));
//    printf("AvrcpCharSets is %d\r\n", sizeof(AvrcpCharSets));
//    printf("AvrcpMediaInfo is %d\r\n", sizeof(AvrcpMediaInfo));
//    printf("AvrcpMediaPlayStatus is %d\r\n", sizeof(AvrcpMediaPlayStatus));
//    printf("AvrcpTrackStruct is %d\r\n", sizeof(AvrcpTrackStruct));
//}

#if BTDM_STACK_ENABLE_HF | BTDM_STACK_ENABLE_AG
void app_bt_send_sco_data(void *channel, uint8_t seq, uint8_t *data, uint16_t length)
{
    BtPacket *sco_packet;
    void *Chan = channel;
    uint16_t data_length;
    uint8_t sub_seq = 0;
    uint8_t index;
    BtStatus status = BT_STATUS_SUCCESS;
    
    sco_packet = btdm_malloc(sizeof(BtPacket));
    if (sco_packet) {
        memset((void *)sco_packet, 0, sizeof(BtPacket));

        if (length == 57) {
            data_length = 60;
            sco_packet->data = btdm_malloc(data_length);
            sco_packet->data[0] = 0x01;
            if (seq & 0x01) {
                sub_seq = 0x30;
            }
            if (seq & 0x02) {
                sub_seq |= 0xc0;
            }
            sco_packet->data[1] = 0x08 | sub_seq;
            memcpy(&sco_packet->data[2], data, length);
            sco_packet->data[59] = 0x00;
            sco_packet->dataLen = data_length;
        }
        else {
            sco_packet->data = btdm_malloc(length);
            memcpy(sco_packet->data, data, length);
            sco_packet->dataLen = length;
        }
        for(index = 0; index < NUM_BT_DEVICES; index++)
        {
            #if BTDM_STACK_ENABLE_AG
            if((uint32_t)&hfg_channel[index] == (uint32_t)channel){
                status = HFG_SendAudioData((HfgChannel *)Chan, sco_packet);
                break;
            }
            #endif
            #if BTDM_STACK_ENABLE_HF
            if((uint32_t)&hf_channel[index] == (uint32_t)channel){
                status = HF_SendAudioData((HfChannel *)Chan, sco_packet);
                break;
            }
            #endif
        }
        if((index == NUM_BT_DEVICES) || (status != BT_STATUS_PENDING)){
            btdm_free(sco_packet->data);
            btdm_free(sco_packet);
        }
    }
}
#endif
void app_bt_init(void)
{
//    app_btdm_struct_size_dump();
    #define EIR_DATA_SIZE 50
    uint8_t eir_data[EIR_DATA_SIZE];
    uint8_t index = 0;
    uint8_t rand_num[4];
    uint8_t hex_2_char[] = {'0', '1', '2','3', '4', '5','6', '7', '8','9', 'a', 'b','c', 'd', 'e', 'f'};

#if BTDM_STACK_ENABLE_HF
    hf_channel = btdm_malloc(sizeof(HfChannel) * NUM_BT_DEVICES);
#endif
#if BTDM_STACK_ENABLE_AG
    hfg_channel = btdm_malloc(sizeof(HfgChannel) * NUM_BT_DEVICES);
#endif
#if BTDM_STACK_ENABLE_A2DP_SNK |BTDM_STACK_ENABLE_A2DP_SRC
    Stream = btdm_malloc(sizeof(A2dpStream) * NUM_STREAMS);
#endif
#if BTDM_STACK_ENABLE_AVRCP
    rcpCtChannel = btdm_malloc(sizeof(AvrcpChannel) * NUM_BT_DEVICES);
#endif
#if BTDM_STACK_ENABLE_PBAP    
    pbap_client = btdm_malloc(sizeof(PbapClientSession) * NUM_BT_DEVICES);
#endif
#if BTDM_STACK_ENABLE_SPP
    spp_dev = btdm_malloc(sizeof(SppDev) * NUM_BT_DEVICES);
    /*给spp_dev申请一块内存*/
#endif    
#if BTDM_STACK_ENABLE_MAP    
    map_client = btdm_malloc(sizeof(MapClientSession) * NUM_BT_DEVICES);
    memset((void *)map_client, 0, sizeof(MapClientSession) * NUM_BT_DEVICES);
#endif    
#if BTDM_STACK_ENABLE_HID
    hid_channel = btdm_malloc(sizeof(HidChannel) * NUM_BT_DEVICES);
#endif  

    ME_InitHandler(&securityHandler);
    securityHandler.callback = security_event_handler;  //发生配对，安全验证，授权之类的事情交给此回调函数
    SEC_RegisterPairingHandler(&securityHandler);
    SEC_RegisterAuthorizeHandler(&securityHandler);
    /*手机配对、输入 PIN、是否允许连接某个 Profile 等，都会进 security_event_handler*/
    /* Set our promiscuous link policies */
    ME_SetDefaultLinkPolicy(((BLP_MASTER_SLAVE_SWITCH|BLP_SNIFF_MODE)&(~BLP_MASK)), 
                            ((BLP_MASTER_SLAVE_SWITCH|BLP_SNIFF_MODE)&(~BLP_MASK)));
#if BT_ADDR_RANDOM_ENABLE                                        
    if(flashdb_get(FDB_KEY_RANDOM_SEED, (void *)&rand_num[0], 4) != 0){
        default_name[7] = hex_2_char[(rand_num[1]&0xf0) >> 4];
        default_name[8] = hex_2_char[(rand_num[1]&0x0f)];
        default_name[9] = hex_2_char[(rand_num[0]&0xf0) >> 4];
        default_name[10] = hex_2_char[(rand_num[0]&0x0f)];
    }
#endif

    ME_SetLocalDeviceName((const uint8_t *)default_name, sizeof(default_name));
    memset(&eir_data[0],0,EIR_DATA_SIZE);
    eir_data[index++] = sizeof(default_name) + 1;
    eir_data[index++] = 0x09;
    memcpy(&(eir_data[index]),default_name,sizeof(default_name));
    ME_SetExtInquiryRsp(0x01,eir_data,sizeof(default_name)+1);

    SEC_SetBondingMode(GENERAL_BONDING);
    SEC_SetIoCapabilities(IO_NO_IO);


    /*  */
    ME_InitHandler(&(globalHandler));
    globalHandler.callback = me_callback;
    ME_RegisterGlobalHandler(&globalHandler);
//    ME_SetEventMask(&globalHandler, (BEM_ACCESSIBLE_CHANGE  |
//                                        BEM_LINK_CONNECT_CNF   |
//                                        BEM_LINK_CONNECT_IND   |
//                                        BEM_LINK_DISCONNECT    |
//                                        BEM_ROLE_CHANGE     |
//                                        BEM_MODE_CHANGE     |
//                                        BEM_INQUIRY_COMPLETE    |
//                                        BEM_INQUIRY_RESULT  |
//                                        BEM_INQUIRY_CANCELED));
    ME_SetEventMask(&globalHandler, BEM_ALL_EVENTS);

    ME_RegisterAcceptHandler(&globalHandler);  

    secParms.level = (BSL_SECURITY_L2_IN | BSL_SECURITY_L2_OUT);//HF_SECURITY_SETTINGS;
    secParms.pinLen = 4;
    
#if BTDM_STACK_ENABLE_HF
    HF_SetSupportedFeature(HF_SDK_FEATURES);
    for (uint8_t i=0; i<NUM_BT_DEVICES; i++) {
        HF_RegisterSec(&hf_channel[i], hf_callback, &secParms);
    }
#endif
    /*  配置为蓝牙音频设备  */
    //ME_SetClassOfDevice( COD_AUDIO 
    //                   | COD_MAJOR_AUDIO 
    //                       | COD_MINOR_AUDIO_HANDSFREE);
                       
#if BTDM_STACK_ENABLE_AG
    HFG_SetSupportedFeature(HFG_SDK_FEATURES);
    for (uint8_t i=0; i<NUM_BT_DEVICES; i++) {
        HFG_RegisterSec(&hfg_channel[i], hfg_callback, &secParms);
    }
#endif

#if BTDM_STACK_ENABLE_A2DP_SNK
    /* Register SBC stream sink */ 
    sbcSnkCodec1.codecType = AVDTP_CODEC_TYPE_SBC;
    sbcSnkCodec1.elemLen = sizeof(SbcSnkElements);
    sbcSnkCodec1.elements = SbcSnkElements;

#if BTDM_STACK_ENABLE_AAC
    aacSnkCodec1.codecType = AVDTP_CODEC_TYPE_MPEG2_4_AAC;
    aacSnkCodec1.elemLen = sizeof(AacSnkElements);
    aacSnkCodec1.elements = AacSnkElements;
#else
    sbcSnkCodec2.codecType = AVDTP_CODEC_TYPE_SBC;
    sbcSnkCodec2.elemLen = sizeof(SbcSnkElements);
    sbcSnkCodec2.elements = SbcSnkElements;
#endif
    ///initialize codec parameters,sample rate 44.1k, bitpool 53
    cfgElements[0] = 0x21;
    cfgElements[1] = 0x15;
    cfgElements[2] = 0x02;
    cfgElements[3] = 0x35;
            
    Stream[0].type = A2DP_STREAM_TYPE_SINK;
    Stream[1].type = A2DP_STREAM_TYPE_SINK;
    A2DP_Register(&Stream[0], &sbcSnkCodec1, a2dp_callback);
#if BTDM_STACK_ENABLE_AAC
    A2DP_Register(&Stream[1], &aacSnkCodec1, a2dp_callback);
#else
    A2DP_Register(&Stream[1], &sbcSnkCodec2, a2dp_callback);
#endif
#endif
    
#if BTDM_STACK_ENABLE_PAN
    PanService pan_service;
    pan_service.type = 0x1115;  // PANU
    pan_service.callback = pan_callback;
    PAN_Register(&pan_service);
    bnep_lwip_init();
#endif

#if BTDM_STACK_ENABLE_A2DP_SRC
    /* Register SBC stream src */ 
    sbcSrcCodec1.codecType = AVDTP_CODEC_TYPE_SBC;
    sbcSrcCodec1.elemLen = sizeof(SbcSrcElements);
    sbcSrcCodec1.elements = SbcSrcElements;
    Stream[2].type = A2DP_STREAM_TYPE_SOURCE;
    Stream[3].type = A2DP_STREAM_TYPE_SOURCE;
    A2DP_Register(&Stream[2], &sbcSrcCodec1, a2dp_callback);
    A2DP_Register(&Stream[3], &sbcSrcCodec1, a2dp_callback);
#endif
      
//    AVDEV_ListenSignalLink(a2dp_device_callback);
    
#if BTDM_STACK_ENABLE_AVRCP
    for (uint8_t i=0; i<NUM_BT_DEVICES; i++) {
        AVRCP_RegisterSec(&rcpCtChannel[i], avrcp_callback, AVRCP_CT|AVRCP_TG, &secParms);
#if AVRCP_ADVANCED_TARGET == XA_ENABLED
        AVRCP_TgSetEventMask(&rcpCtChannel[i], AVRCP_ENABLE_VOLUME_CHANGED);
#endif
    }
#endif

#if BTDM_STACK_ENABLE_PBAP
    for (uint8_t i=0; i<NUM_BT_DEVICES; i++) {
        pbap_client[i].cApp.connFlags = GOEP_NEW_CONN;
        PBAP_RegisterClientSec(&pbap_client[i], pbap_callback, &secParms);
    }
#endif
    /*告诉蓝牙协议栈，有一个spp服务，监听SPP_SERVER_PORT，发生spp事件时调用spp_callback*/
#if BTDM_STACK_ENABLE_SPP
    for (uint8_t i=0; i<NUM_BT_DEVICES; i++) 
    {
        spp_register(&spp_dev[i], &secParms, SPP_SERVER_PORT, spp_callback);
    }
#endif
    
#if BTDM_STACK_ENABLE_MAP
    for (uint8_t i=0; i<NUM_BT_DEVICES; i++) {
        map_client[i].cApp.connFlags = GOEP_NEW_CONN;
        MAP_RegisterClientSec(&map_client[i], map_callback, &secParms);
    }
#endif    

#if BTDM_STACK_ENABLE_HID
    for (uint8_t i=0; i<NUM_BT_DEVICES; i++) {
        HID_RegDevice(&hid_channel[i], hid_callback);
    }
#endif
    //ME_SetAccessibleModeC(BAM_NOT_ACCESSIBLE, &access_mode_nc);
    //ME_SetAccessibleModeNC(BAM_GENERAL_ACCESSIBLE, &access_mode_nc);
    bt_enter_pairing(BAM_GENERAL_ACCESSIBLE, &access_mode_nc);
}
#endif
