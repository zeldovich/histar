#ifndef JOS_INC_DIS_ADMINCLIENT_H
#define JOS_INC_DIS_ADMINCLIENT_H

#include <inc/container.h>

class admin_client
{
public:
    admin_client(const char *name);
    ~admin_client(void);

    void register_cat(uint64_t cat);

private:
    cobj_ref admin_gate(void) const;
    char *name_;
};

#endif
