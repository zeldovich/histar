#include <bits/unimpl.h>
#include <resolv.h>

int
__res_mkquery(int op, const char *dname, int class, int type,
	      const u_char *data, int datalen, const u_char *newrr_in, u_char *buf,
	      int buflen)
{
    set_enosys();
    return -1;
}

int
__dn_skipname(const u_char *comp_dn, const u_char *eom)
{
    set_enosys();
    return -1;
}
