#ifndef JOS_DEV_MSM_TIMERREG
#define JOS_DEV_MSM_TIMERREG

/*
 * MSM has two timers/counters: a General Purpose `GP' timer that ticks at
 * 32768Hz and a Debug `DG' timer that ticks at 19.2MHz. Note, however, that
 * the DG timer resolution is limited to the upper 27 bits, meaning we
 * essentially have 600KHz of resolution.
 *
 * NB: On the 7200A, at least, the DG timer appears to interrupt the cpu
 *     directly (i.e. _not_ via the VIC interrupt controller).
 */

struct msm_timer_reg {
	struct {
		volatile uint32_t match_val;	/* (rw) value @ `count' reset */
		volatile uint32_t count_val;	/* (r) current val; see above */
		volatile uint32_t enable;	/* (rw) control register */
		volatile uint32_t clear;	/* (w) write any to reset cnt */
	} timer[2];
};

#define MSM_TIMER_GP_HZ_SHIFT		0
#define MSM_TIMER_DG_HZ_SHIFT		5

#define ENABLE_CLR_ON_MATCH_EN		0x02	/* 1 => reset on match_val */ 
#define ENABLE_EN			0x01	/* 1 => count! */

#endif /* !JOS_DEV_MSM_TIMERREG */
