/*
 * QEMU model of the Ibex Power Manager
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

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/misc/ibex_pm.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "migration/vmstate.h"
#include "trace.h"

static void ibex_pm_realize (DeviceState *dev, Error **errp) {
    /* nothing to see here officer */
}

static void ibex_pm_reset(DeviceState *dev)
{
    IbexPMState *s = IBEX_PM(dev);
    /* Set all register values to spec defaults */
    s->regs[IBEX_PM_INTR_STATE]            = 0x00;
    s->regs[IBEX_PM_INTR_EN]               = 0x00;
    s->regs[IBEX_PM_INTR_TEST]             = 0x00;
    s->regs[IBEX_PM_ALERT_TEST]            = 0x00;
    s->regs[IBEX_PM_CTRL_CFG_REGWEN]       = 0x01;
    s->regs[IBEX_PM_CTRL]                  = 0x180;
    s->regs[IBEX_PM_CFG_CDC_SYNC]          = 0x00;
    s->regs[IBEX_PM_WAKEUP_EN_REGWEN]      = 0x01;
    s->regs[IBEX_PM_WAKEUP_EN]             = 0x00;
    s->regs[IBEX_PM_WAKE_STATUS]           = 0x00;
    s->regs[IBEX_PM_RESET_EN_REGWEN]       = 0x01;
    s->regs[IBEX_PM_RESET_EN]              = 0x00;
    s->regs[IBEX_PM_RESET_STATUS]          = 0x00;
    s->regs[IBEX_PM_ESCALATE_RESET_STATUS] = 0x00;
    s->regs[IBEX_PM_WAKE_INFO_CAPTURE_DIS] = 0x00;
    s->regs[IBEX_PM_WAKE_INFO]             = 0x00;
    s->regs[IBEX_PM_FAULT_STATUS]          = 0x00;

}

static uint64_t ibex_pm_read(void *opaque, hwaddr addr,
                                     unsigned int size)
{
    IbexPMState *s = opaque;
    uint32_t reg_val = 0x00;

    trace_ibex_lc_read(addr, size);
    /* Match reg index */
    addr = addr >> 2;
    // TODO: Put this inside the switch and match permissions
    reg_val = s->regs[addr];
    switch (addr) {

        default:
            qemu_log_mask(LOG_GUEST_ERROR, "Bad offset 0x%" HWADDR_PRIx "\n",
                          addr << 2);
    }
    return reg_val;
}

static void ibex_pm_write(void *opaque, hwaddr addr,
                                uint64_t val64, unsigned int size)
{
    IbexPMState *s = opaque;
    uint32_t val32 = val64;

    /* Match reg index */
    addr = addr >> 2;
    // TODO: Put this inside the switch and match permissions
    s->regs[addr] = val32;

    switch (addr) {
    /* Skipping any R/O registers */

    default:
        /* The remaining registers are all ro, or bad offset */
        qemu_log_mask(LOG_GUEST_ERROR, "Bad offset 0x%" HWADDR_PRIx "\n",
                      addr << 2);
    }
}

static const MemoryRegionOps ibex_pm_ops = {
    .read = ibex_pm_read,
    .write = ibex_pm_write,
    /* Ibex default LE */
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const VMStateDescription vmstate_ibex = {
    .name = TYPE_IBEX_PM,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, IbexPMState, IBEX_PM_NUM_REGS),
        VMSTATE_END_OF_LIST()
    }
};

static void ibex_pm_init(Object *obj)
{
    IbexPMState *s = IBEX_PM(obj);

    memory_region_init_io(&s->mmio, obj, &ibex_pm_ops, s,
                          TYPE_IBEX_PM, 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static void ibex_pm_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = ibex_pm_realize;
    dc->reset = ibex_pm_reset;
    dc->vmsd = &vmstate_ibex;
}

static const TypeInfo ibex_pm_info = {
    .name          = TYPE_IBEX_PM,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IbexPMState),
    .instance_init = ibex_pm_init,
    .class_init    = ibex_pm_class_init,
};

static void ibex_pm_register_types(void)
{
    type_register_static(&ibex_pm_info);
}

type_init(ibex_pm_register_types)
