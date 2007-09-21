/*
 * A simple remote file system protocol..
 */

typedef string djfs_pathname<>;

enum djfs_op {
    DJFS_READDIR = 1,
    DJFS_READ,
    DJFS_WRITE
};

struct djfs_readdir_arg {
    djfs_pathname pn;
};

struct djfs_readdir_res {
    djfs_pathname ents<>;
};

struct djfs_read_arg {
    djfs_pathname pn;
};

struct djfs_read_res {
    opaque data<>;
};

struct djfs_write_arg {
    djfs_pathname pn;
    opaque data<>;
};

union djfs_request switch (djfs_op op) {
 case DJFS_READDIR:
    djfs_readdir_arg readdir;
 case DJFS_READ:
    djfs_read_arg read;
 case DJFS_WRITE:
    djfs_write_arg write;
};

union djfs_reply_data switch (djfs_op op) {
 case DJFS_READDIR:
    djfs_readdir_res readdir;
 case DJFS_READ:
    djfs_read_res read;
 case DJFS_WRITE:
    void;
};

union djfs_reply switch (int err) {
 case 0:
    djfs_reply_data d;
 default:
    void;
};
