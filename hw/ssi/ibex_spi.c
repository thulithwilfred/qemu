
/*
 * QEMU model of the Ibex SPI Controller
 *
 * Copyright (C) 2018 Western Digital
 * Copyright (C) 2018 Wilfred Mallawa <wilfred.mallawa@wdc.com>
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
#include "hw/ssi/ibex_spi.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"


#ifndef IBEX_SPI_ERR_DEBUG
#define IBEX_SPI_ERR_DEBUG 0
#endif

#define DB_PRINT_L(lvl, fmt, args...) do { \
    if (IBEX_SPI_ERR_DEBUG >= lvl) { \
        qemu_log("%s: " fmt, __func__, ## args); \
    } \
} while (0)

#define DB_PRINT(fmt, args...) DB_PRINT_L(1, fmt, ## args)

static void ibex_spi_reset(DeviceState *dev) 
{
    DB_PRINT("Resetting Ibex SPI\n");
    IbexSPIState *s = IBEX_SPI(dev);

    /* Register Reset */
    s->regs[IBEX_SPI_INTR_STATE]   = 0x00;
    s->regs[IBEX_SPI_INTR_ENABLE]  = 0x00;
    s->regs[IBEX_SPI_INTR_TEST]    = 0x00;
    s->regs[IBEX_SPI_ALERT_TEST]   = 0x00;
    s->regs[IBEX_SPI_CONTROL]      = 0x7f;
    s->regs[IBEX_SPI_STATUS]       = 0x00;
    s->regs[IBEX_SPI_CONFIGOPTS]   = 0x00;
    s->regs[IBEX_SPI_CSID]         = 0x00;
    s->regs[IBEX_SPI_COMMAND]      = 0x00;
    /* RX/TX Modelled by FIFO */
    s->regs[IBEX_SPI_RXDATA]       = 0x00;
    s->regs[IBEX_SPI_TXDATA]       = 0x00;

    s->regs[IBEX_SPI_ERROR_ENABLE] = 0x1F;
    s->regs[IBEX_SPI_ERROR_STATUS] = 0x00;
    s->regs[IBEX_SPI_EVENT_ENABLE] = 0x00;


    fifo8_reset(&s->rx_fifo);
    fifo8_reset(&s->tx_fifo);    

    DB_PRINT("Reset OK\n");
    return;
}

static void ibex_spi_irq(IbexSPIState *s)
{

}


static void ibex_spi_transfer(IbexSPIState *s) 
{
    uint8_t rx, tx;

    tx = fifo8_pop(&s->tx_fifo);
    DB_PRINT("Data to send: 0x%x\n", tx);
    rx = ssi_transfer(s->ssi, tx);
    DB_PRINT("Data received: 0x%x\n", rx);
    fifo8_push(&s->rx_fifo, rx);

    DB_PRINT("Data received: 0x%x - rx pos: %d/%d\n",
             rx, fifo8_num_used(&s->rx_fifo),
             s->regs[IBEX_SPI_STATUS]);
             
    ibex_spi_irq(s);
}


static uint64_t ibex_spi_read(void *opaque, hwaddr addr,
                                     unsigned int size)
{
    IbexSPIState *s = opaque;
    uint64_t rc = 0;

    if (s == NULL) {
         qemu_log_mask(LOG_GUEST_ERROR,
                        "Null device state");
    }

    /* Match reg index */
    addr = addr >> 2;

    switch (addr) {
    case IBEX_SPI_INTR_STATE...IBEX_SPI_INTR_ENABLE:
    case IBEX_SPI_CONTROL...IBEX_SPI_CSID:
    case IBEX_SPI_TXDATA:
        rc = s->regs[addr];
        break;
    case IBEX_SPI_RXDATA:
        rc = fifo8_pop(&s->rx_fifo);
        break;
    case IBEX_SPI_ERROR_ENABLE...IBEX_SPI_EVENT_ENABLE:
        rc = s->regs[addr];
        break;       
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "Bad offset 0x%" HWADDR_PRIx "\n",
                      addr << 2);       
    }

    DB_PRINT("Address: 0x%" HWADDR_PRIx ", value: 0x%lx\n", addr << 2, rc);
    return rc;
}


static void ibex_spi_write(void *opaque, hwaddr addr,
                                uint64_t val64, unsigned int size)
{
    IbexSPIState *s = opaque;
    uint32_t value = val64;

    DB_PRINT("Address: 0x%" HWADDR_PRIx ", Value: 0x%x\n", addr, value);

    if (s == NULL) {
         qemu_log_mask(LOG_GUEST_ERROR,
                        "Null device state");
    }  

    /* Match reg index */
    addr = addr >> 2;

    switch (addr) {
        case IBEX_SPI_INTR_STATE...IBEX_SPI_CONTROL:
        case IBEX_SPI_CONFIGOPTS...IBEX_SPI_COMMAND:
            s->regs[addr] = value;
            break;
        case IBEX_SPI_TXDATA:
            fifo8_push(&s->tx_fifo, value);
            ibex_spi_transfer(s);
            break;
        case IBEX_SPI_ERROR_ENABLE...IBEX_SPI_EVENT_ENABLE:
            s->regs[addr] = value;
            break;
        default:
        qemu_log_mask(LOG_GUEST_ERROR, "Bad offset 0x%" HWADDR_PRIx "\n",
                      addr << 2);            
    }
    ibex_spi_irq(s);
}

//!!!!!!!!!!!!//
static const MemoryRegionOps ibex_spi_ops = {
    .read = ibex_spi_read,
    .write = ibex_spi_write,
    //!! CHECK
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static Property ibex_spi_properties[] = {
    DEFINE_PROP_UINT8("cs-width", IbexSPIState, cs_width, 1),
    DEFINE_PROP_END_OF_LIST(),
};


static void ibex_spi_init(Object *obj)
{
    IbexSPIState *s = IBEX_SPI(obj);

    memory_region_init_io(&s->mmio, obj, &ibex_spi_ops, s,
                          TYPE_IBEX_SPI, 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);
}

static void ibex_spi_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    //dc->realize = sifive_spi_realize;
    dc->reset = ibex_spi_reset;
    //dc->vmsd = &vmstate_sifive;
    device_class_set_props(dc, ibex_spi_properties);
    //dc->props = ibex_spi_properties;
}

static const TypeInfo ibex_spi_info = {
    .name          = TYPE_IBEX_SPI,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IbexSPIState),
    .instance_init = ibex_spi_init,
    .class_init    = ibex_spi_class_init,
};

static void ibex_spi_register_types(void)
{
    type_register_static(&ibex_spi_info);
}

type_init(ibex_spi_register_types)