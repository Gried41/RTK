#include <string.h>

#include "co_log.h"

#include "gap_api.h"
#include "gatt_api.h"
#include "gaf_api.h"
#include "FreeRTOS.h"
#include "timers.h"
//#include "user_bt.h"
#include "hid_service.h"
#include "simple_gatt_service.h"
#include "ANCS_AMS_client.h"
#include "AMS_client.h"

#define APP_BLE_ADV_CHN_MAX     2


static TimerHandle_t ble_sec_req_timer = NULL;
static void app_ble_start_advertising(uint8_t adv_chn);

char local_device_name[] = "30xx_ble";
static adv_handle adv[APP_BLE_ADV_CHN_MAX];
static uint16_t mtu_connected[8];

extern const uint8_t ble_static_addr[];

uint8_t slave_link_conidx;
static uint8_t adv_data[] = {
       
	#if 0
    /* gatt service information */
    0x03,                                //length of this AD
    GAP_ADVTYPE_16BIT_MORE,              //16bit service uuid AD type
    //0xff, 0xf0,                        //value.service uuid:0xFFF0
    LO_UINT16(0XFF72),
    HI_UINT16(0XFF72),
	#endif
	
    /* local device name information */
    0x0C,                                                                           //length of this AD
    GAP_ADVTYPE_LOCAL_NAME_COMPLETE,                                                //complete name AD type
    'S','m','a','r','t',' ','W','a','t','c','h',    //value.local device name 
	
    0x09,                                           // addr mac 
    GAP_ADVTYPE_MANUFACTURER_SPECIFIC,
    LO_UINT16(0x1671),
    HI_UINT16(0x1671),
    0xc2,0x12,0x12,0x12,0x00,0x13,

    0x03,   // length of this data
    GAP_ADVTYPE_APPEARANCE,
    LO_UINT16(0x03c0),
    HI_UINT16(0x03c0),	


	#if 0
    // appearance
    0x03,   // length of this data
    GAP_ADVTYPE_APPEARANCE,
    LO_UINT16(GAP_APPEARE_HID_KEYBOARD),
    HI_UINT16(GAP_APPEARE_HID_KEYBOARD),
	#endif

         
};

/*
 *  Advertising scan response data, max size is 31 bytes
*/
static uint8_t adv_scan_rsp_data[] = {

	#if 1
    0x03,   // length of this data
    GAP_ADVTYPE_16BIT_COMPLETE,
    LO_UINT16(HID_SERV_UUID),
    HI_UINT16(HID_SERV_UUID),
    #endif

	#if 0
    // Tx power level ���书��
    0x02,                       // length of this data
    GAP_ADVTYPE_POWER_LEVEL,
    0,                          // 0dBm
	#endif
};

static uint8_t client_id;

uint16_t app_ble_get_mtu(uint8_t conidx)
{
    return mtu_connected[conidx];
}

void app_ble_set_mtu(uint8_t conidx, uint16_t mtu)
{
    mtu_connected[conidx] = mtu;
}

static void ble_sec_req_timer_cb(TimerHandle_t pxTimer)
{
    gap_security_req(0);
}

static uint16_t gap_callback(struct gap_event *event)
{
    printf("gap_callback: type = %d\r\n", event->type);

    switch(event->type) {
    case GATT_EVT_PROFILE_ADDED:
        {
            printf("gap_callback: GATT_EVT_PROFILE_ADDED: 0x%02X\r\n", event->param.profile_added_status);

            /* service profile has been added successfully, then the advertising can be started */
            app_ble_start_advertising(0);
        }
        break;

    case GAP_EVT_ADV_SET_PARAM:
        printf("adv param set: 0x%02X\r\n", event->param.adv_set_param.status);
        break;

    case GAP_EVT_ADV_SET_ADV_DATA:
        printf("adv data set: 0x%02X\r\n", event->param.adv_set_adv_data.status);
        break;

    case GAP_EVT_ADV_SET_SCAN_RSP:
        printf("adv scan rsp data set: 0x%02X\r\n", event->param.adv_set_scan_rsp.status);
        break;

    case GAP_EVT_ADV_START:
        printf("adv start :0x%02X\r\n", event->param.adv_start.status);
        break;

    case GAP_EVT_ADV_END:
        printf("adv end: 0x%02X\r\n", event->param.adv_end.status);
        break;

    case GAP_EVT_SLAVE_CONNECT:
        {
            //gap_get_link_version(event->param.connect.conidx);
            //gap_get_link_rssi(event->param.connect.conidx);
            //gap_get_link_features(event->param.connect.conidx);
            printf("slave connect[%d], connect num: %d\r\n", event->param.connect.conidx, gap_get_connect_num());
//            gatt_mtu_exchange_req(service_id, event->param.connect.conidx, 247);
            if(ble_sec_req_timer == NULL){
                ble_sec_req_timer = xTimerCreate("ble_sec_req_timer", 1000, pdFALSE, 0, ble_sec_req_timer_cb );
            }
            xTimerStart(ble_sec_req_timer,portMAX_DELAY);
        }
        break;

    case GAP_EVT_DISCONNECT:
        {
            printf("gap_callback: GAP_EVT_DISCONNECT, conidx:%d, reason:0x%02X\r\n", event->param.disconnect.conidx,
                                                                                    event->param.disconnect.reason);
            app_ble_start_advertising(0);
        }
        break;
	
    case GATT_EVT_MTU:
		printf("gap_callback: conidx: %d, GATT_EVT_MTU: %d\r\n", event->param.mtu_ind.conidx, event->param.mtu_ind.mtu);
        app_ble_set_mtu(event->param.mtu_ind.conidx, event->param.mtu_ind.mtu);
        //gap_security_req(event->param.connect.conidx);
        break;
    case GAP_EVT_SMP_ENCRYPT_SUCCESS:
        printf("gap_callback: conidx: %d, GAP_EVT_SMP_ENCRYPT_SUCCESS\r\n", event->param.mtu_ind.conidx);
        break;
    
    case GAP_EVT_SMP_BOND_SUCCESS:
        printf("gap_callback: conidx: %d, GAP_EVT_SMP_BOND_SUCCESS\r\n", event->param.mtu_ind.conidx);
#if BLE_PROFILE_ENABLE_ANCS
        gatt_discovery_all_peer_svc(ANCS_AMS_client_id,event->param.connect.conidx);
#endif
	
#if (BLE_PROFILE_ENABLE_HID)
        hid_service_enable(event->param.connect.conidx);
#endif

//        user_bt_env.connect_times = 3;
//        bt_connect(&event->param.bond.peer_id_addr.addr.addr);
        break;

    case GAP_EVT_NAME_REQ:	
        {
			
        gap_name_req_rsp(event->param.name_req.conidx,
                            event->param.name_req.token,
                                sizeof(local_device_name),
                                (uint8_t *)local_device_name);
        }
        break;

    case GAP_EVT_APPEARANCE_REQ:
        {
            gap_appearance_req_rsp(event->param.appearance_req.conidx,
                                    event->param.appearance_req.token,
                                    GAP_APPEARE_HID_KEYBOARD);
        }
        break;

    case GAP_EVT_LINK_PARAM_REQ:
        {
            struct gap_link_param_update_rsp rsp;
            rsp.accept = true;
            rsp.conidx = event->param.link_param_update_req.conidx;
            rsp.ce_len_max = 2;
            rsp.ce_len_min = 2;
            gap_param_update_rsp(&rsp);
        }
        break;

    case GAP_EVT_LINK_PARAM_UPDATE:
        {
            printf("conn param update,conidx:%d, con_int:%d, latency:%d, timeout%d\r\n", event->param.link_param_update.conidx, 
                                                                                            event->param.link_param_update.con_interval,
                                                                                            event->param.link_param_update.con_latency,
                                                                                            event->param.link_param_update.sup_to);
            //gap_security_req(event->param.connect.conidx);
        }
        break;

    case GAP_EVT_LINK_RSSI:
        printf("gap_callback: conidx: %d, GAP_EVT_LINK_RSSI: %d\r\n", event->param.gap_link_rssi.conidx, event->param.gap_link_rssi.link_rssi);
        break;

    case GAP_EVT_PHY_IND:
        printf("gap_callback: conidx: %d, GAP_EVT_PHY_IND: %d\r\n", event->param.gap_phy_ind.conidx, event->param.gap_phy_ind.tx_phy);
        break;

    case GAP_EVT_PHY_REJECT:
        printf("gap_callback: conidx: %d, GAP_EVT_PHY_REJECT, status: %d\r\n", event->param.gap_phy_update_reject.conidx, event->param.gap_phy_update_reject.status);
        break;

    case GAP_EVT_LINK_VER:
        printf("gap_callback: conidx: %d, GAP_EVT_LINK_VER\r\n", event->param.gap_link_ver.conidx);
        break;

    case GAP_EVT_LINK_FEATURE:
        printf("gap_callback: conidx: %d, GAP_EVT_LINK_FEATURE:%d\r\n", event->param.gap_link_feature.conidx, event->param.gap_link_feature.features[0]);
        break;
    default:
        break;
    }
    
    return 0;
}

static uint16_t gatt_callback(struct gatt_msg *p_msg)
{
    switch(p_msg->msg_evt) {
    case GATT_OP_PEER_SVC_DISC_END:
        switch(p_msg->param.gatt_op_cmp.operation) {
        case GATT_OP_PEER_SVC_DISC_END:
//            LOG_INFO(NULL, "GATT_OP_PEER_SVC_DISC_END: ");
//            uint16_t length = p_msg->param.gatt_op_cmp.arg_len;
//            uint8_t *buffer = p_msg->param.gatt_op_cmp.arg;
//            for(uint8_t i=0; i<length; i++) {
//                LOG_INFO(NULL, "%02x ", buffer[i]);
//            }
//            LOG_INFO(NULL, "\r\n");
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    return 0;
}


void app_ble_init(void)
{
    struct gap_security_param smp_param;
    
    gap_set_cb_func(gap_callback);
    
    smp_param.mitm = false;
    smp_param.secure_connection = true;
    smp_param.bond = true;
    smp_param.rsp_mode = ENABLE_AUTO_RSP;
    smp_param.oob_used = GAP_OOB_AUTH_DATA_NOT_PRESENT;
    smp_param.io_cap = GAP_IO_CAP_NO_INPUT_NO_OUTPUT;
    smp_param.rsp_mode = ENABLE_AUTO_RSP;
    gap_security_param_init(&smp_param);
    
    sp_gatt_add_service();
	
#if BLE_PROFILE_ENABLE_ANCS
    ANCS_AMS_gatt_add_client();
#endif
    
#if BLE_PROFILE_ENABLE_HID
    hid_gatt_add_service();
#endif

//    gaf_cfg();
}

void app_ble_start_advertising(uint8_t adv_chn)
{
    if (adv_chn < APP_BLE_ADV_CHN_MAX) {
        if (adv[adv_chn] == NULL) {
            struct gap_adv_param adv_param;

            adv_param.adv_mode = GAP_ADV_MODE_UNDIRECT;
            adv_param.disc_mode = GAP_ADV_DISC_MODE_GEN_DISC;
            adv_param.filt_policy = GAP_ADV_FILTER_SCAN_ANY_CON_ANY;
            adv_param.adv_chnl_map = GAP_ADV_CHAN_ALL;
            adv_param.phy_mode = GAPM_PHY_TYPE_LE_1M;
            adv_param.own_addr_type = GAP_ADDR_TYPE_STATIC;    // GAPM_GEN_RSLV_ADDR  GAPM_STATIC_ADDR
            
            adv_param.adv_intv_min = 1600;
            adv_param.adv_intv_max = 1600;            
            adv[adv_chn] = gap_adv_create();
            gap_adv_set_param(adv[adv_chn], &adv_param);

//            adv_data[17] = ble_static_addr[5];
//            adv_data[18] = ble_static_addr[4];
//            adv_data[19] = ble_static_addr[3];
//            adv_data[20] = ble_static_addr[2];
//            adv_data[21] = ble_static_addr[1];
//            adv_data[22] = ble_static_addr[0];

            gap_adv_set_adv_data(adv[adv_chn], adv_data, sizeof(adv_data));
            gap_adv_set_scan_rsp(adv[adv_chn], adv_scan_rsp_data, sizeof(adv_scan_rsp_data));
        }
        gap_adv_start(adv[adv_chn], 0, 0);
    }
}

void app_ble_stop_advertising(uint8_t adv_chn)
{
    if ((adv_chn < APP_BLE_ADV_CHN_MAX)
        && (adv[adv_chn] != NULL)) {
        gap_adv_stop(adv[adv_chn]);
    }
}

void app_ble_scan_start(void)
{
    struct gap_scan_param param;

    param.scan_type = GAP_SCAN_TYPE_GEN_DISC;
    param.own_addr_type = GAP_ADDR_TYPE_STATIC;
    param.phy_mode = GAPM_PHY_TYPE_LE_1M;
    
    param.dup_filt_pol = true;
    param.scan_intv = 32;
    param.scan_window = 20;
    param.duration = 0;

    gap_scan_start(&param);
}

void app_ble_scan_stop(void)
{
    gap_scan_stop();
}

void app_ble_conn_start(struct gap_ble_addr *addr)
{
    struct gap_conn_param conn_param;
    conn_param.peer_addr.addr_type = addr->addr_type;
    conn_param.peer_addr.addr.addr[0] = addr->addr.addr[0];
    conn_param.peer_addr.addr.addr[1] = addr->addr.addr[1];
    conn_param.peer_addr.addr.addr[2] = addr->addr.addr[2];
    conn_param.peer_addr.addr.addr[3] = addr->addr.addr[3];
    conn_param.peer_addr.addr.addr[4] = addr->addr.addr[4];
    conn_param.peer_addr.addr.addr[5] = addr->addr.addr[5];
    conn_param.own_addr_type = GAP_ADDR_TYPE_STATIC;
    conn_param.phy_mode = GAPM_PHY_TYPE_LE_1M;
    conn_param.scan_intv = 32;
    conn_param.scan_window = 32;
    conn_param.conn_intv_min = 24;
    conn_param.conn_intv_max = 24;
    conn_param.supervision_to = 500;
    conn_param.slave_latency = 0;
    conn_param.ce_len_max = 2;
    conn_param.ce_len_min = 2;
    gap_conn_start(&conn_param);
}

void app_ble_conn_stop(void)
{
    gap_conn_stop();
}
