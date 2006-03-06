#ifndef _BTREE_H_
#define _BTREE_H_

#include <inc/types.h>
#include <inc/queue.h>
#include <lib/btree/btree_utils.h>
#include <machine/stackwrap.h>

struct btree_node
{
	struct btree *tree ;

	struct {
		offset_t offset ;
        uint16_t flags ;
	} block ;

	uint8_t 		keyCount ;          
	offset_t 	   	*children ;    
	const uint64_t 	*keys ;
	
	LIST_ENTRY(btree_node) node_link ;
    uint64_t    bytesize ;
};

struct btree_manager
{
	int (*node)(struct btree *tree, offset_t offset, struct btree_node **store, void *arg) ;
	int (*write)(struct btree_node *node, void *arg) ;
	int (*free)(void *arg, offset_t offset) ;
	int (*alloc)(struct btree *tree, struct btree_node **store, void *arg) ;
	int (*unpin_node)(void *arg, offset_t offset) ;

	void *arg ;
} ;

#define BTREE_MAGIC 0xcdef9425feed7980

struct btree
{
	struct btree_manager manager ;

	uint8_t	order; 
	uint8_t	s_key ;
	uint8_t s_value ;
	
	uint64_t size; 
        uint64_t height ;

	uint8_t min_leaf;   
	uint8_t min_intrn;    
	
	offset_t root;           
	offset_t left_leaf;       

	uint64_t magic ;
    uint64_t id ;
    
	struct lock lock ;
	
	// current filePos on inserts...no touch
	offset_t *_insFilePos;    
};

int 	 btree_insert(void *tree, const uint64_t *key, offset_t *val) ;
char	 btree_delete(void *tree, const uint64_t *key);
char 	 btree_is_empty(struct btree *tree);
void	 btree_init(struct btree * t, char order, char key_size, 
		 		    char value_size, struct btree_manager * mm) ;
uint64_t btree_size(struct btree *tree);
void 	 btree_erase(struct btree *t) ;

// match key exactly
int btree_search(void *tree, const uint64_t *key, 
					 uint64_t *key_store, uint64_t *val_store) ;
// match the closest key less than or equal to the given key
int btree_ltet(void *tree, const uint64_t *key, 
			   uint64_t *key_store, uint64_t *val_store) ;
// match the closest key greater than or equal to the given key
int btree_gtet(void *tree, const uint64_t *key, 
			   uint64_t *key_store, uint64_t *val_store) ;
void btree_manager_is(struct btree *t, struct btree_manager *mm) ;


#endif /* _BTREE_H_ */

