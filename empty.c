/*
 * Copyright (c) 2015-2019, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== empty.c ========
 */
//---------------------------------------------------------------------------
/* POSIX Header files */
#include <pthread.h>

/* RTOS header files */
#include <FreeRTOS.h>
#include <task.h>

 /* Stack size in bytes */
  #define THREADSTACKSIZE 1024
//---------------------------------------------------------------------------
/* For usleep() */
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include "pde_ir_gen/LGPTTimerLPF3_ext.h"

/* Driver configuration */
#include <pde_ir_gen/IR_PDE_LPF3_REG.h>

#include "ti_drivers_config.h"
#include <ti/drivers/timer/LGPTimerLPF3.h>

#include <ti/devices/cc23x0r5/inc/hw_evtsvt.h>
#include <ti/devices/cc23x0r5/inc/hw_lgpt.h>
#include <ti/devices/cc23x0r5/inc/hw_clkctl.h>
#include <ti/devices/cc23x0r5/inc/hw_ioc.h>
#include <ti/devices/cc23x0r5/driverlib/interrupt.h>
#include <ti/devices/cc23x0r5/inc/hw_fcfg.h>

static PDEIRLPF3_Handle pdeIrHandle = 0;

void IR_Init();
void dataInit();
#define DATA_LEN 18
#define DC2DC_CONTROL true
uint8_t data[DATA_LEN];
uint8_t bleAddr[6];
PDEIRLPF3_Handle gIrHandle = NULL;
void set_DC2DC_ENA(bool enable);
bool inversionFlag = true;
uint8_t DECODER_ADR = 0xFE;
#define DECODER_ADDRESS 0x30
//-----------------------------------------------------------------------------
//void LGPTimerLPF3_PTGT(LGPTimerLPF3_Handle handle, uint16_t value);
//-----------------------------------------------------------------------------
/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    /* 1 second delay */
    uint32_t time = 1;

    /* Call driver init functions */
    GPIO_init();
    // I2C_init();
    // SPI_init();
    // Watchdog_init();

    /* Configure the LED pin */
   // GPIO_setConfig(CONFIG_GPIO_LED_RED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    /* Turn on user LED */
//    GPIO_write(CONFIG_GPIO_LED_RED, CONFIG_GPIO_LED_ON);

    /* DC2DC_CONTROL */
    if(DC2DC_CONTROL) {
       GPIO_setConfig(CONFIG_GPIO_DC2DC_ENA, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    }

    //-------------------------------------------------------------------------
    while (1)
    {
        sleep(time);
    }extern void PDEIRLPF3_init(void);
}
//=============================================================================

extern void PDEIRLPF3_init(void);

void emptyMain(void)
 {
    PDEIRLPF3_init();

     pthread_t thread;
     pthread_attr_t attrs;
     struct sched_param priParam;
     int retc;

     /* Initialize the attributes structure with default values */
     pthread_attr_init(&attrs);

     /* Set priority, detach state, and stack size attributes */
     priParam.sched_priority = 5; // Lower the priority of this task
     retc = pthread_attr_setschedparam(&attrs, &priParam);
     retc |= pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
     retc |= pthread_attr_setstacksize(&attrs, THREADSTACKSIZE);
     if (retc != 0)
     {
         /* failed to set attributes */
         while (1) {}
     }

     retc = pthread_create(&thread, &attrs, mainThread, NULL);


     if (retc != 0)
     {
         /* pthread_create() failed */
         while (1) {}
     }
     // Read in bleAddr MAC address of CC2340R5
       memcpy(bleAddr, fcfg->deviceInfo.bleAddr, 6);
 }
//=============================================================================
void IR_Init() {
    if (pdeIrHandle != 0) {
        return;
    }
    else {
        PDEIRLPF3_init();

        // for now, only need to use the default parameters
        PDEIRLPF3_Params  params;
        PDEIRLPF3_Params_init(&params);

        pdeIrHandle = PDEIRLPF3_open(PDEIRLPF3_CONFIG_0, &params);
        if (pdeIrHandle == 0)
            while (1); // error...

    }
}
/*=============================================================================
 * @fn      BLEConnectionEstablished
 *
 * @brief   Called when a Bluetooth connection has been established
 *          with a peer device.
 *
 * @param   None
 *
 * @return  None.
 */
void BLEConnectionEstablished(void)
{
  // Indicate connection by turning on the green LED
  //GPIO_write(CONFIG_GPIO_LED_GREEN, CONFIG_LED_ON);
}
/*********************************************************************
 * @fn      BLEConnectionTerminated
 *
 * @brief   Called when the Bluetooth connection has been terminated.
 *
 * @param   None
 *
 * @return  None.
 */
void BLEConnectionTerminated(void)
{
  // Indicate disconnection by turning off the LED
  //GPIO_write(CONFIG_GPIO_LED_GREEN, CONFIG_LED_OFF);
}
//=============================================================================
/*
 * @fn      evaluateNewCharacteristicValue
 *
 * @brief   Based on the new value of a given characteristic determine
 *          if the LED should be turned off or on.
 *
 * @param   newValue: Value of the characteristic to consider
 *
 * @return  None.
 */
void evaluateNewCharacteristicValue(uint8_t newValue)
{
// If the new value of the characteristic is 0, then we turn off the red LED
if(newValue == 0)
{
  //  GPIO_write(CONFIG_GPIO_LED_RED, CONFIG_LED_OFF);
     // GPIO_write(CONFIG_GPIO_LED_GREEN, CONFIG_LED_OFF);
}
else
{
  //  GPIO_write(CONFIG_GPIO_LED_RED, CONFIG_LED_ON);
    //  GPIO_write(CONFIG_GPIO_LED_GREEN, CONFIG_LED_ON);
   //   test_pdeir();
//      sendCommand();
//      send_BelcorCommand();

}
}

//=============================================================================
void send_TV_LG_24LJ4540_Command(uint8_t customCode, uint8_t dataCode) {
    //-------------------------------------------------------------------------
    IR_Init();
    //-------------------------------------------------------------------------
//    void setAdvInterval_Fast(void);
//    setAdvInterval_Fast();
    set_DC2DC_ENA(true);
    PDEIRGENLPF3_Transaction pdeIrTrans;
    if(customCode == DECODER_ADR) {
        pdeIrTrans.customCode = DECODER_ADDRESS;
        inversionFlag = false;
    }
    else {
        pdeIrTrans.customCode = customCode;
        inversionFlag = true;
    }
    pdeIrTrans.dataCode = dataCode;

    PDEIRLPF3_transfer(pdeIrHandle, &pdeIrTrans);
}
//=============================================================================
/* DC2DC_CONTROL */
void set_DC2DC_ENA(bool enable) {
 if(DC2DC_CONTROL) {
   if(enable) {
       GPIO_setConfig(CONFIG_GPIO_DC2DC_ENA, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_HIGH);
   } else
   GPIO_setConfig(CONFIG_GPIO_DC2DC_ENA, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
 }
}
//=============================================================================


