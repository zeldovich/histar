#ifndef _BTREE_H_
#define _BTREE_H_

#include <inc/types.h>
#include <lib/btree/btree_utils.h>

struct btree_node
{
	struct btree *tree ;

	struct {
		offset_t offset ;
        uint16_t flags ;
        char 	 dirty;
	} block ;

	uint8_t 		keyCount ;          
	offset_t 	   	*children ;    
	const uint64_t 	*keys ;
};

#define BTREE_NODE_SIZE(order, key_size) (sizeof(struct btree_node) + sizeof(offset_t) * order + \
	sizeof(uint64_t) * (order - 1) * (key_size))


struct btree_manager ;

struct btree
{
	struct btree_manager *mm ;

	uint8_t	order; 
	uint8_t	s_key ;
	
	uint64_t size; 

	uint8_t min_leaf;   
	uint8_t min_intrn;    
	
	offset_t root;           
	offset_t left_leaf;       

	// current filePos on inserts...no touch
	offset_t _insFilePos;    
};

struct btree_manager
{
	int (*node)(struct btree *tree, offset_t offset, struct btree_node **store, void *arg) ;
	int (*free)(void *arg, offset_t offset) ;
	int (*alloc)(struct btree *tree, struct btree_node **store, void *arg) ;
	int (*pin_is)(void *arg, offset_t offset, uint8_t pin) ;

	void *arg ;
} ;

int 	 btree_insert(struct btree * tree, const uint64_t *key, offset_t offset) ;
int64_t	 btree_delete(struct btree *tree, const uint64_t *key);
char 	 btree_is_empty(struct btree *tree);
void 	 btree_init(struct btree * tree, char order, char key_size, struct btree_manager * mm) ;
uint64_t btree_size(struct btree *tree);

// match key exactly
offset_t btree_search(struct btree *tree, const uint64_t *key, uint64_t *key_store) ;
// match the closest key less than or equal to the given key
offset_t btree_ltet(struct btree *tree, const uint64_t *key, uint64_t *key_store) ;
// match the closest key greater than or equal to the given key
offset_t btree_gtet(struct btree *tree, const uint64_t *key, uint64_t *key_store) ;

// debug
void bt_pretty_print(struct btree *tree, offset_t rootOffset, int i);

#endif /* _BTREE_H_ */

