#ifndef JOS_INC_RBAC_HH
#define JOS_INC_RBAC_HH

#include <inc/container.h>

namespace rbac {
    void gate_send(struct cobj_ref gate, void *args, uint64_t n);
    struct cobj_ref gate_send(struct cobj_ref gate, struct cobj_ref arg);
    struct cobj_ref fs_object(const char *pn);
}

struct trans 
{
    struct cobj_ref trans_gate;
};

class role_gate
{
public:
    role_gate(const char *pn);

    void execute(struct trans *trans);
    void acquire(void);

private:
    struct cobj_ref acquire_gate_;
    struct cobj_ref trans_gate_;
};

void self_assign_role(role_gate *role);
struct trans trans_lookup(const char *pn);

#endif
