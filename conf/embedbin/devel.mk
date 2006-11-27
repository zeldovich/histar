EMBEDBIN_ID = conf/embedbin/devel.mk
EMBEDBIN_INIT = conf/embedbin/devel.init

KERN_BINFILES += user/init.init
KERN_BINFILES += user/devinit user/devpt user/reboot user/ureboot
KERN_BINFILES += user/sshd user/ssh.tar user/scp
KERN_BINFILES += user/vim 
KERN_BINFILES += user/bash
KERN_BINFILES += user/sshdi
KERN_BINFILES += user/tar

include conf/embedbin/embedbins.mk
