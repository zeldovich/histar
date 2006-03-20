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
STRIP	:= $(GCCPREFIX)strip
LEX	:= flex

# Native commands
NCC	:= gcc $(CC_VER) -pipe
TAR	:= gtar
PERL	:= perl

# Compiler flags.

WARNS	 := -Wformat=2 -Wextra -Wshadow -Wmissing-noreturn -Wcast-align \
	    -Wwrite-strings -Wno-unused-parameters -Wmissing-format-attribute
CWARNS	 := $(WARNS) -Wmissing-prototypes -Wmissing-declarations
CXXWARNS := $(WARNS)
# Too many false positives:
# -Wconversion -Wcast-qual -Wunreachable-code -Wbad-function-cast -Winline

OPTFLAG := -O2
#OPTFLAG := -O3 -march=opteron
CFLAGS	:= -g -fms-extensions $(OPTFLAG) -fno-strict-aliasing
CSTD	:= -std=c99
INCLUDES := -I$(TOP) -I$(TOP)/kern -I$(TOP)/$(OBJDIR)

# Linker flags for user programs
CRT1	:= $(OBJDIR)/lib/crt1.o
CRTI	:= $(OBJDIR)/lib/crti.o
CRTN	:= $(OBJDIR)/lib/crtn.o

LDEPS	:= $(CRT1) $(CRTI) $(CRTN) \
	   $(OBJDIR)/lib/libjos.a \
	   $(OBJDIR)/lib/liblwip.a \
	   $(OBJDIR)/lib/libc.a
LDFLAGS := -B$(TOP)/$(OBJDIR)/lib -L$(TOP)/$(OBJDIR)/lib
LIBS	:= -ljos -llwip -lc -ljos -lc

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
	$(OBJDIR)/user/%.o $(OBJDIR)/user/%.debuginfo

COMFLAGS   := $(CFLAGS) -Wall -MD
COMCXXFLAGS := 
KFLAGS     := -msoft-float -mno-red-zone -mcmodel=kernel -fno-builtin
KERN_INC   := $(INCLUDES)
KERN_CFLAGS := $(KFLAGS) $(COMFLAGS) $(KERN_INC) -DJOS_KERNEL $(CWARNS) -Werror $(KERN_PROF)
KERN_CXXFLAGS := $(KERN_CFLAGS) $(COMCXXFLAGS)
USER_INC   := $(INCLUDES)
USER_COMFLAGS := $(COMFLAGS) $(USER_INC) -DJOS_USER
USER_CFLAGS := $(USER_COMFLAGS) $(CWARNS)
USER_CXXFLAGS := $(USER_COMFLAGS) $(COMCXXFLAGS) $(CXXWARNS)


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
include pkg/Makefrag
include acpkg/Makefrag

bochs: $(OBJDIR)/kern/bochs.img $(OBJDIR)/fs/fs.img
	bochs-nogui

tags:
	@:

# For deleting the build
clean:
	rm -rf $(OBJDIR)/.deps $(OBJDIR)/*
	rm -f bochs.log

distclean: clean
	rm -f conf/gcc.mk
	find . -type f \( -name '*~' -o -name '.*~' \) -exec rm -f \{\} \;

# Need a rebuild when switching b/t prof and non-prof output...
prof:
	make 'KERN_PROF=-finstrument-functions'

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

.PHONY: all always clean distclean tags
