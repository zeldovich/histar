KERN_BINFILES	:= user/init user/jshell user/ksh user/inittab user/init.sh
KERN_BINFILES	+= user/netd_mom user/jntpd

## Shared libraries, if enabled
ifeq ($(SHARED_ENABLE),yes)
KERN_BINFILES	+= user/ld.so user/libm.so user/libutil.so user/libdl.so
KERN_BINFILES	+= user/libcrypt.so user/libintl.so user/libgcc_wrap.so
KERN_BINFILES	+= user/ldd user/ldconfig
endif

## Pick your TCP stack
KERN_BINFILES	+= user/netd

## Basic command-line programs
KERN_BINFILES	+= user/jls user/asprint user/gl user/jcat user/taintcat
KERN_BINFILES	+= user/jrm user/jcp user/jecho user/mount user/umount
KERN_BINFILES	+= user/jsync user/jmkdir user/reboot user/ureboot
KERN_BINFILES	+= user/jenv user/ps user/wrap user/newpty user/jtrue
KERN_BINFILES	+= user/auth_log user/auth_dir user/auth_user
KERN_BINFILES	+= user/login user/adduser user/passwd user/spawn user/jtrace

## What gets run at startup; flags are:
##  "r" for root
##  "a" for passing args
##  "c" for running in root container
INITTAB_ENTRIES	:= /bin/netd_mom:ra
