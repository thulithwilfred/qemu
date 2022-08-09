/*
 * QEMU model of the Ibex Power Manager (PM)
 * SPEC Reference: https://docs.opentitan.org/hw/ip/spi_host/doc/
 *
 * Copyright (C) 2022 Western Digital
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef IBEX_PM_H
#define IBEX_PM_H

#include "hw/sysbus.h"
#include "hw/hw.h"
#include "hw/ssi/ssi.h"
#include "qemu/fifo8.h"
#include "qom/object.h"
#include "hw/registerfields.h"
#include "qemu/timer.h"

#define TYPE_IBEX_PM "ibex-pm"
#define IBEX_PM(obj) \
    OBJECT_CHECK(IbexPMState, (obj), TYPE_IBEX_PM)


/* LC Registers */
#define IBEX_PM_INTR_STATE                     (0x00 / 4) /* rw1c */
#define IBEX_PM_INTR_EN                        (0x04 / 4) /* rw */
#define IBEX_PM_INTR_TEST                      (0x08 / 4) /* wo */
#define IBEX_PM_ALERT_TEST                     (0x0C / 4) /* wo */
#define IBEX_PM_CTRL_CFG_REGWEN                (0x10 / 4) /* ro */
#define IBEX_PM_CTRL                           (0x14 / 4) /* rw */
#define IBEX_PM_CFG_CDC_SYNC                   (0x18 / 4) /* rw */
#define IBEX_PM_WAKEUP_EN_REGWEN               (0x1C / 4) /* rw0c */
#define IBEX_PM_WAKEUP_EN                      (0x20 / 4) /* rw */
#define IBEX_PM_WAKE_STATUS                    (0x24 / 4) /* ro */
#define IBEX_PM_RESET_EN_REGWEN                (0x28 / 4) /* rw0c */
#define IBEX_PM_RESET_EN                       (0x2c / 4) /* rw */
#define IBEX_PM_RESET_STATUS                   (0x30 / 4) /* ro */
#define IBEX_PM_ESCALATE_RESET_STATUS          (0x34 / 4) /* ro */
#define IBEX_PM_WAKE_INFO_CAPTURE_DIS          (0x38 / 4) /* rw */
#define IBEX_PM_WAKE_INFO                      (0x3C / 4) /* rw1c */
#define IBEX_PM_FAULT_STATUS                   (0x40 / 4) /* ro */


/*  Max Register (Based on addr) */
#define IBEX_PM_NUM_REGS           (IBEX_PM_FAULT_STATUS + 1)

typedef struct {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion mmio;
    uint32_t regs[IBEX_PM_NUM_REGS];

} IbexPMState;

#endif