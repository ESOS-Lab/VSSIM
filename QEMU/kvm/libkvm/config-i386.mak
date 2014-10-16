
LIBDIR := /lib
CFLAGS += -m32
CFLAGS += -D__i386__

libkvm-$(ARCH)-objs := libkvm-x86.o
