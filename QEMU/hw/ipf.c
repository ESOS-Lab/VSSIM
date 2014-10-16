/*
 * Itanium Platform Emulator derived from QEMU PC System Emulator
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 *
 * Copyright (c) 2007 Intel
 * Ported for IA64 Platform Zhang Xiantao <xiantao.zhang@intel.com>
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

#include "hw.h"
#include "pc.h"
#include "fdc.h"
#include "pci.h"
#include "block.h"
#include "sysemu.h"
#include "audio/audio.h"
#include "net.h"
#include "smbus.h"
#include "boards.h"
#include "firmware.h"
#include "ia64intrin.h"
#include <unistd.h>
#include "device-assignment.h"
#include "virtio-blk.h"

#include "qemu-kvm.h"

#define FW_FILENAME "Flash.fd"

/* Leave a chunk of memory at the top of RAM for the BIOS ACPI tables.  */
#define ACPI_DATA_SIZE       0x10000

#define MAX_IDE_BUS 2

static fdctrl_t *floppy_controller;
static RTCState *rtc_state;
static PCIDevice *i440fx_state;

static uint32_t ipf_to_legacy_io(target_phys_addr_t addr)
{
    return (uint32_t)(((addr&0x3ffffff) >> 12 << 2)|((addr) & 0x3));
}

static void ipf_legacy_io_writeb(void *opaque, target_phys_addr_t addr,
				 uint32_t val) {
    uint32_t port = ipf_to_legacy_io(addr);

    cpu_outb(0, port, val);
}

static void ipf_legacy_io_writew(void *opaque, target_phys_addr_t addr,
				 uint32_t val) {
    uint32_t port = ipf_to_legacy_io(addr);

    cpu_outw(0, port, val);
}

static void ipf_legacy_io_writel(void *opaque, target_phys_addr_t addr,
				 uint32_t val) {
    uint32_t port = ipf_to_legacy_io(addr);

    cpu_outl(0, port, val);
}

static uint32_t ipf_legacy_io_readb(void *opaque, target_phys_addr_t addr)
{
    uint32_t port = ipf_to_legacy_io(addr);

    return cpu_inb(0, port);
}

static uint32_t ipf_legacy_io_readw(void *opaque, target_phys_addr_t addr)
{
    uint32_t port = ipf_to_legacy_io(addr);

    return cpu_inw(0, port);
}

static uint32_t ipf_legacy_io_readl(void *opaque, target_phys_addr_t addr)
{
    uint32_t port = ipf_to_legacy_io(addr);

    return cpu_inl(0, port);
}

static CPUReadMemoryFunc *ipf_legacy_io_read[3] = {
    ipf_legacy_io_readb,
    ipf_legacy_io_readw,
    ipf_legacy_io_readl,
};

static CPUWriteMemoryFunc *ipf_legacy_io_write[3] = {
    ipf_legacy_io_writeb,
    ipf_legacy_io_writew,
    ipf_legacy_io_writel,
};

static void pic_irq_request(void *opaque, int irq, int level)
{
    fprintf(stderr,"pic_irq_request called!\n");
}

/* PC cmos mappings */

#define REG_EQUIPMENT_BYTE          0x14

static int cmos_get_fd_drive_type(int fd0)
{
    int val;

    switch (fd0) {
    case 0:
        /* 1.44 Mb 3"5 drive */
        val = 4;
        break;
    case 1:
        /* 2.88 Mb 3"5 drive */
        val = 5;
        break;
    case 2:
        /* 1.2 Mb 5"5 drive */
        val = 2;
        break;
    default:
        val = 0;
        break;
    }
    return val;
}

static void cmos_init_hd(int type_ofs, int info_ofs, BlockDriverState *hd)
{
    RTCState *s = rtc_state;
    int cylinders, heads, sectors;

    bdrv_get_geometry_hint(hd, &cylinders, &heads, &sectors);
    rtc_set_memory(s, type_ofs, 47);
    rtc_set_memory(s, info_ofs, cylinders);
    rtc_set_memory(s, info_ofs + 1, cylinders >> 8);
    rtc_set_memory(s, info_ofs + 2, heads);
    rtc_set_memory(s, info_ofs + 3, 0xff);
    rtc_set_memory(s, info_ofs + 4, 0xff);
    rtc_set_memory(s, info_ofs + 5, 0xc0 | ((heads > 8) << 3));
    rtc_set_memory(s, info_ofs + 6, cylinders);
    rtc_set_memory(s, info_ofs + 7, cylinders >> 8);
    rtc_set_memory(s, info_ofs + 8, sectors);
}

/* convert boot_device letter to something recognizable by the bios */
static int boot_device2nibble(char boot_device)
{
    switch(boot_device) {
    case 'a':
    case 'b':
        return 0x01; /* floppy boot */
    case 'c':
        return 0x02; /* hard drive boot */
    case 'd':
        return 0x03; /* CD-ROM boot */
    case 'n':
        return 0x04; /* Network boot */
    }
    return 0;
}

/* hd_table must contain 4 block drivers */
static void cmos_init(ram_addr_t ram_size, ram_addr_t above_4g_mem_size,
                      const char *boot_device, BlockDriverState **hd_table)
{
    RTCState *s = rtc_state;
    int nbds, bds[3] = { 0, };
    int val;
    int fd0, fd1, nb;
    int i;

    /* various important CMOS locations needed by PC/Bochs bios */

    /* memory size */
    val = 640; /* base memory in K */
    rtc_set_memory(s, 0x15, val);
    rtc_set_memory(s, 0x16, val >> 8);

    val = (ram_size / 1024) - 1024;
    if (val > 65535)
        val = 65535;
    rtc_set_memory(s, 0x17, val);
    rtc_set_memory(s, 0x18, val >> 8);
    rtc_set_memory(s, 0x30, val);
    rtc_set_memory(s, 0x31, val >> 8);

    if (above_4g_mem_size) {
        rtc_set_memory(s, 0x5b, (unsigned int)above_4g_mem_size >> 16);
        rtc_set_memory(s, 0x5c, (unsigned int)above_4g_mem_size >> 24);
        rtc_set_memory(s, 0x5d, above_4g_mem_size >> 32);
    }
    rtc_set_memory(s, 0x5f, smp_cpus - 1);

    if (ram_size > (16 * 1024 * 1024))
        val = (ram_size / 65536) - ((16 * 1024 * 1024) / 65536);
    else
        val = 0;
    if (val > 65535)
        val = 65535;
    rtc_set_memory(s, 0x34, val);
    rtc_set_memory(s, 0x35, val >> 8);

    /* set boot devices, and disable floppy signature check if requested */
#define PC_MAX_BOOT_DEVICES 3
    nbds = strlen(boot_device);

    if (nbds > PC_MAX_BOOT_DEVICES) {
        fprintf(stderr, "Too many boot devices for PC\n");
        exit(1);
    }

    for (i = 0; i < nbds; i++) {
        bds[i] = boot_device2nibble(boot_device[i]);
        if (bds[i] == 0) {
            fprintf(stderr, "Invalid boot device for PC: '%c'\n",
                    boot_device[i]);
            exit(1);
        }
    }

    rtc_set_memory(s, 0x3d, (bds[1] << 4) | bds[0]);
    rtc_set_memory(s, 0x38, (bds[2] << 4) | (fd_bootchk ?  0x0 : 0x1));

    /* floppy type */

    fd0 = fdctrl_get_drive_type(floppy_controller, 0);
    fd1 = fdctrl_get_drive_type(floppy_controller, 1);

    val = (cmos_get_fd_drive_type(fd0) << 4) | cmos_get_fd_drive_type(fd1);
    rtc_set_memory(s, 0x10, val);

    val = 0;
    nb = 0;
    if (fd0 < 3)
        nb++;
    if (fd1 < 3)
        nb++;

    switch (nb) {
    case 0:
        break;
    case 1:
        val |= 0x01; /* 1 drive, ready for boot */
        break;
    case 2:
        val |= 0x41; /* 2 drives, ready for boot */
        break;
    }

    val |= 0x02; /* FPU is there */
    val |= 0x04; /* PS/2 mouse installed */
    rtc_set_memory(s, REG_EQUIPMENT_BYTE, val);

    /* hard drives */

    rtc_set_memory(s, 0x12, (hd_table[0] ? 0xf0 : 0) | (hd_table[1] ? 0x0f : 0));
    if (hd_table[0])
        cmos_init_hd(0x19, 0x1b, hd_table[0]);
    if (hd_table[1])
        cmos_init_hd(0x1a, 0x24, hd_table[1]);

    val = 0;
    for (i = 0; i < 4; i++) {
        if (hd_table[i]) {
            int cylinders, heads, sectors, translation;
            /* NOTE: bdrv_get_geometry_hint() returns the physical
               geometry.  It is always such that: 1 <= sects <= 63, 1
               <= heads <= 16, 1 <= cylinders <= 16383. The BIOS
               geometry can be different if a translation is done. */
            translation = bdrv_get_translation_hint(hd_table[i]);
            if (translation == BIOS_ATA_TRANSLATION_AUTO) {
                bdrv_get_geometry_hint(hd_table[i], &cylinders,
                                       &heads, &sectors);
                if (cylinders <= 1024 && heads <= 16 && sectors <= 63) {
                    /* No translation. */
                    translation = 0;
                } else {
                    /* LBA translation. */
                    translation = 1;
                }
            } else {
                translation--;
            }
            val |= translation << (i * 2);
        }
    }
    rtc_set_memory(s, 0x39, val);
}

static void main_cpu_reset(void *opaque)
{
    CPUState *env = opaque;
    cpu_reset(env);
}

static const int ide_iobase[2] = { 0x1f0, 0x170 };
static const int ide_iobase2[2] = { 0x3f6, 0x376 };
static const int ide_irq[2] = { 14, 15 };

#define NE2000_NB_MAX 6

static int ne2000_io[NE2000_NB_MAX] = { 0x300, 0x320, 0x340,
                                        0x360, 0x280, 0x380 };
static int ne2000_irq[NE2000_NB_MAX] = { 9, 10, 11, 3, 4, 5 };

static int serial_io[MAX_SERIAL_PORTS] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };
static int serial_irq[MAX_SERIAL_PORTS] = { 4, 3, 4, 3 };

static int parallel_io[MAX_PARALLEL_PORTS] = { 0x378, 0x278, 0x3bc };
static int parallel_irq[MAX_PARALLEL_PORTS] = { 7, 7, 7 };

#ifdef HAS_AUDIO
static void audio_init (PCIBus *pci_bus, qemu_irq *pic)
{
    struct soundhw *c;
    int audio_enabled = 0;

    for (c = soundhw; !audio_enabled && c->name; ++c) {
        audio_enabled = c->enabled;
    }

    if (audio_enabled) {
        AudioState *s;

        s = AUD_init ();
        if (s) {
            for (c = soundhw; c->name; ++c) {
                if (c->enabled) {
                    if (c->isa) {
                        c->init.init_isa (s, pic);
                    } else {
                        if (pci_bus) {
                            c->init.init_pci (pci_bus, s);
                        }
                    }
                }
            }
        }
    }
}
#endif

static void pc_init_ne2k_isa(NICInfo *nd, qemu_irq *pic)
{
    static int nb_ne2k = 0;

    if (nb_ne2k == NE2000_NB_MAX)
        return;
    isa_ne2000_init(ne2000_io[nb_ne2k], pic[ne2000_irq[nb_ne2k]], nd);
    nb_ne2k++;
}

/* Itanium hardware initialisation */
static void ipf_init1(ram_addr_t ram_size,
                      const char *boot_device, DisplayState *ds,
                      const char *kernel_filename, const char *kernel_cmdline,
                      const char *initrd_filename,
                      int pci_enabled, const char *cpu_model)
{
    char buf[1024];
    int i;
    ram_addr_t ram_addr;
    ram_addr_t above_4g_mem_size = 0;
    PCIBus *pci_bus;
    PCIDevice *pci_dev;
    int piix3_devfn = -1;
    CPUState *env;
    qemu_irq *cpu_irq;
    qemu_irq *i8259;
    int page_size;
    int index;
    unsigned long ipf_legacy_io_base, ipf_legacy_io_mem;
    BlockDriverState *hd[MAX_IDE_BUS * MAX_IDE_DEVS];
    BlockDriverState *fd[MAX_FD];

    page_size = getpagesize();
    if (page_size != TARGET_PAGE_SIZE) {
	fprintf(stderr,"Error! Host page size != qemu target page size,"
                " you may need to change TARGET_PAGE_BITS in qemu!"
                "host page size:0x%x\n", page_size);
        exit(-1);
    };

    if (ram_size >= 0xc0000000 ) {
        above_4g_mem_size = ram_size - 0xc0000000;
        ram_size = 0xc0000000;
    }

    /* init CPUs */
    if (cpu_model == NULL) {
        cpu_model = "IA64";
    }

    for(i = 0; i < smp_cpus; i++) {
        env = cpu_init(cpu_model);
        if (!env) {
            fprintf(stderr, "Unable to find CPU definition\n");
            exit(1);
        }
        if (i != 0)
            env->hflags |= HF_HALTED_MASK;
        register_savevm("cpu", i, 4, cpu_save, cpu_load, env);
        qemu_register_reset(main_cpu_reset, 0, env);
    }

    /* allocate RAM */
    if (kvm_enabled()) {
        ram_addr = qemu_ram_alloc(0xa0000);
        cpu_register_physical_memory(0, 0xa0000, ram_addr);

        ram_addr = qemu_ram_alloc(0x20000); // Workaround 0xa0000-0xc0000

        ram_addr = qemu_ram_alloc(0x40000);
        cpu_register_physical_memory(0xc0000, 0x40000, ram_addr);

        ram_addr = qemu_ram_alloc(ram_size - 0x100000);
        cpu_register_physical_memory(0x100000, ram_size - 0x100000, ram_addr);
    } else {
        ram_addr = qemu_ram_alloc(ram_size);
        cpu_register_physical_memory(0, ram_size, ram_addr);
    }

    /* above 4giga memory allocation */
    if (above_4g_mem_size > 0) {
        ram_addr = qemu_ram_alloc(above_4g_mem_size);
        cpu_register_physical_memory(0x100000000, above_4g_mem_size, ram_addr);
    }

    /*Load firware to its proper position.*/
    if (kvm_enabled()) {
        unsigned long  image_size;
        uint8_t *image = NULL;
        unsigned long nvram_addr;
        unsigned long nvram_fd = 0;
        unsigned long type = READ_FROM_NVRAM;
        unsigned long i = 0;
        unsigned long fw_offset;
        ram_addr_t fw_mem = qemu_ram_alloc(GFW_SIZE);

        snprintf(buf, sizeof(buf), "%s/%s", bios_dir, FW_FILENAME);
        image = read_image(buf, &image_size );
        if (NULL == image || !image_size) {
            fprintf(stderr, "Error when reading Guest Firmware!\n");
            fprintf(stderr, "Please check Guest firmware at %s\n", buf);
            exit(1);
        }
        fw_offset = GFW_START + GFW_SIZE - image_size;

        cpu_register_physical_memory(GFW_START, GFW_SIZE, fw_mem);
        cpu_physical_memory_write(fw_offset, image, image_size);

        free(image);

        if (nvram) {
            nvram_addr = NVRAM_START;
            nvram_fd = kvm_ia64_nvram_init(type);
            if (nvram_fd != -1) {
                kvm_ia64_copy_from_nvram_to_GFW(nvram_fd);
                close(nvram_fd);
            }
            i = atexit((void *)kvm_ia64_copy_from_GFW_to_nvram);
            if (i != 0)
                fprintf(stderr, "cannot set exit function\n");
        } else
            nvram_addr = 0;

        kvm_ia64_build_hob(ram_size + above_4g_mem_size, smp_cpus, nvram_addr);
    }

    /*Register legacy io address space, size:64M*/
    ipf_legacy_io_base = 0xE0000000;
    ipf_legacy_io_mem = cpu_register_io_memory(0, ipf_legacy_io_read,
                                               ipf_legacy_io_write, NULL);
    cpu_register_physical_memory(ipf_legacy_io_base, 64*1024*1024,
                                 ipf_legacy_io_mem);

    cpu_irq = qemu_allocate_irqs(pic_irq_request, first_cpu, 1);
    i8259 = kvm_i8259_init(cpu_irq[0]);

    if (pci_enabled) {
        pci_bus = i440fx_init(&i440fx_state, i8259);
        piix3_devfn = piix3_init(pci_bus, -1);
    } else {
        pci_bus = NULL;
    }

    if (cirrus_vga_enabled) {
        if (pci_enabled)
            pci_cirrus_vga_init(pci_bus);
        else
            isa_cirrus_vga_init();
    } else {
        if (pci_enabled)
            pci_vga_init(pci_bus, 0, 0);
        else
            isa_vga_init();
    }

    rtc_state = rtc_init(0x70, i8259[8], 2000);

    if (pci_enabled) {
        pic_set_alt_irq_func(isa_pic, NULL, NULL);
    }

    for(i = 0; i < MAX_SERIAL_PORTS; i++) {
        if (serial_hds[i]) {
            serial_init(serial_io[i], i8259[serial_irq[i]], 115200,
                        serial_hds[i]);
        }
    }

    for(i = 0; i < MAX_PARALLEL_PORTS; i++) {
        if (parallel_hds[i]) {
            parallel_init(parallel_io[i], i8259[parallel_irq[i]],
                          parallel_hds[i]);
        }
    }

    for(i = 0; i < nb_nics; i++) {
        NICInfo *nd = &nd_table[i];

        if (!pci_enabled || (nd->model && strcmp(nd->model, "ne2k_isa") == 0))
            pc_init_ne2k_isa(nd, i8259);
        else
            pci_nic_init(nd, "e1000", NULL);
    }

#undef USE_HYPERCALL  //Disable it now, need to implement later!
#ifdef USE_HYPERCALL
    pci_hypercall_init(pci_bus);
#endif

    if (drive_get_max_bus(IF_IDE) >= MAX_IDE_BUS) {
        fprintf(stderr, "qemu: too many IDE bus\n");
        exit(1);
    }

    for(i = 0; i < MAX_IDE_BUS * MAX_IDE_DEVS; i++) {
        index = drive_get_index(IF_IDE, i / MAX_IDE_DEVS, i % MAX_IDE_DEVS);
	if (index != -1)
	    hd[i] = drives_table[index].bdrv;
	else
	    hd[i] = NULL;
    }

    if (pci_enabled) {
        pci_piix3_ide_init(pci_bus, hd, piix3_devfn + 1, i8259);
    } else {
        for(i = 0; i < MAX_IDE_BUS; i++) {
            isa_ide_init(ide_iobase[i], ide_iobase2[i], i8259[ide_irq[i]],
	                 hd[MAX_IDE_DEVS * i], hd[MAX_IDE_DEVS * i + 1]);
        }
    }

    i8042_init(i8259[1], i8259[12], 0x60);
    DMA_init(0);
#ifdef HAS_AUDIO
    audio_init(pci_enabled ? pci_bus : NULL, i8259);
#endif

    for(i = 0; i < MAX_FD; i++) {
        index = drive_get_index(IF_FLOPPY, 0, i);
	if (index != -1)
	    fd[i] = drives_table[index].bdrv;
	else
	    fd[i] = NULL;
    }
    floppy_controller = fdctrl_init(i8259[6], 2, 0, 0x3f0, fd);

    cmos_init(ram_size, above_4g_mem_size, boot_device, hd);

    if (pci_enabled && usb_enabled) {
        usb_uhci_piix3_init(pci_bus, piix3_devfn + 2);
    }

    if (pci_enabled && acpi_enabled) {
        uint8_t *eeprom_buf = qemu_mallocz(8 * 256); /* XXX: make this persistent */
        i2c_bus *smbus;

        /* TODO: Populate SPD eeprom data.  */
        smbus = piix4_pm_init(pci_bus, piix3_devfn + 3, 0xb100, i8259[9]);
        for (i = 0; i < 8; i++) {
            DeviceState *eeprom;
            eeprom = qdev_create((BusState *)smbus, "smbus-eeprom");
            qdev_set_prop_int(eeprom, "address", 0x50 + i);
            qdev_set_prop_ptr(eeprom, "data", eeprom_buf + (i * 256));
            qdev_init(eeprom);
        }
    }

    if (i440fx_state) {
        i440fx_init_memory_mappings(i440fx_state);
    }

    if (pci_enabled) {
	int max_bus;
        int bus;

        max_bus = drive_get_max_bus(IF_SCSI);
	for (bus = 0; bus <= max_bus; bus++) {
            pci_create_simple(pci_bus, -1, "lsi53c895a");
        }
    }
    /* Add virtio block devices */
    if (pci_enabled) {
	int index;
	int unit_id = 0;

	while ((index = drive_get_index(IF_VIRTIO, 0, unit_id)) != -1) {
            pci_dev = pci_create("virtio-blk-pci",
                                 drives_table[index].devaddr);
            qdev_init(&pci_dev->qdev);
	    unit_id++;
	}
    }

#ifdef USE_KVM_DEVICE_ASSIGNMENT
    if (kvm_enabled())
	add_assigned_devices(pci_bus, assigned_devices, assigned_devices_index);
#endif /* USE_KVM_DEVICE_ASSIGNMENT */

}

static void ipf_init_pci(ram_addr_t ram_size,
                         const char *boot_device, DisplayState *ds,
                         const char *kernel_filename,
                         const char *kernel_cmdline,
                         const char *initrd_filename,
                         const char *cpu_model)
{
    ipf_init1(ram_size, boot_device, ds, kernel_filename,
              kernel_cmdline, initrd_filename, 1, cpu_model);
}

QEMUMachine ipf_machine = {
    .name = "itanium",
    .desc = "Itanium Platform",
    .init = (QEMUMachineInitFunc *)ipf_init_pci,
    .max_cpus = 255,
    .is_default = 1,
};

static void ipf_machine_init(void)
{
    qemu_register_machine(&ipf_machine);
}

machine_init(ipf_machine_init);

#define IOAPIC_NUM_PINS 48

static int ioapic_irq_count[IOAPIC_NUM_PINS];

static int ioapic_map_irq(int devfn, int irq_num)
{
    int irq, dev;
    dev = devfn >> 3;
    irq = ((((dev << 2) + (dev >> 3) + irq_num) & 31) + 16);
    return irq;
}

/*
 * Dummy function to provide match for call from hw/apic.c
 */
void apic_set_irq_delivered(void) {
}

void ioapic_set_irq(void *opaque, int irq_num, int level)
{
    int vector, pic_ret;

    PCIDevice *pci_dev = (PCIDevice *)opaque;
    vector = ioapic_map_irq(pci_dev->devfn, irq_num);

    if (level)
        ioapic_irq_count[vector] += 1;
    else
        ioapic_irq_count[vector] -= 1;

    if (kvm_enabled()) {
	if (kvm_set_irq(vector, ioapic_irq_count[vector] == 0, &pic_ret))
            if (pic_ret != 0)
                apic_set_irq_delivered();
	    return;
    }
}

int ipf_map_irq(PCIDevice *pci_dev, int irq_num)
{
	return ioapic_map_irq(pci_dev->devfn, irq_num);
}
