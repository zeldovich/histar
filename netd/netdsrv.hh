#ifndef JOS_INC_NETDSRV_HH
#define JOS_INC_NETDSRV_HH

#include <inc/cpplabel.hh>
#include <inc/gatesrv.hh>

typedef void (*netd_handler)(struct netd_op_args *);

void netd_server_init(uint64_t gate_ct,
		      uint64_t taint_handle,
		      label *l, label *clear,
		      netd_handler h);
void netd_server_enable(void);

#endif
