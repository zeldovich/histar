KERN_BINFILES	:= user/init user/jshell user/ksh user/inittab user/init.sh
KERN_BINFILES	+= user/netd_mom

## Shared libraries, if enabled
ifeq ($(SHARED_ENABLE),yes)
KERN_BINFILES	+= user/ld.so user/libm.so user/libutil.so user/libdl.so
KERN_BINFILES	+= user/libintl.so
#KERN_BINFILES	+= user/libcrypt.so user/libintl.so user/libgcc_wrap.so
#KERN_BINFILES	+= user/libz.so.1
#KERN_BINFILES	+= user/libncurses.so.5
#KERN_BINFILES	+= user/ldd user/ldconfig
endif

## Pick your TCP stack
ifeq ($(K_ARCH),amd64)
#KERN_BINFILES	+= user/vmlinux user/initrd
else
KERN_BINFILES	+= user/netd
endif

## Basic command-line programs
KERN_BINFILES	+= user/jls user/asprint user/gl user/jcat user/taintcat
KERN_BINFILES	+= user/jrm user/jcp user/jecho user/mount user/umount
KERN_BINFILES	+= user/jsync user/jmkdir user/reboot user/ureboot
KERN_BINFILES	+= user/jenv user/ps user/wrap user/newpty user/jtrue
#KERN_BINFILES	+= user/auth_log user/auth_dir user/auth_user
#KERN_BINFILES	+= user/login user/adduser user/passwd user/spawn user/jtrace
KERN_BINFILES	+= user/jtrace

## Common Unix tools
#KERN_BINFILES	+= user/ln user/tr user/expr user/chmod user/touch user/sort
KERN_BINFILES	+= user/ln user/tr user/expr user/chmod user/touch
KERN_BINFILES	+= user/uname
KERN_BINFILES   += user/wget
KERN_BINFILES	+= user/cp
KERN_BINFILES	+= user/mv
KERN_BINFILES	+= user/rm
KERN_BINFILES	+= user/ls
KERN_BINFILES	+= user/sleep
#KERN_BINFILES	+= user/find user/sed user/uname user/awk user/perl
#KERN_BINFILES	+= user/tar user/gzip user/gunzip user/ls user/rm user/cp
#KERN_BINFILES	+= user/bash user/grep user/egrep user/uniq user/mv user/cat
#KERN_BINFILES	+= user/links user/od user/du user/head user/tail user/sync
#KERN_BINFILES	+= user/md5sum user/sha1sum user/mkdir user/stty user/printf
#KERN_BINFILES	+= user/vim user/echo user/dirname user/basename user/file
#KERN_BINFILES	+= user/sleep user/env user/strings user/tty user/xargs
#KERN_BINFILES	+= user/rmdir user/diff user/reset user/hostname user/id
#KERN_BINFILES	+= user/less user/dd user/ed user/patch user/comm user/wc
#KERN_BINFILES	+= user/seq user/fgrep user/date user/tee user/pwd
#KERN_BINFILES	+= user/bzip2 user/bunzip2 user/cmp user/cut user/true
#KERN_BINFILES	+= user/install user/zcat user/whoami
#KERN_BINFILES	+= user/terminfo.tar.gz

## SSH
#KERN_BINFILES   += user/ssh

## Development
#KERN_BINFILES	+= user/gdb user/gdbserver

## Graphics stuff
#KERN_BINFILES	+= user/fbconsd

## Miscellaneous stuff: testing, benchmarking, experimental..
KERN_BINFILES	+= user/swbench
KERN_BINFILES	+= user/udptest
KERN_BINFILES	+= user/spin
KERN_BINFILES	+= user/spinbench

## HTC Radio junk
ifeq ($(K_ARCH),arm)
ifeq ($(SHARED_ENABLE),yes)
KERN_BINFILES	+= blob/libgps.so
KERN_BINFILES	+= blob/libhtc_ril.so
KERN_BINFILES	+= blob/misc_partition.img
KERN_BINFILES	+= blob/AudioPara4.csv
KERN_BINFILES	+= user/smdd
KERN_BINFILES	+= user/battd
KERN_BINFILES	+= user/gpsd
KERN_BINFILES	+= user/smsd
KERN_BINFILES	+= user/smsc
KERN_BINFILES	+= user/rild
KERN_BINFILES	+= user/rilc
KERN_BINFILES	+= user/radiooptions
KERN_BINFILES	+= user/bard
KERN_BINFILES	+= user/rmnetstat
endif
endif

KERN_BINFILES	+= user/irqwait

# movemail
KERN_BINFILES	+= user/movemail user/libmu_cfg.so.0 user/libmu_mbox.so.2
KERN_BINFILES	+= user/libmu_imap.so.2 user/libmu_pop.so.2 user/libmu_auth.so.2
KERN_BINFILES	+= user/libmu_nntp.so.2 user/libmu_mh.so.2 user/libmu_maildir.so.2
KERN_BINFILES	+= user/libmailutils.so.2

# energy junk
KERN_BINFILES	+= user/reserve-test
KERN_BINFILES	+= user/energywrap
KERN_BINFILES	+= user/energycompete
KERN_BINFILES	+= user/energyforeground
KERN_BINFILES	+= user/stfu
KERN_BINFILES	+= user/radiobench
KERN_BINFILES	+= user/mmtest.sh
KERN_BINFILES	+= user/mmtest_sleep.sh
KERN_BINFILES	+= user/rsstest.sh
KERN_BINFILES	+= user/rsstest_sleep.sh
KERN_BINFILES	+= user/backgroundapps.sh
KERN_BINFILES	+= user/backgroundapps
KERN_BINFILES	+= user/memspin
KERN_BINFILES	+= user/backgroundapps_timeronly

# try to hunt down bug?
KERN_BINFILES	+= user/spin.sh
KERN_BINFILES	+= user/backgroundapps_spin.sh

## What gets run at startup; flags are:
##  "r" for root
##  "a" for passing args
##  "c" for running in root container
INITTAB_ENTRIES := /bin/netd_mom:ra
