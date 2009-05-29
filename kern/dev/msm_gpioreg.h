#ifndef JOS_DEV_MSM_GPIOREG
#define JOS_DEV_MSM_GPIOREG

/* MSM7201 register offsets */

#define GPIO1_OUT_0		0x00
#define GPIO2_OUT_1		0x00
#define GPIO1_OUT_2		0x04
#define GPIO1_OUT_3		0x08
#define GPIO1_OUT_4		0x0c
#define GPIO1_OUT_5		0x50

#define GPIO1_OE_0		0x10
#define GPIO2_OE_1		0x08
#define GPIO1_OE_2		0x14
#define GPIO1_OE_3		0x18
#define GPIO1_OE_4		0x1c
#define GPIO1_OE_5		0x54

#define GPIO1_IN_0		0x34
#define GPIO2_IN_1		0x20
#define GPIO1_IN_2		0x38
#define GPIO1_IN_3		0x3c
#define GPIO1_IN_4		0x40
#define GPIO1_IN_5		0x44

// 0 => level-triggered,  1 => edge-triggered
#define GPIO1_INT_DETECT_CTL_0	0x60
#define GPIO2_INT_DETECT_CTL_1	0x50
#define GPIO1_INT_DETECT_CTL_2	0x64
#define GPIO1_INT_DETECT_CTL_3	0x68
#define GPIO1_INT_DETECT_CTL_4	0x6c
#define GPIO1_INT_DETECT_CTL_5	0x0

// 0 => negative polarity, 1 => positive polarity 
#define GPIO1_INT_POLARITY_0	0x70
#define GPIO2_INT_POLARITY_1	0x58
#define GPIO1_INT_POLARITY_2	0x74
#define GPIO1_INT_POLARITY_3	0x78
#define GPIO1_INT_POLARITY_4	0x7c
#define GPIO1_INT_POLARITY_5	0xbc

// the following 3 int register types behave like msm_irq.c
#define GPIO1_INT_EN_0		0x80
#define GPIO2_INT_EN_1		0x60
#define GPIO1_INT_EN_2		0x84
#define GPIO1_INT_EN_3		0x88
#define GPIO1_INT_EN_4		0x8c
#define GPIO1_INT_EN_5		0xb8

#define GPIO1_INT_CLEAR_0	0xa0
#define GPIO2_INT_CLEAR_1	0x70
#define GPIO1_INT_CLEAR_2	0xa4
#define GPIO1_INT_CLEAR_3	0xa8
#define GPIO1_INT_CLEAR_4	0xac
#define GPIO1_INT_CLEAR_5	0xb0

#define GPIO1_INT_STATUS_0	0xa0
#define GPIO2_INT_STATUS_1	0x70
#define GPIO1_INT_STATUS_2	0xa4
#define GPIO1_INT_STATUS_3	0xa8
#define GPIO1_INT_STATUS_4	0xac
#define GPIO1_INT_STATUS_5	0xb0

#endif /* !JOS_DEV_MSM_GPIOREG */
