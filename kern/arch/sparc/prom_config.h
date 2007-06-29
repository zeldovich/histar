/*
 * prom configuration
 */

#ifndef JOS_MACHINE_PROM_CONFIG_H
#define JOS_MACHINE_PROM_CONFIG_H

#include <machine/bootloader.h>

#define CPUFREQ_KHZ BOOTLOADER_FREQ_KHZ
//#define CPUFREQ_KHZ 50000

/* if defined, jump immediately to this address after init */

#ifndef JUMP_TO
#define JUMP_TO BOOTLOADER_physbase /*0x40000000*/
#endif

/* ram configuration */

//16384 //8192 //16384 //4096
#define SRAM_BANK_SIZE_KB BOOTLOADER_SRAMSZ_KB 
#define SRAM_WWS 3
#define SRAM_RWS 3
#define SRAM_WIDTH 32

#define SDRAM_SIZE_MB BOOTLOADER_SDRAMSZ_MB

#define LEONSETUP_MEM_BASEADDR BOOTLOADER_physbase /*0x40000000*/

/* uart1 */

#define UART1_ENABLE 1
#define UART1_BAUDRATE 38400
//#define UART1_BAUDRATE 9600

/* misc settings */

#define ENABLE_CACHE

/* control registers */

#define	PREGS	0x80000000
#define	MCFG1	0x00
#define	MCFG2	0x04
#define	MCFG3	0x08
#define	ECTRL	0x08
#define	FADDR	0x0c
#define	MSTAT	0x10
#define CCTRL	0x14
#define PWDOWN	0x18
#define WPROT1	0x1C
#define WPROT2	0x20
#define LCONF 	0x24
#define	TCNT0	0x40
#define	TRLD0	0x44
#define	TCTRL0	0x48
#define	TCNT1	0x50
#define	TRLD1	0x54
#define	TCTRL1	0x58
#define	SCNT  	0x60
#define	SRLD  	0x64
#define	UDATA0 	0x70
#define	USTAT0 	0x74
#define	UCTRL0 	0x78
#define	USCAL0 	0x7c
#define	UDATA1 	0x80
#define	USTAT1 	0x84
#define	UCTRL1 	0x88
#define	USCAL1 	0x8c
#define	IMASK	0x90
#define	IPEND	0x94
#define	IFORCE	0x98
#define	ICLEAR	0x9c
#define	IOREG	0xA0
#define	IODIR	0xA4
#define	IOICONF	0xA8
#define	IMASK2	0xB0
#define	IPEND2	0xB4
#define	ISTAT2  0xB8
#define	ICLEAR2	0xB8

