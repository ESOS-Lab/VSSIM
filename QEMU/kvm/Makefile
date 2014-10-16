
include config.mak

DESTDIR=

rpmrelease = devel

sane-arch = $(subst i386,x86,$(subst x86_64,x86,$(subst s390x,s390,$(ARCH))))

.PHONY: kernel user libkvm qemu bios vgabios extboot clean libfdt cscope

all: libkvm qemu
ifneq '$(filter $(ARCH), x86_64 i386 ia64)' ''
    all: $(if $(WANT_MODULE), kernel) user
endif

kcmd = $(if $(WANT_MODULE),,@\#)

qemu kernel user libkvm:
	$(MAKE) -C $@

qemu: libkvm
ifneq '$(filter $(ARCH), i386 x86_64)' ''
    qemu: extboot
endif
ifneq '$(filter $(ARCH), powerpc ia64)' ''
    qemu: libfdt
endif
user: libkvm

# sync if kernel/Makefile exists and if using --with-patched-kernel
user libkvm qemu: header-sync-$(if $(wildcard kernel/Makefile),$(if $(WANT_MODULE),n,y),n)

header-sync-n:

header-sync-y:
	make -C kernel \
	LINUX=$(if $(KERNELSOURCEDIR),$(KERNELSOURCEDIR),$(KERNELDIR)) \
	header-sync
	rm -f kernel/include/asm
	ln -sf asm-$(sane-arch) kernel/include/asm

bios:
	$(MAKE) -C $@
	cp bios/BIOS-bochs-latest qemu/pc-bios/bios.bin

vgabios:
	$(MAKE) -C $@
	cp vgabios/VGABIOS-lgpl-latest.bin qemu/pc-bios/vgabios.bin
	cp vgabios/VGABIOS-lgpl-latest.cirrus.bin qemu/pc-bios/vgabios-cirrus.bin

extboot:
	$(MAKE) -C $@
	if ! [ -f qemu/pc-bios/extboot.bin ] \
           || ! cmp -s qemu/pc-bios/extboot.bin extboot/extboot.bin; then \
		cp extboot/extboot.bin qemu/pc-bios/extboot.bin; \
	fi
libfdt:
	$(MAKE) -C $@

LINUX=linux-2.6

sync:
	make -C kernel sync LINUX=$(shell readlink -f "$(LINUX)")

bindir = /usr/bin
bin = $(bindir)/kvm
initdir = /etc/init.d
confdir = /etc/kvm
utilsdir = /etc/kvm/utils

install-rpm:
	mkdir -p $(DESTDIR)/$(bindir)
	mkdir -p $(DESTDIR)/$(confdir)
	mkdir -p $(DESTDIR)/$(initdir)
	mkdir -p $(DESTDIR)/$(utilsdir)
	mkdir -p $(DESTDIR)/etc/udev/rules.d
	make -C qemu DESTDIR=$(DESTDIR)/ install
	ln -sf /usr/kvm/bin/qemu-system-x86_64 $(DESTDIR)/$(bin)
	install -m 755 kvm_stat $(DESTDIR)/$(bindir)/kvm_stat
	cp scripts/kvm $(DESTDIR)/$(initdir)/kvm
	cp scripts/qemu-ifup $(DESTDIR)/$(confdir)/qemu-ifup
	install -t $(DESTDIR)/etc/udev/rules.d scripts/*kvm*.rules

install:
	$(kcmd)make -C kernel DESTDIR="$(DESTDIR)" install
	make -C libkvm DESTDIR="$(DESTDIR)" install
	make -C qemu DESTDIR="$(DESTDIR)" install

tmpspec = .tmp.kvm.spec
RPMTOPDIR = $$(pwd)/rpmtop

rpm:	srpm
	rm -rf $(RPMTOPDIR)/BUILD
	mkdir -p $(RPMTOPDIR)/{BUILD,RPMS/$$(uname -i)}
	rpmbuild --rebuild \
		 --define="_topdir $(RPMTOPDIR)" \
		$(RPMTOPDIR)/SRPMS/kvm-0.0-$(rpmrelease).src.rpm

srpm:
	mkdir -p $(RPMTOPDIR)/{SOURCES,SRPMS}
	sed 's/^Release:.*/Release: $(rpmrelease)/' kvm.spec > $(tmpspec)
	tar czf $(RPMTOPDIR)/SOURCES/kvm.tar.gz qemu
	tar czf $(RPMTOPDIR)/SOURCES/user.tar.gz user
	tar czf $(RPMTOPDIR)/SOURCES/libkvm.tar.gz libkvm
	tar czf $(RPMTOPDIR)/SOURCES/kernel.tar.gz kernel
	tar czf $(RPMTOPDIR)/SOURCES/scripts.tar.gz scripts
	tar czf $(RPMTOPDIR)/SOURCES/extboot.tar.gz extboot
	cp Makefile configure kvm_stat $(RPMTOPDIR)/SOURCES
	rpmbuild  --define="_topdir $(RPMTOPDIR)" -bs $(tmpspec)
	$(RM) $(tmpspec)

clean:
	for i in $(if $(WANT_MODULE), kernel) user libkvm qemu libfdt; do \
		make -C $$i clean; \
	done
	rm -f ./cscope.*

distclean: clean
	rm -f config.mak user/config.mak

cscope:
	rm -f ./cscope.*
	find . -wholename './kernel' -prune -o -name "*.[ch]" -print > ./cscope.files
	cscope -b
