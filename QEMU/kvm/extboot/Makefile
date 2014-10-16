OBJCOPY=objcopy

# from kernel sources - scripts/Kbuild.include
# try-run
# Usage: option = $(call try-run, $(CC)...-o "$$TMP",option-ok,otherwise)
# Exit code chooses option. "$$TMP" is can be used as temporary file and
# is automatically cleaned up.
try-run = $(shell set -e;		\
	TMP="$(TMPOUT).$$$$.tmp";	\
	if ($(1)) >/dev/null 2>&1;	\
	then echo "$(2)";		\
	else echo "$(3)";		\
	fi;				\
	rm -f "$$TMP")

# cc-option-yn
# Usage: flag := $(call cc-option-yn,-march=winchip-c6)
cc-option-yn = $(call try-run,\
	$(CC) $(KBUILD_CFLAGS) $(1) -S -xc /dev/null -o "$$TMP",y,n)

CFLAGS = -Wall -Wstrict-prototypes -Werror -fomit-frame-pointer -fno-builtin
ifeq ($(call cc-option-yn,-fno-stack-protector),y)
CFLAGS += -fno-stack-protector
endif

all: extboot.bin

%.o: %.S
	$(CC) $(CFLAGS) -o $@ -c $<

extboot.img: extboot.o
	$(LD) --oformat binary -Ttext 0 -o $@ $<

extboot.bin: extboot.img signrom
	./signrom extboot.img extboot.bin

signrom: signrom.c
	$(CC) -o $@ -g -Wall $^

clean:
	$(RM) *.o *.img *.bin signrom *~
