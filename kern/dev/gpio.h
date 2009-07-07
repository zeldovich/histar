#ifndef JOS_DEV_GPIO
#define JOS_DEV_GPIO

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

#endif /* !JOS_DEV_GPIO */
