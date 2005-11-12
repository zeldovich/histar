#include <inc/mmu.h>
#include <inc/pmap.h>

.data
	// define empty environment pointer
empty_environ:
	.space 4
	// define page-aligned fsipcbuf for fsipc.c
	// ... and fdtab for file.c
	.p2align PGSHIFT
	.globl fsipcbuf
fsipcbuf:
	.space PGSIZE
	.globl fdtab
fdtab:
	.space PGSIZE


// Define the global symbols 'udata', 'vpt', 'vpd', 'fdtable'
// so that they can be used in C as if they were ordinary global arrays.
	.globl udata
	.set udata, UDATA
	.globl vpt
	.set vpt, UVPT
	.globl vpd
	.set vpd, (UVPT+(UVPT>>12)*4)
	.globl fdtable
	.set fdtable, FDBASE


// Entrypoint - this is where the kernel (or our parent environment)
// starts us running when we are initially loaded into a new environment.
.text
.globl _start
_start:
	// See if we were started with arguments on the stack
	cmpl $USTACKTOP, %esp
	jne args_exist

	// If not, push dummy env/argc/argv arguments.
	// This happens when we are loaded by the kernel,
	// because the kernel does not know about passing arguments.
	pushl $empty_environ
	pushl $empty_environ
	pushl $0

args_exist:
	call libmain
1:      jmp 1b
