#ifndef LINUX_ARCH_INCLUDE_OS_LIB_NET_H
#define LINUX_ARCH_INCLUDE_OS_LIB_NET_H

int if_up(int s, const char *if_alias, unsigned int ip);
int rt_add_gw(int s, unsigned int gw_ip, unsigned int gw_dst, unsigned int gw_nm,
	      const char *if_alias);

/* for debug */
int if_print_all(int s);
int if_print_up(int s);

#endif
