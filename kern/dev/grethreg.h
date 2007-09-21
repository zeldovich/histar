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
#define GRETH_CTRL_100		(1 << 7)

/* Status register */
#define GRETH_STAT_RX_ERR	(1 << 0)
#define GRETH_STAT_TX_ERR	(1 << 1)
#define GRETH_STAT_RX_INT	(1 << 2)
#define GRETH_STAT_TX_INT	(1 << 3)
#define GRETH_STAT_RX_AHBERR	(1 << 4)
#define GRETH_STAT_TX_AHBERR	(1 << 5)
#define GRETH_STAT_TS		(1 << 6)
#define GRETH_STAT_IA		(1 << 7)

/* MDIO control and status register */
#define GRETH_MDIO_WR		0x01
#define GRETH_MDIO_RD		0x02
#define GRETH_MDIO_BUSY		0x08
#define GRETH_MDIO_NVALID	0x10
#define GRETH_MDIO_PHYADDR_SHIFT 11
#define GRETH_MDIO_PHY_MASK	0x1F
#define GRETH_MDIO_REGADDR_SHIFT 6
#define GRETH_MDIO_REGADDR_MASK	0x1F
#define GRETH_MDIO_DATA_SHIFT	16
#define GRETH_MDIO_DATA_MASK	0xFFFF

/* Buffer status bits */
#define GRETH_BD_NUM		128
#define GRETH_BD_LEN		0x000007FF
#define GRETH_BD_EN		0x00000800
#define GRETH_BD_WR		0x00001000
#define GRETH_BD_IE		0x00002000

#define GRETH_TXBD_ERR_UE	0x00004000
#define GRETH_TXBD_ERR_AL	0x00008000
#define GRETH_TXBD_ERR_LC	0x00010000
#define GRETH_TXBD_STATUS	0x0001C000

#define GRETH_TXBD_MORE		0x00020000
#define GRETH_TXBD_IPCS		0x00040000
#define GRETH_TXBD_TCPCS	0x00080000
#define GRETH_TXBD_UDPCS	0x00100000

#define GRETH_RXBD_STATUS	0xFFFFC000

#define GRETH_RXBD_ERR_AE	0x00004000
#define GRETH_RXBD_ERR_FT	0x00008000
#define GRETH_RXBD_ERR_CRC	0x00010000
#define GRETH_RXBD_ERR_OE	0x00020000
#define GRETH_RXBD_ERR_LE	0x00040000
#define GRETH_RXBD_ERR		0x0007C000

#define GRETH_RXBD_IP_DEC	0x00080000
#define GRETH_RXBD_IP_CSERR	0x00100000
#define GRETH_RXBD_UDP_DEC	0x00200000
#define GRETH_RXBD_UDP_CSERR	0x00400000
#define GRETH_RXBD_TCP_DEC	0x00800000
#define GRETH_RXBD_TCP_CSERR	0x01000000

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
