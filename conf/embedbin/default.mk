KERN_BINFILES	:= user/idle user/init user/jshell user/ksh user/inittab
KERN_BINFILES	+= user/netd user/netd_mom
#KERN_BINFILES	+= user/telnetd user/httpd user/httpd_worker user/tcpserv user/ftpd user/jntpd
KERN_BINFILES	+= user/fetch user/dnstest
#KERN_BINFILES	+= user/db user/dbc
#KERN_BINFILES	+= user/lfs_small user/lfs_large user/execbench user/spawnbench user/ipcbench user/true user/syscallbench
KERN_BINFILES	+= user/ls user/asprint user/gl user/jcat user/cat user/rm user/cp user/echo user/mount user/umount user/sync user/mkdir user/reboot user/ureboot
#KERN_BINFILES	+= user/admind user/admctl
KERN_BINFILES	+= user/auth_log user/auth_dir user/auth_user user/login user/adduser user/passwd
#KERN_BINFILES	+= user/hello user/readtest user/fptest user/privtest user/cpphello user/pftest
#KERN_BINFILES	+= user/netd_vpn user/openvpn user/vpn_start
#KERN_BINFILES	+= user/clamscan user/clamwrap user/clamav_main.cvd user/clamav_daily.cvd

#KERN_BINFILES	+= user/tar user/gzip user/gunzip
#KERN_BINFILES	+= user/gcc.tar.gz user/make user/kern.tar.gz user/setup.sh
#KERN_BINFILES	+= user/ptyd user/sshd user/ssh.tar user/scp user/sshdi
#KERN_BINFILES	+= user/vim user/bash

INITTAB_ENTRIES	:= /bin/netd_mom:root /bin/ptyd:root /bin/sshdi:root

#include conf/embedbin/httpd.mk
#include conf/embedbin/dj.mk
