KERN_BINFILES	+= user/server.pem user/dh.pem user/servkey.pem
KERN_BINFILES	+= user/httpd user/httpd_mom user/ssld user/ssl_eprocd
KERN_BINFILES   += user/inetd user/httpd2 user/modulei

KERN_BINFILES	+= user/gs user/gs.tar
KERN_BINFILES   += user/a2ps user/a2ps.tar

INITTAB_ENTRIES += /bin/httpd_mom:root
