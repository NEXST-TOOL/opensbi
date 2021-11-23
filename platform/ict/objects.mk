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
include $(platform_src_dir)/core/$(RV_TARGET).mk
endif

include $(platform_src_dir)/plat/serve_$(SERVE_PLAT).mk

