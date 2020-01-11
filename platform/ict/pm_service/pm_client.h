/*
 * Copyright (c) 2013-2015, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Contains APU specific macros and macros to be defined depending on
 * the execution environment.
 */

#ifndef PM_CLIENT_H
#define PM_CLIENT_H

#include "pm_common.h"
#include "pm_defs.h"

/* Global variables to be set in pm_client.c */
extern const struct pm_proc *primary_proc;

#endif /* PM_CLIENT_H */
