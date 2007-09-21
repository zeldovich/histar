#
# Makefile for x86_64-specific library files.
#

CFLAGS_csum-partial.o := -funroll-loops

obj-y := io.o iomap_copy.o

lib-y := csum-partial.o csum-copy.o csum-wrappers.o delay.o \
	thunk.o clear_page.o copy_page.o bitstr.o bitops.o
lib-y += memcpy.o memmove.o memset.o rwlock.o net.o wrap.o thread.o
