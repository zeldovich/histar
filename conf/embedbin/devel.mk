EMBEDBIN_ID = conf/embedbin/devel.mk
EMBEDBIN_INIT = conf/embedbin/devel.init

KERN_BINFILES += user/init.init
KERN_BINFILES += user/devinit user/devpt user/devnull user/reboot
KERN_BINFILES += user/netdi
KERN_BINFILES += user/sshd user/ssh.tar user/sshdi 
KERN_BINFILES += user/vim user/wget user/tar user/gzip
KERN_BINFILES += user/bash
KERN_BINFILES += user/gcc.tar.gz
KERN_BINFILES += user/make

include conf/embedbin/embedbins.mk