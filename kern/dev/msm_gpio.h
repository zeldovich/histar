#ifndef JOS_DEV_MSM_GPIO
#define JOS_DEV_MSM_GPIO

typedef unsigned int gpio_pin_t;

enum _gpio_direction_t {
	GPIO_DIRECTION_IN,
	GPIO_DIRECTION_OUT
};
typedef enum _gpio_direction_t gpio_direction_t;

enum _gpio_polarity_t {
	GPIO_POLARITY_NEGATIVE,
	GPIO_POLARITY_POSITIVE
};
typedef enum _gpio_polarity_t gpio_polarity_t;

enum _gpio_interrupt_t {
	GPIO_INTERRUPT_LEVEL_TRIGGERED,
	GPIO_INTERRUPT_EDGE_TRIGGERED
};
typedef enum _gpio_interrupt_t gpio_interrupt_t;

void msm_gpio_set_direction(gpio_pin_t, gpio_direction_t);
void msm_gpio_set_interrupt_trigger(gpio_pin_t, gpio_interrupt_t);
void msm_gpio_set_interrupt_polarity(gpio_pin_t, gpio_polarity_t);
void msm_gpio_enable_interrupt(gpio_pin_t);
void msm_gpio_disable_interrupt(gpio_pin_t);
void msm_gpio_clear_interrupt(gpio_pin_t);
int  msm_gpio_get_interrupt_status(gpio_pin_t);
void msm_gpio_write(gpio_pin_t, int);
int  msm_gpio_read(gpio_pin_t);
void msm_gpio_init(uint32_t, uint32_t);
void msm_gpio_irq_enable(uint32_t);
void msm_gpio_irq_handler(void);

#endif /* !JOS_DEV_MSM_GPIO */
