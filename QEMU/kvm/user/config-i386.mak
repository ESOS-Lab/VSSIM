TEST_DIR=test/x86
cstart.o = $(TEST_DIR)/cstart.o
bits = 32
ldarch = elf32-i386
CFLAGS += -D__i386__
CFLAGS += -I $(KERNELDIR)/include

tests=

include config-x86-common.mak
