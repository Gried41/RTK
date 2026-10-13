#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fr30xx.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"
#include "task.h"

#include "co_util.h"

#include "bt_types.h"
#include "me_api.h"
#include "hfg_api.h"
#include "gatt_api.h"

#include "mcu_gnss.h"
#include "mcu_imu.h"
#include "app_at.h"
#include "app_task.h"
#include "app_ble.h"
#include "app_bt.h"
#include "app_audio.h"
#include "app_config.h"
#include "btdm_mem.h"
#include "fdb_app.h"
#include "user_bt.h"
#include "dsp_mem.h"
#include "heap.h"
#include "host.h"
#if BTDM_STACK_ENABLE_PAN
#include "pan_api.h"
#include "ethernetif.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/api.h"
#endif

#define AT_RECV_MAX_LEN             32
#define UART_TO_SPP_QUEUE_LENGTH    512
#define UART_TO_SPP_PACKET_MAX      128
#define UART_TO_SPP_IDLE_TIMEOUT    pdMS_TO_TICKS(3)
#define AT_CMD_MAX_LEN              128

//统计各类型报文数量
uint32_t uart2_gga_count = 0;
uint32_t uart2_rmc_count = 0;
uint32_t uart2_zda_count = 0;
uint32_t uart3_gga_count = 0;
uint32_t uart3_rmc_count = 0;
uint32_t uart3_zda_count = 0;


//串口打印互斥锁
static SemaphoreHandle_t uart_printf_mutex = NULL;

/*  队列，保存输入的AT指令  */
static QueueHandle_t at_cmd_queue;
/*  uart3是单字节接收，用于接收每个字节的变量   */
static uint8_t uart3_rx_byte;
/*  uart3接收指令缓冲区 */
static char uart3_buf[AT_CMD_MAX_LEN];
static uint16_t uart3_buf_len = 0;

/*  队列，保存IMU 向 RX 返回的数据*/
static QueueHandle_t imu_to_rx_queue;
/*   缓冲区 保存IMU 向 RX 返回的数据*/
static uint8_t uart4_rx_byte;
static uint8_t uart4_rx_buf[128];
static uint8_t uart4_rx_buf_len;


/* 队列，保存RX 到 GNSS uart3 的数据*/
static QueueHandle_t rx_to_gnss_uart3_queue;
/* 队列，保存RX 到 GNSS uart2 的数据*/
static QueueHandle_t rx_to_gnss_uart2_queue;
/* 队列，保存GNSS 到 MCU UART2 的数据*/
static QueueHandle_t gnss_uart3_to_rx_queue;
static uint8_t uart2_rx_byte;
static uint8_t uart2_rx_buf[128];
static uint8_t uart2_rx_buf_len;
static volatile uint32_t gnss_com3_dropped_count;

/* 队列，保存GNSS 到 MCU UART1 的数据*/
static QueueHandle_t gnss_uart2_to_rx_queue;
static uint8_t uart1_rx_byte;
static uint8_t uart1_rx_buf[128];
static uint8_t uart1_rx_buf_len;
static volatile uint32_t gnss_com2_dropped_count;


static TaskHandle_t my_task_handle;
static TaskHandle_t rx_to_gnss_uart3_task_handle;
static TaskHandle_t rx_to_gnss_uart2_task_handle;
static TaskHandle_t imu_to_rx_task_handle;
static TaskHandle_t gnss_uart3_to_rx_task_handle;
static TaskHandle_t gnss_uart2_to_rx_task_handle;

volatile uint8_t g_uart_to_spp_flag = 0;

static uint8_t app_at_recv_char;
static QueueHandle_t uart_to_spp_queue;
static TaskHandle_t uart_to_spp_task_handle;        //静态全局变量 放置于.bss段，初始化自动为0也就是NULL；

static uint8_t at_recv_buffer[AT_RECV_MAX_LEN];
static uint8_t at_recv_index = 0;
static uint8_t at_recv_state = 0;
extern uint8_t enable_audio_transfer;

#if BTDM_STACK_ENABLE_A2DP_SNK || BTDM_STACK_ENABLE_A2DP_SRC
void bt_source_get_sbc_param(uint8_t dev_index, struct sbc_encoder_param *sbc_param)
{
    sbc_param->i_bitpool = sbcinfo[dev_index].info.bitPool;
    sbc_param->i_blocks = sbcinfo[dev_index].info.numBlocks;
    sbc_param->i_num_chan = sbcinfo[dev_index].info.numChannels;
    if(sbcinfo[dev_index].info.sampleFreq == 2){
        sbc_param->i_samp_freq = 44100;
    }
    else if(sbcinfo[dev_index].info.sampleFreq == 3){
        sbc_param->i_samp_freq = 48000;
    }
    else{
        sbc_param->i_samp_freq = 0;
    }
    sbc_param->i_snr = (sbcinfo[dev_index].info.allocMethod == 2) ? 1 : 0;
    sbc_param->i_subbands = sbcinfo[dev_index].info.numSubBands;
}
#endif
void btdm_host_send_vendor_cmd(uint8_t type, uint8_t length, void *data);

void btdm_host_vendor_cmd_cmp_evt(uint8_t status, uint8_t len, uint8_t const *param)
{
    printf("status: 0x%02x.\r\n", status);
    for (uint32_t i=0; i<len; i++) {
        printf("%02x ", param[i]);
    }
    printf("\r\n");
}
static void user_hci_callback(const BtEvent *event)
{
    printf("event type = %d,%d\r\n",event->eType,event->p.meToken->p.general.in.parmLen);
    if(event->eType == BTEVENT_COMMAND_COMPLETE){
        if(event->p.meToken->p.general.in.parmLen){
            btdm_free(event->p.meToken->p.general.in.parms);
        }
        btdm_free(event->p.meToken);
    }
}
static void app_at_recv_cmd_A(uint8_t sub_cmd, uint8_t *data)
{
    BD_ADDR addr;
    switch(sub_cmd)
    {
        case 'A':
        {
            uint8_t addr = ascii_strn2val((const char *)&data[0], 16, 2);
            btdm_host_send_vendor_cmd(0x00, 1, &addr);
        }
            printf("OK\r\n");
            break;
        case 'B':
        {
            uint8_t buffer[2];
            buffer[0] = ascii_strn2val((const char *)&data[0], 16, 2);
            buffer[1] = ascii_strn2val((const char *)&data[3], 16, 2);
            btdm_host_send_vendor_cmd(0x01, 2, (void *)&buffer[0]);
        }
            printf("OK\r\n");
            break;
        case 'C':
        {
            uint32_t addr = ascii_strn2val((const char *)&data[0], 16, 8);
            btdm_host_send_vendor_cmd(0x02, 4, (void *)&addr);
        }
            printf("OK\r\n");
            break;
        case 'D':
        {
            uint32_t buffer[2];
            buffer[0] = ascii_strn2val((const char *)&data[0], 16, 8);
            buffer[1] = ascii_strn2val((const char *)&data[9], 16, 8);
            btdm_host_send_vendor_cmd(0x03, 8, (void *)&buffer[0]);
        }
            printf("OK\r\n");
            break;
        case 'G':
            printf("hello world!\r\n");
            break;
        case 'H':
            printf("VAL: 0x%08x.\r\n", *(volatile uint32_t *)ascii_strn2val((const char *)&data[0], 16, 8));
            break;
        case 'I':
            *(volatile uint32_t *)ascii_strn2val((const char *)&data[0], 16, 8) = ascii_strn2val((const char *)&data[9], 16, 8);
            printf("OK\r\n");
            break;
        case 'J':
            printf("OOL VAL: 0x%02x.\r\n", ool_read(ascii_strn2val((const char *)&data[0], 16, 2)));
            break;
        case 'K':
            ool_write(ascii_strn2val((const char *)&data[0], 16, 2), ascii_strn2val((const char *)&data[3], 16, 2));
            printf("OK\r\n");
            break;
        case 'L':
            printf("VAL: 0x%02x.\r\n", *(volatile uint8_t *)(ascii_strn2val((const char *)&data[0], 16, 8)));
            break;
        case 'M':
            *(volatile uint8_t *)(ascii_strn2val((const char *)&data[0], 16, 8)) = ascii_strn2val((const char *)&data[9], 16, 2);
            printf("OK\r\n");
            break;
//        case 'P':
//            co_printf("VAL: 0x%02x.\r\n", *(uint8_t *)(MODEM_BASE + ascii_strn2val((const char *)&data[0], 16, 2)));
//            break;
//        case 'Q':
//            *(uint8_t *)(MODEM_BASE + ascii_strn2val((const char *)&data[0], 16, 2)) = ascii_strn2val((const char *)&data[3], 16, 2);
//            co_printf("OK\r\n");
//            break;
//        case 'S':
//            co_printf("VAL: 0x%02x.\r\n", frspim_rd(FR_SPI_RF_COB_CHAN, ascii_strn2val((const char *)&data[0], 16, 2), 1));
//            break;
//        case 'T':
//            frspim_wr(FR_SPI_RF_COB_CHAN, ascii_strn2val((const char *)&data[0], 16, 2), 1, ascii_strn2val((const char *)&data[3], 16, 2));
//            co_printf("OK\r\n");
//            break;
        case 'N':
            DDB_EnumRecord();
            break;
        case 'O':
        {
            addr.A[0] = ascii_strn2val((const char*)&data[0],16,2);
            addr.A[1] = ascii_strn2val((const char*)&data[2],16,2);
            addr.A[2] = ascii_strn2val((const char*)&data[4],16,2);
            addr.A[3] = ascii_strn2val((const char*)&data[6],16,2);
            addr.A[4] = ascii_strn2val((const char*)&data[8],16,2);
            addr.A[5] = ascii_strn2val((const char*)&data[10],16,2);
            DDB_DeleteRecord(&addr);        
        }
            break;
        case 'Q':
        {
            ///reset local bt addr，only valid in non-broadcast and non-connected state
            uint8_t buffer[7];
            buffer[0] = 0;
            buffer[1] = ascii_strn2val((const char*)&data[0],16,2);
            buffer[2] = ascii_strn2val((const char*)&data[2],16,2);
            buffer[3] = ascii_strn2val((const char*)&data[4],16,2);
            buffer[4] = ascii_strn2val((const char*)&data[6],16,2);
            buffer[5] = ascii_strn2val((const char*)&data[8],16,2);
            buffer[6] = ascii_strn2val((const char*)&data[10],16,2);
            btdm_host_send_vendor_cmd(0x20, 7, (void *)&buffer[0]);
        }
            break;
        case 'U':
        {
            uint32_t *ptr = (uint32_t *)(ascii_strn2val((const char *)&data[0], 16, 8) & (~3));
            uint8_t count = ascii_strn2val((const char *)&data[9], 16, 2);
            uint32_t *start = (uint32_t *)((uint32_t)ptr & (~0x0f));
            for(uint8_t i=0; i<count;) {
                if(((uint32_t)start & 0x0c) == 0) {
                    printf("0x%08x: ", (uint32_t)start);
                }
                if(start < ptr) {
                    printf("        ");
                }
                else {
                    i++;
                    printf("%08x", *start);
                }
                if(((uint32_t)start & 0x0c) == 0x0c) {
                    printf("\r\n");
                }
                else {
                    printf(" ");
                }
                start++;
            }
        }
            break;
        case 'V':
            flash_erase(QSPI0, ascii_strn2val((const char *)&data[0], 16, 8), ascii_strn2val((const char *)&data[9], 16, 8));
            break;
        case 'W':
        {
            uint32_t curr_free, min_free;
            dsp_mem_get_usage(&curr_free, &min_free);
            printf("DSP MEM: %d, %d\r\n", curr_free, min_free);
        }
            break;
        case 'X':
        {
            void system_reset(void);
            system_reset();
        }
            break;
        case 'Y':
            heap_dump_used_mem(ascii_strn2val((const char *)&data[0], 16, 2));
            break;
        case 'Z':
            printf("MEM usage\r\n \
                    \tHEAP_TYPE_SRAM_BLOCK: %d, %d\r\n \
                    \tHEAP_TYPE_DRAM_BLOCK: %d, %d\r\n \
                    \tHEAP_TYPE_BTDM_BLOCK: %d, %d\r\n \
                    \tTOTAL USAGE: %d\r\n", \
                            heap_get_mem_usage(HEAP_TYPE_SRAM_BLOCK), heap_get_max_mem_usage_single(HEAP_TYPE_SRAM_BLOCK), \
                            heap_get_mem_usage(HEAP_TYPE_DRAM_BLOCK), heap_get_max_mem_usage_single(HEAP_TYPE_DRAM_BLOCK), \
                            heap_get_mem_usage(HEAP_TYPE_BTDM_BLOCK), heap_get_max_mem_usage_single(HEAP_TYPE_BTDM_BLOCK), \
                            heap_get_max_mem_usage());
            break;
        default:
            break;
    }
}
#if BTDM_STACK_ENABLE_BT
static void app_at_recv_cmd_B(uint8_t sub_cmd, uint8_t *data)
{
    struct gap_ble_addr peer_addr;
    BD_ADDR addr;
    HfgResponse *rsp;
    BtStatus status = BT_STATUS_FAILED;
    MeCommandToken *token;
//    uint16_t page_timeout = 0x400;
    
    switch(sub_cmd) {
        case 'A':
        {
            extern bool host_get_bt_last_device(BD_ADDR *addr);
            bool ret = host_get_bt_last_device(&addr);
            if(ret == true){
                user_bt_env.connect_times = 3;
                status = bt_connect(&addr, ENABLE_PROFILE_HFG|ENABLE_PROFILE_A2DP_SOURCE);
                printf("status = %d,%x,%x\r\n",status,addr.A[0],addr.A[1]);                    
            }
            else{
                printf("no saved device\r\n");
            }        
        }
            break;
        case 'B':
        {
            extern bool host_get_bt_last_device(BD_ADDR *addr);
            bool ret = host_get_bt_last_device(&addr);
            if(ret == true){
                user_bt_env.connect_times = 3;
                status = bt_connect(&addr, ENABLE_PROFILE_A2DP_SINK);
                printf("status = %d,%x,%x\r\n",status,addr.A[0],addr.A[1]);                    
            }
            else{
                printf("no saved device\r\n");
            }        
        }
            break;
        case 'D':
        {
            token = btdm_malloc(sizeof(MeCommandToken));
            token->p.general.in.hciCommand = 0x1803;//HCC_ENABLE_DUT;
            token->p.general.in.parmLen = 0;
            token->p.general.in.event = 0x0e;       //HCE_COMMAND_COMPLETE
            token->callback = user_hci_callback;    //token is freed at this callback
            ME_SendHciCommandSync(token);
        }
            break;
        case 'E':
        {
            ///set poll interval to 5ms
            if(user_bt_env.dev[0].remDev != 0){
            token = btdm_malloc(sizeof(MeCommandToken));
            token->p.general.in.hciCommand = 0x0807;//HCC_QOS_SETUP;
            token->p.general.in.parmLen = 20;
                uint8_t *qos_setup_param = btdm_malloc(token->p.general.in.parmLen);
            
            uint16_t handle = ME_GetHciConnectionHandle(user_bt_env.dev[0].remDev);
            qos_setup_param[0] = handle&0xff;
            qos_setup_param[1] = (handle>>8)&0xff;
            qos_setup_param[2] = 0;
            qos_setup_param[3] = 2; // qos guaranteed
            
            qos_setup_param[4] = 0x00;
            qos_setup_param[5] = 0x00;
            qos_setup_param[6] = 0x00;
            qos_setup_param[7] = 0x00;
            
            ///Peak_bandwidth
            qos_setup_param[8] = 0x00;
            qos_setup_param[9] = 0x00;
            qos_setup_param[10] = 0x00;
            qos_setup_param[11] = 0x00;
            
            ///latency, default:0xffffffff, don't care
            qos_setup_param[12] = 0x88; //0x1388, 5000us
            qos_setup_param[13] = 0x13;
            qos_setup_param[14] = 0x00;
            qos_setup_param[15] = 0x00;
            
            ///delay variation
            qos_setup_param[16] = 0x00;
            qos_setup_param[17] = 0x00;
            qos_setup_param[18] = 0x00;
            qos_setup_param[19] = 0x00;
                token->p.general.in.parms = qos_setup_param;
                token->p.general.in.event = 0x0f;       //HCE_COMMAND_STATUS
            token->callback = user_hci_callback;    //token is freed at this callback
                status = ME_SendHciCommandAsync(token);                
            }
            printf("status = %d\r\n",status);
        }
            break;
        case 'H':
            ME_Inquiry(BT_IAC_GIAC, 5, 0);
            break;
        case 'I':
            ME_CancelInquiry();
            break;
        
        case 'L':
            flashdb_del(FDB_KEY_BT_LINKKEY);
            break;
        case 'M':
        {
            uint8_t *page_timeout;
            token = btdm_malloc(sizeof(MeCommandToken));
            token->p.general.in.hciCommand = 0x0c18;//HCC_WRITE_PAGE_TIMEOUT;
            token->p.general.in.parmLen = 2;
            page_timeout = btdm_malloc(token->p.general.in.parmLen);
            page_timeout[0] = 0x00;
            page_timeout[1] = 0x04;
            token->p.general.in.parms = page_timeout;
            token->p.general.in.event = 0x0e;       //HCE_COMMAND_COMPLETE
            token->callback = user_hci_callback;    //token is freed at this callback
            status = ME_SendHciCommandAsync(token);
            printf("status = %d\r\n",status);
        }
            break;
        case 'N':
        {
            addr.A[0] = ascii_strn2val((const char*)&data[0],16,2);
            addr.A[1] = ascii_strn2val((const char*)&data[2],16,2);
            addr.A[2] = ascii_strn2val((const char*)&data[4],16,2);
            addr.A[3] = ascii_strn2val((const char*)&data[6],16,2);
            addr.A[4] = ascii_strn2val((const char*)&data[8],16,2);
            addr.A[5] = ascii_strn2val((const char*)&data[10],16,2);
            user_bt_env.connect_times = 3;
            status = bt_connect(&addr,ENABLE_PROFILE_HF|ENABLE_PROFILE_A2DP_SINK);
            printf("status = %d\r\n",status);
        }
            break;

        case 'O':
        {
            addr.A[0] = ascii_strn2val((const char*)&data[0],16,2);
            addr.A[1] = ascii_strn2val((const char*)&data[2],16,2);
            addr.A[2] = ascii_strn2val((const char*)&data[4],16,2);
            addr.A[3] = ascii_strn2val((const char*)&data[6],16,2);
            addr.A[4] = ascii_strn2val((const char*)&data[8],16,2);
            addr.A[5] = ascii_strn2val((const char*)&data[10],16,2);
            user_bt_env.connect_times = 3;
            status = bt_connect(&addr,ENABLE_PROFILE_HFG|ENABLE_PROFILE_A2DP_SOURCE);
            printf("status = %d\r\n",status);
        }
            break;
        case 'P':
            addr.A[0] = ascii_strn2val((const char*)&data[0],16,2);
            addr.A[1] = ascii_strn2val((const char*)&data[2],16,2);
            addr.A[2] = ascii_strn2val((const char*)&data[4],16,2);
            addr.A[3] = ascii_strn2val((const char*)&data[6],16,2);
            addr.A[4] = ascii_strn2val((const char*)&data[8],16,2);
            addr.A[5] = ascii_strn2val((const char*)&data[10],16,2);
            bt_disconnect(&addr,false);
            break;
        
        case 'Q':
            addr.A[0] = ascii_strn2val((const char*)&data[0],16,2);
            addr.A[1] = ascii_strn2val((const char*)&data[2],16,2);
            addr.A[2] = ascii_strn2val((const char*)&data[4],16,2);
            addr.A[3] = ascii_strn2val((const char*)&data[6],16,2);
            addr.A[4] = ascii_strn2val((const char*)&data[8],16,2);
            addr.A[5] = ascii_strn2val((const char*)&data[10],16,2);
            bt_disconnect(&addr,true);
            break;
        
        case 'R':
        {
            extern bool host_get_bt_last_device(BD_ADDR *addr);
            bool ret = host_get_bt_last_device(&addr);
            if(ret == true){
                user_bt_env.connect_times = 3;
                status = bt_connect(&addr, ENABLE_PROFILE_HF|ENABLE_PROFILE_A2DP_SINK);
                printf("status = %d,%x,%x\r\n",status,addr.A[0],addr.A[1]);                    
            }
            else{
                printf("no saved device\r\n");
            }
        }
            break;
        case 'S':
        {
            BtAccessModeInfo access_mode_nc = {
                .inqInterval = 0x800,
                .inqWindow = 0x12,
                .pageInterval = 0x800,
                .pageWindow = 0x12,
            };
            status = bt_enter_pairing(BAM_GENERAL_ACCESSIBLE, &access_mode_nc);
            printf("status = %d\r\n",status);
        }
            break;
        case 'T':
        {
            status = bt_exit_pairing();
            printf("status = %d\r\n",status);

        }
            break;
        case 'U':
        {    
            #define EIR_DATA_SIZE 100
            uint8_t eir_data[EIR_DATA_SIZE];
            uint8_t index = 0;
            memset(&eir_data[0],0,EIR_DATA_SIZE);
            eir_data[index++] = sizeof("FR30xx_SDK_APP") + 1;
            eir_data[index++] = 0x09;
            memcpy(&(eir_data[index]),"FR30xx_SDK_APP",sizeof("FR30xx_SDK_APP"));
            ME_SetExtInquiryRsp(0x01,eir_data,sizeof("FR30xx_SDK_APP")+1);
            ME_SetLocalDeviceName("FR30xx_SDK_APP", 15);            
        }
            break;
        case 'V':
        {
            status = bt_disconnect(&user_bt_env.dev[user_bt_env.last_active_index].remote_bd,true);
            printf("status = %d\r\n",status);
        }
            break;
        case 'W':
            ME_SwitchRole(user_bt_env.dev[0].remDev);
            break;
        case 'X':
        {
            addr.A[0] = ascii_strn2val((const char*)&data[0],16,2);
            addr.A[1] = ascii_strn2val((const char*)&data[2],16,2);
            addr.A[2] = ascii_strn2val((const char*)&data[4],16,2);
            addr.A[3] = ascii_strn2val((const char*)&data[6],16,2);
            addr.A[4] = ascii_strn2val((const char*)&data[8],16,2);
            addr.A[5] = ascii_strn2val((const char*)&data[10],16,2);
            user_bt_env.connect_times = 3;
            status = bt_connect(&addr,ascii_strn2val((const char*)&data[13],16,2));
            printf("status = %d\r\n",status);        
        }
            break;
        default:
            break;
    }
    printf("OK\r\n");
}

#if BTDM_STACK_ENABLE_HF
///HF
static void app_at_recv_cmd_C(uint8_t sub_cmd, uint8_t *data)
{
    BtStatus status;
    BD_ADDR addr;
    switch(sub_cmd) {
        case 'A':
            status = bt_answer_call(user_bt_env.last_active_index);
            printf("status = %d\r\n",status);
            break;
        case 'B':
            status = bt_hang_up(user_bt_env.last_active_index);
            printf("status = %d\r\n",status);
            break;
        case 'C':
            status = bt_redial(user_bt_env.last_active_index);
            printf("status = %d\r\n",status);
            break;
        case 'D':
        {
            uint8_t number[15];
            uint8_t len = 0;
            uint8_t i;
            for(i = 0; i < 15; i++)
            {
                if(data[i] != '\r'){
                    number[i] = data[i];
                }
                else{
                    break;
                }
            }
            len = i;
            //printf("dial: %s,len=%d\r\n",number,len);
            status = bt_dial_number(user_bt_env.last_active_index,number,len);
            printf("status = %d\r\n",status);
        }
        break;
        case 'E':
            status = bt_list_current_calls(user_bt_env.last_active_index);
            printf("status = %d\r\n",status);
            break;
        case 'F':
            status = bt_transfer_sco(user_bt_env.last_active_index);
            printf("status = %d\r\n",status);
            break;
        case 'G':
        {
            uint8_t dtmf = data[0];
            status = bt_send_dtmf(user_bt_env.last_active_index,dtmf);
            printf("status = %d\r\n",status);        
        }
            break;       
        case 'H':
        {
            //vol---[0x00,0x0f]
            uint8_t vol = ascii_strn2val((const char*)&data[0],16,2);
            status = bt_report_spk_volume(user_bt_env.last_active_index,vol);
            printf("status = %d\r\n",status);        
        }
            break;         
        case 'I':
        {
            uint8_t voltage_str[] = "AT+IPHONEACCEV=1,1,6";

            status = bt_send_hf_cmd(user_bt_env.last_active_index,voltage_str);
            printf("status = %d\r\n",status);        
        }
            break;    
        case 'J':
        {
            uint8_t enabled = ascii_strn2val((const char*)&data[0],16,2);
            status = bt_enable_voice_recognition(user_bt_env.last_active_index, enabled);
            printf("status = %d\r\n",status);
        }
            break;
        case 'K':
            printf("voice recog enabled : %d\r\n",bt_is_voice_rec_active(user_bt_env.last_active_index));
            break;
        case 'L':
        {
            uint8_t clk_str[] = "AT+CCLK?";
            status = bt_send_hf_cmd(user_bt_env.last_active_index,clk_str);
            printf("status = %d\r\n",status);        
        }
            break;
        default:
            break;
    }
    printf("OK\r\n");
}
#endif
#if BTDM_STACK_ENABLE_AG
///HFG
static void app_at_recv_cmd_D(uint8_t sub_cmd, uint8_t *data)
{
    BD_ADDR addr;
    HfgResponse *rsp;
    BtStatus status;
    switch(sub_cmd) {
        case 'A':
        {
            BtStatus       status;
            addr.A[0] = ascii_strn2val((const char*)&data[0],16,2);
            addr.A[1] = ascii_strn2val((const char*)&data[2],16,2);
            addr.A[2] = ascii_strn2val((const char*)&data[4],16,2);
            addr.A[3] = ascii_strn2val((const char*)&data[6],16,2);
            addr.A[4] = ascii_strn2val((const char*)&data[8],16,2);
            addr.A[5] = ascii_strn2val((const char*)&data[10],16,2);
            memcpy(&user_bt_env.dev[0].remote_bd,&addr,6);
            status = HFG_CreateServiceLink(&hfg_channel[0], &addr);
            
            if (status == BT_STATUS_PENDING) {
                printf("Opening Channel...\r\n");
            } else {
                printf("Could not open channel, status: %d\r\n", status);
            }
        }
            break;
        case 'B':
        {
            status = BT_STATUS_NO_RESOURCES;
            rsp = (HfgResponse *)btdm_malloc(sizeof(HfgResponse));
            if(rsp != NULL){
                status = HFG_CreateCodecConnection(&hfg_channel[0], ascii_strn2val((const char*)&data[0],16,2), rsp);
            }
            if(status != BT_STATUS_PENDING){
                btdm_free((void *)rsp);
                if(status == BT_STATUS_RESTRICTED){
                    CMGR_SetAudioVoiceSettings(0x60);
                    HFG_CreateAudioLink(&hfg_channel[0],HFG_AR_LOCAL_USER_ACTION);                
                }
            printf("status = %d\r\n",status);
                }
        }
            break;
        case 'C':
            HFG_SetMasterRole(&hfg_channel[0],TRUE);
            break;
        case 'D':
            HFG_DisconnectAudioLink(&hfg_channel[0], HFG_AR_LOCAL_USER_ACTION);
            break;
        #if BTDM_STACK_ENABLE_HF && BTDM_STACK_ENABLE_A2DP_SNK && BTDM_STACK_ENABLE_A2DP_SRC
        case 'N':
        {
            ///for test, conn0 for phone, conn1 for earpods
            enable_audio_transfer = 1;
            if(((user_bt_env.dev[1].conFlags & LINK_STATUS_MEDIA_PLAYING) == 0)
                && (user_bt_env.dev[0].conFlags & LINK_STATUS_MEDIA_PLAYING)){
                A2DP_StartStream(user_bt_env.dev[1].pstream);
                app_audio_a2dp_sink_stop();
            }
            else if(((user_bt_env.dev[1].conFlags & LINK_STATUS_SCO_CONNECTED) == 0)
                &&(user_bt_env.dev[0].conFlags & LINK_STATUS_SCO_CONNECTED)){
                    app_audio_sco_stop();
                    void bt_free_sco_buffer_list(void);
                    bt_free_sco_buffer_list();
                    HFG_CreateAudioLink(user_bt_env.dev[1].hfg_chan, HFG_AR_LOCAL_USER_ACTION);
            }
        }
            break;
        case 'O':
        {
            enable_audio_transfer = 0;
            if((user_bt_env.dev[0].conFlags & LINK_STATUS_MEDIA_PLAYING) 
                && (user_bt_env.dev[1].conFlags & LINK_STATUS_MEDIA_PLAYING)){
                A2DP_SuspendStream(user_bt_env.dev[1].pstream);
                app_audio_a2dp_sink_start(AUDIO_TYPE_SBC, 44100);
            }
            else if((user_bt_env.dev[1].conFlags & LINK_STATUS_SCO_CONNECTED)
                    &&(user_bt_env.dev[0].conFlags & LINK_STATUS_SCO_CONNECTED)){
                extern void encoded_sco_frame_cb(void *arg, uint8_t *data, uint16_t length);
                void bt_free_sco_buffer_list(void);
                bt_free_sco_buffer_list();
                HFG_DisconnectAudioLink(user_bt_env.dev[1].hfg_chan, HFG_AR_LOCAL_USER_ACTION);
                app_audio_sco_start(AUDIO_TYPE_PCM, encoded_sco_frame_cb, user_bt_env.dev[0].hf_chan);
            }
        }
            break;
        #endif
        default:
            break;
    }
    printf("OK\r\n");    
}
#endif
            
#if BTDM_STACK_ENABLE_A2DP_SNK || BTDM_STACK_ENABLE_A2DP_SRC || BTDM_STACK_ENABLE_AVRCP
///A2DP & AVRCP
static void app_at_recv_cmd_E(uint8_t sub_cmd, uint8_t *data)
{
    BD_ADDR addr;
    BtStatus status;
    switch(sub_cmd) {
        #if BTDM_STACK_ENABLE_AVRCP
        case 'A':
            status = AVRCP_SetPanelKey(user_bt_env.dev[user_bt_env.last_active_index].rcp_chan, AVRCP_POP_PLAY, TRUE);
            printf("status = %d\r\n",status);
            break;
        case 'B':
            status = AVRCP_SetPanelKey(user_bt_env.dev[user_bt_env.last_active_index].rcp_chan, AVRCP_POP_PAUSE, TRUE);
            printf("status = %d\r\n",status);
            break;
        case 'C':
            status = AVRCP_SetPanelKey(user_bt_env.dev[user_bt_env.last_active_index].rcp_chan, AVRCP_POP_FORWARD, TRUE);
            printf("status = %d\r\n",status);
            break;
        case 'D':
        {
            status = AVRCP_SetPanelKey(user_bt_env.dev[user_bt_env.last_active_index].rcp_chan, AVRCP_POP_BACKWARD, TRUE);
            printf("status = %d\r\n",status);
        }
            break;
        case 'E':
        {
            uint8_t vol = ascii_strn2val((const char*)&data[0],16,2);
            status = bt_set_media_volume(user_bt_env.last_active_index,vol);
            printf("status = %d\r\n",status);
        }
            break;
        case 'F':
        {
            status = bt_get_media_info(user_bt_env.last_active_index,0x41);
            printf("status = %d\r\n",status);
        }
            break;
        case 'G':
        {
            status = bt_get_playstatus(user_bt_env.last_active_index);
            printf("status = %d\r\n",status);
        }
            break;    
        case 'H':
        {
            AvrcpAdvancedPdu *avrcp_cmd;
            avrcp_cmd = (AvrcpAdvancedPdu *)btdm_malloc(sizeof(AvrcpAdvancedPdu));
            avrcp_cmd->parms = (uint8_t *)btdm_malloc(64);
            AVRCP_CtSetAbsoluteVolume(user_bt_env.dev[user_bt_env.last_active_index].rcp_chan,avrcp_cmd,0x3f);
        }break;
        #endif
        #if BTDM_STACK_ENABLE_A2DP_SRC
        case 'K':
        {
            struct sbc_encoder_param sbc_param;
            extern void encoded_sbc_frame_cb(void *arg, uint8_t *data, uint16_t length);

            bt_source_get_sbc_param(user_bt_env.last_active_index,&sbc_param);
            A2DP_StartStream(user_bt_env.dev[user_bt_env.last_active_index].pstream);
            app_audio_a2dp_source_start(AUDIO_TYPE_MP3, &sbc_param, encoded_sbc_frame_cb, &user_bt_env.last_active_index);        
        }
            break;
        case 'L':
            A2DP_SuspendStream(user_bt_env.dev[user_bt_env.last_active_index].pstream);
            app_audio_a2dp_source_stop();
            break;
        case 'M':
        {
            struct sbc_encoder_param sbc_param;
            extern void encoded_sbc_frame_cb(void *arg, uint8_t *data, uint16_t length);
            bt_source_get_sbc_param(0,&sbc_param);
//                bt_source_get_sbc_param(&sbc_param.alloc_method, &sbc_param.sample_rate, &sbc_param.bitpool);
            A2DP_StartStream(user_bt_env.dev[0].pstream);
            A2DP_StartStream(user_bt_env.dev[1].pstream);
            app_audio_a2dp_source_start(AUDIO_TYPE_MP3, &sbc_param, encoded_sbc_frame_cb, &user_bt_env.last_active_index);        
        }
            break;
        case 'R':
            A2DP_SuspendStream(user_bt_env.dev[0].pstream);
            A2DP_SuspendStream(user_bt_env.dev[1].pstream);
            app_audio_a2dp_source_stop();
            break;
        #endif
        case 'N':
            //play tone
            app_audio_tone_play(AUDIO_TYPE_SBC,NULL,0);
            break;
        case 'O':
            {
                extern const uint8_t mp3_sample[];
                extern uint32_t mp3_sample_get_size(void);
                printf("MP3 sample size: %d\r\n", mp3_sample_get_size());
                app_audio_tone_play(AUDIO_TYPE_MP3, mp3_sample, mp3_sample_get_size());
            }
            break;
        default:
            break;
    }
    printf("OK\r\n");
}
#endif
#if BTDM_STACK_ENABLE_PBAP   
///PBAP
static void app_at_recv_cmd_F(uint8_t sub_cmd, uint8_t *data)
{
    BD_ADDR addr;
    BtStatus status;
    switch(sub_cmd) {    
        case 'C':
//            addr.A[0] = ascii_strn2val((const char*)&data[0],16,2);
//            addr.A[1] = ascii_strn2val((const char*)&data[2],16,2);
//            addr.A[2] = ascii_strn2val((const char*)&data[4],16,2);
//            addr.A[3] = ascii_strn2val((const char*)&data[6],16,2);
//            addr.A[4] = ascii_strn2val((const char*)&data[8],16,2);
//            addr.A[5] = ascii_strn2val((const char*)&data[10],16,2);
            if(user_bt_get_state(user_bt_env.last_active_index) >= BT_STATE_CONNECTED){
                printf("pbap client %d\r\n",pbap_client[user_bt_env.last_active_index].cApp.connState);
                status = PBAP_ClientConnect(&pbap_client[user_bt_env.last_active_index],&user_bt_env.dev[user_bt_env.last_active_index].remote_bd);
                if(status == BT_STATUS_PENDING){
                    user_bt_env.dev[user_bt_env.last_active_index].pbap_client = &pbap_client[user_bt_env.last_active_index];
                }
            }
            printf("status = %d\r\n",status);
            break;
        case 'D':
            status = PBAP_ClientDisconnect(&pbap_client[user_bt_env.last_active_index]);
            printf("status = %d\r\n",status);
            break;
        case 'M':
        {
            uint8_t pbName[64];
            PbapPullPbParms     parms;
            
            memcpy(pbName,PB_LOCAL_MCH_NAME,sizeof(PB_LOCAL_MCH_NAME));
            parms.pbName = pbName;
            memset(parms.filter.byte,0,PBAP_FILTER_SIZE);
            parms.filter.byte[0] = 0x87;
            parms.listStartOffset = 0;
            parms.maxListCount = 4; //0---search missed call and total pb size
            parms.format= VCARD_FORMAT_30;
            status = PBAP_PullPhonebook(&pbap_client[0], &parms);       
            printf("status = %d\r\n",status);        
        }
            break;
        case 'O':
        {
            PbapPullVcardListingParms   parms;
            uint8_t search_val[12]; //= "13262651013";
            memcpy(search_val,&data[0],11);
            uint8_t folder[] = "telecom/pb";
            parms.folderName = folder;
            parms.order = VCARD_SORT_ORDER_ALPHA;
            parms.listStartOffset = 0x00;
            parms.maxListCount = 20;
            parms.searchAttribute = VCARD_SEARCH_ATTRIB_NUMBER;
            parms.searchValue = (char *)search_val;
            status = PBAP_PullVcardListing(&pbap_client[0], &parms);       
            printf("status = %d\r\n",status);        
        }
            break;
        case 'P':
        {
            uint8_t pbName[64];
            PbapPullPbParms     parms;
            
            memcpy(pbName,PB_LOCAL_STORE_NAME,sizeof(PB_LOCAL_STORE_NAME));
            parms.pbName = pbName;
            memset(parms.filter.byte,0,PBAP_FILTER_SIZE);
            parms.filter.byte[0] = 0x87;
            parms.listStartOffset = 0;
            parms.maxListCount = 20; //0---search missed call and total pb size
            parms.format= VCARD_FORMAT_30;
            status = PBAP_PullPhonebook(&pbap_client[0], &parms);       
            printf("status = %d\r\n",status);
        }
            break;
        default:
            break;
    }
}
#endif  

///BLE
static void app_at_recv_cmd_G(uint8_t sub_cmd, uint8_t *data)
{
    struct gap_ble_addr peer_addr;

    switch(sub_cmd) {
        case 'C':
            app_ble_scan_start();
            break;
        case 'D':
            app_ble_scan_stop();
            break;
        case 'E':
            // AT#GE0123456789ab_01
            peer_addr.addr.addr[5] = ascii_strn2val((const char *)&data[0], 16, 2);
            peer_addr.addr.addr[4] = ascii_strn2val((const char *)&data[2], 16, 2);
            peer_addr.addr.addr[3] = ascii_strn2val((const char *)&data[4], 16, 2);
            peer_addr.addr.addr[2] = ascii_strn2val((const char *)&data[6], 16, 2);
            peer_addr.addr.addr[1] = ascii_strn2val((const char *)&data[8], 16, 2);
            peer_addr.addr.addr[0] = ascii_strn2val((const char *)&data[10], 16, 2);
            peer_addr.addr_type = ascii_strn2val((const char *)&data[13], 16, 2);
            app_ble_conn_start(&peer_addr);
            break;
        case 'F':
            app_ble_conn_stop();
            break;
        default:
            break;
    }
    printf("OK\r\n");
}

#if BTDM_STACK_ENABLE_MAP
///MAP
static void app_at_recv_cmd_M(uint8_t sub_cmd, uint8_t *data)
{
    BD_ADDR addr;
    switch(sub_cmd) {
        case 'E':
        {
            ObStatus status;
            ObexTpAddr     target = {0};
            extern bool host_get_bt_last_device(BD_ADDR *addr);
            host_get_bt_last_device(&addr);
            memcpy(&target.proto.bt.addr,&addr,6); 
            target.type = OBEX_TP_BLUETOOTH;
            status = MAP_ClientConnect(&map_client[0],&target);
            printf("status = 0x%x\r\n",status);
        }
            break;
        case 'F':
        {
            ObStatus            status;
            MapGetMessagesListParms     parms;
                /* Assign the phonebook to retrieve */

            parms.mapName = (const uint8_t*)"inbox";
            parms.maxListCount = 0;  
            parms.listStartOffset = 0; 
            //parms.paramMask = 0x108f;
            /* filter unread */
            parms.filterReadStatus = 0;
            status = MAP_ClientGetMessagesListing(&map_client[0],&parms);
            printf("status = %d\r\n",status);
        }
            break;       
        case 'G':
        {
            MapSetFolderParms params;
            ObStatus            status;
            params.folderName = (const uint8_t*)"telecom";
            params.reset = 0;
            params.flags = MAP_SET_FLAG_GO_DOWN;
            status = MAP_ClientSetFolder(&map_client[0],&params);
            printf("status = %d\r\n",status);
        }
            break;    
        case 'H':
        {
            MapSetFolderParms params;
            ObStatus            status;
            params.folderName = (const uint8_t*)"msg";
            params.reset = 0;
            params.flags = MAP_SET_FLAG_GO_DOWN;
            status = MAP_ClientSetFolder(&map_client[0],&params);
            printf("status = %d\r\n",status);
        }
            break;    
        case 'J':
        {
            ObStatus            status;
            status = MAP_ClientDisconnect(&map_client[0]);
            printf("status = %d\r\n",status);
        }
            break;    
        default:
            break;
    }
    printf("OK\r\n");
}
#endif

#if BTDM_STACK_ENABLE_HID
///HID
static void app_at_recv_cmd_H(uint8_t sub_cmd, uint8_t *data)
{
    BD_ADDR addr;
    BtStatus status;
    switch(sub_cmd) {
        case 'A'://左移
        {
            HidInterrupt *Interrupt = (HidInterrupt*)btdm_malloc(sizeof(HidInterrupt));
            
            Interrupt->reportType = HID_REPORT_INPUT;
            Interrupt->dataLen = 4;
            Interrupt->data = (uint8_t *)btdm_malloc(Interrupt->dataLen);

            Interrupt->data[0] = 0x01;
            Interrupt->data[1] = 0xF1;
            Interrupt->data[2] = 0x00;
            Interrupt->data[3] = 0x00;
            
            HID_SendInterrupt(user_bt_env.dev[user_bt_env.last_active_index].hid_chan, Interrupt);
        }
            break;
        case 'B'://右移
        {
            HidInterrupt *Interrupt = (HidInterrupt*)btdm_malloc(sizeof(HidInterrupt));
            
            Interrupt->reportType = HID_REPORT_INPUT;
            Interrupt->dataLen = 4;
            Interrupt->data = (uint8_t *)btdm_malloc(Interrupt->dataLen);

            Interrupt->data[0] = 0x01;
            Interrupt->data[1] = 0x0F;
            Interrupt->data[2] = 0x00;
            Interrupt->data[3] = 0x00;
            
            HID_SendInterrupt(user_bt_env.dev[user_bt_env.last_active_index].hid_chan, Interrupt);
        }
            break;
        
        case 'C': //下移
        {  
            HidInterrupt *Interrupt = (HidInterrupt*)btdm_malloc(sizeof(HidInterrupt));
            
            Interrupt->reportType = HID_REPORT_INPUT;
            Interrupt->dataLen = 4;
            Interrupt->data = (uint8_t *)btdm_malloc(Interrupt->dataLen);

            Interrupt->data[0] = 0x02;
            Interrupt->data[1] = 0x00;
            Interrupt->data[2] = 0x0F;
            Interrupt->data[3] = 0x00;
            
            HID_SendInterrupt(user_bt_env.dev[user_bt_env.last_active_index].hid_chan, Interrupt);
        }
            break;
        
        case 'D': //上移
        {
            HidInterrupt *Interrupt = (HidInterrupt*)btdm_malloc(sizeof(HidInterrupt));
            
            Interrupt->reportType = HID_REPORT_INPUT;
            Interrupt->dataLen = 4;
            Interrupt->data = (uint8_t *)btdm_malloc(Interrupt->dataLen);

            Interrupt->data[0] = 0x02;
            Interrupt->data[1] = 0x00;
            Interrupt->data[2] = 0xF1;
            Interrupt->data[3] = 0x00;
            
            HID_SendInterrupt(user_bt_env.dev[user_bt_env.last_active_index].hid_chan, Interrupt);
        }
            break;
        case 'E':
        {
            addr.A[0] = ascii_strn2val((const char*)&data[0],16,2);
            addr.A[1] = ascii_strn2val((const char*)&data[2],16,2);
            addr.A[2] = ascii_strn2val((const char*)&data[4],16,2);
            addr.A[3] = ascii_strn2val((const char*)&data[6],16,2);
            addr.A[4] = ascii_strn2val((const char*)&data[8],16,2);
            addr.A[5] = ascii_strn2val((const char*)&data[10],16,2);
            memcpy(&user_bt_env.dev[0].remote_bd,&addr,6);
            status = HID_OpenConnection(&hid_channel[0], &addr);
            printf("status = %d\r\n",status);            
        }
            break;
        case 'F':
        {
            status = HID_CloseConnection(user_bt_env.dev[user_bt_env.last_active_index].hid_chan);
            printf("status = %d\r\n",status); 
        }
            break;
        case 'G':
        {
            addr.A[0] = ascii_strn2val((const char*)&data[0],16,2);
            addr.A[1] = ascii_strn2val((const char*)&data[2],16,2);
            addr.A[2] = ascii_strn2val((const char*)&data[4],16,2);
            addr.A[3] = ascii_strn2val((const char*)&data[6],16,2);
            addr.A[4] = ascii_strn2val((const char*)&data[8],16,2);
            addr.A[5] = ascii_strn2val((const char*)&data[10],16,2);
            user_bt_env.connect_times = 3;
            status = bt_connect(&addr,ENABLE_PROFILE_HID);
            printf("status = %d\r\n",status);
        }
            break;
    }
    printf("OK\r\n");
}
#endif

#if BTDM_STACK_ENABLE_SPP
///SPP
static void app_at_recv_cmd_S(uint8_t sub_cmd, uint8_t *data)
{
    BD_ADDR addr;
    BtStatus status;
    switch(sub_cmd) {    
        case 'A':
            addr.A[0] = ascii_strn2val((const char*)&data[0],16,2);
            addr.A[1] = ascii_strn2val((const char*)&data[2],16,2);
            addr.A[2] = ascii_strn2val((const char*)&data[4],16,2);
            addr.A[3] = ascii_strn2val((const char*)&data[6],16,2);
            addr.A[4] = ascii_strn2val((const char*)&data[8],16,2);
            addr.A[5] = ascii_strn2val((const char*)&data[10],16,2);
            user_bt_env.connect_times = 3;
            status = bt_connect(&addr,ENABLE_PROFILE_SPP);
            printf("status = %d\r\n",status);
            break;
        case 'B':
            addr.A[0] = ascii_strn2val((const char*)&data[0],16,2);
            addr.A[1] = ascii_strn2val((const char*)&data[2],16,2);
            addr.A[2] = ascii_strn2val((const char*)&data[4],16,2);
            addr.A[3] = ascii_strn2val((const char*)&data[6],16,2);
            addr.A[4] = ascii_strn2val((const char*)&data[8],16,2);
            addr.A[5] = ascii_strn2val((const char*)&data[10],16,2);
            memcpy(&user_bt_env.dev[0].remote_bd,&addr,6);

            status = spp_connect(&spp_dev[0],&addr);
            printf("status = %d\r\n",status);
            break;
        case 'D':
            status = spp_disconnect(user_bt_env.dev[user_bt_env.last_active_index].spp_dev);
            printf("status = %d\r\n",status);
            break;
        case 'S':
        {
            uint8_t test_data[] = {'1','2','3','4'};
            status = spp_send(user_bt_env.dev[user_bt_env.last_active_index].spp_dev,test_data,sizeof(test_data));
            printf("status = %d\r\n",status);        
        }
            break;
        default:
            break;
    }
    printf("OK\r\n");
}
#endif
#if BTDM_STACK_ENABLE_PAN
/* 定义端口 */
#define TCP_REMOTE_PORT    80 /* 远端端口 */
#define TCP_LOCAL_PORT     8880 /* 本地端口 */

/******************************************************************************
 * 描述  : 数据接收回调函数
 * 参数  : -
 * 返回  : -
******************************************************************************/
static err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb,
                             struct pbuf *p, err_t err)
{
    uint32_t i;
    
    /* 数据回传 */
    //tcp_write(tpcb, p->payload, p->len, 1);
    
    if (p != NULL)
    {
        struct pbuf *ptmp = p;
        
#if 1
        printf(".");
#else
        /* 打印接收到的数据 */
        printf("get msg from %d:%d:%d:%d port:%d:\r\n",
            *((uint8_t *)&tpcb->remote_ip.addr),
            *((uint8_t *)&tpcb->remote_ip.addr + 1),
            *((uint8_t *)&tpcb->remote_ip.addr + 2),
            *((uint8_t *)&tpcb->remote_ip.addr + 3),
            tpcb->remote_port);
        
        while(ptmp != NULL)
        {
            for (i = 0; i < p->len; i++)
            {
                printf("%c", *((char *)p->payload + i));
            }
            
            ptmp = p->next;
        }
        
        printf("\r\n");
#endif

        tcp_recved(tpcb, p->tot_len);
        
        /* 释放缓冲区数据 */
        pbuf_free(p);
    }
    else if (err == ERR_OK)
    {
        printf("tcp client closed\r\n");
        
        tcp_recved(tpcb, p->tot_len);
        
        return tcp_close(tpcb);
    }

    return ERR_OK;
}

/******************************************************************************
 * 描述  : 连接服务器回调函数
 * 参数  : -
 * 返回  : -
******************************************************************************/
static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    printf("tcp client connected\r\n");
    
    tcp_write(tpcb, "tcp client connected", strlen("tcp client connected"), 0);

    /* 注册接收回调函数 */
    tcp_recv(tpcb, tcp_client_recv);

    return ERR_OK;
}

/******************************************************************************
 * 描述  : 创建tcp客户端
 * 参数  : -
 * 返回  : -
******************************************************************************/
void tcp_client_init(uint8_t ip_1, uint8_t ip_2, uint8_t ip_3, uint8_t ip_4, uint16_t port)
{
    struct tcp_pcb *tpcb;
    ip_addr_t serverIp;
    
    printf("tcp_client_init: %d.%d.%d.%d:%d\r\n", ip_1, ip_2, ip_3, ip_4, port);

     /* 服务器IP */
    IP4_ADDR(&serverIp, ip_1, ip_2, ip_3, ip_4);

    /* 创建tcp控制块 */
    tpcb = tcp_new();
    
    if (tpcb != NULL)
    {
        err_t err;
        
        /* 绑定本地端号和IP地址 */
        err = tcp_bind(tpcb, IP_ADDR_ANY, TCP_LOCAL_PORT);

        if (err == ERR_OK)
        {
            /* 连接服务器 */
            tcp_connect(tpcb, &serverIp, port, tcp_client_connected);
        }
        else
        {
            memp_free(MEMP_TCP_PCB, tpcb);
            
            printf("can not bind pcb\r\n");
        }
    }
}

/******************************** END OF FILE ********************************/

static void udp_receive_callback(void *arg, struct udp_pcb *upcb,
                                    struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    uint32_t i;

    /* 打印接收到的数据 */
    printf("get msg from %d:%d:%d:%d port:%d:\r\n",
        *((uint8_t *)&addr->addr), *((uint8_t *)&addr->addr + 1),
        *((uint8_t *)&addr->addr + 2), *((uint8_t *)&addr->addr + 3), port);
    
    if (p != NULL)
    {
        struct pbuf *ptmp = p;
        
        while(ptmp != NULL)
        {
            for (i = 0; i < p->len; i++)
            {
                printf("%c", *((char *)p->payload + i));
            }
            
            ptmp = p->next;
        }
        
        printf("\r\n");
    }
    
    /* 释放缓冲区数据 */
    pbuf_free(p);
}

static void udp_client_send(struct udp_pcb *upcb, char *pData)
{
    struct pbuf *p;
    
    /* 分配缓冲区空间 */
    p = pbuf_alloc(PBUF_TRANSPORT, strlen(pData), PBUF_POOL);
    
    if (p != NULL)
    {
        /* 填充缓冲区数据 */
        pbuf_take(p, pData, strlen(pData));

        /* 发送udp数据 */
        udp_send(upcb, p);

        /* 释放缓冲区空间 */
        pbuf_free(p);
    }
}

static void client(void *thread_param)
{
    ip4_addr_t dns_ip;
    
    char *host_name[] = {"www.baidu.com", "www.csdn.net", "www.hp.com"};
    static uint8_t index = 0;
    
    netconn_gethostbyname(host_name[index], &dns_ip);
    printf("%s: %d.%d.%d.%d.\r\n", host_name[index], dns_ip.addr&0xff,      \
                                                        (dns_ip.addr>>8)&0xff,   \
                                                        (dns_ip.addr>>16)&0xff,   \
                                                        (dns_ip.addr>>24)&0xff);
    index++;
    if (index >= sizeof(host_name)/sizeof(host_name[0])) {
        index = 0;
    }
    
    vTaskDelete(NULL);
}

void client_init(void)
{
    sys_thread_new("client", client, NULL, 512, 4);
}

static void app_at_recv_cmd_P(uint8_t sub_cmd, uint8_t *data)
{
    static PanUser pan;
    static struct udp_pcb *pudp;

    switch(sub_cmd) {
        case 'A':
            {
                extern BtRemoteDevice *last_rem_dev;
				int16_t err_code;
				
                err_code =PAN_Open(last_rem_dev, &pan, benp_lwip_recv_cb, 0x1116);   // 0x1116: NAP
				printf("PAN Open:%d\r\n",err_code);
            }
            break;
        case 'B':
            // AT#PBxxx_xxxxx
            {
                pudp = udp_new();
                if (pudp) {
                    err_t err;
                    ip_addr_t serverIP;
                    
                    IP4_ADDR(&serverIP, 192, 168, 31, ascii_strn2val((const char*)&data[0],10,3));
                    pudp->local_port = ascii_strn2val((const char*)&data[4],10,5);

                    err= udp_connect(pudp, &serverIP, 8080);
                    if (err == ERR_OK) {
                        udp_recv(pudp, udp_receive_callback, NULL);
                    }
                    else {
                        printf("connect UDP failed: %d.\r\n", err);
                    }
                }
                else {
                    printf("create UDP failed.\r\n");
                }
            }
            break;
        case 'C':
            // AT#PB
            {
                udp_client_send(pudp, "udp client connected");
            }
            break;
        case 'D':
            // AT#PDxxx_xxx_xxx_xxx_xxxxx
            {
                uint8_t ip_1, ip_2, ip_3, ip_4;
                uint16_t port;
                ip_1 = ascii_strn2val((const char*)&data[0],10,3);
                ip_2 = ascii_strn2val((const char*)&data[4],10,3);
                ip_3 = ascii_strn2val((const char*)&data[8],10,3);
                ip_4 = ascii_strn2val((const char*)&data[12],10,3);
                port = ascii_strn2val((const char*)&data[16],10,5);
                tcp_client_init(ip_1, ip_2, ip_3, ip_4, port);
            }
            break;
        case 'E':
            // TEST DNS
            {
                void client_init(void);
                client_init();
            }
            break;
    }
}
#endif
#endif //#if BTDM_STACK_ENABLE_BT
void app_at_cmd_recv_handler(uint8_t *data, uint16_t length)
{
    switch(data[0])
    {
        ///rd/wr reg + test
        case 'A':
            app_at_recv_cmd_A(data[1], &data[2]);
            break;
#if BTDM_STACK_ENABLE_BT
        ///me cmd
        case 'B':
            app_at_recv_cmd_B(data[1], &data[2]);
            break;
#if BTDM_STACK_ENABLE_HF           
        ///hf 
        case 'C':
            app_at_recv_cmd_C(data[1], &data[2]);
            break;
#endif
#if BTDM_STACK_ENABLE_AG
        ///hfg
        case 'D':
            app_at_recv_cmd_D(data[1], &data[2]);
            break;
#endif
#if BTDM_STACK_ENABLE_A2DP_SNK || BTDM_STACK_ENABLE_A2DP_SRC || BTDM_STACK_ENABLE_AVRCP
        ///a2dp&avrcp
        case 'E':
            app_at_recv_cmd_E(data[1], &data[2]);
            break;
#endif
#if BTDM_STACK_ENABLE_PBAP
        ///pbap
        case 'F':
            app_at_recv_cmd_F(data[1], &data[2]);
            break;
#endif
        ///BLE
        case 'G':
            app_at_recv_cmd_G(data[1], &data[2]);
            break;
#if BTDM_STACK_ENABLE_MAP
        ///map
        case 'M':
            app_at_recv_cmd_M(data[1], &data[2]);
            break;
#endif
#if BTDM_STACK_ENABLE_HID
        ///hid
        case 'H':
            app_at_recv_cmd_H(data[1], &data[2]);
            break;
#endif
#if BTDM_STACK_ENABLE_SPP
        ///spp
        case 'S':
            app_at_recv_cmd_S(data[1], &data[2]);
            break;
#endif
#if BTDM_STACK_ENABLE_PAN
        case 'P':
            app_at_recv_cmd_P(data[1],&data[2]);
            break;
#endif
#endif
        default:
            break;
    }
}

static void app_at_recv_c(uint8_t c)
{
    switch(at_recv_state)
    {
        case 0:
            if(c == 'A')
            {
                at_recv_state++;
            }
            break;
        case 1:
            if(c == 'T')
                at_recv_state++;
            else
                at_recv_state = 0;
            break;
        case 2:
            if(c == '#')
                at_recv_state++;
            else
                at_recv_state = 0;
            break;
        case 3:
            at_recv_buffer[at_recv_index++] = c;
            if((c == '\n')
               ||(at_recv_index >= AT_RECV_MAX_LEN))
            {
                struct app_task_event *event;
                event = app_task_event_alloc(APP_TASK_EVENT_AT_CMD, at_recv_index, false);
                if(event) {
                    memcpy(event->param, at_recv_buffer, at_recv_index);
                    app_task_event_post(event, false);
                }
                at_recv_state = 0;
                at_recv_index = 0;
            }
            break;
    }
}

void app_at_rx_done(struct __UART_HandleTypeDef *handle)
{
    app_at_recv_c(app_at_recv_char);
    if (handle) 
    {
        uart_receive_IT(handle, &app_at_recv_char, 1);
    }
}

static void uart_to_spp_task(void *arg)
{
    uint8_t packet[UART_TO_SPP_PACKET_MAX];
    uint8_t received_char;
    uint16_t packet_length;
    SppDev *spp_dev;

    (void)arg;
    while (1)
    {
        //重新进入循环先清空packet数组缓冲区，防止上次数据包残留。
        memset(packet, 0, sizeof(packet));

        /*  
            从uart_to_spp_queue队列中接收数据到packet数组缓冲区，portMAX_DELAY代表不停的阻塞等待，没有数据就一直等，让出cpu，直到队列中出现数据。
            等到uart_to_spp_queue队列中有数据时，xQueueReceive函数返回pdPASS，自动取消阻塞状态，然后进入while循环，继续从队列中接收数据，
            直到接收到的数据长度达到UART_TO_SPP_PACKET_MAX或者在UART_TO_SPP_IDLE_TIMEOUT时间内没有接收到数据为止。
        */
        if (xQueueReceive(uart_to_spp_queue, &packet[0], portMAX_DELAY) == pdPASS)
        {
            packet_length = 1;
            /*
                其中的UART_TO_SPP_IDLE_TIMEOUT特别关键，因为uart_to_spp_queue是由回调函数那边单字节处理的，这里的逻辑是当uart_to_spp_queue队列有数据
                则会触发这边的任务函数取消阻塞状态，进入循环读取数据到数组中，有了这个关键字，我们就可以在规定时间内继续阻塞等待队列中的数据，继续读取，
                当超过事件队列没新数据进入时，判断本次数据包已经接收完毕，可以通过spp_send发送。
            */
            while (packet_length < UART_TO_SPP_PACKET_MAX && xQueueReceive(uart_to_spp_queue, &received_char,UART_TO_SPP_IDLE_TIMEOUT) == pdPASS)
            {
                packet[packet_length++] = received_char;
            }


            spp_dev = user_bt_env.dev[user_bt_env.last_active_index].spp_dev;
            if (spp_dev != NULL)
            {
                spp_send(spp_dev, packet, packet_length);

                /*
                //通知my_task任务函数可以开始运行了，用于测试任务通知机制。
                if(my_task_handle != NULL)
                {
                    xTaskNotifyGive(my_task_handle);
                }
                */
            }
        }
    }
}
    /*
        //等待队列数据进入阻塞状态
        if(xQueueReceive(uart_to_spp_queue,packet,portMAX_DELAY) == pdPASS)
        {
            //if语句已经提前读取一次数据，将数组下班后移一位
            packet_length = 1;
            //当队列中仍存在数据时，不断调用xQueueReceive函数将数据读取到数组中，并且限时阻塞等待，当一段时间都没数据后表示读取完毕，准备发送
            //特殊情况1：当队列中的数据长度大于我的数组长度时，停止接收
            while(xQueueReceive(uart_to_spp_queue,&received_char,UART_TO_SPP_IDLE_TIMEOUT) == pdPASS && packet_length < UART_TO_SPP_PACKET_MAX)
            {
                //接收数据到数组中
                packet[packet_length++] = received_char;

                //通知my_task任务函数可以开始运行了，用于测试任务通知机制。
                xTaskNotifyGive(my_task_handle);
            }

        }
    */
/*
@fn:用于IMU26向RX返回的数据
*/
void imu_to_rx_done(struct __UART_HandleTypeDef *handle)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    char ch = (char)uart4_rx_byte;
    /*
        imu向MCU发回的验证信息前两个字符分别是 \r\n,放置第一次空数据进入
    */
    if(ch == '\r' || ch == '\n')
    {
        //结尾了为每条数据添加\0
        if(uart4_rx_buf_len > 0)
        {
            uart4_rx_buf[uart4_rx_buf_len] = '\0';
            if(imu_to_rx_queue != NULL)
            {
                xQueueSendFromISR(imu_to_rx_queue,uart4_rx_buf,&higher_priority_task_woken);
            }
        }
        //将数据交给队列
        uart4_rx_buf_len = 0;

    }
    else
    {
        if(uart4_rx_buf_len < 128-1)
        {
            uart4_rx_buf[uart4_rx_buf_len++] = ch;
        }
        else
        {
            uart4_rx_buf_len = 0;
        }
    }

    /* 重新启动下一字节的接收 */
    uart_receive_IT(handle, &uart4_rx_byte, 1);

    portYIELD_FROM_ISR(higher_priority_task_woken);
}
void imu_to_rx_task(void *arg)
{
    //初始化接收数据缓冲区
    char recv_data[128];
    while(1)
    {
        //阻塞等待队列出现新的数据
        if (xQueueReceive(imu_to_rx_queue,recv_data,portMAX_DELAY) == pdPASS)
        {
            if(recv_data)
            {
                printf("IMU Send:%s\r\n",recv_data);
            }
        }
    }
}
void imu_to_rx_task_init(void)
{
    //创建imu_rx_task的队列；
    imu_to_rx_queue = xQueueCreate(4, 128);
    if(imu_to_rx_queue == NULL)
    {
        printf("imu_to_rx_queue create fail\r\n");
    }
    xTaskCreate(imu_to_rx_task, "imu_to_rx_task", 256, NULL , 1, &imu_to_rx_task_handle);
}

/*
@fn:用于RX接收数据向IMU26发送
*/
void rx_to_imu_done(struct __UART_HandleTypeDef *handle)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    char ch = (char)uart3_rx_byte;
    if( ch == '\r' || ch== '\n')
    {
        if(uart3_buf_len > 0)
        {
            uart3_buf[uart3_buf_len] = '\0';

            if(at_cmd_queue != NULL)
            {
                xQueueSendFromISR(at_cmd_queue,uart3_buf,&higher_priority_task_woken);
            }
            uart3_buf_len = 0;
        }
    }
    else
    {
        if(uart3_buf_len < AT_CMD_MAX_LEN -1 )
        {
            uart3_buf[uart3_buf_len++] = ch;
        }
        else
        {
            //指令过长 丢弃当前指令
            uart3_buf_len = 0;
        }
    }
    //继续接收中断
    uart_receive_IT(handle, &uart3_rx_byte, 1);

    portYIELD_FROM_ISR(higher_priority_task_woken);
}
void my_task(void *arg)
{
    struct __UART_HandleTypeDef *huart_handle = (struct UART_HandleTypeDef  *)arg;
    char cmd[AT_CMD_MAX_LEN];
    while(1) 
    {
        //等待指令输入后调用 void Uart4_Send_AT_cmd(const char *cmd)
        if (xQueueReceive(at_cmd_queue,cmd,portMAX_DELAY) == pdPASS)
        {
            Uart4_Send_AT_cmd(cmd,huart_handle);
        }
    }
}
void My_task_init(UART_HandleTypeDef *handle)
{   
    at_cmd_queue = xQueueCreate(4, 128);
    if(at_cmd_queue == NULL)
    {
        printf("at_cmd_queue create fail\r\n");
        return;
    }
    xTaskCreate(my_task, "my_task", 256, (void *)handle, 1, &my_task_handle);
}  


    /*这个回调是在中断上下文产生的，所以它的运行环境实际上是中断*/
void rx_to_spp_done(struct __UART_HandleTypeDef *handle)
{
        /*   给调度器用的变量    */
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (handle == NULL)
        return;

    if (uart_to_spp_queue != NULL) 
    {
        uart_receive_IT(handle, &app_at_recv_char, 1);

        //xQueueSendFromISR 是专供中断中使用的入队API，用于将app_at_recv_char 这个数据加入到uart_to_spp_queue队列中
        if (xQueueSendFromISR(uart_to_spp_queue, &app_at_recv_char,&higher_priority_task_woken) == pdPASS) 
        {
            g_uart_to_spp_flag = 1;
        }

        /*
            因为这个回调函数维护的是uart_to_spp_queue队列，并且是单字节处理的，我们期望的是将所以字节数据整合完成后再执行任务函数，
            但任务函数先前是阻塞在uart_to_spp_queue队列中的，当队列中只要有一个数据后都触发任务取消阻塞，这是我们不希望的。
            xQueueSendFromISR(uart_to_spp_queue, &app_at_recv_char,&higher_priority_task_woken) == pdPASS;
            这个函数会判断队列增加数据后唤醒的任务优先级是不是比当前中断前运行的任务优先级更高
            如果更高，则将higher_priority_task_woken置为pdTRUE，表示需要切换任务，调度器会在中断退出后进行任务切换，执行更高优先级的任务。
            若不跟高，则higher_priority_task_woken置为pdFALSE，表示不需要切换任务，调度器会继续先前任务
        */
        portYIELD_FROM_ISR(higher_priority_task_woken);     //若这个中断唤醒了更高优先级的任务，则在中断退出后立即切换到更高优先级的任务运行
        /*
            但相同优先级情况下，还需要看FreeRtos的配置，若配置了configUSE_TIME_SLICING为1，则会进行时间片轮转调度，若配置为0，则不会进行时间片轮转调度。
        */
        return;
    }

    uart_receive_IT(handle, &app_at_recv_char, 1);
}

void uart_spp_task_init(void)
{
    uart_to_spp_queue = xQueueCreate(UART_TO_SPP_QUEUE_LENGTH, sizeof(uint8_t));

    xTaskCreate(uart_to_spp_task, "uart_spp", APP_TASK_STACK_SIZE, NULL,APP_TASK_PRIORITY, &uart_to_spp_task_handle);
}


void Uart_Recv_IT(struct __UART_HandleTypeDef *handle)
{
    /*
    if (uart_to_spp_queue == NULL) 
    {
        uart_to_spp_queue = xQueueCreate(UART_TO_SPP_QUEUE_LENGTH, sizeof(uint8_t));
    }
    if (uart_to_spp_queue != NULL && uart_to_spp_task_handle == NULL) 
    {
        xTaskCreate(uart_to_spp_task, "uart_spp", APP_TASK_STACK_SIZE, NULL,APP_TASK_PRIORITY, &uart_to_spp_task_handle);
    }
    */
    if (handle->UARTx == UART1)
    {
        uart_receive_IT(handle, &uart1_rx_byte, 1);
    }
    else if (handle->UARTx == UART2)
    {
        uart_receive_IT(handle, &uart2_rx_byte, 1);
    }
    else if (handle->UARTx == UART3)
    {
        uart_receive_IT(handle, &uart3_rx_byte, 1);
    }
    /* UART4：接收 IMU 返回的数据 */
    else if (handle->UARTx == UART4)
    {
        uart_receive_IT(handle, &uart4_rx_byte, 1);
    }
}


/*
@fn:中断回调函数，处理 RX 的信息并且 发送到队列， UART2 用队列信息去发送给 GNSS
*/
void rx_to_gnss_uart3_done(struct __UART_HandleTypeDef *handle)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    char ch = (char)uart3_rx_byte;

    if (ch == '\r' || ch == '\n')
    {
        if (uart3_buf_len > 0)
        {
            char *cmd_start = NULL;
            size_t cmd_len;

            /* 添加字符串结束符 */
            uart3_buf[uart3_buf_len] = '\0';

            /*
             * com2 VERSION
             * 去掉 com2 前缀，将 VERSION 放入 UART2 对应的队列
             */
            if (strncmp(uart3_buf, "com2 ", 5) == 0 ||strncmp(uart3_buf, "COM2 ", 5) == 0)
            {
                cmd_start = uart3_buf + 5;

                /* 跳过多余空格和制表符 */
                while (*cmd_start == ' ' || *cmd_start == '\t')
                {
                    cmd_start++;
                }

                if (*cmd_start != '\0' && rx_to_gnss_uart2_queue != NULL)
                {
                    /* 将纯指令移动到缓冲区开头 */
                    cmd_len = strlen(cmd_start);
                    memmove(uart3_buf, cmd_start, cmd_len + 1);

                    /* 放入已有的 UART2 队列 */
                    xQueueSendFromISR(rx_to_gnss_uart2_queue,uart3_buf,&higher_priority_task_woken);
                }
            }
            /*
             * com3 VERSION
             * 去掉 com3 前缀，将 VERSION 放入 UART3 对应的队列
             */
            else if (strncmp(uart3_buf, "com3 ", 5) == 0 ||strncmp(uart3_buf, "COM3 ", 5) == 0)
            {
                cmd_start = uart3_buf + 5;

                /* 跳过多余空格和制表符 */
                while (*cmd_start == ' ' || *cmd_start == '\t')
                {
                    cmd_start++;
                }

                if (*cmd_start != '\0' &&
                    rx_to_gnss_uart3_queue != NULL)
                {
                    /* 将纯指令移动到缓冲区开头 */
                    cmd_len = strlen(cmd_start);
                    memmove(uart3_buf, cmd_start, cmd_len + 1);

                    /* 放入已有的 UART3 队列 */
                    xQueueSendFromISR(rx_to_gnss_uart3_queue,uart3_buf,&higher_priority_task_woken
                    );
                }
            }

            /* 当前命令处理完毕 */
            uart3_buf_len = 0;
        }
    }
    else
    {
        /* 接收字符，预留一个字节存放 '\0' */
        if (uart3_buf_len < AT_CMD_MAX_LEN - 1)
        {
            uart3_buf[uart3_buf_len++] = ch;
        }
        else
        {
            /* 命令超长，丢弃当前数据 */
            uart3_buf_len = 0;
        }
    }

    /* 继续接收下一个字节 */
    uart_receive_IT(handle, &uart3_rx_byte, 1);

    portYIELD_FROM_ISR(higher_priority_task_woken);
}
void rx_to_gnss_uart3_task(void *arg)
{
    struct __UART_HandleTypeDef *huart_handle = (struct UART_HandleTypeDef  *)arg;
    char cmd[AT_CMD_MAX_LEN];
    while(1) 
    {
        //等待指令输入后调用 void Uart4_Send_AT_cmd(const char *cmd)
        if (xQueueReceive(rx_to_gnss_uart3_queue,cmd,portMAX_DELAY) == pdPASS)
        {
            Uart2_Send_AT_cmd(cmd,huart_handle);
        }
    }
}
void rx_to_gnss_uart3_task_init(UART_HandleTypeDef *handle)
{   
    rx_to_gnss_uart3_queue = xQueueCreate(4, 128);
    if(rx_to_gnss_uart3_queue == NULL)
    {
        printf("rx_to_gnss_uart3_queue create fail\r\n");
        return;
    }
    xTaskCreate(rx_to_gnss_uart3_task, "rx_to_gnss_uart3_task", 256, (void *)handle, 1, &rx_to_gnss_uart3_task_handle);
}  

/*
@fn:UART2 的接收回调 接收来自 GNSS UART3 的数据
*/
void gnss_uart3_to_rx_done(struct __UART_HandleTypeDef *handle)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    char ch = (char)uart2_rx_byte;
    if(ch == '\r' || ch == '\n')
    {
        //结尾了为每条数据添加\0
        if(uart2_rx_buf_len > 0)
        {
            uart2_rx_buf[uart2_rx_buf_len] = '\0';
            if(gnss_uart3_to_rx_queue != NULL)
            {
                if (xQueueSendFromISR(gnss_uart3_to_rx_queue, uart2_rx_buf, &higher_priority_task_woken) != pdPASS)
                {
                    gnss_com3_dropped_count++;
                }
            }
            else
            {
                gnss_com3_dropped_count++;
            }
        }
        //将数据交给队列
        uart2_rx_buf_len = 0;

    }
    else
    {
        if(uart2_rx_buf_len < 128-1)
        {
            uart2_rx_buf[uart2_rx_buf_len++] = ch;
        }
        else
        {
            uart2_rx_buf_len = 0;
        }
    }

    /* 重新启动下一字节的接收 */
    uart_receive_IT(handle, &uart2_rx_byte, 1);

    portYIELD_FROM_ISR(higher_priority_task_woken);
}
void gnss_uart3_to_rx_task(void *arg)
{
    //初始化接收数据缓冲区
    char recv_data[128];

    uint32_t checked_count = 0;
    uint32_t pass_count = 0;
    uint32_t fail_count = 0;
    uint32_t invalid_count  = 0;
    TickType_t start_tick = xTaskGetTickCount();
    TickType_t last_report_tick = start_tick;
    const TickType_t report_period = pdMS_TO_TICKS(60000);

    while(1)
    {
        //阻塞等待队列出现新的数据
        //if (xQueueReceive(gnss_uart3_to_rx_queue,recv_data,portMAX_DELAY) == pdPASS)
        //{
                //printf("GNSS Uart3 Send:%s\r\n",recv_data);
        //}
        if (xQueueReceive(gnss_uart3_to_rx_queue,recv_data,pdMS_TO_TICKS(1000)) == pdPASS)
        {
            if (strncmp(recv_data, "$GPGGA", 6) == 0 || strncmp(recv_data, "$GNGGA", 6) == 0)
            {
                uart3_gga_count++;
            }
            else if (strncmp(recv_data, "$GPRMC", 6) == 0 ||strncmp(recv_data, "$GNRMC", 6) == 0)
            {
                uart3_rmc_count++;
            }
            else if (strncmp(recv_data, "$GPZDA", 6) == 0 ||strncmp(recv_data, "$GNZDA", 6) == 0)
            {
                uart3_zda_count++;
            }
            int result = nmea_checksum_check(recv_data);

            if (result == 1)
            {
                checked_count++;
                pass_count++;
            }
            else if (result == 0)
            {
                checked_count++;
                fail_count++;
                printf("fail meg : %s\r\n",recv_data);
            }
            else
            {
                invalid_count++;
                printf("invalid meg : %s\r\n",recv_data);
            }
        }
        TickType_t now = xTaskGetTickCount();

        if ((TickType_t)(now - last_report_tick) >= report_period)
        {
            uint32_t fail_rate_x100 = 0;

            if (checked_count > 0)
            {
                fail_rate_x100 =(uint32_t)(((uint64_t)fail_count * 10000U) / checked_count);
            }
            uint32_t elapsed_seconds =(uint32_t)((now - start_tick) / configTICK_RATE_HZ);
            if (uart_printf_mutex != NULL && xSemaphoreTake(uart_printf_mutex, portMAX_DELAY) == pdTRUE)
            {
                printf(
                "\r\n===== GNSS COM3 CHECKSUM STATISTICS =====\r\n"
                "Elapsed       : %lu seconds\r\n"
                "Checked       : %lu\r\n"
                "Pass          : %lu\r\n"
                "Fail          : %lu\r\n"
                "Failure rate  : %lu.%02lu%%\r\n"
                "Invalid format: %lu\r\n"
                "Queue drops   : %lu\r\n"
                "GGA MSG COUNT : %lu\r\n"
                "RMC MSG COUNT : %lu\r\n"
                "ZDA MSG COUNT : %lu\r\n"
                "=========================================\r\n",
                (unsigned long)elapsed_seconds,
                (unsigned long)checked_count,
                (unsigned long)pass_count,
                (unsigned long)fail_count,
                (unsigned long)(fail_rate_x100 / 100),
                (unsigned long)(fail_rate_x100 % 100),
                (unsigned long)invalid_count,
                (unsigned long)gnss_com3_dropped_count,
                (unsigned long)uart2_gga_count,
                (unsigned long)uart2_rmc_count,
                (unsigned long)uart2_zda_count
                );
                    xSemaphoreGive(uart_printf_mutex);
           }
            last_report_tick = now;
        }
    }
}
void gnss_uart3_to_rx_task_init(void)
{
    //创建gnss_uart3_to_rx_task的队列；
    gnss_uart3_to_rx_queue = xQueueCreate(32, 128);
    if(gnss_uart3_to_rx_queue == NULL)
    {
        printf("gnss_uart3_to_rx_queue create fail\r\n");
    }
    xTaskCreate(gnss_uart3_to_rx_task, "gnss_uart3_to_rx_task", 256, NULL , 1, &gnss_uart3_to_rx_task_handle);
}



/*
@fn:中断回调函数，处理 RX 的信息并且 发送到队列， UART2 用队列信息去发送给 GNSS
*/
void rx_to_gnss_uart2_done(struct __UART_HandleTypeDef *handle)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    char ch = (char)uart3_rx_byte;
    if( ch == '\r' || ch== '\n')
    {
        if(uart3_buf_len > 0)
        {
            uart3_buf[uart3_buf_len] = '\0';

            if(rx_to_gnss_uart2_queue != NULL)
            {
                xQueueSendFromISR(rx_to_gnss_uart2_queue,uart3_buf,&higher_priority_task_woken);
            }
            uart3_buf_len = 0;
        }
    }
    else
    {
        if(uart3_buf_len < AT_CMD_MAX_LEN -1 )
        {
            uart3_buf[uart3_buf_len++] = ch;
        }
        else
        {
            //指令过长 丢弃当前指令
            uart3_buf_len = 0;
        }
    }
    //继续接收中断
    uart_receive_IT(handle, &uart3_rx_byte, 1);

    portYIELD_FROM_ISR(higher_priority_task_woken);
}
void rx_to_gnss_uart2_task(void *arg)
{
    struct __UART_HandleTypeDef *huart_handle = (struct UART_HandleTypeDef  *)arg;
    char cmd[AT_CMD_MAX_LEN];
    while(1) 
    {
        if (xQueueReceive(rx_to_gnss_uart2_queue,cmd,portMAX_DELAY) == pdPASS)
        {
            Uart1_Send_AT_cmd(cmd,huart_handle);
        }
    }
}
void rx_to_gnss_uart2_task_init(UART_HandleTypeDef *handle)
{   
    rx_to_gnss_uart2_queue = xQueueCreate(4, 128);
    if(rx_to_gnss_uart2_queue == NULL)
    {
        printf("rx_to_gnss_uart2_queue create fail\r\n");
        return;
    }
    xTaskCreate(rx_to_gnss_uart2_task, "rx_to_gnss_uart2_task", 256, (void *)handle, 1, &rx_to_gnss_uart2_task_handle);
}  


/*
@fn:UART1 的接收回调 接收来自 GNSS UART2 的数据
*/
void gnss_uart2_to_rx_done(struct __UART_HandleTypeDef *handle)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    char ch = (char)uart1_rx_byte;
    if(ch == '\r' || ch == '\n')
    {
        //结尾了为每条数据添加\0
        if(uart1_rx_buf_len > 0)
        {
            uart1_rx_buf[uart1_rx_buf_len] = '\0';
            if(gnss_uart2_to_rx_queue != NULL)
            {
                if (xQueueSendFromISR(gnss_uart2_to_rx_queue, uart1_rx_buf, &higher_priority_task_woken) != pdPASS)
                {
                    gnss_com2_dropped_count++;
                }
            }
            else
            {
                gnss_com2_dropped_count++;
            }
        }
        //将数据交给队列
        uart1_rx_buf_len = 0;

    }
    else
    {
        if(uart1_rx_buf_len < 128-1)
        {
            uart1_rx_buf[uart1_rx_buf_len++] = ch;
        }
        else
        {
            uart1_rx_buf_len = 0;
        }
    }

    /* 重新启动下一字节的接收 */
    uart_receive_IT(handle, &uart1_rx_byte, 1);

    portYIELD_FROM_ISR(higher_priority_task_woken);
}

void gnss_uart2_to_rx_task(void *arg)
{
    //初始化接收数据缓冲区
    char recv_data[128];

    uint32_t checked_count = 0;
    uint32_t pass_count = 0;
    uint32_t fail_count = 0;
    uint32_t invalid_count  = 0;
    TickType_t start_tick = xTaskGetTickCount();
    TickType_t last_report_tick = start_tick;
    const TickType_t report_period = pdMS_TO_TICKS(60000);
    while(1)
    {
        //阻塞等待队列出现新的数据
        //if (xQueueReceive(gnss_uart2_to_rx_queue,recv_data,portMAX_DELAY) == pdPASS)
        //{
                //printf("GNSS Uart2 Send:%s\r\n",recv_data);
        //}
        if (xQueueReceive(gnss_uart2_to_rx_queue,recv_data,pdMS_TO_TICKS(1000)) == pdPASS)
        {
            if (strncmp(recv_data, "$GPGGA", 6) == 0 || strncmp(recv_data, "$GNGGA", 6) == 0)
            {
                uart2_gga_count++;
            }
            else if (strncmp(recv_data, "$GPRMC", 6) == 0 ||strncmp(recv_data, "$GNRMC", 6) == 0)
            {
                uart2_rmc_count++;
            }
            else if (strncmp(recv_data, "$GPZDA", 6) == 0 ||strncmp(recv_data, "$GNZDA", 6) == 0)
            {
                uart2_zda_count++;
            }

            int result = nmea_checksum_check(recv_data);

            if (result == 1)
            {
                checked_count++;
                pass_count++;
            }
            else if (result == 0)
            {
                checked_count++;
                fail_count++;
                printf("fail meg : %s\r\n",recv_data);
            }
            else
            {
                invalid_count++;
                printf("invalid meg : %s\r\n",recv_data);
            }
        }
        TickType_t now = xTaskGetTickCount();

        if ((TickType_t)(now - last_report_tick) >= report_period)
        {
            uint32_t fail_rate_x100 = 0;

            if (checked_count > 0)
            {
                fail_rate_x100 =(uint32_t)(((uint64_t)fail_count * 10000U) / checked_count);
            }
            uint32_t elapsed_seconds =(uint32_t)((now - start_tick) / configTICK_RATE_HZ);

            if (uart_printf_mutex != NULL && xSemaphoreTake(uart_printf_mutex, portMAX_DELAY) == pdTRUE)
            {
                printf(
                "\r\n===== GNSS COM2 CHECKSUM STATISTICS =====\r\n"
                "Elapsed       : %lu seconds\r\n"
                "Checked       : %lu\r\n"
                "Pass          : %lu\r\n"
                "Fail          : %lu\r\n"
                "Failure rate  : %lu.%02lu%%\r\n"
                "Invalid format: %lu\r\n"
                "Queue drops   : %lu\r\n"
                "GGA MSG COUNT : %lu\r\n"
                "RMC MSG COUNT : %lu\r\n"
                "ZDA MSG COUNT : %lu\r\n"
                "=========================================\r\n",
                (unsigned long)elapsed_seconds,
                (unsigned long)checked_count,
                (unsigned long)pass_count,
                (unsigned long)fail_count,
                (unsigned long)(fail_rate_x100 / 100),
                (unsigned long)(fail_rate_x100 % 100),
                (unsigned long)invalid_count,
                (unsigned long)gnss_com2_dropped_count,
                (unsigned long)uart2_gga_count,
                (unsigned long)uart2_rmc_count,
                (unsigned long)uart2_zda_count

            );
            xSemaphoreGive(uart_printf_mutex);
            }
            last_report_tick = now;
        }
    }
        
}

void gnss_uart2_to_rx_task_init(void)
{
    //创建gnss_uart2_to_rx_task的队列；
    gnss_uart2_to_rx_queue = xQueueCreate(32, 128);
    if(gnss_uart2_to_rx_queue == NULL)
    {
        printf("gnss_uart2_to_rx_queue create fail\r\n");
    }
    xTaskCreate(gnss_uart2_to_rx_task, "gnss_uart2_to_rx_task", 256, NULL , 1, &gnss_uart2_to_rx_task_handle);
}


/* 将十六进制字符转换为数值 */
static int hex_to_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    return -1;
}
/*
 *  返回值：
 *  1：校验通过
 *  0：校验失败
 * -1：格式不正确或没有校验字段
 */
static int nmea_checksum_check(const char *line)
{
    //不属于gnss字段信息判断为invalid
    if (line == NULL || line[0] != '$')
        return -1;
    //找到该字节第一次出现的位置 返回其指针
    const char *star = strchr(line, '*');
    //当star为\0代表找到字符串末尾都没找到该数据同样判断为invalid
    if (star == NULL || star[1] == '\0' || star[2] == '\0')
        return -1;

    /* 本函数要求 *HH 后面直接结束 */
    if (star[3] != '\0')
        return -1;
    //将十六进制转化为十进制信息
    int high = hex_to_value(star[1]);
    int low  = hex_to_value(star[2]);

    //说明不是16进制 0-F
    if (high < 0 || low < 0)
        return -1;

    uint8_t calculated = 0;

    /* 只计算 $ 和 * 之间的字符 */
    //line第一个字节为$,舍去，str指向最后校验值前的一个字符
    for (const char *p = line + 1; p < star; p++)
    {
        calculated ^= (uint8_t)*p;
    }
    //high为高位，左移后 或 操作相当于两者相加
    uint8_t received = (uint8_t)((high << 4) | low);

    return (calculated == received) ? 1 : 0;
}


/* printf 打印互斥锁 */

void uart_printf_mutex_init(void)
{
    uart_printf_mutex = xSemaphoreCreateMutex();

    if(uart_printf_mutex == NULL)
    {
        printf("uart_printf_mutex_init fail!\r\n");
    }
}