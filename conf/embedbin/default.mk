KERN_BINFILES	:= user/init user/jshell user/ksh user/inittab user/init.sh
KERN_BINFILES	+= user/netd_mom user/jntpd

## Shared libraries, if enabled
ifeq ($(SHARED_ENABLE),yes)
KERN_BINFILES	+= user/ld.so user/libm.so user/libutil.so user/libdl.so
KERN_BINFILES	+= user/libcrypt.so user/libintl.so user/libgcc_wrap.so
KERN_BINFILES	+= user/libz.so.1
KERN_BINFILES	+= user/ldd user/ldconfig
endif

## Pick your TCP stack
ifeq ($(K_ARCH),amd64)
KERN_BINFILES	+= user/vmlinux user/initrd
else
KERN_BINFILES	+= user/netd
endif

## Basic command-line programs
KERN_BINFILES	+= user/jls user/asprint user/gl user/jcat user/taintcat
KERN_BINFILES	+= user/jrm user/jcp user/jecho user/mount user/umount
KERN_BINFILES	+= user/jsync user/jmkdir user/reboot user/ureboot
KERN_BINFILES	+= user/jenv user/ps user/wrap user/newpty
KERN_BINFILES	+= user/auth_log user/auth_dir user/auth_user
KERN_BINFILES	+= user/login user/adduser user/passwd user/spawn

## Common Unix tools
KERN_BINFILES	+= user/ln user/tr user/expr user/chmod user/touch user/sort
KERN_BINFILES	+= user/find user/sed user/uname user/awk user/perl user/wget
KERN_BINFILES	+= user/tar user/gzip user/gunzip user/ls user/rm user/cp
KERN_BINFILES	+= user/bash user/grep user/egrep user/uniq user/mv user/cat
KERN_BINFILES	+= user/links user/od user/du user/head user/tail user/sync
KERN_BINFILES	+= user/md5sum user/sha1sum user/mkdir user/stty user/printf
KERN_BINFILES	+= user/vim user/echo user/dirname user/basename user/file
KERN_BINFILES	+= user/sleep user/env user/strings user/tty user/xargs
KERN_BINFILES	+= user/rmdir user/diff user/reset user/hostname user/id
KERN_BINFILES	+= user/less user/dd user/ed user/patch user/comm user/wc
KERN_BINFILES	+= user/seq user/fgrep user/date

## SSH
KERN_BINFILES	+= user/sshd user/ssh.tar user/scp user/sshdi
KERN_BINFILES	+= user/ssh user/ssh-agent user/ssh-add

## Development
KERN_BINFILES	+= user/gcc.tar.gz user/include.tar.gz
KERN_BINFILES	+= user/make user/ar user/nm user/objdump user/objcopy
KERN_BINFILES	+= user/strip user/readelf
#KERN_BINFILES	+= user/gdb user/gdbserver
#KERN_BINFILES	+= user/python.tar.gz

## ClamAV
KERN_BINFILES	+= user/clamscan user/clamav_main.cvd user/clamav_daily.cvd

## Graphics stuff
#KERN_BINFILES	+= user/libfreetype.so.6 user/libfontconfig.so.1
#KERN_BINFILES	+= user/fc-cache user/fc-list user/libexpat.so.1
#KERN_BINFILES	+= user/fbconsd user/fonts.tar.gz
#KERN_BINFILES	+= user/fbi user/libexif.so.12

## Miscellaneous stuff: testing, benchmarking, experimental..
KERN_BINFILES	+= user/fetch user/dnstest
#KERN_BINFILES	+= user/db user/dbc
#KERN_BINFILES	+= user/lfs_small user/lfs_large user/execbench
#KERN_BINFILES	+= user/spawnbench user/ipcbench user/true user/syscallbench
#KERN_BINFILES	+= user/admind user/admctl
#KERN_BINFILES	+= user/hello user/readtest user/fptest user/privtest
#KERN_BINFILES	+= user/cpphello user/pftest
#KERN_BINFILES	+= user/netd_vpn user/openvpn user/vpn_start

## What gets run at startup; flags are:
##  "r" for root
##  "a" for passing args
##  "c" for running in root container
INITTAB_ENTRIES	:= /bin/netd_mom:ra /bin/sshdi:ra

#include conf/embedbin/httpd.mk
#include conf/embedbin/dj.mk
