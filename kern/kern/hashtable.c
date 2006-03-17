#include <kern/lib.h>
#include <kern/hashtable.h>
#include <inc/error.h>

#define TOMB  0xFFFFFFFFFFFFFFFF
#define LEN   1
#define LEV   0xDEADBEEF

// hash function from: http://burtleburtle.net/bob/hash/evahash.html 

/*
--------------------------------------------------------------------
mix -- mix 3 64-bit values reversibly.
mix() takes 48 machine instructions, but only 24 cycles on a superscalar
  machine (like Intel's new MMX architecture).  It requires 4 64-bit
  registers for 4::2 parallelism.
All 1-bit deltas, all 2-bit deltas, all deltas composed of top bits of
  (a,b,c), and all deltas of bottom bits were tested.  All deltas were
  tested both on random keys and on keys that were nearly all zero.
  These deltas all cause every bit of c to change between 1/3 and 2/3
  of the time (well, only 113/400 to 287/400 of the time for some
  2-bit delta).  These deltas all cause at least 80 bits to change
  among (a,b,c) when the mix is run either forward or backward (yes it
  is reversible).
This implies that a hash using mix64 has no funnels.  There may be
  characteristics with 3-bit deltas or bigger, I didn't test for
  those.
--------------------------------------------------------------------
*/
#define mix64(a,b,c) \
{ \
        a -= b; a -= c; a ^= (c>>43); \
        b -= c; b -= a; b ^= (a<<9); \
        c -= a; c -= b; c ^= (b>>8); \
        a -= b; a -= c; a ^= (c>>38); \
        b -= c; b -= a; b ^= (a<<23); \
        c -= a; c -= b; c ^= (b>>5); \
        a -= b; a -= c; a ^= (c>>35); \
        b -= c; b -= a; b ^= (a<<49); \
        c -= a; c -= b; c ^= (b>>11); \
        a -= b; a -= c; a ^= (c>>12); \
        b -= c; b -= a; b ^= (a<<18); \
        c -= a; c -= b; c ^= (b>>22); \
}


/*
--------------------------------------------------------------------
 This works on all machines, is identical to hash() on little-endian 
 machines, and it is much faster than hash(), but it requires
 -- that the key be an array of uint64_t's, and
 -- that all your machines have the same endianness, and
 -- that the length be the number of uint64_t's in the key
--------------------------------------------------------------------
*/
static uint64_t 
hash2(register uint64_t *k, register uint64_t length, register uint64_t level)
{
        register uint64_t a,b,c,len;

        /* Set up the internal state */
        len = length;
        a = b = level;                         /* the previous hash value */
        c = 0x9e3779b97f4a7c13LL; /* the golden ratio; an arbitrary value */


        /*---------------------------------------- handle most of the key */
        while (len >= 3)
        {
                a += k[0];
                b += k[1];
                c += k[2];
                mix64(a,b,c);
                k += 3; len -= 3;
        }

        /*-------------------------------------- handle the last 2 uint64_t's */
        c += length;
        switch(len)              /* all the case statements fall through */
        {
        /* c is reserved for the length */
                case  2: b+=k[1];
                case  1: a+=k[0];
        /* case 0: nothing left to add */
        }

        mix64(a,b,c);
        /*-------------------------------------------- report the result */
        return c;
}


int
hash_put(struct hashtable *table, uint64_t key, uint64_t val)
{
        uint64_t len = 1 ;
        uint64_t lev = 0xDEADBEEF ;
        uint64_t probe = 0 ;
   
        if (key == 0 || key == TOMB)
                return -E_INVAL ;

        if (table->size == table->capacity)
                return -E_NO_SPACE ;

        int i ;
        for (i = 0 ; i < table->capacity ; i++) {
                probe = (hash2(&key, len, lev) + i) % table->capacity ;
                if (table->entry[probe].key == 0 
                || table->entry[probe].key == key)
                        break ;               
        }              

        table->entry[probe].key = key ;
        table->entry[probe].val = val ;
        table->size++ ;
        return 0 ;
}

int
hash_get(struct hashtable *table, uint64_t key, uint64_t *val)
{
        uint64_t len = 1 ;
        uint64_t lev = 0xDEADBEEF ;
        uint64_t probe ;
   
        if (key == 0 || key == TOMB)
                return -E_INVAL ;
   
        for (int i = 0 ; i < table->capacity ; i++) {
                probe = (hash2(&key, len, lev) + i) % table->capacity ;
                if (table->entry[probe].key == key) {
                        *val = table->entry[probe].val ;
                        return 0 ;
                }
                else if (table->entry[probe].key == 0)
                        break ;
        }
        return -E_NOT_FOUND ;
}

int
hash_del(struct hashtable *table, uint64_t key)
{
    uint64_t len = 1 ;
    uint64_t lev = 0xDEADBEEF ;
    uint64_t probe ;
   
   
    if (key == 0 || key == TOMB)
        return -E_INVAL ;
        
    for (int i = 0 ; i < table->capacity ; i++) {
            probe = (hash2(&key, len, lev) + i) % table->capacity ;
            if (table->entry[probe].key == key) {
                    table->entry[probe].key = TOMB ;
                    table->entry[probe].val = 0 ;
                    table->size-- ;
                    return 0 ;
            }
            else if (table->entry[probe].key == 0)
                    break ;
    }

    return -E_NOT_FOUND ;   
}

void
hash_init(struct hashtable *table, struct hashentry *back, int n)
{
        memset(back, 0, sizeof(struct hashentry) * n) ;
        table->entry = back ;
        table->capacity = n ;      
        table->size = 0 ; 
}

void
hash_print(struct hashtable *table)
{
        int i ;
        for (i = 0 ; i < table->capacity ; i++)
                cprintf("i %d key %ld val %ld\n", i, table->entry[i].key, table->entry[i].val) ;
}


void 
hashiter_init(struct hashtable *table, struct hashiter *iter)
{
    memset(iter, 0, sizeof(*iter)) ;
    iter->hi_table = table ;
    return ;   
}

int 
hashiter_next(struct hashiter *iter)
{
    struct hashtable *t = iter->hi_table ;
    
    int i = iter->hi_index ;
    for (; i < t->capacity ; i++) {
        if (t->entry[i].key != 0 &&
            t->entry[i].key != TOMB)
        {
            iter->hi_index = i + 1 ;
            iter->hi_val = t->entry[i].val ;
            iter->hi_key = t->entry[i].key ; 
            return 1 ;
        }
    }
    return 0 ;
}
