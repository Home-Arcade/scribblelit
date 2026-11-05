APP_VERSION ?= 0.2.9
include $(DEVKITARM)/3ds_rules
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
PORTLIBS := $(DEVKITPRO)/portlibs/3ds
CFLAGS := -g -Wall -O2 -mword-relocations -ffunction-sections $(ARCH) -D__3DS__ -D_GNU_SOURCE -DAPP_VERSION=\"$(APP_VERSION)\" \
	-I$(CTRULIB)/include -I$(PORTLIBS)/include
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -L$(CTRULIB)/lib -L$(PORTLIBS)/lib

all: SLUpdater.elf ScribbleLit.elf
ScribbleLit.elf: source/main.c source/ui.c source/sl_core.c source/sl_find.c source/sl_forward.c source/ips.c source/ui.h source/sl_core.h source/sl_find.h source/sl_patches.h source/letter_art.h
	$(CC) $(CFLAGS) source/main.c source/ui.c source/sl_core.c source/sl_find.c source/sl_forward.c source/ips.c -o $@ $(LDFLAGS) -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lz -lcitro2d -lcitro3d -lctru -lm
SLUpdater.elf: updater/main.c
	$(CC) $(CFLAGS) updater/main.c -o $@ $(LDFLAGS) -lctru -lm
clean:
	rm -f ScribbleLit.elf SLUpdater.elf
