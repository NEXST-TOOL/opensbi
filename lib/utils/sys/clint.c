/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/riscv_io.h>
#include <sbi/riscv_atomic.h>
#include <sbi/sbi_hart.h>
#include <sbi_utils/sys/clint.h>

#include <sbi/sbi_console.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_io.h>
#define uart_base (volatile void *)0xE0000000
/* UART base address was defined in plat/serve_X.mk */
#define UART_TXFIFO_FULL        (1 << UART_TXFIFO_FULL_BIT)
#define UART_RXFIFO_EMPTY       (1 << UART_RXFIFO_EMPTY_BIT)
#define UART_RXFIFO_DATA        0x000000ff


static inline void set_reg(u32 num, u32 offset)
{
        writel(num, uart_base + offset);
}

static inline u32 get_reg(u32 offset)
{
        return readl(uart_base + offset);
}


static void serve_uart_putc(char ch)
{
        while (get_reg(UART_REG_CH_STAT) & UART_TXFIFO_FULL)
                ;

        set_reg(ch, UART_REG_TX_FIFO);
}


static void sbi_Debug_puts(const char *str)
{
        while (*str) {
                serve_uart_putc(*str);
                str++;
        }
}


static u32 clint_ipi_hart_count;
static volatile void *clint_ipi_base;
static volatile u32 *clint_ipi;

void clint_ipi_send(u32 target_hart)
{
	if (clint_ipi_hart_count <= target_hart)
		return;

	/* Set CLINT IPI */
	writel(1, &clint_ipi[target_hart]);
}

void clint_ipi_clear(u32 target_hart)
{
	if (clint_ipi_hart_count <= target_hart)
		return;

	/* Clear CLINT IPI */
	writel(0, &clint_ipi[target_hart]);
}

int clint_warm_ipi_init(void)
{
	u32 hartid = sbi_current_hartid();

	if (!clint_ipi_base)
		return -1;

	/* Clear CLINT IPI */
	clint_ipi_clear(hartid);

	return 0;
}

int clint_cold_ipi_init(unsigned long base, u32 hart_count)
{
	/* Figure-out CLINT IPI register address */
	clint_ipi_hart_count = hart_count;
	clint_ipi_base	     = (void *)base;
	clint_ipi	     = (u32 *)clint_ipi_base;

	return 0;
}

static u32 clint_time_hart_count;
static volatile void *clint_time_base;
static volatile u64 *clint_time_val;
static volatile u64 *clint_time_cmp;

static inline u32 clint_time_read_hi()
{
	return readl_relaxed((u32 *)clint_time_val + 1);
}

u64 clint_timer_value(void)
{
#if __riscv_xlen == 64
	return readq_relaxed(clint_time_val);
#else
	u32 lo, hi;

	do {
		hi = clint_time_read_hi();
		lo = readl_relaxed(clint_time_val);
	} while (hi != clint_time_read_hi());

	return ((u64)hi << 32) | (u64)lo;
#endif
}

void clint_timer_event_stop(void)
{
	sbi_Debug_puts("\n\rlib/utils/sys/clint.c: into clint_timer_event_stop()");
	u32 target_hart = sbi_current_hartid();

	if (clint_time_hart_count <= target_hart)
		return;

		/* Clear CLINT Time Compare */
#if __riscv_xlen == 64
	writeq_relaxed(-1ULL, &clint_time_cmp[target_hart]);
#else
	writel_relaxed(-1UL, &clint_time_cmp[target_hart]);
	writel_relaxed(-1UL, (void *)(&clint_time_cmp[target_hart]) + 0x04);
#endif
	sbi_Debug_puts("\n\rlib/utils/sys/clint.c: done clint_timer_event_stop()");
}

void clint_timer_event_start(u64 next_event)
{
	sbi_Debug_puts("\n\rlib/utils/sys/clint.c: into clint_timer_event_start()");
	u32 target_hart = sbi_current_hartid();

	if (clint_time_hart_count <= target_hart)
		return;

		/* Program CLINT Time Compare */
#if __riscv_xlen == 64
	writeq_relaxed(next_event, &clint_time_cmp[target_hart]);
#else
	u32 mask = -1UL;
	writel_relaxed(next_event & mask, &clint_time_cmp[target_hart]);
	writel_relaxed(next_event >> 32,
		       (void *)(&clint_time_cmp[target_hart]) + 0x04);
#endif
	sbi_Debug_puts("\n\rlib/utils/sys/clint.c: done clint_timer_event_start()");

}

int clint_warm_timer_init(void)
{
	sbi_Debug_puts("\n\rlib/utils/sys/clint.c: into clint_warm_timer_init()");
	u32 target_hart = sbi_current_hartid();

	if (clint_time_hart_count <= target_hart || !clint_time_base)
		return -1;

		/* Clear CLINT Time Compare */
#if __riscv_xlen == 64
	writeq_relaxed(-1ULL, &clint_time_cmp[target_hart]);
#else
	writel_relaxed(-1UL, &clint_time_cmp[target_hart]);
	writel_relaxed(-1UL, (void *)(&clint_time_cmp[target_hart]) + 0x04);
#endif
	sbi_printf("\n\rtarget_hart: %u",target_hart);
	sbi_printf("\n\rclint_time_cmp[%u]: %lx",target_hart,clint_time_cmp[target_hart]);
	sbi_printf("\n\rCSR_Read: 0x%lx - CSR_MIP",csr_read(CSR_MIP));

	sbi_printf("\n\rWaiting for CSR_MIP clear ...");
	while(csr_read(CSR_MIP))
	{
		sbi_printf("\n\rclint_time_val(mtime):%lx",readq_relaxed(clint_time_val));
	}
	sbi_printf("\n\rlib/utils/sys/clint.c: Done Clear CLINT Time Compare");
	return 0;
}

int clint_cold_timer_init(unsigned long base, u32 hart_count)
{
	sbi_Debug_puts("\n\rlib/utils/sys/clint.c: into clint_cold_timer_init()");
	/* Figure-out CLINT Time register address */
	clint_time_hart_count = hart_count;
	clint_time_base	      = (void *)base;
	clint_time_val	      = (u64 *)(clint_time_base + 0xbff8);
	clint_time_cmp	      = (u64 *)(clint_time_base + 0x4000);

	return 0;
}
