/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "fr30xx.h"

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

#include "fdb_app.h"
#include "host.h"
#include "mcu_imu.h"
#include "app_task.h"
#include "app_at.h"
#include "audio_scene.h"
#include "app_rpmsg.h"
#if DSP_CODE_LOAD_MODE == DSP_CODE_LOAD_FROM_DSP_FLASH
#include "ext_flash_program.h"
#endif
#if DSP_CODE_USE_PSRAM == 1
#include "driver_psram.h"
#endif

/* hardware handlers */
static UART_HandleTypeDef Uart1_handle;
static UART_HandleTypeDef Uart2_handle;
static UART_HandleTypeDef Uart3_handle;
static UART_HandleTypeDef Uart4_handle;
static UART_HandleTypeDef dsp_uart_handle;
static CALI_HandleTypeDef cali_handle;
#if ENABLE_RTOS_MONITOR == 1
/* FreeRTOS running status monitor task */
static TaskHandle_t monitor_task_handle;
volatile unsigned int CPU_RunTime;
static uint8_t CPU_RunInfo[2048];
#endif

/* APP task */
TaskHandle_t app_task_handle;

void controller_start(void);
void host_start(void);

#if defined(__ARMCC_VERSION) || defined(__CC_ARM)
int fputc(int c, FILE *fp)
{
    uart_transmit(&Uart3_handle, (void *)&c, 1);

    return c;
}
#endif

#ifdef __GNUC__
int _write(int file, char *ptr, int len)
{
    uart_transmit(&Uart3_handle, (void *)ptr, len);
    return len;
}
#endif

#ifdef __ICCARM__
int putchar(int c)
{
    uart_transmit(&Uart3_handle, (void *)&c, 1);
    while(!(Uart3_handle.UARTx->USR.TFE));
    return c;
}
#endif

#if ENABLE_RTOS_MONITOR == 1
static void monitor_task(void *arg)
{
    while(1) {
        vTaskDelay(10000);

        memset(CPU_RunInfo,0,2048);
        vTaskList((char *)&CPU_RunInfo);
        printf("---------------------------------------------\r\n");
        printf("name           	state     priority  stack     seq\r\n");
        printf("%s", CPU_RunInfo);
        printf("---------------------------------------------\r\n");
        
        memset(CPU_RunInfo,0,400);
        vTaskGetRunTimeStats((char *)&CPU_RunInfo);
        printf("name                counter             usage\r\n");
        printf("%s", CPU_RunInfo);
        printf("---------------------------------------------\r\n");
    }
}
#endif

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	assert( 0 );
}

void vApplicationTickHook(void)
{
    #if ENABLE_RTOS_MONITOR == 1
    CPU_RunTime++;
    #endif
}
	
static void cali_done_handle(CALI_HandleTypeDef *hcali, uint32_t result)
{
    system_set_LPRCCLK(cali_calc_rc_freq(hcali, result));
    system_prevent_sleep_clear(SYSTEM_PREVENT_SLEEP_TYPE_CALIBRATION);
}

#if DSP_CODE_LOAD_MODE == DSP_CODE_LOAD_FROM_DSP_FLASH
static uint32_t ext_flash_read_id(void)
{
    return flash_read_id(QSPI1);
}

static uint32_t ext_flash_write(uint32_t offset, uint32_t length, const uint8_t *buffer)
{
    flash_write(QSPI1, offset, length, buffer);
    return length;
}

static uint32_t ext_flash_read(uint32_t offset, uint32_t length, uint8_t *buffer)
{
    flash_read(QSPI1, offset, length, buffer);
    return length;
}

static void ext_flash_erase(uint32_t offset, uint32_t length)
{
    flash_erase(QSPI1, offset, length);
}

static const struct ext_flash_operator_t ext_flash_op = {
    .flash_init = ext_flash_read_id,
    .read = ext_flash_read,
    .write = ext_flash_write,
    .erase = ext_flash_erase,
    .chip_erase = NULL,
    .protect_disable = NULL,
    .protect_enable = NULL,
};

static void prog_uart_init(uint32_t bandrate)
{
    GPIO_InitTypeDef gpio_config;

    /* configure PB4 and PB5 to UART3 function */
    gpio_config.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    gpio_config.Mode = GPIO_MODE_AF_PP;
    gpio_config.Pull = GPIO_PULLUP;
    gpio_config.Alternate = GPIO_FUNCTION_1;
    gpio_init(GPIOB, &gpio_config);

    /* UART3: used for Log and AT command */
    __SYSTEM_UART3_CLK_ENABLE();
    Uart3_handle.UARTx           = UART3;
    Uart3_handle.Init.BaudRate   = bandrate;
    Uart3_handle.Init.DataLength = UART_DATA_LENGTH_8BIT;
    Uart3_handle.Init.StopBits   = UART_STOPBITS_1;
    Uart3_handle.Init.Parity     = UART_PARITY_NONE;
    Uart3_handle.Init.FIFO_Mode  = UART_FIFO_ENABLE;
    Uart3_handle.TxCpltCallback  = NULL;
    Uart3_handle.RxCpltCallback  = NULL;
    uart_init(&Uart3_handle);
    NVIC_SetPriority(UART3_IRQn, 4);
}

static void prog_uart_read(uint8_t *data, uint16_t length)
{
    uart_receive(&Uart3_handle, data, length);
}

static uint16_t prog_uart_read_no_block(uint8_t *data, uint16_t length)
{
    int len = 0;
    UART_HandleTypeDef *huart = &Uart3_handle;

    while (len < length)
    {
        /* Rx ready */
        if (!(huart->UARTx->LSR.LSR_BIT.DR))
            break;
        /* receive data */
        data[len++] = huart->UARTx->DATA_DLL.DATA;
    }
    return len;
}

static void prog_uart_write(uint8_t *data, uint16_t length)
{
    uart_transmit(&Uart3_handle, data, length);
}

static const struct ext_flash_prog_uart_op_t ext_flash_prog_uart_op = {
    .init = prog_uart_init,
    .read = prog_uart_read,
    .read_no_block = prog_uart_read_no_block,
    .write = prog_uart_write,
};
#endif

__RAM_CODE void hw_clock_init(void)
{
    System_ClkConfig_t sys_clk_cfg;

    sys_clk_cfg.SPLL_CFG.PLL_N = 8;
    sys_clk_cfg.SPLL_CFG.PLL_M = 0;
    sys_clk_cfg.SPLL_CFG.PowerEn = 1;
    sys_clk_cfg.MCU_Clock_Source = MCU_CLK_SEL_SPLL_CLK;
    sys_clk_cfg.SOC_DIV = 1;
    sys_clk_cfg.MCU_DIV = 1;
    sys_clk_cfg.APB0_DIV = 1;
    sys_clk_cfg.APB1_DIV = 1;
    sys_clk_cfg.APB2_DIV = 1;
    System_SPLL_config(&sys_clk_cfg.SPLL_CFG, 1000);
    System_MCU_clock_Config(&sys_clk_cfg);

    __SYSTEM_UART_CLK_SELECT_SPLL();
}

__RAM_CODE void hw_xip_flash_init(bool wake_up)
{
    System_XIPConfig_t xip_config;
    xip_config.CLK_SRC_SEL = XIP_CLK_SEL_SPLL;
    xip_config.DIV_SEL = QSPI_BAUDRATE_DIV_4;
    xip_config.RD_TYPE = FLASH_RD_TYPE_QUAD;
    xip_config.WR_TYPE = FLASH_WR_TYPE_SINGLE;
    system_xip_flash_init(&xip_config, wake_up);
}
#if DSP_CODE_LOAD_MODE == DSP_CODE_LOAD_FROM_DSP_FLASH
__RAM_CODE void hw_dsp_xip_flash_init(bool wake_up)
{
    /* init QSPI1 for DSP XIP flash */
    __SYSTEM_QSPI1_CLK_SELECT_SPLL();
    __SYSTEM_QSPI1_CLK_ENABLE();

#if 0
    /* config OSPI pad to QSPI1 function, used to access DSP XIP flash */
    SYSTEM->OspiPadConfig.OSPI_FuncMux = 0x55555555;
    
    /* power on DSP flash */
    ool_write(0xfc, ool_read(0xfc) | 0x01);
#else
    {
        /* config PC12~PC15 to QSPI1 function, used to access DSP XIP flash */
        GPIO_InitTypeDef gpio_config;
        gpio_config.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
        gpio_config.Mode = GPIO_MODE_AF_PP;
        gpio_config.Pull = GPIO_PULLUP;
        gpio_config.Alternate = GPIO_FUNCTION_C;
        gpio_init(GPIOB, &gpio_config);
        gpio_config.Pin = GPIO_PIN_0 | GPIO_PIN_1;
        gpio_config.Mode = GPIO_MODE_AF_PP;
        gpio_config.Pull = GPIO_PULLUP;
        gpio_config.Alternate = GPIO_FUNCTION_C;
        gpio_init(GPIOC, &gpio_config);
    }
#endif

    /* configure QSPI1 controller, used to access DSP XIP flash */
    flash_init_controller(QSPI1, FLASH_RD_TYPE_QUAD_FAST, FLASH_WR_TYPE_SINGLE);
    __QSPI_DELAY_CS_START_SET(QSPI1, 4);
    __QSPI_DELAY_CS_END_SET(QSPI1, 4);
    flash_set_baudrate(QSPI1, QSPI_BAUDRATE_DIV_4);
    __QSPI_READ_CAPTURE_DELAY_SET(QSPI1, 2);
    flash_enable_quad(QSPI1);
}
#endif

__RAM_CODE bool user_deep_sleep_check(void)
{
    return host_before_sleep_check();
}

__RAM_CODE void user_entry_before_sleep(void)
{
    while(!(Uart3_handle.UARTx->USR.TFE));
    system_delay_us(100);
    ool_write16(PMU_REG_PIN_PULL_EN, 0x3fff);
    ool_write16(PMU_REG_PIN_PULL_SEL, 0x3ffd);

    ool_write(PMU_REG_PMU_GATE_M, ool_read(PMU_REG_PMU_GATE_M) | 0x40);
}

__RAM_CODE void user_entry_after_sleep(void)
{
    /* 
     * enable pull up of all 3.3v IO, these configuration will be latched by set 
     * BIT6 of PMU_REG_PMU_GATE_M regsiter. used to avoid electric leakage
     */
    SYSTEM->PortA_PullSelect = 0x0000ffff;
    SYSTEM->PortB_PullSelect = 0x0000fff7;
    SYSTEM->PortC_PullSelect = 0x0000ffff;
    SYSTEM->PortD_PullSelect = 0x0000ffff;
    SYSTEM->PortE_PullSelect = 0x0000ffff;
    SYSTEM->PortA_PullEN = 0x0000ffff;
    SYSTEM->PortB_PullEN = 0x0000ffff;
    SYSTEM->PortC_PullEN = 0x0000ffff;
    SYSTEM->PortD_PullEN = 0x0000ffff;
    SYSTEM->PortE_PullEN = 0x0000ffff;
    SYSTEM->QspiPadConfig.QSPI_PullEN = 0x0000000;
    
    SYSTEM->PortA_InputOpenCircuit = 0xFFFFFFFF;
    SYSTEM->PortB_InputOpenCircuit = 0xFFFFFFFF;
    SYSTEM->PortC_InputOpenCircuit = 0xFFFFFFFF;
    SYSTEM->PortD_InputOpenCircuit = 0xFFFFFFFF;
    SYSTEM->PortE_InputOpenCircuit = 0xFFFFFFFF;

    hw_clock_init();
    hw_xip_flash_init(true);
    host_hci_reinit();
#if DSP_CODE_LOAD_MODE == DSP_CODE_LOAD_FROM_DSP_FLASH
    hw_dsp_xip_flash_init(true);
#endif

#if DSP_CODE_USE_PSRAM == 1
    psram_init(PSRAM_CLK_SEL_SPLL);
#endif
    ool_write(PMU_REG_PMU_GATE_M, ool_read(PMU_REG_PMU_GATE_M) & (~0x40));
    
    GPIO_InitTypeDef gpio_config;

    NVIC_SetPriority(UART0_IRQn, 2);
    NVIC_EnableIRQ(UART0_IRQn);
    NVIC_SetPriority(PMU_IRQn, 4);
    NVIC_EnableIRQ(PMU_IRQn);
    
    /* configure PB4 and PB5 to UART3 function */
    __SYSTEM_GPIOA_CLK_ENABLE();
    gpio_config.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    gpio_config.Mode = GPIO_MODE_AF_PP;
    gpio_config.Pull = GPIO_PULLUP;
    gpio_config.Alternate = GPIO_FUNCTION_1;
    gpio_init(GPIOB, &gpio_config);
    
    /* UART0: used for Log and AT command */
    __SYSTEM_UART3_CLK_ENABLE();
    Uart3_handle.UARTx = UART3;
    Uart3_handle.Init.BaudRate   = 921600;
    Uart3_handle.Init.DataLength = UART_DATA_LENGTH_8BIT;
    Uart3_handle.Init.StopBits   = UART_STOPBITS_1;
    Uart3_handle.Init.Parity     = UART_PARITY_NONE;
    Uart3_handle.Init.FIFO_Mode  = UART_FIFO_ENABLE;
    Uart3_handle.TxCpltCallback  = NULL;
    Uart3_handle.RxCpltCallback  = rx_to_spp_done;
    uart_init(&Uart3_handle);
    NVIC_SetPriority(UART3_IRQn, 4);
    NVIC_EnableIRQ(UART3_IRQn);
    Uart_Recv_IT(&Uart3_handle);

    // /* configure PA4 and PA5 to UART1 function */
    // gpio_config.Pin = GPIO_PIN_12 | GPIO_PIN_13;
    // gpio_config.Pull = GPIO_PULLUP;
    // gpio_config.Alternate = GPIO_FUNCTION_1;
    // gpio_init(GPIOA, &gpio_config);

    // /* UART1: used for DSP Log */
    // __SYSTEM_UART1_CLK_ENABLE();
    // dsp_uart_handle.UARTx = UART1;
    // dsp_uart_handle.Init.BaudRate   = 2000000;
    // dsp_uart_handle.Init.DataLength = UART_DATA_LENGTH_8BIT;
    // dsp_uart_handle.Init.StopBits   = UART_STOPBITS_1;
    // dsp_uart_handle.Init.Parity     = UART_PARITY_NONE;
    // dsp_uart_handle.Init.FIFO_Mode  = UART_FIFO_ENABLE;
    // dsp_uart_handle.Init.AUTO_FLOW  = UART_AUTO_FLOW_DISABLE;
    // dsp_uart_handle.RxCpltCallback  = NULL;
    // uart_init(&dsp_uart_handle);
    // NVIC_EnableIRQ(UART1_IRQn);
    // NVIC_SetPriority(UART1_IRQn, 4);
    
    {
        static bool first_wakeup = true;
        bool do_calib = false;
        static TickType_t last_tick;
        TickType_t curr_tick;
        if (first_wakeup) {
            first_wakeup = false;
            last_tick = xTaskGetTickCount();
            curr_tick = last_tick;
            do_calib = true;
        }
        else {
            curr_tick = xTaskGetTickCount();
            if ((curr_tick - last_tick) > 10000) {
                last_tick = curr_tick;
                do_calib = true;
            }
        }
        
        if (do_calib) {
            /* restart calibration */
            __SYSTEM_CALI_CLK_ENABLE();
            cali_handle.mode = CALI_UP_MODE_NORMAL;
            cali_handle.rc_cnt = 60;
            cali_handle.DoneCallback = cali_done_handle;
            cali_init(&cali_handle);
            cali_start_IT(&cali_handle);
            system_prevent_sleep_set(SYSTEM_PREVENT_SLEEP_TYPE_CALIBRATION);
            NVIC_SetPriority(CALI_IRQn, 2);
            NVIC_EnableIRQ(CALI_IRQn);
        }
    }
    app_rpmsg_recover();
}

__RAM_CODE void user_entry_after_sleep_user(void)
{    
    
}

int main( void )
{
    //test git pull
    GPIO_InitTypeDef gpio_config;
    // UART_HandleTypeDef dsp_uart_handle;
    uint32_t error;

    system_delay_us(1000000);

    pmu_init();
//    /* Power Keep: 32KB PRAM, 128KB SRAM */
//    ool_write16(PMU_REG_PKSRAM_GATE, ~0x00e3);
    
    /* initial system clock and XIP flash */
    hw_clock_init();
    hw_xip_flash_init(false);
#if DSP_CODE_LOAD_MODE == DSP_CODE_LOAD_FROM_DSP_FLASH
    hw_dsp_xip_flash_init(false);
    ext_flash_program(&ext_flash_op, &ext_flash_prog_uart_op);
#endif
#if DSP_CODE_USE_PSRAM == 1
    /* power on PSRAM */
    pmu_psram_power_ctrl(true);
    system_delay_us(1000);
    psram_init(PSRAM_CLK_SEL_SPLL);
#endif
    /*-------------------------------------------------UART3 INIT------------------------------------------------------------------------*/
    /*      GPIOA_CLKC_ENABLE也会使能能GOIOB 时钟       */
    __SYSTEM_GPIOA_CLK_ENABLE();
    /* configure PB4 and PB5 to UART3 function */
    gpio_config.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    gpio_config.Mode = GPIO_MODE_AF_PP;
    gpio_config.Pull = GPIO_PULLUP;
    gpio_config.Alternate = GPIO_FUNCTION_1;
    gpio_init(GPIOB, &gpio_config);

    /* UART3: used for Log and AT command */
    __SYSTEM_UART3_CLK_ENABLE();
    Uart3_handle.UARTx = UART3;
    Uart3_handle.Init.BaudRate   = 921600;
    Uart3_handle.Init.DataLength = UART_DATA_LENGTH_8BIT;
    Uart3_handle.Init.StopBits   = UART_STOPBITS_1; 
    Uart3_handle.Init.Parity     = UART_PARITY_NONE;
    Uart3_handle.Init.FIFO_Mode  = UART_FIFO_ENABLE;
    Uart3_handle.TxCpltCallback  = NULL;
    Uart3_handle.RxCpltCallback  = rx_to_gnss_done;  //RX 接收完毕后调用的回调函数
    uart_init(&Uart3_handle);
    NVIC_EnableIRQ(UART3_IRQn);                     //使能中断
    NVIC_SetPriority(UART3_IRQn, 4);
    Uart_Recv_IT(&Uart3_handle); //uart_receive_IT
    /*---------------------------------------------------------UART4 INIT--------------------------------------------------------------------------*/
    /*UART4 :used for connect to IMU19(UART1) --PD0 PD1*/
    __SYSTEM_GPIOD_CLK_ENABLE();
    //configure PD0 and PD1 to UART4 function
    gpio_config.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio_config.Mode = GPIO_MODE_AF_PP;
    gpio_config.Pull = GPIO_PULLUP;
    gpio_config.Alternate = GPIO_FUNCTION_1;
    gpio_init(GPIOD, &gpio_config);
    //将IMU_PWR_EN 拉高使能 PD3;
    gpio_config.Pin = GPIO_PIN_3;
    gpio_config.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_config.Pull = GPIO_NOPULL;
    gpio_init(GPIOD, &gpio_config);
    gpio_write_pin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET);
    //UART4 configure
    __SYSTEM_UART4_CLK_ENABLE();
    Uart4_handle.UARTx = UART4;
    Uart4_handle.Init.BaudRate   = 115200;
    Uart4_handle.Init.DataLength = UART_DATA_LENGTH_8BIT;
    Uart4_handle.Init.StopBits   = UART_STOPBITS_1; 
    Uart4_handle.Init.Parity     = UART_PARITY_NONE;
    Uart4_handle.Init.FIFO_Mode  = UART_FIFO_ENABLE;
    Uart4_handle.TxCpltCallback  = NULL;   
    Uart4_handle.RxCpltCallback  = imu_to_rx_done;        //接收来自IMU数据回调
    uart_init(&Uart4_handle);
    NVIC_EnableIRQ(UART4_IRQn);                     //使能中断
    NVIC_SetPriority(UART4_IRQn, 4);
    //暂不开启接收中断
    Uart_Recv_IT(&Uart4_handle);


    /*----------------------------------------------------UART2 INIT-------------------------------------------------------------------------------*/
    /*UART2 :used for connect to um980(UART3) --PB0 PB1*/
    __SYSTEM_GPIOB_CLK_ENABLE();
    __SYSTEM_GPIOD_CLK_ENABLE();    //GNSS 使能引脚需要 GPIOD 时钟
    //configure PB0 and PB1 to UART2 function
    gpio_config.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio_config.Mode = GPIO_MODE_AF_PP;
    gpio_config.Pull = GPIO_PULLUP;
    gpio_config.Alternate = GPIO_FUNCTION_1;
    gpio_init(GPIOB, &gpio_config);
    //UART2 configure
    __SYSTEM_UART2_CLK_ENABLE();
    Uart2_handle.UARTx = UART2;
    Uart2_handle.Init.BaudRate   = 921600;
    Uart2_handle.Init.DataLength = UART_DATA_LENGTH_8BIT;
    Uart2_handle.Init.StopBits   = UART_STOPBITS_1; 
    Uart2_handle.Init.Parity     = UART_PARITY_NONE;
    Uart2_handle.Init.FIFO_Mode  = UART_FIFO_ENABLE;
    Uart2_handle.TxCpltCallback  = NULL;   
    Uart2_handle.RxCpltCallback  = gnss_uart3_to_rx_done;        //接收来自GNSS数据回调
    uart_init(&Uart2_handle);
    NVIC_EnableIRQ(UART2_IRQn);                     //使能中断
    NVIC_SetPriority(UART2_IRQn, 4);
    Uart_Recv_IT(&Uart2_handle); //uart_receive_IT
    /*-------------------------------------------------UART1 INIT-----------------------------------------------------------------------------*/
    __SYSTEM_GPIOA_CLK_ENABLE();
    __SYSTEM_GPIOD_CLK_ENABLE();    //GNSS 使能引脚需要 GPIOD 时钟
    //configure PA13 and PA12 to UART1 function
    gpio_config.Pin = GPIO_PIN_12 | GPIO_PIN_13;
    gpio_config.Mode = GPIO_MODE_AF_PP;
    gpio_config.Pull = GPIO_PULLUP;
    gpio_config.Alternate = GPIO_FUNCTION_1;
    gpio_init(GPIOA, &gpio_config);
    //将GNSS_PWR_EN 拉高使能 PD2;
    gpio_config.Pin = GPIO_PIN_2;
    gpio_config.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_config.Pull = GPIO_NOPULL;
    gpio_init(GPIOD, &gpio_config);
    gpio_write_pin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
    //UART1 configure
    __SYSTEM_UART1_CLK_ENABLE();
    Uart1_handle.UARTx = UART1;
    Uart1_handle.Init.BaudRate   = 921600;
    Uart1_handle.Init.DataLength = UART_DATA_LENGTH_8BIT;
    Uart1_handle.Init.StopBits   = UART_STOPBITS_1; 
    Uart1_handle.Init.Parity     = UART_PARITY_NONE;
    Uart1_handle.Init.FIFO_Mode  = UART_FIFO_ENABLE;
    Uart1_handle.TxCpltCallback  = NULL;   
    Uart1_handle.RxCpltCallback  = gnss_uart2_to_rx_done;        //接收来自GNSS数据回调
    uart_init(&Uart1_handle);
    NVIC_EnableIRQ(UART1_IRQn);                     //使能中断
    NVIC_SetPriority(UART1_IRQn, 4);
    Uart_Recv_IT(&Uart1_handle); //uart_receive_IT


    //printf打印互斥锁初始化 
    uart_printf_mutex_init();
    /* do calibration, get current RC frequency */
    __SYSTEM_CALI_CLK_ENABLE();
    cali_handle.mode = CALI_UP_MODE_NORMAL;
    cali_handle.rc_cnt = 200;
    cali_handle.DoneCallback = cali_done_handle;
    cali_init(&cali_handle);
    cali_start_IT(&cali_handle);
    system_prevent_sleep_set(SYSTEM_PREVENT_SLEEP_TYPE_CALIBRATION);
    NVIC_SetPriority(CALI_IRQn, 4);
    NVIC_EnableIRQ(CALI_IRQn);
    
    /* init flashdb to store user data */
    flashdb_init();

    /* get random seed*/
    uint32_t rand_num;    
    size_t size = flashdb_get(FDB_KEY_RANDOM_SEED, (void *)&rand_num, 4);
    printf("flashdb get random seed :%d\r\n",size);
    if(size == 0)
    {
        __SYSTEM_TRNG_CLK_ENABLE();
        trng_init();
        trng_read_rand_num((uint8_t *)&rand_num,4);
        flashdb_set(FDB_KEY_RANDOM_SEED,(uint8_t *)&rand_num,4);
        __SYSTEM_TRNG_CLK_DISABLE();
    }
    srand(rand_num);
    printf("flash db get rand num: %x\r\n",rand_num);
    /* Create tasks */
#if ENABLE_RTOS_MONITOR == 1
    xTaskCreate(monitor_task, "monitor", MONITOR_TASK_STACK_SIZE, NULL, MONITOR_TASK_PRIORITY, &monitor_task_handle);
#endif

    /* create application task */
    rx_to_gnss_uart3_task_init(&Uart2_handle);
    rx_to_gnss_uart2_task_init(&Uart1_handle);
    rx_to_imu_task_init(&Uart4_handle);
    gnss_uart2_to_rx_task_init();
    gnss_uart3_to_rx_task_init();
    imu_to_rx_task_init();
    //audio_scene_init(AUDIO_SCENE_TASK_STACK_SIZE, AUDIO_SCENE_TASK_PRIORITY);


    /* Keep the system awake while diagnosing UART3 RX. */
    system_prevent_sleep_set(SYSTEM_PREVENT_SLEEP_TYPE_DISABLE);

    printf("FR509C: BTDM test.\r\n");

    /* Start the scheduler itself. */
    vTaskStartScheduler();
    /*
        启动FreeRtos调度器  创建idle空闲任务，在没有其他任务运行时，空闲任务会运行，空闲任务的优先级最低，idle任务会调用vApplicationIdleHook()函数
    */

    return 0;
}

void uart1_irq(void)
{
    uart_IRQHandler(&Uart1_handle);
}
void uart2_irq(void)
{
    uart_IRQHandler(&Uart2_handle);
}
void uart3_irq(void)
{
    uart_IRQHandler(&Uart3_handle);
}
void uart4_irq(void)
{
    uart_IRQHandler(&Uart4_handle);
}


void PMU_GPIO_PMU_IRQHandler(void)
{
    uint16_t data = ool_read16(PMU_REG_PIN_DATA);
    uint16_t result = ool_read16(PMU_REG_PIN_XOR_RESULT);
    
    /* update last value with latest data */
    ool_write16(PMU_REG_PIN_LAST_V, data);
    /* clear last XOR result */
    ool_write16(PMU_REG_PIN_XOR_CLR, result);
    
    if (data & PMU_PIN_9) {
        system_prevent_sleep_clear(SYSTEM_PREVENT_SLEEP_TYPE_HCI_RX);
    }
    else {
        system_prevent_sleep_set(SYSTEM_PREVENT_SLEEP_TYPE_HCI_RX);
    }
}

void cali_irq(void)
{
    cali_IRQHandler(&cali_handle);
}

