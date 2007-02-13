#ifndef _BTREE_SYS_H_
#define _BTREE_SYS_H_

/*
 * All the btree system dependencies and knobs
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* basic types */
typedef uint64_t bkey_t;
typedef uint64_t bval_t;
typedef uint64_t offset_t;
typedef uint16_t bchild_ndx_t;

/* system emulation */
int sys_alloc(size_t n, offset_t *off);
int sys_free(offset_t off);
int sys_clear(void);
int sys_flush(void);
void sys_read(size_t n, offset_t off, void *buf);
void sys_write(size_t n, offset_t off, void *buf);

#define BKEY_F "lx"
#define BVAL_F "lx"
#define OFF_F  "lx"

#define wprintf printf

/* adjust how key and val variables are declared */
#define BKEY_T(td, k) bkey_t k[td->bt_key_len];
#define BVAL_T(td, v) bval_t v[td->bt_val_len];

#endif
