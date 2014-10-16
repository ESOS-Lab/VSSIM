CFLAGS += -I../include/powerpc
CFLAGS += -Wa,-mregnames -I test/lib
CFLAGS += -ffreestanding

cstart := test/powerpc/cstart.o

cflatobjs += \
	test/lib/powerpc/io.o

$(libcflat): LDFLAGS += -nostdlib

# these tests do not use libcflat
simpletests := \
	test/powerpc/spin.bin \
	test/powerpc/io.bin \
	test/powerpc/sprg.bin

# theses tests use cstart.o, libcflat, and libgcc
tests := \
	test/powerpc/exit.bin \
	test/powerpc/helloworld.bin

include config-powerpc-$(PROCESSOR).mak


all: kvmtrace kvmctl $(libcflat) $(simpletests) $(tests)

$(simpletests): %.bin: %.o
	$(CC) -nostdlib $^ -Wl,-T,flat.lds -o $@

$(tests): %.bin: $(cstart) %.o $(libcflat)
	$(CC) -nostdlib $^ $(libgcc) -Wl,-T,flat.lds -o $@

kvmctl_objs = main-ppc.o iotable.o ../libkvm/libkvm.a

arch_clean:
	$(RM) $(simpletests) $(tests) $(cstart)
	$(RM) $(patsubst %.bin, %.elf, $(simpletests) $(tests))
	$(RM) $(patsubst %.bin, %.o, $(simpletests) $(tests))
