#include <inc/base64.h>

#include <stdlib.h>
#include <string.h>

/*
 * Copied from http://base64.sourceforge.net/b64.c
 */

static const char cb64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char cd64[] =
    "|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

static void __attribute__((unused))
encodeblock(unsigned char in[3], unsigned char out[4], int len)
{
    out[0] = cb64[ in[0] >> 2 ];
    out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
    out[2] = (unsigned char) (len > 1 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
    out[3] = (unsigned char) (len > 2 ? cb64[ in[2] & 0x3f ] : '=');
}

static void
decodeblock(unsigned char in[4], unsigned char out[3])
{   
    out[0] = (unsigned char ) (in[0] << 2 | in[1] >> 4);
    out[1] = (unsigned char ) (in[1] << 4 | in[2] >> 2);
    out[2] = (unsigned char ) (((in[2] << 6) & 0xc0) | in[3]);
}

char *
base64_decode(char *s)
{
    // Wasteful but safe
    char *ret = malloc(strlen(s) + 1);

    if (ret == 0)
	return 0;

    unsigned char *p = (unsigned char *) s;
    unsigned char *r = (unsigned char *) ret;

    while (*p) {
	unsigned char in[4], out[3], v;

	int len, i;
	for (len = 0, i = 0; i < 4 && *p; i++) {
	    v = 0;
	    while (*p && v == 0) {
		v = (unsigned char) *(p++);
		v = (unsigned char) ((v < 43 || v > 122) ? 0 : cd64[ v - 43 ]);
		if (v)
		    v = (unsigned char) ((v == '$') ? 0 : v - 61);
	    }

	    if (v) {
		len++;
		in[i] = (unsigned char) (v - 1);
	    }
        }

	if (len) {
	    decodeblock(in, out);
	    for (i = 0; i < len - 1; i++)
		*(r++) = out[i];
	}
    }

    *r = '\0';
    return ret;
}
