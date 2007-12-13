#ifndef HASHTABLE_H_
#define HASHTABLE_H_

// simple hashtable, uses open addressing with linear probing

struct hashentry {
    uint64_t key;
    uint64_t val;
};

struct hashtable {
    union {
	struct hashentry *entry;
	struct hashentry **entry2;
    };

    int capacity;
    int size;
    int pgents;
};

struct hashiter {
    struct hashtable *hi_table;
    int hi_index;
    uint64_t hi_key;
    uint64_t hi_val;
};

void hash_init(struct hashtable *table, struct hashentry *back, int n);
void hash_init2(struct hashtable *table, struct hashentry **back, int n, int pgsize);
int hash_put(struct hashtable *table, uint64_t key, uint64_t val);
int hash_get(struct hashtable *table, uint64_t key, uint64_t *val);
int hash_del(struct hashtable *table, uint64_t key);
void hash_print(struct hashtable *table);

void hashiter_init(struct hashtable *table, struct hashiter *iter);
int hashiter_next(struct hashiter *iter);

#endif /*HASHTABLE_H_ */
