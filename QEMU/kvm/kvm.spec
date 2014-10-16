Name:           kvm
Version:        0.0
Release:        0
Summary:        Kernel Virtual Machine virtualization environment

Group:          System Environment/Kernel
License:        GPL
URL:            http://www.qumranet.com
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}

ExclusiveArch:  i386 x86_64 ia64

Requires:	kvm-kmod bridge-utils

%define Distribution %(rpm -q -qf /etc/redhat-release --qf '%%{name}' | cut -d"-"  -f 1)
%define os_version %(rpm -q --qf '%%{version}' %{Distribution}-release)
%define os_release %(rpm -q --qf '%%{release}' %{Distribution}-release | cut -d"." -f 1)

%if %([ x"%{Distribution}" = x"fedora" -a x"%{os_version}" = x"5" ] && echo 1 || echo 0)
%define require_gccver 32
%endif

%if %([ x"%{Distribution}" = x"fedora" -a 0"%{os_version}" -ge "8" ] && echo 1 || echo 0)
%define qemuldflags --qemu-ldflags=-Wl,--build-id
%else
%define qemuldflags ""
%endif

%if %([ x"%{Distribution}" = x"centos" -a x"%{os_version}" = x"4" ] && echo 1 || echo 0)
%define require_gccver 32
%endif

%if %([ x"%{Distribution}" = x"redhat" -a x"%{os_release}" = x"5" ] && echo 1 || echo 0)
%define require_gccver 34
%endif

%if %( [ x"%{require_gccver}" = x"32" ] && echo 1 || echo 0)
BuildRequires: compat-gcc-32
%else
BuildRequires: compat-gcc-34
%endif

BuildRequires:  SDL-devel zlib-devel alsa-lib-devel

%define _prebuilt %{?prebuilt:1}%{!?prebuilt:0}

%if !%{_prebuilt}
Source0: kvm.tar.gz
Source1: user.tar.gz
Source2: kernel.tar.gz
Source3: scripts.tar.gz
Source4: Makefile
Source5: configure
Source6: kvm_stat
Source7: libkvm.tar.gz
Source8: extboot.tar.gz
%endif

%description
The Kernel Virtual Machine provides a virtualization enviroment for processors
with hardware support for virtualization: Intel's VT-x&VT-i and AMD's AMD-V.

%prep

%if !%{_prebuilt}
%setup -T -b 0 -n qemu
%setup -T -b 1 -n user -D
%setup -T -b 2 -n kernel -D
%setup -T -b 7 -n libkvm -D
%setup -T -b 3 -n scripts -D
%setup -T -b 8 -n extboot -D
cd ..
cp %{_sourcedir}/Makefile %{_sourcedir}/configure %{_sourcedir}/kvm_stat .
%endif

%build

rm -rf %{buildroot}

%if !%{_prebuilt}
cd ..
./configure --prefix=/usr/kvm %{qemuldflags}
make -C libkvm
make -C user
%ifarch i386 x86_64
make extboot
%endif
#(cd qemu;
#    ./co
#   kpath="$(readlink -f ../kernel/include)"
#   upath="$(readlink -f ../user)"
#   ./configure --target-list=$(uname -i)-softmmu \
#	    --extra-cflags="-I$kpath -I$upath" \
#	    --extra-ldflags="-L$upath" \
#	    --disable-kqemu --enable-kvm --prefix=/usr/kvm
#)
make -C qemu
%endif

%install

%if !%{_prebuilt}
cd ..
%else
cd %{objdir}
%endif

make DESTDIR=%{buildroot} install-rpm

%define bindir /usr/bin
%define bin %{bindir}/kvm
%define initdir /etc/init.d
%define confdir /etc/kvm
%define utilsdir /etc/kvm/utils

%post 
/sbin/chkconfig --add kvm
/sbin/chkconfig --level 2345 kvm on
/sbin/chkconfig --level 16 kvm off
/usr/sbin/groupadd -fg 444 kvm

%preun
if [ "$1" != 0 ]; then
	/sbin/service kvm stop
	/sbin/chkconfig --level 2345 kvm off
	/sbin/chkconfig --del kvm
fi

%clean
%{__rm} -rf %{buildroot}

%files
/usr/bin/kvm
/usr/bin/kvm_stat
%{confdir}/qemu-ifup
%{initdir}/kvm  
/etc/udev/rules.d/*kvm*.rules
/usr/kvm
%changelog
