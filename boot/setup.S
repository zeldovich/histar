#include <machine/mmu.h>

#define SETUP_RELOC(x)	(x - 0x30200)
	
.set PROT_MODE_CSEG,0x8		# code segment selector
.set PROT_MODE_DSEG,0x10        # data segment selector
.set CR0_PE_ON,0x1		# protected mode enable flag
			
.code16
.globl begtext, begdata, begbss, endtext, enddata, endbss

.text
begtext:
.data
begdata:
.bss
begbss:
.text

start:
	jmp	trampoline

# This is the setup header, and it must start at %cs:2
# Copied from Linux arch/i386/boot/setup.S and filled in enough 
# for SYSLINUX (header sign, header version, load high)

		.ascii	"HdrS"		# header signature
		.word	0x0204		# header version number (>= 0x0105)
					# or else old loadlin-1.5 will fail)
realmode_swtch:	.word	0, 0		# default_switch, SETUPSEG
start_sys_seg:	.word	0
		.word	0		# pointing to kernel version string
					# above section of header is compatible
					# with loadlin-1.5 (header v1.5). Don't
					# change it.

type_of_loader:	.byte	0		# = 0, old one (LILO, Loadlin,
					#      Bootlin, SYSLX, bootsect...)
					# See Documentation/i386/boot.txt for
					# assigned ids
	
# flags, unused bits must be zero (RFU) bit within loadflags
loadflags:
LOADED_HIGH	= 1			# If set, the kernel is loaded high
CAN_USE_HEAP	= 0x80			# If set, the loader also has set
					# heap_end_ptr to tell how much
					# space behind setup.S can be used for
					# heap purposes.
					# Only the loader knows what is free
		.byte	LOADED_HIGH

setup_move_size: .word  0x8000		# size to move, when setup is not
					# loaded at 0x90000. We will move setup 
					# to 0x90000 then just before jumping
					# into the kernel. However, only the
					# loader knows how much data behind
					# us also needs to be loaded.

code32_start:				# here loaders can put a different
					# start address for 32-bit code.
		.long	0x100000	# 0x100000 = default for big kernel

ramdisk_image:	.long	0		# address of loaded ramdisk image
					# Here the loader puts the 32-bit
					# address where it loaded the image.
					# This only will be read by the kernel.

ramdisk_size:	.long	0		# its size in bytes

bootsect_kludge:
		.long	0		# obsolete

heap_end_ptr:	.word	0		# (Header version 0x0201 or later)
					# space from here (exclusive) down to
					# end of setup code can be used by setup
					# for local heap purposes.

pad1:		.word	0
cmd_line_ptr:	.long 0			# (Header version 0x0202 or later)
					# If nonzero, a 32-bit pointer
					# to the kernel command line.
					# The command line should be
					# located between the start of
					# setup and the end of low
					# memory (0xa0000), or it may
					# get overwritten before it
					# gets read.  If this field is
					# used, there is no longer
					# anything magical about the
					# 0x90000 segment; the setup
					# can be located anywhere in
					# low memory 0x10000 or higher.

ramdisk_max:	.long 0xffffffff
	
trampoline:	call	start_of_setup
		.align 16

start_of_setup:
		cli				# Disable interrupts
		cld				# String operations increment

		# Set up the important data segment registers (DS, ES, SS).
		movw	%cs,%ax			# -> Data Segment
		movw	%ax,%ds
		xorw	%ax,%ax			# Segment number zero
		movw	%ax,%es			# -> Extra Segment
		movw	%ax,%ss			# -> Stack Segment

		# Set up the stack pointer, growing downward from 0x7c00.
		movl	$start,%esp         	# Stack Pointer

#### Enable A20:
####   For fascinating historical reasons (related to the fact that
####   the earliest 8086-based PCs could only address 1MB of physical memory
####   and subsequent 80286-based PCs wanted to retain maximum compatibility),
####   physical address line 20 is tied to low when the machine boots.
####   Obviously this a bit of a drag for us, especially when trying to
####   address memory above 1MB.  This code undoes this.
	
seta20.1:	inb	$0x64,%al		# Get status
		testb	$0x2,%al		# Busy?
		jnz	seta20.1		# Yes
		movb	$0xd1,%al		# Command: Write
		outb	%al,$0x64		#  output port
seta20.2:	inb	$0x64,%al		# Get status
		testb	$0x2,%al		# Busy?
		jnz	seta20.2		# Yes
		movb	$0xdf,%al		# Enable
		outb	%al,$0x60		#  A20

#### Before going to protected mode, query memory size
#### Taken from Linux 2.6.6's arch/i386/boot/setup.S
		stc			# Fix to work around buggy
		xorw	%cx,%cx		# BIOSes which don't clear/set
		xorw	%dx,%dx		# carry on pass/error of
					# e801h memory size call
					# or merely pass cx,dx though
					# without changing them.
		movw	$0xe801,%ax
		int	$0x15
		jc	nobiosmem

		cmpw	$0x0,%cx	# Kludge to handle BIOSes
		jne	memdetected	# which report their extended
		cmpw	$0x0,%dx	# memory in AX/BX rather than
		jne	memdetected	# CX/DX. The spec I have read
		movw	%ax,%cx		# seems to indicate AX/BX
		movw	%bx,%dx		# are more reasonable anyway...
		jmp	memdetected

nobiosmem:	xorw	%cx,%cx
		xorw	%dx,%dx
memdetected:

#### Switch from real to protected mode	
####     The descriptors in our GDT allow all physical memory to be accessed.
####     Furthermore, the descriptors have base addresses of 0, so that the
####     segment translation is a NOP, ie. virtual addresses are identical to
####     their physical addresses.  With this setup, immediately after
####	 enabling protected mode it will still appear to this code
####	 that it is running directly on physical memory with no translation.
####	 This initial NOP-translation setup is required by the processor
####	 to ensure that the transition to protected mode occurs smoothly.
	
real_to_prot:	cli				# Mandatory since we dont set up an IDT
		movl	$SETUP_RELOC(gdtdesc), %eax
		lgdtl	(%eax)			# load GDT -- mandatory in protected mode
		movl	%cr0, %eax		# turn on protected mode
		orl	$CR0_PE_ON, %eax	# 
		movl	%eax, %cr0		# 
	        ### CPU magic: jump to relocation, flush prefetch queue, and reload %cs
		### Has the effect of just jmp to the next instruction, but simultaneous
		### loads CS with $PROT_MODE_CSEG.
		ljmpl	$PROT_MODE_CSEG, $protcseg

#### we are in 32-bit protected mode (hence the .code32)
.code32
protcseg:
		movw	$PROT_MODE_DSEG, %ax	# Our data segment selector
		movw	%ax, %ds		# -> DS: Data Segment
		movw	%ax, %es		# -> ES: Extra Segment
		movw	%ax, %fs		# -> FS
		movw	%ax, %gs		# -> GS
		movw	%ax, %ss		# -> SS: Stack Segment

		movl	$0xffff,%eax
		andl	%eax,%edx		# clip to 16 bits
		shll	$6,%edx			# go from 64K to 1K chunks
		andl	%eax,%ecx		# clip to 16 bits
		addl	%edx,%ecx		# add results
		
		pushl	%ecx			# Push the amount of extended memory

		movl	0x100018,%eax		# read elf64 e_entry
		addl	$0x80000000,%eax	# get rid of offset
		call	*%eax

.p2align 2					# force 4 byte alignment
gdt:
	.quad 0					# null segment
	.quad SEG32(SEG_X|SEG_R, 0x0, 0xffffffff, 0) # code seg
	.quad SEG32(SEG_W, 0x0, 0xffffffff, 0)	# data seg
	
gdtdesc:
	.word	0x17			# sizeof(gdt) - 1
	.long	gdt			# address gdt
