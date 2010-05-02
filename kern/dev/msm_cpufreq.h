#ifndef JOS_DEV_MSM_CPUFREQ
#define JOS_DEV_MSM_CPUFREQ

#define A11S_CLK_CNTL	0x100
#define A11S_CLK_SEL	0x104
#define VDD_SVS_PLEVEL	0x124

void msm_cpufreq_fast(void);
void msm_cpufreq_slow(void);
void msm_cpufreq_init(uint32_t);

#endif /* !JOS_DEV_MSM_CPUFREQ */
