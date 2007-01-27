#ifndef JOS_INC_DIS_HH
#define JOS_INC_DIS_HH

#include <async.h>

class djprot : virtual public refcount {
 public:
    static ptr<djserv> alloc(uint16_t port);
};

#endif
