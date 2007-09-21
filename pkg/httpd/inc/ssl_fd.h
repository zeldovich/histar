#ifndef JOS_INC_SSL_FD_H
#define JOS_INC_SSL_FD_H

int ssl_accept(void *ctx, int s);
int ssl_init(const char *server_pem, const char *dh_pem, 
	     const char *servkey_pem, void **ctx);

#endif
