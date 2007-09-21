# Target: jos64 x86-64 
# removed linux specific stuff from gdb/config/i386/linux64.mt

TDEPFILES= amd64-tdep.o \
        i386-tdep.o i387-tdep.o glibc-tdep.o \
        solib.o solib-svr4.o corelow.o symfile-mem.o
DEPRECATED_TM_FILE= solib.h
