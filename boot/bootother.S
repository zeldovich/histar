#include <machine/mmu.h>
	
# Start an Application Processor. This must be placed on a 4KB boundary
# somewhere in the 1st MB of conventional memory (APBOOTSTRAP). However,
# due to some shortcuts below it's restricted further to within the 1st
# 64KB. The AP starts in real-mode, with
#   CS selector set to the startup memory address/16;
#   CS base set to startup memory address;
#   CS limit set to 64KB;
#   CPL and IP set to 0.
#

.set PROT_MODE_CSEG, 0x8         # kernel code segment selector
.set PROT_MODE_DSEG, 0x10        # kernel data segment selector
.set CR0_PE_ON,      0x1         # protected mode enable flag

.globl start
start:
	.code16                     # Assemble for 16-bit mode
	cli                         # Disable interrupts
	cld                         # String operations increment

# Set up the important data segment registers (DS, ES, SS).
	xorw    %ax,%ax             # Segment number zero
	movw    %ax,%ds             # -> Data Segment
	movw    %ax,%es             # -> Extra Segment
	movw    %ax,%ss             # -> Stack Segment

# Print a diagnostic message	
	movw	$0xb800, %ax
	movw	%ax, %es
	movw	$(0x0600 + '$'), %es:(0x00)
	
# Switch from real to protected mode, using a bootstrap GDT
# and segment translation that makes virtual addresses 
# identical to their physical addresses, so that the 
# effective memory map does not change during the switch.
	lgdt	gdtdesc
	movl	%cr0, %eax
	orl	$CR0_PE_ON, %eax
	movl	%eax, %cr0

# Jump to next instruction, but in 32-bit code segment.
# Switches processor into 32-bit mode.
	ljmp	$PROT_MODE_CSEG, $protcseg

.code32
protcseg:
	movw	$PROT_MODE_DSEG, %ax	# Our data segment selector
	movw	%ax, %ds		# -> DS: Data Segment
	movw	%ax, %es		# -> ES: Extra Segment
	movw	%ax, %fs		# -> FS
	movw	%ax, %gs		# -> GS
	movw	%ax, %ss		# -> SS: Stack Segment
	movw	$(0x0700 + 'A'), (0xb8002)
	
	movl	start + 4092, %ecx
	movl	$start, %eax
	jmp	%ecx

.align 4
gdt:
	.quad 0					# null segment
	SEG32_ASM(STA_X|STA_R, 0x0, 0xffffffff)	# code seg
	SEG32_ASM(STA_W, 0x0, 0xffffffff)	# data seg

.align 16
gdtdesc:
	.word	0x17			# sizeof(gdt) - 1
gdtaddr:
	.long	gdt			# address gdt
