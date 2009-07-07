#ifndef JOS_DEV_MSM_GPIO
#define JOS_DEV_MSM_GPIO

#include <dev/gpio.h>

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
