/*
 * Copyright (c) 2013-2015, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PM_SVC_MAIN_H
#define PM_SVC_MAIN_H

#include "pm_common.h"

int pm_setup(void);
int pm_ecall_handler(long funcid, unsigned long *args, unsigned long *outval);

#endif /* PM_SVC_MAIN_H */
