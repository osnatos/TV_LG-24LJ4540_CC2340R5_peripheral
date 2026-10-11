/******************************************************************************

@file  app_peripheral.c

@brief This example file demonstrates how to activate the peripheral role with
the help of BLEAppUtil APIs.

Two structures are used for event handling, one for connection events and one
for advertising events.
In each, eventMask is used to specify the events that will be received
and handled.
In addition, fill the BLEAppUtil_AdvInit_t structure with variables generated
by the Sysconfig.

In the events handler functions, write what actions are done after each event.
In this example, after a connection is made, activation is performed for
re-advertising up to the maximum connections.

In the Peripheral_start() function at the bottom of the file, registration,
initialization and activation are done using the BLEAppUtil API functions,
using the structures defined in the file.

More details on the functions and structures can be seen next to the usage.

Group: WCS, BTS
Target Device: cc23xx

******************************************************************************

 Copyright (c) 2022-2026, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

******************************************************************************


*****************************************************************************/

#if defined( HOST_CONFIG ) && ( HOST_CONFIG & ( PERIPHERAL_CFG ) )

//*****************************************************************************
//! Includes
//*****************************************************************************
#include "ti_ble_config.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/menu/menu_module.h"
#include <app_main.h>
#include <ti/drivers/dpl/ClockP.h>

//*****************************************************************************
//! Prototypes
//*****************************************************************************
void Peripheral_AdvEventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData);
void Peripheral_GAPConnEventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData);
//void RefreshProtectiveTimer(uint32_t ticks);
void protectiveTimeoutCallback(uintptr_t arg);
void initProtectiveTimer(void);
void RefreshProtectiveTimer_Sec(uint32_t timeoutSec);
void setAdvInterval_Slow(char *pData);
void changeAdvIntervalRuntime(uint32_t intervalMs);
void setAdvSlow(char *pData);
//*****************************************************************************
//! Globals
//*****************************************************************************
ClockP_Struct protectiveClock;
//static ClockP_Struct protectiveClock;
ClockP_Struct myClockStruct;
ClockP_Handle myClockHandle;
/*
typedef enum {
    TIMEOUT_ACTION_NOTHING,
    TIMEOUT_ACTION_TURN_ADV_FAST,
    TIMEOUT_ACTION_TURN_ADV_SLOW
} TimerAction_t;
*/
// #define FAST_ADV_TIME_OUT_mSec  60000   //  1 minute
#define FAST_ADV_TIME_OUT_mSec  180000  //  3 minute
//#define FAST_ADV_INTERVAL_mSec  210   // 210 msec
#define FAST_ADV_INTERVAL_mSec  100     // 100 msec
#define FAST_ADV_INTERVAL_Tick  160     // 100 msec
//#define FAST_ADV_INTERVAL_Tick  336   // 210 msec
#define SLOW_ADV_INTERVAL_mSec 2000     // 2 sec
#define TIMEOUT_PROTECTION_SEC 12000    //  2 minute

BLEAppUtil_EventHandler_t peripheralConnHandler =
{
    .handlerType    = BLEAPPUTIL_GAP_CONN_TYPE,
    .pEventHandler  = Peripheral_GAPConnEventHandler,
    .eventMask      = BLEAPPUTIL_LINK_ESTABLISHED_EVENT |
                      BLEAPPUTIL_LINK_PARAM_UPDATE_REQ_EVENT |
                      BLEAPPUTIL_LINK_TERMINATED_EVENT,
};

BLEAppUtil_EventHandler_t peripheralAdvHandler =
{
    .handlerType    = BLEAPPUTIL_GAP_ADV_TYPE,
    .pEventHandler  = Peripheral_AdvEventHandler,
    .eventMask      = BLEAPPUTIL_ADV_START_AFTER_ENABLE |
                      BLEAPPUTIL_ADV_END_AFTER_DISABLE
};

//! Stores adv handles
uint8_t peripheralAdvHandles[BLE_CONFIG_NUM_ADV_SETS];
volatile static bool Adv_slow_flag = false;
//volatile bool Adv_fast_flag = false;
volatile bool test_flag;
volatile uint32_t adv_interval;
bool stop_flag = false;
//*****************************************************************************
//! Functions
//*****************************************************************************

/*********************************************************************
 * @fn      Peripheral_AdvEventHandler
 *
 * @brief   The purpose of this function is to handle advertise events
 *          that rise from the GAP and were registered in
 *          @ref BLEAppUtil_registerEventHandler
 *
 * @param   event - message event.
 * @param   pMsgData - pointer to message data.
 *
 * @return  none
 */
void Peripheral_AdvEventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{

 //   BLEAppUtil_AdvEventData_t *advData = (BLEAppUtil_AdvEventData_t *)pMsgData;
 //   uint8_t adv_Handle = advData->pBuf->advHandle;
//    bStatus_t status;



    switch(event)
    {
        case BLEAPPUTIL_ADV_START_AFTER_ENABLE:
        {
 //           test_flag = false;
 /*           if(stop_flag) {
                stop_flag = false;
            } else
            {
            changeAdvIntervalRuntime(FAST_ADV_INTERVAL_mSec);
            stop_flag = true;
            }
 */
            break;
        }

        case BLEAPPUTIL_ADV_END_AFTER_DISABLE:
        {
/*
               if(Adv_slow_flag == false)  // 210ms fast interval
               {
                   adv_interval = 336; // 210ms fast interval
                   // Refresh the protective fuse timer
 //                   RefreshProtectiveTimer(FAST_ADV_TIME_OUT_mSec);
               }
               else // 2s slow interval
               {
                   Adv_slow_flag = false;
                   adv_interval =  3200; // 2s slow interval
                   // RefreshProtectiveTimer  TODO
               }


               test_flag = true;

  //             volatile uint32_t adv_interval = 336;  // 210ms fast interval

               GapAdv_setParam(peripheralAdvHandles[0], GAP_ADV_PARAM_PRIMARY_INTERVAL_MIN, &adv_interval);
               GapAdv_setParam(peripheralAdvHandles[0], GAP_ADV_PARAM_PRIMARY_INTERVAL_MAX, &adv_interval);





          //     status = GapAdv_setParam(adv_Handle, GAP_ADV_PARAM_PRIMARY_INTERVAL_MIN, &interval);
         //      if (status != SUCCESS) { __builtin_debugtrap(); }

         //      status = GapAdv_setParam(adv_Handle, GAP_ADV_PARAM_PRIMARY_INTERVAL_MAX, &interval);
        //       if (status != SUCCESS) { __builtin_debugtrap(); }



               // Refresh the protective fuse timer
  //             RefreshProtectiveTimer(FAST_ADV_TIME_OUT_mSec);
*/
             break;
        }

        default:
        {
            break;
        }
    }
}

/*********************************************************************
 * @fn      Peripheral_GAPConnEventHandler
 *
 * @brief   The purpose of this function is to handle connection related
 *          events that rise from the GAP and were registered in
 *          @ref BLEAppUtil_registerEventHandler
 *
 * @param   event - message event.
 * @param   pMsgData - pointer to message data.
 *
 * @return  none
 */
void Peripheral_GAPConnEventHandler(uint32 event, BLEAppUtil_msgHdr_t *pMsgData)
{
    switch(event)
    {
        case BLEAPPUTIL_LINK_ESTABLISHED_EVENT:
        {
            /* Check if we reach the maximum allowed number of connections */
             if(linkDB_NumActive() < linkDB_NumConns())
             {
                 /* Start advertising since there is room for more connections */
                 BleConfig_startAdvSets(peripheralAdvHandles, NULL, BLE_CONFIG_NUM_ADV_SETS);
             }
             else
             {
                 /* Stop advertising since there is no room for more connections */
                 BleConfig_stopAdvSets(peripheralAdvHandles, NULL, BLE_CONFIG_NUM_ADV_SETS);
             }

             RefreshProtectiveTimer_Sec(TIMEOUT_PROTECTION_SEC); // The timer will go off in 2 min.

             break;
        }

        case BLEAPPUTIL_LINK_TERMINATED_EVENT:
        {
 //                BleConfig_startAdvSets(peripheralAdvHandles, NULL, BLE_CONFIG_NUM_ADV_SETS);

  //               status = BLEAppUtil_advStart(peripheralAdvHandles, &advSetStartParams);


                 uint32_t advIntervalUnits;
                 if(Adv_slow_flag) {
                     advIntervalUnits = 3200; // 2000 ms
                     Adv_slow_flag = false;
                 }
                 else {
                  //   advIntervalUnits = 336; // 210 ms
                     advIntervalUnits = FAST_ADV_INTERVAL_Tick; // 336; // 210 ms

                  }

                 // Update minimum and maximum primary advertising intervals
                  GapAdv_setParam(peripheralAdvHandles[0], GAP_ADV_PARAM_PRIMARY_INTERVAL_MIN, &advIntervalUnits);
                  GapAdv_setParam(peripheralAdvHandles[0], GAP_ADV_PARAM_PRIMARY_INTERVAL_MAX, &advIntervalUnits);

                  BLEAppUtil_AdvStart_t advSetStartParams = {
                          .enableOptions       = GAP_ADV_ENABLE_OPTIONS_USE_MAX,
                          .durationOrMaxEvents = 0
                  };

                  // Restart advertising using the framework
 //                  BLEAppUtil_advStart(peripheralAdvHandles[0], &advSetStartParams);
                   BleConfig_startAdvSets(peripheralAdvHandles, NULL, BLE_CONFIG_NUM_ADV_SETS);







             break;
        }

        default:
        {
            break;
        }
    }
}

/*********************************************************************
 * @fn      Peripheral_start
 *
 * @brief   This function is called after stack initialization,
 *          the purpose of this function is to initialize and
 *          register the specific events handlers of the peripheral
 *          application module
 *
 * @return  SUCCESS, errorInfo
 */
bStatus_t Peripheral_start()
{

 //   Power_releaseConstraint(PowerLPF3_DISALLOW_STANDBY);  // Natan

    bStatus_t status = SUCCESS;

    status = BLEAppUtil_registerEventHandler(&peripheralConnHandler);
    if(status != SUCCESS)
    {
        // Return status value
        return(status);
    }

    status = BLEAppUtil_registerEventHandler(&peripheralAdvHandler);
    if(status != SUCCESS)
    {
        return(status);
    }

    // Initiate the advertise sets
    uint8_t peripheralAdvHandlesStatuses[BLE_CONFIG_NUM_ADV_SETS];
    BleConfig_initAdvSets(peripheralAdvHandles, peripheralAdvHandlesStatuses);
    for (int i = 0; i < BLE_CONFIG_NUM_ADV_SETS; i++)
    {
        // Return the first FAILURE status value
        if (peripheralAdvHandlesStatuses[i] != SUCCESS)
        {
            return(peripheralAdvHandlesStatuses[i]);
        }
    }

    BleConfig_startAdvSets(peripheralAdvHandles, NULL, BLE_CONFIG_NUM_ADV_SETS);

    // Disable the low-frequency clock loss monitoring module using hardware,
    // by writing 0 to the direct physical address of the LFMONITOR register on the CC2340R5 chip
//    *(volatile uint32_t *)(0x4000E000 + 0x18) = 0x00000000;

//    Power_releaseConstraint(PowerLPF3_DISALLOW_STANDBY);

    initProtectiveTimer();

    // Return status value
    return(status);
}

//=============================================================================
void protectiveTimeoutCallback(uintptr_t arg)
{

   BLEAppUtil_invokeFunction(setAdvSlow, NULL);

}
//-----------------------------------------------------------------------------
void RefreshProtectiveTimer_Ms(uint32_t timeoutMs)
{
    uint32_t ticks = timeoutMs * 1000U; // 1 tick = 1 microsecond,

    ClockP_stop(myClockHandle);
    ClockP_setTimeout(myClockHandle, ticks);
    ClockP_start(myClockHandle);
}
//-----------------------------------------------------------------------------
void RefreshProtectiveTimer_Sec(uint32_t timeoutSec)
{
    uint32_t ticks = timeoutSec * 1000000U; // 1 tick = 1 microsecond,

    ClockP_stop(myClockHandle);
    ClockP_setTimeout(myClockHandle, ticks);
    ClockP_start(myClockHandle);
}
//-----------------------------------------------------------------------------

void initProtectiveTimer(void)
{
    ClockP_Params params;
    ClockP_Params_init(&params);
    params.period = 0;        // One-shot
    params.startFlag = false; // Let's not start right away
    params.arg = (uintptr_t)NULL;

    myClockHandle = ClockP_construct(&myClockStruct,
                            (ClockP_Fxn)protectiveTimeoutCallback, 0, &params);
}

//=============================================================================
/* Main execution function running in the safe application thread context */
void setAdvInterval_Slow(char *pData)
{
    uint32_t interval = 3200; /* 2 seconds */
    bStatus_t status;

    // 1. Stop the advertising set using explicit index [0]
/*    status = BLEAppUtil_advStop(peripheralAdvHandles[0]);
    if (status != SUCCESS)
    {
        __builtin_debugtrap();
    }
*/
    /* 2. Overwrite primary advertising interval parameters in stack memory */
    status = GapAdv_setParam(peripheralAdvHandles[0], GAP_ADV_PARAM_PRIMARY_INTERVAL_MIN, &interval);
    if (status != SUCCESS) { __builtin_debugtrap(); }

    status = GapAdv_setParam(peripheralAdvHandles[0], GAP_ADV_PARAM_PRIMARY_INTERVAL_MAX, &interval);
    if (status != SUCCESS) { __builtin_debugtrap(); }
/*
    BLEAppUtil_AdvStart_t myStartParams;
    myStartParams.enableOptions   = GAP_ADV_ENABLE_OPTIONS_USE_MAX;
    myStartParams.durationOrMaxEvents = 0; // Endless advertising

    // 3. Restart advertising with the 2 required arguments and index [0]
    status = BLEAppUtil_advStart(peripheralAdvHandles[0], &myStartParams);
    if (status != SUCCESS)
    {
        __builtin_debugtrap();   }
        */
}

void setAdvIntervalTicks(uint32_t ticks)
{
 //   uint32_t interval = mSec * 1.6;
//    bStatus_t status;

    uint32_t interval =  ticks;

    GapAdv_setParam(peripheralAdvHandles[0], GAP_ADV_PARAM_PRIMARY_INTERVAL_MIN, &interval);
    GapAdv_setParam(peripheralAdvHandles[0], GAP_ADV_PARAM_PRIMARY_INTERVAL_MAX, &interval);
}

void changeAdvIntervalRuntime(uint32_t intervalMs)
{
    bStatus_t status;

    // Convert milliseconds to BLE advertisement units (1 unit = 0.625 ms)
    uint32_t advIntervalUnits = (intervalMs * 8) / 5;

    // Stop advertising through the F3 BLEAppUtil framework
    status = BLEAppUtil_advStop(peripheralAdvHandles[0]);

    if (status == SUCCESS)
    {
        // Update minimum and maximum primary advertising intervals
        GapAdv_setParam(peripheralAdvHandles[0], GAP_ADV_PARAM_PRIMARY_INTERVAL_MIN, &advIntervalUnits);
        GapAdv_setParam(peripheralAdvHandles[0], GAP_ADV_PARAM_PRIMARY_INTERVAL_MAX, &advIntervalUnits);

        BLEAppUtil_AdvStart_t advSetStartParams = {
                .enableOptions       = GAP_ADV_ENABLE_OPTIONS_USE_MAX,
                .durationOrMaxEvents = 0
        };

        // Restart advertising using the framework
         BLEAppUtil_advStart(peripheralAdvHandles[0], &advSetStartParams);

    }
}


void setAdvSlow(char *pData)
{
    bStatus_t status;

    // Convert milliseconds to BLE advertisement units (1 unit = 0.625 ms)
//    uint32_t advIntervalUnits = (intervalMs * 8) / 5;
    uint32_t advIntervalUnits = 3200; // 2sec


    // Stop advertising through the F3 BLEAppUtil framework
    status = BLEAppUtil_advStop(peripheralAdvHandles[0]);

    if (status == SUCCESS)
    {
        // Update minimum and maximum primary advertising intervals
        GapAdv_setParam(peripheralAdvHandles[0], GAP_ADV_PARAM_PRIMARY_INTERVAL_MIN, &advIntervalUnits);
        GapAdv_setParam(peripheralAdvHandles[0], GAP_ADV_PARAM_PRIMARY_INTERVAL_MAX, &advIntervalUnits);

        BLEAppUtil_AdvStart_t advSetStartParams = {
                .enableOptions       = GAP_ADV_ENABLE_OPTIONS_USE_MAX,
                .durationOrMaxEvents = 0
        };

        // Restart advertising using the framework
         BLEAppUtil_advStart(peripheralAdvHandles[0], &advSetStartParams);

    }
}


void setAdvSlowFlag(bool flag){
    Adv_slow_flag = flag;
}

#endif // ( HOST_CONFIG & ( PERIPHERAL_CFG ) )
