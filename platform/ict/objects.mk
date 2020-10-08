#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Institute of Computing Technology, Chinese Academy of Sciences.
#
# Authors:
#   Yisong Chang <changyisong@ict.ac.cn>
#

platform-objs-y += platform.o
platform-genflags-y += -DSERVE_HART_COUNT=$(HART_COUNT)

ifneq ($(RV_TARGET),)
include $(platform_src_dir)/$(RV_TARGET).mk
endif

ifeq ($(SERVE_PLAT),r)
platform-genflags-y += -DSERVE_UART0_ADDR=0xE0000000
platform-genflags-y += -DSERVE_ECALL_EXT=0
else
platform-genflags-y += -DSERVE_UART0_ADDR=0xFF000000
platform-genflags-y += -DSERVE_ECALL_EXT=1
endif

