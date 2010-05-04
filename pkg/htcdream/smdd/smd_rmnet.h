#ifndef SMD_RMNET_H_
#define SMD_RMNET_H_

struct rmnet_stats {
	size_t tx_frames, tx_frame_bytes, tx_dropped;
	size_t rx_frames, rx_frame_bytes, rx_dropped;
	uint64_t tx_last_nsec;
	uint64_t rx_last_nsec;
};

#ifdef __cplusplus
extern "C" {
#endif
int	smd_rmnet_open(int);
int	smd_rmnet_close(int);
int	smd_rmnet_xmit(int, void *, int);
int	smd_rmnet_recv(int, void *, int);
int	smd_rmnet_fast_setup(int, void *, int, void *, int);
int	smd_rmnet_stats(int, struct rmnet_stats *);
void	smd_rmnet_init(void);
#ifdef __cplusplus
};
#endif

#define NPKTQUEUE 128 
struct ringpkt {
        char buf[1514];
        int bytes;
};

struct ringseg {
        struct ringpkt q[NPKTQUEUE];
        uint64_t q_head;
        uint64_t q_tail;
};

#endif
