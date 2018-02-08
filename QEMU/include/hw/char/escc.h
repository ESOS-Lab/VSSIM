#ifndef HW_ESCC_H
#define HW_ESCC_H

/* escc.c */
#define TYPE_ESCC "escc"
#define ESCC_SIZE 4
MemoryRegion *escc_init(hwaddr base, qemu_irq irqA, qemu_irq irqB,
              Chardev *chrA, Chardev *chrB,
              int clock, int it_shift);

void slavio_serial_ms_kbd_init(hwaddr base, qemu_irq irq,
                               int disabled, int clock, int it_shift);

#endif
