extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include <inc/fbcons.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/fd.h>
#include <inc/fb.h>
#include <inc/string.h>

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>

static FT_Face the_face;
static uint32_t cols, rows;
static uint32_t char_width, char_height;
static jos_fb_mode kern_fb;

static FT_Face
get_font(const char *name)
{
    FcInit();

    FT_Library freetype;
    if (FT_Init_FreeType(&freetype)) {
	fprintf(stderr, "get_font: FT_Init_FreeType failed\n");
	return 0;
    }

    FcPattern *p = FcNameParse((const FcChar8 *) name);
    FcConfigSubstitute(0, p, FcMatchPattern);
    FcDefaultSubstitute(p);

    FcResult r;
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
get_glyph_bitmap(FT_Face face, uint8_t c)
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
render(uint32_t row, uint32_t col, uint8_t c, uint8_t inverse)
{
    uint32_t bytes_per_pixel = (kern_fb.vm.bpp + 7) / 8;
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
    for (uint32_t y = 0; y < char_height; y++)
	sys_fb_set(((row * char_height + y) * kern_fb.vm.xres +
		    col * char_width) * bytes_per_pixel,
		   char_width * bytes_per_pixel,
		   &buf[y * char_width * bytes_per_pixel]);

    free(buf);
}

static void
refresh(volatile uint8_t *newbuf, uint8_t *oldbuf,
	uint32_t *oldcurx, uint32_t *oldcury,
	uint32_t newcurx, uint32_t newcury)
{
    for (uint32_t r = 0; r < rows; r++) {
	for (uint32_t c = 0; c < cols; c++) {
	    uint32_t i = r * cols + c;
	    if ((oldbuf[i] != newbuf[i]) ||
		(c == *oldcurx && r == *oldcury) ||
		(c == newcurx && r == newcury))
	    {
		render(r, c, newbuf[i], (c == newcurx && r == newcury));
		oldbuf[i] = newbuf[i];
	    }
	}
    }

    *oldcurx = newcurx;
    *oldcury = newcury;
}

static void __attribute__((noreturn))
worker(void *arg)
{
    fbcons_seg *fs = (fbcons_seg *) arg;

    uint8_t *screenbuf = (uint8_t *) malloc(rows * cols);
    if (!screenbuf) {
	fprintf(stderr, "cannot allocate screen buffer\n");
	exit(-1);
    }
    memset(screenbuf, 0, rows * cols);

    uint32_t oldx = 0, oldy = 0;
    uint64_t updates;

    jthread_mutex_lock(&fs->mu);

    for (;;) {
	updates = fs->updates;
	refresh(&fs->data[0], screenbuf, &oldx, &oldy, fs->xpos, fs->ypos);

	while (fs->updates == updates) {
	    jthread_mutex_unlock(&fs->mu);
	    sys_sync_wait(&fs->updates, updates, UINT64(~0));
	    jthread_mutex_lock(&fs->mu);
	}
    }
}

int
main(int ac, char **av)
try
{
    const char *fontname = "Monospace-16";

    if (ac != 3 && ac != 4) {
	fprintf(stderr, "Usage: %s taint grant [fontname]\n", av[0]);
	exit(-1);
    }

    uint64_t taint, grant;
    error_check(strtou64(av[1], 0, 10, &taint));
    error_check(strtou64(av[2], 0, 10, &grant));

    if (ac == 4)
	fontname = av[3];

    the_face = get_font(fontname);
    if (!the_face) {
	fprintf(stderr, "Font problem, exiting.\n");
	exit(-1);
    }

    char_width = (the_face->bbox.xMax - the_face->bbox.xMin) *
		 the_face->size->metrics.x_ppem / the_face->units_per_EM;
    char_height = (the_face->bbox.yMax - the_face->bbox.yMin) *
		  the_face->size->metrics.y_ppem / the_face->units_per_EM;

    error_check(sys_fb_get_mode(&kern_fb));

    cols = kern_fb.vm.xres / char_width;
    rows = kern_fb.vm.yres / char_height;
    printf("Console size: %d x %d\n", cols, rows);

    label fsl(1);
    fsl.set(taint, 3);
    fsl.set(grant, 0);

    cobj_ref fs_seg;
    fbcons_seg *fs = 0;
    error_check(segment_alloc(start_env->shared_container,
			      sizeof(*fs) + rows * cols,
			      &fs_seg, (void **) &fs,
			      fsl.to_ulabel(), "consbuf"));

    fs->taint = taint;
    fs->grant = grant;
    fs->cols = cols;
    fs->rows = rows;

    memset((uint8_t *) &fs->data[0], ' ', rows * cols);

    const char *msg = "fbconsd running.";
    memcpy((char *) &fs->data[0], msg, strlen(msg));
    fs->ypos = 1;

    struct fs_object_meta m;
    m.dev_id = devfbcons.dev_id;
    error_check(sys_obj_set_meta(fs_seg, 0, &m));

    cobj_ref tid;
    error_check(thread_create(start_env->proc_container,
			      &worker, fs, &tid, "worker"));

    process_report_exit(0, 0);
    for (;;) {
	uint64_t v;
	sys_sync_wait(&v, v, UINT64(~0));
    }
} catch (std::exception &e) {
    fprintf(stderr, "%s: %s\n", av[0], e.what());
    exit(-1);
}