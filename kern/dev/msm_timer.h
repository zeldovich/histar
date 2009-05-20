#ifndef JOS_DEV_MSM_TIMER
#define JOS_DEV_MSM_TIMER

enum { MSM_TIMER_GP, MSM_TIMER_DG };

void msm_timer_init(uint32_t, int, int, uint64_t);

#endif /* !JOS_DEV_MSM_TIMER */
