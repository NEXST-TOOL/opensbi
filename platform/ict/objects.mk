#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Institute of Computing Technology, Chinese Academy of Sciences.
#
# Authors:
#   Yisong Chang <changyisong@ict.ac.cn>
#

platform-objs-y += platform.o
#ifneq ($(SERVE_PLAT),r)
#platform-objs-y += pmu_ipi.o
#endif

platform-genflags-y += -DSERVE_HART_COUNT=$(HART_COUNT) \

ifeq ($(SERVE_PLAT),r)
platform-genflags-y += -DSERVE_UART0_ADDR=0xE0000000
else
platform-genflags-y += -DSERVE_UART0_ADDR=0xFF000000
endif

