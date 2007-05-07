#include <inc/openssl.h>
#include <inc/stdio.h>

#include <stdio.h>
#include <errno.h>

#include <openssl/err.h>

static void
openssl_print_errq(int (*print)(const char *fmt, ...))
{
    int e;
    char buf[128];

    print("openssl error queue:\n");
    
    while ((e = ERR_get_error())) {
	ERR_error_string(e, buf);
	print(" %s\n", buf);
    }
}

void
openssl_print_error(SSL *ssl, int r, int use_cprintf)
{
    int e;

    int __attribute__((format (__printf__, 1, 0)))
	(*print)(const char *fmt, ...) = &printf;
    if (use_cprintf)
	print = &cprintf;

    switch ((e = SSL_get_error(ssl, r))) {
    case SSL_ERROR_NONE:
	print("SSL_ERROR_NONE\n");
	break;
    case SSL_ERROR_ZERO_RETURN:
	print("SSL_ERROR_ZERO_RETURN: SSL connection closed\n");
	break;
    case SSL_ERROR_WANT_READ:
	print("SSL_ERROR_WANT_READ: operation could not complete -- retry\n");
	break;
    case SSL_ERROR_WANT_WRITE:
	print("SSL_ERROR_WANT_WRITE: operation could not complete -- retry\n");
	break;
    case SSL_ERROR_WANT_CONNECT:
	print("SSL_ERROR_WANT_CONNECT: "
	       "operation could not complete (not connected)\n");
	break;
    case SSL_ERROR_WANT_ACCEPT:
	print("SSL_ERROR_WANT_ACCEPT: "
	       "operation could not complete (not accepted)\n");
	break;
    case SSL_ERROR_WANT_X509_LOOKUP:
	print("SSL_ERROR_WANT_X509_LOOKUP: \n");
	break;
    case SSL_ERROR_SYSCALL:
	print("SSL_ERROR_SYSCALL: ");
	if (ERR_peek_error())
	    openssl_print_errq(print);
	else {
	    if (r == 0) 
		print("unexpected EOF\n");
	    else if (r == -1)
		print("%s\n", strerror(errno));
	    else 
		print("unknown error\n");
	}
	break;
    case SSL_ERROR_SSL:
	print("SSL_ERROR_SSL: ");
	openssl_print_errq(print);
	break;
    default:
	print("unknown SSL error: %d\n", e);
	break;
    }
}
