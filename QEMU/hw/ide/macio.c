/*
 * QEMU IDE Emulation: MacIO support.
 *
 * Copyright (c) 2003 Fabrice Bellard
 * Copyright (c) 2006 Openedhand Ltd.
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
#include "hw/hw.h"
#include "hw/ppc/mac.h"
#include "hw/ppc/mac_dbdma.h"
#include "sysemu/block-backend.h"
#include "sysemu/dma.h"

#include "hw/ide/internal.h"

/* debug MACIO */
// #define DEBUG_MACIO

#ifdef DEBUG_MACIO
static const int debug_macio = 1;
#else
static const int debug_macio = 0;
#endif

#define MACIO_DPRINTF(fmt, ...) do { \
        if (debug_macio) { \
            printf(fmt , ## __VA_ARGS__); \
        } \
    } while (0)


/***********************************************************/
/* MacIO based PowerPC IDE */

#define MACIO_PAGE_SIZE 4096

static void pmac_ide_atapi_transfer_cb(void *opaque, int ret)
{
    DBDMA_io *io = opaque;
    MACIOIDEState *m = io->opaque;
    IDEState *s = idebus_active_if(&m->bus);
    int64_t offset;

    MACIO_DPRINTF("pmac_ide_atapi_transfer_cb\n");

    if (ret < 0) {
        MACIO_DPRINTF("DMA error: %d\n", ret);
        qemu_sglist_destroy(&s->sg);
        ide_atapi_io_error(s, ret);
        goto done;
    }

    if (!m->dma_active) {
        MACIO_DPRINTF("waiting for data (%#x - %#x - %x)\n",
                      s->nsector, io->len, s->status);
        /* data not ready yet, wait for the channel to get restarted */
        io->processing = false;
        return;
    }

    if (s->io_buffer_size <= 0) {
        MACIO_DPRINTF("End of IDE transfer\n");
        qemu_sglist_destroy(&s->sg);
        ide_atapi_cmd_ok(s);
        m->dma_active = false;
        goto done;
    }

    if (io->len == 0) {
        MACIO_DPRINTF("End of DMA transfer\n");
        goto done;
    }

    if (s->lba == -1) {
        /* Non-block ATAPI transfer - just copy to RAM */
        s->io_buffer_size = MIN(s->io_buffer_size, io->len);
        dma_memory_write(&address_space_memory, io->addr, s->io_buffer,
                         s->io_buffer_size);
        io->len = 0;
        ide_atapi_cmd_ok(s);
        m->dma_active = false;
        goto done;
    }

    /* Calculate current offset */
    offset = ((int64_t)s->lba << 11) + s->io_buffer_index;

    qemu_sglist_init(&s->sg, DEVICE(m), io->len / MACIO_PAGE_SIZE + 1,
                     &address_space_memory);
    qemu_sglist_add(&s->sg, io->addr, io->len);
    s->io_buffer_size -= io->len;
    s->io_buffer_index += io->len;
    io->len = 0;

    s->bus->dma->aiocb = dma_blk_read(s->blk, &s->sg, offset, 0x1,
                                      pmac_ide_atapi_transfer_cb, io);
    return;

done:
    dma_memory_unmap(&address_space_memory, io->dma_mem, io->dma_len,
                     io->dir, io->dma_len);

    if (ret < 0) {
        block_acct_failed(blk_get_stats(s->blk), &s->acct);
    } else {
        block_acct_done(blk_get_stats(s->blk), &s->acct);
    }

    ide_set_inactive(s, false);
    io->dma_end(opaque);
}

static void pmac_ide_transfer_cb(void *opaque, int ret)
{
    DBDMA_io *io = opaque;
    MACIOIDEState *m = io->opaque;
    IDEState *s = idebus_active_if(&m->bus);
    int64_t offset;

    MACIO_DPRINTF("pmac_ide_transfer_cb\n");

    if (ret < 0) {
        MACIO_DPRINTF("DMA error: %d\n", ret);
        qemu_sglist_destroy(&s->sg);
        ide_dma_error(s);
        goto done;
    }

    if (!m->dma_active) {
        MACIO_DPRINTF("waiting for data (%#x - %#x - %x)\n",
                      s->nsector, io->len, s->status);
        /* data not ready yet, wait for the channel to get restarted */
        io->processing = false;
        return;
    }

    if (s->io_buffer_size <= 0) {
        MACIO_DPRINTF("End of IDE transfer\n");
        qemu_sglist_destroy(&s->sg);
        s->status = READY_STAT | SEEK_STAT;
        ide_set_irq(s->bus);
        m->dma_active = false;
        goto done;
    }

    if (io->len == 0) {
        MACIO_DPRINTF("End of DMA transfer\n");
        goto done;
    }

    /* Calculate number of sectors */
    offset = (ide_get_sector(s) << 9) + s->io_buffer_index;

    qemu_sglist_init(&s->sg, DEVICE(m), io->len / MACIO_PAGE_SIZE + 1,
                     &address_space_memory);
    qemu_sglist_add(&s->sg, io->addr, io->len);
    s->io_buffer_size -= io->len;
    s->io_buffer_index += io->len;
    io->len = 0;

    switch (s->dma_cmd) {
    case IDE_DMA_READ:
        s->bus->dma->aiocb = dma_blk_read(s->blk, &s->sg, offset, 0x1,
                                          pmac_ide_atapi_transfer_cb, io);
        break;
    case IDE_DMA_WRITE:
        s->bus->dma->aiocb = dma_blk_write(s->blk, &s->sg, offset, 0x1,
                                           pmac_ide_transfer_cb, io);
        break;
    case IDE_DMA_TRIM:
        s->bus->dma->aiocb = dma_blk_io(blk_get_aio_context(s->blk), &s->sg,
                                        offset, 0x1, ide_issue_trim, s->blk,
                                        pmac_ide_transfer_cb, io,
                                        DMA_DIRECTION_TO_DEVICE);
        break;
    default:
        abort();
    }

    return;

done:
    dma_memory_unmap(&address_space_memory, io->dma_mem, io->dma_len,
                     io->dir, io->dma_len);

    if (s->dma_cmd == IDE_DMA_READ || s->dma_cmd == IDE_DMA_WRITE) {
        if (ret < 0) {
            block_acct_failed(blk_get_stats(s->blk), &s->acct);
        } else {
            block_acct_done(blk_get_stats(s->blk), &s->acct);
        }
    }

    ide_set_inactive(s, false);
    io->dma_end(opaque);
}

static void pmac_ide_transfer(DBDMA_io *io)
{
    MACIOIDEState *m = io->opaque;
    IDEState *s = idebus_active_if(&m->bus);

    MACIO_DPRINTF("\n");

    if (s->drive_kind == IDE_CD) {
        block_acct_start(blk_get_stats(s->blk), &s->acct, io->len,
                         BLOCK_ACCT_READ);

        pmac_ide_atapi_transfer_cb(io, 0);
        return;
    }

    switch (s->dma_cmd) {
    case IDE_DMA_READ:
        block_acct_start(blk_get_stats(s->blk), &s->acct, io->len,
                         BLOCK_ACCT_READ);
        break;
    case IDE_DMA_WRITE:
        block_acct_start(blk_get_stats(s->blk), &s->acct, io->len,
                         BLOCK_ACCT_WRITE);
        break;
    default:
        break;
    }

    pmac_ide_transfer_cb(io, 0);
}

static void pmac_ide_flush(DBDMA_io *io)
{
    MACIOIDEState *m = io->opaque;
    IDEState *s = idebus_active_if(&m->bus);

    if (s->bus->dma->aiocb) {
        blk_drain(s->blk);
    }
}

/* PowerMac IDE memory IO */
static void pmac_ide_writeb (void *opaque,
                             hwaddr addr, uint32_t val)
{
    MACIOIDEState *d = opaque;

    addr = (addr & 0xFFF) >> 4;
    switch (addr) {
    case 1 ... 7:
        ide_ioport_write(&d->bus, addr, val);
        break;
    case 8:
    case 22:
        ide_cmd_write(&d->bus, 0, val);
        break;
    default:
        break;
    }
}

static uint32_t pmac_ide_readb (void *opaque,hwaddr addr)
{
    uint8_t retval;
    MACIOIDEState *d = opaque;

    addr = (addr & 0xFFF) >> 4;
    switch (addr) {
    case 1 ... 7:
        retval = ide_ioport_read(&d->bus, addr);
        break;
    case 8:
    case 22:
        retval = ide_status_read(&d->bus, 0);
        break;
    default:
        retval = 0xFF;
        break;
    }
    return retval;
}

static void pmac_ide_writew (void *opaque,
                             hwaddr addr, uint32_t val)
{
    MACIOIDEState *d = opaque;

    addr = (addr & 0xFFF) >> 4;
    val = bswap16(val);
    if (addr == 0) {
        ide_data_writew(&d->bus, 0, val);
    }
}

static uint32_t pmac_ide_readw (void *opaque,hwaddr addr)
{
    uint16_t retval;
    MACIOIDEState *d = opaque;

    addr = (addr & 0xFFF) >> 4;
    if (addr == 0) {
        retval = ide_data_readw(&d->bus, 0);
    } else {
        retval = 0xFFFF;
    }
    retval = bswap16(retval);
    return retval;
}

static void pmac_ide_writel (void *opaque,
                             hwaddr addr, uint32_t val)
{
    MACIOIDEState *d = opaque;

    addr = (addr & 0xFFF) >> 4;
    val = bswap32(val);
    if (addr == 0) {
        ide_data_writel(&d->bus, 0, val);
    }
}

static uint32_t pmac_ide_readl (void *opaque,hwaddr addr)
{
    uint32_t retval;
    MACIOIDEState *d = opaque;

    addr = (addr & 0xFFF) >> 4;
    if (addr == 0) {
        retval = ide_data_readl(&d->bus, 0);
    } else {
        retval = 0xFFFFFFFF;
    }
    retval = bswap32(retval);
    return retval;
}

static const MemoryRegionOps pmac_ide_ops = {
    .old_mmio = {
        .write = {
            pmac_ide_writeb,
            pmac_ide_writew,
            pmac_ide_writel,
        },
        .read = {
            pmac_ide_readb,
            pmac_ide_readw,
            pmac_ide_readl,
        },
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_pmac = {
    .name = "ide",
    .version_id = 4,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        VMSTATE_IDE_BUS(bus, MACIOIDEState),
        VMSTATE_IDE_DRIVES(bus.ifs, MACIOIDEState),
        VMSTATE_BOOL(dma_active, MACIOIDEState),
        VMSTATE_END_OF_LIST()
    }
};

static void macio_ide_reset(DeviceState *dev)
{
    MACIOIDEState *d = MACIO_IDE(dev);

    ide_bus_reset(&d->bus);
}

static int ide_nop_int(IDEDMA *dma, int x)
{
    return 0;
}

static int32_t ide_nop_int32(IDEDMA *dma, int32_t l)
{
    return 0;
}

static void ide_dbdma_start(IDEDMA *dma, IDEState *s,
                            BlockCompletionFunc *cb)
{
    MACIOIDEState *m = container_of(dma, MACIOIDEState, dma);

    s->io_buffer_index = 0;
    if (s->drive_kind == IDE_CD) {
        s->io_buffer_size = s->packet_transfer_size;
    } else {
        s->io_buffer_size = s->nsector * BDRV_SECTOR_SIZE;
    }

    MACIO_DPRINTF("\n\n------------ IDE transfer\n");
    MACIO_DPRINTF("buffer_size: %x   buffer_index: %x\n",
                  s->io_buffer_size, s->io_buffer_index);
    MACIO_DPRINTF("lba: %x    size: %x\n", s->lba, s->io_buffer_size);
    MACIO_DPRINTF("-------------------------\n");

    m->dma_active = true;
    DBDMA_kick(m->dbdma);
}

static const IDEDMAOps dbdma_ops = {
    .start_dma      = ide_dbdma_start,
    .prepare_buf    = ide_nop_int32,
    .rw_buf         = ide_nop_int,
};

static void macio_ide_realizefn(DeviceState *dev, Error **errp)
{
    MACIOIDEState *s = MACIO_IDE(dev);

    ide_init2(&s->bus, s->irq);

    /* Register DMA callbacks */
    s->dma.ops = &dbdma_ops;
    s->bus.dma = &s->dma;
}

static void macio_ide_initfn(Object *obj)
{
    SysBusDevice *d = SYS_BUS_DEVICE(obj);
    MACIOIDEState *s = MACIO_IDE(obj);

    ide_bus_new(&s->bus, sizeof(s->bus), DEVICE(obj), 0, 2);
    memory_region_init_io(&s->mem, obj, &pmac_ide_ops, s, "pmac-ide", 0x1000);
    sysbus_init_mmio(d, &s->mem);
    sysbus_init_irq(d, &s->irq);
    sysbus_init_irq(d, &s->dma_irq);
}

static void macio_ide_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = macio_ide_realizefn;
    dc->reset = macio_ide_reset;
    dc->vmsd = &vmstate_pmac;
    set_bit(DEVICE_CATEGORY_STORAGE, dc->categories);
}

static const TypeInfo macio_ide_type_info = {
    .name = TYPE_MACIO_IDE,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MACIOIDEState),
    .instance_init = macio_ide_initfn,
    .class_init = macio_ide_class_init,
};

static void macio_ide_register_types(void)
{
    type_register_static(&macio_ide_type_info);
}

/* hd_table must contain 2 block drivers */
void macio_ide_init_drives(MACIOIDEState *s, DriveInfo **hd_table)
{
    int i;

    for (i = 0; i < 2; i++) {
        if (hd_table[i]) {
            ide_create_drive(&s->bus, i, hd_table[i]);
        }
    }
}

void macio_ide_register_dma(MACIOIDEState *s, void *dbdma, int channel)
{
    s->dbdma = dbdma;
    DBDMA_register_channel(dbdma, channel, s->dma_irq,
                           pmac_ide_transfer, pmac_ide_flush, s);
}

type_init(macio_ide_register_types)
