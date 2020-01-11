/*
 * Copyright (c) 2013-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * APU specific definition of processors in the subsystem as well as functions
 * for getting information about and changing state of the APU.
 */

#include "plat_ipi.h"
#include "zynqmp_def.h"
#include "pm_api_sys.h"
#include "pm_client.h"
#include "pm_ipi.h"

#define IRQ_MAX		84
#define NUM_GICD_ISENABLER	((IRQ_MAX >> 5) + 1)
#define UNDEFINED_CPUID		(~0)

#define PM_SUSPEND_MODE_STD		0
#define PM_SUSPEND_MODE_POWER_OFF	1

extern const struct pm_ipi pl0_ipi;

const struct pm_ipi pl0_ipi = {
	.local_ipi_id = IPI_ID_PL0,
	.remote_ipi_id = IPI_ID_PMU0,
	.buffer_base = IPI_BUFFER_PL0_BASE,
};

/* Order in pm_procs_all array must match cpu ids */
static const struct pm_proc pm_procs_all[] = {
	{
		.node_id = NODE_IPI_PL_0,
		.pwrdn_mask = 0,
		.ipi = &pl0_ipi,
	},
};

const struct pm_proc *primary_proc = &pm_procs_all[0];
