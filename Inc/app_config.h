#ifndef _APP_CONFIG_H
#define _APP_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

/* ========================================================== */
/* =========         SYSTEM configuration           ========= */
/* ========================================================== */

// <h> SYSTEM Configuration

// <o> SYSTEM_CLOCK_SEL
//  <i> used to define system working clock
//  <24000000=> 24MHz
//  <48000000=> 48MHz
//  <96000000=> 96MHz
//  <144000000=> 144MHz
//  <192000000=> 192MHz
//  <240000000=> 240MHz
#define SYSTEM_CLOCK_SEL                    24000000

// <o> DSP_CODE_LOAD_MODE
//  <i> choose dsp code load mode: 1. from sd card; 2. compiled into XIP bin file; 3. from fixed address in DSP XIP flash
//  <1=> DSP_CODE_LOAD_FROM_SDCARD
//  <2=> DSP_CODE_LOAD_FROM_MCU_FLASH
//  <3=> DSP_CODE_LOAD_FROM_DSP_FLASH
#define DSP_CODE_LOAD_FROM_SDCARD           1
#define DSP_CODE_LOAD_FROM_MCU_FLASH        2
#define DSP_CODE_LOAD_FROM_DSP_FLASH        3
#define DSP_CODE_LOAD_MODE            2

//  <i> DSP USE PSRAM or not 
#define DSP_CODE_USE_PSRAM                  0
//  <i> DSP USE SRAM as heap or not 
#define DSP_CODE_USE_SRAM_HEAP              0   

#define CONTROLLER_CODE_LOAD_MODE_SDCARD        1
#define CONTROLLER_CODE_LOAD_MODE_XIP_RO        2
#define CONTROLLER_CODE_LOAD_MODE_FIX_ADDRESS   3

// <o> CONTROLLER_CODE_LOAD_MODE
//  <i> choose controller code load mode: 1. from sd card; 2. compiled into XIP bin file; 3. from fixed address in XIP flash
//  <1=> CONTROLLER_CODE_LOAD_MODE_SDCARD
//  <2=> CONTROLLER_CODE_LOAD_MODE_XIP_RO
//  <3=> CONTROLLER_CODE_LOAD_MODE_FIX_ADDRESS
#define CONTROLLER_CODE_LOAD_MODE               2

// <o> CONTROLLER_CODE_INC_SENSOR_HUB
//  <i> when CONTROLLER_CODE_LOAD_MODE is CONTROLLER_CODE_LOAD_MODE_XIP_RO, choose which firmware is commpiled into XIP bin file.
//  <0=> NO
//  <1=> YES
#define CONTROLLER_CODE_INC_SENSOR_HUB          0

// <o> CONTROLLER_CODE_FIX_ADDRESS
//  <i> used to define where to fetch controller code, this defination is valid when CONTROLLER_CODE_LOAD_MODE is CONTROLLER_CODE_LOAD_MODE_FIX_ADDRESS
#define CONTROLLER_CODE_FIX_ADDRESS         0x001AC000

// </h>

/* ========================================================== */
/* =========         FreeRTOS configuration         ========= */
/* ========================================================== */

// <h> FreeRTOS Configuration

// <o> ENABLE_RTOS_MONITOR
//  <i> check to enable or disable RTOS monitor, default: DISABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define ENABLE_RTOS_MONITOR                 1
//是否打开任务监视器
// <o> FREERTOS_MAX_PRIORITY
//  <i> used to define priority of monitor task
//  <1-10:1>
#define FREERTOS_MAX_PRIORITY               10

// <o> MONITOR_TASK_PRIORITY
//  <i> used to define priority of monitor task, should not be larger than FREERTOS_MAX_PRIORITY
#define MONITOR_TASK_PRIORITY               1
#if MONITOR_TASK_PRIORITY > FREERTOS_MAX_PRIORITY
    #error "MONITOR_TASK_PRIORITY should not be larger than FREERTOS_MAX_PRIORITY"
#endif

// <o> APP_TASK_PRIORITY
//  <i> used to define priority of app task, should not be larger than FREERTOS_MAX_PRIORITY
#define APP_TASK_PRIORITY                   2
#if APP_TASK_PRIORITY > FREERTOS_MAX_PRIORITY
    #error "APP_TASK_PRIORITY should not be larger than FREERTOS_MAX_PRIORITY"
#endif

// <o> HOST_TASK_PRIORITY
//  <i> used to define priority of host task, should not be larger than FREERTOS_MAX_PRIORITY
#define HOST_TASK_PRIORITY                  5
#if HOST_TASK_PRIORITY > FREERTOS_MAX_PRIORITY
    #error "HOST_TASK_PRIORITY should not be larger than FREERTOS_MAX_PRIORITY"
#endif

// <o> RPMSG_TASK_PRIORITY
//  <i> used to define priority of rpmsg task, should not be larger than FREERTOS_MAX_PRIORITY
#define RPMSG_TASK_PRIORITY                 6
#if RPMSG_TASK_PRIORITY > FREERTOS_MAX_PRIORITY
    #error "RPMSG_TASK_PRIORITY should not be larger than FREERTOS_MAX_PRIORITY"
#endif

// <o> AUDIO_SCENE_TASK_PRIORITY
//  <i> used to define priority of rpmsg task, should not be larger than FREERTOS_MAX_PRIORITY
#define AUDIO_SCENE_TASK_PRIORITY           6
#if AUDIO_SCENE_TASK_PRIORITY > FREERTOS_MAX_PRIORITY
    #error "AUDIO_SCENE_TASK_PRIORITY should not be larger than FREERTOS_MAX_PRIORITY"
#endif

// <o> MONITOR_TASK_STACK_SIZE
//  <i> used to define priority of monitor task
#define MONITOR_TASK_STACK_SIZE             128

// <o> APP_TASK_STACK_SIZE
//  <i> used to define priority of APP task
#define APP_TASK_STACK_SIZE                 1024 //256

// <o> HOST_TASK_STACK_SIZE
//  <i> used to define priority of btdm-host task
#define HOST_TASK_STACK_SIZE                2048

// <o> RPMSG_TASK_STACK_SIZE
//  <i> used to define priority of RPMSG task
#define RPMSG_TASK_STACK_SIZE                2048

// <o> AUDIO_SCENE_TASK_STACK_SIZE
//  <i> used to define priority of audio scene task
#define AUDIO_SCENE_TASK_STACK_SIZE          1024

// </h>

/* ========================================================== */
/* =========           BTDM configuration           ========= */
/* ========================================================== */

// <h> BTDM STACK Configuration

// <o> BTDM_STACK_ENABLE
//  <i> check to enable or disable bluetooth, default: DISABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define BTDM_STACK_ENABLE                   1

// <o> BTDM_STACK_HCI_BAUDRATE
//  <i> check to enable or disable bluetooth, default: DISABLE
//  <1500000=> 1500000
//  <921600=> 921600
//  <460800=> 460800
//  <115200=> 115200
#define BTDM_STACK_HCI_BAUDRATE             1500000

// <o> BTDM_STACK_ENABLE_BT
//  <i> check to enable or disable classic bluetooth, default: DISABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define BTDM_STACK_ENABLE_BT                1

// <o> BTDM_STACK_ENABLE_HF
//  <i> check to enable or disable hand free, default: DISABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define BTDM_STACK_ENABLE_HF                0

// <o> BTDM_STACK_ENABLE_AG
//  <i> check to enable or disable audio-gate, default: DISABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define BTDM_STACK_ENABLE_AG                0

// <o> BTDM_STACK_ENABLE_A2DP_SRC
//  <i> check to enable or disable a2dp srouce, default: DISABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define BTDM_STACK_ENABLE_A2DP_SRC          0

// <o> BTDM_STACK_ENABLE_A2DP_SNK
//  <i> check to enable or disable a2dp sink, default: DISABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define BTDM_STACK_ENABLE_A2DP_SNK          0

// <o> BTDM_STACK_ENABLE_AVRCP
//  <i> check to enable or disable avrcp, default: DISABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define BTDM_STACK_ENABLE_AVRCP             0

// <o> BTDM_STACK_ENABLE_AAC
//  <i> check to enable or disable aac codec, default: DISABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define BTDM_STACK_ENABLE_AAC               0

// <o> BTDM_STACK_ENABLE_PBAP
//  <i> check to enable or disable pbap, default: DISABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define BTDM_STACK_ENABLE_PBAP              0

// <o> BTDM_STACK_ENABLE_SPP
//  <i> check to enable or disable spp, default: DISABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define BTDM_STACK_ENABLE_SPP               1

// <o> BTDM_STACK_ENABLE_PAN
//  <i> check to enable or disable PAN, default: DISABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define BTDM_STACK_ENABLE_PAN               0

// <o> BTDM_STACK_ENABLE_MAP
//  <i> check to enable or disable map, default: DISABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define BTDM_STACK_ENABLE_MAP               0

// <o> BTDM_STACK_ENABLE_HID
//  <i> check to enable or disable hid, default: DISABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define BTDM_STACK_ENABLE_HID               0
// </h>

/* ========================================================== */
/* =========     BTDM Address configuration         ========= */
/* ========================================================== */

// <h> BTDM Address configuration

// <o> BT_ADDR_RANDOM_ENABLE
//  <i> check to enable or disable random bt addr, default: ENABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define BT_ADDR_RANDOM_ENABLE                   0

// <o> BLE_ADDR_RANDOM_ENABLE
//  <i> check to enable or disable random ble static addr, default: ENABLE
//  <0=> DISABLE
//  <1=> ENABLE
#define BLE_ADDR_RANDOM_ENABLE                   1

// </h>
// <<< end of configuration section >>>

#endif  // _APP_CONFIG_H
