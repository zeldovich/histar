#ifndef JOS_DEV_MSM_IRQ
#define JOS_DEV_MSM_IRQ

#define MSM_NIRQS       64	/* >= MSM_NIRQS implies a gpio interrupt */

void msm_irq_init(uint32_t);
void irq_arch_handle(void);

#endif /* !JOS_DEV_MSM_IRQ */
