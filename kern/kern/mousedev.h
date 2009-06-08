#ifndef JOS_KERN_MOUSEDEV_H
#define JOS_KERN_MOUSEDEV_H

#include <machine/types.h>

struct mouse_device {
    int (*mouse_read) (uint8_t*, uint64_t, uint64_t);
    int (*mouse_probe) (void);
};

enum { mousedevs_max = 1 };
extern struct mouse_device *mousedevs[mousedevs_max];
extern uint64_t mousedevs_num;

void mousedev_register(struct mouse_device *mousedev);

#endif
