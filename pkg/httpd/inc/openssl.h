#ifndef JOS_HTTPD_INC_OPENSSL_H
#define JOS_HTTPD_INC_OPENSSL_H

#include <openssl/ssl.h>

void openssl_print_error(SSL *ssl, int r, int use_cprintf);

#endif
