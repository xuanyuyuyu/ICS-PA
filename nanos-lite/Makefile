NANOS_HOME := $(CURDIR)
NANOS_KCONFIG := $(NANOS_HOME)/Kconfig
NANOS_CONF := $(NEMU_HOME)/tools/kconfig/build/conf
NANOS_MCONF := $(NEMU_HOME)/tools/kconfig/build/mconf

# Configuration-only goals must not include the Abstract Machine build system,
# which requires ARCH and would try to build an image.
ifneq ($(filter menuconfig olddefconfig,$(MAKECMDGOALS)),)

$(NANOS_CONF):
	@$(MAKE) -s -C $(NEMU_HOME)/tools/kconfig NAME=conf

$(NANOS_MCONF):
	@$(MAKE) -s -C $(NEMU_HOME)/tools/kconfig NAME=mconf

menuconfig: $(NANOS_MCONF) $(NANOS_CONF)
	@cd $(NANOS_HOME) && $(NANOS_MCONF) $(NANOS_KCONFIG)
	@cd $(NANOS_HOME) && $(NANOS_CONF) -s --syncconfig $(NANOS_KCONFIG)

olddefconfig: $(NANOS_CONF)
	@cd $(NANOS_HOME) && $(NANOS_CONF) -s --olddefconfig $(NANOS_KCONFIG)
	@cd $(NANOS_HOME) && $(NANOS_CONF) -s --syncconfig $(NANOS_KCONFIG)

.PHONY: menuconfig olddefconfig

else

-include $(NANOS_HOME)/include/config/auto.conf
CFLAGS += $(if $(CONFIG_STRACE),-DCONFIG_STRACE,)

#HAS_NAVY = 1
RAMDISK_FILE = build/ramdisk.img

NAME = nanos-lite
SRCS = $(shell find -L ./src/ -name "*.c" -o -name "*.cpp" -o -name "*.S")
include $(AM_HOME)/Makefile

ifeq ($(ARCH),native)
ISA = am_native
else
INC_PATH += include $(NAVY_HOME)/libs/libc/include
endif

./src/resources.S: $(RAMDISK_FILE)
	@touch $@

ifeq ($(HAS_NAVY),)
files = $(RAMDISK_FILE) src/files.h src/syscall.h
# create an empty file if it does not exist
$(foreach f,$(files),$(if $(wildcard $f),, $(shell touch $f)))
else

ifeq ($(wildcard $(NAVY_HOME)/libs/libos/src/syscall.h),)
  $(error $$NAVY_HOME must be a Navy-apps repo)
endif

update:
	$(MAKE) -s -C $(NAVY_HOME) ISA=$(ISA) ramdisk
	@ln -sf $(NAVY_HOME)/build/ramdisk.img $(RAMDISK_FILE)
	@ln -sf $(NAVY_HOME)/build/ramdisk.h src/files.h
	@ln -sf $(NAVY_HOME)/libs/libos/src/syscall.h src/syscall.h

.PHONY: update
endif

# Recompile nanos-lite when a menuconfig option changes.
ifneq ($(wildcard $(NANOS_HOME)/include/config/auto.conf),)
$(OBJS): $(NANOS_HOME)/include/config/auto.conf
endif

endif
