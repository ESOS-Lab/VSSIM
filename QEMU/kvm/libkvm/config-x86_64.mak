
LIBDIR := /lib64
CFLAGS += -m64
CFLAGS += -D__x86_64__

libkvm-$(ARCH)-objs := libkvm-x86.o
