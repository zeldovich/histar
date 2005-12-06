#
# This makefile system follows the structuring conventions
# recommended by Peter Miller in his excellent paper:
#
#	Recursive Make Considered Harmful
#	http://aegis.sourceforge.net/auug97.pdf
#
OBJDIR := obj

ifdef GCCPREFIX
SETTINGGCCPREFIX := true
else
-include conf/gcc.mk
endif

TOP :=	$(shell echo $${PWD-`pwd`})

# Cross-compiler jos toolchain
#
# This Makefile will automatically use the cross-compiler toolchain
# installed as 'TARGET-*', if one exists.  If the host tools ('gcc',
# 'objdump', and so forth) compile for a 32-bit x86 ELF target, that will
# be detected as well.  If you have the right compiler toolchain installed
# using a different name, set GCCPREFIX explicitly by doing

ARCH	:= amd64
TARGET	:= x86_64-jos-linux
OBJTYPE	:= elf64-x86-64

CC	:= $(GCCPREFIX)gcc -pipe
CXX	:= $(GCCPREFIX)c++ -pipe
GCC_LIB := $(shell $(CC) -print-libgcc-file-name)
AS	:= $(GCCPREFIX)as
AR	:= $(GCCPREFIX)ar
LD	:= $(GCCPREFIX)ld
OBJCOPY	:= $(GCCPREFIX)objcopy
OBJDUMP	:= $(GCCPREFIX)objdump
NM	:= $(GCCPREFIX)nm

# Native commands
NCC	:= gcc $(CC_VER) -pipe
TAR	:= gtar
PERL	:= perl

# Compiler flags
# Note that -O2 is required for the boot loader to fit within 512 bytes;
# -fno-builtin is required to avoid refs to undefined functions in the kernel.
DEFS	:=
CFLAGS	:= -g -Wall -Werror
#CFLAGS	:= -g -Wall -Werror -O2 -fno-strict-aliasing
CSTD	:= -std=c99
INCLUDES := -I$(TOP) -I$(TOP)/kern -I$(OBJDIR) \
	-I$(TOP)/inc/net -I$(TOP)/inc/net/ipv4

# Linker flags for user programs
LDEPS	:= $(OBJDIR)/lib/entry.o $(OBJDIR)/lib/libjos.a
LDFLAGS := -nostdlib -L$(OBJDIR)/lib
LIBS	:= $(OBJDIR)/lib/entry.o -ljos $(GCC_LIB)

# Lists that the */Makefrag makefile fragments will add to
OBJDIRS :=

# Make sure that 'all' is the first target
all:

# Eliminate default suffix rules
.SUFFIXES:

# Delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# make it so that no intermediate .o files are ever deleted
.PRECIOUS: %.o $(OBJDIR)/boot/%.o $(OBJDIR)/kern/%.o \
	$(OBJDIR)/lib/%.o $(OBJDIR)/fs/%.o $(OBJDIR)/asfs/%.o \
	$(OBJDIR)/user/%.o

COMFLAGS   := $(CFLAGS) -nostdinc -Wall -MD # -fno-builtin 
COMCXXFLAGS := -fno-exceptions -fno-rtti
KFLAGS     := -msoft-float -mno-red-zone -mcmodel=kernel -fno-builtin
KERN_CFLAGS := $(KFLAGS) $(COMFLAGS) -Werror $(DEFS) $(INCLUDES) -DJOS_KERNEL
KERN_CXXFLAGS := $(KERN_CFLAGS) $(COMCXXFLAGS)
USER_CFLAGS := $(COMFLAGS) $(DEFS) $(CFLAGS) $(INCLUDES) -DJOS_USER
USER_CXXFLAGS := $(USER_CFLAGS) $(COMCXXFLAGS)
HOST_CFLAGS := $(CFLAGS) -Ihostinc/


# try to infer the correct GCCPREFIX
conf/gcc.mk:
	@if $(TARGET)-objdump -i 2>&1 | grep '^$(OBJTYPE)$$' >/dev/null 2>&1; \
	then echo 'GCCPREFIX=$(TARGET)-' >conf/gcc.mk; \
	elif objdump -i 2>&1 | grep '^$(OBJTYPE)$$' >/dev/null 2>&1; \
	then echo 'GCCPREFIX=' >conf/gcc.mk; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find GCC/binutils for $(OBJTYPE)." 1>&2; \
	echo "*** Is the directory with $(TARGET)-gcc in your PATH?" 1>&2; \
	echo "*** If $(OBJTYPE) toolchain is installed with a command" 1>&2; \
	echo "*** prefix other than '$(TARGET)-', set your GCCPREFIX" 1>&2; \
	echo "*** environment variable to that prefix re-run 'gmake'." 1>&2; \
	echo "*** To turn off this error: echo GCCPREFIX= >conf/gcc.mk" 1>&2; \
	echo "***" 1>&2; exit 1; fi

# variables should be recursive
FSIMGTXTFILES =
USERAPPS =
OKWSAPPS =
OKDBAPPS =

# Include Makefrags for subdirectories
include boot/Makefrag
include kern/Makefrag
include lib/Makefrag
include user/Makefrag
#include fs/Makefrag
#include asfs/Makefrag
#include okws/Makefrag
#include okdb/Makefrag

bochs: $(OBJDIR)/kern/bochs.img $(OBJDIR)/fs/fs.img
	bochs-nogui

# For deleting the build
clean:
	rm -rf $(OBJDIR)/.deps $(OBJDIR)/*
	rm -f bochs.log

distclean: clean
	rm -f conf/gcc.mk
	find . -type f \( -name '*~' -o -name '.*~' \) -exec rm -f \{\} \;

# This magic automatically generates makefile dependencies
# for header files included from C source files we compile,
# and keeps those dependencies up-to-date every time we recompile.
# See 'mergedep.pl' for more information.
$(OBJDIR)/.deps: $(foreach dir, $(OBJDIRS), $(wildcard $(OBJDIR)/$(dir)/*.d))
	@mkdir -p $(@D)
	$(PERL) mergedep.pl $@ $^

-include $(OBJDIR)/.deps

GNUmakefile: obj/machine
obj/machine:
	@mkdir -p $(@D)
	ln -s $(TOP)/kern/arch/$(ARCH) $@

always:
	@:

.PHONY: all always clean distclean
