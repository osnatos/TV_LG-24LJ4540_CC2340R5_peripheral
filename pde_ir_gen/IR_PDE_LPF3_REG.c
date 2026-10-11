/*
 * IR_PDE_LPF3_REG.c
 *
 *  Created on: Apr 12, 2024
 *      Author: a0232184
 */


#include <pde_ir_gen/IR_PDE_LPF3_REG.h>
#include <pde_ir_gen/LGPTTimerLPF3_ext.h>
#include <ti/drivers/timer/LGPTimerLPF3.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/GPIO.h>
#include <ti/devices/cc23x0r5/inc/hw_memmap.h>
#include <ti/devices/cc23x0r5/inc/hw_ints.h>
#include <ti/devices/cc23x0r5/inc/hw_types.h>
#include <ti/devices/cc23x0r5/inc/hw_lgpt.h>
#include <ti/devices/cc23x0r5/inc/hw_evtsvt.h>

//-----------------------------------------------------------------------------
//#define BIT_ACTIVE_CNT 15   // 396
#define BIT_ACTIVE_CNT 16     // 420-424
#define BIT0_FRAME_CNT 30     // 816-817
//#define BIT0_FRAME_CNT 29   // 790
//#define BIT1_FRAME_CNT 50   // 1342
//#define BIT1_FRAME_CNT 51   // 1370
#define BIT1_FRAME_CNT 52     // 1394

void set_DC2DC_ENA(bool enable);
volatile uint8_t RepeatCounter=0;  // Natan
volatile uint16_t ConnHandle;      // Natan
//-----------------------------------------------------------------------------
// for debug purposes
#define STATIC

#define CARRIER_FREQ_kHz    (38)
//#define TICK_FROM_USEC(usec) ((usec)*CARRIER_FREQ_kHz/1000)
#define TICK_FROM_USEC(usec) ((10*(usec)*CARRIER_FREQ_kHz+5000)/10000)

// this is for offset due to overhead from interrupt handling
#define CAL_OFFSET_USEC (31)
#define CAL_STATIC_OFFSET_USEC (31)
#define CAL_OFFSET_TICK (TICK_FROM_USEC(CAL_OFFSET_USEC))
//#define FRAME_CNT_CAL(frameCount) (frameCount - CAL_OFFSET_TICK)
#define FRAME_CNT_CAL(frameCount) (frameCount)

extern const LGPTimerLPF3_Config LGPTimerLPF3_config[];
extern const uint_least8_t LGPTimerLPF3_count;

static volatile uint8_t gInitDone = 0;
static PDEIRGENLPF3_Transaction gCurTrans;
//-----------------------------------------------------------------------------
// Natan
//extern DATA_LEN
extern uint8_t data[];
int data_ind = 0;
extern bool inversionFlag;
extern PDEIRLPF3_Handle pdeIrHandle;
//-----------------------------------------------------------------------------
typedef struct
{
    // how many cycles of carrier wave freq that it will be active
    uint16_t activeCount;

    // how many cycles of carrier wave freq total for the length
    uint16_t frameCount;
} PDEIR_Encoding;

static const uint16_t CAL_ACTIVE_PLUS_TICK_OFFSET = 1;
static const uint16_t CAL_FRAME_MINUS_TICK_OFFSET = 1;
static const uint16_t CAL_BIT_VAL_1_FRAME_MINUS_TICK_OFFSET = 2;
static const uint16_t CAL_BIT_VAL_0_FRAME_MINUS_TICK_OFFSET = 0;
static const uint16_t CAL_REPEAT2_PLUS_TICK_OFFSET = 17; //14; // 12;
static const uint16_t CAL_STOP_BIT_MINUS_TICK_OFFSET = 49;

static inline uint16_t CNT_PRE_CAL_ACTIVE(uint16_t cntPostCal)
{
    return cntPostCal - CAL_ACTIVE_PLUS_TICK_OFFSET;
}
static inline uint16_t CNT_PRE_CAL_FRAME(uint16_t cntPostCal)
{
    return cntPostCal + CAL_FRAME_MINUS_TICK_OFFSET;
}
static inline uint16_t CNT_POST_CAL_FRAME(uint16_t cntPreCal)
{
    return cntPreCal - CAL_FRAME_MINUS_TICK_OFFSET;
}

//-----------------------------------------------------------------------------
static const PDEIR_Encoding NEC_lead =
{
     .activeCount = TICK_FROM_USEC(9000) + CAL_ACTIVE_PLUS_TICK_OFFSET,
     .frameCount = TICK_FROM_USEC(9000 + 4500) - CAL_FRAME_MINUS_TICK_OFFSET
};

static const PDEIR_Encoding NEC_bitval_0 =
{
     .activeCount = TICK_FROM_USEC(560) + CAL_ACTIVE_PLUS_TICK_OFFSET,
     .frameCount = TICK_FROM_USEC(1125) - CAL_FRAME_MINUS_TICK_OFFSET - CAL_BIT_VAL_0_FRAME_MINUS_TICK_OFFSET
};
static const PDEIR_Encoding NEC_bitval_1 =
{
     .activeCount = TICK_FROM_USEC(560) + CAL_ACTIVE_PLUS_TICK_OFFSET,
     .frameCount =  TICK_FROM_USEC(2250) + CAL_FRAME_MINUS_TICK_OFFSET - CAL_BIT_VAL_1_FRAME_MINUS_TICK_OFFSET
};

// stop bit should be modulated til the end of the frame_interval
//static PDEIR_Encoding NEC_stop_bit;

static const PDEIR_Encoding NEC_repeat[2] =
{
     {
          .activeCount = TICK_FROM_USEC(9000) + CAL_ACTIVE_PLUS_TICK_OFFSET,
          .frameCount = TICK_FROM_USEC(9000 + 2250) - CAL_FRAME_MINUS_TICK_OFFSET
     },
     {
          .activeCount = TICK_FROM_USEC(560) + CAL_ACTIVE_PLUS_TICK_OFFSET,
          .frameCount = TICK_FROM_USEC(108000 - 9000 - 2250 - 560) - CAL_FRAME_MINUS_TICK_OFFSET + CAL_REPEAT2_PLUS_TICK_OFFSET
     }
};
//-----------------------------------------------------------------------------
static const PDEIRLPF3_Params defaultParams =
{
 // for NEC-like
 .carrierFreq_kHz = 38,
 .frameInterval_usec = 108000
};

static PDEIRLPF3_Object object;
static const PDEIRLPF3_HWAttrs hwAttrs =
{
     .LgptCarrierBaseAddr = LGPTLPF3_CARRIER_BASE,
     .LgptModulatorBaseAddr = LGPTLPF3_MODULATOR_BASE,
     .LgptModulatorInts  =  LGPT_IMSET_ZERO_SET
};

STATIC const PDEIRLPF3_Config PDEIRLPF3_configs[PDEIRLPF3_CONFIG_QTY] =
{
    // PDEIRLPF3_CONFIG_0
    {
        .object   = &object,
        .hwAttrs  = &hwAttrs
    }
};

static uint8_t setLgptIndexes(PDEIRLPF3_Config *config);
static uint8_t isMatchedLgptIndexes(PDEIRLPF3_Config *config);
static PDEIRLPF3_Status initHw(PDEIRLPF3_Handle handle, PDEIRLPF3_Params *params);
static LGPTimerLPF3_ChannelNo getLgptChan(uint8_t lgptConfigIndex);
static void PDEIRLPF3_modulator_callback();


void PDEIRLPF3_init()
{
    // not sure if anything is needed to initialize yet...

    if (gInitDone)
    {
        return;
    }

    {
        object.isOpen = 0;
        object.state = PDEIRLPF3_STATE_IDLE;
        LGPTimerLPF3_init();
    }

    gInitDone = 1;
}
void PDEIRLPF3_Params_init(PDEIRLPF3_Params *pParams)
{
    if (pParams)
    {
        *pParams = defaultParams;
    }
}
PDEIRLPF3_Handle PDEIRLPF3_open(uint8_t index, PDEIRLPF3_Params *pParams)
{
    if (index > PDEIRLPF3_CONFIG_QTY)
        return 0;

    PDEIRLPF3_Handle handle             = &PDEIRLPF3_configs[index];

    PDEIRLPF3_Object *object            = handle->object;
    PDEIRLPF3_HWAttrs const *hwAttrs    = handle->hwAttrs;
    LGPTimerLPF3_Object *carrierObject   = object->LgptCarrierHandle->object;
    LGPTimerLPF3_Object *modulatorObject = object->LgptModulatorHandle->object;

    uint32_t key = HwiP_disable();
    if (object->isOpen || carrierObject->isOpen || modulatorObject->isOpen)
    {
        HwiP_restore(key);
        return 0;
    }
    object->isOpen = 1;
    carrierObject->isOpen = 1;
    modulatorObject->isOpen = 1;
    HwiP_restore(key);

    Power_setConstraint(PowerLPF3_DISALLOW_STANDBY);

    Power_setDependency(PowerLPF3_PERIPH_LGPT0);
    Power_setDependency(PowerLPF3_PERIPH_LGPT1);

    PDEIRLPF3_Params params;
    if (pParams)
    {
        params = *pParams;
    }
    else
    {
        params = defaultParams;
    }

    object->bitIndex = 0;
    object->countPassed = 0; // how many ticks of params.carrierFreq_kHz
    object->state = PDEIRLPF3_STATE_IDLE;

    if (initHw(handle, &params) != PDEIRLPF3_STATUS_OK)
        while (1); // error

    object->frameInterval_tick = TICK_FROM_USEC(params.frameInterval_usec);

    return handle;

}

PDEIRLPF3_Status PDEIRLPF3_transfer(PDEIRLPF3_Handle handle, PDEIRGENLPF3_Transaction *transaction)
{
    if (!handle) return PDEIRLPF3_STATUS_INVALID_PAR;

    uint32_t key = HwiP_disable();
    if (!handle && handle->object->state != PDEIRLPF3_STATE_IDLE) {
        HwiP_restore(key);
        return PDEIRLPF3_STATUS_BAD_STATE;
    }
    handle->object->state = PDEIRLPF3_STATE_STARTING;
    HwiP_restore(key);

    // this is new transmission, so clear the object
    // alternatively the object may be cleared after the end of a transaction...
    handle->object->bitIndex = 0;
    handle->object->countPassed = 0;
    gCurTrans = *transaction;

    handle->object->state = PDEIRLPF3_STATE_LEAD_CODE;
    handle->object->countPassed += CNT_PRE_CAL_FRAME(NEC_lead.frameCount);

    // Set TGT to the length of the symbol, assume lead frame for now
    HWREG(LGPT1_BASE + LGPT_O_TGT) &= ~LGPT_TGT_VAL_M;
    HWREG(LGPT1_BASE + LGPT_O_TGT) |= (NEC_lead.frameCount << LGPT_TGT_VAL_S);

    // Set C0CC to number of 38 kHz pulses in the symbol, assume lead frame for now
    HWREG(LGPT1_BASE + LGPT_O_C0CC) &= ~LGPT_C0CC_VAL_M;
    HWREG(LGPT1_BASE + LGPT_O_C0CC) |= (NEC_lead.activeCount << LGPT_C0CC_VAL_S);

    // clear and enable interrupt for modulator
    IntClearPend(INT_LGPT1_COMB);
    IntEnable(INT_LGPT1_COMB);

    // start the IRGEN system
    // Start carrier wave in up-periodic. CTL.MODE = 2
    HWREG(LGPT0_BASE + LGPT_O_CTL) &= ~LGPT_CTL_MODE_M;
    HWREG(LGPT0_BASE + LGPT_O_CTL) |= LGPT_CTL_MODE_UP_PER;

    // Start modulator in up-periodic. CTL.MODE = 2
    HWREG(LGPT1_BASE + LGPT_O_CTL) &= ~LGPT_CTL_MODE_M;
    HWREG(LGPT1_BASE + LGPT_O_CTL) |= LGPT_CTL_MODE_UP_PER;


    uint16_t activeCount;
    uint16_t frameCount;

    if (gCurTrans.customCode & 0x0001) {
        activeCount = NEC_bitval_1.activeCount;
        frameCount = NEC_bitval_1.frameCount;
    } else {
        activeCount = NEC_bitval_0.activeCount;
        frameCount = NEC_bitval_0.frameCount;
    }

    // PTGT = frameCount
    HWREG(LGPT1_BASE + LGPT_O_PTGT) &= ~LGPT_TGT_VAL_M;
    HWREG(LGPT1_BASE + LGPT_O_PTGT) |= (frameCount << LGPT_TGT_VAL_S);

    // PC0CC = activeCount
    HWREG(LGPT1_BASE + LGPT_O_PC0CC) &= ~LGPT_C0CC_VAL_M;
    HWREG(LGPT1_BASE + LGPT_O_PC0CC) |= (activeCount << LGPT_C0CC_VAL_S);

    return PDEIRLPF3_STATUS_OK;
}


PDEIRLPF3_Status PDEIRLPF3_stop(PDEIRLPF3_Handle handle)
{
    if (!handle)
        return PDEIRLPF3_STATUS_INVALID_PAR;

    HWREG(handle->hwAttrs->LgptCarrierBaseAddr + LGPT_O_CTL) &= ~LGPT_CTL_MODE_M;
    HWREG(handle->hwAttrs->LgptModulatorBaseAddr) &= ~LGPT_CTL_MODE_M;
    IntDisable(INT_LGPT1_COMB);

    return PDEIRLPF3_STATUS_OK;
}
void PDEIRLPF3_close(PDEIRLPF3_Handle handle)
{
    if (!handle)
        return;

    uint32_t key = HwiP_disable();
    if (!handle->object->isOpen)
    {
        HwiP_restore(key);
        return;
    }
    handle->object->isOpen = 0;
    handle->object->LgptCarrierHandle->object->isOpen = 0;
    handle->object->LgptModulatorHandle->object->isOpen = 0;

    PDEIRLPF3_stop(handle);
    Power_releaseConstraint(PowerLPF3_DISALLOW_STANDBY);
    Power_releaseDependency(PowerLPF3_PERIPH_LGPT0);
    Power_releaseDependency(PowerLPF3_PERIPH_LGPT1);

    HwiP_restore(key);

   //  while (1); // TODO

}

static PDEIRLPF3_Status initHw(PDEIRLPF3_Handle handle, PDEIRLPF3_Params *params)
{
    // initialize LGPTs... LGPT0 for carrier wave and LGPT1 for modulator

    /** from TRM:
        LGPT0 (Carrier wave generation):
        • Set the PRECFG.TICKDIV and TGT to fulfill the following equation:
        (PRECFG.TICKDIV+1)*TGT*2 = 48 MHz / 38 kHz
        E.g. PRECFG.TICKDIV = 3 and TGT 421. This gives a carrier wave of approximately 38 kHz (Theoretically
        38.00475 kHz).
        • Set C0CC = 210 to give roughly 50% duty cycle.
        • Set C0CFG.CCACT = TGL_ON_CMP, and C0CFG.OUT0 = 1.
        • Set IRGEN.CTL = 1.
        • Start timer in up-periodic. CTL.MODE = 2
     */

    // Toby(2024-04-11): use these values instead
    const uint8_t div = 3;
    const uint16_t tgt = 315;
    const uint16_t dutyHigh = tgt/3;
//    const uint16_t tgt = 421;       // Natan
//    const uint16_t dutyHigh = tgt/2;  // Natan
    uint32_t srcClkFreq = params->carrierFreq_kHz * 1000;

    // setup LGPT0 for IR carrier
    {
        const uint8_t ccactConfig = LGPT_C0CFG_CCACT_SET_ON_0_TGL_ON_CMP; // dutyHigh based duty cycle

        // ? start clock for LGPT0 and wait for it to complete?
        HWREG(CLKCTL_BASE + CLKCTL_O_CLKENSET0) |= CLKCTL_CLKENSET0_LGPT0_CLK_SET;
        while ((HWREG(CLKCTL_BASE + CLKCTL_O_CLKCFG0) & CLKCTL_CLKCFG0_LGPT0_M)
                !=  CLKCTL_CLKCFG0_LGPT0_CLK_EN);

        // halt on debug
        HWREG(LGPT0_BASE + LGPT_O_EMU) |= LGPT_EMU_HALT_EN | LGPT_EMU_CTL_IMMEDIATE;

        // Set the PRECFG.TICKDIV and TGT
        HWREG(LGPT0_BASE + LGPT_O_PRECFG) &= ~LGPT_PRECFG_TICKDIV_M;
        HWREG(LGPT0_BASE + LGPT_O_PRECFG) |= (div << LGPT_PRECFG_TICKDIV_S);
        HWREG(LGPT0_BASE + LGPT_O_TGT) &= ~LGPT_TGT_VAL_M;
        HWREG(LGPT0_BASE + LGPT_O_TGT) |= (tgt << LGPT_TGT_VAL_S);

        // Set C0CC to give roughly 33% duty cycle.
        HWREG(LGPT0_BASE + LGPT_O_C0CC) &= ~LGPT_C0CC_VAL_M;
        HWREG(LGPT0_BASE + LGPT_O_C0CC) |= (dutyHigh << LGPT_C0CC_VAL_S);

        // Set C0CFG.CCACT = TGL_ON_CMP, and C0CFG.OUT0 = 1.
        HWREG(LGPT0_BASE + LGPT_O_C0CFG) &= ~LGPT_C0CFG_CCACT_M;
        HWREG(LGPT0_BASE + LGPT_O_C0CFG) |= ccactConfig;
        HWREG(LGPT0_BASE + LGPT_O_C0CFG) &= ~LGPT_C0CFG_OUT0_M;
        HWREG(LGPT0_BASE + LGPT_O_C0CFG) |= LGPT_C0CFG_OUT0_EN;

        // Set IRGEN.CTL = 1.
        HWREG(LGPT0_BASE + LGPT_O_IRGEN) &= ~LGPT_IRGEN_CTL_M;
        HWREG(LGPT0_BASE + LGPT_O_IRGEN) |= LGPT_IRGEN_CTL_EN;
    }

    // setup LGPT1 for IR modulator
    {
        // ? start clock for LGPT0 and wait for it to complete
        HWREG(CLKCTL_BASE + CLKCTL_O_CLKENSET0) |= CLKCTL_CLKENSET0_LGPT1_CLK_SET;
        while ((HWREG(CLKCTL_BASE + CLKCTL_O_CLKCFG0) & CLKCTL_CLKCFG0_LGPT1_M)
                !=  CLKCTL_CLKCFG0_LGPT1_CLK_EN);

        // halt on debug
        HWREG(LGPT1_BASE + LGPT_O_EMU) |= LGPT_EMU_HALT_EN | LGPT_EMU_CTL_IMMEDIATE;

        // enable interrupt for updating the IR symbol
        //IntRegister(INT_LGPT1_COMB, test_lgpt_hwreg_callback);

        IntRegister(INT_LGPT1_COMB, PDEIRLPF3_modulator_callback);

        IntClearPend(INT_LGPT1_COMB);
        IntSetPriority(INT_LGPT1_COMB, INT_PRI_LEVEL0);
        //IntEnable(INT_LGPT1_COMB);
        //HWREG(LGPT1_BASE + LGPT_O_IMSET) |= LGPT_IMSET_TGT_SET | LGPT_IMSET_ZERO_SET;
        HWREG(LGPT1_BASE + LGPT_O_IMSET) |= handle->hwAttrs->LgptModulatorInts;

        // add a phase delay, may help with rounding errors // Natan
        HWREG(LGPT1_BASE + LGPT_O_CTL) &= ~LGPT_CTL_INTP_M;
        HWREG(LGPT1_BASE + LGPT_O_CTL) |= LGPT_CTL_INTP_LATE;

        // Set PRECFG.TICKSRC = FALL_TICK
        HWREG(LGPT1_BASE + LGPT_O_PRECFG) &= ~LGPT_PRECFG_TICKSRC_M;
        HWREG(LGPT1_BASE + LGPT_O_PRECFG) |= LGPT_PRECFG_TICKSRC_FALL_TICK;

        // Set C0CFG.CCACT = SET_ON_0_TGL_ON_CMP, C0CFG.OUT0 = 1
        HWREG(LGPT1_BASE + LGPT_O_C0CFG) &= ~LGPT_C0CFG_CCACT_M;
        HWREG(LGPT1_BASE + LGPT_O_C0CFG) |= LGPT_C0CFG_CCACT_SET_ON_0_TGL_ON_CMP;
        HWREG(LGPT1_BASE + LGPT_O_C0CFG) &= ~LGPT_C0CFG_OUT0_M;
        HWREG(LGPT1_BASE + LGPT_O_C0CFG) |= LGPT_C0CFG_OUT0_EN;

        // Set EVTSVT.LGPT1TENSEL = LGPT0C0
        HWREG(EVTSVT_BASE + EVTSVT_O_LGPT1TENSEL) |= EVTSVT_LGPT1TENSEL_PUBID_LGPT0C0;

        // Set TGT to the length of the symbol, assume lead frame for now
        HWREG(LGPT1_BASE + LGPT_O_TGT) &= ~LGPT_TGT_VAL_M;
        HWREG(LGPT1_BASE + LGPT_O_TGT) |= (NEC_lead.frameCount << LGPT_TGT_VAL_S);

        // Set C0CC to number of 38 kHz pulses in the symbol, assume lead frame for now
        HWREG(LGPT1_BASE + LGPT_O_C0CC) &= ~LGPT_C0CC_VAL_M;
        HWREG(LGPT1_BASE + LGPT_O_C0CC) |= (NEC_lead.activeCount << LGPT_C0CC_VAL_S);

    }

    // configure IR pin
    {
        HWREG(IOC_BASE + IOC_O_IOC11) |= IOC_IOC11_PORTCFG_PFUNC3;
    }

    return PDEIRLPF3_STATUS_OK;
}

static void PDEIRLPF3_modulator_callback()
{
    uint32_t lgptMis = HWREG(LGPT1_BASE + LGPT_O_MIS);

    if (lgptMis & LGPT_MIS_ZERO_SET)
    {
//        HWREG(LGPT1_BASE + LGPT_O_ICLR) = LGPT_MIS_ZERO_SET;
 //       IntClearPend(INT_LGPT1_COMB);

        volatile PDEIRLPF3_Handle pdeIrHandle = (PDEIRLPF3_Handle) &PDEIRLPF3_configs[0];
        volatile PDEIRLPF3_Object *pdeIrObj = pdeIrHandle->object;
        // 1. Force the compiler to treat the handle and its object structure as volatile RAM variables
  //      volatile PDEIRLPF3_Config *pdeIrConfig = (volatile PDEIRLPF3_Config *)&PDEIRLPF3_configs[0];
  //      volatile PDEIRLPF3_Object *pdeIrObj    = (volatile PDEIRLPF3_Object *)pdeIrConfig->object;


        volatile uint16_t activeCount = 0;
        volatile uint16_t frameCount = 0;

        switch (pdeIrObj->state)
        {
        case PDEIRLPF3_STATE_LEAD_CODE:
        {
            pdeIrObj->state = PDEIRLPF3_STATE_CUST_CODE_1;
            pdeIrObj->bitIndex = 1;

            if ((1 << pdeIrObj->bitIndex) & gCurTrans.customCode) {
                activeCount = NEC_bitval_1.activeCount;
                frameCount = NEC_bitval_1.frameCount;
            } else {
                activeCount = NEC_bitval_0.activeCount;
                frameCount = NEC_bitval_0.frameCount;
            }

            pdeIrObj->bitIndex += 1;
            uint16_t bit0_frame = (gCurTrans.customCode & 0x01) ? NEC_bitval_1.frameCount
                                                                : NEC_bitval_0.frameCount;

            // IMPORTANT: Accumulate the duration of the current frame
            pdeIrObj->countPassed += CNT_PRE_CAL_FRAME(bit0_frame);

        } break;

        case PDEIRLPF3_STATE_CUST_CODE_1:
            // The active buffer plays bits 1 through 7.
        {
            if ((1 << pdeIrObj->bitIndex) & gCurTrans.customCode) {
                activeCount = NEC_bitval_1.activeCount;
                frameCount = NEC_bitval_1.frameCount;
            } else {
                activeCount = NEC_bitval_0.activeCount;
                frameCount = NEC_bitval_0.frameCount;
            }



            pdeIrObj->bitIndex += 1;

            // We wait until the index becomes equal to 8
            // (this means that Bit 7 has successfully gone to Shadow)
            if (pdeIrObj->bitIndex >= 8) {
                pdeIrObj->state = PDEIRLPF3_STATE_CUST_CODE_2_N;
                pdeIrObj->bitIndex = 0;
            }

            // IMPORTANT: Accumulate the duration of the current frame
            pdeIrObj->countPassed += CNT_PRE_CAL_FRAME(frameCount);

        } break;

        case PDEIRLPF3_STATE_CUST_CODE_2_N:
        {
            /* 1. Extract the logical state of the current bit from the custom code */
            bool currentBit = ((1 << pdeIrObj->bitIndex) & gCurTrans.customCode) ? true : false;

            /*
             * 2. Apply conditional inversion based on the target device profile:
             *    - inversionFlag == true  (TV LG Mode): Enforce 1's complement inversion.
             *    - inversionFlag == false (Decoder Mode): Pass the byte as direct un-inverted data.
             */
            if (inversionFlag) {
                currentBit = !currentBit;
            }

            /* 3. Assign hardware timing values based on the resolved bit state */
            if (currentBit) {
                activeCount = NEC_bitval_1.activeCount;
                frameCount  = NEC_bitval_1.frameCount;
            } else {
                activeCount = NEC_bitval_0.activeCount;
                frameCount  = NEC_bitval_0.frameCount;
            }

            /* 4. Advance the bit pointer and safely update the state machine pipeline bounds */
            pdeIrObj->bitIndex += 1;
            if (pdeIrObj->bitIndex >= 8) {
                pdeIrObj->state = PDEIRLPF3_STATE_DATA_CODE_1;
                pdeIrObj->bitIndex = 0; /* Reset index register for the next data payload phase */
            }

            /* IMPORTANT: Accumulate the duration of the current frame into the tracking registers */
            pdeIrObj->countPassed += CNT_PRE_CAL_FRAME(frameCount);

        } break;

        case PDEIRLPF3_STATE_DATA_CODE_1:
        {   // Direct Command Byte
            if ((1 << pdeIrObj->bitIndex) & gCurTrans.dataCode) {
                activeCount = NEC_bitval_1.activeCount;
                frameCount = NEC_bitval_1.frameCount;
            } else {
                activeCount = NEC_bitval_0.activeCount;
                frameCount = NEC_bitval_0.frameCount;
            }
            pdeIrObj->bitIndex += 1;
            if (pdeIrObj->bitIndex >= 8) {
                pdeIrObj->state = PDEIRLPF3_STATE_DATA_CODE_2;
                pdeIrObj->bitIndex = 0;
            }

            // IMPORTANT: Accumulate the duration of the current frame
            pdeIrObj->countPassed += CNT_PRE_CAL_FRAME(frameCount);

        } break;

        case PDEIRLPF3_STATE_DATA_CODE_2:
        {   // Inverted command byte (1's complement)
            if ((1 << pdeIrObj->bitIndex) & gCurTrans.dataCode) {
                activeCount = NEC_bitval_0.activeCount;
                frameCount = NEC_bitval_0.frameCount;
            } else {
                activeCount = NEC_bitval_1.activeCount;
                frameCount = NEC_bitval_1.frameCount;
            }
            pdeIrObj->bitIndex += 1;
            if (pdeIrObj->bitIndex >= 8) {
                pdeIrObj->state = PDEIRLPF3_STATE_STOP_BIT;
                pdeIrObj->bitIndex = 0;
            }

            // IMPORTANT: Accumulate the duration of the current frame
            pdeIrObj->countPassed += CNT_PRE_CAL_FRAME(frameCount);

        } break;

        case PDEIRLPF3_STATE_STOP_BIT:
        {   // All 32 bits have fired. Loading the final stop bit into the pipeline.
            activeCount = NEC_bitval_0.activeCount;
            frameCount = pdeIrObj->frameInterval_tick - pdeIrObj->countPassed - CAL_STOP_BIT_MINUS_TICK_OFFSET;
            pdeIrObj->state = PDEIRLPF3_STATE_REPEAT_CODE_1;
        } break;

        case PDEIRLPF3_STATE_REPEAT_CODE_1:
        {   // First phase of the final repetition
            activeCount = NEC_repeat[0].activeCount;
            frameCount = NEC_repeat[0].frameCount;
            pdeIrObj->state = PDEIRLPF3_STATE_REPEAT_CODE_2;
        } break;

        case PDEIRLPF3_STATE_REPEAT_CODE_2:
        {   // Second phase of the final repetitio
            activeCount = NEC_repeat[1].activeCount;
            frameCount = NEC_repeat[1].frameCount;
          //  pdeIrObj->state = PDEIRLPF3_STATE_STOP;
            pdeIrObj->state = PDEIRLPF3_STATE_LAST_EXPANDED;
        } break;

        case PDEIRLPF3_STATE_LAST_EXPANDED:
        {
            if(RepeatCounter > 0) {
               RepeatCounter = RepeatCounter-1;
               activeCount = NEC_repeat[0].activeCount;
               frameCount = NEC_repeat[0].frameCount;
               pdeIrObj->state = PDEIRLPF3_STATE_REPEAT_CODE_2;
           }
           else
           {
            pdeIrObj->state = PDEIRLPF3_STATE_STOP;
           }
        } break;
/*
        case PDEIRLPF3_STATE_STOP:
        {
            // The pipeline is completely empty. Turning off the physical layers.
            pdeIrObj->state = PDEIRLPF3_STATE_IDLE;
            set_DC2DC_ENA(false);
 //           PDEIRLPF3_stop(pdeIrHandle);
            PDEIRLPF3_close(pdeIrHandle);   // ???  Natan
            pdeIrHandle = 0;

            HWREG(0x40080000 + 0x20) |= 0x1; // Turn the radio beats back on

            Power_releaseConstraint(PowerLPF3_DISALLOW_STANDBY);
            return;
        } break;

        default:
          //  while (1);
            break;
        }
 */
        case PDEIRLPF3_STATE_STOP: {

            pdeIrObj->state = PDEIRLPF3_STATE_IDLE;
            set_DC2DC_ENA(false);
            PDEIRLPF3_stop(pdeIrHandle);
         //   PDEIRLPF3_close(pdeIrHandle);
            pdeIrHandle = 0;


            HWREG(0x40080000 + 0x20) |= 0x1; // Turn the radio beats back on
  //          Power_releaseConstraint(PowerLPF3_DISALLOW_STANDBY);
            void changeAdvIntervalRuntime(uint32_t intervalMs);
    //        setAdvIntervalTicks(210);  // 210ms

        //    extern void App_RadioPostIrNotification(void);
        //    App_RadioPostIrNotification();
   //         extern void App_RadioPostIrNotification(uintptr_t arg);
   //         BLEAppUtil_invokeFunction(App_RadioPostIrNotification, 0);
            return;
           } break;

        default:
            // while (1);
            break;
        }


       // HARDWARE PIPELINE RECHARGE AT END OF INTERRUPT:
        HWREG(LGPT1_BASE + LGPT_O_PTGT) =  ((HWREG(LGPT1_BASE + LGPT_O_PTGT) & ~LGPT_TGT_VAL_M )
                                                               | (frameCount << LGPT_TGT_VAL_S));


        HWREG(LGPT1_BASE + LGPT_O_PC0CC) = ((HWREG(LGPT1_BASE + LGPT_O_PC0CC) & ~LGPT_C0CC_VAL_M)
                                                               | (activeCount << LGPT_C0CC_VAL_S));

              // Restoring the saved timer mode
 //  ???             HWREG(LGPT1_BASE + LGPT_O_CTL) |= savedModeLgpt1;

    }
}

