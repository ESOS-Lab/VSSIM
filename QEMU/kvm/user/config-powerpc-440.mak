

# for some reason binutils hates tlbsx unless we say we're 405  :(
CFLAGS += -Wa,-m405 -I test/lib/powerpc/44x

cflatobjs += \
	test/lib/powerpc/44x/map.o \
	test/lib/powerpc/44x/tlbwe.o \
	test/lib/powerpc/44x/timebase.o

simpletests += \
	test/powerpc/44x/tlbsx.bin \
	test/powerpc/44x/tlbwe_16KB.bin \
	test/powerpc/44x/tlbwe_hole.bin \
	test/powerpc/44x/tlbwe.bin
