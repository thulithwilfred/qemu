/*
 * QEMU model of the Ibex Life Cycle Controller
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
#ifndef IBEX_LC_CTRL_H
#define IBEX_LC_CTRL_H

#include "hw/sysbus.h"
#include "hw/hw.h"
#include "hw/ssi/ssi.h"
#include "qemu/fifo8.h"
#include "qom/object.h"
#include "hw/registerfields.h"
#include "qemu/timer.h"

#define TYPE_IBEX_LC_CTRL "ibex-lc"
#define IBEX_LC_CTRL(obj) \
    OBJECT_CHECK(IbexLCState, (obj), TYPE_IBEX_LC_CTRL)

/* LC Registers */
#define IBEX_LC_CTRL_ALERT_TEST                      (0x00 / 4) /* wo */
#define IBEX_CTRL_STATUS                             (0x04 / 4) /* ro */
#define IBEX_CTRL_CLAIM_TRANSITION_IF                (0x08 / 4) /* rw */
#define IBEX_LC_CTRL_TRANSITION_REGWEN               (0x0C / 4) /* ro */
#define IBEX_LC_CTRL_TRANSITION_CMD                  (0x10 / 4) /* r0w1c */
#define IBEX_LC_CTRL_TRANSITION_CTRL                 (0x14 / 4) /* rw1s*/
#define IBEX_LC_CTRL_TRANSITION_TOKEN_0              (0x18 / 4) /* rw */
#define IBEX_LC_CTRL_TRANSITION_TOKEN_1              (0x1C / 4) /* rw */
#define IBEX_LC_CTRL_TRANSITION_TOKEN_2              (0x20 / 4) /* rw */
#define IBEX_LC_CTRL_TRANSITION_TOKEN_3              (0x24 / 4) /* rw */
#define IBEX_LC_CTRL_TRANSITION_TARGET               (0x28 / 4) /* rw */
#define IBEX_LC_CTRL_OTP_VENDOR_TEST_CTRL            (0x2C / 4) /* rw */
#define IBEX_LC_CTRL_OTP_VENDOR_TEST_STATUS          (0x30 / 4) /* ro */
#define IBEX_LC_CTRL_LC_STATE                        (0x34 / 4) /* ro */
#define IBEX_LC_CTRL_LC_TRANSITION_CNT               (0x38 / 4) /* ro */
#define IBEX_LC_CTRL_LC_ID_STATE                     (0x3C / 4) /* ro */
#define IBEX_LC_CTRL_HW_REV                          (0x40 / 4) /* ro */
#define IBEX_LC_CTRL_DEVICE_ID_0                     (0x44 / 4) /* ro */
#define IBEX_LC_CTRL_DEVICE_ID_1                     (0x48 / 4) /* ro */
#define IBEX_LC_CTRL_DEVICE_ID_2                     (0x4C / 4) /* ro */
#define IBEX_LC_CTRL_DEVICE_ID_3                     (0x50 / 4) /* ro */
#define IBEX_LC_CTRL_DEVICE_ID_4                     (0x54 / 4) /* ro */
#define IBEX_LC_CTRL_DEVICE_ID_5                     (0x58 / 4) /* ro */
#define IBEX_LC_CTRL_DEVICE_ID_6                     (0x5C / 4) /* ro */
#define IBEX_LC_CTRL_DEVICE_ID_7                     (0x60 / 4) /* ro */
#define IBEX_LC_CTRL_MANUF_STATE_0                   (0x64 / 4) /* ro */
#define IBEX_LC_CTRL_MANUF_STATE_1                   (0x68 / 4) /* ro */
#define IBEX_LC_CTRL_MANUF_STATE_2                   (0x6C / 4) /* ro */
#define IBEX_LC_CTRL_MANUF_STATE_3                   (0x70 / 4) /* ro */
#define IBEX_LC_CTRL_MANUF_STATE_4                   (0x74 / 4) /* ro */
#define IBEX_LC_CTRL_MANUF_STATE_5                   (0x78 / 4) /* ro */
#define IBEX_LC_CTRL_MANUF_STATE_6                   (0x7C / 4) /* ro */
#define IBEX_LC_CTRL_MANUF_STATE_7                   (0x80 / 4) /* ro */

/*  Max Register (Based on addr) */
#define IBEX_LC_NUM_REGS           (IBEX_LC_CTRL_MANUF_STATE_7 + 1)

/* Lifecycle States  */
/* Unlocked test state where debug functions are enabled. */
#define TEST_UNLOCKED0 0x02108421

typedef struct {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion mmio;
    uint32_t regs[IBEX_LC_NUM_REGS];

} IbexLCState;


#endif