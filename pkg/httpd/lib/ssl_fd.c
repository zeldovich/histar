#include <inc/ssl_fd.h>
#include <inc/stdio.h>
#include <inc/fd.h>

#include <fcntl.h>

#include <openssl/ssl.h>

struct ssl_fd {
    SSL *ssl;
};

int
ssl_accept(void *ctx, int s)
{
    struct Fd *fd;
    int r = fd_alloc(&fd, "ssl fd");
    if (r < 0)
	return r;
    
    BIO *sbio = BIO_new_socket(s, BIO_NOCLOSE);
    SSL *ssl = SSL_new(ctx);
    SSL_set_bio(ssl, sbio, sbio);

    
    if((r = SSL_accept(ssl)) <= 0)
	return -1;
    
    fd->fd_dev_id = 'S';
    fd->fd_omode = O_RDWR;

    struct ssl_fd * sfd = (struct ssl_fd *)fd->fd_cust.buf;
    sfd->ssl = ssl;

    return fd2num(fd);
}

static ssize_t
ssl_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    struct ssl_fd *sfd = (struct ssl_fd *)fd->fd_cust.buf;
    SSL *ssl = sfd->ssl;
    
    int r = SSL_read(ssl, (char *)buf, count);
    if (r <= 0)
	return -1;
    return r;
}

static ssize_t
ssl_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    struct ssl_fd *sfd = (struct ssl_fd *)fd->fd_cust.buf;
    SSL *ssl = sfd->ssl;
    
    int r = SSL_write(ssl, (char *)buf, count);
    if (r <= 0)
	return -1;
    return r;
}

static int
ssl_close(struct Fd *fd)
{
    struct ssl_fd *sfd = (struct ssl_fd *)fd->fd_cust.buf;
    SSL *ssl = sfd->ssl;

    // don't do two-step shutdown procedure
    SSL_shutdown(ssl);
    SSL_free(ssl);
    return 0;
}

struct Dev devssl = 
{
    .dev_id = 'S',
    .dev_name = "ssl",
    .dev_read = ssl_read,
    .dev_write = ssl_write,
    .dev_close = ssl_close,
};

int 
ssl_init(const char *server_pem, const char *dh_pem, 
	 const char *servkey_pem, void **ctx_store)
{
    dev_register(&devssl);
    
    SSL_CTX *ctx;
    SSL_library_init();
    SSL_load_error_strings();

    SSL_METHOD *meth = SSLv23_method();
    ctx = SSL_CTX_new(meth);

    // Load our keys and certificates
    if(!(SSL_CTX_use_certificate_chain_file(ctx, server_pem))) {
	cprintf("Can't read certificate file %s\n", server_pem);
	return -1;
    }
    
    // From an example
    if (OPENSSL_VERSION_NUMBER < 0x00905100L)
	SSL_CTX_set_verify_depth(ctx, 1);

    // Load the dh params to use
    if (dh_pem) {
	DH *ret = 0;
	BIO *bio;
	
	if (!(bio = BIO_new_file(dh_pem,"r"))) {
	    cprintf("Couldn't open DH file %s\n", dh_pem);
	    return -1;
	}
	
	ret = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
	BIO_free(bio);
	if(SSL_CTX_set_tmp_dh(ctx, ret) < 0 ) {
	    cprintf("Couldn't set DH parameters using %s\n", dh_pem);
	    return -1;
	}
    }

    if(!(SSL_CTX_use_PrivateKey_file(ctx, servkey_pem, SSL_FILETYPE_PEM)))
	cprintf("Can't read key file %s", servkey_pem);    

    *ctx_store = ctx;
    return 0;
}
