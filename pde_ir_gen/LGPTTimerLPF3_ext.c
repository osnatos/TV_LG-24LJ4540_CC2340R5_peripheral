/*
 * LGPTTimerLPF3_ext.c
 *
 *  Created on: Apr 11, 2024
 *      Author: a0232184
 */

#include <pde_ir_gen/LGPTTimerLPF3_ext.h>
#include <ti/drivers/timer/LGPTimerLPF3.h>
#include <ti/devices/cc23x0r5/inc/hw_memmap.h>
#include <ti/devices/cc23x0r5/inc/hw_types.h>
#include <ti/devices/cc23x0r5/inc/hw_lgpt.h>

extern LGPTimerLPF3_Config LGPTimerLPF3_config[];
extern const uint_least8_t LGPTimerLPF3_count;

void LGPTimerLPF3_init()
{
    uint8_t i;
    for (i = 0; i < LGPTimerLPF3_count; i++)
    {
        LGPTimerLPF3_config[i].object->isOpen = 0;
    }
}

void LGPTimerLPF3_enableInt(LGPTimerLPF3_Handle h, LGPTimerLPF3_Interrupt intMask)
{
    if (h)
    {
        const uint16_t validMask = 0x3FF; // 10 bits set for 10 unique int sources for LGPT
        HWREG(h->hwAttrs->baseAddr + LGPT_O_IMSET) |= intMask & validMask;
    }
}

void LGPT_ClearInterrupts_Ext(void)
{
    // Clear and disable interrupt registers directly via hardware memory maps
    // for both LGPT0 (Carrier) and LGPT1 (Modulator) base addresses

    // 1. Wipe LGPT0 (Usually at LGPT0_BASE)
    HWREG(LGPT0_BASE + LGPT_O_IMCLR) = 0x3FF; // Disable all 10 interrupt sources
    HWREG(LGPT0_BASE + LGPT_O_ICLR)  = 0x3FF; // Clear any pending interrupt flags

    // 2. Wipe LGPT1 (Usually at LGPT1_BASE)
    HWREG(LGPT1_BASE + LGPT_O_IMCLR) = 0x3FF; // Disable all 10 interrupt sources
    HWREG(LGPT1_BASE + LGPT_O_ICLR)  = 0x3FF; // Clear any pending interrupt flags
}
