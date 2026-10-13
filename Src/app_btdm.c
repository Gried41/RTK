#include "app_config.h"
#include "app_task.h"
#include "app_btdm.h"
#include "app_ble.h"
#include "app_bt.h"

#include "controller.h"
#include "host.h"
#include "fdb_app.h"
#include "user_bt.h"

void app_btdm_start(void)
{
    //app_ble_init();
#if BTDM_STACK_ENABLE_BT == 1
    user_bt_init();
    app_bt_init();
#endif
}

void host_ready_cb(void)
{
    struct app_task_event *event;
    /* notify application BTDM stack is ready. */
    event = app_task_event_alloc(APP_TASK_EVENT_HOST_INITED, 0, true);
    app_task_event_post(event, false);
}

void app_btdm_init(void)
{
    uint8_t bt_addr[] = {0x32, 0x32, 0x32, 0x32, 0x32, 0x32};
    uint8_t id_length = 5;
    
    system_get_unique_ID(bt_addr, &id_length);
    /* prepare for BTDM stack */
    /*  启动蓝牙底层 Controller
        初始化射频、基带、链路控制等硬件相关部分
        设置本机蓝牙地址（BD_ADDR）
        配置与 Host 通信的 HCI 波特率（BTDM_STACK_HCI_BAUDRATE）
    */
    controller_start(BTDM_STACK_HCI_BAUDRATE, bt_addr, bt_addr, 0);
    bt_addr[5] = 0xc0;
#if BTDM_STACK_ENABLE_BT == 1
    /*
        启动蓝牙 Host 协议栈（L2CAP、RFCOMM、SDP、GAP、SPP 等上层）
        创建 Host 任务（HOST_TASK_STACK_SIZE、HOST_TASK_PRIORITY）
        通过 HCI 与已经启动的 Controller 通信
    */
    host_btdm_start(BTDM_STACK_HCI_BAUDRATE, HOST_TASK_STACK_SIZE, HOST_TASK_PRIORITY, bt_addr);
#else
    host_ble_start(BTDM_STACK_HCI_BAUDRATE, HOST_TASK_STACK_SIZE, HOST_TASK_PRIORITY, bt_addr);
#endif
    
    /* 
     * init MCU->BT pin, configure PMU_PIN_8 output BBG_EN signal, this pin is used to 
     * notice BT core that MCU is in working mode.
     */
    /*
        PMU是一个GPIO控制器，除了有普通GPIO功能之外，还能够参与电源的低功耗，能够控制一些特殊的功能，比如控制蓝牙模块的电源开关，控制MCU进入低功耗模式等。
        往PMU中的一个寄存器PMU_REG_DIAG_CTRL写入数据
    */
    ool_write(PMU_REG_DIAG_CTRL, 0x82);
    ool_write(PMU_REG_PIN_IOMUX_H, 0x03);

    /* 
     * init BT->MCU pin, system should not enter sleep mode when this pin is low level. 
     * This pin is used by BT core to notice MCU than BT core is in working mode.
     */
    system_prevent_sleep_set(SYSTEM_PREVENT_SLEEP_TYPE_HCI_RX);

    /*
        将PMU_PIN_9配置为输入上拉模式，将0作为参考状态，检测其状态变化，一旦变化就产生PMU中断。
    */
    pmu_gpio_int_init(PMU_PIN_9, PMU_GPIO_PULL_UP, 0);
    pmu_enable_isr(PMU_GPIO_PMU_INT_MSK_BIT);
    NVIC_SetPriority(PMU_IRQn, 4);
    NVIC_EnableIRQ(PMU_IRQn);
}

