KERN_BINFILES   += user/xhelper user/djwebappd user/eprocd-launch
KERN_BINFILES   += user/djd user/djc user/ctallocd user/djechod user/djfsd
KERN_BINFILES   += user/djperld user/djauthproxy user/djlogin user/djguardcall user/remote-run
KERN_BINFILES   += user/dhelper user/zhelper.sh

INITTAB_ENTRIES += /bin/djd:root /bin/ctallocd: /bin/djfsd: /bin/djechod: /bin/djauthproxy: /bin/djwebappd: /bin/djguardcall: /bin/xhelper:r
