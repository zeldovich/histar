#ifndef JOS_INC_NETDSRV_HH
#define JOS_INC_NETDSRV_HH

#include <inc/cpplabel.hh>
#include <inc/gatesrv.hh>

gatesrv *netd_server_init(uint64_t gate_ct, uint64_t entry_ct,
			  uint64_t taint_handle,
			  label *l, label *clear);

#endif
