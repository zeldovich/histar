/* 
 * Copyright (C) 2004 Konrad Eisele (eiselekd@web.de), Gaisler Research
 */

#ifndef JOS_MACHINE_LEON3_H
#define JOS_MACHINE_LEON3_H

/*
 *  The following defines the bits in the LEON UART Status Registers.
 */

#define LEON_REG_UART_STATUS_DR   0x00000001	/* Data Ready */
#define LEON_REG_UART_STATUS_TSE  0x00000002	/* TX Send Register Empty */
#define LEON_REG_UART_STATUS_THE  0x00000004	/* TX Hold Register Empty */
#define LEON_REG_UART_STATUS_BR   0x00000008	/* Break Error */
#define LEON_REG_UART_STATUS_OE   0x00000010	/* RX Overrun Error */
#define LEON_REG_UART_STATUS_PE   0x00000020	/* RX Parity Error */
#define LEON_REG_UART_STATUS_FE   0x00000040	/* RX Framing Error */
#define LEON_REG_UART_STATUS_ERR  0x00000078	/* Error Mask */

/*
 *  The following defines the bits in the LEON UART Ctrl Registers.
 */

#define LEON_REG_UART_CTRL_RE     0x00000001	/* Receiver enable */
#define LEON_REG_UART_CTRL_TE     0x00000002	/* Transmitter enable */
#define LEON_REG_UART_CTRL_RI     0x00000004	/* Receiver interrupt enable */
#define LEON_REG_UART_CTRL_TI     0x00000008	/* Transmitter interrupt enable */
#define LEON_REG_UART_CTRL_PS     0x00000010	/* Parity select */
#define LEON_REG_UART_CTRL_PE     0x00000020	/* Parity enable */
#define LEON_REG_UART_CTRL_FL     0x00000040	/* Flow control enable */
#define LEON_REG_UART_CTRL_LB     0x00000080	/* Loop Back enable */

#define LEON3_GPTIMER_EN 1
#define LEON3_GPTIMER_RL 2
#define LEON3_GPTIMER_LD 4
#define LEON3_GPTIMER_IRQEN 8
#define LEON3_GPTIMER_SEPIRQ 8

#define LEON23_REG_TIMER_CONTROL_EN    0x00000001  /* 1 = enable counting */
                                                   /* 0 = hold scalar and counter */
#define LEON23_REG_TIMER_CONTROL_RL    0x00000002  /* 1 = reload at 0 */
                                                   /* 0 = stop at 0 */
#define LEON23_REG_TIMER_CONTROL_LD    0x00000004  /* 1 = load counter */
                                                   /* 0 = no function */
#define LEON23_REG_TIMER_CONTROL_IQ    0x00000008  /* 1 = irq enable */
                                                   /* 0 = no function */

/*
 *  The following defines the bits in the LEON PS/2 Status Registers.
 */

#define LEON_REG_PS2_STATUS_DR   0x00000001 /* Data Ready */
#define LEON_REG_PS2_STATUS_PE   0x00000002 /* Parity error */
#define LEON_REG_PS2_STATUS_FE   0x00000004 /* Framing error */
#define LEON_REG_PS2_STATUS_KI   0x00000008 /* Keyboard inhibit */
#define LEON_REG_PS2_STATUS_RF   0x00000010 /* RX buffer full */
#define LEON_REG_PS2_STATUS_TF   0x00000020 /* TX buffer full */ 

/*
 *  The following defines the bits in the LEON PS/2 Ctrl Registers.
 */

#define LEON_REG_PS2_CTRL_RE     0x00000001 /* Receiver enable */
#define LEON_REG_PS2_CTRL_TE     0x00000002 /* Transmitter enable */
#define LEON_REG_PS2_CTRL_RI     0x00000004 /* Keyboard receive interrupt  */
#define LEON_REG_PS2_CTRL_TI     0x00000008 /* Keyboard transmit interrupt */

#define LEON3_IRQMPSTATUS_CPUNR     28
#define LEON3_IRQMPSTATUS_BROADCAST 27

#ifndef __ASSEMBLER__

typedef struct {
	volatile unsigned int ilevel;
	volatile unsigned int ipend;
	volatile unsigned int iforce;
	volatile unsigned int iclear;
	volatile unsigned int mpstatus;
	volatile unsigned int mpbroadcast;
	volatile unsigned int notused02;
	volatile unsigned int notused03;
	volatile unsigned int notused10;
	volatile unsigned int notused11;
	volatile unsigned int notused12;
	volatile unsigned int notused13;
	volatile unsigned int notused20;
	volatile unsigned int notused21;
	volatile unsigned int notused22;
	volatile unsigned int notused23;
	volatile unsigned int mask[16];
        volatile unsigned int force[16];
} LEON3_IrqCtrl_Regs_Map;

typedef struct {
	volatile unsigned int data;
	volatile unsigned int status;
	volatile unsigned int ctrl;
	volatile unsigned int scaler;
} LEON3_APBUART_Regs_Map;

typedef struct {
	volatile unsigned int val;
	volatile unsigned int rld;
	volatile unsigned int ctrl;
	volatile unsigned int unused;
} LEON3_GpTimerElem_Regs_Map;

typedef struct {
	volatile unsigned int scalar;
	volatile unsigned int scalar_reload;
	volatile unsigned int config;
	volatile unsigned int unused;
	volatile LEON3_GpTimerElem_Regs_Map e[8];
} LEON3_GpTimer_Regs_Map;

#define GPTIMER_CONFIG_IRQNT(a) (((a) >> 3) & 0x1f)
#define GPTIMER_CONFIG_ISSEP(a) ((a)&(1<<8))
#define GPTIMER_CONFIG_NTIMERS(a) ((a)&(0x7))
#define LEON3_GPTIMER_CTRL_PENDING 0x10
#define LEON3_GPTIMER_CONFIG_NRTIMERS(c) ((c)->config & 0x7)
#define LEON3_GPTIMER_CTRL_ISPENDING(r) (((r)&LEON3_GPTIMER_CTRL_PENDING)?1:0)

typedef void (*GPTIMER_CALLBACK)(void);
typedef struct _sparc_gptimer {
    LEON3_GpTimer_Regs_Map *inst; 
    unsigned int ctrl, reload, value, scalarreload;
    int irq, flags, idxinst, idx, enabled, connected, minscalar;
    int ticksPerSecond, stat;
    GPTIMER_CALLBACK callback; int arg;
} sparc_gptimer;

#define GPTIMER_INST_TIMER_MAX 8
typedef struct _sparc_gptimer_inst {
    LEON3_GpTimer_Regs_Map *base;
    unsigned int scalarreload;
    int count, baseirq,  free, config, connected, minscalar;
    sparc_gptimer timers[GPTIMER_INST_TIMER_MAX];
} sparc_gptimer_inst;

extern volatile LEON3_IrqCtrl_Regs_Map *LEON3_IrqCtrl_Regs;
extern volatile LEON3_GpTimer_Regs_Map *LEON3_GpTimer_Regs;

typedef struct {
  volatile unsigned int iodata;
  volatile unsigned int ioout;
  volatile unsigned int iodir;
  volatile unsigned int irqmask;
  volatile unsigned int irqpol;
  volatile unsigned int irqedge;
} LEON3_IOPORT_Regs_Map;

typedef struct {
  volatile unsigned int write;
  volatile unsigned int dummy;
  volatile unsigned int txcolor;
  volatile unsigned int bgcolor;
} LEON3_VGA_Regs_Map;
extern volatile LEON3_VGA_Regs_Map *leon_apbvga;

typedef struct {
  volatile unsigned int status; 				/* 0x00 */
  volatile unsigned int video_length; 	/* 0x04 */
  volatile unsigned int front_porch;		/* 0x08 */
  volatile unsigned int sync_length;		/* 0x0c */
	volatile unsigned int line_length;		/* 0x10 */
	volatile unsigned int fb_pos;					/* 0x14 */
	volatile unsigned int clk_vector[4];	/* 0x18 */
	volatile unsigned int clut;						/* 0x28 */
} LEON3_GRVGA_Regs_Map;

typedef struct {
  volatile unsigned int data;
  volatile unsigned int status;
  volatile unsigned int ctrl;
} LEON3_APBPS2_REGS_Map;
extern volatile LEON3_APBPS2_REGS_Map *leon_apbps2;

typedef struct {
	volatile unsigned int cfg_stat;
	volatile unsigned int bar0;
	volatile unsigned int page0;
	volatile unsigned int bar1;
	volatile unsigned int page1;
	volatile unsigned int iomap;
	volatile unsigned int stat_cmd;
} LEON3_GRPCI_Regs_Map;

/*
 *  Types and structure used for AMBA Plug & Play bus scanning 
 */

typedef struct amba_device_table {
	int devnr;		/* numbrer of devices on AHB or APB bus */
	unsigned int *addr[16];	/* addresses to the devices configuration tables */
	unsigned int allocbits[1];	/* 0=unallocated, 1=allocated driver */
} amba_device_table;

typedef struct amba_confarea_type {
	amba_device_table ahbmst;
	amba_device_table ahbslv;
	amba_device_table apbslv;
	unsigned int apbmst;
} amba_confarea_type;


extern unsigned long amba_find_apbslv_addr(unsigned long vendor,
					   unsigned long device,
					   unsigned long *irq);

// collect apb slaves
typedef struct amba_apb_device {
	unsigned int start, irq, bus_id;
} amba_apb_device;
extern int amba_get_number_apbslv_devices(int vendor, int device); 
extern int amba_get_free_apbslv_devices(int vendor, int device,
					amba_apb_device * dev, int nr);
extern void amba_free_apbslv_device(unsigned int bus_id);
extern int amba_find_next_apbslv_devices (int vendor, int device, amba_apb_device * dev, int nr);

// collect ahb slaves
typedef struct amba_ahb_device {
	unsigned int start[4], irq;
} amba_ahb_device;
extern int amba_get_number_ahbslv_devices(int vendor, int device);
extern int amba_get_free_ahbslv_devices(int vendor, int device,
					amba_ahb_device * dev, int nr);
extern void amba_free_ahbslv_device(unsigned int bus_id);

extern void amba_prinf_config(void);
extern void amba_init(void);
extern int amba_is_init;

extern void vendor_dev_string(unsigned long conf, char *vendorbuf, char *devbuf);

#define ASI_LEON3_SYSCTRL		0x02
#define ASI_LEON3_SYSCTRL_ICFG		0x08
#define ASI_LEON3_SYSCTRL_DCFG		0x0c
#define ASI_LEON3_SYSCTRL_CFG_SNOOPING (1<<27)
#define ASI_LEON3_SYSCTRL_CFG_SSIZE(c) (1<<((c>>20)&0xf))

extern __inline__ unsigned long sparc_leon3_get_dcachecfg(void) {
	unsigned int retval;
	__asm__ __volatile__("lda [%1] %2, %0\n\t" :
			     "=r" (retval) :
			     "r" (ASI_LEON3_SYSCTRL_DCFG),
			     "i" (ASI_LEON3_SYSCTRL));
	return (retval);
}

/*enable snooping*/
extern __inline__ void sparc_leon3_enable_snooping(void) {
  __asm__ volatile ("lda [%%g0] 2, %%l1\n\t"  \
                    "set 0x800000, %%l2\n\t"  \
                    "or  %%l2, %%l1, %%l2\n\t" \
                    "sta %%l2, [%%g0] 2\n\t"  \
                    : : : "l1", "l2");	
};

extern __inline__ void sparc_leon3_disable_cache(void) {
  __asm__ volatile ("lda [%%g0] 2, %%l1\n\t"  \
                    "set 0x00000f, %%l2\n\t"  \
                    "andn  %%l2, %%l1, %%l2\n\t" \
                    "sta %%l2, [%%g0] 2\n\t"  \
                    : : : "l1", "l2");	
};


#endif

#endif
