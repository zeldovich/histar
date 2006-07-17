#ifndef JOS_INC_DIS_OMCLIENT_H
#define JOS_INC_DIS_OMCLIENT_H

#include <inc/dis/omparam.h>
#include <inc/container.h>

class om_client
{
public:
    om_client(const char *name);
    ~om_client(void);

    bool observable(om_res *res, palid k);
    bool modifiable(om_res *res, palid k);
    
private:
    cobj_ref om_gate(void) const;
    int om_test(om_res *res, palid k, bool observe);

    char *name_;
};

#endif
