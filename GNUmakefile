#
# This makefile system follows the structuring conventions
# recommended by Peter Miller in his excellent paper:
#
#	Recursive Make Considered Harmful
#	http://aegis.sourceforge.net/auug97.pdf
#

# Target kernel architecture/type

K_ARCH	:= amd64
#K_ARCH	:= i386
#K_ARCH	:= ft
#K_ARCH	:= sparc
#K_ARCH	:= um

## Use multiple objdirs to build multiple architectures with same source tree
OBJDIR	:= obj
#OBJDIR	:= obj.$(K_ARCH)

-include conf/gcc.$(K_ARCH).mk
include conf/Makefrag.$(K_ARCH)

GCC_LIB := $(shell $(CC) -print-libgcc-file-name)
TOP	:= $(shell echo $${PWD-`pwd`})

# Native commands
NCC	:= gcc $(CC_VER) -pipe
TAR	:= gtar
PERL	:= perl
LEX	:= flex
YACC	:= bison
RPCC	:= rpcc
TAME 	:= tame

# Compiler flags.
COMWARNS := -Wformat=2 -Wextra -Wmissing-noreturn \
	    -Wwrite-strings -Wno-unused-parameter -Wmissing-format-attribute \
	    -Wswitch-default -fno-builtin-fork
CWARNS	 := $(COMWARNS) -Wmissing-prototypes -Wmissing-declarations -Wshadow
CXXWARNS := $(COMWARNS) -Wno-non-template-friend
# SFS seems to be violating -Woverloaded-virtual
# Too many false positives:
# -Wconversion -Wcast-qual -Wunreachable-code -Wbad-function-cast -Winline
# -Weffc++ -Wswitch-enum -Wcast-align

OPTFLAG	 := -O3 -fno-omit-frame-pointer
ifeq ($(ARCH),amd64)
OPTFLAG  += -march=athlon64
endif

ifeq ($(K_ARCH),ft)
COMWARNS := 
CWARNS	 := 
OPTFLAG	 := 
endif

COMFLAGS := -g $(OPTFLAG) -fno-strict-aliasing -Wall -MD -DJOS_ARCH_$(ARCH)
CSTD	 := -std=c99 -fms-extensions
INCLUDES := -I$(TOP) -I$(TOP)/kern -I$(TOP)/$(OBJDIR)

# Linker flags for user programs
CRT1	:= $(OBJDIR)/lib/crt1.o
CRTI	:= $(OBJDIR)/lib/crti.o
CRTN	:= $(OBJDIR)/lib/crtn.o

LDEPS	:= $(CRT1) $(CRTI) $(CRTN) \
	   $(OBJDIR)/lib/libjos.a \
	   $(OBJDIR)/lib/32/libjos.a \
	   $(OBJDIR)/lib/libnetd.a \
	   $(OBJDIR)/lib/liblwip.a \
	   $(OBJDIR)/lib/libc.a \
	   $(OBJDIR)/lib/libm.a \
	   $(OBJDIR)/lib/libcrypt.a \
	   $(OBJDIR)/lib/libutil.a \
	   $(OBJDIR)/lib/libstdc++.a

LDFLAGS := -B$(TOP)/$(OBJDIR)/lib -L$(TOP)/$(OBJDIR)/lib \
	   -specs=$(TOP)/conf/gcc.specs -static

# Lists that the */Makefrag makefile fragments will add to
OBJDIRS :=

# Make sure that 'all' is the first target
all:

# Eliminate default suffix rules
.SUFFIXES:

# Delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# make it so that no intermediate .o files are ever deleted
.PRECIOUS: %.o $(OBJDIR)/boot/%.o $(OBJDIR)/kern/%.o $(OBJDIR)/lib/%.o \
	$(OBJDIR)/user/%.o $(OBJDIR)/user/%.debuginfo $(OBJDIR)/user/% \
	$(OBJDIR)/kern/kernel.base $(OBJDIR)/kern/kernel.init

KERN_CFLAGS := $(COMFLAGS) $(INCLUDES) -DJOS_KERNEL $(CWARNS) -Werror $(KERN_PROF)
USER_INC    := $(INCLUDES)
USER_COMFLAGS = $(COMFLAGS) $(USER_INC) -DJOS_USER
USER_CFLAGS   = $(USER_COMFLAGS) $(CWARNS)
USER_CXXFLAGS = $(USER_COMFLAGS) $(CXXWARNS) -D__STDC_FORMAT_MACROS

ifeq ($(K_ARCH),sparc)
KERN_CFLAGS += -mrestore
endif

# try to infer the correct GCCPREFIX
conf/gcc.$(K_ARCH).mk:
	@if $(TARGET)-objdump -i 2>&1 | grep '^$(OBJTYPE)$$' >/dev/null 2>&1; \
	then echo 'GCCPREFIX=$(TARGET)-' >conf/gcc.$(K_ARCH).mk; \
	elif objdump -i 2>&1 | grep '^$(OBJTYPE)$$' >/dev/null 2>&1; \
	then echo 'GCCPREFIX=' >conf/gcc.$(K_ARCH).mk; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find GCC/binutils for $(OBJTYPE)." 1>&2; \
	echo "*** Is the directory with $(TARGET)-gcc in your PATH?" 1>&2; \
	echo "*** If $(OBJTYPE) toolchain is installed with a command" 1>&2; \
	echo "*** prefix other than '$(TARGET)-', set your GCCPREFIX" 1>&2; \
	echo "*** environment variable to that prefix re-run 'gmake'." 1>&2; \
	echo "*** To turn off this error:" 1>&2; \
	echo "***     echo GCCPREFIX= >conf/gcc.$(K_ARCH).mk" 1>&2; \
	echo "***" 1>&2; exit 1; fi

# Include Makefrags for subdirectories
include boot/Makefrag
include kern/Makefrag
include lib/Makefrag
include netd/Makefrag
include dj/Makefrag
include pkg/Makefrag
include acpkg/Makefrag
include user/Makefrag
include test/Makefrag

bochs: $(OBJDIR)/kern/bochs.img $(OBJDIR)/fs/fs.img
	bochs-nogui

tags:
	@:

# For deleting the build
clean:
	rm -rf $(OBJDIR)/.deps $(OBJDIR)/* acpkg/*/__build
	rm -f bochs.log

distclean: clean
	rm -f conf/gcc.$(K_ARCH).mk
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

GNUmakefile: $(OBJDIR)/machine
$(OBJDIR)/machine:
	@mkdir -p $(@D)
	ln -s $(TOP)/kern/arch/$(K_ARCH) $@

always:
	@:

.PHONY: all always clean distclean tags
