#This is a make file with common rules for both x86 & x86-64

CFLAGS += -I../include/x86

all: kvmctl kvmtrace test_cases

kvmctl_objs= main.o iotable.o ../libkvm/libkvm.a
balloon_ctl: balloon_ctl.o

cflatobjs += \
	test/lib/x86/io.o \
	test/lib/x86/smp.o

$(libcflat): LDFLAGS += -nostdlib
$(libcflat): CFLAGS += -ffreestanding -I test/lib

CFLAGS += -m$(bits)

FLATLIBS = test/lib/libcflat.a $(libgcc)
%.flat: %.o $(FLATLIBS)
	$(CC) $(CFLAGS) -nostdlib -o $@ -Wl,-T,flat.lds $^ $(FLATLIBS)

tests-common = $(TEST_DIR)/bootstrap \
			$(TEST_DIR)/vmexit.flat $(TEST_DIR)/tsc.flat \
			$(TEST_DIR)/smptest.flat  $(TEST_DIR)/port80.flat \
			$(TEST_DIR)/realmode.flat $(TEST_DIR)/msr.flat

test_cases: $(tests-common) $(tests)

$(TEST_DIR)/%.o: CFLAGS += -std=gnu99 -ffreestanding -I test/lib -I test/lib/x86
 
$(TEST_DIR)/bootstrap: $(TEST_DIR)/bootstrap.o
	$(CC) -nostdlib -o $@ -Wl,-T,bootstrap.lds $^
 
$(TEST_DIR)/irq.flat: $(TEST_DIR)/print.o
 
$(TEST_DIR)/access.flat: $(cstart.o) $(TEST_DIR)/access.o $(TEST_DIR)/print.o
 
$(TEST_DIR)/hypercall.flat: $(cstart.o) $(TEST_DIR)/hypercall.o $(TEST_DIR)/print.o
 
$(TEST_DIR)/sieve.flat: $(cstart.o) $(TEST_DIR)/sieve.o \
		$(TEST_DIR)/print.o $(TEST_DIR)/vm.o
 
$(TEST_DIR)/vmexit.flat: $(cstart.o) $(TEST_DIR)/vmexit.o
 
$(TEST_DIR)/test32.flat: $(TEST_DIR)/test32.o

$(TEST_DIR)/smptest.flat: $(cstart.o) $(TEST_DIR)/smptest.o
 
$(TEST_DIR)/emulator.flat: $(cstart.o) $(TEST_DIR)/vm.o $(TEST_DIR)/print.o

$(TEST_DIR)/port80.flat: $(cstart.o) $(TEST_DIR)/port80.o

$(TEST_DIR)/tsc.flat: $(cstart.o) $(TEST_DIR)/tsc.o

$(TEST_DIR)/apic.flat: $(cstart.o) $(TEST_DIR)/apic.o $(TEST_DIR)/vm.o \
		       $(TEST_DIR)/print.o 

$(TEST_DIR)/realmode.flat: $(TEST_DIR)/realmode.o
	$(CC) -m32 -nostdlib -o $@ -Wl,-T,$(TEST_DIR)/realmode.lds $^

$(TEST_DIR)/realmode.o: bits = 32

$(TEST_DIR)/msr.flat: $(cstart.o) $(TEST_DIR)/msr.o

arch_clean:
	$(RM) $(TEST_DIR)/bootstrap $(TEST_DIR)/*.o $(TEST_DIR)/*.flat \
	$(TEST_DIR)/.*.d $(TEST_DIR)/lib/.*.d $(TEST_DIR)/lib/*.o
