/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_atomic.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_ecall.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_ipi.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_system.h>
#include <sbi/sbi_timer.h>
#include <sbi/sbi_version.h>

#ifdef WITH_SM
#include <sm.h>
#endif

#define BANNER                                              \
	"   ____                    _____ ____ _____\n"     \
	"  / __ \\                  / ____|  _ \\_   _|\n"  \
	" | |  | |_ __   ___ _ __ | (___ | |_) || |\n"      \
	" | |  | | '_ \\ / _ \\ '_ \\ \\___ \\|  _ < | |\n" \
	" | |__| | |_) |  __/ | | |____) | |_) || |_\n"     \
	"  \\____/| .__/ \\___|_| |_|_____/|____/_____|\n"  \
	"        | |\n"                                     \
	"        |_|\n\n"

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


static void sbi_boot_prints(struct sbi_scratch *scratch, u32 hartid)
{
	int xlen;
	char str[64];
	const struct sbi_platform *plat = sbi_platform_ptr(scratch);

#ifdef OPENSBI_VERSION_GIT
	sbi_printf("\nOpenSBI %s\n", OPENSBI_VERSION_GIT);
#else
	sbi_printf("\nOpenSBI v%d.%d\n", OPENSBI_VERSION_MAJOR,
		   OPENSBI_VERSION_MINOR);
#endif

	sbi_printf(BANNER);

	/* Determine MISA XLEN and MISA string */
	xlen = misa_xlen();
	if (xlen < 1) {
		sbi_printf("Error %d getting MISA XLEN\n", xlen);
		sbi_hart_hang();
	}
	xlen = 16 * (1 << xlen);
	misa_string(str, sizeof(str));

	/* Platform details */
	sbi_printf("Platform Name          : %s\n", sbi_platform_name(plat));
	sbi_printf("Platform HART Features : RV%d%s\n", xlen, str);
	sbi_printf("Platform Max HARTs     : %d\n",
		   sbi_platform_hart_count(plat));
	sbi_printf("Current Hart           : %u\n", hartid);
	/* Firmware details */
	sbi_printf("Firmware Base          : 0x%lx\n", scratch->fw_start);
	sbi_printf("Firmware Size          : %d KB\n",
		   (u32)(scratch->fw_size / 1024));
	/* Generic details */
	sbi_printf("Runtime SBI Version    : %d.%d\n",
		   sbi_ecall_version_major(), sbi_ecall_version_minor());
	sbi_printf("\n");
	sbi_printf("\n\rhartid: %d",hartid);
	sbi_hart_pmp_dump(scratch);

}

static void __noreturn init_coldboot(struct sbi_scratch *scratch, u32 hartid)
{
	int rc;
	const struct sbi_platform *plat = sbi_platform_ptr(scratch);
	sbi_Debug_puts("\n\rinto: init_coldboot: sbi_system_early_init");
	rc = sbi_system_early_init(scratch, TRUE);
	if (rc)
	{
		sbi_Debug_puts("\n\r rc=1,hang");
		sbi_hart_hang();
	}
	sbi_Debug_puts("\n\rinto: init_coldboot: sbi_hart_init");
	rc = sbi_hart_init(scratch, hartid, TRUE);
	if (rc)
	{
		sbi_Debug_puts("\n\rsbi_hart_init rc=1,hang");
		sbi_hart_hang();
	}
	sbi_Debug_puts("\n\rinto: init_coldboot:  sbi_console_init");
	rc = sbi_console_init(scratch);
	if (rc)
		sbi_hart_hang();
	sbi_Debug_puts("\n\rinto: init_coldboot:  sbi_platform_irqchip_init");	
	rc = sbi_platform_irqchip_init(plat, TRUE);
	if (rc)
		sbi_hart_hang();
	sbi_Debug_puts("\n\rinto: init_coldboot:  sbi_ipi_init");
	rc = sbi_ipi_init(scratch, TRUE);
	if (rc)
		sbi_hart_hang();
	sbi_Debug_puts("\n\rinto: init_coldboot:  sbi_timer_init");
	rc = sbi_timer_init(scratch, TRUE);
	if (rc)
		sbi_hart_hang();
        sbi_Debug_puts("\n\rinto: init_coldboot:  sbi_system_final_init");
	rc = sbi_system_final_init(scratch, TRUE);
	if (rc)
		sbi_hart_hang();
        sbi_Debug_puts("\n\rinto: init_coldboot: sbi_boot_prints");
	if (!(scratch->options & SBI_SCRATCH_NO_BOOT_PRINTS))
		sbi_boot_prints(scratch, hartid);
//        sbi_Debug_puts("\n\rinto: init_coldboot: sbi_hart_wake_coldboot_harts");
//	sbi_printf("\ninto: init_coldboot: sbi_hart_wake_coldboot_harts");
	if (!sbi_platform_has_hart_hotplug(plat))
	{
		sbi_Debug_puts("\n\rinto: init_coldboot: sbi_hart_wake_coldboot_harts");
		sbi_hart_wake_coldboot_harts(scratch, hartid);
	}

#ifdef WITH_SM
    sbi_printf("Initializing sm...\r\n");
	sm_init();
	sbi_printf("sm init done...\r\n");
#endif

	sbi_hart_mark_available(hartid);
//      sbi_Debug_puts("\n\rinto: init_coldboot:  sbi_hart_switch_mode");
	sbi_printf("\r\nscratch->next_addr: %lx",scratch->next_addr);
        sbi_printf("\r\nscratch->next_mode: %lu",scratch->next_mode);
        sbi_printf("\r\nscratch->next_arg1: %lu",scratch->next_arg1);
        //sbi_printf("\r\nsbi_hart_hang here...");
	for (int i=0; i<6; ++i)
		sbi_printf("\r\n ");
	//sbi_hart_hang();
	sbi_printf("\r\n\"hartid\" into sbi_hart_switch_mode() :%x",hartid);	
	sbi_hart_switch_mode(hartid, scratch->next_arg1, scratch->next_addr,
			     scratch->next_mode, FALSE);
}

static void __noreturn init_warmboot(struct sbi_scratch *scratch, u32 hartid)
{
	int rc;
	const struct sbi_platform *plat = sbi_platform_ptr(scratch);
        sbi_Debug_puts("\n\rinto: init_warmboot");
	if (!sbi_platform_has_hart_hotplug(plat))
		sbi_hart_wait_for_coldboot(scratch, hartid);
	
	if (sbi_platform_hart_disabled(plat, hartid))
		sbi_hart_hang();
        sbi_Debug_puts("\n\rinto: init_warmboot: sbi_system_early_init");
	rc = sbi_system_early_init(scratch, FALSE);
	if (rc)
		sbi_hart_hang();
        sbi_Debug_puts("\n\rinto: init_warmboot: sbi_hart_init");
	rc = sbi_hart_init(scratch, hartid, FALSE);
	if (rc)
		sbi_hart_hang();
        sbi_Debug_puts("\n\rinto: init_warmboot: sbi_platform_irqchip_init");
	rc = sbi_platform_irqchip_init(plat, FALSE);
	if (rc)
		sbi_hart_hang();
        sbi_Debug_puts("\n\rinto: init_warmboot: sbi_ipi_init");
	rc = sbi_ipi_init(scratch, FALSE);
	if (rc)
		sbi_hart_hang();
        sbi_Debug_puts("\n\rinto: init_warmboot: sbi_timer_init");
	rc = sbi_timer_init(scratch, FALSE);
	if (rc)
		sbi_hart_hang();
        sbi_Debug_puts("\n\rinto: init_warmboot: sbi_system_final_init");
	rc = sbi_system_final_init(scratch, FALSE);
	if (rc)
		sbi_hart_hang();

	sbi_hart_mark_available(hartid);
	
#ifdef WITH_SM
	sbi_printf("Initializing sm...\r\n");
	sm_init();
	sbi_printf("sm init done...\r\n");
#endif

	if (sbi_platform_has_hart_hotplug(plat))
		/* TODO: To be implemented in-future. */
		sbi_hart_hang();
	else
	{
        	sbi_Debug_puts("\n\rinto: init_warmboot: sbi_hart_switch_mode");
		sbi_hart_switch_mode(hartid, scratch->next_arg1,
				     scratch->next_addr,
				     scratch->next_mode, FALSE);
	}
}

static atomic_t coldboot_lottery = ATOMIC_INITIALIZER(0);

/**
 * Initialize OpenSBI library for current HART and jump to next
 * booting stage.
 *
 * The function expects following:
 * 1. The 'mscratch' CSR is pointing to sbi_scratch of current HART
 * 2. Stack pointer (SP) is setup for current HART
 * 3. Interrupts are disabled in MSTATUS CSR
 * 4. All interrupts are disabled in MIE CSR
 *
 * @param scratch pointer to sbi_scratch of current HART
 */
void __noreturn sbi_init(struct sbi_scratch *scratch)
{
	bool coldboot			= FALSE;
	u32 hartid			= sbi_current_hartid();
	const struct sbi_platform *plat = sbi_platform_ptr(scratch);
	sbi_Debug_puts("\n\r ----- sbi_init ----- \n\r");
	if (sbi_platform_hart_disabled(plat, hartid))
		sbi_hart_hang();

	if (atomic_add_return(&coldboot_lottery, 1) == 1)
		coldboot = TRUE;

	if (coldboot)
	{
		sbi_Debug_puts("\n\rinto: init_coldboot ... ");
		init_coldboot(scratch, hartid);
	}
	else
	{
                sbi_Debug_puts("\n\rinto: init_warmboot ... ");
		init_warmboot(scratch, hartid);
	}
}
