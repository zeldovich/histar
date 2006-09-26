EMBEDBIN_ID = conf/embedbin/devel.mk
EMBEDBIN_INIT = conf/embedbin/devel.init

KERN_BINFILES += user/init.init
KERN_BINFILES += user/devinit user/devpt user/devnull user/reboot
KERN_BINFILES += user/netdi
KERN_BINFILES += user/sshd user/ssh.tar user/tar user/sshdi 
KERN_BINFILES += user/vim user/wget
KERN_BINFILES += user/bash

include conf/embedbin/embedbins.mk