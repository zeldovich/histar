#ifndef _SMDDGATE_H_
#define _SMDDGATE_H_

#ifdef __cplusplus 
extern "C" {
#endif

int smddgate_init();
int smddgate_get_battery_info(struct htc_get_batt_info_rep *);
int smddgate_tty_open(int);
int smddgate_tty_close(int);
int smddgate_tty_read(int, void *, size_t);
int smddgate_tty_write(int, const void *, size_t);
int smddgate_qmi_open(int);
int smddgate_qmi_close(int);
int smddgate_qmi_read(int, void *, size_t);
int smddgate_qmi_write(int, const void *, size_t);
int smddgate_qmi_readwait(int *, int *, int);
void *smddgate_rpcrouter_create_local_endpoint(int, uint32_t, uint32_t);
int smddgate_rpcrouter_destroy_local_endpoint(void *);
int smddgate_rpc_register_server(void *, uint32_t, uint32_t);
int smddgate_rpc_unregister_server(void *, uint32_t, uint32_t);
int smddgate_rpc_read(void *, void *, size_t);
int smddgate_rpc_write(void *, const void *, size_t);
int smddgate_rpc_endpoint_read_select(void **, int, uint64_t);
int smddgate_rmnet_open(int);
int smddgate_rmnet_config(int, struct htc_netconfig *);
int smddgate_rmnet_tx(int, char *, size_t);
int smddgate_rmnet_rx(int, char *, size_t);
int smddgate_rmnet_fast_setup(int, void **, void **);

#ifdef __cplusplus
}
#endif

#endif	/* !_SMDDGATE_H_ */
