int lwip_init(void (*cb)(void *), void *cbarg, 
	      const char* iface_alias, const char* mac_addr);
const char *lwip_mac_addr(void);
