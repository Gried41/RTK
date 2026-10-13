
#include <stdint.h>
#include <stdbool.h>
#include "user_bt.h"
#include "assert.h"
#include "app_task.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "btdm_mem.h"
#include "app_audio.h"
#if BTDM_STACK_ENABLE_BT
#define MAX_RECONNECT_TIMES 3   //reconnect times if page timeout or linkloss
#define PAGE_TIMEOUT    0x1000   //connect timeout value, uint:625us
#define MAX_NAME_SIZE   24
#define SCO_DATA_BUFFER_COUNT       3

uint8_t enable_audio_transfer = 0; 

#if BTDM_STACK_ENABLE_HF || BTDM_STACK_ENABLE_AG
///enable audio transfer, audio is transfered from phone to earphone
uint8_t voice_phone_data[120];
uint8_t voice_headset_data[120];

struct sco_data_t {
    struct co_list_hdr hdr;
    void *arg;
    uint16_t length;
    uint8_t data[];
};

const uint8_t hf_enable_voltage_notify[] = "AT+XAPL=AAAA-1111-01,10";
const uint8_t hf_search_remote_dev_type[] = "AT+CGMI";
static uint8_t sco_data_buffering = 0;
static struct co_list sco_data_list;
#endif

struct user_bt_env_t user_bt_env;
static TimerHandle_t bt_connect_timer = NULL;
#if BTDM_STACK_ENABLE_AVRCP
static TimerHandle_t bt_avrcp_connect_timer = NULL;
#endif
static TimerHandle_t bt_linkloss_connect_timer = NULL;

#if BTDM_STACK_ENABLE_A2DP_SNK || BTDM_STACK_ENABLE_A2DP_SRC
static TimerHandle_t bt_a2dp_connect_timer = NULL;
struct sbc_info_t sbcinfo[NUM_BT_DEVICES];
uint8_t source_sbc_frame_cnt[NUM_BT_DEVICES] = {0};
static void bt_save_codec_info(uint8_t dev_index, uint8_t *elements)
{
    memcpy(&sbcinfo[dev_index].elem[0], elements, 4);
    /* copy codec info to local*/
    sbcinfo[dev_index].info.bitPool = elements[3];
    if(elements[0]&0x20){
        sbcinfo[dev_index].info.sampleFreq = 2;//44100
    }
    else{
        sbcinfo[dev_index].info.sampleFreq = 3;//48000
    }
    sbcinfo[dev_index].info.allocMethod = elements[1]&0x03; //1-->loudness, 2--->snr
    
    if(elements[1]&0x10){
        sbcinfo[dev_index].info.numBlocks = 16;
    }
    else if(elements[1]&0x20){
        sbcinfo[dev_index].info.numBlocks = 12;
    }
    else if(elements[1]&0x40){
        sbcinfo[dev_index].info.numBlocks = 8;
    }
    else{
        sbcinfo[dev_index].info.numBlocks = 4;
    }
    
    if(elements[1]&0x04){
        sbcinfo[dev_index].info.numSubBands = 8;
    }
    else{
        sbcinfo[dev_index].info.numSubBands = 4;
    }
}

static uint16_t sbc_calc_frame_len(struct sbc_info_t *info)
{
    uint16_t temp = (1*info->info.numSubBands) + (info->info.numBlocks*info->info.bitPool);
    uint16_t frame_len = 4 + ((4*info->info.numSubBands*info->info.numChannels)>>3) + (temp>>3);
    if(temp%8){
        frame_len++;
    }   
    return frame_len;
}
uint32_t bt_source_discard_pkt_num = 0;
extern uint8_t bt_get_flush_enable(uint16_t id);

void encoded_sbc_frame_cb(void *arg, uint8_t *data, uint16_t length)
{
    BtStatus status;
    A2dpSbcPacket *packet;

    uint16_t frame_len = sbc_calc_frame_len(&sbcinfo[0]);
    if(length != frame_len*5){
        printf("length = %d,%d\r\n",length,frame_len);
    }
    ///fix for some airpods compatibility
    if(user_bt_env.bt_source_sbc_pkt_num > 6){
        printf("pkt buffer max...%d\r\n", bt_source_discard_pkt_num);
        if(bt_get_flush_enable(user_bt_env.dev[0].pstream->stream.conn.l2ChannelId)){
            bt_source_discard_pkt_num ++;
        }
        return;
    }
//    printf("src send sbc:%d\r\n",user_bt_get_state(0) );
    #if 1
    if(user_bt_get_state(0) == BT_STATE_MEDIA_PLAYING){
        packet = (A2dpSbcPacket *)btdm_malloc(sizeof(A2dpSbcPacket));
        packet->frameSize = frame_len;
        packet->dataLen = frame_len*5;
        packet->data = (uint8_t *)btdm_malloc(packet->dataLen);
        memcpy(packet->data, data, length);
        status = A2DP_StreamSendSbcPacket(user_bt_env.dev[0].pstream,packet,&sbcinfo[0].info);    
        if(status != BT_STATUS_PENDING){
            printf("err sending sbc data\r\n");
            btdm_free(packet->data);
            btdm_free(packet);
        }
        else{
        user_bt_env.bt_source_sbc_pkt_num++;
    }
    }
    #else
    if(user_bt_get_state(0) == BT_STATE_MEDIA_PLAYING){
        if(source_sbc_frame_cnt[0] < 3){
            packet = (A2dpSbcPacket *)btdm_malloc(sizeof(A2dpSbcPacket));
            packet->frameSize = frame_len;
            packet->dataLen = frame_len*5;
            packet->data = (uint8_t *)btdm_malloc(packet->dataLen);
            memcpy(packet->data, data, length);
            status = A2DP_StreamSendSbcPacket(user_bt_env.dev[0].pstream,packet,&sbcinfo[0].info);    
            user_bt_env.bt_source_sbc_pkt_num++; 
            source_sbc_frame_cnt[0]++;
            fputc('A',0);
        }

    }
    if(user_bt_get_state(1) == BT_STATE_MEDIA_PLAYING){
       if(source_sbc_frame_cnt[1] < 3){
            packet = (A2dpSbcPacket *)btdm_malloc(sizeof(A2dpSbcPacket));
            packet->frameSize = frame_len;
            packet->dataLen = frame_len*5;
            packet->data = (uint8_t *)btdm_malloc(packet->dataLen);
            memcpy(packet->data, data, length);
            status = A2DP_StreamSendSbcPacket(user_bt_env.dev[1].pstream,packet,&sbcinfo[1].info);    
            user_bt_env.bt_source_sbc_pkt_num++;
            source_sbc_frame_cnt[1] ++;
            fputc('B',0);
        }
    }
    #endif

}
#endif
uint8_t bt_get_free_dev(void)
{
    uint8_t index;
    for(index = 0; index < NUM_BT_DEVICES; index++){
        if(user_bt_env.dev[index].state == BT_LINK_STATE_IDLE){
            break;
        }
    }
    return index;
}

uint8_t bt_find_dev_index(BD_ADDR *addr)
{
    uint8_t index;
    printf("find dev :%x,%x,,%x\r\n",addr->A[0],addr->A[1],user_bt_env.dev[0].remote_bd.A[0]);
    for(index = 0; index < NUM_BT_DEVICES; index++){
        if(memcmp(&user_bt_env.dev[index].remote_bd, addr, sizeof(BD_ADDR)) == 0){
            break;
        }
    }
    return index;
}
uint8_t bt_find_dev_index_by_dev(void *dev)
{
    uint8_t index;
    if(dev == NULL){
        return NUM_BT_DEVICES;
    }
    for(index = 0; index < NUM_BT_DEVICES; index++){
        if(user_bt_env.dev[index].remDev == dev){
            break;
        }
    }
    return index;
}
#if BTDM_STACK_ENABLE_HF
uint8_t bt_find_dev_index_by_hfchan(HfChannel *chan)
{
    uint8_t index;
    if(chan == NULL){
        return NUM_BT_DEVICES;
    }
    for(index = 0; index < NUM_BT_DEVICES; index++){
        if(user_bt_env.dev[index].hf_chan == chan){
            break;
        }
    }
    return index;
}
#endif
#if BTDM_STACK_ENABLE_AG
uint8_t bt_find_dev_index_by_hfgchan(HfgChannel *chan)
{
    uint8_t index;
    if(chan == NULL){
        return NUM_BT_DEVICES;
    }
    for(index = 0; index < NUM_BT_DEVICES; index++){
        if(user_bt_env.dev[index].hfg_chan == chan){
            break;
        }
    }
    return index;
}
#endif
#if BTDM_STACK_ENABLE_AVRCP
uint8_t bt_find_dev_index_by_rcpchan(AvrcpChannel *chan)
{
    uint8_t index;
    if(chan == NULL){
        return NUM_BT_DEVICES;
    }
    for(index = 0; index < NUM_BT_DEVICES; index++){
        if(user_bt_env.dev[index].rcp_chan == chan){
            break;
        }
    }
    return index;
}
#endif
#if BTDM_STACK_ENABLE_PBAP
uint8_t bt_find_dev_index_by_pbapclient(PbapClientSession *client)
{
    uint8_t index;
    if(client == NULL){
        return NUM_BT_DEVICES;
    }
    for(index = 0; index < NUM_BT_DEVICES; index++){
        if(user_bt_env.dev[index].pbap_client == client){
            break;
        }
    }
    return index;
}
#endif
static void bt_parse_inq_result(uint8_t *input, uint8_t *name, uint8_t *len)
{
    uint8_t i = 0;
    uint8_t name_offset = 0;
    uint8_t *ptr = NULL;

    if ((input == NULL) || (name == NULL) || (len == NULL)) {
        return;
    }

    ptr = input;
    *len = 0;

    while (i < 239) {
        uint8_t length = *(ptr + i);
        uint8_t step = length + 1;

        if ((length == 0) || (length > (239-i))) {
            break;
        }

        if (*(ptr + i + 1) == 0x09) {
            if (length >= 2) {
                *len = length - 1;
                name_offset = i + 2;
            }
            break;
        }
        i += (length + 1);
    }

    if (*len >= MAX_NAME_SIZE) {
        memcpy(name, input + name_offset, MAX_NAME_SIZE);
        *len = MAX_NAME_SIZE;
    } 
    else if (*len > 0) {
        memcpy(name, input + name_offset, *len);
    }
}
#if BTDM_STACK_ENABLE_HF || BTDM_STACK_ENABLE_AG
void encoded_sco_frame_cb(void *arg, uint8_t *data, uint16_t length)
{
    static uint16_t seq = 0;
    if (sco_data_buffering) {
        struct sco_data_t *sco_data;
        
        sco_data = (void *)btdm_malloc(sizeof(struct sco_data_t) + length);
        sco_data->arg = arg;
        sco_data->length = length;
        memcpy((void *)&sco_data->data[0], data, length);
        co_list_push_back(&sco_data_list, &sco_data->hdr);
        
        sco_data_buffering--;
        if (sco_data_buffering == 0) {
            sco_data = (void *)co_list_pop_front(&sco_data_list);
            while (sco_data) {
                //fputc('D', NULL);
                app_bt_send_sco_data(sco_data->arg, seq++, sco_data->data, sco_data->length);
                btdm_free((void *)sco_data);
                sco_data = (void *)co_list_pop_front(&sco_data_list);
            }
        }
    }
    else {
        app_bt_send_sco_data(arg, seq++, data, length);
    }
}

void bt_free_sco_buffer_list(void)
{
    struct sco_data_t *sco_data;
    sco_data_buffering = SCO_DATA_BUFFER_COUNT;

    sco_data = (void *)co_list_pop_front(&sco_data_list);
    while (sco_data) {
        btdm_free((void *)sco_data);
        sco_data = (void *)co_list_pop_front(&sco_data_list);
    }
}
#endif

BtStatus bt_do_connect(BD_ADDR *addr, uint8_t profiles, void **chan)
{
    BtStatus ret = BT_STATUS_FAILED;
    uint8_t index = 0;
    printf("bt do connect\r\n");
    if((profiles & user_bt_env.enable_profiles) == profiles){
        //connect to remote device depending on the profile
        if (0){
        }
        #if BTDM_STACK_ENABLE_HF
        else if(profiles & ENABLE_PROFILE_HF){
            index = bt_get_free_hf_channel();
            if(index < NUM_BT_DEVICES){
                ret = HF_CreateServiceLink(&hf_channel[index], addr);
                *chan = (void *)&hf_channel[index];            
            }
        }
        #endif
        #if BTDM_STACK_ENABLE_AG
        else if(profiles & ENABLE_PROFILE_HFG){
            index = bt_get_free_hfg_channel();
            if(index < NUM_BT_DEVICES){
                ret = HFG_CreateServiceLink(&hfg_channel[index], addr);
                *chan = (void *)&hfg_channel[index];
            }
        }
        #endif
        #if BTDM_STACK_ENABLE_A2DP_SNK
        else if(profiles & ENABLE_PROFILE_A2DP_SINK){
            index = bt_get_free_a2dp_sink_stream();
            if(index < NUM_BT_DEVICES){
                ret = A2DP_OpenStream(&Stream[index],addr);
                *chan = (void *)&Stream[index];
            }
        }
        #endif
        #if BTDM_STACK_ENABLE_A2DP_SRC
        else if(profiles & ENABLE_PROFILE_A2DP_SOURCE){
            index = bt_get_free_a2dp_source_stream();
            if(index < NUM_STREAMS){
                ret = A2DP_OpenStream(&Stream[index],addr);
                *chan = (void *)&Stream[index];
            }
        }
        #endif
        #if BTDM_STACK_ENABLE_HID
        else if(profiles & ENABLE_PROFILE_HID){
            index = bt_get_free_hid_channel();
            printf("free hid index:%d\r\n",index);
            if(index < NUM_BT_DEVICES){
                ret = HID_OpenConnection(&hid_channel[index],addr);
                *chan = (void *)&hid_channel[index];
            }
        }
        #endif
        #if BTDM_STACK_ENABLE_SPP 
        else if(profiles & ENABLE_PROFILE_SPP){
            index = bt_get_free_spp_channel();
            if(index < NUM_BT_DEVICES){
                ret = spp_connect(&spp_dev[index],addr);
                *chan = (void *)&spp_dev[index];
            }        
        }    
        #endif
    }
    printf("ret=%x\r\n",ret);
    return ret;
}

BtStatus bt_do_disconnect(BD_ADDR *addr, CmgrHandler *handler,uint8_t force)
{
    uint8_t dev_index;
    BtStatus status = BT_STATUS_PENDING;
    
    dev_index = bt_find_dev_index(addr);
    xTimerStop(bt_connect_timer,portMAX_DELAY);
    #if BTDM_STACK_ENABLE_AVRCP
    xTimerStop(bt_avrcp_connect_timer,portMAX_DELAY);
    #endif

    #if BTDM_STACK_ENABLE_A2DP_SRC
    xTimerStop(bt_a2dp_connect_timer,portMAX_DELAY);
    #endif
    xTimerStop(bt_linkloss_connect_timer,portMAX_DELAY);
    user_bt_env.connect_times = 0;
    
    printf("bt do disconnect\r\n");
    if(force == true){
        if(user_bt_env.dev[dev_index].remDev != NULL){
            ///do acl disconnect
            ME_ForceDisconnectLinkWithReason(NULL,user_bt_env.dev[dev_index].remDev,BEC_USER_TERMINATED,TRUE); 
        }
        else{
//            status = ME_ForceCancelCreateLink();
            status = CMGR_RemoveDataLink(handler);
        }
    }
    else{
        #if BTDM_STACK_ENABLE_HF
        if(user_bt_env.dev[dev_index].conFlags & LINK_STATUS_SCO_CONNECTED){
            HF_DisconnectAudioLink(user_bt_env.dev[dev_index].hf_chan);
        }
        if(user_bt_env.dev[dev_index].hf_chan->state != HF_STATE_CLOSED){
            HF_DisconnectServiceLink(user_bt_env.dev[dev_index].hf_chan);
        }
        #endif
        #if BTDM_STACK_ENABLE_AG
        if(user_bt_env.dev[dev_index].hfg_chan->state != HFG_STATE_CLOSED){
            HFG_DisconnectServiceLink(user_bt_env.dev[dev_index].hfg_chan);
        }
        #endif
        #if BTDM_STACK_ENABLE_AVRCP
        if(user_bt_env.dev[dev_index].rcp_chan->chnl.conn.state != 0){
            AVRCP_Disconnect(user_bt_env.dev[dev_index].rcp_chan);
        }
        #endif
        #if BTDM_STACK_ENABLE_A2DP_SNK || BTDM_STACK_ENABLE_A2DP_SRC
        if(user_bt_env.dev[dev_index].pstream->stream.state != 0){
            A2DP_CloseStream(user_bt_env.dev[dev_index].pstream);
        }    
        #endif
        #if BTDM_STACK_ENABLE_PBAP
        if(user_bt_env.dev[dev_index].pbap_client != NULL){
            PBAP_ClientDisconnect(user_bt_env.dev[dev_index].pbap_client);
        }
        #endif
        #if BTDM_STACK_ENABLE_HID
        if(user_bt_env.dev[dev_index].hid_chan->state != HID_STATE_CLOSED){
            HID_CloseConnection(user_bt_env.dev[dev_index].hid_chan);
        }
        #endif
        #if BTDM_STACK_ENABLE_SPP
        if(user_bt_env.dev[dev_index].spp_dev->state != DEVICE_STATE_DISCONNECTED){
            spp_disconnect(user_bt_env.dev[dev_index].spp_dev);
        }
        #endif        
        
    }
    return status;
}

void bt_disconnect_cb(BD_ADDR *addr, uint8_t errcode)
{
    BtStatus status;
    if(errcode == BEC_CONNECTION_TIMEOUT){
        uint8_t dev_index = bt_find_dev_index(addr);
        user_bt_env.connect_times = MAX_RECONNECT_TIMES;
        memcpy(&user_bt_env.last_dev_addr,addr,BD_ADDR_SIZE);
        printf("linkloss>>%d,%x\r\n",dev_index,user_bt_env.dev[dev_index].connect_profile);
        if(user_bt_env.dev[dev_index].connect_profile & (ENABLE_PROFILE_A2DP_SINK|ENABLE_PROFILE_HF)){
            xTimerChangePeriod(bt_linkloss_connect_timer, 10000, portMAX_DELAY);
            vTimerSetTimerID(bt_linkloss_connect_timer,(void *)(uint32_t)user_bt_env.dev[dev_index].connect_profile);
            xTimerStart(bt_linkloss_connect_timer,portMAX_DELAY);        
        }
    }
    else if(errcode == BEC_USER_TERMINATED){
    
    }
    else if(errcode == BEC_LOCAL_TERMINATED){
    
    }
    else{
    
    }
}

void bt_connect_timer_cb(TimerHandle_t pxTimer)
{
    printf("bt connect timer cb\r\n");
    if(user_bt_env.connect_times > 0){
        user_bt_env.connect_times --;
        bt_connect(&user_bt_env.last_dev_addr, user_bt_env.last_connect_profile);    
    }
}
#if BTDM_STACK_ENABLE_AVRCP
void bt_avrcp_connect_timer_cb(TimerHandle_t pxTimer)
{   
    BtStatus status;
    printf("bt avrcp connect timer cb\r\n");

    uint8_t dev_index = (uint32_t)pvTimerGetTimerID(bt_avrcp_connect_timer);
    if(((user_bt_env.dev[dev_index].conFlags&LINK_STATUS_AVC_CONNECTED) == 0)
        && ((user_bt_env.dev[dev_index].conFlags&LINK_STATUS_AV_CONNECTED) == LINK_STATUS_AV_CONNECTED)){
        uint8_t index = bt_get_free_avrcp_channel();
        status = AVRCP_Connect(&rcpCtChannel[index],&user_bt_env.dev[dev_index].remote_bd);
        printf("avrcp connect status = %d\r\n",status);
    }
}
#endif
#if BTDM_STACK_ENABLE_A2DP_SRC
void bt_a2dp_connect_timer_cb(TimerHandle_t pxTimer)
{   
    BtStatus status;
    printf("bt a2dp connect timer cb\r\n");

    uint8_t dev_index = (uint32_t)pvTimerGetTimerID(bt_a2dp_connect_timer);
    if(((user_bt_env.dev[dev_index].conFlags&LINK_STATUS_AV_CONNECTED) == 0)
        && ((user_bt_env.dev[dev_index].conFlags&LINK_STATUS_HFG_CONNECTED) == LINK_STATUS_HFG_CONNECTED)){
        uint8_t index = bt_get_free_a2dp_source_stream();
        if(index < NUM_STREAMS){
            A2DP_OpenStream(&Stream[index], &user_bt_env.dev[dev_index].remote_bd);    
        }    
        printf("a2dp connect status = %d\r\n",status);
    }
}
#endif
void bt_linkloss_connect_timer_cb(TimerHandle_t pxTimer)
{
    printf("bt linkloss timer cb\r\n");
    uint8_t profiles = (uint32_t)pvTimerGetTimerID(bt_linkloss_connect_timer);
    bt_connect(&user_bt_env.last_dev_addr, profiles);
}

void bt_connect_cb(uint8_t type, BD_ADDR *addr, uint8_t errcode)
{
//    printf("bt connect cb:type = %d,addr=0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,err=%d\r\n",\
//            type,addr->A[0],addr->A[1],addr->A[2],addr->A[3],addr->A[4],addr->A[5],errcode);

    if((type == BT_EVENT_CON_CNF) && (errcode == BEC_PAGE_TIMEOUT) && (user_bt_env.connect_times > 0))
    {
        memcpy(&user_bt_env.last_dev_addr,addr,BD_ADDR_SIZE);
        xTimerStart(bt_connect_timer,portMAX_DELAY);
    }
}

static void bt_free_elt(void)
{
    struct bt_connect_elt_t *elt;
    GLOBAL_INT_DISABLE();
    elt = (struct bt_connect_elt_t *)co_list_pop_front(&user_bt_env.op_list);
    if(elt){
        user_bt_env.cur_action = BT_ACTION_NULL;
        user_bt_env.action_cnt --;
        btdm_free((void *)elt);
    }
    GLOBAL_INT_RESTORE();
}

uint8_t  bt_check_conn(uint8_t dev_index)
{
    uint16_t conn_flag = 0;
    uint16_t conn_flag_tmp;
    uint8_t ret = false;
    if(user_bt_env.enable_profiles & (ENABLE_PROFILE_HF | ENABLE_PROFILE_A2DP_SINK | ENABLE_PROFILE_A2DP_SOURCE | ENABLE_PROFILE_HFG))
    {
        if(user_bt_env.enable_profiles & ENABLE_PROFILE_HF){
            conn_flag |= LINK_STATUS_HF_CONNECTED;
        }
        if(user_bt_env.enable_profiles & (ENABLE_PROFILE_A2DP_SINK | ENABLE_PROFILE_A2DP_SOURCE)){
            conn_flag |= LINK_STATUS_AV_CONNECTED | LINK_STATUS_AVC_CONNECTED;
        }
        if(user_bt_env.enable_profiles & ENABLE_PROFILE_HFG){
            conn_flag |= LINK_STATUS_HFG_CONNECTED;
        }    
    }
    else{
        if(user_bt_env.enable_profiles & ENABLE_PROFILE_SPP){
            conn_flag |= LINK_STATUS_SPP_CONNECTED;
        }        
        else if(user_bt_env.enable_profiles & ENABLE_PROFILE_HID){
            conn_flag |= LINK_STATUS_HID_CONNECTED;
        }
    }
    
    if((conn_flag & (LINK_STATUS_HF_CONNECTED|LINK_STATUS_HFG_CONNECTED)) == (LINK_STATUS_HF_CONNECTED|LINK_STATUS_HFG_CONNECTED)){
        ///both supported,only check one profile
        conn_flag_tmp = conn_flag & (~LINK_STATUS_HF_CONNECTED);
        if((user_bt_env.dev[dev_index].conFlags & conn_flag_tmp) == conn_flag_tmp){
            ret = true;
        }
        conn_flag_tmp = conn_flag & (~LINK_STATUS_HFG_CONNECTED);
        if((user_bt_env.dev[dev_index].conFlags & conn_flag_tmp) == conn_flag_tmp){
            ret = true;
        }
    }
    else if((user_bt_env.dev[dev_index].conFlags & conn_flag) == conn_flag){
        ret = true;
    }
    printf("bt check conn flag = %x,profile=%x,conn_flg=%x,conn profile=%x\r\n",user_bt_env.dev[dev_index].conFlags,user_bt_env.enable_profiles,conn_flag,user_bt_env.dev[dev_index].connect_profile);

    return ret;
}

void bt_update_conn_status(uint8_t event, void *chan, const void *param)
{
    uint8_t dev_index;
    BD_ADDR addr;
    BtRemoteDevice *rm_dev = NULL;
    BtStatus status;
    //HfCallbackParms *info;
    
    if(0){}
    #if BTDM_STACK_ENABLE_HF
    else if(event < BT_PROFILE_HF_MAX){
//        info = (HfCallbackParms *)param;
        if((event == BT_PROFILE_HF_CONN_REQ) || (event == BT_PROFILE_HF_CONN)){
            rm_dev = ((HfCallbackParms *)param)->p.remDev;
            dev_index = bt_find_dev_index_by_dev(rm_dev);
        }
        else{
            dev_index = bt_find_dev_index_by_hfchan(chan);
        } 
    }
    #endif
    #if BTDM_STACK_ENABLE_A2DP_SNK || BTDM_STACK_ENABLE_A2DP_SRC
    else if(event < BT_PROFILE_A2DP_MAX){
        if(event == BT_PROFILE_A2DP_DISCONN){
            A2dpCallbackParms *pm = (A2dpCallbackParms *)param;
            rm_dev = pm->p.device;
            if(rm_dev == NULL){
                rm_dev = A2DP_GetRemoteDevice((A2dpStream *)chan);
            }
            dev_index = bt_find_dev_index_by_dev(rm_dev);
        }
        else{
            rm_dev = A2DP_GetRemoteDevice((A2dpStream *)chan);
            dev_index = bt_find_dev_index_by_dev(rm_dev);
        }
    }
    #endif
    #if BTDM_STACK_ENABLE_AVRCP
    else if(event < BT_PROFILE_AVRCP_MAX){
        if((event == BT_PROFILE_AVRCP_CONN) || (event == BT_PROFILE_AVRCP_DISCONN)){
            rm_dev = ((AvrcpCallbackParms *)param)->p.remDev;
            dev_index = bt_find_dev_index_by_dev(rm_dev);
        }
        else{
            dev_index = bt_find_dev_index_by_rcpchan(chan);
        } 
    }
    #endif
    #if BTDM_STACK_ENABLE_PBAP
    else if(event < BT_PROFILE_PBAP_MAX){
        dev_index = bt_find_dev_index_by_pbapclient(((PbapClientCallbackParms *)param)->client);
    }
    #endif
    #if BTDM_STACK_ENABLE_AG
    else if(event < BT_PROFILE_HFG_MAX){
        if(event == BT_PROFILE_HFG_AUDIO_DATA){
            dev_index = bt_find_dev_index_by_hfgchan(chan);
        }
        else{
            rm_dev = ((HfgCallbackParms *)param)->p.remDev;
            dev_index = bt_find_dev_index_by_dev(rm_dev);        
        }
    }
    #endif
    #if BTDM_STACK_ENABLE_SPP
    else if(event < BT_PROFILE_SPP_MAX){
            rm_dev = ((SppCallbackParms *)param)->p.remDev;
            dev_index = bt_find_dev_index_by_dev(rm_dev);        
    }
    #endif
    #if BTDM_STACK_ENABLE_HID
    else if(event < BT_PROFILE_HID_MAX){
        rm_dev = ((HidCallbackParms *)param)->ptrs.remDev;
        dev_index = bt_find_dev_index_by_dev(rm_dev);        
    }
    #endif
    else if(event < BT_INFO_MAX){
        dev_index = 0;
    }
    if(dev_index >= NUM_BT_DEVICES){
        printf("!!!!error dev index, event = %d,%x,%x\r\n",event,(unsigned int)rm_dev,(unsigned int)user_bt_env.dev[0].remDev);
        return;
    }
    //printf("bt update conn %d\r\n",event);
    switch(event){
        #if BTDM_STACK_ENABLE_HF
        case BT_PROFILE_HF_CONN_REQ:
            user_bt_env.dev[dev_index].responder = TRUE;
            user_bt_env.dev[dev_index].hf_chan = chan;
        break;
        
        case BT_PROFILE_HF_CONN:
        {
            user_bt_env.dev[dev_index].hf_chan = chan;
            user_bt_env.dev[dev_index].conFlags |= LINK_STATUS_HF_CONNECTED;
            user_bt_env.dev[dev_index].connect_profile |= ENABLE_PROFILE_HF;
            ///check connection
            if(true == bt_check_conn(dev_index)){
                bt_connect_act_cmp(BT_EVENT_PROFILE_CONNECT,0,rm_dev);
            }
            
            ///enable voltage notify
            bt_send_hf_cmd(dev_index,hf_enable_voltage_notify);
            
            ///search remote device type
            bt_send_hf_cmd(dev_index,hf_search_remote_dev_type);
            
            bt_enable_caller_id_notify(dev_index, 1);
            #if BTDM_STACK_ENABLE_A2DP_SNK
            ///connect a2dp in master role
            if((user_bt_env.dev[dev_index].responder == FALSE)
                && ((user_bt_env.dev[dev_index].conFlags & LINK_STATUS_AV_CONNECTED) == 0)
                && (user_bt_env.enable_profiles & ENABLE_PROFILE_A2DP_SINK))
            {
                    uint8_t index = bt_get_free_a2dp_sink_stream();
                    status = A2DP_OpenStream(&Stream[index],&user_bt_env.dev[dev_index].remote_bd);
                    printf("a2dp open stream status=%d\r\n",status);
            }
            #endif

        }
        break;
    
        case BT_PROFILE_HF_DISCONN:
        {
//            ME_GetBdAddr(info->p.remDev, addr);
//            dev_index = bt_find_dev_index(addr);
//            HfCallbackParms *info = (HfCallbackParms *)param;
//            dev_index = bt_find_dev_index_by_dev(info->p.remDev);
            user_bt_env.dev[dev_index].hf_chan = NULL;
            user_bt_env.dev[dev_index].conFlags &= ~LINK_STATUS_HF_CONNECTED;
        }
        break;
        case BT_PROFILE_HF_CALL:
        {
            user_bt_env.dev[dev_index].active = ((HfCallbackParms *)param)->p.call;
            if((enable_audio_transfer == 0)
              ||(enable_audio_transfer && ((user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_HFG_CONNECTED) == 0)))
            {
                if((user_bt_env.enable_local_ring == true) 
                    && (user_bt_env.dev[dev_index].active == HF_CALL_ACTIVE)
                    && (user_bt_env.dev[dev_index].conFlags & LINK_STATUS_SCO_CONNECTED)){
                    sco_data_buffering = SCO_DATA_BUFFER_COUNT;
                    {
                        struct sco_data_t *sco_data;
                        sco_data = (void *)co_list_pop_front(&sco_data_list);
                        while (sco_data) {
                            btdm_free((void *)sco_data);
                            sco_data = (void *)co_list_pop_front(&sco_data_list);
                        }
                    }
                    if (((HfChannel*)chan)->codecID == 2) {
                        CMGR_SetAudioVoiceSettings(0x63);
                        app_audio_sco_start(AUDIO_TYPE_MSBC, encoded_sco_frame_cb, chan);
                    }
                    else {
                        CMGR_SetAudioVoiceSettings(0x60);
                        app_audio_sco_start(AUDIO_TYPE_PCM, encoded_sco_frame_cb, chan);
                    }         
                }
            }
        }
        break;
        case BT_PROFILE_HF_CALLSETUP:
        {
            user_bt_env.dev[dev_index].setup_state =((HfCallbackParms *)param)->p.callSetup;
            if((enable_audio_transfer == 0)
              ||(enable_audio_transfer && ((user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_HFG_CONNECTED) == 0)))
            {
                if((user_bt_env.enable_local_ring == true) 
                    && ((user_bt_env.dev[dev_index].setup_state == HF_CALL_SETUP_OUT) || (user_bt_env.dev[dev_index].setup_state == HF_CALL_SETUP_ALERT))
                    && (user_bt_env.dev[dev_index].conFlags & LINK_STATUS_SCO_CONNECTED)){
                    sco_data_buffering = SCO_DATA_BUFFER_COUNT;
                    {
                        struct sco_data_t *sco_data;
                        sco_data = (void *)co_list_pop_front(&sco_data_list);
                        while (sco_data) {
                            btdm_free((void *)sco_data);
                            sco_data = (void *)co_list_pop_front(&sco_data_list);
                        }
                    }
                    if (((HfChannel*)chan)->codecID == 2) {
                        CMGR_SetAudioVoiceSettings(0x63);
                        app_audio_sco_start(AUDIO_TYPE_MSBC, encoded_sco_frame_cb, chan);
                    }
                    else {
                        CMGR_SetAudioVoiceSettings(0x60);
                        app_audio_sco_start(AUDIO_TYPE_PCM, encoded_sco_frame_cb, chan);
                    }         
                }
            }
        }
        break;
        case BT_PROFILE_HF_AUDIO_CONN:
        {
            user_bt_env.dev[dev_index].conFlags |= LINK_STATUS_SCO_CONNECTED;
            if((user_bt_env.enable_local_ring == false)
                || (user_bt_env.dev[dev_index].active == HF_CALL_ACTIVE)
                || (user_bt_env.dev[dev_index].setup_state == HF_CALL_SETUP_OUT)
                || (user_bt_env.dev[dev_index].setup_state == HF_CALL_SETUP_ALERT)){
                if((enable_audio_transfer == 0)
                    ||(enable_audio_transfer && ((user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_HFG_CONNECTED) == 0))){
                    sco_data_buffering = SCO_DATA_BUFFER_COUNT;
                    {
                        struct sco_data_t *sco_data;
                        sco_data = (void *)co_list_pop_front(&sco_data_list);
                        while (sco_data) {
                            btdm_free((void *)sco_data);
                            sco_data = (void *)co_list_pop_front(&sco_data_list);
                        }
                    }
                    if (((HfChannel*)chan)->codecID == 2) {
                        CMGR_SetAudioVoiceSettings(0x63);
                        app_audio_sco_start(AUDIO_TYPE_MSBC, encoded_sco_frame_cb, chan);
                    }
                    else {
                        CMGR_SetAudioVoiceSettings(0x60);
                        app_audio_sco_start(AUDIO_TYPE_PCM, encoded_sco_frame_cb, chan);
                    }       
                }                    
            }
            #if BTDM_STACK_ENABLE_AG
            if(enable_audio_transfer){
                if((NUM_BT_DEVICES == 2)&&(user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_HFG_CONNECTED)){
                    //HFG_CreateAudioLink(user_bt_env.dev[!dev_index].hfg_chan, HFG_AR_LOCAL_USER_ACTION);
                    HfgResponse *rsp;
                    status = BT_STATUS_NO_RESOURCES;
                    rsp = (HfgResponse *)btdm_malloc(sizeof(HfgResponse));
                    if(rsp != NULL){
                        status = HFG_CreateCodecConnection(&hfg_channel[0], ((HfChannel*)chan)->codecID, rsp);
                    }
                    printf("status = %d\r\n",status);
                    if(status != BT_STATUS_PENDING){
                        btdm_free((void *)rsp);
                        //No sco transfer if earphone doesn't support msbc 
//                        if(status == BT_STATUS_RESTRICTED){
//                            CMGR_SetAudioVoiceSettings(0x60);
//                            HFG_CreateAudioLink(&hfg_channel[0],HFG_AR_LOCAL_USER_ACTION);                
//                        }
                    }
                }            
            }
            #endif
            system_prevent_sleep_set(SYSTEM_PREVENT_SLEEP_SCO_ONGOING);  
        }
        break;
        case BT_PROFILE_HF_AUDIO_DISCONN:
        {
            struct sco_data_t *sco_data;
            user_bt_env.dev[dev_index].conFlags &= ~LINK_STATUS_SCO_CONNECTED;
            system_prevent_sleep_clear(SYSTEM_PREVENT_SLEEP_SCO_ONGOING);
            if((enable_audio_transfer == 0)
                ||(enable_audio_transfer && ((user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_HFG_CONNECTED) == 0))){
                app_audio_sco_stop();
                sco_data = (void *)co_list_pop_front(&sco_data_list);
                while (sco_data) {
                    btdm_free((void *)sco_data);
                    sco_data = (void *)co_list_pop_front(&sco_data_list);
                }
            }
            #if BTDM_STACK_ENABLE_AG    
            if(enable_audio_transfer){
                if((NUM_BT_DEVICES == 2) && (user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_SCO_CONNECTED)){
                    HFG_DisconnectAudioLink(user_bt_env.dev[!dev_index].hfg_chan, HFG_AR_LOCAL_USER_ACTION);
                }            
            }
            #endif
        }
        break;
        case BT_PROFILE_HF_RING:
        {
            if(user_bt_env.enable_local_ring == true){
                app_audio_tone_play(AUDIO_TYPE_SBC,NULL,0);         
            }
        }
        break;
        case BT_PROFILE_HF_AT_RESULT:
        {
            HfAtData *atdata = (HfAtData *)(((HfCallbackParms *)param)->p.data);
            printf("at result:");
            for(uint8_t i = 0; i < atdata->dataLen; i++)
            {
                printf("%c",atdata->data[i]);
            }            
            printf("\r\n");
        }
        break;
        case BT_PROFILE_HF_AUDIO_DATA:
        {
            if((enable_audio_transfer)
                &&((user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_SCO_CONNECTED))){
                memcpy(voice_phone_data,((HfCallbackParms*)param)->p.audioData->data,((HfCallbackParms*)param)->p.audioData->len);
                BtPacket *sco_packet;
                BtStatus status;
                sco_packet = btdm_malloc(sizeof(BtPacket));
                memset((void *)sco_packet, 0, sizeof(BtPacket));
                sco_packet->data = btdm_malloc(120);
                memcpy(sco_packet->data, voice_headset_data, ((HfCallbackParms*)param)->p.audioData->len);
                sco_packet->dataLen = ((HfCallbackParms*)param)->p.audioData->len;
                status = HF_SendAudioData((HfChannel *)chan, sco_packet);
                if(status != BT_STATUS_PENDING){
                    printf("no pending:%d\r\n",status);
                    btdm_free((void *)sco_packet->data);
                    btdm_free((void *)sco_packet);
                }
            }
            else{
                uint8_t audio_codec_type;
                if (((HfChannel*)chan)->codecID == 2) {
                    audio_codec_type = AUDIO_TYPE_MSBC;
                }
                else {
                    audio_codec_type = AUDIO_TYPE_PCM;
                }
                app_audio_sco_recv((((HfCallbackParms*)param)->p.audioData->errFlags == 0), audio_codec_type, ((HfCallbackParms*)param)->p.audioData->data, ((HfCallbackParms*)param)->p.audioData->len);      
            }
        }
        break;
        case BT_PROFILE_HF_SPK_VOL:
        {
            printf("HF SPK VOLUME: %d\r\n",((HfCallbackParms *)param)->p.gain);
        }
        break;
        case BT_PROFILE_HF_CURRENT_CALL:
        {
            const char *number = ((HfCallbackParms *)param)->p.callListParms->number;
            printf("current call number: ");
            for(uint8_t i = 0; i < strlen(number); i++)
            {
                printf("%c ",number[i]);
            }
            printf("\r\n");
        }
        break;
        #endif
        #if BTDM_STACK_ENABLE_A2DP_SNK || BTDM_STACK_ENABLE_A2DP_SRC
        case BT_PROFILE_A2DP_OPEN_IND:
        {
            user_bt_env.dev[dev_index].responder = TRUE;
        }
        break;
        
        case BT_PROFILE_A2DP_CONN:
        {
            user_bt_env.dev[dev_index].pstream = chan;
            user_bt_env.dev[dev_index].conFlags |= LINK_STATUS_AV_CONNECTED;
            if(((A2dpStream *)chan)->type == A2DP_STREAM_TYPE_SINK){
                user_bt_env.dev[dev_index].connect_profile |= ENABLE_PROFILE_A2DP_SINK;
            }
            else{
                user_bt_env.dev[dev_index].connect_profile |= ENABLE_PROFILE_A2DP_SOURCE;
            }
            if(true == bt_check_conn(dev_index)){
                bt_connect_act_cmp(BT_EVENT_PROFILE_CONNECT,0,rm_dev);
            }
            #if BTDM_STACK_ENABLE_AVRCP
            if(((user_bt_env.dev[dev_index].conFlags&LINK_STATUS_AVC_CONNECTED) == 0)
              && (user_bt_env.dev[dev_index].responder == FALSE)){
                vTimerSetTimerID(bt_avrcp_connect_timer,(void *)(uint32_t)dev_index);
                xTimerStart(bt_avrcp_connect_timer,portMAX_DELAY);
            }
            #endif
            AvdtpConfigRequest *req = ((A2dpCallbackParms *)param)->p.configReq;
            bt_save_codec_info(dev_index, &req->codec.elements[0]);
            printf("a2dp connected.%d\r\n",dev_index);
            printf("a2dp conn: %d,%x,%x,%x,%x\r\n",req->codec.codecType,req->codec.elements[0],req->codec.elements[1],req->codec.elements[2],req->codec.elements[3]);
        }
        break;
        
        case BT_PROFILE_A2DP_DISCONN:
        {
            user_bt_env.dev[dev_index].pstream = NULL;
            user_bt_env.dev[dev_index].conFlags &= ~LINK_STATUS_AV_CONNECTED;
            printf("a2dp disconnected.%d\r\n",dev_index);

            if(user_bt_env.dev[dev_index].connect_profile & ENABLE_PROFILE_A2DP_SOURCE){
                #if BTDM_STACK_ENABLE_A2DP_SRC
                app_audio_a2dp_source_stop();  
                #endif
                if((enable_audio_transfer) && (user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_MEDIA_PLAYING)){
                    ///earphone suspended, but phone is still streaming, restart earphone stream if needed
//                    app_audio_a2dp_sink_start(AUDIO_TYPE_SBC, 44100);
                }
            }
            
            if(user_bt_env.dev[dev_index].connect_profile & ENABLE_PROFILE_A2DP_SINK){
                #if BTDM_STACK_ENABLE_A2DP_SNK
                app_audio_a2dp_sink_stop();
                #endif
            }
            
            if(enable_audio_transfer){
                if((user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_MEDIA_PLAYING)
                    && (user_bt_env.dev[!dev_index].connect_profile & ENABLE_PROFILE_A2DP_SOURCE)){
                    /* connecting phone and earpods case*/
                    ME_SetLinkPolicy(user_bt_env.dev[!dev_index].remDev,BLP_MASTER_SLAVE_SWITCH|BLP_SNIFF_MODE);
                    A2DP_SuspendStream(user_bt_env.dev[!dev_index].pstream);
                }
            }
            system_prevent_sleep_clear(SYSTEM_PREVENT_SLEEP_A2DP_ONGOING);
        }
        break;
        
        case BT_PROFILE_A2DP_PLAYING:
        {
            AvdtpCodec       *codec;
            uint32_t sample_rate;
            user_bt_env.dev[dev_index].conFlags |= LINK_STATUS_MEDIA_PLAYING;
            printf("a2dp playing.%d\r\n",dev_index);

                /* connecting phone and earpods case*/
            if((enable_audio_transfer)
                &&(user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_AV_CONNECTED)
//                && ((user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_MEDIA_PLAYING) == 0)
                && (user_bt_env.dev[!dev_index].connect_profile & ENABLE_PROFILE_A2DP_SOURCE)){
                ME_SetLinkPolicy(user_bt_env.dev[!dev_index].remDev,BLP_MASTER_SLAVE_SWITCH);//disable sniff
                A2DP_StartStream(user_bt_env.dev[!dev_index].pstream);
            }
            else {
                if(user_bt_env.dev[dev_index].connect_profile & ENABLE_PROFILE_A2DP_SINK){
                    codec = A2DP_GetRegisteredCodec((A2dpStream *)chan);
                    if (codec->codecType == AVDTP_CODEC_TYPE_SBC) {
                        if(sbcinfo[dev_index].info.sampleFreq == 2) {
                            sample_rate = 44100;
                        }
                        else if(sbcinfo[dev_index].info.sampleFreq == 3) {
                            sample_rate = 48000;
                        }
                        else {
                            assert(0);
                        }
                        #if BTDM_STACK_ENABLE_A2DP_SNK
                        app_audio_a2dp_sink_start(AUDIO_TYPE_SBC, sample_rate);
                        #endif
                    }
                    else if (codec->codecType == AVDTP_CODEC_TYPE_MPEG2_4_AAC) {
                        if (codec->elements[1] & A2DP_AAC_CODEC_FREQ_44100) {
                            sample_rate = 44100;
                        }
                        else if (codec->elements[2] & A2DP_AAC_CODEC_FREQ_48000) {
                            sample_rate = 48000;
                        }
                        else {
                            assert(0);
                        }
                        #if BTDM_STACK_ENABLE_A2DP_SNK
                        app_audio_a2dp_sink_start(AUDIO_TYPE_AAC, sample_rate);
                        #endif
                    }            
                }            
            }
            system_prevent_sleep_set(SYSTEM_PREVENT_SLEEP_A2DP_ONGOING);
        }
        break;
        
        case BT_PROFILE_A2DP_SUSPEND:
        {
            printf("a2dp suspend.%d\r\n",dev_index);
            user_bt_env.dev[dev_index].conFlags &= ~LINK_STATUS_MEDIA_PLAYING;

            if((user_bt_env.dev[dev_index].connect_profile & ENABLE_PROFILE_A2DP_SOURCE)){
                #if BTDM_STACK_ENABLE_A2DP_SRC
                app_audio_a2dp_source_stop();
                #endif
                if((enable_audio_transfer) && (user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_MEDIA_PLAYING)){
                    ///earphone suspended, but phone is still streaming, restart earphone stream
                    ME_SetLinkPolicy(user_bt_env.dev[dev_index].remDev,BLP_MASTER_SLAVE_SWITCH);//disable sniff
                    A2DP_StartStream(user_bt_env.dev[dev_index].pstream);
                }
            }
            if(user_bt_env.dev[dev_index].connect_profile & ENABLE_PROFILE_A2DP_SINK){
                #if BTDM_STACK_ENABLE_A2DP_SNK
                app_audio_a2dp_sink_stop();
                #endif
            }      
            
            if(enable_audio_transfer){
                if((user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_MEDIA_PLAYING)
                    && (user_bt_env.dev[!dev_index].connect_profile & ENABLE_PROFILE_A2DP_SOURCE)){
                    /* connecting phone and earpods case*/
                    ME_SetLinkPolicy(user_bt_env.dev[!dev_index].remDev,BLP_MASTER_SLAVE_SWITCH|BLP_SNIFF_MODE);
                    A2DP_SuspendStream(user_bt_env.dev[!dev_index].pstream);
                }
            }

            system_prevent_sleep_clear(SYSTEM_PREVENT_SLEEP_A2DP_ONGOING);
        }
        break;
        case BT_PROFILE_A2DP_STREAM_DATA:
        {
            if((enable_audio_transfer)
               &&(user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_MEDIA_PLAYING)
               && (user_bt_env.dev[!dev_index].connect_profile & ENABLE_PROFILE_A2DP_SOURCE))
            {
                BtStatus status;
                uint16_t frame_len = sbc_calc_frame_len(&sbcinfo[dev_index]);
//                printf("send frame len=%d\r\n",frame_len);
                if(user_bt_env.bt_source_sbc_pkt_num < 10){
                    A2dpSbcPacket *packet = (A2dpSbcPacket *)btdm_malloc(sizeof(A2dpSbcPacket));
                    packet->frameSize = frame_len;
                    packet->dataLen = ((A2dpCallbackParms *)param)->len - 13;
                    packet->data = (uint8_t *)btdm_malloc(packet->dataLen);
                    memcpy(packet->data,((A2dpCallbackParms *)param)->p.data + 13,packet->dataLen);
                    status = A2DP_StreamSendSbcPacket(user_bt_env.dev[!dev_index].pstream,packet,&sbcinfo[!dev_index].info);
                    
                    if(status != BT_STATUS_PENDING){
                        printf("err sending sbc data\r\n");
                        btdm_free(packet->data);
                        btdm_free(packet);
                    }
                    else{
                        user_bt_env.bt_source_sbc_pkt_num ++;
                    }
                }
            }
            else{
                #if BTDM_STACK_ENABLE_A2DP_SNK
                app_audio_a2dp_sink_play(((A2dpCallbackParms *)param)->p.data + 13, ((A2dpCallbackParms *)param)->len - 13); 
                #endif
            }
        }    
        break;
        case BT_PROFILE_A2DP_SBC_PKT_SENT:

            if(source_sbc_frame_cnt[dev_index] > 0){
                source_sbc_frame_cnt[dev_index] --;     
            }
            if(bt_source_discard_pkt_num > 0){
                status = A2DP_StreamSendSbcPacket(user_bt_env.dev[0].pstream,((A2dpCallbackParms *)param)->p.sbcPacket,&sbcinfo[0].info);   
                if(status != BT_STATUS_PENDING){
                    printf("err sending sbc data\r\n");
                    btdm_free(((A2dpCallbackParms *)param)->p.sbcPacket->data);
                    btdm_free(((A2dpCallbackParms *)param)->p.sbcPacket);
                }
                bt_source_discard_pkt_num --;
            }else{
                btdm_free(((A2dpCallbackParms *)param)->p.sbcPacket->data);
                btdm_free(((A2dpCallbackParms *)param)->p.sbcPacket);
            user_bt_env.bt_source_sbc_pkt_num--;
            }
        break;
        #endif
        #if BTDM_STACK_ENABLE_AVRCP
        case BT_PROFILE_AVRCP_CONN:
        {
            user_bt_env.dev[dev_index].rcp_chan = chan;
            user_bt_env.dev[dev_index].conFlags |= LINK_STATUS_AVC_CONNECTED;
            if(true == bt_check_conn(dev_index)){
                bt_connect_act_cmp(BT_EVENT_PROFILE_CONNECT,0,rm_dev);
            }
        }
        break;
        
        case BT_PROFILE_AVRCP_DISCONN:
        {
            user_bt_env.dev[dev_index].rcp_chan = NULL;
            user_bt_env.dev[dev_index].conFlags &= ~LINK_STATUS_AVC_CONNECTED;
        }
        break;
        
        case BT_PROFILE_AVRCP_EVENT_SUPPORT:
        {
            uint16_t *event_mask = (uint16_t *)param;
            uint16_t event_sup = (*event_mask) & (AVRCP_ENABLE_PLAY_STATUS_CHANGED|AVRCP_ENABLE_TRACK_CHANGED|AVRCP_ENABLE_VOLUME_CHANGED);
            printf("register notify:%x\r\n",event_sup);
                bt_avrcp_register_notification(chan,event_sup);
            
//            if(user_bt_env.dev[dev_index].connect_profile&ENABLE_PROFILE_A2DP_SINK){
//                printf("register notify:%x\r\n",event_sup);
//                bt_avrcp_register_notification(chan,event_sup);
//            }
//            else{
//                bt_avrcp_register_notification(chan,AVRCP_ENABLE_VOLUME_CHANGED);
//            }
        }
        break;
        case BT_PROFILE_AVRCP_TRACK_CHANGED:
        {
            bt_get_playstatus(dev_index);
        }
        break;
        case BT_PROFILE_AVRCP_MEDIA_STATUS:
        {
            uint8_t *media_status = (uint8_t *)param;
            if((*media_status == AVRCP_MEDIA_PAUSED) || (*media_status == AVRCP_MEDIA_STOPPED)){
                printf("media paused\r\n");
                user_bt_env.dev[dev_index].conFlags &= ~LINK_STATUS_MEDIA_PLAYING;
            }
            else{
                printf("media playing\r\n");
                user_bt_env.dev[dev_index].conFlags |= LINK_STATUS_MEDIA_PLAYING;
                bt_get_playstatus(dev_index);
            }
        }
        break;
        
        case BT_PROFILE_AVRCP_MEDIA_INFO:
        {
            uint8_t numIds = *(uint8_t *)param;
            AvrcpMediaAttr *attr = (AvrcpMediaAttr *)((uint8_t *)param+4);
            printf("MEDIA INFO: attribute number %d\r\n",numIds);
            for(uint8_t i=0; i<numIds; i++)
            {
                printf("attr: %d,len: %d,string: %s\r\n",attr[i].attrId,attr[i].length,attr[i].string);
            }
            printf("\r\n");
        }
        break;
        
        case BT_PROFILE_AVRCP_MEDIA_POS:
        {
            uint32_t length = *(uint32_t *)param;
            uint32_t position = *(uint32_t *)((uint8_t *)param+4);
            AvrcpMediaStatus  mediaStatus = *(uint8_t *)((uint8_t *)param + 8);
            printf("Media pos: %d,%d,%d\r\n",length,position,mediaStatus);
            bt_get_media_info(dev_index, AVRCP_ENABLE_MEDIA_ATTR_TITLE|AVRCP_ENABLE_MEDIA_ATTR_ARTIST|AVRCP_ENABLE_MEDIA_ATTR_GENRE|AVRCP_ENABLE_MEDIA_ATTR_ALBUM);
 
        }
        break;     
        #endif
        #if BTDM_STACK_ENABLE_PBAP
        case BT_PROFILE_PBAP_CONN:
        {
            user_bt_env.dev[dev_index].conFlags |= LINK_STATUS_PBAP_CONNECTED;
        }    
        break;
        
        case BT_PROFILE_PBAP_DISCONN:
        {
            user_bt_env.dev[dev_index].pbap_client = NULL;
            user_bt_env.dev[dev_index].conFlags &= ~LINK_STATUS_PBAP_CONNECTED;
        }    
        break;
        
        case BT_PROFILE_PBAP_DATA_IND:
        {
            printf("data ind: \r\n");
            for(uint16_t i = 0; i < ((PbapClientCallbackParms *)param)->u.dataInd.len; i++)
            {
                printf("%c",((PbapClientCallbackParms *)param)->u.dataInd.buffer[i]);
            }
            printf("\r\n");
        }    
        break;
        
        case BT_PROFILE_PBAP_COMP:
            
        break;
        #endif
        #if BTDM_STACK_ENABLE_AG
        case BT_PROFILE_HFG_CONN_REQ:
            user_bt_env.dev[dev_index].responder = TRUE;
        break;
        case BT_PROFILE_HFG_CONN:
            user_bt_env.dev[dev_index].conFlags |= LINK_STATUS_HFG_CONNECTED;
            user_bt_env.dev[dev_index].hfg_chan = chan;
            user_bt_env.dev[dev_index].connect_profile |= ENABLE_PROFILE_HFG;

            ///check connection
            if(true == bt_check_conn(dev_index)){
                bt_connect_act_cmp(BT_EVENT_PROFILE_CONNECT,0,rm_dev);
            }
            #if BTDM_STACK_ENABLE_A2DP_SRC
            if(((user_bt_env.dev[dev_index].conFlags & LINK_STATUS_AV_CONNECTED) == 0)
                && (user_bt_env.enable_profiles & ENABLE_PROFILE_A2DP_SOURCE)){
                if(user_bt_env.dev[dev_index].responder == FALSE){
                    uint8_t index = bt_get_free_a2dp_source_stream();
                    if(index < NUM_STREAMS){
                        status = A2DP_OpenStream(&Stream[index], &user_bt_env.dev[dev_index].remote_bd);  
                    }                
                }
                else{
                    printf("...start a2dp timer\r\n");
                    vTimerSetTimerID(bt_a2dp_connect_timer,(void *)(uint32_t)dev_index);
                    xTimerStart(bt_a2dp_connect_timer,portMAX_DELAY);
                }
            }
            #endif
        break;
        case BT_PROFILE_HFG_DISCONN:
        {
            user_bt_env.dev[dev_index].conFlags &= ~LINK_STATUS_HFG_CONNECTED;
            user_bt_env.dev[dev_index].hfg_chan = NULL;      
        }
        break;
        
        case BT_PROFILE_HFG_AUDIO_CONN:
        {
            ///disable sniff
            ME_SetLinkPolicy(user_bt_env.dev[dev_index].remDev,BLP_MASTER_SLAVE_SWITCH);
            user_bt_env.dev[dev_index].conFlags |= LINK_STATUS_SCO_CONNECTED;
            if((enable_audio_transfer == 0)
                || (enable_audio_transfer && ((user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_HF_CONNECTED) == 0))){
                bt_free_sco_buffer_list();

                if (((HfgChannel*)chan)->codecID == 2) {
                    CMGR_SetAudioVoiceSettings(0x63);
                    app_audio_sco_start(AUDIO_TYPE_MSBC, encoded_sco_frame_cb, chan);
                }
                else {
                    CMGR_SetAudioVoiceSettings(0x60);
                    app_audio_sco_start(AUDIO_TYPE_PCM, encoded_sco_frame_cb, chan);
                }
            }
            system_prevent_sleep_set(SYSTEM_PREVENT_SLEEP_SCO_ONGOING);  
        }
        break;
        
        case BT_PROFILE_HFG_AUDIO_DISCONN:
        {
            ///enable sniff
            struct sco_data_t *sco_data;
            ME_SetLinkPolicy(user_bt_env.dev[dev_index].remDev,BLP_MASTER_SLAVE_SWITCH|BLP_SNIFF_MODE);
            user_bt_env.dev[dev_index].conFlags &= ~LINK_STATUS_SCO_CONNECTED;
            system_prevent_sleep_clear(SYSTEM_PREVENT_SLEEP_SCO_ONGOING);
            
            app_audio_sco_stop();
            bt_free_sco_buffer_list();
            #if BTDM_STACK_ENABLE_HF
            if((user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_SCO_CONNECTED)){
                app_audio_sco_start(AUDIO_TYPE_PCM, encoded_sco_frame_cb, user_bt_env.dev[0].hf_chan);
            }
            #endif
        }
        break;
        
        case BT_PROFILE_HFG_AUDIO_DATA:             
        {
            if((enable_audio_transfer)
                &&(user_bt_env.dev[!dev_index].conFlags & LINK_STATUS_SCO_CONNECTED)){
                BtPacket *sco_packet;
                sco_packet = btdm_malloc(sizeof(BtPacket));
                memset((void *)sco_packet, 0, sizeof(BtPacket));
                sco_packet->data = btdm_malloc(120);
                memcpy(sco_packet->data, voice_phone_data, ((HfCallbackParms*)param)->p.audioData->len);
                memcpy(voice_headset_data, ((HfCallbackParms*)param)->p.audioData->data, ((HfCallbackParms*)param)->p.audioData->len);
                ///loopback
                sco_packet->dataLen = ((HfCallbackParms*)param)->p.audioData->len;
                status = HFG_SendAudioData((HfgChannel *)chan, sco_packet);
                if(status != BT_STATUS_PENDING){
                    printf("no pending:%d\r\n",status);
                    btdm_free((void *)sco_packet->data);
                    btdm_free((void *)sco_packet);
                }             
            }
            else{
                uint8_t audio_codec_type;
                bool valid = (((HfgCallbackParms*)param)->p.audioData->errFlags == 0);
                if (((HfgChannel*)chan)->codecID == 2) {
                    audio_codec_type = AUDIO_TYPE_MSBC;
                    if((((HfCallbackParms*)param)->p.audioData->data[0] != 0x01)||((((HfCallbackParms*)param)->p.audioData->data[1] & 0x08) != 0x08)){
                        ///in hfg mode, data is all zero but errflags is valid, just discard it 
                        valid = false;
                    }
                }
                else {
                    audio_codec_type = AUDIO_TYPE_PCM;
                }
                app_audio_sco_recv(valid, audio_codec_type, ((HfCallbackParms*)param)->p.audioData->data, ((HfCallbackParms*)param)->p.audioData->len);      
            }
        }        
        break;
        #endif
        #if BTDM_STACK_ENABLE_SPP
        case BT_PROFILE_SPP_CONN:
        {
            user_bt_env.dev[dev_index].conFlags |= LINK_STATUS_SPP_CONNECTED;
            user_bt_env.dev[dev_index].spp_dev = chan;
            user_bt_env.dev[dev_index].connect_profile |= ENABLE_PROFILE_SPP;
            ///check connection
            if(true == bt_check_conn(dev_index)){
                bt_connect_act_cmp(BT_EVENT_PROFILE_CONNECT,0,rm_dev);
            }
        }    
        break;
        
        case BT_PROFILE_SPP_DISCONN:
        {
            user_bt_env.dev[dev_index].spp_dev = NULL;
            user_bt_env.dev[dev_index].conFlags &= ~LINK_STATUS_SPP_CONNECTED;
        }    
        break;
        #endif
        #if BTDM_STACK_ENABLE_HID
        case BT_PROFILE_HID_CONN:
        {
            user_bt_env.dev[dev_index].conFlags |= LINK_STATUS_HID_CONNECTED;
            user_bt_env.dev[dev_index].hid_chan = chan;
            user_bt_env.dev[dev_index].connect_profile |= ENABLE_PROFILE_HID;
            ///check connection
            if(true == bt_check_conn(dev_index)){
                bt_connect_act_cmp(BT_EVENT_PROFILE_CONNECT,0,rm_dev);
            }
        }    
        break;
        
        case BT_PROFILE_HID_DISCONN:
        {
            user_bt_env.dev[dev_index].hid_chan = NULL;
            user_bt_env.dev[dev_index].conFlags &= ~LINK_STATUS_HID_CONNECTED;
        }    
        break;
        #endif       
        case BT_INFO_INQ_RESULT:
        {
            BtInquiryResult    *inqResult = (BtInquiryResult *)param;
            uint8_t name[MAX_NAME_SIZE + 1];
            uint8_t len;
            printf("inq result: %x\r\n",inqResult->classOfDevice);
            printf("addr:0x%02x%02x%02x%02x%02x%02x\r\n",inqResult->bdAddr.A[0],inqResult->bdAddr.A[1],inqResult->bdAddr.A[2],
                    inqResult->bdAddr.A[3],inqResult->bdAddr.A[4],inqResult->bdAddr.A[5]);
            printf("rssi:%d,name: ",inqResult->rssi);
            bt_parse_inq_result((uint8_t *)inqResult->extInqResp,name,&len);   
            name[len] = '\0';
            for(uint8_t i = 0; i < len; i++)
            {
                printf("%c",name[i]);
            }
            printf("\r\n");
            if(len == 0){
                bt_name_query(&inqResult->bdAddr, &inqResult->psi);  
            }
        }
        break;
        
        case BT_INFO_INQ_CANCEL:
        {
            printf("inquiry cancelled\r\n");
        }
        break;
        case BT_INFO_INQ_COMP:
        {
            //do inquiry again
            ME_Inquiry(BT_IAC_GIAC, 5, 0);
        }
        break;
    }
}

CmgrHandler* bt_get_cmgr_handler(uint8_t dev_index)
{
    CmgrHandler *handler = NULL;
    if(0){}
    #if BTDM_STACK_ENABLE_HF
    else if(user_bt_env.dev[dev_index].hf_chan){
        handler = &user_bt_env.dev[dev_index].hf_chan->cmgrHandler;
    }
    #endif
    #if BTDM_STACK_ENABLE_AG
    else if(user_bt_env.dev[dev_index].hfg_chan){
        handler = &user_bt_env.dev[dev_index].hfg_chan->cmgrHandler;
    }
    #endif
    #if BTDM_STACK_ENABLE_A2DP_SNK || BTDM_STACK_ENABLE_A2DP_SRC
    else if(user_bt_env.dev[dev_index].pstream){
        handler = &user_bt_env.dev[dev_index].pstream->device->cmgrHandler;
    }
    #endif
    #if BTDM_STACK_ENABLE_SPP
    else if(user_bt_env.dev[dev_index].spp_dev){
        handler = &user_bt_env.dev[dev_index].spp_dev->cmgrHandler;
    }
    #endif
    #if BTDM_STACK_ENABLE_HID
    else if(user_bt_env.dev[dev_index].hid_chan){
        handler = &user_bt_env.dev[dev_index].hid_chan->cmgrHandler;
    }
    #endif
    return handler;
}

void bt_connect_act_cmp(uint8_t evt,uint8_t error,BtRemoteDevice *rem_dev)
{
    uint8_t dev_index;
    struct bt_connect_elt_t *elt;
    uint8_t cur_act = BT_ACTION_NULL;
    BD_ADDR *cur_addr = NULL;
    uint8_t check_op_list = false;    
    BtStatus ret;
    
    elt = (struct bt_connect_elt_t *)co_list_pick(&user_bt_env.op_list);
    if(elt){
        cur_act = elt->action;
        cur_addr = &elt->addr;
    }
        printf("bt_connect_act_cmp:evt=%d,act = %d,error=%d\r\n", evt,
            elt ? elt->action : BT_ACTION_NULL, error);
    switch(evt){
        case BT_EVENT_CON_IND:
        case BT_EVENT_CON_CNF: 
            dev_index = bt_find_dev_index(&rem_dev->bdAddr);
            
            if(dev_index >= NUM_BT_DEVICES){
                if(evt == BT_EVENT_CON_IND){
                    dev_index = bt_get_free_dev();
                    assert(dev_index < NUM_BT_DEVICES);
                }
                else{
                    printf("shall not be here...,%s,%d\r\n",__FUNCTION__,__LINE__);
                }
            }
            printf("rem dev cod:%x\r\n",rem_dev->cod);
            if(evt == BT_EVENT_CON_IND){
                user_bt_env.dev[dev_index].connect_profile = 0;
//                #if (BTDM_STACK_ENABLE_HF && BTDM_STACK_ENABLE_AG) || (BTDM_STACK_ENABLE_A2DP_SNK && BTDM_STACK_ENABLE_A2DP_SRC)
//                if((rem_dev->cod & 0x0400) == 0x0400){
//                    #if BTDM_STACK_ENABLE_AG
//                    user_bt_env.dev[dev_index].connect_profile |= ENABLE_PROFILE_HFG;
//                    #endif
//                    #if BTDM_STACK_ENABLE_A2DP_SRC
//                    user_bt_env.dev[dev_index].connect_profile |= ENABLE_PROFILE_A2DP_SOURCE;
//                    #endif
//                }
//                else{
//                    #if BTDM_STACK_ENABLE_HF
//                    user_bt_env.dev[dev_index].connect_profile |= ENABLE_PROFILE_HF;
//                    #endif
//                    #if BTDM_STACK_ENABLE_A2DP_SNK
//                    user_bt_env.dev[dev_index].connect_profile |= ENABLE_PROFILE_A2DP_SINK;
//                    #endif
//                }
//                #endif
            }
            if(user_bt_env.bt_connect_cb){
                user_bt_env.bt_connect_cb(evt,&rem_dev->bdAddr, error);
            }
            if(error == BEC_NO_ERROR){

                user_bt_env.dev[dev_index].remDev = rem_dev;
                user_bt_env.dev[dev_index].state = BT_LINK_STATE_ACL_CONNECTED;
                user_bt_env.last_active_index = dev_index;
                memcpy(&user_bt_env.dev[dev_index].remote_bd,&rem_dev->bdAddr,BD_ADDR_SIZE);
                if((memcmp(&rem_dev->bdAddr,cur_addr,BD_ADDR_SIZE) == 0)&&(cur_act == BT_ACTION_DISCONNECT)){
                    bt_do_disconnect(cur_addr,bt_get_cmgr_handler(dev_index),true);
                }
            }
            else{
                user_bt_env.dev[dev_index].state = BT_LINK_STATE_IDLE;
                memset(&user_bt_env.dev[dev_index].remote_bd,0,BD_ADDR_SIZE);
                if((cur_act == BT_ACTION_CONNECT) || (cur_act == BT_ACTION_DISCONNECT)){
                    bt_free_elt();
                    check_op_list = true;
                }
            }   

        break;           
            
        case BT_EVENT_PROFILE_CONNECT:
            dev_index = bt_find_dev_index(&rem_dev->bdAddr);
            user_bt_env.dev[dev_index].state = BT_LINK_STATE_PROFILE_CONNECTED;
            memcpy(&user_bt_env.last_dev_addr,&rem_dev->bdAddr,BD_ADDR_SIZE);

            printf("\r\nIV\r\n");
            if(user_bt_env.bt_connect_cb){
                user_bt_env.bt_connect_cb(evt,&rem_dev->bdAddr, error);
            }
            if(cur_act == BT_ACTION_CONNECT){
                bt_free_elt();
                check_op_list = true;
            }
        break;

        case BT_EVENT_DISCONNECT:
            dev_index = bt_find_dev_index(&rem_dev->bdAddr);
            user_bt_env.dev[dev_index].state = BT_LINK_STATE_IDLE;
            if(user_bt_env.bt_disconnect_cb){
                user_bt_env.bt_disconnect_cb(&rem_dev->bdAddr, error);
            }
            
            if((cur_act == BT_ACTION_DISCONNECT) || (cur_act == BT_ACTION_CONNECT)){
                bt_free_elt();
                check_op_list = true;
            }
            memset(&user_bt_env.dev[dev_index],0,sizeof(APP_DEVICE));
        break;
            
        case BT_EVENT_ACC_CHG:
            if(user_bt_env.bt_access_change_cb){
                user_bt_env.bt_access_change_cb(error);
            }
            user_bt_env.access_state = error;
            if(cur_act == BT_ACTION_ACCESS){
                bt_free_elt();
                check_op_list = true;
            }
        break;
    }

    ///check pending op list
    if(check_op_list == true){
        elt = (struct bt_connect_elt_t *)co_list_pick(&user_bt_env.op_list);
        while(elt && (check_op_list == true))
        {
            printf("check pending list act=%d,state=%d,addr0=%x\r\n",elt->action,user_bt_env.dev[0].state,user_bt_env.dev[0].remote_bd.A[0]);
            switch(elt->action){
                case BT_ACTION_CONNECT:
                {
                    dev_index = bt_find_dev_index(&elt->addr);
                    if(dev_index >= NUM_BT_DEVICES){
                        //device not exist, find free dev
                        dev_index = bt_get_free_dev();
                        if(dev_index < NUM_BT_DEVICES){
                            //get free dev, create new connection
                            void *chan;
                            ret = bt_do_connect(&elt->addr, elt->profiles, &chan);
                            if(ret == BT_STATUS_PENDING){
                                user_bt_env.cur_action = BT_ACTION_CONNECT;
                                user_bt_env.dev[dev_index].connect_profile = elt->profiles;
                                user_bt_env.last_connect_profile = elt->profiles; 
                                user_bt_env.dev[dev_index].state = BT_LINK_STATE_CONNECTING;
                                memcpy(&user_bt_env.dev[dev_index].remote_bd,&elt->addr,sizeof(BD_ADDR));
                                #if BTDM_STACK_ENABLE_HF
                                if(elt->profiles & ENABLE_PROFILE_HF){
                                    user_bt_env.dev[dev_index].hf_chan = chan;
                                }
                                #endif
                                check_op_list = false;
                            }
                            else{
                                bt_free_elt();
                            }
                        }
                        else{
                            //no free dev
                            bt_free_elt();
                        }
                    }
                    else{
                        //device exist,check device state
                        if(user_bt_env.dev[dev_index].state == BT_LINK_STATE_PROFILE_CONNECTED){
                            //profile connected,return success
                            bt_free_elt();
                        }else{
                            check_op_list = false;
                        }
                    }
                }
                break;
                case BT_ACTION_DISCONNECT:
                {
                    dev_index = bt_find_dev_index(&elt->addr);
                    if(dev_index >= NUM_BT_DEVICES){
                        //don't find device
                        bt_free_elt();
                    }
                    else{
                        if(user_bt_env.dev[dev_index].state == BT_LINK_STATE_PROFILE_CONNECTED){
                            //profile connected,return success
                            user_bt_env.dev[dev_index].state = BT_LINK_STATE_DISCONNECTING;
                            user_bt_env.cur_action = BT_ACTION_DISCONNECT;
                            bt_do_disconnect(&elt->addr,NULL,false);
                        }
                        //other state,just return
                        check_op_list = false;
                    }
                }
                break;
                case BT_ACTION_ACCESS:
                {
                    printf("shall not be here..error action access change.");
                    bt_free_elt();
                }
                break;
            }
            elt = (struct bt_connect_elt_t *)co_list_pick(&user_bt_env.op_list);
        }
    }    
}


void bt_access_change_cb(uint8_t mode)
{
    printf("access change cb: %d\r\n",mode);
}

BtStatus bt_connect(BD_ADDR *addr, uint8_t profiles)
{
    BtStatus ret = BT_STATUS_PENDING;
    uint8_t dev_index = 0;
    struct bt_connect_elt_t *elt;
    uint8_t add_to_list = false;
    
    printf("bt connect: %x,%x,%x\r\n",addr->A[0],addr->A[1],profiles);

    //param check
    assert(addr != NULL);
    
//    //error bt state, return failed
//    for(index = 0; index < NUM_BT_DEVICES; index++){
//        if((user_bt_env.dev[index].state == BT_LINK_STATE_CONNECTING) || (user_bt_env.dev[index].state == BT_LINK_STATE_DISCONNECTING)){
//            return BT_STATUS_FAILED;
//        }
//    }

    elt = (struct bt_connect_elt_t *)btdm_malloc(sizeof(struct bt_connect_elt_t));
    memcpy(&elt->addr,addr,BD_ADDR_SIZE);
    elt->action = BT_ACTION_CONNECT;
    if(co_list_is_empty(&user_bt_env.op_list)){
        //list empty, check device index
        dev_index = bt_find_dev_index(addr);
        if(dev_index >= NUM_BT_DEVICES){
            //not connection with the addr
            dev_index = bt_get_free_dev();
            if(dev_index >= NUM_BT_DEVICES){
                //no free device
                printf("err...dev state %d\r\n",user_bt_env.dev[0].state);
                btdm_free(elt);
                return BT_STATUS_NO_RESOURCES;
            }
        }
        printf("connect state=%d\r\n",user_bt_env.dev[dev_index].state);
        if(user_bt_env.dev[dev_index].state == BT_LINK_STATE_IDLE){
            //idle state, do connect imediately
             void *chan;
            ret = bt_do_connect(addr,profiles,&chan);
            if(ret == BT_STATUS_PENDING){
                add_to_list = true;
                user_bt_env.cur_action = BT_ACTION_CONNECT;
                user_bt_env.dev[dev_index].state = BT_LINK_STATE_CONNECTING;
                user_bt_env.dev[dev_index].connect_profile = profiles;
                user_bt_env.last_connect_profile = profiles; 
                memcpy(&user_bt_env.dev[dev_index].remote_bd,addr,sizeof(BD_ADDR));
                if(0){}
                #if BTDM_STACK_ENABLE_HF
                else if(profiles & ENABLE_PROFILE_HF){
                    user_bt_env.dev[dev_index].hf_chan = chan;
                }
                #endif
                #if BTDM_STACK_ENABLE_AG
                else if(profiles & ENABLE_PROFILE_HFG){
                    user_bt_env.dev[dev_index].hfg_chan = chan;
                }
                #endif
                #if BTDM_STACK_ENABLE_A2DP_SNK || BTDM_STACK_ENABLE_A2DP_SRC
                else if(profiles & (ENABLE_PROFILE_A2DP_SINK | ENABLE_PROFILE_A2DP_SOURCE)){
                    user_bt_env.dev[dev_index].pstream = chan;
                }
                #endif
                #if BTDM_STACK_ENABLE_SPP
                else if(profiles & ENABLE_PROFILE_SPP){
                    user_bt_env.dev[dev_index].spp_dev = chan;
                }
                #endif
                #if BTDM_STACK_ENABLE_HID
                else if(profiles & ENABLE_PROFILE_HID){
                    user_bt_env.dev[dev_index].hid_chan = chan;
                }
                #endif
            }
            else{
                btdm_free(elt);
            }
        }
        else if(user_bt_env.dev[dev_index].state == BT_LINK_STATE_DISCONNECTING){
            //disconnecting,usally shall not be here
            add_to_list = true;
        }
        else if((user_bt_env.dev[dev_index].state == BT_LINK_STATE_CONNECTING)
            || (user_bt_env.dev[dev_index].state == BT_LINK_STATE_ACL_CONNECTED)){
            //connecting or acl connected,just free connect elt,return pending
            btdm_free(elt);
        }
        else if(user_bt_env.dev[dev_index].state == BT_LINK_STATE_PROFILE_CONNECTED){
            //profile connected,return success
            btdm_free(elt);
            ret = BT_STATUS_SUCCESS;
        }
    }else{
        add_to_list = true;
    }
    
    if(add_to_list == true){
        elt->profiles = profiles;
        GLOBAL_INT_DISABLE();
        co_list_push_back(&user_bt_env.op_list,&elt->hdr);
        user_bt_env.action_cnt ++;
        GLOBAL_INT_RESTORE();
    }
    
    return ret;
}

BtStatus bt_disconnect(BD_ADDR *addr, uint8_t force_disconnect) 
{
    BtStatus status = BT_STATUS_PENDING;
    uint8_t dev_index = 0;
    struct bt_connect_elt_t *elt,*cur_elt;
    uint8_t add_to_list = false;
    printf("bt disconnect\r\n");
    if(addr == NULL){
        return BT_STATUS_FAILED;
    }
    elt = (struct bt_connect_elt_t *)btdm_malloc(sizeof(struct bt_connect_elt_t));
    memcpy(&elt->addr,addr,BD_ADDR_SIZE);
    elt->action = BT_ACTION_DISCONNECT;
    
    if((co_list_is_empty(&user_bt_env.op_list)) || (force_disconnect == true)){
        dev_index = bt_find_dev_index(addr);
        printf("cur state=%d\r\n",user_bt_env.dev[dev_index].state);
        if(dev_index < NUM_BT_DEVICES){
            if(user_bt_env.dev[dev_index].state == BT_LINK_STATE_IDLE){
                memset(&user_bt_env.dev[dev_index].remote_bd,0,sizeof(BD_ADDR));
                status = BT_STATUS_SUCCESS;
            }
            else if(user_bt_env.dev[dev_index].state == BT_LINK_STATE_DISCONNECTING){
                add_to_list = false;
                if(force_disconnect == true){
//                    cur_elt = (struct bt_connect_elt_t *)co_list_pick(&user_bt_env.op_list);
                    bt_do_disconnect(addr,bt_get_cmgr_handler(dev_index),force_disconnect);
                }
            }
            else if((user_bt_env.dev[dev_index].state == BT_LINK_STATE_CONNECTING)
                || (user_bt_env.dev[dev_index].state == BT_LINK_STATE_ACL_CONNECTED))
            {
                add_to_list = true;
                if(force_disconnect == true){
                    GLOBAL_INT_DISABLE();
                    cur_elt = (struct bt_connect_elt_t *)co_list_pick(&user_bt_env.op_list);
                    if(cur_elt && (cur_elt->action == BT_ACTION_CONNECT)){
                        co_list_pop_front(&user_bt_env.op_list);
                        user_bt_env.action_cnt --;
                        btdm_free(cur_elt);
                    }
                    GLOBAL_INT_RESTORE();
                    user_bt_env.dev[dev_index].state = BT_LINK_STATE_DISCONNECTING;
                    status = bt_do_disconnect(addr,bt_get_cmgr_handler(dev_index),force_disconnect);
                    if(status != BT_STATUS_PENDING){
                        add_to_list = false;
                        user_bt_env.dev[dev_index].state = BT_LINK_STATE_IDLE;
                    }

                }
            }
            else if(user_bt_env.dev[dev_index].state == BT_LINK_STATE_PROFILE_CONNECTED){
                user_bt_env.dev[dev_index].state = BT_LINK_STATE_DISCONNECTING;
                bt_do_disconnect(addr,NULL,force_disconnect);
                add_to_list = true;
            }
        }
        else{
            btdm_free(elt);
            return BT_STATUS_FAILED;
        }
    }
    else{
        add_to_list = true;
    }
    
    if(add_to_list == true){
        printf("add to list\r\n");
        GLOBAL_INT_DISABLE();
        co_list_push_back(&user_bt_env.op_list,&elt->hdr);
        user_bt_env.action_cnt ++;
        GLOBAL_INT_RESTORE();
    }
    else{
        btdm_free(elt);
    }
    
    return status;
}

void user_bt_init(void)
{
    struct bt_connect_elt_t *elt;
    /*
        释放目前的user_bt_env所有结构体。
    */
    while(!co_list_is_empty(&user_bt_env.op_list))
    {
        user_bt_env.action_cnt --;
        elt = (struct bt_connect_elt_t *)co_list_pop_front(&user_bt_env.op_list);
        btdm_free((void *)elt);
    }
    memset((void *)&user_bt_env,0,sizeof(struct user_bt_env_t));
    co_list_init(&user_bt_env.op_list);
    user_bt_env.bt_disconnect_cb = bt_disconnect_cb;
    user_bt_env.bt_connect_cb = bt_connect_cb;
    user_bt_env.bt_access_change_cb = bt_access_change_cb;
            /*          更改为spp           */
    //user_bt_env.enable_profiles = ENABLE_PROFILE_HFG | ENABLE_PROFILE_A2DP_SINK | ENABLE_PROFILE_HF | ENABLE_PROFILE_A2DP_SOURCE;
    user_bt_env.enable_profiles = ENABLE_PROFILE_SPP;
    #if BTDM_STACK_ENABLE_HF
    user_bt_env.enable_profiles &= ~ENABLE_PROFILE_HF;
    #endif
    #if BTDM_STACK_ENABLE_AG
    user_bt_env.enable_profiles &= ~ENABLE_PROFILE_HFG;
    #endif    
    #if BTDM_STACK_ENABLE_A2DP_SNK
    user_bt_env.enable_profiles &= ~ENABLE_PROFILE_A2DP_SINK;
    #endif    
    #if BTDM_STACK_ENABLE_A2DP_SRC
    user_bt_env.enable_profiles &= ~ENABLE_PROFILE_A2DP_SOURCE;
    #endif
    #if BTDM_STACK_ENABLE_SPP
    user_bt_env.enable_profiles &= ~ENABLE_PROFILE_SPP;
    #endif
    #if BTDM_STACK_ENABLE_HID
    user_bt_env.enable_profiles &= ~ENABLE_PROFILE_HID;
    #endif    
    
    user_bt_env.enable_local_ring = false;
    user_bt_env.connect_times = MAX_RECONNECT_TIMES;
    user_bt_env.page_timeout = PAGE_TIMEOUT;
    ME_SetPageTimeout(user_bt_env.page_timeout); 
    
    #if BTDM_STACK_ENABLE_HF && BTDM_STACK_ENABLE_AG
//    if(enable_audio_transfer){
//        CMGR_SetAudioVoiceSettings(0x60);
//        ///only support cvsd
//        HFG_SetSupportedFeature(HFG_SDK_FEATURES&~HFG_FEATURE_CODEC_NEGOTIATON);
//        HF_SetSupportedFeature(HF_SDK_FEATURES&~HF_FEATURE_CODEC_NEGOTIATION_CFG);
//    }
    #endif

    BtScoTxParms customParms;
    customParms.voiceSetting = CMGR_GetAudioVoiceSettings();
    customParms.maxLatency = 0x000D;
    if(enable_audio_transfer){
        customParms.retransmissionEffort = 0x01;
    }
    else{
        customParms.retransmissionEffort = 0x02;
    }
    customParms.eScoPktType = BESPT_EV3 | BESPT_NO_2_EV5 | BESPT_NO_3_EV5 | BESPT_NO_3_EV3;
    customParms.transmitBandwidth = 0x1f40;
    customParms.receiveBandwidth = 0x1f40;

    CMGR_SetAudioDefaultParms(CMGR_AUDIO_PARMS_CUSTOM);
    CMGR_SetAudioCustomParms(&customParms);
    
    #if BTDM_STACK_ENABLE_A2DP_SNK || BTDM_STACK_ENABLE_A2DP_SRC
    for (uint8_t i=0; i<NUM_BT_DEVICES; i++) {
        sbcinfo[i].info.allocMethod = 1; //1--loudness, 2-- snr
        sbcinfo[i].info.sampleFreq = 2; // 0--16k, 1--32k, 2--44.1k, 3--48k
        sbcinfo[i].info.bitPool = 0x35;
        sbcinfo[i].info.channelMode = 3; // 0--mono, 1-- dual_chal, 2--stereo, 3--joint stereo
        sbcinfo[i].info.numBlocks = 16;
        sbcinfo[i].info.numChannels = 2;
        sbcinfo[i].info.numSubBands = 8;
        sbcinfo[i].elem[0] = 0x21;
        sbcinfo[i].elem[1] = 0x15;
        sbcinfo[i].elem[2] = 0x02;
        sbcinfo[i].elem[3] = 0x35;
    }     
    #endif
    if(bt_connect_timer == NULL){
        bt_connect_timer = xTimerCreate("bt_connect_timer", 10, pdFALSE, 0, bt_connect_timer_cb );
    }
    #if BTDM_STACK_ENABLE_AVRCP
    if(bt_avrcp_connect_timer == NULL){
        bt_avrcp_connect_timer = xTimerCreate("bt_avrcp_connect_timer", 500, pdFALSE, 0, bt_avrcp_connect_timer_cb );
    }
    #endif
    #if BTDM_STACK_ENABLE_A2DP_SRC    
    if(bt_a2dp_connect_timer == NULL){
        bt_a2dp_connect_timer = xTimerCreate("bt_a2dp_connect_timer", 4000, pdFALSE, 0, bt_a2dp_connect_timer_cb );
    }
    #endif
    if(bt_linkloss_connect_timer == NULL){
        bt_linkloss_connect_timer = xTimerCreate("bt_linkloss_connect_timer", 500, pdFALSE, 0, bt_linkloss_connect_timer_cb );
    }
    
    ///example for power on reconnect(as AG&Source)
//    BtStatus status;
//    BD_ADDR addr; 
//    extern bool host_get_bt_last_device(BD_ADDR *addr);
//    bool ret = host_get_bt_last_device(&addr);
//    if(ret == true){
//        user_bt_env.connect_times = 3;
//        status = bt_connect(&addr, ENABLE_PROFILE_HFG|ENABLE_PROFILE_A2DP_SOURCE);
//        printf("status = %d,%x,%x\r\n",status,addr.A[0],addr.A[1]);                    
//    }
//    else{
//        printf("no saved device\r\n");
//    }
}

uint8_t user_bt_get_state(uint8_t dev_index)
{
    uint8_t state = BT_STATE_IDLE;
    
    switch(user_bt_env.dev[dev_index].state)
    {
        case BT_LINK_STATE_IDLE:
            state = BT_STATE_IDLE;
            break;
        
        case BT_LINK_STATE_CONNECTING:
            state = BT_STATE_CONNECTING;
            break;
        
        case BT_LINK_STATE_DISCONNECTING:
            state = BT_STATE_DISCONNECTING;
            break;
        
        case BT_LINK_STATE_ACL_CONNECTED:
        case BT_LINK_STATE_PROFILE_CONNECTED:
        {
            state = BT_STATE_CONNECTED;
            if(0){}
            #if BTDM_STACK_ENABLE_HF
            else if(user_bt_env.dev[dev_index].active == HF_CALL_ACTIVE){
                state = BT_STATE_HFP_CALLACTIVE;
            }
            else if(user_bt_env.dev[dev_index].setup_state == HF_CALL_SETUP_IN){
                state = BT_STATE_HFP_INCOMMING;
            }
            else if((user_bt_env.dev[dev_index].setup_state == HF_CALL_SETUP_OUT)
                ||(user_bt_env.dev[dev_index].setup_state == HF_CALL_SETUP_ALERT)){
                state = BT_STATE_HFP_OUTGOING;
            }
            #endif
            #if BTDM_STACK_ENABLE_A2DP_SNK || BTDM_STACK_ENABLE_A2DP_SRC
            else if(user_bt_env.dev[dev_index].conFlags & LINK_STATUS_MEDIA_PLAYING){
                state = BT_STATE_MEDIA_PLAYING;
            }    
            #endif
        }
        break;        
    }
    
    return state;
}

BtStatus bt_enter_pairing(uint8_t access, BtAccessModeInfo *info)
{
    BtStatus status = BT_STATUS_PENDING;
    struct bt_connect_elt_t *elt;
    printf("bt enter pairing\r\n");

    if((access == BAM_NOT_ACCESSIBLE) || (access > BAM_GENERAL_ACCESSIBLE)){
        return BT_STATUS_INVALID_PARM;
    }
    if(user_bt_env.access_state == access){
        return BT_STATUS_SUCCESS;
    }
    
    //TBD,only change nonconnected access state
    status = ME_SetAccessibleModeNC(access, info);
    if(status == BT_STATUS_PENDING){
        elt = (struct bt_connect_elt_t *)btdm_malloc(sizeof(struct bt_connect_elt_t));
        elt->action = BT_ACTION_ACCESS;
        GLOBAL_INT_DISABLE();
        co_list_push_back(&user_bt_env.op_list,&elt->hdr);
        user_bt_env.action_cnt ++;
        GLOBAL_INT_RESTORE();
    }
    
    return status;
}

BtStatus bt_exit_pairing(void)
{
    BtStatus status = BT_STATUS_PENDING;
    struct bt_connect_elt_t *elt;

    status = ME_SetAccessibleModeNC(BAM_NOT_ACCESSIBLE, NULL);
    if(status == BT_STATUS_PENDING){
        elt = (struct bt_connect_elt_t *)btdm_malloc(sizeof(struct bt_connect_elt_t));
        elt->action = BT_ACTION_ACCESS;
        GLOBAL_INT_DISABLE();
        co_list_push_back(&user_bt_env.op_list,&elt->hdr);
        user_bt_env.action_cnt ++;
        GLOBAL_INT_RESTORE();
    }

    return status;
}
#if BTDM_STACK_ENABLE_HF
BtStatus bt_answer_call(uint8_t dev_index)
{
    HfCommand *cmd;
    uint8_t state = user_bt_get_state(dev_index);
    
    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].hf_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }
    
    if(state != BT_STATE_HFP_INCOMMING){
        return BT_STATUS_FAILED;
    }
    cmd = (HfCommand *)btdm_malloc(sizeof(HfCommand));
    return HF_AnswerCall(user_bt_env.dev[dev_index].hf_chan, cmd);
}

BtStatus bt_dial_number(uint8_t dev_index, uint8_t *number, uint16_t len)
{

    HfCommand *cmd;
    
    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].hf_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }

    cmd = (HfCommand *)btdm_malloc(sizeof(HfCommand));
    return HF_DialNumber(user_bt_env.dev[dev_index].hf_chan, number, len, cmd);
}

BtStatus bt_redial(uint8_t dev_index)
{
    HfCommand *cmd;
    
    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].hf_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }

    cmd = (HfCommand *)btdm_malloc(sizeof(HfCommand));
    return HF_Redial(user_bt_env.dev[dev_index].hf_chan, cmd);
}

BtStatus bt_call_hold(uint8_t dev_index, HfHoldAction action, uint8_t index)
{
    HfCommand *cmd;
    
    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].hf_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }

    cmd = (HfCommand *)btdm_malloc(sizeof(HfCommand));
    return HF_CallHold(user_bt_env.dev[dev_index].hf_chan, action, index, cmd);
}

BtStatus bt_hang_up(uint8_t dev_index)
{
    HfCommand *cmd;
    uint8_t state = user_bt_get_state(dev_index);

    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].hf_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }
    printf("state = %d\r\n",state);
    if((state != BT_STATE_HFP_INCOMMING) && (state != BT_STATE_HFP_OUTGOING) && (state != BT_STATE_HFP_CALLACTIVE)){
        return BT_STATUS_FAILED;
    }
    cmd = (HfCommand *)btdm_malloc(sizeof(HfCommand));
    return HF_Hangup(user_bt_env.dev[dev_index].hf_chan, cmd);
}

BtStatus bt_list_current_calls(uint8_t dev_index)
{
    HfCommand *cmd;
    uint8_t state = user_bt_get_state(dev_index);

    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].hf_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }
    printf("state = %d\r\n",state);

    if((state != BT_STATE_HFP_INCOMMING) && (state != BT_STATE_HFP_OUTGOING) && (state != BT_STATE_HFP_CALLACTIVE)){
        return BT_STATUS_FAILED;
    }
    cmd = (HfCommand *)btdm_malloc(sizeof(HfCommand));
    return HF_ListCurrentCalls(user_bt_env.dev[dev_index].hf_chan, cmd);
}

BtStatus bt_transfer_sco(uint8_t dev_index)
{
    HfCommand *cmd;
    uint8_t state = user_bt_get_state(dev_index);

    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].hf_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }
    if(state < BT_STATE_CONNECTED){
        return BT_STATUS_FAILED;
    }
    printf("conFlags = %d\r\n",user_bt_env.dev[dev_index].conFlags);
    if(user_bt_env.dev[dev_index].conFlags & LINK_STATUS_SCO_CONNECTED){
        return HF_DisconnectAudioLink(user_bt_env.dev[dev_index].hf_chan);
    }else{
        cmd = (HfCommand *)btdm_malloc(sizeof(HfCommand));
        //return HF_CreateAudioLink(user_bt_env.dev[dev_index].hf_chan);
        return HF_CodecConnectionReq(user_bt_env.dev[dev_index].hf_chan, cmd);
    }
}

BtStatus bt_send_dtmf(uint8_t dev_index, uint8_t dtmf)
{
    HfCommand *cmd;

    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].hf_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }

    cmd = (HfCommand *)btdm_malloc(sizeof(HfCommand));
    return HF_GenerateDtmf(user_bt_env.dev[dev_index].hf_chan, dtmf, cmd);
}

BtStatus bt_report_mic_volume(uint8_t dev_index, uint8_t vol)
{
    HfCommand *cmd;
    uint8_t state = user_bt_get_state(dev_index);

    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].hf_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }
    if((state != BT_STATE_HFP_INCOMMING) && (state != BT_STATE_HFP_OUTGOING) && (state != BT_STATE_HFP_CALLACTIVE)){
        return BT_STATUS_FAILED;
    }
    
    cmd = (HfCommand *)btdm_malloc(sizeof(HfCommand));
    return HF_ReportMicVolume(user_bt_env.dev[dev_index].hf_chan, vol, cmd);
}

BtStatus bt_report_spk_volume(uint8_t dev_index, uint8_t vol)
{
    HfCommand *cmd;
    uint8_t state = user_bt_get_state(dev_index);

    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].hf_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }
    if((state != BT_STATE_HFP_INCOMMING) && (state != BT_STATE_HFP_OUTGOING) && (state != BT_STATE_HFP_CALLACTIVE)){
        return BT_STATUS_FAILED;
    }
    
    cmd = (HfCommand *)btdm_malloc(sizeof(HfCommand));
    return HF_ReportSpeakerVolume(user_bt_env.dev[dev_index].hf_chan, vol, cmd);
}

BtStatus bt_send_hf_cmd(uint8_t dev_index, const uint8_t *at_str)
{
    HfCommand *cmd;

    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].hf_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }

    cmd = (HfCommand *)btdm_malloc(sizeof(HfCommand));
    return HF_SendAtCommand(user_bt_env.dev[dev_index].hf_chan, (const char*)at_str, cmd);
}

BtStatus bt_enable_voice_recognition(uint8_t dev_index, uint8_t enabled)
{
    HfCommand *cmd;

    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].hf_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }

    cmd = (HfCommand *)btdm_malloc(sizeof(HfCommand));
    return HF_EnableVoiceRecognition(user_bt_env.dev[dev_index].hf_chan, enabled, cmd);
}

uint8_t bt_is_voice_rec_active(uint8_t dev_index)
{
    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].hf_chan == NULL)){
        return 0;
    }
    return HF_IsVoiceRecActive(user_bt_env.dev[dev_index].hf_chan); 
}

BtStatus bt_enable_caller_id_notify(uint8_t dev_index, uint8_t enabled)
{
    HfCommand *cmd;

    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].hf_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }

    cmd = (HfCommand *)btdm_malloc(sizeof(HfCommand));
    return HF_EnableCallerIdNotify(user_bt_env.dev[dev_index].hf_chan, enabled, cmd);
}
#endif
#if BTDM_STACK_ENABLE_AVRCP
BtStatus bt_set_media_volume(uint8_t dev_index, uint8_t volume)
{
    AvrcpAdvancedPdu *pdu;
    BtStatus status;
    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].rcp_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }

    pdu = (AvrcpAdvancedPdu *)btdm_malloc(sizeof(AvrcpAdvancedPdu));
    pdu->parms = (uint8_t *)btdm_malloc(64);
    status = AVRCP_TgSetAbsoluteVolume(user_bt_env.dev[dev_index].rcp_chan, pdu,volume);
    if (status != BT_STATUS_PENDING) {
        btdm_free(pdu->parms);
        btdm_free(pdu);
    }
    return status;
}
#if AVRCP_BROWSING_CONTROLLER == XA_ENABLED
BtStatus bt_get_mediaplayer(uint8_t dev_index)
{
    AvrcpAdvancedPdu *pdu;
    BtStatus status;
    
    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].rcp_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }
    pdu = (AvrcpAdvancedPdu *)btdm_malloc(sizeof(AvrcpAdvancedPdu));
    pdu->parms = (uint8_t *)btdm_malloc(64);
    printf("alloc %x\r\n",pdu->parms);

    status = AVRCP_CtGetFolderItems(user_bt_env.dev[dev_index].rcp_chan, pdu,AVRCP_SCOPE_MEDIA_PLAYER_LIST,0,9,0,NULL);
    if (status != BT_STATUS_PENDING) {
        btdm_free(pdu->parms);
        btdm_free(pdu);
    }
    return status;
}
#endif
BtStatus bt_get_playstatus(uint8_t dev_index)
{
    AvrcpAdvancedPdu *pdu;
    BtStatus status;
    
    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].rcp_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }
    pdu = (AvrcpAdvancedPdu *)btdm_malloc(sizeof(AvrcpAdvancedPdu));
    pdu->parms = (uint8_t *)btdm_malloc(64);
    status = AVRCP_CtGetPlayStatus(user_bt_env.dev[dev_index].rcp_chan, pdu);
    if (status != BT_STATUS_PENDING) {
        btdm_free(pdu->parms);
        btdm_free(pdu);
    }
    return status;
}

BtStatus bt_get_media_info(uint8_t dev_index,AvrcpMediaAttrIdMask mediaMask)
{
    AvrcpAdvancedPdu *pdu;
    BtStatus status;
    
    if((dev_index >= NUM_BT_DEVICES) || (user_bt_env.dev[dev_index].rcp_chan == NULL)){
        return BT_STATUS_INVALID_PARM;
    }
    pdu = (AvrcpAdvancedPdu *)btdm_malloc(sizeof(AvrcpAdvancedPdu));
    pdu->parms = (uint8_t *)btdm_malloc(64);
    status = AVRCP_CtGetMediaInfo(user_bt_env.dev[dev_index].rcp_chan, pdu, mediaMask);
    if (status != BT_STATUS_PENDING) {
        btdm_free(pdu->parms);
        btdm_free(pdu);
    }
    return status;
}
#endif
#endif
