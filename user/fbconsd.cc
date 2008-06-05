extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <inc/fbcons.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/fd.h>
#include <inc/fb.h>
#include <linux/fb.h>
#include <inc/string.h>
#include <inc/kbdcodes.h>

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>

enum { vt_count = 6 };

static fbcons_seg *all_fs[vt_count];
static uint64_t cur_vt;
static jthread_mutex_t vt_mu;
static uint64_t reboots;

static FT_Face the_face;
static uint32_t cols, rows;
static uint32_t char_width, char_height;
static fb_var_screeninfo fbinfo;
static uint64_t borderpx;

static int fb_fd;
static void *fb_mem;
static uint64_t fb_size;

static FT_Face
get_font(const char *name)
{
    FcInit();

    FcPattern *p = FcNameParse((const FcChar8 *) name);
    FcConfigSubstitute(0, p, FcMatchPattern);
    FcDefaultSubstitute(p);

    FcResult r = FcResultMatch;
    FcPattern *m = FcFontMatch(0, p, &r);
    FcPatternDestroy(p);
    if (r != FcResultMatch) {
	fprintf(stderr, "get_font: no match\n");
	return 0;
    }

    FcChar8 *fontname = FcNameUnparse(m);
    char *colon = strchr((const char *) fontname, ':');
    if (colon)
	*colon = '\0';
    printf("Found font %s\n", fontname);

    FT_Library freetype;
    if (FT_Init_FreeType(&freetype)) {
	fprintf(stderr, "get_font: FT_Init_FreeType failed\n");
	return 0;
    }

    FT_Face f = 0;
    r = FcPatternGetFTFace(m, FC_FT_FACE, 0, &f);
    if (r == FcResultMatch)
	return f;

    FcChar8 *filename;
    r = FcPatternGetString(m, FC_FILE, 0, &filename);
    if (r == FcResultMatch) {
	double pixelsize;
	r = FcPatternGetDouble(m, FC_PIXEL_SIZE, 0, &pixelsize);
	if (r != FcResultMatch)
	    pixelsize = 16;

	if (FT_New_Face(freetype, (const char *) filename, 0, &f)) {
	    fprintf(stderr, "get_font: cannot allocate face\n");
	    return 0;
	}

	FT_Set_Pixel_Sizes(f, 0, (int) pixelsize);
	return f;
    }

    fprintf(stderr, "get_font: cannot open font\n");
    return 0;
}

static FT_BitmapGlyph
get_glyph_bitmap(FT_Face face, uint32_t c)
{
    if (FT_Load_Char(face, c, FT_LOAD_DEFAULT))
	return 0;

    FT_Glyph glyph;
    if (FT_Get_Glyph(face->glyph, &glyph))
	return 0;

    if (glyph->format != FT_GLYPH_FORMAT_BITMAP) {
	FT_Vector pen;
	pen.x = 0;
	pen.y = -face->size->metrics.descender;

	if (FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, &pen, 1)) {
	    FT_Done_Glyph(glyph);
	    return 0;
	}
    }

    return (FT_BitmapGlyph) glyph;
}

static void
render(uint32_t row, uint32_t col, uint32_t c, uint8_t inverse)
{
    uint32_t bytes_per_pixel = (fbinfo.bits_per_pixel + 7) / 8;
    uint8_t *buf = (uint8_t *) malloc(bytes_per_pixel *
				      char_width * char_height);
    if (!buf) {
	fprintf(stderr, "render: cannot allocate bitmap\n");
	return;
    }
    memset(buf, inverse ? 255 : 0,
	   bytes_per_pixel * char_width * char_height);

    FT_Bitmap *bitmap;
    uint8_t *src;

    FT_BitmapGlyph glyph = get_glyph_bitmap(the_face, c);
    if (!glyph)
	goto draw;

    bitmap = &glyph->bitmap;
    src = bitmap->buffer;

    for (int y = 0; y < bitmap->rows; y++, src += bitmap->pitch) {
	for (int x = 0; x < bitmap->width; x++) {
	    uint32_t xpos = glyph->left + x;
	    uint32_t ypos = char_height - glyph->top + y;

	    if (xpos >= char_width || ypos >= char_height)
		continue;

	    uint8_t val = 0;
	    if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO)
		val = (src[x >> 3] & (1 << (7 - (x & 7)))) ? 255 : 0;
	    else if (bitmap->pixel_mode == FT_PIXEL_MODE_GRAY)
		val = src[x];
	    else
		continue;

	    if (inverse)
		val = 255 - val;

	    for (uint32_t i = 0; i < bytes_per_pixel; i++)
		buf[(ypos * char_width + xpos) * bytes_per_pixel + i] = val;
	}
    }

    FT_Done_Glyph((FT_Glyph) glyph);

 draw:
    for (uint32_t y = 0; y < char_height; y++) {
        char *dst = (char *) fb_mem;
        dst = &dst[((row * char_height + (y) + borderpx) * fbinfo.xres +
		    col * char_width + borderpx) * bytes_per_pixel];
        memcpy(dst, &buf[y * char_width * bytes_per_pixel],
	       char_width * bytes_per_pixel);
    }
    free(buf);
}

static void
refresh(volatile uint32_t *newbuf, uint32_t *oldbuf,
	uint64_t *oldredraw, uint64_t curredraw,
	uint32_t *oldcurx, uint32_t *oldcury,
	uint32_t newcurx, uint32_t newcury)
{
    if (curredraw != *oldredraw && borderpx) {
	uint32_t bytes_per_pixel = (fbinfo.bits_per_pixel + 7) / 8;
	uint32_t buflen = bytes_per_pixel * fbinfo.xres;
	uint8_t *buf = (uint8_t *) malloc(buflen);
	if (buf) {
	    memset(buf, 0xff, buflen);

	    char *dst;
	    for (uint32_t y = 0; y < borderpx; y++) {
		dst = (char *) fb_mem;
                dst = &dst[fbinfo.xres * y * bytes_per_pixel];
                memcpy(dst, buf, fbinfo.xres * bytes_per_pixel);

		dst = (char *) fb_mem;
                dst = &dst[fbinfo.xres * (fbinfo.yres - y - 1) *
                           bytes_per_pixel];
                memcpy(dst, buf, fbinfo.xres * bytes_per_pixel);
	    }

	    for (uint32_t y = 0; y < fbinfo.yres; y++) {
		dst = (char *) fb_mem;
                dst = &dst[fbinfo.xres * y * bytes_per_pixel];
                memcpy(dst, buf, borderpx * bytes_per_pixel);

		dst = (char *) fb_mem;
                dst = &dst[(fbinfo.xres * (y + 1) - borderpx) *
			   bytes_per_pixel];
                memcpy(dst, buf, borderpx * bytes_per_pixel);
	    }

	    free(buf);
	}
    }

    for (uint32_t r = 0; r < rows; r++) {
	for (uint32_t c = 0; c < cols; c++) {
	    uint32_t i = r * cols + c;
	    if ((oldbuf[i] != newbuf[i]) ||
		(c == *oldcurx && r == *oldcury) ||
		(c == newcurx && r == newcury) ||
		(curredraw != *oldredraw))
	    {
		render(r, c, newbuf[i], (c == newcurx && r == newcury));
		oldbuf[i] = newbuf[i];
	    }
	}
    }

    *oldcurx = newcurx;
    *oldcury = newcury;
    *oldredraw = curredraw;
}

static void __attribute__((noreturn))
input_worker(void *arg)
{
    for (;;) {
	unsigned char c;
	ssize_t cc = read(0, &c, 1);
	if (cc <= 0)
	    continue;

	if (c >= KEY_F1 && c <= KEY_F6) {
	    jthread_mutex_lock(&vt_mu);
	    cur_vt = (c - KEY_F1);
	    sys_sync_wakeup(&cur_vt);
	    jthread_mutex_unlock(&vt_mu);
	    continue;
	}

	fbcons_seg *fs = all_fs[cur_vt];
	jthread_mutex_lock(&fs->mu);

	if (fs->incount < sizeof(fs->inbuf)) {
	    uint64_t npos = (fs->inpos + fs->incount) % sizeof(fs->inbuf);
	    fs->inbuf[npos] = c;
	    fs->incount++;
	    sys_sync_wakeup(&fs->incount);
	}
	jthread_mutex_unlock(&fs->mu);
    }
}

static void __attribute__((noreturn))
reboot_watcher(void *arg)
{
    /*
     * This thread abuses the sys_sync_wait() behavior of waking up
     * every waiting thread after machine reboot.
     */
    uint64_t zero = 0;

    for (;;) {
	sys_sync_wait(&zero, 0, UINT64(~0));
	reboots++;
	sys_sync_wakeup(&reboots);
    }
}

static void __attribute__((noreturn))
worker(void *arg)
{
    int r = sys_self_set_waitslots(3);
    if (r < 0) {
	fprintf(stderr, "cannot set waitslots\n");
	exit(-1);
    }

    fbcons_seg *fs;
    uint32_t *screenbuf = (uint32_t *) malloc(rows * cols * sizeof(fs->data[0]));
    if (!screenbuf) {
	fprintf(stderr, "cannot allocate screen buffer\n");
	exit(-1);
    }

 new_vt:
    jthread_mutex_lock(&vt_mu);
    uint64_t worker_vt = cur_vt;
    jthread_mutex_unlock(&vt_mu);

    uint64_t curboot = reboots;

    fs = all_fs[worker_vt];
    memset(screenbuf, 0, rows * cols * sizeof(fs->data[0]));

    jthread_mutex_lock(&fs->mu);

    uint32_t oldx = 0, oldy = 0;
    uint64_t updates = 0;
    uint64_t redraw = fs->redraw - 1;

    for (;;) {
	updates = fs->updates;
	if (!fs->stopped)
	    refresh(&fs->data[0], screenbuf,
		    &redraw, fs->redraw,
		    &oldx, &oldy, fs->xpos, fs->ypos);

	while (fs->updates == updates &&
	       worker_vt == cur_vt &&
	       reboots == curboot)
	{
	    jthread_mutex_unlock(&fs->mu);

	    volatile uint64_t *addrs[] = { &fs->updates, &cur_vt, &reboots };
	    uint64_t vals[] = { updates, worker_vt, curboot };
	    uint64_t refcts[] = { 0, 0, 0 };

	    sys_sync_wait_multi(&addrs[0], &vals[0], &refcts[0],
				3, UINT64(~0));

	    jthread_mutex_lock(&fs->mu);
	}

	if (worker_vt != cur_vt || reboots != curboot) {
	    jthread_mutex_unlock(&fs->mu);
	    goto new_vt;
	}
    }
}

int
main(int ac, char **av)
try
{
    if (ac != 6) {
	fprintf(stderr, "Usage: %s taint grant fbdevpath fontname "
                        "borderpixels\n", av[0]);
	exit(-1);
    }

    uint64_t taint, grant;
    error_check(strtou64(av[1], 0, 10, &taint));
    error_check(strtou64(av[2], 0, 10, &grant));

    fb_fd = open(av[3], O_RDWR);
    if (fb_fd < 0) {
        fprintf(stderr, "Couldn't open fb at %s\n", av[3]);
        exit(-1);
    }
    
    const char *fontname = av[4];
    error_check(strtou64(av[5], 0, 10, &borderpx));

    the_face = get_font(fontname);
    if (!the_face) {
	fprintf(stderr, "Font problem, exiting.\n");
	exit(-1);
    }

    char_width = (the_face->bbox.xMax - the_face->bbox.xMin) *
		 the_face->size->metrics.x_ppem / the_face->units_per_EM;
    char_height = (the_face->bbox.yMax - the_face->bbox.yMin) *
		  the_face->size->metrics.y_ppem / the_face->units_per_EM;

    error_check(ioctl(fb_fd, FBIOGET_VSCREENINFO, &fbinfo));

    fb_size = fbinfo.xres * fbinfo.yres * fbinfo.bits_per_pixel;
    fb_mem = mmap(0, fb_size, PROT_READ | PROT_WRITE,
                  MAP_SHARED, fb_fd, 0);
    if (fb_mem == MAP_FAILED) {
        fprintf(stderr, "Couldn't mmap fb from %s\n", av[3]);
        exit(-1);
    }

    if (borderpx * 2 >= fbinfo.xres || borderpx * 2 >= fbinfo.yres) {
	fprintf(stderr, "Border too large (%"PRIu64")\n", borderpx);
	exit(-1);
    }

    cols = (fbinfo.xres - borderpx * 2) / char_width;
    rows = (fbinfo.yres - borderpx * 2) / char_height;
    printf("Console size: %d x %d\n", cols, rows);

    label fsl(1);
    fsl.set(taint, 3);
    fsl.set(grant, 0);

    for (uint32_t vt = 0; vt < vt_count; vt++) {
	char buf[16];
	sprintf(&buf[0], "consbuf%d", vt);

	cobj_ref fs_seg;
	fbcons_seg *fs = 0;
	error_check(segment_alloc(start_env->shared_container,
				  sizeof(*fs) +
				  rows * cols * sizeof(fs->data[0]),
				  &fs_seg, (void **) &fs,
				  fsl.to_ulabel(), &buf[0]));
	all_fs[vt] = fs;

	fs->taint = taint;
	fs->grant = grant;
	fs->cols = cols;
	fs->rows = rows;

	for (uint32_t i = 0; i < rows * cols; i++)
	    fs->data[i] = ' ';

	char msgbuf[64];
	sprintf(&msgbuf[0], "fbconsd running, vt%d.", vt);
	for (uint32_t i = 0; i < strlen(msgbuf); i++)
	    fs->data[i] = msgbuf[i];
	fs->ypos = 1;

	struct fs_object_meta m;
	m.dev_id = devfbcons.dev_id;
	error_check(sys_obj_set_meta(fs_seg, 0, &m));
    }

    cobj_ref tid;
    error_check(thread_create(start_env->proc_container,
			      &worker, 0, &tid, "worker"));
    error_check(thread_create(start_env->proc_container,
			      &input_worker, 0, &tid, "input-worker"));
    error_check(thread_create(start_env->proc_container,
			      &reboot_watcher, 0, &tid, "reboot-watcher"));

    process_report_exit(0, 0);
    for (;;) {
	uint64_t v;
	sys_sync_wait(&v, v, UINT64(~0));
    }
} catch (std::exception &e) {
    fprintf(stderr, "%s: %s\n", av[0], e.what());
    exit(-1);
}
