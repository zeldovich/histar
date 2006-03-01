#ifndef HASHTABLE_H_
#define HASHTABLE_H_

// simple hashtable, uses open addressing with linear probing
// cannot delete or resize

#include <inc/types.h>

struct hashentry
{
        uint64_t key ;
        uint64_t val ;
} ;

struct hashtable
{
        struct hashentry *table ;
        int capacity ;
        int size ;
} ;

void hash_init(struct hashtable *table, struct hashentry *back, int n) ;
int hash_put(struct hashtable *table, uint64_t key, uint64_t val) ;
int hash_get(struct hashtable *table, uint64_t key, uint64_t *val) ;
void hash_print(struct hashtable *table) ;

#endif /*HASHTABLE_H_*/
