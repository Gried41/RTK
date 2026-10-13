#ifndef _APP_TASK_H
#define _APP_TASK_H

#include <stdint.h>
#include <stdbool.h>

#include "co_list.h"

#define APP_TASK_EVENT_AT_CMD           0x00
#define APP_TASK_EVENT_RPMSG_INITED     0x01
#define APP_TASK_EVENT_HOST_INITED      0x02
#define APP_TASK_EVENT_AUDIO            0x03

struct app_task_event {
    struct co_list_hdr hdr;
    
    uint8_t event_type;

    uint16_t param_len;
    uint8_t param[];
};

void app_task_event_post(struct app_task_event *event, bool high);

struct app_task_event *app_task_event_alloc(uint8_t event_type, uint32_t param_len, bool block);

void app_task_init(void);



#endif  // _APP_TASK_H
