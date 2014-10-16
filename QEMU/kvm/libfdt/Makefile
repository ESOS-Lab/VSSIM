include ../config.mak
include ../user/config.mak

LIBFDT_SRCS = fdt.c fdt_ro.c fdt_wip.c fdt_sw.c fdt_rw.c fdt_strerror.c
LIBFDT_INCLUDES = fdt.h libfdt.h
LIBFDT_EXTRA = libfdt_internal.h
LIBFDT_LIB = libfdt.a

LIBFDT_OBJS = $(LIBFDT_SRCS:%.c=%.o)

CFLAGS += -I .

$(LIBFDT_LIB): $(LIBFDT_OBJS)
	$(AR) rcs $@ $^

all: $(LIBFDT_LIB)

clean:
	rm -rf *.o *.a
