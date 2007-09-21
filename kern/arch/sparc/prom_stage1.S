/*
 *
 */

#define CONFIG_LEON
#define CONFIG_LEON3
#include <machine/leon.h> 
#include <dev/ambapp.h>
#include <machine/prom_config.h>
#include <machine/asm.h>
		
	.seg	"text"

ENTRY(prom_start)  
	ba  1000f
          nop
        
!unsigned int ahbslv_scan(register unsigned int vendor,register unsigned int driver) {
!  register unsigned int conf, mbar,i, *confp;
!  register unsigned int cfg_area = (unsigned int ) (LEON3_IO_AREA | LEON3_CONF_AREA | LEON3_AHB_SLAVE_CONF_AREA);
!  for (i = 0; i < LEON3_AHB_SLAVES; i++) 
!  {
!    confp = (unsigned int*)(cfg_area + (i * LEON3_AHB_CONF_WORDS * 4));
!    conf = *confp;
!    //mbar = *(unsigned int*)(i * LEON3_AHB_CONF_WORDS+ (4 * 4));
!    if ((amba_vendor(conf) == vendor) && (amba_device(conf) == driver)) {
!      return (unsigned int)confp;
!    }
!  }
!  return 0;
!}

        
	
         
	.section	".text"
	.align 4
ahbslv_scan:
	mov	%o0, %g1
	mov	-2048, %o5
	mov	0, %o3
	sll	%o3, 5, %o0
.LL11:
	add	%o5, %o0, %o4
	ld	[%o5+%o0], %o2
	srl	%o2, 24, %o0
	cmp	%o0, %g1
	bne,a	.LL10
	add	%o3, 1, %o3
	srl	%o2, 12, %o0
	and	%o0, 4095, %o0
	cmp	%o0, %o1
	be	.LL1
	mov	%o4, %o2
	add	%o3, 1, %o3
.LL10:
	cmp	%o3, 7
	bleu,a	.LL11
	sll	%o3, 5, %o0
	mov	0, %o2
.LL1:
	retl
	mov	%o2, %o0


!unsigned int apbslv_scan(register unsigned int base,register unsigned int vendor, register unsigned int driver) {
!  register unsigned int conf, mbar,i, *confp;
!  for (i = 0; i < LEON3_APB_SLAVES; i++) 
!  {
!    confp = (unsigned int*)(base + (i * LEON3_APB_CONF_WORDS * 4));
!    conf = *confp;
!    //mbar = *(unsigned int*)(i * LEON3_AHB_CONF_WORDS+ (4 * 4));
!    if ((amba_vendor(conf) == vendor) && (amba_device(conf) == driver)) {
!      return (unsigned int)confp;
!    }
!  }
!  return 0;
!}


         
	.section	".text"
	.align 4

apbslv_scan:
	mov	%o0, %g1
	mov	0, %o4
	sll	%o4, 3, %o0
.LL22:
	add	%g1, %o0, %o5
	ld	[%g1+%o0], %o3
	srl	%o3, 24, %o0
	cmp	%o0, %o1
	bne,a	.LL21
	add	%o4, 1, %o4
	srl	%o3, 12, %o0
	and	%o0, 4095, %o0
	cmp	%o0, %o2
	be	.LL12
	mov	%o5, %o3
	add	%o4, 1, %o4
.LL21:
	cmp	%o4, 15
	bleu,a	.LL22
	sll	%o4, 3, %o0
	mov	0, %o3
.LL12:
	retl
	mov	%o3, %o0



!unsigned int getbase(register unsigned int *mbar,register unsigned int iobase) {
!  register unsigned int conf = mbar[1];
!  return ((iobase & 0xfff00000) |
!          ((conf & 0xfff00000)>> 12)) & (((conf & 0x0000fff0) <<4) | 0xfff00000);
!  
!}
        
         
	.section	".text"
	.align 4

getbase:
	ld	[%o0+4], %o2
	sethi	%hi(-1048576), %o3
	and	%o1, %o3, %o1
	and	%o2, %o3, %o0
	srl	%o0, 12, %o0
	or	%o1, %o0, %o1
	sethi	%hi(64512), %o0
	or	%o0, 1008, %o0
	and	%o2, %o0, %o2
	sll	%o2, 4, %o2
	or	%o2, %o3, %o2
	and	%o1, %o2, %o1
	retl
	mov	%o1, %o0
        

1000:
	wr	%g0, 0xfe0, %psr
	mov	%g0, %tbr
	mov	%g0, %wim
	flush


	flush

	set	0x1000f, %g2
        !set	0x00000, %g2
        sta %g2, [%g0] 2          ! init cache
	
	clr	%g1
	clr	%g2
	clr	%g3
	clr	%g4
	clr	%g5
	clr	%g6
	clr	%g7
        
        set VENDOR_GAISLER, %o0
        set GAISLER_APBMST, %o1
        call ahbslv_scan
        nop
        
        cmp %g0,%o0
        be prom_start
         nop
        
        ld [%o0+16],%g1
        set LEON3_IO_AREA,%o0
        and %g1,%o0,%g1            !g1: apb base

        set LEON3_CONF_AREA,%o0
        or %g1,%o0,%g2             !g2: apb conf base


	! *********** irq *****************
        mov %g2,%o0
        set VENDOR_GAISLER, %o1
        set GAISLER_IRQMP,%o2
        call apbslv_scan
         nop
        cmp %g0,%o0
        be prom_start
         nop
        
        call getbase
         mov %g1,%o1
                        
        st    %g0, [%o1 + 0x0]  !irq pri   
        st    %g0, [%o1 + 0x4]  !irq pend  
        st    %g0, [%o1 + 0x8]  !irq force 
        st    %g0, [%o1 + 0xC]  !irq clear
	st		%g0, [%o1 + 0x40] !irq mask processor 0
	st		%g0, [%o1 + 0x80] !irq force processor 0	
	
	! ************ timer ************

        mov %g2,%o0
        set VENDOR_GAISLER, %o1
        set GAISLER_GPTIMER,%o2
        call apbslv_scan
         nop
        cmp %g0,%o0
        be prom_start
         nop
        
        call getbase
         mov %g1,%o1
        
	set	(BOOTLOADER_FREQ_KHZ+500)/1000, %o0		! Load timer scaler register
	sub	%o0, 1, %o0
  	st	%o0, [%o1 + 0x00] ! scaler
  	st	%o0, [%o1 + 0x04] ! scaler reload

	! ************ mctrl ************
	
        mov %g2,%o0                  ! esa mctrl
        set VENDOR_ESA, %o1
        set ESA_MCTRL,%o2
        call apbslv_scan
         nop
        cmp %g0,%o0
        be .LL1mctlsdram
         nop

        call getbase
         mov %g1,%o1

	set	BOOTLOADER_memcfg1, %o0		! Load memory config register 1
	ld	[%o1+0x00], %o2		
	and	%o2, 0x300, %o2
	or	%o0, %o2, %o0
  	st	%o0, [%o1 + 0x00]
	set	BOOTLOADER_memcfg2, %o0          ! Load memory config register 2
	st	%o0, [%o1 + 0x04]

	set	BOOTLOADER_ftreg, %o0		! Init FT register
	st	%o0, [%o1 + 0x08]

1:	
	ba	.LL1mctlok
	nop

	! ************ gaisler sdram mctrl ************
	
.LL1mctlsdram:

        set VENDOR_GAISLER, %o0
        set GAISLER_SDCTRL, %o1
        call ahbslv_scan
        nop
        
        cmp %g0,%o0
        be .LL1mctlok                ! assuming sram/prom ctrl
         nop

        ld [%o0+16+4],%g1            ! get io bar
        set LEON3_IO_AREA,%o0
        and %g1,%o0,%g1
	srl %g1,12,%g1
        or  %g1,%o0,%g1
	

	set	BOOTLOADER_grlib_sdram, %o0
	st	%o0, [%g1 + 0x100]
        
	
	! ************ uart ************
	
.LL1mctlok:		
        mov %g2,%o0
        set VENDOR_GAISLER, %o1
        set GAISLER_APBUART,%o2
        call apbslv_scan
         nop
        cmp %g0,%o0
        be prom_start
         nop
        
        call getbase
         mov %g1,%o1
        
#define UARTBASE %g5       
        mov %o1,UARTBASE             !uart base address
                
        
        !########################################################
        ! initialization: protect g5 (uartbase)
	
      !	set	PREGS, %g1		! on-chip register base address

      !st    %g2, [%g1 + 0x14]	! clear some LEON registers
      !st    %g0, [%g1 + 0x08]  !mcfg3   
      !st    %g0, [%g1 + 0x90]  !irq pri   
      !st    %g0, [%g1 + 0x94]  !irq pend  
      !st    %g0, [%g1 + 0x98]  !irq force 
      !st    %g0, [%g1 + 0x9C]  !irq clear
      !st    %g0, [%g1 + 0xA0]  !i/o inout
      !st    %g0, [%g1 + 0xA4]  !i/o dir
      !st    %g0, [%g1 + 0xA8]  !i/o port irq
      !st    %g0, [%g1 + 0x78]  !uart1 ctrl
      !st    %g0, [%g1 + 0x88]  !uart2 ctrl
        
      !	set	0xaa00, %g2		! io port initialization
      !	st    %g2, [%g1 + 0xA0] 

#ifdef ENABLE_CACHE
cache:
      !	set	0x1000f, %g2
      !	st	%g2, [%g1 + CCTRL]	! enable cache
#endif

fpu:
      !	ld	[%g1 + LCONF], %o1	! probe for Meiko FPU
      !	and	%o1, 0x10, %o1
      !	sll	%o1, 8, %o1
      !	wr	%o1, 0xe0, %psr
	
 
stackp:
	set	BOOTLOADER_stack, %fp  
	andn	%fp, 0x0f, %fp
	sub	%fp, 64, %sp           


uart1:

        set	BOOTLOADER_uart, %g2		! Load UART scaler register
	st	%g2, [UARTBASE + 0x0c]  ! uart scalar
#if defined(CONFIG_UARTLOOPBACK) || (BOOTLOADER_loopback==1)
	set	0x83|(BOOTLOADER_flow<<6), %g2			! Enable uarts, (flow ctrl), loopback
#else
	set	0x03|(BOOTLOADER_flow<<6), %g2			! Enable uarts, (flow ctrl)
#endif	  
  	st	%g2, [UARTBASE + 0x08]

	!set	0x8000, %g2		! set pio directions for serial io
	!st	%g2, [%g1 + IODIR]
	!st	%g0, [%g1 + USTAT0]	
	!set	3, %g2
	!st	%g2, [%g1 + UCTRL0]	! enable receiver and transmitter

        !set	BOOTLOADER_uart, %g2		! Load UART scaler register
	!st	%g2, [%g1 + USCAL0]

stage2:
	call	start
	nop


dead:	ta 0				! if call returns...
	nop

