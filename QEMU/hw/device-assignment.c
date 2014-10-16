/*
 * Copyright (c) 2007, Neocleus Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 *
 *
 *  Assign a PCI device from the host to a guest VM.
 *
 *  Adapted for KVM by Qumranet.
 *
 *  Copyright (c) 2007, Neocleus, Alex Novik (alex@neocleus.com)
 *  Copyright (c) 2007, Neocleus, Guy Zana (guy@neocleus.com)
 *  Copyright (C) 2008, Qumranet, Amit Shah (amit.shah@qumranet.com)
 *  Copyright (C) 2008, Red Hat, Amit Shah (amit.shah@redhat.com)
 *  Copyright (C) 2008, IBM, Muli Ben-Yehuda (muli@il.ibm.com)
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/io.h>
#include <pci/pci.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "qemu-kvm.h"
#include "hw.h"
#include "pc.h"
#include "sysemu.h"
#include "console.h"
#include "device-assignment.h"

/* From linux/ioport.h */
#define IORESOURCE_IO       0x00000100  /* Resource type */
#define IORESOURCE_MEM      0x00000200
#define IORESOURCE_IRQ      0x00000400
#define IORESOURCE_DMA      0x00000800
#define IORESOURCE_PREFETCH 0x00001000  /* No side effects */

/* #define DEVICE_ASSIGNMENT_DEBUG 1 */

#ifdef DEVICE_ASSIGNMENT_DEBUG
#define DEBUG(fmt, ...)                                       \
    do {                                                      \
      fprintf(stderr, "%s: " fmt, __func__ , __VA_ARGS__);    \
    } while (0)
#else
#define DEBUG(fmt, ...) do { } while(0)
#endif

static uint32_t guest_to_host_ioport(AssignedDevRegion *region, uint32_t addr)
{
    return region->u.r_baseport + (addr - region->e_physbase);
}

static void assigned_dev_ioport_writeb(void *opaque, uint32_t addr,
                                       uint32_t value)
{
    AssignedDevRegion *r_access = opaque;
    uint32_t r_pio = guest_to_host_ioport(r_access, addr);

    DEBUG("r_pio=%08x e_physbase=%08x r_baseport=%08lx value=%08x\n",
	  r_pio, (int)r_access->e_physbase,
	  (unsigned long)r_access->u.r_baseport, value);

    outb(value, r_pio);
}

static void assigned_dev_ioport_writew(void *opaque, uint32_t addr,
                                       uint32_t value)
{
    AssignedDevRegion *r_access = opaque;
    uint32_t r_pio = guest_to_host_ioport(r_access, addr);

    DEBUG("r_pio=%08x e_physbase=%08x r_baseport=%08lx value=%08x\n",
          r_pio, (int)r_access->e_physbase,
	  (unsigned long)r_access->u.r_baseport, value);

    outw(value, r_pio);
}

static void assigned_dev_ioport_writel(void *opaque, uint32_t addr,
                       uint32_t value)
{
    AssignedDevRegion *r_access = opaque;
    uint32_t r_pio = guest_to_host_ioport(r_access, addr);

    DEBUG("r_pio=%08x e_physbase=%08x r_baseport=%08lx value=%08x\n",
	  r_pio, (int)r_access->e_physbase,
          (unsigned long)r_access->u.r_baseport, value);

    outl(value, r_pio);
}

static uint32_t assigned_dev_ioport_readb(void *opaque, uint32_t addr)
{
    AssignedDevRegion *r_access = opaque;
    uint32_t r_pio = guest_to_host_ioport(r_access, addr);
    uint32_t value;

    value = inb(r_pio);

    DEBUG("r_pio=%08x e_physbase=%08x r_=%08lx value=%08x\n",
          r_pio, (int)r_access->e_physbase,
          (unsigned long)r_access->u.r_baseport, value);

    return value;
}

static uint32_t assigned_dev_ioport_readw(void *opaque, uint32_t addr)
{
    AssignedDevRegion *r_access = opaque;
    uint32_t r_pio = guest_to_host_ioport(r_access, addr);
    uint32_t value;

    value = inw(r_pio);

    DEBUG("r_pio=%08x e_physbase=%08x r_baseport=%08lx value=%08x\n",
          r_pio, (int)r_access->e_physbase,
	  (unsigned long)r_access->u.r_baseport, value);

    return value;
}

static uint32_t assigned_dev_ioport_readl(void *opaque, uint32_t addr)
{
    AssignedDevRegion *r_access = opaque;
    uint32_t r_pio = guest_to_host_ioport(r_access, addr);
    uint32_t value;

    value = inl(r_pio);

    DEBUG("r_pio=%08x e_physbase=%08x r_baseport=%08lx value=%08x\n",
          r_pio, (int)r_access->e_physbase,
          (unsigned long)r_access->u.r_baseport, value);

    return value;
}

static void assigned_dev_iomem_map(PCIDevice *pci_dev, int region_num,
                                   uint32_t e_phys, uint32_t e_size, int type)
{
    AssignedDevice *r_dev = container_of(pci_dev, AssignedDevice, dev);
    AssignedDevRegion *region = &r_dev->v_addrs[region_num];
    PCIRegion *real_region = &r_dev->real_device.regions[region_num];
    uint32_t old_ephys = region->e_physbase;
    uint32_t old_esize = region->e_size;
    int first_map = (region->e_size == 0);
    int ret = 0;

    DEBUG("e_phys=%08x r_virt=%p type=%d len=%08x region_num=%d \n",
          e_phys, region->u.r_virtbase, type, e_size, region_num);

    region->e_physbase = e_phys;
    region->e_size = e_size;

    if (!first_map)
	kvm_destroy_phys_mem(kvm_context, old_ephys,
                             TARGET_PAGE_ALIGN(old_esize));

    if (e_size > 0) {
        /* deal with MSI-X MMIO page */
        if (real_region->base_addr <= r_dev->msix_table_addr &&
                real_region->base_addr + real_region->size >=
                r_dev->msix_table_addr) {
            int offset = r_dev->msix_table_addr - real_region->base_addr;
            ret = munmap(region->u.r_virtbase + offset, TARGET_PAGE_SIZE);
            if (ret == 0)
                DEBUG("munmap done, virt_base 0x%p\n",
                        region->u.r_virtbase + offset);
            else {
                fprintf(stderr, "%s: fail munmap msix table!\n", __func__);
                exit(1);
            }
            cpu_register_physical_memory(e_phys + offset,
                    TARGET_PAGE_SIZE, r_dev->mmio_index);
        }
	ret = kvm_register_phys_mem(kvm_context, e_phys,
                                    region->u.r_virtbase,
                                    TARGET_PAGE_ALIGN(e_size), 0);
    }

    if (ret != 0) {
	fprintf(stderr, "%s: Error: create new mapping failed\n", __func__);
	exit(1);
    }
}

static void assigned_dev_ioport_map(PCIDevice *pci_dev, int region_num,
                                    uint32_t addr, uint32_t size, int type)
{
    AssignedDevice *r_dev = container_of(pci_dev, AssignedDevice, dev);
    AssignedDevRegion *region = &r_dev->v_addrs[region_num];
    int first_map = (region->e_size == 0);
    CPUState *env;

    region->e_physbase = addr;
    region->e_size = size;

    DEBUG("e_phys=0x%x r_baseport=%x type=0x%x len=%d region_num=%d \n",
          addr, region->u.r_baseport, type, size, region_num);

    if (first_map) {
	struct ioperm_data *data;

	data = qemu_mallocz(sizeof(struct ioperm_data));
	if (data == NULL) {
	    fprintf(stderr, "%s: Out of memory\n", __func__);
	    exit(1);
	}

	data->start_port = region->u.r_baseport;
	data->num = region->r_size;
	data->turn_on = 1;

	kvm_add_ioperm_data(data);

	for (env = first_cpu; env; env = env->next_cpu)
	    kvm_ioperm(env, data);
    }

    register_ioport_read(addr, size, 1, assigned_dev_ioport_readb,
                         (r_dev->v_addrs + region_num));
    register_ioport_read(addr, size, 2, assigned_dev_ioport_readw,
                         (r_dev->v_addrs + region_num));
    register_ioport_read(addr, size, 4, assigned_dev_ioport_readl,
                         (r_dev->v_addrs + region_num));
    register_ioport_write(addr, size, 1, assigned_dev_ioport_writeb,
                          (r_dev->v_addrs + region_num));
    register_ioport_write(addr, size, 2, assigned_dev_ioport_writew,
                          (r_dev->v_addrs + region_num));
    register_ioport_write(addr, size, 4, assigned_dev_ioport_writel,
                          (r_dev->v_addrs + region_num));
}

static uint8_t pci_find_cap_offset(struct pci_dev *pci_dev, uint8_t cap)
{
    int id;
    int max_cap = 48;
    int pos = PCI_CAPABILITY_LIST;
    int status;

    status = pci_read_byte(pci_dev, PCI_STATUS);
    if ((status & PCI_STATUS_CAP_LIST) == 0)
        return 0;

    while (max_cap--) {
        pos = pci_read_byte(pci_dev, pos);
        if (pos < 0x40)
            break;

        pos &= ~3;
        id = pci_read_byte(pci_dev, pos + PCI_CAP_LIST_ID);

        if (id == 0xff)
            break;
        if (id == cap)
            return pos;

        pos += PCI_CAP_LIST_NEXT;
    }
    return 0;
}

static void assigned_dev_pci_write_config(PCIDevice *d, uint32_t address,
                                          uint32_t val, int len)
{
    int fd;
    ssize_t ret;
    AssignedDevice *pci_dev = container_of(d, AssignedDevice, dev);

    DEBUG("(%x.%x): address=%04x val=0x%08x len=%d\n",
          ((d->devfn >> 3) & 0x1F), (d->devfn & 0x7),
          (uint16_t) address, val, len);

    if (address == 0x4) {
        pci_default_write_config(d, address, val, len);
        /* Continue to program the card */
    }

    if ((address >= 0x10 && address <= 0x24) || address == 0x30 ||
        address == 0x34 || address == 0x3c || address == 0x3d ||
        pci_access_cap_config(d, address, len)) {
        /* used for update-mappings (BAR emulation) */
        pci_default_write_config(d, address, val, len);
        return;
    }

    DEBUG("NON BAR (%x.%x): address=%04x val=0x%08x len=%d\n",
          ((d->devfn >> 3) & 0x1F), (d->devfn & 0x7),
          (uint16_t) address, val, len);

    fd = pci_dev->real_device.config_fd;

again:
    ret = pwrite(fd, &val, len, address);
    if (ret != len) {
	if ((ret < 0) && (errno == EINTR || errno == EAGAIN))
	    goto again;

	fprintf(stderr, "%s: pwrite failed, ret = %zd errno = %d\n",
		__func__, ret, errno);

	exit(1);
    }
}

static uint32_t assigned_dev_pci_read_config(PCIDevice *d, uint32_t address,
                                             int len)
{
    uint32_t val = 0;
    int fd;
    ssize_t ret;
    AssignedDevice *pci_dev = container_of(d, AssignedDevice, dev);

    if (address < 0x4 || (pci_dev->need_emulate_cmd && address == 0x4) ||
	(address >= 0x10 && address <= 0x24) || address == 0x30 ||
        address == 0x34 || address == 0x3c || address == 0x3d ||
        pci_access_cap_config(d, address, len)) {
        val = pci_default_read_config(d, address, len);
        DEBUG("(%x.%x): address=%04x val=0x%08x len=%d\n",
              (d->devfn >> 3) & 0x1F, (d->devfn & 0x7), address, val, len);
        return val;
    }

    /* vga specific, remove later */
    if (address == 0xFC)
        goto do_log;

    fd = pci_dev->real_device.config_fd;

again:
    ret = pread(fd, &val, len, address);
    if (ret != len) {
	if ((ret < 0) && (errno == EINTR || errno == EAGAIN))
	    goto again;

	fprintf(stderr, "%s: pread failed, ret = %zd errno = %d\n",
		__func__, ret, errno);

	exit(1);
    }

do_log:
    DEBUG("(%x.%x): address=%04x val=0x%08x len=%d\n",
          (d->devfn >> 3) & 0x1F, (d->devfn & 0x7), address, val, len);

    if (!pci_dev->cap.available) {
        /* kill the special capabilities */
        if (address == 4 && len == 4)
            val &= ~0x100000;
        else if (address == 6)
            val &= ~0x10;
    }

    return val;
}

static int assigned_dev_register_regions(PCIRegion *io_regions,
                                         unsigned long regions_num,
                                         AssignedDevice *pci_dev)
{
    uint32_t i;
    PCIRegion *cur_region = io_regions;

    for (i = 0; i < regions_num; i++, cur_region++) {
        if (!cur_region->valid)
            continue;
        pci_dev->v_addrs[i].num = i;

        /* handle memory io regions */
        if (cur_region->type & IORESOURCE_MEM) {
            int t = cur_region->type & IORESOURCE_PREFETCH
                ? PCI_ADDRESS_SPACE_MEM_PREFETCH
                : PCI_ADDRESS_SPACE_MEM;

            /* map physical memory */
            pci_dev->v_addrs[i].e_physbase = cur_region->base_addr;
            if (i == PCI_ROM_SLOT) {
                pci_dev->v_addrs[i].u.r_virtbase =
                    mmap(NULL,
                         (cur_region->size + 0xFFF) & 0xFFFFF000,
                         PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE,
                         0, (off_t) 0);

            } else {
                pci_dev->v_addrs[i].u.r_virtbase =
                    mmap(NULL,
                         (cur_region->size + 0xFFF) & 0xFFFFF000,
                         PROT_WRITE | PROT_READ, MAP_SHARED,
                         cur_region->resource_fd, (off_t) 0);
            }

            if (pci_dev->v_addrs[i].u.r_virtbase == MAP_FAILED) {
                pci_dev->v_addrs[i].u.r_virtbase = NULL;
                fprintf(stderr, "%s: Error: Couldn't mmap 0x%x!"
                        "\n", __func__,
                        (uint32_t) (cur_region->base_addr));
                return -1;
            }

            if (i == PCI_ROM_SLOT) {
                memset(pci_dev->v_addrs[i].u.r_virtbase, 0,
                       (cur_region->size + 0xFFF) & 0xFFFFF000);
                mprotect(pci_dev->v_addrs[PCI_ROM_SLOT].u.r_virtbase,
                         (cur_region->size + 0xFFF) & 0xFFFFF000, PROT_READ);
            }

            pci_dev->v_addrs[i].r_size = cur_region->size;
            pci_dev->v_addrs[i].e_size = 0;

            /* add offset */
            pci_dev->v_addrs[i].u.r_virtbase +=
                (cur_region->base_addr & 0xFFF);

            pci_register_bar((PCIDevice *) pci_dev, i,
                             cur_region->size, t,
                             assigned_dev_iomem_map);
            continue;
        }
        /* handle port io regions */
        pci_dev->v_addrs[i].e_physbase = cur_region->base_addr;
        pci_dev->v_addrs[i].u.r_baseport = cur_region->base_addr;
        pci_dev->v_addrs[i].r_size = cur_region->size;
        pci_dev->v_addrs[i].e_size = 0;

        pci_register_bar((PCIDevice *) pci_dev, i,
                         cur_region->size, PCI_ADDRESS_SPACE_IO,
                         assigned_dev_ioport_map);

        /* not relevant for port io */
        pci_dev->v_addrs[i].memory_index = 0;
    }

    /* success */
    return 0;
}

static int get_real_device(AssignedDevice *pci_dev, uint8_t r_bus,
                           uint8_t r_dev, uint8_t r_func)
{
    char dir[128], name[128];
    int fd, r = 0;
    FILE *f;
    unsigned long long start, end, size, flags;
    unsigned long id;
    struct stat statbuf;
    PCIRegion *rp;
    PCIDevRegions *dev = &pci_dev->real_device;

    dev->region_number = 0;

    snprintf(dir, sizeof(dir), "/sys/bus/pci/devices/0000:%02x:%02x.%x/",
	     r_bus, r_dev, r_func);

    snprintf(name, sizeof(name), "%sconfig", dir);

    fd = open(name, O_RDWR);
    if (fd == -1) {
        fprintf(stderr, "%s: %s: %m\n", __func__, name);
        return 1;
    }
    dev->config_fd = fd;
again:
    r = read(fd, pci_dev->dev.config, sizeof(pci_dev->dev.config));
    if (r < 0) {
        if (errno == EINTR || errno == EAGAIN)
            goto again;
        fprintf(stderr, "%s: read failed, errno = %d\n", __func__, errno);
    }

    snprintf(name, sizeof(name), "%sresource", dir);

    f = fopen(name, "r");
    if (f == NULL) {
        fprintf(stderr, "%s: %s: %m\n", __func__, name);
        return 1;
    }

    for (r = 0; r < PCI_NUM_REGIONS; r++) {
	if (fscanf(f, "%lli %lli %lli\n", &start, &end, &flags) != 3)
	    break;

        rp = dev->regions + r;
        rp->valid = 0;
        size = end - start + 1;
        flags &= IORESOURCE_IO | IORESOURCE_MEM | IORESOURCE_PREFETCH;
        if (size == 0 || (flags & ~IORESOURCE_PREFETCH) == 0)
            continue;
        if (flags & IORESOURCE_MEM) {
            flags &= ~IORESOURCE_IO;
            if (r != PCI_ROM_SLOT) {
                snprintf(name, sizeof(name), "%sresource%d", dir, r);
                fd = open(name, O_RDWR);
                if (fd == -1)
                    continue;
                rp->resource_fd = fd;
            }
        } else
            flags &= ~IORESOURCE_PREFETCH;

        rp->type = flags;
        rp->valid = 1;
        rp->base_addr = start;
        rp->size = size;
        DEBUG("region %d size %d start 0x%llx type %d resource_fd %d\n",
              r, rp->size, start, rp->type, rp->resource_fd);
    }

    fclose(f);

    /* read and fill device ID */
    snprintf(name, sizeof(name), "%svendor", dir);
    f = fopen(name, "r");
    if (f == NULL) {
        fprintf(stderr, "%s: %s: %m\n", __func__, name);
        return 1;
    }
    if (fscanf(f, "%li\n", &id) == 1) {
	pci_dev->dev.config[0] = id & 0xff;
	pci_dev->dev.config[1] = (id & 0xff00) >> 8;
    }
    fclose(f);

    /* read and fill vendor ID */
    snprintf(name, sizeof(name), "%sdevice", dir);
    f = fopen(name, "r");
    if (f == NULL) {
        fprintf(stderr, "%s: %s: %m\n", __func__, name);
        return 1;
    }
    if (fscanf(f, "%li\n", &id) == 1) {
	pci_dev->dev.config[2] = id & 0xff;
	pci_dev->dev.config[3] = (id & 0xff00) >> 8;
    }
    fclose(f);

    /* dealing with virtual function device */
    snprintf(name, sizeof(name), "%sphysfn/", dir);
    if (!stat(name, &statbuf))
	    pci_dev->need_emulate_cmd = 1;
    else
	    pci_dev->need_emulate_cmd = 0;

    dev->region_number = r;
    return 0;
}

static LIST_HEAD(, AssignedDevInfo) adev_head;

#ifdef KVM_CAP_IRQ_ROUTING
static void free_dev_irq_entries(AssignedDevice *dev)
{
    int i;

    for (i = 0; i < dev->irq_entries_nr; i++)
        kvm_del_routing_entry(kvm_context, &dev->entry[i]);
    free(dev->entry);
    dev->entry = NULL;
    dev->irq_entries_nr = 0;
}
#endif

static void free_assigned_device(AssignedDevInfo *adev)
{
    AssignedDevice *dev = adev->assigned_dev;

    if (dev) {
        int i;

        for (i = 0; i < dev->real_device.region_number; i++) {
            PCIRegion *pci_region = &dev->real_device.regions[i];
            AssignedDevRegion *region = &dev->v_addrs[i];

            if (!pci_region->valid)
                continue;

            if (pci_region->type & IORESOURCE_IO) {
                kvm_remove_ioperm_data(region->u.r_baseport, region->r_size);
                continue;
            } else if (pci_region->type & IORESOURCE_MEM) {
                if (region->e_size > 0)
                    kvm_destroy_phys_mem(kvm_context, region->e_physbase,
                                         TARGET_PAGE_ALIGN(region->e_size));

                if (region->u.r_virtbase) {
                    int ret = munmap(region->u.r_virtbase,
                                     (pci_region->size + 0xFFF) & 0xFFFFF000);
                    if (ret != 0)
                        fprintf(stderr,
				"Failed to unmap assigned device region: %s\n",
				strerror(errno));
                }
	    }
        }

        if (dev->real_device.config_fd) {
            close(dev->real_device.config_fd);
            dev->real_device.config_fd = 0;
        }

        pci_unregister_device(&dev->dev, 1);
#ifdef KVM_CAP_IRQ_ROUTING
        free_dev_irq_entries(dev);
#endif
        adev->assigned_dev = dev = NULL;
    }

    LIST_REMOVE(adev, next);
    qemu_free(adev);
}

static uint32_t calc_assigned_dev_id(uint8_t bus, uint8_t devfn)
{
    return (uint32_t)bus << 8 | (uint32_t)devfn;
}

static int assign_device(AssignedDevInfo *adev)
{
    struct kvm_assigned_pci_dev assigned_dev_data;
    AssignedDevice *dev = adev->assigned_dev;
    int r;

    memset(&assigned_dev_data, 0, sizeof(assigned_dev_data));
    assigned_dev_data.assigned_dev_id  =
	calc_assigned_dev_id(dev->h_busnr, dev->h_devfn);
    assigned_dev_data.busnr = dev->h_busnr;
    assigned_dev_data.devfn = dev->h_devfn;

#ifdef KVM_CAP_IOMMU
    /* We always enable the IOMMU if present
     * (or when not disabled on the command line)
     */
    r = kvm_check_extension(kvm_state, KVM_CAP_IOMMU);
    if (r && !adev->disable_iommu)
	assigned_dev_data.flags |= KVM_DEV_ASSIGN_ENABLE_IOMMU;
#endif

    r = kvm_assign_pci_device(kvm_context, &assigned_dev_data);
    if (r < 0)
	fprintf(stderr, "Failed to assign device \"%s\" : %s\n",
                adev->name, strerror(-r));
    return r;
}

static int assign_irq(AssignedDevInfo *adev)
{
    struct kvm_assigned_irq assigned_irq_data;
    AssignedDevice *dev = adev->assigned_dev;
    int irq, r = 0;

    /* Interrupt PIN 0 means don't use INTx */
    if (pci_read_byte(dev->pdev, PCI_INTERRUPT_PIN) == 0)
        return 0;

    irq = pci_map_irq(&dev->dev, dev->intpin);
    irq = piix_get_irq(irq);

#ifdef TARGET_IA64
    irq = ipf_map_irq(&dev->dev, irq);
#endif

    if (dev->girq == irq)
        return r;

    memset(&assigned_irq_data, 0, sizeof(assigned_irq_data));
    assigned_irq_data.assigned_dev_id =
        calc_assigned_dev_id(dev->h_busnr, dev->h_devfn);
    assigned_irq_data.guest_irq = irq;
    assigned_irq_data.host_irq = dev->real_device.irq;
#ifdef KVM_CAP_ASSIGN_DEV_IRQ
    if (dev->irq_requested_type) {
        assigned_irq_data.flags = dev->irq_requested_type;
        r = kvm_deassign_irq(kvm_context, &assigned_irq_data);
        /* -ENXIO means no assigned irq */
        if (r && r != -ENXIO)
            perror("assign_irq: deassign");
    }

    assigned_irq_data.flags = KVM_DEV_IRQ_GUEST_INTX;
    if (dev->cap.available & ASSIGNED_DEVICE_CAP_MSI)
        assigned_irq_data.flags |= KVM_DEV_IRQ_HOST_MSI;
    else
        assigned_irq_data.flags |= KVM_DEV_IRQ_HOST_INTX;
#endif

    r = kvm_assign_irq(kvm_context, &assigned_irq_data);
    if (r < 0) {
        fprintf(stderr, "Failed to assign irq for \"%s\": %s\n",
                adev->name, strerror(-r));
        fprintf(stderr, "Perhaps you are assigning a device "
                "that shares an IRQ with another device?\n");
        return r;
    }

    dev->girq = irq;
    dev->irq_requested_type = assigned_irq_data.flags;
    return r;
}

static void deassign_device(AssignedDevInfo *adev)
{
#ifdef KVM_CAP_DEVICE_DEASSIGNMENT
    struct kvm_assigned_pci_dev assigned_dev_data;
    AssignedDevice *dev = adev->assigned_dev;
    int r;

    memset(&assigned_dev_data, 0, sizeof(assigned_dev_data));
    assigned_dev_data.assigned_dev_id  =
	calc_assigned_dev_id(dev->h_busnr, dev->h_devfn);

    r = kvm_deassign_pci_device(kvm_context, &assigned_dev_data);
    if (r < 0)
	fprintf(stderr, "Failed to deassign device \"%s\" : %s\n",
                adev->name, strerror(-r));
#endif
}

void remove_assigned_device(AssignedDevInfo *adev)
{
    deassign_device(adev);
    free_assigned_device(adev);
}

AssignedDevInfo *get_assigned_device(int pcibus, int slot)
{
    AssignedDevice *assigned_dev = NULL;
    AssignedDevInfo *adev = NULL;

    LIST_FOREACH(adev, &adev_head, next) {
        assigned_dev = adev->assigned_dev;
        if (pci_bus_num(assigned_dev->dev.bus) == pcibus &&
            PCI_SLOT(assigned_dev->dev.devfn) == slot)
            return adev;
    }

    return NULL;
}

/* The pci config space got updated. Check if irq numbers have changed
 * for our devices
 */
void assigned_dev_update_irqs()
{
    AssignedDevInfo *adev;

    adev = LIST_FIRST(&adev_head);
    while (adev) {
        AssignedDevInfo *next = LIST_NEXT(adev, next);
        int r;

        r = assign_irq(adev);
        if (r < 0)
            remove_assigned_device(adev);

        adev = next;
    }
}

#ifdef KVM_CAP_IRQ_ROUTING

#ifdef KVM_CAP_DEVICE_MSI
static void assigned_dev_update_msi(PCIDevice *pci_dev, unsigned int ctrl_pos)
{
    struct kvm_assigned_irq assigned_irq_data;
    AssignedDevice *assigned_dev = container_of(pci_dev, AssignedDevice, dev);
    uint8_t ctrl_byte = pci_dev->config[ctrl_pos];
    int r;

    memset(&assigned_irq_data, 0, sizeof assigned_irq_data);
    assigned_irq_data.assigned_dev_id  =
        calc_assigned_dev_id(assigned_dev->h_busnr,
                (uint8_t)assigned_dev->h_devfn);

    if (assigned_dev->irq_requested_type) {
	    assigned_irq_data.flags = assigned_dev->irq_requested_type;
	    free_dev_irq_entries(assigned_dev);
	    r = kvm_deassign_irq(kvm_context, &assigned_irq_data);
	    /* -ENXIO means no assigned irq */
	    if (r && r != -ENXIO)
		    perror("assigned_dev_update_msi: deassign irq");
    }

    if (ctrl_byte & PCI_MSI_FLAGS_ENABLE) {
        assigned_dev->entry = calloc(1, sizeof(struct kvm_irq_routing_entry));
        if (!assigned_dev->entry) {
            perror("assigned_dev_update_msi: ");
            return;
        }
        assigned_dev->entry->u.msi.address_lo =
                *(uint32_t *)(pci_dev->config + pci_dev->cap.start +
                              PCI_MSI_ADDRESS_LO);
        assigned_dev->entry->u.msi.address_hi = 0;
        assigned_dev->entry->u.msi.data = *(uint16_t *)(pci_dev->config +
                pci_dev->cap.start + PCI_MSI_DATA_32);
        assigned_dev->entry->type = KVM_IRQ_ROUTING_MSI;
        r = kvm_get_irq_route_gsi(kvm_context);
        if (r < 0) {
            perror("assigned_dev_update_msi: kvm_get_irq_route_gsi");
            return;
        }
        assigned_dev->entry->gsi = r;

        kvm_add_routing_entry(kvm_context, assigned_dev->entry);
        if (kvm_commit_irq_routes(kvm_context) < 0) {
            perror("assigned_dev_update_msi: kvm_commit_irq_routes");
            assigned_dev->cap.state &= ~ASSIGNED_DEVICE_MSI_ENABLED;
            return;
        }
	assigned_dev->irq_entries_nr = 1;

        assigned_irq_data.guest_irq = assigned_dev->entry->gsi;
	assigned_irq_data.flags = KVM_DEV_IRQ_HOST_MSI | KVM_DEV_IRQ_GUEST_MSI;
        if (kvm_assign_irq(kvm_context, &assigned_irq_data) < 0)
            perror("assigned_dev_enable_msi: assign irq");

        assigned_dev->irq_requested_type = assigned_irq_data.flags;
    }
}
#endif

#ifdef KVM_CAP_DEVICE_MSIX
static int assigned_dev_update_msix_mmio(PCIDevice *pci_dev)
{
    AssignedDevice *adev = container_of(pci_dev, AssignedDevice, dev);
    u16 entries_nr = 0, entries_max_nr;
    int pos = 0, i, r = 0;
    u32 msg_addr, msg_upper_addr, msg_data, msg_ctrl;
    struct kvm_assigned_msix_nr msix_nr;
    struct kvm_assigned_msix_entry msix_entry;
    void *va = adev->msix_table_page;

    if (adev->cap.available & ASSIGNED_DEVICE_CAP_MSI)
        pos = pci_dev->cap.start + PCI_CAPABILITY_CONFIG_MSI_LENGTH;
    else
        pos = pci_dev->cap.start;

    entries_max_nr = pci_dev->config[pos + 2];
    entries_max_nr &= PCI_MSIX_TABSIZE;
    entries_max_nr += 1;

    /* Get the usable entry number for allocating */
    for (i = 0; i < entries_max_nr; i++) {
        memcpy(&msg_ctrl, va + i * 16 + 12, 4);
        memcpy(&msg_data, va + i * 16 + 8, 4);
        /* Ignore unused entry even it's unmasked */
        if (msg_data == 0)
            continue;
        entries_nr ++;
    }

    if (entries_nr == 0) {
        fprintf(stderr, "MSI-X entry number is zero!\n");
        return -EINVAL;
    }
    msix_nr.assigned_dev_id = calc_assigned_dev_id(adev->h_busnr,
                                          (uint8_t)adev->h_devfn);
    msix_nr.entry_nr = entries_nr;
    r = kvm_assign_set_msix_nr(kvm_context, &msix_nr);
    if (r != 0) {
        fprintf(stderr, "fail to set MSI-X entry number for MSIX! %s\n",
			strerror(-r));
        return r;
    }

    free_dev_irq_entries(adev);
    adev->irq_entries_nr = entries_nr;
    adev->entry = calloc(entries_nr, sizeof(struct kvm_irq_routing_entry));
    if (!adev->entry) {
        perror("assigned_dev_update_msix_mmio: ");
        return -errno;
    }

    msix_entry.assigned_dev_id = msix_nr.assigned_dev_id;
    entries_nr = 0;
    for (i = 0; i < entries_max_nr; i++) {
        if (entries_nr >= msix_nr.entry_nr)
            break;
        memcpy(&msg_ctrl, va + i * 16 + 12, 4);
        memcpy(&msg_data, va + i * 16 + 8, 4);
        if (msg_data == 0)
            continue;

        memcpy(&msg_addr, va + i * 16, 4);
        memcpy(&msg_upper_addr, va + i * 16 + 4, 4);

        r = kvm_get_irq_route_gsi(kvm_context);
        if (r < 0)
            return r;

        adev->entry[entries_nr].gsi = r;
        adev->entry[entries_nr].type = KVM_IRQ_ROUTING_MSI;
        adev->entry[entries_nr].flags = 0;
        adev->entry[entries_nr].u.msi.address_lo = msg_addr;
        adev->entry[entries_nr].u.msi.address_hi = msg_upper_addr;
        adev->entry[entries_nr].u.msi.data = msg_data;
        DEBUG("MSI-X data 0x%x, MSI-X addr_lo 0x%x\n!", msg_data, msg_addr);
	kvm_add_routing_entry(kvm_context, &adev->entry[entries_nr]);

        msix_entry.gsi = adev->entry[entries_nr].gsi;
        msix_entry.entry = i;
        r = kvm_assign_set_msix_entry(kvm_context, &msix_entry);
        if (r) {
            fprintf(stderr, "fail to set MSI-X entry! %s\n", strerror(-r));
            break;
        }
        DEBUG("MSI-X entry gsi 0x%x, entry %d\n!",
                msix_entry.gsi, msix_entry.entry);
        entries_nr ++;
    }

    if (r == 0 && kvm_commit_irq_routes(kvm_context) < 0) {
	    perror("assigned_dev_update_msix_mmio: kvm_commit_irq_routes");
	    return -EINVAL;
    }

    return r;
}

static void assigned_dev_update_msix(PCIDevice *pci_dev, unsigned int ctrl_pos)
{
    struct kvm_assigned_irq assigned_irq_data;
    AssignedDevice *assigned_dev = container_of(pci_dev, AssignedDevice, dev);
    uint16_t *ctrl_word = (uint16_t *)(pci_dev->config + ctrl_pos);
    int r;

    memset(&assigned_irq_data, 0, sizeof assigned_irq_data);
    assigned_irq_data.assigned_dev_id  =
            calc_assigned_dev_id(assigned_dev->h_busnr,
                    (uint8_t)assigned_dev->h_devfn);

    if (assigned_dev->irq_requested_type) {
        assigned_irq_data.flags = assigned_dev->irq_requested_type;
        free_dev_irq_entries(assigned_dev);
        r = kvm_deassign_irq(kvm_context, &assigned_irq_data);
        /* -ENXIO means no assigned irq */
        if (r && r != -ENXIO)
            perror("assigned_dev_update_msix: deassign irq");
    }
    assigned_irq_data.flags = KVM_DEV_IRQ_HOST_MSIX | KVM_DEV_IRQ_GUEST_MSIX;

    if (*ctrl_word & PCI_MSIX_ENABLE) {
        if (assigned_dev_update_msix_mmio(pci_dev) < 0) {
            perror("assigned_dev_update_msix_mmio");
            return;
        }
        if (kvm_assign_irq(kvm_context, &assigned_irq_data) < 0) {
            perror("assigned_dev_enable_msix: assign irq");
            return;
        }
        assigned_dev->irq_requested_type = assigned_irq_data.flags;
    }
}
#endif
#endif

static void assigned_device_pci_cap_write_config(PCIDevice *pci_dev, uint32_t address,
                                                 uint32_t val, int len)
{
    AssignedDevice *assigned_dev = container_of(pci_dev, AssignedDevice, dev);
    unsigned int pos = pci_dev->cap.start, ctrl_pos;

    pci_default_cap_write_config(pci_dev, address, val, len);
#ifdef KVM_CAP_IRQ_ROUTING
#ifdef KVM_CAP_DEVICE_MSI
    if (assigned_dev->cap.available & ASSIGNED_DEVICE_CAP_MSI) {
        ctrl_pos = pos + PCI_MSI_FLAGS;
        if (address <= ctrl_pos && address + len > ctrl_pos)
            assigned_dev_update_msi(pci_dev, ctrl_pos);
        pos += PCI_CAPABILITY_CONFIG_MSI_LENGTH;
    }
#endif
#ifdef KVM_CAP_DEVICE_MSIX
    if (assigned_dev->cap.available & ASSIGNED_DEVICE_CAP_MSIX) {
        ctrl_pos = pos + 3;
        if (address <= ctrl_pos && address + len > ctrl_pos) {
            ctrl_pos--; /* control is word long */
            assigned_dev_update_msix(pci_dev, ctrl_pos);
	}
        pos += PCI_CAPABILITY_CONFIG_MSIX_LENGTH;
    }
#endif
#endif
    return;
}

static int assigned_device_pci_cap_init(PCIDevice *pci_dev)
{
    AssignedDevice *dev = container_of(pci_dev, AssignedDevice, dev);
    PCIRegion *pci_region = dev->real_device.regions;
    int next_cap_pt = 0;

    pci_dev->cap.length = 0;
#ifdef KVM_CAP_IRQ_ROUTING
#ifdef KVM_CAP_DEVICE_MSI
    /* Expose MSI capability
     * MSI capability is the 1st capability in capability config */
    if (pci_find_cap_offset(dev->pdev, PCI_CAP_ID_MSI)) {
        dev->cap.available |= ASSIGNED_DEVICE_CAP_MSI;
        memset(&pci_dev->config[pci_dev->cap.start + pci_dev->cap.length],
               0, PCI_CAPABILITY_CONFIG_MSI_LENGTH);
        pci_dev->config[pci_dev->cap.start + pci_dev->cap.length] =
                        PCI_CAP_ID_MSI;
        pci_dev->cap.length += PCI_CAPABILITY_CONFIG_MSI_LENGTH;
        next_cap_pt = 1;
    }
#endif
#ifdef KVM_CAP_DEVICE_MSIX
    /* Expose MSI-X capability */
    if (pci_find_cap_offset(dev->pdev, PCI_CAP_ID_MSIX)) {
        int pos, entry_nr, bar_nr;
        u32 msix_table_entry;
        dev->cap.available |= ASSIGNED_DEVICE_CAP_MSIX;
        memset(&pci_dev->config[pci_dev->cap.start + pci_dev->cap.length],
               0, PCI_CAPABILITY_CONFIG_MSIX_LENGTH);
        pos = pci_find_cap_offset(dev->pdev, PCI_CAP_ID_MSIX);
        entry_nr = pci_read_word(dev->pdev, pos + 2) & PCI_MSIX_TABSIZE;
        pci_dev->config[pci_dev->cap.start + pci_dev->cap.length] = 0x11;
        pci_dev->config[pci_dev->cap.start +
                        pci_dev->cap.length + 2] = entry_nr;
        msix_table_entry = pci_read_long(dev->pdev, pos + PCI_MSIX_TABLE);
        *(uint32_t *)(pci_dev->config + pci_dev->cap.start +
                      pci_dev->cap.length + PCI_MSIX_TABLE) = msix_table_entry;
        *(uint32_t *)(pci_dev->config + pci_dev->cap.start +
                      pci_dev->cap.length + PCI_MSIX_PBA) =
                    pci_read_long(dev->pdev, pos + PCI_MSIX_PBA);
        bar_nr = msix_table_entry & PCI_MSIX_BIR;
        msix_table_entry &= ~PCI_MSIX_BIR;
        dev->msix_table_addr = pci_region[bar_nr].base_addr + msix_table_entry;
        if (next_cap_pt != 0) {
            pci_dev->config[pci_dev->cap.start + next_cap_pt] =
                pci_dev->cap.start + pci_dev->cap.length;
            next_cap_pt += PCI_CAPABILITY_CONFIG_MSI_LENGTH;
        } else
            next_cap_pt = 1;
        pci_dev->cap.length += PCI_CAPABILITY_CONFIG_MSIX_LENGTH;
    }
#endif
#endif

    return 0;
}

static uint32_t msix_mmio_readl(void *opaque, target_phys_addr_t addr)
{
    AssignedDevice *adev = opaque;
    unsigned int offset = addr & 0xfff;
    void *page = adev->msix_table_page;
    uint32_t val = 0;

    memcpy(&val, (void *)((char *)page + offset), 4);

    return val;
}

static uint32_t msix_mmio_readb(void *opaque, target_phys_addr_t addr)
{
    return ((msix_mmio_readl(opaque, addr & ~3)) >>
            (8 * (addr & 3))) & 0xff;
}

static uint32_t msix_mmio_readw(void *opaque, target_phys_addr_t addr)
{
    return ((msix_mmio_readl(opaque, addr & ~3)) >>
            (8 * (addr & 3))) & 0xffff;
}

static void msix_mmio_writel(void *opaque,
                             target_phys_addr_t addr, uint32_t val)
{
    AssignedDevice *adev = opaque;
    unsigned int offset = addr & 0xfff;
    void *page = adev->msix_table_page;

    DEBUG("write to MSI-X entry table mmio offset 0x%lx, val 0x%lx\n",
		    addr, val);
    memcpy((void *)((char *)page + offset), &val, 4);
}

static void msix_mmio_writew(void *opaque,
                             target_phys_addr_t addr, uint32_t val)
{
    msix_mmio_writel(opaque, addr & ~3,
                     (val & 0xffff) << (8*(addr & 3)));
}

static void msix_mmio_writeb(void *opaque,
                             target_phys_addr_t addr, uint32_t val)
{
    msix_mmio_writel(opaque, addr & ~3,
                     (val & 0xff) << (8*(addr & 3)));
}

static CPUWriteMemoryFunc *msix_mmio_write[] = {
    msix_mmio_writeb,	msix_mmio_writew,	msix_mmio_writel
};

static CPUReadMemoryFunc *msix_mmio_read[] = {
    msix_mmio_readb,	msix_mmio_readw,	msix_mmio_readl
};

static int assigned_dev_register_msix_mmio(AssignedDevice *dev)
{
    dev->msix_table_page = mmap(NULL, 0x1000,
                                PROT_READ|PROT_WRITE,
                                MAP_ANONYMOUS|MAP_PRIVATE, 0, 0);
    if (dev->msix_table_page == MAP_FAILED) {
        fprintf(stderr, "fail allocate msix_table_page! %s\n",
                strerror(errno));
        return -EFAULT;
    }
    memset(dev->msix_table_page, 0, 0x1000);
    dev->mmio_index = cpu_register_io_memory(
                        msix_mmio_read, msix_mmio_write, dev);
    return 0;
}

struct PCIDevice *init_assigned_device(AssignedDevInfo *adev,
                                       const char *devaddr)
{
    PCIBus *bus;
    int devfn;
    int r;
    AssignedDevice *dev;
    PCIDevice *pci_dev;
    struct pci_access *pacc;
    uint8_t e_device, e_intx;

    DEBUG("Registering real physical device %s (bus=%x dev=%x func=%x)\n",
          adev->name, adev->bus, adev->dev, adev->func);

    bus = pci_get_bus_devfn(&devfn, devaddr);
    pci_dev = pci_register_device(bus, adev->name,
              sizeof(AssignedDevice), devfn, assigned_dev_pci_read_config,
              assigned_dev_pci_write_config);
    dev = container_of(pci_dev, AssignedDevice, dev);

    if (NULL == dev) {
        fprintf(stderr, "%s: Error: Couldn't register real device %s\n",
                __func__, adev->name);
        return NULL;
    }

    adev->assigned_dev = dev;

    if (get_real_device(dev, adev->bus, adev->dev, adev->func)) {
        fprintf(stderr, "%s: Error: Couldn't get real device (%s)!\n",
                __func__, adev->name);
        goto out;
    }

    /* handle real device's MMIO/PIO BARs */
    if (assigned_dev_register_regions(dev->real_device.regions,
                                      dev->real_device.region_number,
                                      dev))
        goto out;

    /* handle interrupt routing */
    e_device = (dev->dev.devfn >> 3) & 0x1f;
    e_intx = dev->dev.config[0x3d] - 1;
    dev->intpin = e_intx;
    dev->run = 0;
    dev->girq = 0;
    dev->h_busnr = adev->bus;
    dev->h_devfn = PCI_DEVFN(adev->dev, adev->func);

    pacc = pci_alloc();
    pci_init(pacc);
    dev->pdev = pci_get_dev(pacc, 0, adev->bus, adev->dev, adev->func);

    if (pci_enable_capability_support(pci_dev, 0, NULL,
                    assigned_device_pci_cap_write_config,
                    assigned_device_pci_cap_init) < 0)
        goto assigned_out;

    /* assign device to guest */
    r = assign_device(adev);
    if (r < 0)
        goto assigned_out;

    /* assign irq for the device */
    r = assign_irq(adev);
    if (r < 0)
        goto assigned_out;

    /* intercept MSI-X entry page in the MMIO */
    if (dev->cap.available & ASSIGNED_DEVICE_CAP_MSIX)
        if (assigned_dev_register_msix_mmio(dev))
            return NULL;

    return &dev->dev;

assigned_out:
    deassign_device(adev);
out:
    free_assigned_device(adev);
    return NULL;
}

/*
 * Syntax to assign device:
 *
 * -pcidevice host=bus:dev.func[,dma=none][,name=Foo]
 *
 * Example:
 * -pcidevice host=00:13.0,dma=pvdma
 *
 * dma can currently only be 'none' to disable iommu support.
 */
AssignedDevInfo *add_assigned_device(const char *arg)
{
    char device[16];
    char dma[6];
    int r;
    AssignedDevInfo *adev;

    adev = qemu_mallocz(sizeof(AssignedDevInfo));
    if (adev == NULL) {
        fprintf(stderr, "%s: Out of memory\n", __func__);
        return NULL;
    }
    r = get_param_value(device, sizeof(device), "host", arg);
    if (!r)
         goto bad;

    r = pci_parse_host_devaddr(device, &adev->bus, &adev->dev, &adev->func);
    if (r)
        goto bad;

    r = get_param_value(adev->name, sizeof(adev->name), "name", arg);
    if (!r)
	snprintf(adev->name, sizeof(adev->name), "%s", device);

#ifdef KVM_CAP_IOMMU
    r = get_param_value(dma, sizeof(dma), "dma", arg);
    if (r && !strncmp(dma, "none", 4))
        adev->disable_iommu = 1;
#endif

    LIST_INSERT_HEAD(&adev_head, adev, next);
    return adev;
bad:
    fprintf(stderr, "pcidevice argument parse error; "
            "please check the help text for usage\n");
    qemu_free(adev);
    return NULL;
}

void add_assigned_devices(PCIBus *bus, const char **devices, int n_devices)
{
    int i;

    for (i = 0; i < n_devices; i++) {
        struct AssignedDevInfo *adev;

        adev = add_assigned_device(devices[i]);
        if (!adev) {
            fprintf(stderr, "Could not add assigned device %s\n", devices[i]);
            exit(1);
        }

        if (!init_assigned_device(adev, NULL)) {
            fprintf(stderr, "Failed to initialize assigned device %s\n",
                    devices[i]);
            exit(1);
        }
    }
}

/* Option ROM header */
struct option_rom_header {
    uint8_t signature[2];
    uint8_t rom_size;
    uint32_t entry_point;
    uint8_t reserved[17];
    uint16_t pci_header_offset;
    uint16_t expansion_header_offset;
} __attribute__ ((packed));

/* Option ROM PCI data structure */
struct option_rom_pci_header {
    uint8_t signature[4];
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t vital_product_data_offset;
    uint16_t structure_length;
    uint8_t structure_revision;
    uint8_t class_code[3];
    uint16_t image_length;
    uint16_t image_revision;
    uint8_t code_type;
    uint8_t indicator;
    uint16_t reserved;
} __attribute__ ((packed));

/*
 * Scan the list of Option ROMs at roms. If a suitable Option ROM is found,
 * allocate a ram space and copy it there. Then return its size aligned to
 * both 2KB and target page size.
 */
#define OPTION_ROM_ALIGN(x) (((x) + 2047) & ~2047)
static int scan_option_rom(uint8_t devfn, void *roms, ram_addr_t offset)
{
    int i, size, total_size;
    uint8_t csum;
    ram_addr_t addr;
    struct option_rom_header *rom;
    struct option_rom_pci_header *pcih;

    rom = roms;

    for ( ; ; ) {
        /* Invalid signature means we're out of option ROMs. */
        if (strncmp((char *)rom->signature, "\x55\xaa", 2) ||
             (rom->rom_size == 0))
            break;

        size = rom->rom_size * 512;
        /* Invalid checksum means we're out of option ROMs. */
        csum = 0;
        for (i = 0; i < size; i++)
            csum += ((uint8_t *)rom)[i];
        if (csum != 0)
            break;

        /* Check the PCI header (if any) for a match. */
        pcih = (struct option_rom_pci_header *)
                ((char *)rom + rom->pci_header_offset);
        if ((rom->pci_header_offset != 0) &&
             !strncmp((char *)pcih->signature, "PCIR", 4))
            goto found;

        rom = (struct option_rom_header *)((char *)rom + size);
    }

    return 0;

 found:
    /* The size should be both 2K-aligned and page-aligned */
    total_size = (TARGET_PAGE_SIZE < 2048)
                  ? OPTION_ROM_ALIGN(size + 1)
                  : TARGET_PAGE_ALIGN(size + 1);

    /* Size of all available ram space is 0x10000 (0xd0000 to 0xe0000) */
    if ((offset + total_size) > 0x10000u) {
        fprintf(stderr, "Option ROM size %x exceeds available space\n", size);
        return 0;
    }

    addr = qemu_ram_alloc(total_size);
    cpu_register_physical_memory(0xd0000 + offset, total_size, addr | IO_MEM_ROM);

    /* Write ROM data and devfn to phys_addr */
    cpu_physical_memory_write_rom(0xd0000 + offset, (uint8_t *)rom, size);
    cpu_physical_memory_write_rom(0xd0000 + offset + size, &devfn, 1);

    return total_size;
}

/*
 * Scan the assigned devices for the devices that have an option ROM, and then
 * load the corresponding ROM data to RAM. If an error occurs while loading an
 * option ROM, we just ignore that option ROM and continue with the next one.
 */
ram_addr_t assigned_dev_load_option_roms(ram_addr_t rom_base_offset)
{
    ram_addr_t offset = rom_base_offset;
    AssignedDevInfo *adev;

    LIST_FOREACH(adev, &adev_head, next) {
        int size, len;
        void *buf;
        FILE *fp;
        uint8_t i = 1;
        char rom_file[64];

        snprintf(rom_file, sizeof(rom_file),
                 "/sys/bus/pci/devices/0000:%02x:%02x.%01x/rom",
                 adev->bus, adev->dev, adev->func);

        if (access(rom_file, F_OK))
            continue;

        /* Write something to the ROM file to enable it */
        fp = fopen(rom_file, "wb");
        if (fp == NULL)
            continue;
        len = fwrite(&i, 1, 1, fp);
        fclose(fp);
        if (len != 1)
            continue;

        /* The file has to be closed and reopened, otherwise it won't work */
        fp = fopen(rom_file, "rb");
        if (fp == NULL)
            continue;

        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        buf = malloc(size);
        if (buf == NULL) {
            fclose(fp);
            continue;
        }

        fread(buf, size, 1, fp);
        if (!feof(fp) || ferror(fp)) {
            free(buf);
            fclose(fp);
            continue;
        }

        /* Copy ROM contents into the space backing the ROM BAR */
        if (adev->assigned_dev->v_addrs[PCI_ROM_SLOT].r_size >= size &&
            adev->assigned_dev->v_addrs[PCI_ROM_SLOT].u.r_virtbase) {
            mprotect(adev->assigned_dev->v_addrs[PCI_ROM_SLOT].u.r_virtbase,
                     size, PROT_READ | PROT_WRITE);
            memcpy(adev->assigned_dev->v_addrs[PCI_ROM_SLOT].u.r_virtbase,
                   buf, size);
            mprotect(adev->assigned_dev->v_addrs[PCI_ROM_SLOT].u.r_virtbase,
                     size, PROT_READ);
        }

        /* Scan the buffer for suitable ROMs and increase the offset */
        offset += scan_option_rom(adev->assigned_dev->dev.devfn, buf, offset);

        free(buf);
        fclose(fp);
    }

    return offset;
}
