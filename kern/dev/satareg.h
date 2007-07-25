#ifndef JOS_DEV_SATAREG_H
#define JOS_DEV_SATAREG_H

struct sata_fis_reg {
    uint8_t type;
    uint8_t cflag;
    union {
	uint8_t command;	/* H2D */
	uint8_t status;		/* D2H */
    };
    union {
	uint8_t features;	/* H2D */
	uint8_t error;		/* D2H */
    };

    union {
	uint8_t sector;
	uint8_t lba_0;
    };
    union {
	uint8_t cyl_lo;
	uint8_t lba_1;
    };
    union {
	uint8_t cyl_hi;
	uint8_t lba_2;
    };
    uint8_t dev_head;

    union {
	uint8_t sector_ex;
	uint8_t lba_3;
    };
    union {
	uint8_t cyl_lo_ex;
	uint8_t lba_4;
    };
    union {
	uint8_t cyl_hi_ex;
	uint8_t lba_5;
    };
    uint8_t features_ex;

    uint8_t sector_count;
    uint8_t sector_count_ex;
    uint8_t __pad1;
    uint8_t control;

    uint8_t __pad2[4];
};

#define SATA_FIS_REG_CFLAG	(1 << 7)	/* issuing new command */

struct sata_fis_devbits {
    uint8_t type;
    uint8_t intr;
    uint8_t status;
    uint8_t error;
    uint8_t __pad[4];
};

#define SATA_FIS_TYPE_REG_H2D	0x27
#define SATA_FIS_TYPE_REG_D2H	0x34
#define SATA_FIS_TYPE_DEVBITS	0xA1	/* always D2H */

#endif
