#ifdef __cplusplus
extern "C" {
#endif
int	smd_rmnet_open(int);
void	smd_rmnet_close(int);
int	smd_rmnet_xmit(int, void *, int);
int	smd_rmnet_recv(int, void *, int);
int	smd_rmnet_fast_setup(int, void *, int, void *, int);
void	smd_rmnet_init(void);
#ifdef __cplusplus
};
#endif

#define NPKTQUEUE 64
struct ringpkt {
        char buf[1514];
        int bytes;
};

struct ringseg {
        struct ringpkt q[NPKTQUEUE];
        uint64_t q_head;
        uint64_t q_tail;
};
