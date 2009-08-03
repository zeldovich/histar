#ifndef _SMDDGATE_H_
#define _SMDDGATE_H_

extern "C" {

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
int smddgate_qmi_select();
void *smddgate_rpcrouter_create_local_endpoint(int, uint32_t, uint32_t);
int smddgate_rpcrouter_destroy_local_endpoint(void *);
int smddgate_rpc_read(void *, void *, size_t);
int smddgate_rpc_write(void *, const void *, size_t);
int smddgate_rpc_select();

}

#endif	/* !_SMDDGATE_H_ */
