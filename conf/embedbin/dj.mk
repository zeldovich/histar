KERN_BINFILES   += user/bootstrapd user/djwebappd user/eprocd-launch
KERN_BINFILES   += user/djd user/djc user/ctallocd user/djechod user/djfsd
KERN_BINFILES   += user/djperld user/djauthproxy user/djlogin
KERN_BINFILES	+= user/djguardcall user/remote-run user/djmeasure
KERN_BINFILES   += user/bootstrapc user/zhelper.sh

ifeq ($(SHARED_ENABLE),yes)
KERN_BINFILES	+= user/libgmp.so.3 user/libasync.so.0 user/libarpc.so.0
KERN_BINFILES	+= user/libsfscrypt.so.0 user/libsvc.so.0 user/libsfsmisc.so.0
endif

INITTAB_ENTRIES += /bin/djd:rc /bin/ctallocd: /bin/djfsd:
INITTAB_ENTRIES += /bin/djechod: /bin/djauthproxy: /bin/djwebappd:
INITTAB_ENTRIES += /bin/djguardcall: /bin/bootstrapd:r
