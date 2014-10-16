
include config.mak

DESTDIR :=

.PHONY: arch_clean clean

#make sure env CFLAGS variable is not used
CFLAGS =

libgcc := $(shell $(CC) --print-libgcc-file-name)

libcflat := test/lib/libcflat.a
cflatobjs := \
	test/lib/panic.o \
	test/lib/printf.o \
	test/lib/string.o

#include architecure specific make rules
include config-$(ARCH).mak

# cc-option
# Usage: OP_CFLAGS+=$(call cc-option, -falign-functions=0, -malign-functions=0)

cc-option = $(shell if $(CC) $(1) -S -o /dev/null -xc /dev/null \
              > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

CFLAGS += -O1
CFLAGS += $(autodepend-flags) -g -fomit-frame-pointer -Wall
CFLAGS += $(call cc-option, -fno-stack-protector, "")
CFLAGS += $(call cc-option, -fno-stack-protector-all, "")
CFLAGS += -I../include
CFLAGS += -I ../libkvm

LDFLAGS += $(CFLAGS) -L ../libkvm

CXXFLAGS = $(autodepend-flags)

autodepend-flags = -MMD -MF $(dir $*).$(notdir $*).d

LDFLAGS += -pthread -lrt

kvmtrace_objs= kvmtrace.o

kvmctl: $(kvmctl_objs)
	$(CC) $(LDFLAGS) $^ -o $@

kvmtrace: $(kvmtrace_objs)
	$(CC) $(LDFLAGS) $^ -o $@

$(libcflat): $(cflatobjs)
	$(AR) rcs $@ $^

%.o: %.S
	$(CC) $(CFLAGS) -c -nostdlib -o $@ $^

-include .*.d

clean: arch_clean
	$(RM) kvmctl kvmtrace *.o *.a .*.d $(libcflat) $(cflatobjs)
