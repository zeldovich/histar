#include <kern/kobjhdr.h>

typedef enum {
    device_net,
    device_fb
} device_type;

struct Device {
    struct kobject_hdr dv_ko;
    device_type dv_type;
    uint64_t dv_idx;
};


