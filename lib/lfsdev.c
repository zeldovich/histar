#include <inc/fd.h>
#include <inc/lfsimpl.h>

struct Dev devlfs = {
    .dev_id = 'o',
    .dev_name = "lind-fs",
    .dev_open = &lfs_open,
    .dev_close = &lfs_close,
    .dev_read = &lfs_read,
};
