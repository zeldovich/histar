#ifndef JOS_DEV_IDE_H
#define JOS_DEV_IDE_H

// Command block registers
#define IDE_REG_DATA		0	// read-write

#define IDE_REG_FEATURES	1	// write-only
#define IDE_REG_ERROR		1	// read-only

#define IDE_REG_SECTOR_COUNT	2	// read-write
#define IDE_REG_LBA_LOW		3	// read-write
#define IDE_REG_LBA_MID		4	// read-write
#define IDE_REG_LBA_HI		5	// read-write
#define IDE_REG_DEVICE		6	// read-write

#define IDE_REG_CMD		7	// write-only
#define IDE_REG_STATUS		7	// read-only

// Device register bits
#define IDE_DEV_LBA	0x40

// Device control register (write to control block register) bits
#define	IDE_CTL_LBA48	0x80
#define IDE_CTL_SRST	0x04
#define IDE_CTL_NIEN	0x02

// Status bits
#define IDE_STAT_BSY	0x80
#define IDE_STAT_DRDY	0x40
#define IDE_STAT_DF	0x20
#define IDE_STAT_DRQ	0x08
#define IDE_STAT_ERR	0x01

// Command register values
#define IDE_CMD_READ	    0x20
#define IDE_CMD_READ_DMA    0xc8
#define IDE_CMD_WRITE	    0x30
#define IDE_CMD_WRITE_DMA   0xca
#define IDE_CMD_IDENTIFY    0xec

// Identify device structure
struct identify_device {
    uint16_t pad0[10];	    // Words 0-9
    char serial[20];	    // Words 10-19
    uint16_t pad1[3];	    // Words 20-22
    char firmware[8];	    // Words 23-26
    char model[40];	    // Words 27-46
    uint16_t pad2[13];	    // Words 47-59
    uint32_t lba_sectors;   // Words 60-61, assuming little-endian
    uint16_t pad3[26];	    // Words 62-87
    uint16_t udma_mode;	    // Word 88
};

// Bus-master physical region descriptor
struct ide_prd {	// PRD must be 4-byte-aligned and not cross 64K
    uint32_t addr;	// buffer must be 2-byte-aligned and not cross 64K
    uint32_t count;	// bits 0:15 indicate byte count
			// bits 16:30 are reserved (zero)
			// bit 31 indicates end of PRD list
};
#define IDE_PRD_EOT	    (1 << 31)

#endif
