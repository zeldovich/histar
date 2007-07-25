#ifndef JOS_DEV_AHCIREG_H
#define JOS_DEV_AHCIREG_H

#include <machine/memlayout.h>

struct ahci_reg_global {
    uint32_t cap;		/* host capabilities */
    uint32_t ghc;		/* global host control */
    uint32_t is;		/* interrupt status */
    uint32_t pi;		/* ports implemented */
    uint32_t vs;		/* version */
    uint32_t ccc_ctl;		/* command completion coalescing control */
    uint32_t ccc_ports;		/* command completion coalescing ports */
    uint32_t em_loc;		/* enclosure management location */
    uint32_t em_ctl;		/* enclosure management control */
    uint32_t cap2;		/* extended host capabilities */
    uint32_t bohc;		/* BIOS/OS handoff control and status */
};

#define AHCI_CAP_NCS(cap)	(((cap) >> 8) & 0x1f)
#define AHCI_GHC_AE		(1 << 31)
#define AHCI_GHC_IE		(1 << 1)
#define AHCI_GHC_HR		(1 << 0)

struct ahci_reg_port {
    uint64_t clb;		/* command list base address */
    uint64_t fb;		/* FIS base address */
    uint32_t is;		/* interrupt status */
    uint32_t ie;		/* interrupt enable */
    uint32_t cmd;		/* command and status */
    uint32_t reserved;
    uint32_t tfd;		/* task file data */
    uint32_t sig;		/* signature */
    uint32_t ssts;		/* sata phy status: SStatus */
    uint32_t sctl;		/* sata phy control: SControl */
    uint32_t serr;		/* sata phy error: SError */
    uint32_t sact;		/* sata phy active: SActive */
    uint32_t ci;		/* command issue */
    uint32_t sntf;		/* sata phy notification: SNotify */
    uint32_t fbs;		/* FIS-based switching control */
};

struct ahci_reg {
    union {
	struct ahci_reg_global;
	char __pad[0x100];
    };

    struct {
	union {
	    struct ahci_reg_port;
	    char __pad[0x80];
	};
    } port[32];
};

struct ahci_recv_fis {
    union {
	uint8_t dsfis[0x1c];	/* DMA setup FIS */
	uint8_t __pad0[0x20];
    };

    uint8_t psfis[0x20];	/* PIO setup FIS */
    uint8_t rfis[0x14];		/* D2H register FIS */
    uint8_t __pad1[4];
    uint8_t sdbfis[8];		/* set device bits FIS */
    uint8_t ufis[0x40];		/* unknown FIS */
    uint8_t reserved[0x60];
};

struct ahci_cmd_header {
    uint16_t flags;
    uint16_t prdtl;
    uint32_t prdbc;
    uint64_t ctba;
    uint64_t reserved0;
    uint64_t reserved1;
};

struct ahci_prd {
    uint64_t dba;
    uint32_t reserved;
    uint32_t dbc;		/* one less than #bytes */
};

struct ahci_cmd_table {
    uint8_t cfis[0x40];		/* command FIS */
    uint8_t acmd[0x10];		/* ATAPI command */
    uint8_t reserved[0x30];
    struct ahci_prd prdt[DISK_REQMAX / PGSIZE + 1];
};

#endif
