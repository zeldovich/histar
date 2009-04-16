#ifndef JOS_DEV_MSM_TTYCONSREG
#define JOS_DEV_MSM_TTYCONSREG

struct msm_ttycons_reg {
	volatile uint32_t mtr_mr1;		/* 0x00 */
	volatile uint32_t mtr_mr2;		/* 0x04 */
	volatile uint32_t mtr_csr;		/* 0x08 rd: status, wr: ctrl */
	volatile uint32_t mtr_tfrf;		/* 0x0c rd: recv, wr: xmit */
	volatile uint32_t mtr_cr;		/* 0x10 */ 
	volatile uint32_t mtr_imr;		/* 0x14 */
	volatile uint32_t mtr_ipr;		/* 0x18 */
	volatile uint32_t mtr_tfwr;		/* 0x1c tx watermark */
	volatile uint32_t mtr_rfwr;		/* 0x20 rx watermark */
	volatile uint32_t mtr_hcr;		/* 0x24 */
	volatile uint32_t mtr_mreg;		/* 0x28 */
	volatile uint32_t mtr_nreg;		/* 0x2c */
	volatile uint32_t mtr_dreg;		/* 0x30 */
	volatile uint32_t mtr_mndreg;		/* 0x34 */
	volatile uint32_t mtr_irda;		/* 0x38 */
	volatile uint32_t mtr_pad;		/* 0x3c */
	volatile uint32_t mtr_misr_mode;	/* 0x40 */
	volatile uint32_t mtr_misr_reset;	/* 0x44 */
	volatile uint32_t mtr_misr_export;	/* 0x48 */
	volatile uint32_t mtr_misr_val;		/* 0x4c */
	volatile uint32_t mtr_test_ctrl;	/* 0x50 */
};

#define MSM_UART_CSR_HUNT_CHAR		(1 << 7)
#define MSM_UART_CSR_RX_BREAK		(1 << 6)
#define MSM_UART_CSR_PAR_FRAME_ERR	(1 << 5)
#define MSM_UART_CSR_OVERRUN		(1 << 4)
#define MSM_UART_CSR_TX_EMPTY		(1 << 3)
#define MSM_UART_CSR_TX_READY		(1 << 2)
#define MSM_UART_CSR_RX_FULL		(1 << 1)
#define MSM_UART_CSR_RX_READY		(1 << 0)

#endif /* !JOS_DEV_MSM_TTYCONSREG */
