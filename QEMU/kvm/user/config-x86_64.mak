TEST_DIR=test/x86
cstart.o = $(TEST_DIR)/cstart64.o
bits = 64
ldarch = elf64-x86-64
CFLAGS += -D__x86_64__

tests = $(TEST_DIR)/access.flat $(TEST_DIR)/irq.flat $(TEST_DIR)/sieve.flat \
      $(TEST_DIR)/simple.flat $(TEST_DIR)/stringio.flat \
      $(TEST_DIR)/memtest1.flat $(TEST_DIR)/emulator.flat \
      $(TEST_DIR)/hypercall.flat $(TEST_DIR)/apic.flat

include config-x86-common.mak
