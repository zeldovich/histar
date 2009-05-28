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

#define ATAG_MSM_PTABLE	0x4d534d70	/* partition table */
#define ATAG_MSM_WIFI	0x57494649
#define ATAG_AKM8976	0x89768976	/* no idea */

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

struct atag_revision {
	uint32_t revision;
};

struct atag_msm_ptentry {
	char name[16];		/* partition name */
	uint32_t start; 	/* start block */
	uint32_t length;	/* total blocks */
	uint32_t flags;		/* flags - SBZ */
};

struct atag {
	uint32_t words;		/* number of 32-bit words, including header */
	uint32_t tag;		/* tag type */

	union atag_u {
		struct atag_core	core;
		struct atag_mem		mem;
		struct atag_cmdline	cmdline;
		struct atag_msm_ptentry	msm_ptable[1];
		struct atag_revision	revision;
	} u;
};

#endif /* !__ASSEMBLER__ */

#endif /* !JOS_MACHINE_ATAG */
