/*
 * This code snagged from the Anroid legacy bootloader msm7k sources.
 */

/*
 * Copyright (c) 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the 
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <kern/timer.h>
#include <kern/fb.h>
#include <kern/pageinfo.h>
#include <dev/msm_mddi.h>
#include <dev/msm_mddireg.h>
#include <inc/error.h>

static uint32_t mddi_base;	// MDDI base address

unsigned int fb_width = 0;
unsigned int fb_height = 0;

static unsigned int short *FB;
static mddi_llentry *mlist;

static inline uint32_t
mddi_readl(uint32_t regoff)
{
	volatile uint32_t *reg = (volatile uint32_t *)(mddi_base + regoff);
	assert(mddi_base);
	return (*reg);
}

static inline void
mddi_writel(uint32_t val, uint32_t regoff)
{
	volatile uint32_t *reg = (volatile uint32_t *)(mddi_base + regoff);
	assert(mddi_base);
	*reg = val;
}

static void *
mddi_alloc(unsigned int bytes)
{
	void *ptr;
	unsigned int pages;

	pages = (bytes + (PGSIZE - 1)) / PGSIZE;

	int r = page_alloc_n(&ptr, pages, 0);
	if (r < 0)
		panic("%s: failed to allocate %d page(s)", __func__, pages);

	// steal these pages from the kernel for good
	for (unsigned int i = 0; i < pages; i++) {
		struct page_info *pi;
		pi = page_to_pageinfo((char *)ptr + (i * PGSIZE));
		assert(pi != NULL);
		pi->pi_reserved = 1;
	}

	return (ptr);
}

static void wr32(void *_dst, unsigned int n)
{
    volatile unsigned char *src = (volatile unsigned char*) &n;
    volatile unsigned char *dst = _dst;

    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
};

static int printcaps(mddi_client_caps *c)
{
    if((c->length != 0x4a) || (c->type != 0x42)) {
        cprintf("MSM MDDI: bad caps header\n");
        memset(c, 0, sizeof(*c));
        return 1;
    }

    cprintf("MSM MDDI: bm: %d,%d win %d,%d rgb 0x%x\n",
            c->bitmap_width, c->bitmap_height,
            c->display_window_width, c->display_window_height,
            c->rgb_cap);
    cprintf("MSM MDDI: vend 0x%x prod 0x%x\n",
            c->manufacturer_name, c->product_code);

    fb_width = c->bitmap_width;
    fb_height = c->bitmap_height;

    //panel_init(c);

    return 0;
}

mddi_llentry *mlist_remote_write = 0;

void msm_mddi_remote_write(unsigned int val, unsigned int reg)
{
    mddi_llentry *ll;
    mddi_register_access *ra;
    unsigned int s;

    if(mlist_remote_write == 0) {
        mlist_remote_write = mddi_alloc(sizeof(mddi_llentry));
    }

    ll = mlist_remote_write;
    
    ra = &(ll->u.r);
    ra->length = 14 + 4;
    ra->type = TYPE_REGISTER_ACCESS;
    ra->client_id = 0;
    ra->rw_info = MDDI_WRITE | 1;
    ra->crc = 0;

    wr32(&ra->reg_addr, reg);
    wr32(&ra->reg_data, val);    

    ll->flags = 1;
    ll->header_count = 14;
    ll->data_count = 4;
    wr32(&ll->data, (unsigned int) &ra->reg_data);
    wr32(&ll->next, 0);
    ll->reserved = 0;

    mddi_writel((unsigned int)kva2pa((void *)ll), MDDI_PRI_PTR);

    s = mddi_readl(MDDI_STAT);
    while((s & 0x20) == 0){
        s = mddi_readl(MDDI_STAT);
    }
}

static void mddi_start_update(void)
{
    mddi_writel((unsigned int)kva2pa((void *)mlist), MDDI_PRI_PTR);
}

int msm_mddi_update_done(void)
{
    return !!(mddi_readl(MDDI_STAT) & MDDI_STAT_PRI_LINK_LIST_DONE);
}

static void mddi_do_cmd(unsigned int cmd)
{
    mddi_writel(cmd, MDDI_CMD);

    while (!(mddi_readl(MDDI_INT) & MDDI_INT_NO_REQ_PKTS_PENDING)) ;
}

volatile unsigned char *rev_pkt_buf;

static int mddi_get_caps(void)
{
    unsigned int timeout = 100000;
    unsigned int n;

    memset((void *)rev_pkt_buf, 0xee, 256);

//    mddi_writel(CMD_HIBERNATE, MDDI_CMD);
//    mddi_writel(CMD_LINK_ACTIVE, MDDI_CMD);
    
    mddi_writel(256, MDDI_REV_SIZE);
    mddi_writel((unsigned int)kva2pa((void *)rev_pkt_buf), MDDI_REV_PTR);
    mddi_do_cmd(CMD_FORCE_NEW_REV_PTR);

        /* sometimes this will fail -- do it three times for luck... */
    mddi_do_cmd(CMD_RTD_MEASURE);
    timer_delay(1 * 1000 * 1000);

    mddi_do_cmd(CMD_RTD_MEASURE);
    timer_delay(1 * 1000 * 1000);

    mddi_do_cmd(CMD_RTD_MEASURE);
    timer_delay(1 * 1000 * 1000);

    mddi_do_cmd(CMD_GET_CLIENT_CAP);

    do {
        n = mddi_readl(MDDI_INT);
    } while(!(n & MDDI_INT_REV_DATA_AVAIL) && (--timeout));
    
    if(timeout == 0) cprintf("MSM_MDDI: %s: timeout\n", __func__);
    return(printcaps((mddi_client_caps*) rev_pkt_buf));
}

static int
msm_mddi_fb_set(void *arg, uint64_t offset, uint64_t nbytes, const uint8_t *buf)
{
    struct fb_device *fb = arg;

    if (offset + nbytes > PGSIZE * fb->fb_npages)
	return -E_INVAL;

    memcpy(pa2kva(fb->fb_base) + offset, buf, nbytes);
    return 0;
}

void msm_mddi_init(uint32_t base)
{
    unsigned int n;

    mddi_base = base;

    rev_pkt_buf = mddi_alloc(256);

    mddi_do_cmd(CMD_RESET);

        /* disable periodic rev encap */
    mddi_do_cmd(CMD_PERIODIC_REV_ENC | 0);

    mddi_writel(0x0001, MDDI_VERSION);
    mddi_writel(0x3C00, MDDI_BPS);
    mddi_writel(0x0003, MDDI_SPM);

    mddi_writel(0x0005, MDDI_TA1_LEN);
    mddi_writel(0x000C, MDDI_TA2_LEN);
    mddi_writel(0x0096, MDDI_DRIVE_HI);
    mddi_writel(0x0050, MDDI_DRIVE_LO);
    mddi_writel(0x003C, MDDI_DISP_WAKE);
    mddi_writel(0x0002, MDDI_REV_RATE_DIV);

        /* needs to settle for 5uS */
    if (mddi_readl(MDDI_PAD_CTL) == 0) {
        mddi_writel(0x08000, MDDI_PAD_CTL);
        timer_delay(5 * 1000);
    }

    mddi_writel(0xA850F, MDDI_PAD_CTL);
    mddi_writel(0x60006, MDDI_DRIVER_START_CNT);

    mddi_writel((unsigned int)kva2pa((void *)rev_pkt_buf), MDDI_REV_PTR);
    mddi_writel(256, MDDI_REV_SIZE);
    mddi_writel(256, MDDI_REV_ENCAP_SZ);

    mddi_do_cmd(CMD_FORCE_NEW_REV_PTR);
    
        /* disable hibernate */
    mddi_do_cmd(CMD_HIBERNATE | 0);

    //panel_backlight(0);
    
    //panel_poweron();
    
    mddi_do_cmd(CMD_LINK_ACTIVE);

    do {
        n = mddi_readl(MDDI_STAT);
    } while(!(n & MDDI_STAT_LINK_ACTIVE));

        /* v > 8?  v > 8 && < 0x19 ? */
    mddi_writel(2, MDDI_TEST);

//    mddi_writel(CMD_PERIODIC_REV_ENC | 0, MDDI_CMD); /* disable */
        
    if (mddi_get_caps())
        return;

#if 0
    mddi_writel(0x5666, MDDI_MDP_VID_FMT_DES);
    mddi_writel(0x00C3, MDDI_MDP_VID_PIX_ATTR);
    mddi_writel(0x0000, MDDI_MDP_CLIENTID);
#endif

    cprintf("MSM MDDI: panel is %d x %d\n", fb_width, fb_height);

    FB = mddi_alloc(2 * fb_width * fb_height);
    mlist = mddi_alloc(sizeof(mddi_llentry) * (fb_height / 8));

    cprintf("MSM MDDI: FB @ kva 0x%x (pa 0x%x), mlist @ kva 0x%x (pa 0x%x)\n", (unsigned int) FB, (unsigned int)kva2pa(FB), (unsigned int) mlist, (unsigned int)kva2pa(mlist));

    for(n = 0; n < (fb_height / 8); n++) {
        unsigned int y = n * 8;
        unsigned int pixels = fb_width * 8;
        mddi_video_stream *vs = &(mlist[n].u.v);

        vs->length = sizeof(mddi_video_stream) - 2 + (pixels * 2);
        vs->type = TYPE_VIDEO_STREAM;
        vs->client_id = 0;
        vs->format = 0x5565; // FORMAT_16BPP;
        vs->pixattr = PIXATTR_BOTH_EYES | PIXATTR_TO_ALL;
        
        vs->left = 0;
        vs->right = fb_width - 1;
        vs->top = y;
        vs->bottom = y + 7;
        
        vs->start_x = 0;
        vs->start_y = y;
        
        vs->pixels = pixels;
        vs->crc = 0;
        vs->reserved = 0;
        
        mlist[n].header_count = sizeof(mddi_video_stream) - 2;
        mlist[n].data_count = pixels * 2;
        mlist[n].reserved = 0;
        wr32(&mlist[n].data, kva2pa((void *)((unsigned int) FB) + (y * fb_width * 2)));

        mlist[n].flags = 0;
        wr32(&mlist[n].next, kva2pa((void *)(unsigned int) (mlist + n + 1)));
    }

    mlist[n-1].flags = 1;
    wr32(&mlist[n-1].next, 0);

    mddi_writel(0, MDDI_INTEN);			// disable interrupts
    mddi_writel(CMD_HIBERNATE, MDDI_CMD);
    mddi_writel(CMD_LINK_ACTIVE, MDDI_CMD);

    // wipe the screen
    for(n = 0; n < (fb_width * fb_height); n++) FB[n] = 0;
    mddi_start_update();

    //panel_backlight(1);

    // refresh the screen ~30 times per second
    static struct periodic_task msm_mddi_timer;
    msm_mddi_timer.pt_interval_msec = 1000 / 30;
    msm_mddi_timer.pt_fn = mddi_start_update;
    timer_add_periodic(&msm_mddi_timer);

    // register our framebuffer device
    static struct fb_device fbdev;
    fbdev.fb_base   = kva2pa((void *)FB);
    fbdev.fb_npages = ((2 * fb_width * fb_height) + PGSIZE - 1) / PGSIZE; 
    fbdev.fb_arg    = &fbdev;
    fbdev.fb_set    = &msm_mddi_fb_set;

    // populate anything that lib/fb.c grabs from the vbe fields
    fbdev.fb_mode.vm.xres = fb_width;
    fbdev.fb_mode.vm.yres = fb_height;
    fbdev.fb_mode.vm.bpp = 16;
    fbdev.fb_mode.vm.bytes_per_scanline = 2 * fb_width;
    fbdev.fb_mode.vm.fb_color[vbe_red  ].masksize = 5;
    fbdev.fb_mode.vm.fb_color[vbe_green].masksize = 6;
    fbdev.fb_mode.vm.fb_color[vbe_blue ].masksize = 5;
    fbdev.fb_mode.vm.fb_color[vbe_red  ].fieldpos = 0;
    fbdev.fb_mode.vm.fb_color[vbe_green].fieldpos = 5;
    fbdev.fb_mode.vm.fb_color[vbe_blue ].fieldpos = 11;
    fbdev_register(&fbdev);
}
