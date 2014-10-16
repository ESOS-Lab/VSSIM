#ifndef VM_H
#define VM_H

void setup_vm();

void *vmalloc(unsigned long size);
void vfree(void *mem);
void *vmap(unsigned long long phys, unsigned long size);

#endif
