/*
 * SPDX-License-Identifier: BSD-2-Clause
 * 
 * Copyright (c) 2019 Institute of Computing Technology, Chinese Academy of Sciences.
 *
 * Authors:
 *   Yisong Chang <changyisong@ict.ac.cn>
 */

#include <libfdt.h>
#include <fdt.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_platform.h>
#include <sbi/riscv_io.h>
#include <sbi_utils/irqchip/plic.h>
#include <sbi_utils/sys/clint.h>

#include "pm_service/pm_svc_main.h"

/* clang-format off */

#define SERVE_CLINT_ADDR		0x2000000

#define SERVE_PLIC_ADDR			0xc000000
#define SERVE_PLIC_NUM_SOURCES	16

/* UART base address was defined in config.mk */
#define UART_REG_FIFO		0x30
#define UART_REG_CH_STAT	0x2C

#define UART_TXFIFO_FULL	(1 << 4)
#define UART_RXFIFO_EMPTY	(1 << 1)
#define UART_RXFIFO_DATA	0x000000ff

#define SERVE_HART_STACK_SIZE		8192

#define SERVE_ENABLED_HART_MASK		((1 << SERVE_HART_COUNT) - 1)	

#define SERVE_HARITD_DISABLED		~(SERVE_ENABLED_HART_MASK)

#define SERVE_EXT_PM		0x09000000

static volatile void *uart_base;

/* clang-format on */

static inline void set_uart_base()
{
	uart_base = (volatile void *)SERVE_UART0_ADDR;
}

static inline u32 get_reg(u32 offset)
{
	return readl(uart_base + offset);
}

static inline void set_reg(u32 num, u32 offset)
{
	writel(num, uart_base + offset);
}

void serve_uart_putc(char ch)
{
	while (get_reg(UART_REG_CH_STAT) & UART_TXFIFO_FULL)
		;

	set_reg(ch, UART_REG_FIFO);
}

int serve_uart_getc(void)
{
	u32 ret = get_reg(UART_REG_CH_STAT);
	if (!(ret & UART_RXFIFO_EMPTY))
	{
		ret = get_reg(UART_REG_FIFO);
		return ret & UART_RXFIFO_DATA;
	}
	return -1;
}

static int serve_early_init(bool cold_boot)
{
	if (!cold_boot)
		return 0;

	set_uart_base();

	return 0;
}

static int serve_final_init(bool cold_boot)
{
	int rc;

	if (cold_boot) {
#if SERVE_ECALL_EXT
		rc = pm_setup();
		if (rc)
			return rc;
#endif
	}

	return 0;
}

static u32 serve_pmp_region_count(u32 hartid)
{
	return 7;
}

static int serve_pmp_region_info(u32 hartid, u32 index, ulong *prot,
				 ulong *addr, ulong *log2size)
{
	int ret = 0;

	switch (index) {

	//set S-MODE region with the lowest priority, leaving PMP1 - PMP6 to secure monitor
	case 6:
		*prot	  = PMP_R | PMP_W | PMP_X;
		*addr	  = 0;
		*log2size = __riscv_xlen;
		break;
	default:
		ret = -1;
		break;
	};

	return ret;
}

static int serve_irqchip_init(bool cold_boot)
{
	int rc;
	u32 hartid = sbi_current_hartid();

	if (cold_boot) {
		rc = plic_cold_irqchip_init(SERVE_PLIC_ADDR,
						SERVE_PLIC_NUM_SOURCES,
						SERVE_HART_COUNT);
		if (rc)
			return rc;
	}

	return plic_warm_irqchip_init(hartid, (hartid) ? (2 * hartid - 1) : 0,
					  (hartid) ? (2 * hartid) : -1);
}

static int serve_ipi_init(bool cold_boot)
{
	int rc;

	if (cold_boot) {
		rc = clint_cold_ipi_init(SERVE_CLINT_ADDR, SERVE_HART_COUNT);
		if (rc)
			return rc;
	}

	return clint_warm_ipi_init();
}

static int serve_timer_init(bool cold_boot)
{
	int rc;

	if (cold_boot) {
		rc = clint_cold_timer_init(SERVE_CLINT_ADDR, SERVE_HART_COUNT);
		if (rc)
			return rc;
	}

	return clint_warm_timer_init();
}

static int serve_vendor_ext_check(long extid) {
#if SERVE_ECALL_EXT
	switch (extid) {
		case SERVE_EXT_PM:
			return 1;
		default:
			return 0;
	}
#else
	return 0;
#endif
}

static int serve_vendor_ext_provider(
	long extid,
	long funcid,
	unsigned long *args,
	unsigned long *out_value,
	struct sbi_trap_info *out_trap
) {
#if SERVE_ECALL_EXT
	switch (extid) {
		case SERVE_EXT_PM:
			return pm_ecall_handler(funcid, args, out_value);
		default:
			return SBI_ENOTSUPP;
	}
#else
	return SBI_ENOTSUPP;
#endif
}

const struct sbi_platform_operations platform_ops = {
	.pmp_region_count	= serve_pmp_region_count,
	.pmp_region_info	= serve_pmp_region_info,
	.early_init			= serve_early_init,
	.final_init			= serve_final_init,
	.console_putc		= serve_uart_putc,
	.console_getc		= serve_uart_getc,
	.irqchip_init		= serve_irqchip_init,
	.ipi_send		= clint_ipi_send,
	.ipi_clear		= clint_ipi_clear,
	.ipi_init		= serve_ipi_init,
	.timer_value		= clint_timer_value,
	.timer_event_stop	= clint_timer_event_stop,
	.timer_event_start	= clint_timer_event_start,
	.timer_init		= serve_timer_init,
	.vendor_ext_check	 = serve_vendor_ext_check,
	.vendor_ext_provider  = serve_vendor_ext_provider,
};

const struct sbi_platform platform = {
	.opensbi_version	= OPENSBI_VERSION,
	.platform_version	= SBI_PLATFORM_VERSION(0x0, 0x01),
	.name			= "ICT SERVE",
	.features		= SBI_PLATFORM_DEFAULT_FEATURES,
	.hart_count		= SERVE_HART_COUNT,
	.hart_stack_size	= SERVE_HART_STACK_SIZE,
	.disabled_hart_mask	= SERVE_HARITD_DISABLED,
	.platform_ops_addr	= (unsigned long)&platform_ops
};
