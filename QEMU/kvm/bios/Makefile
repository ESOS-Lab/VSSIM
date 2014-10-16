# Copyright (C) 2001  MandrakeSoft S.A.
#
#   MandrakeSoft S.A.
#   43, rue d'Aboukir
#   75002 Paris - France
#   http://www.linux-mandrake.com/
#   http://www.mandrakesoft.com/
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

# Makefile for the BIOS component of bochs


.SUFFIXES: .cc

srcdir = .


SHELL = /bin/sh

CXX = g++
CXXFLAGS = -g -O2 -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES

# cc-option, copied from user/Makefile
# Usage: OP_CFLAGS+=$(call cc-option, -falign-functions=0, -malign-functions=0)

cc-option = $(shell if $(CC) $(1) -S -o /dev/null -xc /dev/null \
              > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

CFLAGS = -m32
CFLAGS += $(call cc-option, -fno-stack-protector, "")
CFLAGS += $(call cc-option, -fno-stack-protector-all, "")

LDFLAGS = 
LIBS =  -lm
RANLIB = ranlib

BCC = bcc
GCC = gcc $(CFLAGS)
HOST_CC = gcc
AS86 = as86

BX_INCDIRS = -I.. -I$(srcdir)/.. -I../iodev -I$(srcdir)/../iodev
LOCAL_CXXFLAGS =

BUILDDATE = `date '+%m/%d/%y'`
BIOS_BUILD_DATE = "-DBIOS_BUILD_DATE=\"$(BUILDDATE)\""
#
# -------- end configurable options --------------------------
#


.cc.o:
	$(CXX) -c $(BX_INCDIRS) $(CXXFLAGS) $(LOCAL_CXXFLAGS) $< -o $@


bios: biossums BIOS-bochs-latest BIOS-bochs-legacy 

clean:
	rm -f  *.o *.a *.s _rombios*_.c rombios*.txt rombios*.sym
	rm -f  usage biossums rombios16.bin
	rm -f  rombios32.bin rombios32.out acpi-dsdt.hex acpi-ssdt.hex

dist-clean: clean
	rm -f  Makefile

bios-clean:
	rm -f  BIOS-bochs-*

BIOS-bochs-legacy: rombios.c apmbios.S biossums rombios.h
	$(GCC) $(BIOS_BUILD_DATE) -DLEGACY -E -P $< > _rombiosl_.c
	$(BCC) -o rombiosl.s -C-c -D__i86__ -0 -S _rombiosl_.c
	sed -e 's/^\.text//' -e 's/^\.data//' rombiosl.s > _rombiosl_.s
	$(AS86) _rombiosl_.s -b tmpl.bin -u- -w- -g -0 -j -O -l rombiosl.txt
	-perl ${srcdir}/makesym.perl < rombiosl.txt > rombiosl.sym
	mv tmpl.bin $@
	./biossums $@
	rm -f  _rombiosl_.s


rombios16.bin: rombios.c apmbios.S biossums rombios.h
	$(GCC) $(BIOS_BUILD_DATE) -E -P $< > _rombios_.c
	$(BCC) -o rombios.s -C-c -D__i86__ -0 -S _rombios_.c
	sed -e 's/^\.text//' -e 's/^\.data//' rombios.s > _rombios_.s
	$(AS86) _rombios_.s -b tmp.bin -u- -w- -g -0 -j -O -l rombios.txt
	-perl ${srcdir}/makesym.perl < rombios.txt > rombios.sym
	mv tmp.bin rombios16.bin
	./biossums rombios16.bin
	rm -f  _rombios_.s


rombios32.bin: rombios32.out rombios.h
	objcopy -O binary $< $@
	./biossums -pad $@

rombios32.out: rombios32start.o rombios32.o vapic.o rombios32.ld
	ld -o $@ -T rombios32.ld rombios32start.o vapic.o rombios32.o

rombios32.o: rombios32.c acpi-dsdt.hex acpi-ssdt.hex
	$(GCC) -m32 -O2 -Wall -c -o $@ $<

acpi-dsdt.hex: acpi-dsdt.dsl
	cpp -P $< $<.i
	iasl -tc -p $@ $<.i
	sed -i -e's/^unsigned char AmlCode/const unsigned char DSDTCode/' $@
	rm $<.i

acpi-ssdt.hex: acpi-ssdt.dsl
	cpp -P $< $<.i
	iasl -tc -p $@ $<.i
	sed -i -e's/^unsigned char AmlCode/const unsigned char SSDTCode/' $@
	rm $<.i

rombios32start.o: rombios32start.S
	$(GCC) -m32 -c -o $@ $<

vapic.o: vapic.S
	$(GCC) -m32 -c -o $@ $<

BIOS-bochs-latest: rombios16.bin rombios32.bin
	cat rombios32.bin rombios16.bin > $@

biossums: biossums.c
	$(HOST_CC) -o biossums biossums.c
