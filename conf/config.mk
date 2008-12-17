## Target kernel architecture/type, one of the following:
##	amd64 i386 ft sparc um arm
K_ARCH := nacl

## Use a separate obj directory for each target architecture;
## useful for building multiple architectures in the same tree.
MULTIOBJ := yes

## Additional suffix for the obj directory name, to distinguish
## multiple builds of the same arch in the same tree
OBJSUFFIX := 

