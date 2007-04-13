#ifndef JOS_INC_NTP_H
#define JOS_INC_NTP_H

#include <inc/types.h>

typedef struct {
    uint32_t ts_sec;
    uint32_t ts_frac;
} ntp_ts;

struct ntp_auth {
    uint32_t key_id;
    uint32_t md[4];
};

struct ntp_packet {
    uint8_t ntp_lvm;
    uint8_t ntp_stratum;
    uint8_t ntp_poll;
    uint8_t ntp_precision;

    uint32_t ntp_root_delay;
    uint32_t ntp_root_dispersion;
    uint32_t ntp_reference_id;

    ntp_ts ntp_ref_ts;
    ntp_ts ntp_originate_ts;
    ntp_ts ntp_receive_ts;
    ntp_ts ntp_transmit_ts;

    struct ntp_auth ntp_auth[];
};

#define NTP_LI_NONE	0
#define NTP_LI_61	1
#define NTP_LI_59	2
#define NTP_LI_NOSYNC	3

#define NTP_MODE_CLIENT	3
#define NTP_MODE_SERVER	4
#define NTP_MODE_BCAST	5

#define NTP_LVM_ENCODE(li, ver, mode) \
	((((li) & 3) << 6) | (((ver) & 7) << 3) | ((mode) & 7))
#define NTP_LVM_LI(lvm) (((lvm) >> 6) & 3)
#define NTP_LVM_VER(lvm) (((lvm) >> 3) & 7)
#define NTP_LVM_MODE(lvm) ((lvm) & 7)

#endif
