extern "C" {

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

/*
 * _C_toupper_ Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

const short _C_toupper_[1 + 256] = {
	EOF,
	0x00,	0x01,	0x02,	0x03,	0x04,	0x05,	0x06,	0x07,
	0x08,	0x09,	0x0a,	0x0b,	0x0c,	0x0d,	0x0e,	0x0f,
	0x10,	0x11,	0x12,	0x13,	0x14,	0x15,	0x16,	0x17,
	0x18,	0x19,	0x1a,	0x1b,	0x1c,	0x1d,	0x1e,	0x1f,
	0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	0x26,	0x27,
	0x28,	0x29,	0x2a,	0x2b,	0x2c,	0x2d,	0x2e,	0x2f,
	0x30,	0x31,	0x32,	0x33,	0x34,	0x35,	0x36,	0x37,
	0x38,	0x39,	0x3a,	0x3b,	0x3c,	0x3d,	0x3e,	0x3f,
	0x40,	0x41,	0x42,	0x43,	0x44,	0x45,	0x46,	0x47,
	0x48,	0x49,	0x4a,	0x4b,	0x4c,	0x4d,	0x4e,	0x4f,
	0x50,	0x51,	0x52,	0x53,	0x54,	0x55,	0x56,	0x57,
	0x58,	0x59,	0x5a,	0x5b,	0x5c,	0x5d,	0x5e,	0x5f,
	0x60,	'A',	'B',	'C',	'D',	'E',	'F',	'G',
	'H',	'I',	'J',	'K',	'L',	'M',	'N',	'O',
	'P',	'Q',	'R',	'S',	'T',	'U',	'V',	'W',
	'X',	'Y',	'Z',	0x7b,	0x7c,	0x7d,	0x7e,	0x7f,
	0x80,	0x81,	0x82,	0x83,	0x84,	0x85,	0x86,	0x87,
	0x88,	0x89,	0x8a,	0x8b,	0x8c,	0x8d,	0x8e,	0x8f,
	0x90,	0x91,	0x92,	0x93,	0x94,	0x95,	0x96,	0x97,
	0x98,	0x99,	0x9a,	0x9b,	0x9c,	0x9d,	0x9e,	0x9f,
	0xa0,	0xa1,	0xa2,	0xa3,	0xa4,	0xa5,	0xa6,	0xa7,
	0xa8,	0xa9,	0xaa,	0xab,	0xac,	0xad,	0xae,	0xaf,
	0xb0,	0xb1,	0xb2,	0xb3,	0xb4,	0xb5,	0xb6,	0xb7,
	0xb8,	0xb9,	0xba,	0xbb,	0xbc,	0xbd,	0xbe,	0xbf,
	0xc0,	0xc1,	0xc2,	0xc3,	0xc4,	0xc5,	0xc6,	0xc7,
	0xc8,	0xc9,	0xca,	0xcb,	0xcc,	0xcd,	0xce,	0xcf,
	0xd0,	0xd1,	0xd2,	0xd3,	0xd4,	0xd5,	0xd6,	0xd7,
	0xd8,	0xd9,	0xda,	0xdb,	0xdc,	0xdd,	0xde,	0xdf,
	0xe0,	0xe1,	0xe2,	0xe3,	0xe4,	0xe5,	0xe6,	0xe7,
	0xe8,	0xe9,	0xea,	0xeb,	0xec,	0xed,	0xee,	0xef,
	0xf0,	0xf1,	0xf2,	0xf3,	0xf4,	0xf5,	0xf6,	0xf7,
	0xf8,	0xf9,	0xfa,	0xfb,	0xfc,	0xfd,	0xfe,	0xff
};

const short *_toupper_tab_ = _C_toupper_;

/* garbage to get libhtc_ril.so to link without using -fstack-protector */
long __stack_chk_guard[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

void __stack_chk_fail(void);
void __attribute__((__noreturn__))
__stack_chk_fail(void)
{
	fprintf(stderr, "%s: STACK CHECK FAILED\n", __func__);
	exit(1);
}

volatile int *__errno(void);
volatile int *
__errno(void)
{
	return &errno;
}

int __android_log_print(int, const char *, const char *, ...);
int
__android_log_print(int level, const char *tag, const char *fmt, ...)
{
	va_list va;

	fprintf(stderr, "android_log: level %d, tag '%s': ", level, tag);
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	fprintf(stderr, "\n");

	return (0);
}

int property_set(const char *, const char *);
int property_get(const char *, char *, const char *);

int
property_set(const char *key, const char *val)
{
	fprintf(stderr, "property_set: [%s] [%s]\n", key, val);
	return (0);
}

int
property_get(const char *key, char *val, const char *default_val)
{
	fprintf(stderr, "property_get: [%s] [%s]\n", key, default_val);
	strcpy(val, default_val);
	return (strlen(default_val));
}

/* libs/cutils/strdup8to16.c
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/


/* See http://www.unicode.org/reports/tr22/ for discussion
 * on invalid sequences
 */

#define UTF16_REPLACEMENT_CHAR 0xfffd

/* Clever trick from Dianne that returns 1-4 depending on leading bit sequence*/
#define UTF8_SEQ_LENGTH(ch) (((0xe5000000 >> ((ch >> 3) & 0x1e)) & 3) + 1)

/* note: macro expands to multiple lines */
#define UTF8_SHIFT_AND_MASK(unicode, byte)  \
            (unicode)<<=6; (unicode) |= (0x3f & (byte));

#define UNICODE_UPPER_LIMIT 0x10fffd    

/**
 * out_len is an out parameter (which may not be null) containing the
 * length of the UTF-16 string (which may contain embedded \0's)
 */

typedef uint16_t char16_t;

char16_t *strdup8to16(const char *, size_t *);
char16_t *strcpy8to16(char16_t *, const char *, size_t *);
size_t strlen8to16(const char *);
char16_t *strcpylen8to16(char16_t *, const char *, int, size_t *);

// XXX
//#define SIZE_MAX       (18446744073709551615UL)
#define SIZE_MAX       (4294967295U)

char16_t * strdup8to16 (const char* s, size_t *out_len)
{
    char16_t *ret;
    size_t len;

    if (s == NULL) return NULL;

    len = strlen8to16(s);

    // fail on overflow
    if (len && SIZE_MAX/len < sizeof(char16_t))
        return NULL;

    // no plus-one here. UTF-16 strings are not null terminated
    ret = (char16_t *) malloc (sizeof(char16_t) * len);

    return strcpy8to16 (ret, s, out_len);
}

/**
 * Like "strlen", but for strings encoded with Java's modified UTF-8.
 *
 * The value returned is the number of UTF-16 characters required
 * to represent this string.
 */
size_t strlen8to16 (const char* utf8Str)
{
    size_t len = 0;
    int ic;
    int expected = 0;

    while ((ic = *utf8Str++) != '\0') {
        /* bytes that start 0? or 11 are lead bytes and count as characters.*/
        /* bytes that start 10 are extention bytes and are not counted */
         
        if ((ic & 0xc0) == 0x80) {
            /* count the 0x80 extention bytes. if we have more than
             * expected, then start counting them because strcpy8to16
             * will insert UTF16_REPLACEMENT_CHAR's
             */
            expected--;
            if (expected < 0) {
                len++;
            }
        } else {
            len++;
            expected = UTF8_SEQ_LENGTH(ic) - 1;

            /* this will result in a surrogate pair */
            if (expected == 3) {
                len++;
            }
        }
    }

    return len;
}



/*
 * Retrieve the next UTF-32 character from a UTF-8 string.
 *
 * Stops at inner \0's
 *
 * Returns UTF16_REPLACEMENT_CHAR if an invalid sequence is encountered
 *
 * Advances "*pUtf8Ptr" to the start of the next character.
 */
static inline uint32_t getUtf32FromUtf8(const char** pUtf8Ptr)
{
    uint32_t ret;
    int seq_len;
    int i;

    /* Mask for leader byte for lengths 1, 2, 3, and 4 respectively*/
    static const char leaderMask[4] = {0xff, 0x1f, 0x0f, 0x07};

    /* Bytes that start with bits "10" are not leading characters. */
    if (((**pUtf8Ptr) & 0xc0) == 0x80) {
        (*pUtf8Ptr)++;
        return UTF16_REPLACEMENT_CHAR;
    }

    /* note we tolerate invalid leader 11111xxx here */    
    seq_len = UTF8_SEQ_LENGTH(**pUtf8Ptr);

    ret = (**pUtf8Ptr) & leaderMask [seq_len - 1];

    if (**pUtf8Ptr == '\0') return ret;

    (*pUtf8Ptr)++;
    for (i = 1; i < seq_len ; i++, (*pUtf8Ptr)++) {
        if ((**pUtf8Ptr) == '\0') return UTF16_REPLACEMENT_CHAR;
        if (((**pUtf8Ptr) & 0xc0) != 0x80) return UTF16_REPLACEMENT_CHAR;

        UTF8_SHIFT_AND_MASK(ret, **pUtf8Ptr);
    }

    return ret;
}


/**
 * out_len is an out parameter (which may not be null) containing the
 * length of the UTF-16 string (which may contain embedded \0's)
 */

char16_t * strcpy8to16 (char16_t *utf16Str, const char*utf8Str, 
                                       size_t *out_len)
{   
    char16_t *dest = utf16Str;

    while (*utf8Str != '\0') {
        uint32_t ret;

        ret = getUtf32FromUtf8(&utf8Str);

        if (ret <= 0xffff) {
            *dest++ = (char16_t) ret;
        } else if (ret <= UNICODE_UPPER_LIMIT)  {
            /* Create surrogate pairs */
            /* See http://en.wikipedia.org/wiki/UTF-16/UCS-2#Method_for_code_points_in_Plane_1.2C_Plane_2 */

            *dest++ = 0xd800 | ((ret - 0x10000) >> 10);
            *dest++ = 0xdc00 | ((ret - 0x10000) &  0x3ff);
        } else {
            *dest++ = UTF16_REPLACEMENT_CHAR;
        }
    }

    *out_len = dest - utf16Str;

    return utf16Str;
}

/**
 * length is the number of characters in the UTF-8 string.
 * out_len is an out parameter (which may not be null) containing the
 * length of the UTF-16 string (which may contain embedded \0's)
 */

char16_t * strcpylen8to16 (char16_t *utf16Str, const char*utf8Str,
                                       int length, size_t *out_len)
{
    /* TODO: Share more of this code with the method above. Only 2 lines changed. */
    
    char16_t *dest = utf16Str;

    const char *end = utf8Str + length; /* This line */
    while (utf8Str < end) {             /* and this line changed. */
        uint32_t ret;

        ret = getUtf32FromUtf8(&utf8Str);

        if (ret <= 0xffff) {
            *dest++ = (char16_t) ret;
        } else if (ret <= UNICODE_UPPER_LIMIT)  {
            /* Create surrogate pairs */
            /* See http://en.wikipedia.org/wiki/UTF-16/UCS-2#Method_for_code_points_in_Plane_1.2C_Plane_2 */

            *dest++ = 0xd800 | ((ret - 0x10000) >> 10);
            *dest++ = 0xdc00 | ((ret - 0x10000) &  0x3ff);
        } else {
            *dest++ = UTF16_REPLACEMENT_CHAR;
        }
    }

    *out_len = dest - utf16Str;

    return utf16Str;
}

/* libs/cutils/strdup16to8.c
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

size_t strnlen16to8(const char16_t *, size_t);
char *strncpy16to8(char *, const char16_t *, size_t);
char *strndup16to8(const char16_t *, size_t);

/**
 * Given a UTF-16 string, compute the length of the corresponding UTF-8
 * string in bytes.
 */
size_t strnlen16to8(const char16_t* utf16Str, size_t len)
{
   size_t utf8Len = 0;

   while (len--) {
       unsigned int uic = *utf16Str++;

       if (uic > 0x07ff)
           utf8Len += 3;
       else if (uic > 0x7f || uic == 0)
           utf8Len += 2;
       else
           utf8Len++;
   }
   return utf8Len;
}


/**
 * Convert a Java-Style UTF-16 string + length to a JNI-Style UTF-8 string.
 *
 * This basically means: embedded \0's in the UTF-16 string are encoded
 * as "0xc0 0x80"
 *
 * Make sure you allocate "utf8Str" with the result of strlen16to8() + 1,
 * not just "len".
 * 
 * Please note, a terminated \0 is always added, so your result will always
 * be "strlen16to8() + 1" bytes long.
 */
char* strncpy16to8(char* utf8Str, const char16_t* utf16Str, size_t len)
{
    char* utf8cur = utf8Str;

    while (len--) {
        unsigned int uic = *utf16Str++;

        if (uic > 0x07ff) {
            *utf8cur++ = (uic >> 12) | 0xe0;
            *utf8cur++ = ((uic >> 6) & 0x3f) | 0x80;
            *utf8cur++ = (uic & 0x3f) | 0x80;
        } else if (uic > 0x7f || uic == 0) {
            *utf8cur++ = (uic >> 6) | 0xc0;
            *utf8cur++ = (uic & 0x3f) | 0x80;
        } else {
            *utf8cur++ = uic;

            if (uic == 0) {
                break;
            }           
        }       
    }

   *utf8cur = '\0';

   return utf8Str;
}

/**
 * Convert a UTF-16 string to UTF-8.
 *
 * Make sure you allocate "dest" with the result of strblen16to8(),
 * not just "strlen16()".
 */
char * strndup16to8 (const char16_t* s, size_t n)
{
    char *ret;

    if (s == NULL) {
        return NULL;
    }

    ret = (char *)malloc(strnlen16to8(s, n) + 1);

    strncpy16to8 (ret, s, n);
    
    return ret;    
}

int
android_get_control_socket(const char *name)
{
	const char *path;

	if (strcmp(name, "rild") == 0) {
		path = "/tmp/rild-socket";
	} else if (strcmp(name, "rild-debug") == 0) {
		path = "/tmp/rild-debug-socket";
	} else {
		fprintf(stderr, "%s: unknown control socket name '%s'\n", name);
		return (-1);
	}

	int s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		fprintf(stderr, "%s: socket() failed\n", __func__);
		return (s);
	}

	struct sockaddr_un sun;
	sun.sun_family = AF_UNIX;
	strcpy(sun.sun_path, path);
	unlink(sun.sun_path);
	int ret = bind(s, (struct sockaddr *)&sun, sizeof(sun.sun_family) +
	    strlen(sun.sun_path));
	if (ret != 0) {
		fprintf(stderr, "%s: bind() failed\n", __func__);
		close(s);
		return (ret);
	}

	return (s);
}

} // extern "C"
