#ifndef JOS_DEV_MSM_TIMERREG
#define JOS_DEV_MSM_TIMERREG

struct msm_timer_reg {
	volatile uint32_t mtr_matchvalue;
	volatile uint32_t mtr_countvalue;
	volatile uint32_t mtr_enable;
	volatile uint32_t mtr_clear;
};

#endif /* !JOS_DEV_MSM_TIMERREG */
