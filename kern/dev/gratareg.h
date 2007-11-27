#ifndef JOS_DEV_GRATAREG_H
#define JOS_DEV_GRATAREG_H

struct grata_regs {
    uint32_t ctrl;
    uint32_t stat;
    uint32_t pctr;
    uint32_t pftr0;

    uint32_t pftr1;
    uint32_t dtr0;
    uint32_t dtr1;
    uint32_t pad0;

    uint32_t pad1[8];

    uint16_t dev_data;
    uint16_t pad2;

    uint8_t dev_feat_err;
    uint8_t pad3[3];

    /* grip says sector number before sector count, but that's suspicious.. */
    uint8_t dev_sector;
    uint8_t pad4[3];

    uint8_t dev_sector_count;
    uint8_t pad5[3];

    uint8_t dev_cyl_lo;
    uint8_t pad6[3];

    uint8_t dev_cyl_hi;
    uint8_t pad7[3];

    uint8_t dev_device;
    uint8_t pad8[3];

    uint8_t dev_cmdstat;
    uint8_t pad9[3];

    uint32_t pada[6];

    uint8_t dev_altstat;
    uint8_t dev_padb[7];
};

#define GRATA_CTRL_CFPOWER	(1 << 31)
#define GRATA_CTRL_IDE_EN	(1 << 7)
#define GRATA_CTRL_FT1_EN	(1 << 6)
#define GRATA_CTRL_FT0_EN	(1 << 5)
#define GRATA_CTRL_FT1_IORDY_EN	(1 << 3)
#define GRATA_CTRL_FT0_IORDY_EN	(1 << 2)
#define GRATA_CTRL_CPT_IORDY_EN	(1 << 1)
#define GRATA_CTRL_ATA_RESET	(1 << 0)

#define GRATA_STAT_PIO_ACTIVE	(1 << 7)
#define GRATA_STAT_INTR		(1 << 0)

#endif
