/*
 * KVM-based ITS implementation for a GICv3-based system
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Written by Pavel Fedin <p.fedin@samsung.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/intc/arm_gicv3_its_common.h"
#include "sysemu/sysemu.h"
#include "sysemu/kvm.h"
#include "kvm_arm.h"
#include "migration/migration.h"

#define TYPE_KVM_ARM_ITS "arm-its-kvm"
#define KVM_ARM_ITS(obj) OBJECT_CHECK(GICv3ITSState, (obj), TYPE_KVM_ARM_ITS)

static int kvm_its_send_msi(GICv3ITSState *s, uint32_t value, uint16_t devid)
{
    struct kvm_msi msi;

    if (unlikely(!s->translater_gpa_known)) {
        MemoryRegion *mr = &s->iomem_its_translation;
        MemoryRegionSection mrs;

        mrs = memory_region_find(mr, 0, 1);
        memory_region_unref(mrs.mr);
        s->gits_translater_gpa = mrs.offset_within_address_space + 0x40;
        s->translater_gpa_known = true;
    }

    msi.address_lo = extract64(s->gits_translater_gpa, 0, 32);
    msi.address_hi = extract64(s->gits_translater_gpa, 32, 32);
    msi.data = le32_to_cpu(value);
    msi.flags = KVM_MSI_VALID_DEVID;
    msi.devid = devid;
    memset(msi.pad, 0, sizeof(msi.pad));

    return kvm_vm_ioctl(kvm_state, KVM_SIGNAL_MSI, &msi);
}

static void kvm_arm_its_realize(DeviceState *dev, Error **errp)
{
    GICv3ITSState *s = ARM_GICV3_ITS_COMMON(dev);
    Error *local_err = NULL;

    /*
     * Block migration of a KVM GICv3 ITS device: the API for saving and
     * restoring the state in the kernel is not yet available
     */
    error_setg(&s->migration_blocker, "vITS migration is not implemented");
    migrate_add_blocker(s->migration_blocker, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        error_free(s->migration_blocker);
        return;
    }

    s->dev_fd = kvm_create_device(kvm_state, KVM_DEV_TYPE_ARM_VGIC_ITS, false);
    if (s->dev_fd < 0) {
        error_setg_errno(errp, -s->dev_fd, "error creating in-kernel ITS");
        return;
    }

    /* explicit init of the ITS */
    kvm_device_access(s->dev_fd, KVM_DEV_ARM_VGIC_GRP_CTRL,
                      KVM_DEV_ARM_VGIC_CTRL_INIT, NULL, true);

    /* register the base address */
    kvm_arm_register_device(&s->iomem_its_cntrl, -1, KVM_DEV_ARM_VGIC_GRP_ADDR,
                            KVM_VGIC_ITS_ADDR_TYPE, s->dev_fd);

    gicv3_its_init_mmio(s, NULL);

    kvm_msi_use_devid = true;
    kvm_gsi_direct_mapping = false;
    kvm_msi_via_irqfd_allowed = kvm_irqfds_enabled();
}

static void kvm_arm_its_init(Object *obj)
{
    GICv3ITSState *s = KVM_ARM_ITS(obj);

    object_property_add_link(obj, "parent-gicv3",
                             "kvm-arm-gicv3", (Object **)&s->gicv3,
                             object_property_allow_set_link,
                             OBJ_PROP_LINK_UNREF_ON_RELEASE,
                             &error_abort);
}

static void kvm_arm_its_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    GICv3ITSCommonClass *icc = ARM_GICV3_ITS_COMMON_CLASS(klass);

    dc->realize = kvm_arm_its_realize;
    icc->send_msi = kvm_its_send_msi;
}

static const TypeInfo kvm_arm_its_info = {
    .name = TYPE_KVM_ARM_ITS,
    .parent = TYPE_ARM_GICV3_ITS_COMMON,
    .instance_size = sizeof(GICv3ITSState),
    .instance_init = kvm_arm_its_init,
    .class_init = kvm_arm_its_class_init,
};

static void kvm_arm_its_register_types(void)
{
    type_register_static(&kvm_arm_its_info);
}

type_init(kvm_arm_its_register_types)
