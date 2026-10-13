#include "fr30xx.h"

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

#include "rpmsg.h"
#include "rpmsg_lite.h"
#include "rpmsg_queue.h"
#include "rpmsg_ns.h"

#include "dsp.h"

#include "app_config.h"
#include "app_task.h"
#include "app_rpmsg.h"

/* Remote rpmsg instance */
static struct rpmsg_lite_instance *remote_rpmsg;
static TaskHandle_t rpmsg_task_handle;

static void rpmsg_recv(struct rpmsg_lite_instance *rpmsg, struct rpmsg_msg_t *msg)
{
//    printf("rpmsg_recv: msg_type = %d.\r\n", msg->msg_type);

    switch (msg->msg_type) {
        case RPMSG_MSG_TYPE_SYNC_INVOKE:
            if (msg->p.sync_func.func_id == RPMSG_SYNC_FUNC_SUM) {
                uint32_t address = (uint32_t)msg->p.sync_func.param;
                struct rpmsg_sync_msg_sum_t *param = msg->p.sync_func.param;
                if (address >= DSP_DRAM_BASE_ADDR) {
                    address = DSP_DRAM_2_MCU_SRAM(address);
                }
                param = (void *)address;
                rpmsg_send_sync_ret(rpmsg, 0, param->x + param->y);
            }
            else {
                rpmsg_send_sync_ret(rpmsg, 0xffffffff, 0);
            }
            break;
        case RPMSG_MSG_TYPE_ASYNC_MSG:
            break;
        default:
            break;
    }
}

static void rpmsg_task(void *arg)
{
    struct app_task_event *event;
    struct rpmsg_msg_t *msg;
    uint32_t msg_len;
    uint32_t src_addr;
    uint32_t length;
    
    dsp_prepare();

#if DSP_CODE_LOAD_MODE == DSP_CODE_LOAD_FROM_MCU_FLASH
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
    extern uint8_t DSP_CODE_ROM_BASE, DSP_CODE_ROM_END;
    src_addr = (uint32_t)&DSP_CODE_ROM_BASE;
    length = (uint32_t)&DSP_CODE_ROM_END - (uint32_t)&DSP_CODE_ROM_BASE;
#elif defined(__GNUC__) || defined(__ICCARM__)
    extern const uint8_t dsp_code_rom_buffer[];
    extern const uint32_t dsp_code_rom_buffer_size;
    src_addr = (uint32_t)&dsp_code_rom_buffer[0];
    length = dsp_code_rom_buffer_size;
#endif
    if (dsp_load_code_from_internal_flash(src_addr, length) == false) {

        printf("DSP image load failed.\r\n");
        while(1);
    }
#elif DSP_CODE_LOAD_MODE == DSP_CODE_LOAD_FROM_DSP_FLASH
    uint32_t iram_code_offset = *(volatile uint32_t *)DSP_FLASH_DAC_BASE;
    uint32_t *src = (void *)(DSP_FLASH_DAC_BASE + iram_code_offset);
    uint32_t *dest = (void *)(DSP_IMEM_MCU_BASE_ADDR+0x400);
    length = *(volatile uint32_t *)(DSP_FLASH_DAC_BASE+4);
    for (uint32_t i=0; i<length; i+=4) {
        *dest++ = *src++;
    }
#elif DSP_CODE_LOAD_MODE == DSP_CODE_LOAD_FROM_SDCARD
    if (dsp_load_code_from_fs("bin\\dsp.bin") == false) {

        printf("DSP image load failed.\r\n");
        while(1);
    }
#else
    #error "Unsupported load mode"
#endif

    printf("DSP is released.\r\n");
    remote_rpmsg = rpmsg_remote_init(rpmsg_recv);
    dsp_run(0x78400660);
    system_prevent_sleep_set(SYSTEM_PREVENT_SLEEP_TYPE_DSP);
    rpmsg_wait_master_ready(remote_rpmsg);
#if DSP_CODE_USE_SRAM_HEAP == 1
    dsp_mem_sram_heap_init(0x20070000, 0x10000);
#endif
    system_prevent_sleep_clear(SYSTEM_PREVENT_SLEEP_TYPE_DSP);

    printf("DSP is ready.\r\n");

    /* notice APP that rpmsg is ready. */
	event = app_task_event_alloc(APP_TASK_EVENT_RPMSG_INITED, 0, true);
    app_task_event_post(event, false);

    while (1) 
    {
        src_addr = rpmsg_recv_msg(remote_rpmsg, &msg, &msg_len);    //类似socket编程 阻塞等待消息
        rpmsg_recv(remote_rpmsg, msg);                              //对收到消息进一步处理
        rpmsg_lite_release_rx_buffer(remote_rpmsg, msg);            //释放缓冲区，
    }
}

__WEAK void app_rpmsg_ready(void)
{
}

void app_rpmsg_init(void)
{
    /* create rpmsg task. */
    xTaskCreate(rpmsg_task, "RPMSG_TASK", RPMSG_TASK_STACK_SIZE, NULL, RPMSG_TASK_PRIORITY, &rpmsg_task_handle);
}

void app_rpmsg_recover(void)
{
    dsp_prepare();
    
    uint32_t src_addr;
    uint32_t length;

#if DSP_CODE_LOAD_MODE == DSP_CODE_LOAD_FROM_MCU_FLASH
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
    extern uint8_t DSP_CODE_ROM_BASE, DSP_CODE_ROM_END;
    src_addr = (uint32_t)&DSP_CODE_ROM_BASE;
    length = (uint32_t)&DSP_CODE_ROM_END - (uint32_t)&DSP_CODE_ROM_BASE;
#elif defined(__GNUC__) || defined(__ICCARM__)
    extern const uint8_t dsp_code_rom_buffer[];
    extern const uint32_t dsp_code_rom_buffer_size;
    src_addr = (uint32_t)&dsp_code_rom_buffer[0];
    length = dsp_code_rom_buffer_size;
#endif
    dsp_load_rw_from_internal_flash(src_addr, length);
#elif DSP_CODE_LOAD_MODE == DSP_CODE_LOAD_FROM_DSP_FLASH
    uint32_t iram_code_offset = *(volatile uint32_t *)DSP_FLASH_DAC_BASE;
    uint32_t *src = (void *)(DSP_FLASH_DAC_BASE + iram_code_offset);
    uint32_t *dest = (void *)(DSP_IMEM_MCU_BASE_ADDR+0x400);
    length = *(volatile uint32_t *)(DSP_FLASH_DAC_BASE+4);
    for (uint32_t i=0; i<length; i+=4) {
        *dest++ = *src++;
    }
#else
    #error "choose correct DSP code load mode"
#endif

    GLOBAL_INT_DISABLE();
    dsp_run(0x78400660);
    rpmsg_remote_recover();
    GLOBAL_INT_RESTORE();
}

