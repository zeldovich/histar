#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/timer.h>
#include <dev/msm_cpufreq.h>
#include <dev/msm_proc_comm.h>

#define PLL_TCX0 -1 
#define PLL_0	 0
#define PLL_1	 1
#define PLL_2	 2
#define PLL_3	 3

// from Linux
static struct {
	uint32_t freq;
	uint32_t pll;
	uint32_t sel;
	uint32_t div;
} cpufreqs[] = {
        { 19200, PLL_TCX0, 0, 0 },
        { 61440, PLL_0,  4, 3 },
        { 81920, PLL_0,  4, 2 },
        { 96000, PLL_1,  1, 7 },
        { 122880, PLL_0, 4, 1 },
        { 128000, PLL_1, 1, 5 },
        { 176000, PLL_2, 2, 5 },
        { 192000, PLL_1, 1, 3 },
        { 245760, PLL_0, 4, 0 },
        { 256000, PLL_1, 1, 2 },
        { 264000, PLL_2, 2, 3 },
        { 352000, PLL_2, 2, 2 },
        { 384000, PLL_1, 1, 1 },
        { 528000, PLL_2, 2, 1 },
        { 0, 0, 0, 0 }
};

static volatile uint32_t *clk_cntl;
static volatile uint32_t *clk_sel;
static volatile uint32_t *vdd_svs;

// 19.2mhz 
void
msm_cpufreq_slow()
{
	uint32_t tmp;

	// set up src0
	tmp = *clk_cntl;
	tmp &= ~(0x7f << 8);
	tmp |= ((0 << 12) | (0 << 8));	// useless, but sel 0, div 0
	*clk_cntl = tmp;

	// wait states
	*clk_cntl |= (100 << 16);	

	// switch to src0
	*clk_sel &= ~1;

	// set ahb divisor
	tmp = *clk_sel;
	tmp &= ~0x6;
	tmp |= (0 << 1);		// useless, but div by 1
	*clk_sel = tmp;

	// drop voltage
	*vdd_svs = (1 << 7) | (0 << 3);

	timer_delay(1000);
}

// 528mhz
void
msm_cpufreq_fast()
{
	static int inited = 0;
	uint32_t tmp, id, on;

	if (!inited) {
		// enable needed pll
		id = PLL_2;
		on = 1;
		msm_proc_comm(PCOM_CLKCTL_RPC_PLL_REQUEST, &id, &on);
	}

	// bump voltage
	*vdd_svs = (1 << 7) | (7 << 3);

	// set ahb divisor
	tmp = *clk_sel;
	tmp &= ~0x6;	
	tmp |= (3 << 1);
	*clk_sel = tmp;

	// wait states
	*clk_cntl |= (100 << 16);	

	// set up src1
	tmp = *clk_cntl;
	tmp &= ~0x7f; 
	tmp |= ((2 << 4) | 1);
	*clk_cntl = tmp;

	// switch to src1	
	*clk_sel |= 1;

	if (!inited) {
		// disable unneeded default pll
		id = PLL_1;
		on = 0;
		msm_proc_comm(PCOM_CLKCTL_RPC_PLL_REQUEST, &id, &on);
	}

	timer_delay(1000);
	inited = 1;
}

static int
report_state()
{
	/* 'src' selects SRC0 or SRC1 */
	uint32_t src	  = (*clk_sel & 0x1);
	uint32_t ahb_div  = ((*clk_sel >> 1) & 0x3) + 1;
	uint32_t src0_sel = (*clk_cntl >> 12) & 0x7;
	uint32_t src0_div = (*clk_cntl >> 8) & 0xf;
	uint32_t src1_sel = (*clk_cntl >> 4) & 0x7;
	uint32_t src1_div = (*clk_cntl >> 0) & 0xf;
	uint32_t cur_vdd  = (*vdd_svs) & 0x7;

	uint32_t src_sel = (src == 0) ? src0_sel : src1_sel;
	uint32_t src_div = (src == 0) ? src0_div : src1_div;

	int khz = 0;
	int i;
	for (i = 0; cpufreqs[i].freq != 0; i++) {
		if (cpufreqs[i].sel == src_sel && cpufreqs[i].div == src_div) {
			khz = cpufreqs[i].freq;
			break;
		}
	}

	cprintf("MSM CPUFREQ: %u kHz\n", khz);
	cprintf("MSM CPUFREQ: AHB divisor %u, src %u, cur vdd %u\n",
	    ahb_div, src, cur_vdd);
	cprintf("MSM CPUFREQ: src0 sel %u, src0 div %u, "
	    "src1 sel %u, src1 div %u\n", src0_sel, src0_div, src1_sel,
	    src1_div);

	return khz;
}

void
msm_cpufreq_init(uint32_t base)
{
	clk_cntl = (void *)(base + A11S_CLK_CNTL);	
	clk_sel  = (void *)(base + A11S_CLK_SEL);	
	vdd_svs  = (void *)(base + VDD_SVS_PLEVEL);	

	if (report_state(clk_cntl, clk_sel, vdd_svs) != 528000) {
		cprintf("MSM CPUFREQ: Moving to 528000 kHz\n");
		msm_cpufreq_fast();
		report_state();
	}
}
