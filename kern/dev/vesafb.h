#ifndef JOS_DEV_VESAFB_H
#define JOS_DEV_VESAFB_H

#include <dev/vesa.h>

void	vesafb_init(struct vbe_control_info *ctl_info,
		    struct vbe_mode_info *mode_info,
		    uint32_t mode);

#endif
