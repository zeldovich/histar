#ifndef JOS_DEV_VESA_H
#define JOS_DEV_VESA_H

enum {
    vbe_red,
    vbe_green,
    vbe_blue,
    vbe_reserved,
    vbe_rgb_max
};

struct vbe_info {
    char     sig[4];		/* 'V' 'E' 'S' 'A' */
    uint16_t ver;		/* version# */
    uint32_t oem_string_addr;	/* OEM string */
    uint32_t cap;		/* capabilities */
    uint32_t modeaddr;		/* video mode pointer */
    uint16_t memsize;		/* 64k memory blocks */

    uint16_t oem_rev;		/* OEM software rev */
    uint32_t oem_vendor_addr;	/* vendor name */
    uint32_t oem_product_addr;	/* product name */
    uint32_t oem_prev_addr;	/* product rev string */

    uint8_t  vbe_pad[222];
    uint8_t  oem_pad[256];
} __attribute__((packed));

struct vbe_mode_info {
    uint16_t attr;
    uint8_t  win_attr[2];
    uint16_t win_granularity;
    uint16_t win_size;
    uint16_t win_segment[2];
    uint32_t win_func_ptr;
    uint16_t bytes_per_scanline;

    uint16_t xres;
    uint16_t yres;
    uint8_t  xcharsize;
    uint8_t  ycharsize;
    uint8_t  nplanes;
    uint8_t  bpp;
    uint8_t  nbanks;
    uint8_t  memmodel;
    uint8_t  banksize;
    uint8_t  nipages;
    uint8_t  pad0;

    struct {
	uint8_t masksize;
	uint8_t fieldpos;
    } color[vbe_rgb_max];
    uint8_t  colormode;

    uint32_t fb_physaddr;
    uint8_t  pad1[6];

    uint16_t fb_bytes_per_scanline;   
    uint8_t  bank_nipages;
    uint8_t  fb_nipages;
    struct {
	uint8_t masksize;
	uint8_t fieldpost;
    } fb_color[vbe_rgb_max];
    uint32_t maxclock;

    uint8_t  pad2[190];
} __attribute__((packed));

#endif
