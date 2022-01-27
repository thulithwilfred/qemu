
/*
 * QEMU model of the Ibex SPI Controller
 * SPEC Reference: https://docs.opentitan.org/hw/ip/spi_host/doc/
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
#include "hw/ssi/ibex_spi_host.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "migration/vmstate.h"

REG32(INTR_STATE, 0x00)
    FIELD(INTR_STATE, ERROR, 0, 1)
    FIELD(INTR_STATE, SPI_EVENT, 1, 1)
REG32(INTR_ENABLE, 0x04)
    FIELD(INTR_ENABLE, ERROR, 0, 1)
    FIELD(INTR_ENABLE, SPI_EVENT, 1, 1)
REG32(INTR_TEST, 0x08)
    FIELD(INTR_TEST, ERROR, 0, 1)
    FIELD(INTR_TEST, SPI_EVENT, 1, 1)
REG32(ALERT_TEST, 0x0c)
    FIELD(ALERT_TEST, FETAL_TEST, 0, 1)
REG32(CONTROL, 0x10)
    FIELD(CONTROL, RX_WATERMARK, 0, 8)
    FIELD(CONTROL, TX_WATERMARK, 1, 8)
    FIELD(CONTROL, OUTPUT_EN, 29, 1)
    FIELD(CONTROL, SW_RST, 30, 1)
    FIELD(CONTROL, SPIEN, 31, 1)
REG32(STATUS, 0x14)
    FIELD(STATUS, TXQD, 0, 8)
    FIELD(STATUS, RXQD, 18, 8)
    FIELD(STATUS, CMDQD, 16, 3)
    FIELD(STATUS, RXWM, 20, 1)
    FIELD(STATUS, BYTEORDER, 22, 1)
    FIELD(STATUS, RXSTALL, 23, 1)
    FIELD(STATUS, RXEMPTY, 24, 1)
    FIELD(STATUS, RXFULL, 25, 1)
    FIELD(STATUS, TXWM, 26, 1)
    FIELD(STATUS, TXSTALL, 27, 1)
    FIELD(STATUS, TXEMPTY, 28, 1)
    FIELD(STATUS, TXFULL, 29, 1)
    FIELD(STATUS, ACTIVE, 30, 1)
    FIELD(STATUS, READY, 31, 1)
REG32(CONFIGOPTS, 0x18)
    FIELD(CONFIGOPTS, CLKDIV_0, 0, 16)
    FIELD(CONFIGOPTS, CSNIDLE_0, 16, 4)
    FIELD(CONFIGOPTS, CSNTRAIL_0, 20, 4)
    FIELD(CONFIGOPTS, CSNLEAD_0, 24, 4)
    FIELD(CONFIGOPTS, FULLCYC_0, 29, 1)
    FIELD(CONFIGOPTS, CPHA_0, 30, 1)
    FIELD(CONFIGOPTS, CPOL_0, 31, 1)
REG32(CSID, 0x1c)
    FIELD(CSID, CSID, 0, 32)
REG32(COMMAND, 0x20)
    FIELD(COMMAND, LEN, 0,8)
    FIELD(COMMAND, CSAAT, 9, 1)
    FIELD(COMMAND, SPEED, 10, 2)
    FIELD(COMMAND, DIRECTION, 12, 2)
REG32(ERROR_ENABLE, 0x2c)
    FIELD(ERROR_ENABLE, CMDBUSY, 0,1)
    FIELD(ERROR_ENABLE, OVERFLOW, 1, 1)
    FIELD(ERROR_ENABLE, UNDERFLOW, 2, 1)
    FIELD(ERROR_ENABLE, CMDINVAL, 3, 1)
    FIELD(ERROR_ENABLE, CSIDINVAL, 4, 1)
REG32(ERROR_STATUS, 0x30)
    FIELD(ERROR_STATUS, CMDBUSY, 0,1)
    FIELD(ERROR_STATUS, OVERFLOW, 1, 1)
    FIELD(ERROR_STATUS, UNDERFLOW, 2, 1)
    FIELD(ERROR_STATUS, CMDINVAL, 3, 1)
    FIELD(ERROR_STATUS, CSIDINVAL, 4, 1)
    FIELD(ERROR_STATUS, ACCESSINVAL, 5, 1)
REG32(EVENT_ENABLE, 0x30)
    FIELD(EVENT_ENABLE, RXFULL, 0,1)
    FIELD(EVENT_ENABLE, TXEMPTY, 1, 1)
    FIELD(EVENT_ENABLE, RXWM, 2, 1)
    FIELD(EVENT_ENABLE, TXWM, 3, 1)
    FIELD(EVENT_ENABLE, READY, 4, 1)
    FIELD(EVENT_ENABLE, IDLE, 5, 1)


#ifndef IBEX_SPI_HOST_ERR_DEBUG
#define IBEX_SPI_HOST_ERR_DEBUG 0
#endif

#define DB_PRINT_L(lvl, fmt, args...) do { \
    if (IBEX_SPI_HOST_ERR_DEBUG >= lvl) { \
        qemu_log("%s: " fmt, __func__, ## args); \
    } \
} while (0)

#define DB_PRINT(fmt, args...) DB_PRINT_L(1, fmt, ## args)

static inline uint8_t div4_round_up(uint8_t dividend) {
    return ((dividend + (3))/4);
}

static void ibex_spi_rxfifo_reset(IbexSPIHostState *s)
{
    /* Empty the RX FIFO and assert RXEMPTY */
    fifo8_reset(&s->rx_fifo);
    s->regs[IBEX_SPI_HOST_STATUS] &= ~R_STATUS_RXFULL_MASK;
    s->regs[IBEX_SPI_HOST_STATUS] |= R_STATUS_RXEMPTY_MASK;
}

static void ibex_spi_txfifo_reset(IbexSPIHostState *s)  
{   
    /* Empty the TX FIFO and assert TXEMPTY */
    fifo8_reset(&s->tx_fifo);
    s->regs[IBEX_SPI_HOST_STATUS] &= ~R_STATUS_TXFULL_MASK;
    s->regs[IBEX_SPI_HOST_STATUS] |= R_STATUS_TXEMPTY_MASK;
}

static void ibex_spi_host_reset(DeviceState *dev) 
{
    DB_PRINT("Resetting Ibex SPI\n");
    IbexSPIHostState *s = IBEX_SPI_HOST(dev);

    /* SPI Host Register Reset */
    s->regs[IBEX_SPI_HOST_INTR_STATE]   = 0x00;
    s->regs[IBEX_SPI_HOST_INTR_ENABLE]  = 0x00;
    s->regs[IBEX_SPI_HOST_INTR_TEST]    = 0x00;
    s->regs[IBEX_SPI_HOST_ALERT_TEST]   = 0x00;
    s->regs[IBEX_SPI_HOST_CONTROL]      = 0x7f;
    s->regs[IBEX_SPI_HOST_STATUS]       = 0x00;
    s->regs[IBEX_SPI_HOST_CONFIGOPTS]   = 0x00;
    s->regs[IBEX_SPI_HOST_CSID]         = 0x00;
    s->regs[IBEX_SPI_HOST_COMMAND]      = 0x00;
    /* RX/TX Modelled by FIFO */
    s->regs[IBEX_SPI_HOST_RXDATA]       = 0x00;
    s->regs[IBEX_SPI_HOST_TXDATA]       = 0x00;

    s->regs[IBEX_SPI_HOST_ERROR_ENABLE] = 0x1F;
    s->regs[IBEX_SPI_HOST_ERROR_STATUS] = 0x00;
    s->regs[IBEX_SPI_HOST_EVENT_ENABLE] = 0x00;

    ibex_spi_rxfifo_reset(s);
    ibex_spi_txfifo_reset(s);
    
    return;
}

/* Check if we need to trigger an interrupt.
 * The two interrupts lines (host_err and event) can 
 * be enabled seperately in 'IBEX_SPI_HOST_INTR_ENABLE'.
 * 
 * Interrupts are triggered based on the ones 
 * enabled in the `IBEX_SPI_HOST_EVENT_ENABLE` and `IBEX_SPI_HOST_ERROR_ENABLE`.
 */
static void ibex_spi_host_irq(IbexSPIHostState *s)
{    
    bool error_en = s->regs[IBEX_SPI_HOST_INTR_ENABLE] & R_INTR_ENABLE_ERROR_MASK;
    bool event_en = s->regs[IBEX_SPI_HOST_INTR_ENABLE] & R_INTR_ENABLE_SPI_EVENT_MASK;
    bool err_pending = s->regs[IBEX_SPI_HOST_INTR_STATE] & R_INTR_STATE_ERROR_MASK;
    bool status_pending = s->regs[IBEX_SPI_HOST_INTR_STATE] & R_INTR_STATE_SPI_EVENT_MASK;
    int err_irq = 0, event_irq = 0;

    /* Error IRQ enabled and Error IRQ Cleared*/
    if (error_en && !err_pending) {
        /* Event enabled, Interrupt Test Error */
        if (s->regs[IBEX_SPI_HOST_INTR_TEST] & R_INTR_TEST_ERROR_MASK) {
            //printf("1\n");
            err_irq = 1;
        } else if ((s->regs[IBEX_SPI_HOST_ERROR_ENABLE] &  R_ERROR_ENABLE_CMDBUSY_MASK) &&
                    s->regs[IBEX_SPI_HOST_ERROR_STATUS] & R_ERROR_STATUS_CMDBUSY_MASK) {
            /* Wrote to COMMAND when not READY */
            //printf("2\n");
            err_irq = 1;
        } else if ((s->regs[IBEX_SPI_HOST_ERROR_ENABLE] &  R_ERROR_ENABLE_CMDINVAL_MASK) &&
                    s->regs[IBEX_SPI_HOST_ERROR_STATUS] & R_ERROR_STATUS_CMDINVAL_MASK) {
            /* Invalid command segment */
            //printf("2\n");
            err_irq = 1;
        } else if ((s->regs[IBEX_SPI_HOST_ERROR_ENABLE] & R_ERROR_ENABLE_CSIDINVAL_MASK) &&
                    s->regs[IBEX_SPI_HOST_ERROR_STATUS] & R_ERROR_STATUS_CSIDINVAL_MASK) {
            /* Invalid value for CSID */
            err_irq = 1;
        } 
        if (err_irq) {
            s->regs[IBEX_SPI_HOST_INTR_STATE] |= R_INTR_STATE_ERROR_MASK;
        }
        qemu_set_irq(s->host_err, err_irq);
    }
    
    /* Event IRQ Enabled and Event IRQ Cleared */
    if (event_en && !status_pending) {
        if (s->regs[IBEX_SPI_HOST_INTR_TEST] & R_INTR_TEST_SPI_EVENT_MASK) {
            /* Event enabled, Interrupt Test Event */
            event_irq = 1;
            //printf("3\n");
        } else if ((s->regs[IBEX_SPI_HOST_EVENT_ENABLE] & R_EVENT_ENABLE_READY_MASK) &&
                    (s->regs[IBEX_SPI_HOST_STATUS] & R_STATUS_READY_MASK)) {
            /* SPI Host ready for next command */
            event_irq = 1;   
            //printf("4\n");  
        } else if ((s->regs[IBEX_SPI_HOST_EVENT_ENABLE] & R_EVENT_ENABLE_TXEMPTY_MASK) &&
                    (s->regs[IBEX_SPI_HOST_STATUS] & R_STATUS_TXEMPTY_MASK)) {
            /* SPI TXEMPTY, TXFIFO drained */
            event_irq = 1;
            //printf("5\n");
        } else if ((s->regs[IBEX_SPI_HOST_EVENT_ENABLE] & R_EVENT_ENABLE_RXFULL_MASK) &&
                    (s->regs[IBEX_SPI_HOST_STATUS] & R_STATUS_RXFULL_MASK)) {
            /* SPI RXFULL, RXFIFO  full */
            event_irq = 1;
            //printf("6\n");
        }
        if (event_irq) {
            s->regs[IBEX_SPI_HOST_INTR_STATE] |= R_INTR_STATE_SPI_EVENT_MASK;
        }
        qemu_set_irq(s->event, event_irq);
    }
}

static void ibex_spi_host_transfer(IbexSPIHostState *s) 
{
    uint32_t rx, tx;
    /* Extract tx_len */
    uint8_t segment_len = ((s->regs[IBEX_SPI_HOST_COMMAND] & R_COMMAND_LEN_MASK)
                          >> R_COMMAND_LEN_SHIFT);
    //printf("QEMU: WRITE DATA %d\n\n", segment_len);
    while (segment_len > 0) {
        if (fifo8_is_empty(&s->tx_fifo)) {
            /* Dummy TX Byte */
            tx = 0;
        } else {
            tx = fifo8_pop(&s->tx_fifo);
        }
        
        rx = ssi_transfer(s->ssi, tx);
        //TODO RM TEST: ECHO FOR TESTING
        rx = tx;
        //TODO RM:
        //printf("Data to send: 0x%x\n", tx);
        DB_PRINT("Data to send: 0x%x\n", tx);
        DB_PRINT("Data received: 0x%x\n", rx);

        if (!fifo8_is_full(&s->rx_fifo)) {
         fifo8_push(&s->rx_fifo, rx);
        } else {
            /* Assert RXFULL */
            s->regs[IBEX_SPI_HOST_STATUS] |= R_STATUS_RXFULL_MASK;
        }
        --segment_len;
    }
    
    /* Assert Ready */
    s->regs[IBEX_SPI_HOST_STATUS] |= R_STATUS_READY_MASK;
    /* Set RXQD */
    s->regs[IBEX_SPI_HOST_STATUS] &= ~R_STATUS_RXQD_MASK;
    s->regs[IBEX_SPI_HOST_STATUS] |= (R_STATUS_RXQD_MASK & div4_round_up(segment_len));
    /* Set TXQD */
    s->regs[IBEX_SPI_HOST_STATUS] &= ~R_STATUS_TXQD_MASK;
    s->regs[IBEX_SPI_HOST_STATUS] |= div4_round_up(fifo8_num_used(&s->tx_fifo)) & R_STATUS_TXQD_MASK;
    /* Clear TXFULL */
    s->regs[IBEX_SPI_HOST_STATUS] &= ~R_STATUS_TXFULL_MASK;
    /* Assert TXEMPTY and drop remaining bytes that exceed segment_len */
    ibex_spi_txfifo_reset(s);
    /* Reset RXEMPTY */
    s->regs[IBEX_SPI_HOST_STATUS] &= ~R_STATUS_RXEMPTY_MASK;
    
    ibex_spi_host_irq(s);
}

static uint64_t ibex_spi_host_read(void *opaque, hwaddr addr,
                                     unsigned int size)
{
    IbexSPIHostState *s = opaque;
    uint32_t rc = 0;
    uint8_t rx_byte = 0;
    //TODO RM:
    //printf("QEMU: SPI REG READ 0x%lx\n", addr);
    if (s == NULL) {
         qemu_log_mask(LOG_GUEST_ERROR,
                        "Null device state");
    }
    /* Match reg index */
    addr = addr >> 2;
    switch (addr) {
    /* Skipping any W/O registers */
    case IBEX_SPI_HOST_INTR_STATE...IBEX_SPI_HOST_INTR_ENABLE:
    case IBEX_SPI_HOST_CONTROL...IBEX_SPI_HOST_STATUS:
        rc = s->regs[addr];
        break;
    case IBEX_SPI_HOST_CSID:
        rc = s->regs[addr];
        break;
    case IBEX_SPI_HOST_CONFIGOPTS:
        rc = s->config_opts[s->regs[IBEX_SPI_HOST_CSID]];
        break;
    case IBEX_SPI_HOST_TXDATA:
        rc = s->regs[addr];
        break;
    case IBEX_SPI_HOST_RXDATA:
        /* Clear RXFULL */
        s->regs[IBEX_SPI_HOST_STATUS] &= ~R_STATUS_RXFULL_MASK;

        if (fifo8_is_empty(&s->rx_fifo)) {
            /* Assert RXEMPTY, no IRQ */
            s->regs[IBEX_SPI_HOST_STATUS] |= R_STATUS_RXEMPTY_MASK;
            return rc;
        }

        for (int i = 0; i < 4; ++i) {
            if (fifo8_is_empty(&s->rx_fifo)) {
                /* Assert RXEMPTY, no IRQ */
                s->regs[IBEX_SPI_HOST_STATUS] |= R_STATUS_RXEMPTY_MASK;
                return rc;
            }
            rx_byte = fifo8_pop(&s->rx_fifo);
            rc |= rx_byte << (i * 8);
            //printf("SENDING: %d\n", rx_byte << (i * 8));
        }         
        break;
    case IBEX_SPI_HOST_ERROR_ENABLE...IBEX_SPI_HOST_EVENT_ENABLE:
        rc = s->regs[addr];
        break;       
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "Bad offset 0x%" HWADDR_PRIx "\n",
                      addr << 2);       
    }
    return rc;
}


static void ibex_spi_host_write(void *opaque, hwaddr addr,
                                uint64_t val64, unsigned int size)
{
    IbexSPIHostState *s = opaque;
    uint64_t current_time = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    uint32_t val32 = val64;
    //uint32_t shift_mask = 0xff;
    uint8_t txqd_len;
    //TODO RM:
    //printf("Write Address: 0x%" HWADDR_PRIx ", value: 0x%x\n", addr, val32);
    DB_PRINT("Address: 0x%" HWADDR_PRIx ", value: 0x%x\n", addr, val32);

    if (s == NULL) {
         qemu_log_mask(LOG_GUEST_ERROR,
                        "Null device state");
    }  
    /* Match reg index */
    addr = addr >> 2;
    
    switch (addr) {
    /* Skipping any R/O registers */
    case IBEX_SPI_HOST_INTR_STATE...IBEX_SPI_HOST_INTR_ENABLE:
        s->regs[addr] = val32;
        break;
    case IBEX_SPI_HOST_INTR_TEST:
        s->regs[addr] = val32;
        ibex_spi_host_irq(s);
        break;
    case IBEX_SPI_HOST_ALERT_TEST:
        s->regs[addr] = val32;
        qemu_log_mask(LOG_UNIMP,
                        "%s: SPI_ALERT_TEST is not supported\n", __func__);
        break;
    case IBEX_SPI_HOST_CONTROL:
        s->regs[addr] = val32;

        if (val32 & R_CONTROL_SW_RST_MASK)  {
            //TODO Feature: this could be implemented
            qemu_log_mask(LOG_UNIMP,
                          "%s: SW_RST is not supported\n", __func__);           
        }
        if (val32 & R_CONTROL_OUTPUT_EN_MASK)  {
            qemu_log_mask(LOG_UNIMP,
                          "%s: CONTROL_OUTPUT_EN is not supported\n", __func__);           
        }
        break;
    case IBEX_SPI_HOST_CONFIGOPTS:
        /* Update the respective config-opts register based on CSIDth index */
        s->config_opts[s->regs[IBEX_SPI_HOST_CSID]] = val32;  
        qemu_log_mask(LOG_UNIMP,
                      "%s: CONFIGOPTS Hardware settings not supported\n", __func__);           
        break;
    case IBEX_SPI_HOST_CSID:
        if (val32 >= s->num_cs) {
            /* CSID exceeds max num_cs */
            printf("QEMU: Setting CSID [%d]\n", val32);
            s->regs[IBEX_SPI_HOST_ERROR_STATUS] |= R_ERROR_STATUS_CSIDINVAL_MASK;
            ibex_spi_host_irq(s);
            return;
        }
        s->regs[addr] = val32;
        break;
    case IBEX_SPI_HOST_COMMAND:
        s->regs[addr] = val32;
        /* SPI not ready, IRQ Error */
        if (!(s->regs[IBEX_SPI_HOST_STATUS] & R_STATUS_READY_MASK)) {
            s->regs[IBEX_SPI_HOST_ERROR_STATUS] |= R_ERROR_STATUS_CMDBUSY_MASK;
            ibex_spi_host_irq(s);
            return;    
        }
        /* Assert Not Ready */
        s->regs[IBEX_SPI_HOST_STATUS] &= ~R_STATUS_READY_MASK;

        /* Set Transfer Callback */
        timer_mod(s->fifo_trigger_handle, current_time +
            (TX_INTERRUPT_TRIGGER_DELAY_NS * 2));

        if (((val32 & R_COMMAND_DIRECTION_MASK) >> R_COMMAND_DIRECTION_SHIFT) != BIDIRECTIONAL_TRANSFER) {
             qemu_log_mask(LOG_UNIMP,
                          "%s: Rx Only/Tx Only are not supported\n", __func__);            
        } 

        if (val32 & R_COMMAND_CSAAT_MASK)  {
            qemu_log_mask(LOG_UNIMP,
                          "%s: CSAAT is not supported\n", __func__);           
        }
        if (val32 & R_COMMAND_SPEED_MASK)  {
            qemu_log_mask(LOG_UNIMP,
                          "%s: SPEED is not supported\n", __func__);           
        }
        break;
    case IBEX_SPI_HOST_TXDATA:
        /* Attempting to write when TXFULL */
        if (fifo8_is_full(&s->tx_fifo)) {
            s->regs[IBEX_SPI_HOST_STATUS] |= R_STATUS_TXFULL_MASK;
            ibex_spi_host_irq(s);
            return;
        }
        //TODO Size based on sizearg 32bit..
        fifo8_push(&s->tx_fifo, (uint8_t)val32);

        /* Reset TXEMPTY */
        s->regs[IBEX_SPI_HOST_STATUS] &= ~R_STATUS_TXEMPTY_MASK;
        /* Update TXQD */
        txqd_len = (s->regs[IBEX_SPI_HOST_STATUS] & R_STATUS_TXQD_MASK) >> R_STATUS_TXQD_SHIFT;
        txqd_len++;
        s->regs[IBEX_SPI_HOST_STATUS] &= ~R_STATUS_TXQD_MASK;
        s->regs[IBEX_SPI_HOST_STATUS] |= txqd_len;
        /* Assert Ready */
        s->regs[IBEX_SPI_HOST_STATUS] |= R_STATUS_READY_MASK;
        break;
    case IBEX_SPI_HOST_ERROR_ENABLE:
        s->regs[addr] = val32;

        if (val32 & R_ERROR_ENABLE_OVERFLOW_MASK)  {
            qemu_log_mask(LOG_UNIMP,
                          "%s: OVERFLOW is not supported\n", __func__);           
        }

        if (val32 & R_ERROR_ENABLE_UNDERFLOW_MASK)  {
            qemu_log_mask(LOG_UNIMP,
                          "%s: UNDERFLOW is not supported\n", __func__);           
        }
        if (val32 & R_ERROR_ENABLE_CMDINVAL_MASK)  {
            //TODO: Could be added in
            qemu_log_mask(LOG_UNIMP,
                          "%s: Segment Length is not supported\n", __func__);           
        }
        break;
    case IBEX_SPI_HOST_ERROR_STATUS:
    /*  Indicates that any errors that have occurred. 
     *  When an error occurs, the corresponding bit must be cleared 
     *  here before issuing any further commands 
     */
        s->regs[addr] = val32;
        break;
    case IBEX_SPI_HOST_EVENT_ENABLE:
    /* Controls which classes of SPI events raise an interrupt. */
        s->regs[addr] = val32;
        
        if (val32 & R_EVENT_ENABLE_RXWM_MASK)  {
            qemu_log_mask(LOG_UNIMP,
                          "%s: RXWM is not supported\n", __func__);           
        }
        if (val32 & R_EVENT_ENABLE_TXWM_MASK)  {
            qemu_log_mask(LOG_UNIMP,
                          "%s: TXWM is not supported\n", __func__);           
        }

        if (val32 & R_EVENT_ENABLE_IDLE_MASK)  {
            qemu_log_mask(LOG_UNIMP,
                          "%s: IDLE is not supported\n", __func__);           
        }
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "Bad offset 0x%" HWADDR_PRIx "\n",
                      addr << 2);            
    }
}

static const MemoryRegionOps ibex_spi_ops = {
    .read = ibex_spi_host_read,
    .write = ibex_spi_host_write,
    //TODO SPI: Verify This
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static Property ibex_spi_properties[] = {
    DEFINE_PROP_UINT8("num_cs", IbexSPIHostState, num_cs, 1),
    DEFINE_PROP_END_OF_LIST(),
};

//TODO Configopts[numCS]

static const VMStateDescription vmstate_ibex = {
    .name = TYPE_IBEX_SPI_HOST,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        /* DO */
        VMSTATE_TIMER_PTR(fifo_trigger_handle, IbexSPIHostState),
        VMSTATE_END_OF_LIST()
    }
};

static void fifo_trigger_update(void *opaque)
{
    IbexSPIHostState *s = opaque;
    ibex_spi_host_transfer(s);
}

static void ibex_spi_host_realize(DeviceState *dev, Error **errp)
{
    IbexSPIHostState *s = IBEX_SPI_HOST(dev);
    int i;

    s->ssi = ssi_create_bus(dev, "ssi");

    s->cs_lines = g_new0(qemu_irq, s->num_cs);
    
    for (i = 0; i < s->num_cs; ++i) {
        sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->cs_lines[i]);
    }

    /* Setup CONFIGOPTS Multi-register */
    s->config_opts = malloc(sizeof(uint32_t) * s->num_cs);

    /* Setup FIFO Interrupt Timer */
    s->fifo_trigger_handle = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                          fifo_trigger_update, s);

    /* FIFO sizes as per OT Spec */
    fifo8_create(&s->tx_fifo, IBEX_SPI_HOST_TXFIFO_LEN);
    fifo8_create(&s->rx_fifo, IBEX_SPI_HOST_RXFIFO_LEN);
}

static void ibex_spi_host_init(Object *obj)
{
    IbexSPIHostState *s = IBEX_SPI_HOST(obj);
    //TODO RM:
    //printf("QEMU: SPI INIT\n");

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->host_err);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->event);
    
    memory_region_init_io(&s->mmio, obj, &ibex_spi_ops, s,
                          TYPE_IBEX_SPI_HOST, 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static void ibex_spi_host_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = ibex_spi_host_realize;
    dc->reset = ibex_spi_host_reset;
    dc->vmsd = &vmstate_ibex;
    device_class_set_props(dc, ibex_spi_properties);
}

static const TypeInfo ibex_spi_host_info = {
    .name          = TYPE_IBEX_SPI_HOST,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IbexSPIHostState),
    .instance_init = ibex_spi_host_init,
    .class_init    = ibex_spi_host_class_init,
};

static void ibex_spi_host_register_types(void)
{
    type_register_static(&ibex_spi_host_info);
}

type_init(ibex_spi_host_register_types)