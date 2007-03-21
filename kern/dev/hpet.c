#include <machine/pmap.h>
#include <dev/hpet.h>
#include <dev/hpetreg.h>

struct hpet_state {
    struct hpet_reg *reg;
    uint32_t freq_hz;
    uint16_t min_tick;
};

static struct hpet_state the_hpet;

void
hpet_attach(struct acpi_table_hdr *th)
{
    struct hpet_state *hpet = &the_hpet;
    struct hpet_acpi *t = (struct hpet_acpi *) th;
    hpet->reg = pa2kva(t->base.addr);

    uint64_t period = hpet->reg->cap >> 32;
    hpet->freq_hz = UINT64(1000000000000000) / period;
    hpet->min_tick = t->min_tick;
    cprintf("HPET: %d Hz, min tick %d\n", hpet->freq_hz, hpet->min_tick);

    hpet->reg->config = 1;
    cprintf("HPET: currently at %"PRIu64"\n", hpet->reg->counter);
}
