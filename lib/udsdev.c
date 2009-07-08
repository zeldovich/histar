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
    .dev_addref = &uds_addref,
    .dev_setsockopt = &uds_setsockopt,
    .dev_unref = &uds_unref,
    .dev_statsync = &uds_statsync,
    .dev_probe = &uds_probe,
    .dev_shutdown = &uds_shutdown,
    .dev_ioctl = &uds_ioctl,
    .dev_recvfrom = &uds_recvfrom,
    .dev_sendto = &uds_sendto
};
