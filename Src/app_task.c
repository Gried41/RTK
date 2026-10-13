#include "FreeRTOS.h"
#include "task.h"

#include "app_at.h"
#include "app_task.h"
#include "app_bt.h"
#include "app_ble.h"
#include "app_audio.h"
#include "app_rpmsg.h"
#include "app_config.h"
#include "mcu_imu.h"

static struct co_list event_list;

extern TaskHandle_t app_task_handle;
void hci_controller_read(void);
void hci_host_read(void);

static void app_task_event_handler(void)
{
    struct app_task_event *event = NULL;

    taskENTER_CRITICAL();
    event = (struct app_task_event *)co_list_pop_front(&event_list);//临界区
    /*
        可以认为从event_list链表中取出一个事件，event_list是一个全局变量，类型是struct co_list，每一个节点都是一个任务
        取出一个任务赋予event指针，后续指针拿着这个事件去判断类型并且执行相应的函数
    */
    taskEXIT_CRITICAL();
    
    if(event) 
    {
        switch(event->event_type) 
        {
            case APP_TASK_EVENT_AT_CMD:
                app_at_cmd_recv_handler(event->param, event->param_len);
                break;
            case APP_TASK_EVENT_RPMSG_INITED:
                app_btdm_init();                //简单初始化蓝牙前期的硬件和软件环境，启动蓝牙底层Controller和Host协议栈
                break;
            case APP_TASK_EVENT_HOST_INITED:
                /*
                    分为user_bt_init和app_bt_init两个函数，user_bt_init是用户自定义的蓝牙初始化函数，app_bt_init是SDK提供的蓝牙初始化函数
                    user_bt_init函数主要是初始化蓝牙协议栈的各个模块，包括HF、A2DP、AVRCP、SPP等，并且注册各个模块的回调函数，
                    app_bt_init函数主要是初始化蓝牙协议栈的各个模块，包括HCI、L2CAP、RFCOMM、SDP等，并且注册各个模块的回调函数
                */
                app_btdm_start();
                break;
            case APP_TASK_EVENT_AUDIO:
                app_audio_event_handler(event);
                break;
            default:
                break;
        }
        
        vPortFree((void *)event);
    }
}

void app_task_event_post(struct app_task_event *event, bool high)
{
    uint32_t old_basepri;
    
    if(xPortIsInsideInterrupt()) 
    {
        BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
        old_basepri = taskENTER_CRITICAL_FROM_ISR();
        if(high) {
            co_list_push_front(&event_list, &event->hdr);
        }
        else {
            co_list_push_back(&event_list, &event->hdr);
        }
        taskEXIT_CRITICAL_FROM_ISR(old_basepri);
        vTaskNotifyGiveFromISR(app_task_handle, &pxHigherPriorityTaskWoken);    //唤醒app_task任务，通知有事件发生
        portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
    }
    else {
        taskENTER_CRITICAL();
        if(high) {
            co_list_push_front(&event_list, &event->hdr);
        }
        else {
            co_list_push_back(&event_list, &event->hdr);
        }
        taskEXIT_CRITICAL();
        xTaskNotifyGive(app_task_handle);
    }
}

struct app_task_event *app_task_event_alloc(uint8_t event_type, uint32_t param_len, bool block)
{
    struct app_task_event *event = NULL;
    
    while(event == NULL) {
        event = pvPortMalloc(sizeof(struct app_task_event) + param_len);
        if (event == NULL) {
            if (block) {
                vTaskDelay(10);
            }
            else {
                return NULL;
            }
        }
    }
    
    event->event_type = event_type;
    event->param_len = param_len;
    
    return event;
}
//app_task只会执行一次并且不断跑下去，用于初始化任务链表，任务通信，等待任务被唤醒然后执行handler函数，handler函数会根据事件类型去执行相应的处理函数
static void app_task(void *arg) //参数来自xTaskCreate函数的第四个参数xTaskCreate(app_task, "app", APP_TASK_STACK_SIZE, NULL ---)这里是NULL
{
    co_list_init(&event_list);
    /*事件列表初始化，将事件列表初始化为空链表，co_list_init函数是SDK提供的链表初始化函数，event_list是一个全局变量，
      类型是struct co_list，表示一个链表，链表的每个节点是一个struct co_list_hdr结构体，
      struct co_list_hdr结构体包含一个指向下一个节点的指针和一个指向上一个节点的指针
    */
    app_rpmsg_init();
    /*
        通信初始化
        void app_rpmsg_init(void)
        {
            create rpmsg task. 
            xTaskCreate(rpmsg_task, "RPMSG_TASK", RPMSG_TASK_STACK_SIZE, NULL, RPMSG_TASK_PRIORITY, &rpmsg_task_handle);
        }
            本质上也是一个任务
    */

    while(1) 
    {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        /*
            让当前任务，app_task，进入阻塞状态，直到有事件发生
            ulTaskNotifyTake函数是FreeRTOS提供的任务通知函数，用于等待任务通知，pdFALSE表示不清除通知值，portMAX_DELAY表示无限等待
            其他任务通过xTaskNotifyGive(workerTaskHandle);来唤醒它 参数workerTaskHandle为要唤醒任务的句柄。这里为app_task_handle
            通知可以理解为一个变量，别的任务通知一次该变量+1，执行完成后通知值变量-1，只有通知变量大于0时才会唤醒任务。
            所以是否清除通知值取决于当前任务是否需要多次唤醒，如果不需要则可以清除通知值
        */
        app_task_event_handler();
        /*
            当app_task任务被唤醒后，调用app_task_event_handler函数处理事件，app_task_event_handler函数会从事件列表中取出一个事件，并根据事件类型调用相应的处理函数

        */
    }
}
void app_task_init(void)
{
    xTaskCreate(app_task, "app", APP_TASK_STACK_SIZE, NULL, APP_TASK_PRIORITY, &app_task_handle);
} 
/*
my_task_handle 不是回调函数，只是一个任务句柄，用于给后续函数调用该任务，
"my_task"是任务名，用于调试和查看任务状态，NULL是任务参数。
*/