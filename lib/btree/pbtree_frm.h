#ifndef FRM_H_
#define FRM_H_

#include <lib/btree/btree_utils.h>

// buffer for freelist node operations
struct frm
{
#define FRM_BUF_SIZE 10  // 2x the value of the freelist btree's max height
    uint64_t to_use[FRM_BUF_SIZE] ;
    uint64_t to_free[FRM_BUF_SIZE] ;
    
    uint32_t n_use ;
    uint32_t n_free ;

    uint8_t service ;
    uint8_t servicing ;
} ;

struct freelist ;

int frm_free(uint64_t id, offset_t offset, void *arg) ;
int frm_new(uint64_t id, uint8_t **mem, uint64_t *off, void *arg) ;
void frm_service_one(struct frm *f, struct freelist *l) ;
void frm_service(struct freelist *l) ;
uint32_t frm_init(struct frm *f, uint64_t base) ;


#endif /*FRM_H_*/
