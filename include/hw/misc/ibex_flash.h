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
#ifndef IBEX_FLASH_H
#define IBEX_FLASH_H

#include "hw/sysbus.h"
#include "hw/hw.h"
#include "hw/ssi/ssi.h"
#include "qemu/fifo8.h"
#include "qom/object.h"
#include "hw/registerfields.h"
#include "qemu/timer.h"

#define TYPE_IBEX_FLASH "ibex-flash"
#define IBEX_FLASH(obj) \
    OBJECT_CHECK(IbexFlashState, (obj), TYPE_IBEX_FLASH)


/* LC Registers */

#define IBEX_FLASH_CTRL                  (0x50 / 4) /* ro */


/*  Max Register (Based on addr) */
#define IBEX_FLASH_NUM_REGS           (IBEX_FLASH_CTRL + 1)

typedef struct {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion mmio;
    uint32_t regs[IBEX_FLASH_NUM_REGS];

} IbexFlashState;

#endif