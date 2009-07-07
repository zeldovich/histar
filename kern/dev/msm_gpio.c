#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <dev/msm_irq.h>	// for MSM_NIRQS
#include <dev/msm_gpio.h>
#include <dev/msm_gpioreg.h>

#define MSM_GPIO_INT_STATUS_WAR

typedef volatile uint32_t gpio_reg_t;

enum _gpio_type_t {
	GPIO_OUT,
	GPIO_OE,
	GPIO_IN,
	GPIO_INT_DETECT_CTL,
	GPIO_INT_POLARITY,
	GPIO_INT_EN,
	GPIO_INT_CLEAR,
	GPIO_INT_STATUS
};
typedef enum _gpio_type_t gpio_type_t;

enum _gpio_group_t {
	GPIO1,
	GPIO2
};
typedef enum _gpio_group_t gpio_group_t;

static struct {
	uint32_t	first_pin;
	uint32_t	last_pin;
	gpio_group_t	gpio_group;
	gpio_reg_t	offsets[8];
} gpio_pins[] = {
	{ 0, 15, GPIO1,
	  { GPIO1_OUT_0,
	    GPIO1_OE_0,
	    GPIO1_IN_0,
	    GPIO1_INT_DETECT_CTL_0,
	    GPIO1_INT_POLARITY_0,
	    GPIO1_INT_EN_0,
	    GPIO1_INT_CLEAR_0,
	    GPIO1_INT_STATUS_0
	  }
	},

	{ 16, 42, GPIO2,
	  { GPIO2_OUT_1,
	    GPIO2_OE_1,
	    GPIO2_IN_1,
	    GPIO2_INT_DETECT_CTL_1,
	    GPIO2_INT_POLARITY_1,
	    GPIO2_INT_EN_1,
	    GPIO2_INT_CLEAR_1,
	    GPIO2_INT_STATUS_1
	  }
	},

	{ 43, 67, GPIO1,
	  { GPIO1_OUT_2,
	    GPIO1_OE_2,
	    GPIO1_IN_2,
	    GPIO1_INT_DETECT_CTL_2,
	    GPIO1_INT_POLARITY_2,
	    GPIO1_INT_EN_2,
	    GPIO1_INT_CLEAR_2,
	    GPIO1_INT_STATUS_2
	  }
	},

	{ 68, 94, GPIO1,
	  { GPIO1_OUT_3,
	    GPIO1_OE_3,
	    GPIO1_IN_3,
	    GPIO1_INT_DETECT_CTL_3,
	    GPIO1_INT_POLARITY_3,
	    GPIO1_INT_EN_3,
	    GPIO1_INT_CLEAR_3,
	    GPIO1_INT_STATUS_3
	  }
	},

	{ 95, 106, GPIO1,
	  { GPIO1_OUT_4,
	    GPIO1_OE_4,
	    GPIO1_IN_4,
	    GPIO1_INT_DETECT_CTL_4,
	    GPIO1_INT_POLARITY_4,
	    GPIO1_INT_EN_4,
	    GPIO1_INT_CLEAR_4,
	    GPIO1_INT_STATUS_4
	  }
	},

	{ 107, 121, GPIO1,
	  { GPIO1_OUT_5,
	    GPIO1_OE_5,
	    GPIO1_IN_5,
	    GPIO1_INT_DETECT_CTL_5,
	    GPIO1_INT_POLARITY_5,
	    GPIO1_INT_EN_5,
	    GPIO1_INT_CLEAR_5,
	    GPIO1_INT_STATUS_5
	  }
	}
};

#define NGPIO_SETS ((int)(sizeof(gpio_pins) / sizeof(gpio_pins[0])))

// register offsets, set in msm_gpio_init
static uint32_t gpio1_base;
static uint32_t gpio2_base;

// workaround for broken status clear
#ifdef MSM_GPIO_INT_STATUS_WAR
static uint32_t msm_gpio_int_status_war[NGPIO_SETS];
#endif

static unsigned int
gpio_lookup_idx(gpio_pin_t pin)
{
	static int lastidx = 0;
	int i;

	assert(pin <= 121);

	for (i = -1; i < NGPIO_SETS; i++) {
		int idx = (i == -1) ? lastidx : i;

		if (pin >= gpio_pins[idx].first_pin &&
		    pin <= gpio_pins[idx].last_pin) {
			lastidx = idx;
			return (idx);
		}
	}

	panic("%s:%s bad gpio pin %u", __FILE__, __func__, pin);
} 

static gpio_reg_t *
gpio_lookup(gpio_pin_t pin, int type, int *bitno)
{
	int idx = gpio_lookup_idx(pin);
	gpio_reg_t addr;

	assert(type >= GPIO_OUT && type <= GPIO_INT_STATUS);

	addr = gpio_pins[idx].offsets[type];
	if (gpio_pins[idx].gpio_group == GPIO1)
		addr += gpio1_base;
	else
		addr += gpio2_base;

	if (bitno != NULL)
		*bitno = pin - gpio_pins[idx].first_pin;
	return ((gpio_reg_t *)addr);
}

static inline int
gpio_read(gpio_type_t type, gpio_pin_t pin)
{
	int bit;

	gpio_reg_t *reg = gpio_lookup(pin, type, &bit);
	return (!!(*reg & (1 << bit)));
}

static inline void
gpio_write(gpio_type_t type, gpio_pin_t pin, int on)
{
	int bit;
	gpio_reg_t *reg = gpio_lookup(pin, type, &bit);

	if (on)
		*reg |=  (1 << bit);
	else
		*reg &= ~(1 << bit);
}

void
msm_gpio_set_direction(gpio_pin_t pin, gpio_direction_t type)
{
	if (type == GPIO_DIRECTION_IN)
		gpio_write(GPIO_OE, pin, 0);
	else
		gpio_write(GPIO_OE, pin, 1);
}

void
msm_gpio_set_interrupt_trigger(gpio_pin_t pin, gpio_interrupt_t type)
{
	if (type == GPIO_INTERRUPT_LEVEL_TRIGGERED)
		gpio_write(GPIO_INT_DETECT_CTL, pin, 0);
	else
		gpio_write(GPIO_INT_DETECT_CTL, pin, 1);
}

void
msm_gpio_set_interrupt_polarity(gpio_pin_t pin, gpio_polarity_t type)
{
	if (type == GPIO_POLARITY_NEGATIVE)
		gpio_write(GPIO_INT_POLARITY, pin, 0);
	else
		gpio_write(GPIO_INT_POLARITY, pin, 1);
}

void
msm_gpio_enable_interrupt(gpio_pin_t pin)
{
	gpio_write(GPIO_INT_EN, pin, 1);
}

void
msm_gpio_disable_interrupt(gpio_pin_t pin)
{
	gpio_write(GPIO_INT_EN, pin, 0);
}

void
msm_gpio_clear_interrupt(gpio_pin_t pin)
{
	int bit;
	int idx = gpio_lookup_idx(pin);
	gpio_reg_t *reg = gpio_lookup(pin, GPIO_INT_STATUS, &bit);

#ifdef MSM_GPIO_INT_STATUS_WAR
	// Clearing, according to Linux, is broken. We need to save the
	// others, else we may lose them. Apparently this doesn't always
	// work (interrupts occuring between the status read and clear
	// write will be lost), but Android doesn't seem to care.
	msm_gpio_int_status_war[idx] |= *reg;
	msm_gpio_int_status_war[idx] &= (1 << bit);
#endif

	gpio_write(GPIO_INT_CLEAR, pin, 1);
}

int
msm_gpio_get_interrupt_status(gpio_pin_t pin)
{
#ifdef MSM_GPIO_INT_STATUS_WAR
	int bit;
	int idx = gpio_lookup_idx(pin);
	gpio_lookup(pin, GPIO_INT_STATUS, &bit);
	if (msm_gpio_int_status_war[idx] & (1 << bit))
		return (1);
#endif
	return (gpio_read(GPIO_INT_STATUS, pin));
}

void
msm_gpio_write(gpio_pin_t pin, int on)
{
	gpio_write(GPIO_OUT, pin, on);
}

int
msm_gpio_read(gpio_pin_t pin)
{
	return (gpio_read(GPIO_IN, pin));
}

void
msm_gpio_init(uint32_t g1base, uint32_t g2base)
{
	gpio_pin_t pin;

	gpio1_base = g1base;
	gpio2_base = g2base;

	for (pin = 0; pin <= gpio_pins[NGPIO_SETS - 1].last_pin; pin++)
		msm_gpio_disable_interrupt(pin);

	cprintf("MSM GPIOs @ 0x%08x (GPIO1) and 0x%08x (GPIO2)\n",
	    gpio1_base, gpio2_base);
}

void
msm_gpio_irq_enable(uint32_t irq)
{
	gpio_pin_t pin;

	assert(irq >= MSM_NIRQS);
	assert(irq < (MSM_NIRQS + gpio_pins[NGPIO_SETS - 1].last_pin));

	pin = irq - MSM_NIRQS;
	msm_gpio_enable_interrupt(pin);
}

/* called explicitly from msm_irq.c whenever an interrupt exception occurs */
void
msm_gpio_irq_handler()
{
	gpio_pin_t pin;

	for (pin = 0; pin <= gpio_pins[NGPIO_SETS - 1].last_pin; pin++) {
		if (msm_gpio_get_interrupt_status(pin) &&
		    gpio_read(GPIO_INT_EN, pin)) {
			irq_handler(pin + MSM_NIRQS);
			msm_gpio_clear_interrupt(pin);
		}
	}
}
