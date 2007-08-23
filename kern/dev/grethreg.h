#ifndef JOS_DEV_GRETHREG_H
#define JOS_DEV_GRETHREG_H

/* Control register */
#define GRETH_CTRL_TX_EN	(1 << 0)
#define GRETH_CTRL_RX_EN	(1 << 1)
#define GRETH_CTRL_TX_INT	(1 << 2)
#define GRETH_CTRL_RX_INT	(1 << 3)
#define GRETH_CTRL_FD		(1 << 4)
#define GRETH_CTRL_PROMISC	(1 << 5)
#define GRETH_CTRL_RESET	(1 << 6)

/* Status register */
#define GRETH_STAT_RX_ERR	(1 << 0)
#define GRETH_STAT_TX_ERR	(1 << 1)
#define GRETH_STAT_RX_INT	(1 << 2)
#define GRETH_STAT_TX_INT	(1 << 3)
#define GRETH_STAT_RX_AHBERR	(1 << 4)
#define GRETH_STAT_TX_AHBERR	(1 << 5)
#define GRETH_STAT_TS		(1 << 6)
#define GRETH_STAT_IA		(1 << 7)

#define GRETH_BD_EN 0x800
#define GRETH_BD_WR 0x1000
#define GRETH_BD_IE 0x2000
#define GRETH_BD_LEN 0x7FF

#define GRETH_TXBD_STATUS 0x0001C000
#define GRETH_TXBD_MORE 0x20000
#define GRETH_TXBD_IPCS 0x40000
#define GRETH_TXBD_TCPCS 0x80000
#define GRETH_TXBD_UDPCS 0x100000
#define GRETH_TXBD_ERR_LC 0x10000
#define GRETH_TXBD_ERR_UE 0x4000
#define GRETH_TXBD_ERR_AL 0x8000
#define GRETH_TXBD_NUM 128
#define GRETH_TXBD_NUM_MASK (GRETH_TXBD_NUM-1)
#define GRETH_TX_BUF_SIZE 2048

#define GRETH_RXBD_STATUS    0xFFFFC000

#define GRETH_RXBD_ERR_AE    0x4000
#define GRETH_RXBD_ERR_FT    0x8000
#define GRETH_RXBD_ERR_CRC   0x10000
#define GRETH_RXBD_ERR_OE    0x20000
#define GRETH_RXBD_ERR_LE    0x40000
#define GRETH_RXBD_ERR       0x7C000

#define GRETH_RXBD_IP_DEC    0x80000
#define GRETH_RXBD_IP_CSERR  0x100000
#define GRETH_RXBD_UDP_DEC   0x200000
#define GRETH_RXBD_UDP_CSERR 0x400000
#define GRETH_RXBD_TCP_DEC   0x800000
#define GRETH_RXBD_TCP_CSERR 0x1000000

#define GRETH_RXBD_NUM 128
#define GRETH_RXBD_NUM_MASK (GRETH_RXBD_NUM-1)
#define GRETH_RX_BUF_SIZE 2048

/* Ethernet configuration registers */
struct greth_regs {
    volatile uint32_t control;
    volatile uint32_t status;
    volatile uint32_t esa_msb;
    volatile uint32_t esa_lsb;
    volatile uint32_t mdio;
    volatile uint32_t tx_desc_p;
    volatile uint32_t rx_desc_p;
    volatile uint32_t edcl_ip;
};

/* Ethernet buffer descriptor */
struct greth_bd {
    volatile uint32_t stat;
    volatile uint32_t addr;           /* Buffer address */
};

#endif
