#ifndef BTREE_DEBUG_H_
#define BTREE_DEBUG_H_

#include <btree/btree.h>

void btree_sanity_check_impl(void *tree);
void btree_pretty_print_impl(void *tree);

#endif /*BTREE_DEBUG_H_ */
