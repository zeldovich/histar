#ifndef CAD_HH_
#define CAD_HH_

typedef enum {
    aa_new,
} auth_agent_op_t;

struct auth_agent_arg
{
    auth_agent_op_t op;    
    uint64_t agent_grant;
    char     name[16];
    // return
    uint64_t agent_ct;
};

#endif /*CAD_HH_*/
