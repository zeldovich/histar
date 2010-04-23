#ifndef JOS_MACHINE_CPUIDENTIFY_H
#define JOS_MACHINE_CPUIDENTIFY_H

#ifndef __ASSEMBLER__

/* cache configuration */
uint32_t arm_picache_ways;
uint32_t arm_picache_lines;
uint32_t arm_picache_line_size;
uint32_t arm_picache_size;
uint32_t arm_picache_pbit;
uint32_t arm_pdcache_ways;
uint32_t arm_pdcache_lines;
uint32_t arm_pdcache_line_size;
uint32_t arm_pdcache_size;
uint32_t arm_pdcache_pbit;
uint32_t arm_pcache_unified;

struct cpufunc {
	void (*cf_tlb_flush_entry)(void *);
	void (*cf_write_buffer_drain)(void);
	void (*cf_icache_invalidate)(void);
	void (*cf_dcache_flush_invalidate)(void);
	void (*cf_dcache_flush_invalidate_range)(void *, uint32_t);
	void (*cf_sleep)(void);
};
extern struct cpufunc cpufunc;

void cpu_identify(void);

#endif /* !__ASSEMBLER__ */

#endif /* !JOS_MACHINE_CPUIDENTIFY_H */
