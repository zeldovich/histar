## Target kernel architecture/type, one of the following:
##	amd64 i386 ft sparc um arm
K_ARCH := amd64

## Create a separate build directory for each git branch and for each arch
OBJSUFFIX := $(shell git-symbolic-ref -q HEAD | \
	       sed -e s,refs/heads/,.,).$(K_ARCH)

