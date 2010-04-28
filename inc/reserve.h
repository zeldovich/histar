#ifndef JOS_INC_CAPACITOR_H
#define JOS_INC_CAPACITOR_H

struct ReserveInfo {
    uint64_t rs_level;		// mJ
    uint64_t rs_consumed;	// mJ
    uint64_t rs_decayed;	// mJ
};

#endif

