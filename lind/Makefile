#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies.
#
# Copyright (C) 2002 Jeff Dike (jdike@karaya.com)
# Licensed under the GPL
#

ARCH_DIR := arch/lind

OS ?= $(shell uname -s)

# We require bash because the vmlinux link and loader script cpp use bash
# features.
SHELL := /bin/bash

head-y := 
libs-y += arch/lind/lib/
core-y			+= $(ARCH_DIR)/kernel/		\
			   $(ARCH_DIR)/drivers/		\
			   $(ARCH_DIR)/mm/		\
			   $(ARCH_DIR)/fs/		\
			   $(ARCH_DIR)/os-lib/		\
			   $(ARCH_DIR)/os-$(OS)/

ARCH_SYMLINKS = $(ARCH_DIR)/os include/asm-lind

ifneq ($(MAKEFILES-INCL),)
  include $(MAKEFILES-INCL)
endif

ARCH_INCLUDE	:= -I$(ARCH_DIR) -I$(ARCH_DIR)/include
ifneq ($(KBUILD_SRC),)
ARCH_INCLUDE	+= -I$(srctree)/$(ARCH_DIR) -I$(srctree)/$(ARCH_DIR)/include
endif
SYS_DIR		:= $(ARCH_DIR)/include/sysdep-$(SUBARCH)

# -Dvmap=kernel_vmap prevents anything from referencing the libpcap.o symbol so
# named - it's a common symbol in libpcap, so we get a binary which crashes.
#
# Same things for in6addr_loopback and mktime - found in libc. For these two we
# only get link-time error, luckily.
#
# These apply to USER_CFLAGS to.

CFLAGS += $(CFLAGS-y) -D__arch_lind__ -DSUBARCH=\"$(SUBARCH)\"	\
	$(ARCH_INCLUDE) $(MODE_INCLUDE) -Dvmap=kernel_vmap	\
	-Din6addr_loopback=kernel_in6addr_loopback -g

AFLAGS += $(ARCH_INCLUDE) -g

USER_CFLAGS := $(patsubst $(KERNEL_DEFINES),,$(patsubst -D__KERNEL__,,\
	$(patsubst -I%,,$(CFLAGS)))) $(ARCH_INCLUDE) $(MODE_INCLUDE) \
	-D_FILE_OFFSET_BITS=64

#include $(srctree)/$(ARCH_DIR)/Makefile-$(SUBARCH)

#This will adjust *FLAGS accordingly to the platform.
include $(srctree)/$(ARCH_DIR)/Makefile-os-$(OS)

# -Derrno=kernel_errno - This turns all kernel references to errno into
# kernel_errno to separate them from the libc errno.  This allows -fno-common
# in CFLAGS.  Otherwise, it would cause ld to complain about the two different
# errnos.
# These apply to kernelspace only.
KERNEL_DEFINES = -Derrno=kernel_errno -Dsigprocmask=kernel_sigprocmask \
	-Dmktime=kernel_mktime -Dstrchr=kernel_strchr $(ARCH_KERNEL_DEFINES)
CFLAGS += $(KERNEL_DEFINES)
CFLAGS += $(call cc-option,-fno-unit-at-a-time,)

# These are needed for clean and mrproper, since in that case .config is not
# included; the values here are meaningless

CONFIG_NEST_LEVEL ?= 0
CONFIG_KERNEL_HALF_GIGS ?= 0

SIZE = (($(CONFIG_NEST_LEVEL) + $(CONFIG_KERNEL_HALF_GIGS)) * 0x20000000)

PHONY += linux

all: vmlinux

archprepare: $(ARCH_SYMLINKS)

CONFIG_KERNEL_STACK_ORDER ?= 2
STACK_SIZE := $(shell echo $$[ 4096 * (1 << $(CONFIG_KERNEL_STACK_ORDER)) ] )

ifndef START
  START = $(shell echo $$[ $(TOP_ADDR) - $(SIZE) ] )
endif

# Should be in a "Makefile.sysdep"
ELF_FORMAT := elf64-x86-64
ELF_ARCH   := i386:x86-64

CPPFLAGS_vmlinux.lds = $(ARCH_INCLUDE) \
	-DSTART=$(START) $(CPP_MODE-y) \
	-DKERNEL_STACK_SIZE=$(STACK_SIZE) \
	-DELF_ARCH=$(ELF_ARCH) \
	-DELF_FORMAT="$(ELF_FORMAT)"

#The wrappers will select whether using "malloc" or the kernel allocator.
#LINK_WRAPS = -Wl,--wrap,malloc -Wl,--wrap,free -Wl,--wrap,calloc

CFLAGS_vmlinux :=
define cmd_vmlinux__
	$(CC) $(CFLAGS_vmlinux) $(LIND_LDFLAGS) -o $@ \
	-Wl,-T,$(vmlinux-lds) $(vmlinux-init) \
	-Wl,--start-group $(vmlinux-main) $(LIND_LIBS) -Wl,--end-group \
	$(filter-out $(vmlinux-lds) $(vmlinux-init) $(vmlinux-main) \
	-lutil \
	FORCE ,$^) ; rm -f linux
endef

CLEAN_FILES += $(ARCH_DIR)/os

MRPROPER_FILES += $(ARCH_SYMLINKS)

archclean:
	@find . \( -name '*.bb' -o -name '*.bbg' -o -name '*.da' \
		-o -name '*.gcov' \) -type f -print | xargs rm -f

$(ARCH_DIR)/os:
	@echo '  SYMLINK $@'
ifneq ($(KBUILD_SRC),)
	mkdir -p $(ARCH_DIR)
	$(Q)ln -fsn $(srctree)/$(ARCH_DIR)/os-$(OS) $(ARCH_DIR)/os
else
	$(Q)cd $(ARCH_DIR) && ln -sf os-$(OS) os
endif

include/asm-lind:
	@echo '  SYMLINK $@'
	$(Q)cd include && ln -sf ../$(ARCH_DIR)/asm-lind asm-lind

export SUBARCH USER_CFLAGS OS
