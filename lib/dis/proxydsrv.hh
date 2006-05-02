#ifndef PROXYD_HH_
#define PROXYD_HH_

#include <inc/cpplabel.hh>

struct cobj_ref proxydsrv_create(uint64_t gate_container, const char *name,
                label *label, label *clearance);

#endif /*PROXYD_HH_*/
