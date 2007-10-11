KERN_BINFILES	:= user/init user/jshell user/ksh user/inittab
KERN_BINFILES	+= user/netd_mom user/jntpd

## Shared libraries (for amd64)
KERN_BINFILES	+= user/ld.so user/libm.so user/libutil.so user/libdl.so

## Pick your TCP stack
KERN_BINFILES	+= user/netd
#KERN_BINFILES	+= user/vmlinux user/initrd

## Basic command-line programs
KERN_BINFILES	+= user/ls user/asprint user/gl user/jcat user/cat
KERN_BINFILES	+= user/rm user/cp user/echo user/mount user/umount
KERN_BINFILES	+= user/sync user/mkdir user/reboot user/ureboot
KERN_BINFILES	+= user/jenv user/ps user/wrap
KERN_BINFILES	+= user/auth_log user/auth_dir user/auth_user
KERN_BINFILES	+= user/login user/adduser user/passwd

## Common Unix tools
#KERN_BINFILES	+= user/ln user/tr user/expr user/chmod user/touch user/sort
#KERN_BINFILES	+= user/find user/sed user/uname user/awk user/perl user/wget
#KERN_BINFILES	+= user/tar user/gzip user/gunzip
#KERN_BINFILES	+= user/vim user/bash user/grep user/egrep user/uniq user/mv
#KERN_BINFILES	+= user/links user/od user/du user/head user/tail
#KERN_BINFILES	+= user/md5sum user/sha1sum

## SSH
#KERN_BINFILES	+= user/ptyd user/sshd user/ssh.tar user/scp user/sshdi

## Development
#KERN_BINFILES	+= user/gcc.tar.gz user/include.tar.gz
#KERN_BINFILES	+= user/make user/ar user/nm user/gdbserver
#KERN_BINFILES	+= user/python.tar.gz

## ClamAV
#KERN_BINFILES	+= user/clamscan user/clamwrap
#KERN_BINFILES	+= user/clamav_main.cvd user/clamav_daily.cvd

## Miscellaneous stuff: testing, benchmarking, experimental..
KERN_BINFILES	+= user/fetch user/dnstest
#KERN_BINFILES	+= user/db user/dbc
#KERN_BINFILES	+= user/lfs_small user/lfs_large user/execbench
#KERN_BINFILES	+= user/spawnbench user/ipcbench user/true user/syscallbench
#KERN_BINFILES	+= user/admind user/admctl
#KERN_BINFILES	+= user/hello user/readtest user/fptest user/privtest
#KERN_BINFILES	+= user/cpphello user/pftest
#KERN_BINFILES	+= user/netd_vpn user/openvpn user/vpn_start

## What gets run at startup; flags are "r" for root, "a" for passing args
INITTAB_ENTRIES	:= /bin/netd_mom:ra /bin/ptyd:ra /bin/sshdi:ra

#include conf/embedbin/httpd.mk
#include conf/embedbin/dj.mk
