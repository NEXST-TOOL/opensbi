#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Institute of Computing Technology, Chinese Academy of Sciences.
#
# Authors:
#   Yisong Chang <changyisong@ict.ac.cn>
#

platform-objs-y += platform.o
ifneq ($(SERVE_PLAT),r)
platform-objs-y += pm_service/ipi.o
platform-objs-y += pm_service/pm_api_clock.o
platform-objs-y += pm_service/pm_api_ioctl.o
platform-objs-y += pm_service/pm_api_pinctrl.o
platform-objs-y += pm_service/pm_api_sys.o
platform-objs-y += pm_service/pm_client.o
platform-objs-y += pm_service/pm_ipi.o
platform-objs-y += pm_service/pm_svc_main.o
platform-objs-y += pm_service/zynqmp_ipi.o
endif

platform-genflags-y += -DSERVE_HART_COUNT=$(HART_COUNT) \

ifeq ($(SERVE_PLAT),r)
platform-genflags-y += -DSERVE_UART0_ADDR=0xE0000000
platform-genflags-y += -DSERVE_ECALL_EXT=0
else
platform-genflags-y += -DSERVE_UART0_ADDR=0xFF000000
platform-genflags-y += -DSERVE_ECALL_EXT=1
endif

