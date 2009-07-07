#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <dev/htcdream_gpio.h>
//#include <dev/htcdream_gpioreg.h>

typedef volatile uint8_t gpio_reg_t;

// register offset, set in htcdream_gpio_init
static uint32_t gpio_base;

static gpio_reg_t shadow[4] = { 0x80, 0x04, 0x00, 0x04 };

void
htcdream_gpio_set_direction(gpio_pin_t pin, gpio_direction_t type)
{
	panic("%s: unimplemented", __func__);
}

void
htcdream_gpio_set_interrupt_trigger(gpio_pin_t pin, gpio_interrupt_t type)
{
	panic("%s: unimplemented", __func__);
}

void
htcdream_gpio_set_interrupt_polarity(gpio_pin_t pin, gpio_polarity_t type)
{
	panic("%s: unimplemented", __func__);
}

void
htcdream_gpio_enable_interrupt(gpio_pin_t pin)
{
	panic("%s: unimplemented", __func__);
}

void
htcdream_gpio_disable_interrupt(gpio_pin_t pin)
{
	panic("%s: unimplemented", __func__);
}

void
htcdream_gpio_clear_interrupt(gpio_pin_t pin)
{
	panic("%s: unimplemented", __func__);
}

int
htcdream_gpio_get_interrupt_status(gpio_pin_t pin)
{
	panic("%s: unimplemented", __func__);
}

void
htcdream_gpio_write(gpio_pin_t pin, int on)
{
	int regnum = (pin / 8);
	int regoff = regnum * 2;
	int bit = 1U << (pin % 8);
	gpio_reg_t *reg = (gpio_reg_t *)(gpio_base + regoff);

	if (on)
		shadow[regnum] |= bit;
	else
		shadow[regnum] &= ~bit;

	*reg = shadow[regnum];
}

int
htcdream_gpio_read(gpio_pin_t pin)
{
	int regnum = (pin / 8);
	int regoff = regnum * 2;
	int bit = 1U << (pin % 8);
	gpio_reg_t *reg = (gpio_reg_t *)(gpio_base + regoff);

	return (!!(*reg & bit));
}

void
htcdream_gpio_init(uint32_t base)
{
	int i;

	gpio_base = base;

	cprintf("HTC DREAM GPIOs @ 0x%08x", gpio_base);

	for (i = 0; i < 4; i++) {
		gpio_reg_t *reg = (gpio_reg_t *)(gpio_base + (i * 2));
		*reg = shadow[i];
	}

	cprintf("; initialized\n");
}

void
htcdream_gpio_irq_enable(uint32_t irq)
{
	panic("%s: unimplemented", __func__);
}

/* called explicitly from msm_irq.c whenever an interrupt exception occurs */
void
htcdream_gpio_irq_handler()
{
	panic("%s: unimplemented", __func__);
}
