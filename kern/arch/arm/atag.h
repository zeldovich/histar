#ifndef JOS_MACHINE_ATAG
#define JOS_MACHINE_ATAG

#define	ATAG_NONE	0x00000000	/* terminates the list */
#define	ATAG_CORE	0x54410001
#define	ATAG_MEM	0x54410002
#define	ATAG_VIDEOTEXT	0x54410003
#define	ATAG_RAMDISK	0x54410004
#define	ATAG_INITRD2	0x54420005
#define	ATAG_SERIAL	0x54410006
#define	ATAG_REVISION	0x54410007
#define	ATAG_VIDEOLFB	0x54410008
#define	ATAG_CMDLINE	0x54410009

#ifndef __ASSEMBLER__

struct atag_core {
	uint32_t flags;
	uint32_t page_size;
	uint32_t root_dev;
};

struct atag_mem {
	uint32_t bytes;
	uint32_t offset;
};

struct atag_cmdline {
	char cmdline[0];
};

struct atag {
	uint32_t words;		/* number of 32-bit words, including header */
	uint32_t tag;		/* tag type */

	union atag_u {
		struct atag_core	core;
		struct atag_mem		mem;
		struct atag_cmdline	cmdline;
	} u;
};

#endif /* !__ASSEMBLER__ */

#endif /* !JOS_MACHINE_ATAG */
