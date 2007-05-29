#include <inc/fd.h>
#include <inc/udsimpl.h>

struct Dev devuds = {
    .dev_id = 'u',
    .dev_name = "unix-domain",
    .dev_read = &uds_read,
    .dev_write = &uds_write,
    .dev_close = &uds_close,
    .dev_connect = &uds_connect,
    .dev_bind = &uds_bind,
    .dev_listen = &uds_listen,
    .dev_accept = &uds_accept,
    .dev_onfork = &uds_onfork,
};
