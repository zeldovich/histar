#ifndef JOS_DEV_MSM_TTYCONSREG
#define JOS_DEV_MSM_TTYCONSREG

/* msm uart registers (w = write only, r = read only, rw = read/write) */
struct msm_ttycons_reg {
	volatile uint32_t uart_mr1;		/* 0x00 (rw) */
	volatile uint32_t uart_mr2;		/* 0x04 (rw) */
	union {
		volatile uint32_t uart_csr;	/* 0x08 (w) */
		volatile uint32_t uart_sr;	/* 0x08 (r) status register */
	};
	union {
		volatile uint32_t uart_tf;	/* 0x0c (w) next tx fifo val */
		volatile uint32_t uart_rf;	/* 0x0c (r) next rx fifo val */
	};
	union {
		volatile uint32_t uart_cr;	/* 0x10 (w) command reg */
		volatile uint32_t uart_misr;	/* 0x10 (r) mask = (isr & imr)*/
	};
	union {
		volatile uint32_t uart_imr;	/* 0x14 (w) intr mask reg */
		volatile uint32_t uart_isr;	/* 0x14 (r) intr status reg */
	};
	volatile uint32_t uart_ipr;		/* 0x18 (rw) intr prog reg */
	volatile uint32_t uart_tfwr;		/* 0x1c (rw) tx watermark */
	volatile uint32_t uart_rfwr;		/* 0x20 (rw) rx watermark */
	volatile uint32_t uart_hcr;		/* 0x24 (rw) hunt char reg */
	volatile uint32_t uart_mreg;		/* 0x28 (rw) M value reg */
	volatile uint32_t uart_nreg;		/* 0x2c (rw) N value reg */
	volatile uint32_t uart_dreg;		/* 0x30 (rw) D value reg */
	volatile uint32_t uart_mndreg;		/* 0x34 (rw) M,N,D extra val */
	volatile uint32_t uart_irda;		/* 0x38 (w) irda control */
	volatile uint32_t uart_sim_cfg;		/* 0x3c (w) uart3 SIM intrface*/
	volatile uint32_t uart_misr_mode;	/* 0x40 (rw) */
	volatile uint32_t uart_misr_reset;	/* 0x44 (rw) */
	volatile uint32_t uart_misr_export;	/* 0x48 (rw) */
	volatile uint32_t uart_misr_val;	/* 0x4c (rw) */
	volatile uint32_t uart_test_ctrl;	/* 0x50 (rw) */
};

#define UART_MR1_RX_RDY_CTL			0x0080
#define UART_MR1_CTS_CTL			0x0040
#define UART_MR1_AUTO_RFR_LEVEL_MASK		0x003f

#define UART_MR2_ERROR_MODE			0x0040
#define UART_MR2_BITS_PER_CHAR_MASK		0x0030
#define UART_MR2_BITS_PER_CHAR_5		0x0000
#define UART_MR2_BITS_PER_CHAR_6		0x0010 
#define UART_MR2_BITS_PER_CHAR_7		0x0020
#define UART_MR2_BITS_PER_CHAR_8		0x0030
#define UART_MR2_STOP_BIT_LEN_MASK		0x000c
#define UART_MR2_STOP_BIT_LEN_0_563		0x0000	/* 9/16 bit time(.563)*/
#define UART_MR2_STOP_BIT_LEN_1			0x0004
#define UART_MR2_STOP_BIT_LEN_1_563		0x0008	/* 1 9/16 bit time */
#define UART_MR2_STOP_BIT_LEN_2			0x000c
#define UART_MR2_PARITY_MODE_MASK		0x0003
#define UART_MR2_PARITY_MODE_NONE		0x0000
#define UART_MR2_PARITY_MODE_ODD		0x0001
#define UART_MR2_PARITY_MODE_EVEN		0x0002
#define UART_MR2_PARITY_MODE_SPACE		0x0003

#define UART_CSR_RX_CLK_SEL_MASK		0x00f0
#define UART_CSR_TX_CLK_SEL_MASK		0x000f

#define UART_SR_HUNT_CHAR			0x0080	/* hunt char recv'd */
#define UART_SR_RX_BREAK			0x0040	/* rx break */
#define UART_SR_PAR_FRAME_ERR			0x0020	/* parity error */
#define UART_SR_UART_OVERRUN			0x0010	/* rx fifo overrun */
#define UART_SR_TXEMT				0x0008	/* tx underrun */
#define UART_SR_TXRDY				0x0004	/* tx fifo not full */
#define UART_SR_RXFULL				0x0002	/* rx fifo full */ 
#define UART_SR_RXRDY				0x0001	/* rx fifo not full */

#define UART_CR_CHANNEL_COMMAND_MASK		0x01f0
#define UART_CR_CHANNEL_COMMAND_NULL		0x0000	/* nop */
#define UART_CR_CHANNEL_COMMAND_RESET_RX	0x0001	/* reset receiver */
#define UART_CR_CHANNEL_COMMAND_RESET_TX	0x0002	/* reset transmitter */
#define UART_CR_CHANNEL_COMMAND_RESET_ERR_STAT	0x0003	/* reset error status */
#define UART_CR_CHANNEL_COMMAND_RESET_BREAK	0x0004	/* reset break chg irq*/
#define UART_CR_CHANNEL_COMMAND_START_BREAK	0x0005	/* start break */
#define UART_CR_CHANNEL_COMMAND_STOP_BREAK	0x0006	/* stop break */
#define UART_CR_CHANNEL_COMMAND_RESET_CTS_N	0x0007	/* reset CTS_N ISR:5 */
#define UART_CR_CHANNEL_COMMAND_RESET_TX_ERROR	0x0008	/* reset TX_ERROR */
#define UART_CR_CHANNEL_COMMAND_PACKET_MODE	0x0009	/* reset TX_ERROR */
#define UART_CR_CHANNEL_COMMAND_MODE_RESET	0x000c	/* sample data md. off*/
#define UART_CR_CHANNEL_COMMAND_RFR_N		0x000d	/* assert RFR_N */
#define UART_CR_CHANNEL_COMMAND_RESET_RFR_ND	0x000d	/* reset RFR_N */
#define UART_CR_CHANNEL_COMMAND_CLEAR_TX_DONE	0x0010	/* clear TX_DONE ISR:8*/
#define UART_CR_TX_DISABLE			0x0008
#define UART_CR_TX_EN				0x0004
#define UART_CR_RX_DISABLE			0x0002
#define UART_CR_RX_EN				0x0001

// The following bits enable/disable various interrupts
#define UART_IMR_TX_DONE			0x0100	/* last byte sent */
#define UART_IMR_TX_ERROR			0x0080	/* parity error */
#define UART_IMR_CURRENT_CTS			0x0040	/* current CTS state */
#define UART_IMR_DELTA_CTS			0x0020	/* delta CTS state */
#define UART_IMR_RXLEV				0x0010	/* rx fifo watermark */
#define UART_IMR_RXSTALE			0x0008	/* rx fifo stale */
#define UART_IMR_RXHUNT				0x0004	/* rx break condition */
#define UART_IMR_HCR				0x0002	/* rx HCR enabled */
#define UART_IMR_TXLEV				0x0001	/* tx fifo watermark */

// The following bits indicate an interrupt due to an event enabled in the IMR.
#define UART_ISR_TX_DONE			0x0100
#define UART_ISR_TX_ERROR			0x0080
#define UART_ISR_CURRENT_CTS			0x0040
#define UART_ISR_DELTA_CTS			0x0020
#define UART_ISR_RXLEV				0x0010
#define UART_ISR_RXSTALE			0x0008
#define UART_ISR_RXHUNT				0x0004
#define UART_ISR_HCR				0x0002
#define UART_ISR_TXLEV				0x0001

#define UART_IPR_STALE_TIMEOUT_MSB_MASK		0x0080	/* STALE_TIMEOUT MSB */
#define UART_IPR_SAMPLE_DATA			0x0040	/* sample data mode on*/
#define UART_IPR_RXSTALE_LAST			0x0020	/* rxstale calculatn */
#define UART_IPR_STALE_TIMEOUT_LSB_MASK		0x000f	/* STALE_TIMEOUT LSB */

#define UART_TFWR_TFW_MASK			0x003f	/* tx watermark: 0-63 */

#define UART_RFWR_RFW_MASK			0x003f	/* rx watermark: 0-63 */

#define UART_HCR_CHAR_MASK			0x00ff	/* hunted char value */

#define UART_MNDREG_MREG_LSB_MASK		0x0020
#define UART_MNDREG_NREG_LSB_MASK		0x001c
#define UART_MNDREG_DREG_LSB_MASK		0x0003

#define UART_IRDA_IRDA_LOOPBACK			0x0008	/* enable loopback */
#define UART_IRDA_INVERT_IRDA_TX		0x0004	/* invert tx polarity */
#define UART_IRDA_INVERT_IRDA_RX		0x0002	/* invert rx polarity */
#define UART_IRDA_IRDA_EN			0x0001	/* enable irda trancvr*/

#define UART_SIM_CFG_UIM_TX_MODE		0x20000
#define UART_SIM_CFG_UIM_RX_MODE		0x10000
#define UART_SIM_CFG_SIM_STOP_BIT_LEN_MASK	0x0ff00
#define UART_SIM_CFG_SIM_CLK_ON			0x00080
#define UART_SIM_CFG_SIM_CLK_TD8_SEL		0x00040
#define UART_SIM_CFG_SIM_CLK_STOP_HIGH		0x00020
#define UART_SIM_CFG_SIM_CLK_SEL		0x00010
#define UART_SIM_CFG_MASK_RX			0x00008
#define UART_SIM_CFG_SWAP_D			0x00004
#define UART_SIM_CFG_INV_D			0x00002
#define UART_SIM_CFG_SIM_SEL			0x00001

#define UART_MISR_MODE_MODE_MASK		0x0003
#define UART_MISR_MODE_MODE_DISABLED		0x0000
#define UART_MISR_MODE_MODE_ENABLED_TX_TEST	0x0001
#define UART_MISR_MODE_MODE_ENABLED_RX_TEST	0x0002

#define UART_MISR_RESET_RESET			0x0001

#define UART_MISR_EXPORT_EXPORT			0x0001

#define UART_MISR_VAL_MASK			0x03ff

#define UART_MISR_TEST_CTRL_TEST_EN		0x0010
#define UART_MISR_TEST_CTRL_TEST_SEL_MASK	0x000f

#endif /* !JOS_DEV_MSM_TTYCONSREG */
