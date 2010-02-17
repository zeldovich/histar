struct qmi_config {
	uint32_t ip, mask, gateway, dns1, dns2;
};

extern "C" {
int	smd_qmi_open(int);
void	smd_qmi_close(int);
int	smd_qmi_read(int, unsigned char *, int);
void	smd_qmi_readwait(const int *, int *, const int);
int	smd_qmi_write(int, const unsigned char *, int);
int	smd_qmi_init(void);
void    smd_qmi_config(struct qmi_config *, struct qmi_config *,
	    struct qmi_config *);
}
