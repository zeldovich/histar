#ifndef JOS_INC_SMDD_H
#define JOS_INC_SMDD_H

#include <inc/container.h>

// from linux's htc_battery.c
struct htc_get_batt_info_rep {
	struct rpc_reply_hdr hdr; 
	struct battery_info_reply {
		uint32_t batt_id;            /* Battery ID from ADC */
		uint32_t batt_vol;           /* Battery voltage from ADC */
		uint32_t batt_temp;          /* Battery Temperature (C) from formula and ADC */
		uint32_t batt_current;       /* Battery current from ADC */
		uint32_t level;              /* formula */
		uint32_t charging_source;    /* 0: no cable, 1:usb, 2:AC */
		uint32_t charging_enabled;   /* 0: Disable, 1: Enable */
		uint32_t full_bat;           /* Full capacity of battery (mAh) */
	} info;
};

enum {
	tty_open,
	tty_read,
	tty_write,
	tty_close,
	qmi_open,
	qmi_read,
	qmi_write,
	qmi_close,
	qmi_readwait,
	rpcrouter_create_local_endpoint,
	rpcrouter_destroy_local_endpoint,
	rpc_register_server,
	rpc_unregister_server,
	rpc_read,
	rpc_write,
	rpc_endpoint_read_select,
	get_battery_info,
	rmnet_open,
	rmnet_config,
	rmnet_tx,
	rmnet_rx
};

struct smdd_req {
	int op;
	struct cobj_ref obj;
	int fd;
	void *token;
	int bufbytes;
	union {
		char buf[256];
	};
};

struct htc_netconfig {
	uint32_t ip, mask, gw, dns1, dns2;
};

struct smdd_reply {
	int err;
	int fd;
	void *token;
	int bufbytes;
	union {
		char buf[256];
		struct htc_get_batt_info_rep batt_info; 
		struct htc_netconfig netconfig;
	};
};

#endif /* !JOS_INC_SMDD_H */
