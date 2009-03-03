#ifndef JOS_DEV_GOLDFISH_TIMERREG
#define JOS_DEV_GOLDFISH_TIMERREG

struct goldfish_timer_reg {
	volatile uint32_t timer_low;       /* low bits and update timer_high */
	volatile uint32_t timer_high;      /* high bits as of last timer_low */
	volatile uint32_t alarm_low;       /* low alarm bits and activate */
	volatile uint32_t alarm_high;      /* high alarm bits */
	volatile uint32_t interrupt_clear; /* write anything to clear */
	volatile uint32_t alarm_clear;     /* write anything to clear */
};

#endif /* !JOS_DEV_GOLDFISH_TIMERREG */
