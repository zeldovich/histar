/* arch/arm/mach-msm/proc_comm.c
 *
 * Copyright (C) 2007-2008 Google, Inc.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <kern/lib.h>
#include <kern/arch.h>
#include <dev/msm_proc_comm.h>

static void
writel(uint32_t val, uint32_t addr)
{
	volatile uint32_t *mem = (void *)addr;
	*mem = val;
}

static uint32_t
readl(uint32_t addr)
{
	volatile uint32_t *mem = (void *)addr;
	return *mem;
}

#define MSM_CSR_BASE 0xc0100000
#define MSM_A2M_INT(n) (MSM_CSR_BASE + 0x400 + (n) * 4)

static inline void notify_other_proc_comm(void)
{
        writel(1, MSM_A2M_INT(6));
}

#define APP_COMMAND 0x00
#define APP_STATUS  0x04
#define APP_DATA1   0x08
#define APP_DATA2   0x0C

#define MDM_COMMAND 0x10
#define MDM_STATUS  0x14
#define MDM_DATA1   0x18
#define MDM_DATA2   0x1C

/* Poll for a state change.
 *
 * Return an error in the event of a modem crash and
 * restart so the msm_proc_comm() routine can restart
 * the operation from the beginning.
 */
static int proc_comm_wait_for(uint32_t addr, unsigned value)
{
	for (;;) {
		if (readl(addr) == value)
			return 0;
	}
}

int msm_proc_comm(unsigned cmd, unsigned *data1, unsigned *data2)
{
	uint32_t base = 0x01f00000; //MSM_SHARED_RAM_BASE;
	unsigned long flags;
	int ret;

	for (;;) {
		if (proc_comm_wait_for(base + MDM_STATUS, PCOM_READY))
			continue;

		writel(cmd, base + APP_COMMAND);
		writel(data1 ? *data1 : 0, base + APP_DATA1);
		writel(data2 ? *data2 : 0, base + APP_DATA2);

		notify_other_proc_comm();

		if (proc_comm_wait_for(base + APP_COMMAND, PCOM_CMD_DONE))
			continue;

		if (readl(base + APP_STATUS) != PCOM_CMD_FAIL) {
			if (data1)
				*data1 = readl(base + APP_DATA1);
			if (data2)
				*data2 = readl(base + APP_DATA2);
			ret = 0;
		} else {
			ret = -1;
		}
		break;
	}

	writel(PCOM_CMD_IDLE, base + APP_COMMAND);

	return ret;
}
