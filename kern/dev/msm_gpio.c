#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <dev/msm_gpio.h>
#include <dev/msm_gpio_reg.h>

#define MSM_GPIO_INT_STATUS_WAR

typedef volatile uint32_t gpio_reg_t;

enum gpio_type_t {
	GPIO_OUT,
	GPIO_OE,
	GPIO_IN,
	GPIO_INT_DETECT_CTL,
	GPIO_POLARITY,
	GPIO_EN,
	GPIO_CLEAR,
	GPIO_STATUS
};

enum gpio_group_t {
	GPIO1,
	GPIO2
};

static struct {
	uint32_t	first_pin;
	uint32_t	last_pin;
	gpio_group_t	group;
	gpio_reg_t	offsets[8];
} gpio_pins = {
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
	},
};

#define NGPIO_SETS (sizeof(gpio_pins) / sizeof(gpio_pins[0]))

// register offsets, set in msm_gpio_init
static gpio_reg_t gpio1_base;
static gpio_reg_t gpio2_base;

// workaround for broken status clear
#ifdef MSM_GPIO_INT_STATUS_WAR
static uint32_t msm_gpio_int_status_war;
#endif

static gpio_reg_t *
gpio_lookup(int pin, int type, int *bitno)
{
	static int lastidx = 0;
	int i;

	assert(pin >= 0 && pin <= 121);
	assert(type >= GPIO_OUT && type <= GPIO_STATUS);

	for (i = -1; i < NGPIO_SETS; i++) {
		int idx = (i == -1) ? lastidx : i;

		if (pin >= gpio_pins[idx].first_pin &&
		    pin <= gpio_pins[idx].last_pin) {
			gpio_reg_t addr = gpio_pins[idx].offsets[type];

			if (gpio_pins[idx].gpio_group == GPIO1)
				addr += gpio1_base;
			else
				addr += gpio2_base;

			if (bitno != NULL)
				*bitno = pin - gpio_pins[idx].first_pin;
			lastidx = idx;
			return ((gpio_reg_t *)ptr);
		}
	}

	assert(0);
	return (0);
}

static inline int
gpio_read(gpio_type_t type, int pin)
{
	int bit;

	gpio_reg_t *reg = gpio_lookup(pin, type, &bit);
	return (!!(*reg & (1 << bit)));
}

static inline void
gpio_write(gpio_type_t type, int pin, int on)
{
	int bit;
	gpio_reg_t *reg = gpio_lookup(pin, type, &bit);

	if (on)
		*reg |=  (1 << bit);
	else
		*reg &= ~(1 << bit);
}

void
msm_gpio_set_direction(int pin, gpio_direction_t type)
{
	if (type == GPIO_DIRECTION_IN)
		gpio_write(GPIO_OE, pin, 0);
	else
		gpio_write(GPIO_OE, pin, 1);
}

void
msm_gpio_set_interrupt_trigger(int pin, gpio_interrupt_t type)
{
	if (type == GPIO_INTERRUPT_LEVEL_TRIGGERED)
		gpio_write(GPIO_INT_DETECT_CTL, pin, 0);
	else
		gpio_write(GPIO_INT_DETECT_CTL, pin, 1);
}

void
msm_gpio_set_interrupt_polarity(int pin, gpio_polarity_t type)
{
	if (type == GPIO_POLARITY_NEGATIVE)
		gpio_write(GPIO_POLARITY, pin, 0);
	else
		gpio_write(GPIO_POLARITY, pin, 1);
}

void
msm_gpio_enable_interrupt(int pin)
{
	gpio_write(GPIO_INT_EN, pin, 1);
}

void
msm_gpio_disable_interrupt(int pin)
{
	gpio_write(GPIO_INT_EN, pin, 0);
}

void
msm_gpio_clear_interrupt(int pin)
{
	int bit;
	gpio_reg_t *reg = gpio_lookup(pin, GPIO_INT_STATUS, &bit);

#ifdef MSM_GPIO_INT_STATUS_WAR
	// Clearing, according to Linux, is broken. We need to save the
	// others, else we may lose them. Apparently this doesn't always
	// work (interrupts occuring between the status read and clear
	// write will be lost), but Android doesn't seem to care.
	gpio_int_status_war |= *reg;
	gpio_int_status_war &=  (1 << bit);
#endif

	gpio_write(GPIO_INT_CLEAR, pin, 1);
}

void
msm_gpio_get_interrupt_status(int pin)
{
#ifdef MSM_GPIO_INT_STATUS_WAR
	int bit;
	gpio_lookup(pin, GPIO_INT_STATUS, &bit);
	if (gpio_int_status_war & (1 << bit))
		return (1);
#endif
	return (gpio_read(GPIO_INT_STATUS, pin));
}

void
msm_gpio_write(int pin, int on)
{
	gpio_write(GPIO_OUT, pin, on);
}

int
msm_gpio_read(int pin)
{
	return (gpio_read(GPIO_IN, pin));
}

void
msm_gpio_init(uint32_t g1base, uint32_t g2base)
{
	gpio1_base = g1base;
	gpio2_base = g2base;
}
