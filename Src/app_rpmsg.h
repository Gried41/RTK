#ifndef _APP_RPMSG_H
#define _APP_RPMSG_H

/************************************************************************************
 * @fn      app_rpmsg_init
 *
 * @brief   A task will be created, At the beginning of this task, it will load DSP code,
 *          boot DSP, and establish RPMSG communication channel with DSP, then it will
 *          take charge of handling incomming DSP messages.
 */
void app_rpmsg_init(void);

/************************************************************************************
 * @fn      app_rpmsg_ready
 *
 * @brief   This is a weak function, it will be called after rpmsg is initialized. User
 *          should implement this function to handle follow processing. DSP service is
 *          ready after calling this function.
 */
void app_rpmsg_ready(void);

/************************************************************************************
 * @fn      app_rpmsg_recover
 *
 * @brief   When system enter deep sleep mode, there are two ways to handle DSP state:
 *          1. SHUT DOWM mode: Just enter deep sleep mode with no action for DSP. All 
 *             state of DSP will lost in this mode. User should call this function to 
 *             recover DSP after wake up. DSP will stay in initial state after calling
 *             this function.
 *          2. DEEP SLEEP mode: Use app_rpmsg_suspend to save DSP state before enter
 *             deep sleep mode or put DSP into IDLE state. When system wakeup from sleep 
 *             or need recover DSP into work mode, user should call app_rpmsg_resume to 
 *             resume all stateof DSP. DSP will resume to previous state with nothing
 *             changed.
 */
void app_rpmsg_recover(void);

#endif  // _APP_RPMSG_H
