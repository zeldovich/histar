/*
 * A simple remote file system protocol..
 */

typedef string djfs_pathname<>;

enum djfs_op {
    DJFS_READDIR = 1
};

union djfs_request switch (djfs_op op) {
 case DJFS_READDIR:
    djfs_pathname pn;
};

union djfs_reply_data switch (djfs_op op) {
 case DJFS_READDIR:
    djfs_pathname ents<>;
};

union djfs_reply switch (int err) {
 case 0:
    djfs_reply_data d;
 default:
    void;
};
