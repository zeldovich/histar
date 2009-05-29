#ifndef JOS_DEV_MSM_GPIO
#define JOS_DEV_MSM_GPIO

enum gpio_direction_t {
	GPIO_DIRECTION_IN,
	GPIO_DIRECTION_OUT
};

enum gpio_polarity_t {
	GPIO_POLARITY_NEGATIVE,
	GPIO_POLARITY_POSITIVE
};

enum gpio_interrupt_t {
	GPIO_INTERRUPT_LEVEL_TRIGGERED,
	GPIO_INTERRUPT_EDGE_TRIGGERED
};

void msm_gpio_set_direction(int, gpio_direction_t);
void msm_gpio_set_interrupt_trigger(int, gpio_interrupt_t);
void msm_gpio_set_interrupt_polarity(int, gpio_polarity_t);
void msm_gpio_enable_interrupt(int);
void msm_gpio_disable_interrupt(int);
void msm_gpio_clear_interrupt(int);
void msm_gpio_get_interrupt_status(int);
void msm_gpio_write(int, int);
int  msm_gpio_read(int);

#endif /* !JOS_DEV_MSM_GPIO */
