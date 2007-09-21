#ifndef JOS_INC_ALIGNMACRO_H
#define JOS_INC_ALIGNMACRO_H

#define __jos_alignment_of(type)					\
    ({									\
	struct {							\
	    type a;							\
	    char b;							\
	} __align_plus_one;						\
	sizeof(__align_plus_one) - sizeof(type);			\
    })

#endif
