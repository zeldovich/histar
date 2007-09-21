#ifndef JOS_MACHINE_BOOTLOADER_H
#define JOS_MACHINE_BOOTLOADER_H

/* from snapgear-p32/vendors/gaisler/common/bootloader.h and 
 * generated using Snapgear's make config
 */

#define BOOTLOADER_FREQ_KHZ 40000

#define BOOTLOADER_SRAMSZ_KB 0
#define BOOTLOADER_SDRAMSZ_MB 16

#define BOOTLOADER_BAUD 38400
#define BOOTLOADER_uart     0x83
#define BOOTLOADER_loopback 0x0
#define BOOTLOADER_flow     0x0

#define BOOTLOADER_memcfg1  0x28022
#define BOOTLOADER_memcfg2  0x81006020
#define BOOTLOADER_ftreg    0x13b000
#define BOOTLOADER_grlib_sdram 0x8100013b

#define BOOTLOADER_freq      40500000

#define BOOTLOADER_ramsize   0xfff000
#define BOOTLOADER_romsize   0x800000
#define BOOTLOADER_stack     0x40ffefe0

#define BOOTLOADER_physbase  0x40000000

#endif
