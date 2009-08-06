extern "C" {
#include <inc/string.h>
#include <inc/gateparam.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/mman.h>

#include <inc/vt220iso8x16.h>

}

#include "../smdd/msm_rpcrouter2.h"
#include <inc/smdd.h>
#include <inc/rild.h>

#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>

#include "../support/misc.h"
#include "../support/smddgate.h"

#define BG	0x5555
#define FG	0xeeee

static wsdisplay_font *the_face = &vt220iso8x16;
static uint8_t *the_face_buf;

static unsigned int char_width = 8;
static unsigned int char_height = 16;

static int batt_level = 0;
static int batt_ma = 0;
static int ril_state = 0;
static const char *ril_state_str = "off";

static const char *state2str[5] = {
	"off",
	"not initialized",
	"radio on, sim not ready",
	"radio on, sim locked or absent",
	"radio on, sim ready"
};

// ick. lots of duplication from fbconsd
static void
flatten_font()
{
	static uint32_t bytes_per_pixel = 2;
	uint8_t *buf = (uint8_t *) malloc(the_face->numchars * bytes_per_pixel *
	    char_width * char_height);

	uint64_t bits = the_face->numchars * char_width * char_height;
	for (uint64_t p = 0; p < bits; p++) {
		uint64_t o = bytes_per_pixel * p;
		for (uint8_t b = 0; b < bytes_per_pixel; b++)
		buf[o + b] = ((the_face->data[p / 8] >> (8 - (p % 8))) & 1) ?
		    FG : BG;
	}

	// we never free this mem, but whatever
	the_face_buf = buf;
}

static void
render(void *fb_mem, uint32_t row, uint32_t col, uint32_t c, uint8_t inverse)
{
	// *some* attempt to mimic NetBSD's font structure where convenient
	static uint32_t bytes_per_pixel = 2;
	static uint64_t bytes_per_glyph = char_width * char_height *
	    bytes_per_pixel;
	const uint8_t *src = &the_face_buf[(bytes_per_glyph * c) %
	    (the_face->numchars * bytes_per_glyph)];

	int borderpx = 0;
	int xres = 320;
	for (uint32_t y = 0; y < char_height; y++) {
		char *dst = (char *) fb_mem;
		dst = &dst[((row * char_height + (y) + borderpx) * xres +
		col * char_width + borderpx) * bytes_per_pixel];
		if (inverse) {
			for (uint32_t x = 0; x < char_width * bytes_per_pixel; x++)
				dst[x] = src[y * char_width * bytes_per_pixel] ^ 0xFF;
		} else {
			memcpy(dst, &src[y * char_width * bytes_per_pixel],
			    char_width * bytes_per_pixel);
		}
	}
}

static void
printline(uint16_t *fb, int n, const char *msg)
{
	if (n < 0 || n >= 2) {
		fprintf(stderr, "bard: invalid line\n");
		return;
	}

	fb += (n * 320 * 16);

	// 40 chars/line
	int i;
	for (i = 0; i < 320 / 8 && msg[i] != '\0'; i++) {
		render(fb, 0, i, msg[i], 0);
	}
	while (i < 320 / 8)
		render(fb, 0, i++, ' ', 0);
}

static void
update_stats()
{
	// grab radio info from rild, battery info from battd
	struct htc_get_batt_info_rep batt_info;

	if (smddgate_get_battery_info(&batt_info) == 0) {
		batt_ma = be32_to_cpu(batt_info.info.batt_current);
		batt_level = be32_to_cpu(batt_info.info.level);
	}

	struct gate_call_data gcd;
	struct rild_req *req = (struct rild_req *)&gcd.param_buf[0];
	struct rild_reply *rep = (struct rild_reply *)&gcd.param_buf[0];

	int64_t rildgt = container_find(start_env->root_container, kobj_gate, "rild gate");
	if (rildgt >= 0) {
		struct cobj_ref rildgate = COBJ(start_env->root_container, rildgt);

		req->op = get_ril_state;
		gate_call(rildgate, 0, 0, 0).call(&gcd, 0);
		if (rep->err >= 0 && rep->err <= 4) {
			ril_state = rep->err;
			ril_state_str = state2str[rep->err];
		}
	}
}

static void
handle_bar(uint16_t *fb, int xres, int yres)
{
	int i, j;

	// clear it
	for (i = 0; i < yres; i++)
		for (j = 0; j < xres; j++)
			fb[i * xres + j] = BG;

	while (1) {
		char line1[41];
		char line2[41];

		update_stats();

		snprintf(line1, sizeof(line1), "Battery %d%% (current: %d mA)",
		    batt_level, batt_ma);
		snprintf(line2, sizeof(line2), "Radio state %d (%s)",
		    ril_state, ril_state_str);

		printline(fb, 0, line1);
		printline(fb, 1, line2);

		// could just be event-driven...
		sleep(1);
	}
}

int
main(int argc, char **argv)
try
{
	void *fb_mem;
	int fb_fd;

	if (smddgate_init()) {
		fprintf(stderr, "bard: smddgate_init failed\n");
		exit(1);
	}

	flatten_font();

	fb_fd = open("/dev/fb0", O_RDWR);
	if (fb_fd == -1) {
		fprintf(stderr, "bard: failed to open fb device\n");
		exit(1);
	}

	fb_mem = mmap(0, 320 * 480 * 2, PROT_READ | PROT_WRITE,
	    MAP_SHARED, fb_fd, 0);
	if (fb_mem == MAP_FAILED) {
		fprintf(stderr, "bard: couldn't mmap fb device\n");
		exit(1);
	}
	
	// screen is 320x480. we have last 32 y pixels (bottom most; 16bpp)
	int skip = 320 * (480 - 32);
	handle_bar((uint16_t *)fb_mem + skip, 320, 32);

	return (0);
} catch (std::exception &e) {
	printf("battd: %s\n", e.what());
}
