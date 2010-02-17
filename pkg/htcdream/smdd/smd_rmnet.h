extern "C" {
int	smd_rmnet_open(int);
void	smd_rmnet_close(int);
int	smd_rmnet_xmit(int, void *, int);
int	smd_rmnet_recv(int, void *, int);
void	smd_rmnet_init(void);
};
