#ifndef JOS_DEV_HTCDREAM_GPIO
#define JOS_DEV_HTCDREAM_GPIO

#include <dev/gpio.h>

void htcdream_gpio_set_direction(gpio_pin_t, gpio_direction_t);
void htcdream_gpio_set_interrupt_trigger(gpio_pin_t, gpio_interrupt_t);
void htcdream_gpio_set_interrupt_polarity(gpio_pin_t, gpio_polarity_t);
void htcdream_gpio_enable_interrupt(gpio_pin_t);
void htcdream_gpio_disable_interrupt(gpio_pin_t);
void htcdream_gpio_clear_interrupt(gpio_pin_t);
int  htcdream_gpio_get_interrupt_status(gpio_pin_t);
void htcdream_gpio_write(gpio_pin_t, int);
int  htcdream_gpio_read(gpio_pin_t);
void htcdream_gpio_init(uint32_t);
void htcdream_gpio_irq_enable(uint32_t);
void htcdream_gpio_irq_handler(void);

#endif /* !JOS_DEV_HTCDREAM_GPIO */
