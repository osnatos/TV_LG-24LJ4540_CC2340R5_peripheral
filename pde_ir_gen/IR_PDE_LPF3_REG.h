/*
 * IR_PDE_LPF3_REG.h
 *
 *  Created on: Apr 12, 2024
 *      Author: a0232184
 */

#ifndef PDE_IR_GEN_IR_PDE_LPF3_REG_H_
#define PDE_IR_GEN_IR_PDE_LPF3_REG_H_

#include <stdint.h>
#include <ti/devices/cc23x0r5/inc/hw_memmap.h>
#include <ti/devices/cc23x0r5/inc/hw_evtsvt.h>
#include <ti/devices/cc23x0r5/inc/hw_lgpt.h>
#include <ti/devices/cc23x0r5/inc/hw_clkctl.h>
#include <ti/devices/cc23x0r5/inc/hw_ioc.h>
#include <ti/devices/cc23x0r5/driverlib/interrupt.h>
#include <ti/drivers/timer/LGPTimerLPF3.h>

#define PDEIRLPF3_CONFIG_0   (0)

#define PDEIRLPF3_CONFIG_QTY  (1)
uint32_t leadMarkDuration;
uint32_t leadSpaceDuration;

/*
 * These are based on CC2340R5 TRM section "10.4.3 IR Generation"
 */
#define LGPTLPF3_CARRIER_BASE    (LGPT0_BASE)
#define LGPTLPF3_MODULATOR_BASE  (LGPT1_BASE)
#define LGPTLPF3_CHANNEL         (0) // always use channel 0 for given timer


typedef struct
{
    uint32_t    carrierFreq_kHz; // maybe assume this is constant for now
    uint32_t    frameInterval_usec;
} PDEIRLPF3_Params;

typedef struct
{
    uint8_t customCode;
    uint8_t dataCode;
} PDEIRGENLPF3_Transaction;

typedef enum
{
    PDEIRLPF3_STATE_IDLE,
    PDEIRLPF3_STATE_STARTING,
    PDEIRLPF3_STATE_LEAD_CODE,
    PDEIRLPF3_STATE_CUST_CODE_1,
    PDEIRLPF3_STATE_CUST_CODE_2_N,
    PDEIRLPF3_STATE_DATA_CODE_1,
    PDEIRLPF3_STATE_DATA_CODE_2,
    PDEIRLPF3_STATE_STOP_BIT,
    PDEIRLPF3_STATE_REPEAT_CODE_1,
    PDEIRLPF3_STATE_REPEAT_CODE_2,
    PDEIRLPF3_STATE_LAST_EXPANDED, // Natan
    PDEIRLPF3_STATE_STOP,  // Natan
    PDEIRLPF3_STATE_CS  // Natan
} PDEIRLPF3_State;

typedef struct
{
    uint8_t     isOpen;
    uint32_t    bitIndex;
    uint32_t    countPassed;
    uint32_t    frameInterval_tick;

    uint8_t LgptCarrier_index;
    uint8_t LgptModulator_index;

    LGPTimerLPF3_Handle LgptCarrierHandle;
    LGPTimerLPF3_Handle LgptModulatorHandle;

    PDEIRLPF3_State     state;
} PDEIRLPF3_Object;

typedef struct
{
    uint32_t LgptModulatorBaseAddr;
    uint32_t LgptCarrierBaseAddr;

    uint32_t LgptModulatorInts;
} PDEIRLPF3_HWAttrs;

typedef struct
{
    PDEIRLPF3_Object    *object;
    const PDEIRLPF3_HWAttrs   *hwAttrs;
} PDEIRLPF3_Config;

typedef enum
{
    PDEIRLPF3_STATUS_OK = 0,
    PDEIRLPF3_STATUS_BAD_STATE,
    PDEIRLPF3_STATUS_INVALID_PAR,
    PDEIRLPF3_STATUS_BUSY,
} PDEIRLPF3_Status;

typedef PDEIRLPF3_Config *PDEIRLPF3_Handle;

extern void PDEIRLPF3_init();
extern void PDEIRLPF3_Params_init(PDEIRLPF3_Params *pParams);
extern PDEIRLPF3_Handle PDEIRLPF3_open(uint8_t index, PDEIRLPF3_Params *pParams);
extern PDEIRLPF3_Status PDEIRLPF3_transfer(PDEIRLPF3_Handle handle, PDEIRGENLPF3_Transaction *transaction);
extern PDEIRLPF3_Status PDEIRLPF3_stop(PDEIRLPF3_Handle handle);
extern void PDEIRLPF3_close(PDEIRLPF3_Handle handle);
extern volatile bool g_ir_disconnect_on_stop;


#endif /* PDE_IR_GEN_IR_PDE_LPF3_REG_H_ */
