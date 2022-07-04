/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_barrier.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_fp.h>
#include <sbi/riscv_locks.h>
#include <sbi/sbi_bits.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_platform.h>

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



/**
 * Return HART ID of the caller.
 */
unsigned int sbi_current_hartid()
{
	return (u32)csr_read(CSR_MHARTID);
}

static void mstatus_init(struct sbi_scratch *scratch, u32 hartid)
{
	const struct sbi_platform *plat = sbi_platform_ptr(scratch);

	/* Enable FPU */
	if (misa_extension('D') || misa_extension('F'))
		csr_write(CSR_MSTATUS, MSTATUS_FS);

	/* Enable user/supervisor use of perf counters */
	if (misa_extension('S') && sbi_platform_has_scounteren(plat))
		csr_write(CSR_SCOUNTEREN, -1);
	if (sbi_platform_has_mcounteren(plat))
		csr_write(CSR_MCOUNTEREN, -1);

	/* Disable all interrupts */
	csr_write(CSR_MIE, 0);

	/* Disable S-mode paging */
	if (misa_extension('S'))
		csr_write(CSR_SATP, 0);
}

/*static int fp_init(u32 hartid)
{
#ifdef __riscv_flen
	int i;
#endif

	if (!misa_extension('D') && !misa_extension('F'))
		return 0;

	if (!(csr_read(CSR_MSTATUS) & MSTATUS_FS))
		return SBI_EINVAL;

#ifdef __riscv_flen
	for (i = 0; i < 32; i++)
		init_fp_reg(i);
	csr_write(CSR_FCSR, 0);
#endif

	return 0;
}*/
static int fp_init(u32 hartid)
{
	return 0;
}

static int delegate_traps(struct sbi_scratch *scratch, u32 hartid)
{
	const struct sbi_platform *plat = sbi_platform_ptr(scratch);
	unsigned long interrupts, exceptions;

	if (!misa_extension('S'))
		/* No delegation possible as mideleg does not exist*/
		return 0;

	/* Send M-mode interrupts and most exceptions to S-mode */
	interrupts = MIP_SSIP | MIP_STIP | MIP_SEIP;
	exceptions = (1U << CAUSE_MISALIGNED_FETCH) | (1U << CAUSE_BREAKPOINT) |
		     (1U << CAUSE_USER_ECALL);
	if (sbi_platform_has_mfaults_delegation(plat))
		exceptions |= (1U << CAUSE_FETCH_PAGE_FAULT) |
			      (1U << CAUSE_LOAD_PAGE_FAULT) |
			      (1U << CAUSE_STORE_PAGE_FAULT);

	/*
	 * If hypervisor extension available then we only handle
	 * hypervisor calls (i.e. ecalls from HS-mode) and we let
	 * HS-mode handle supervisor calls (i.e. ecalls from VS-mode)
	 */
	if (misa_extension('H'))
		exceptions |= (1U << CAUSE_SUPERVISOR_ECALL);

	csr_write(CSR_MIDELEG, interrupts);
	csr_write(CSR_MEDELEG, exceptions);

	if (csr_read(CSR_MIDELEG) != interrupts)
		return SBI_EFAIL;
	if (csr_read(CSR_MEDELEG) != exceptions)
		return SBI_EFAIL;

	return 0;
}

unsigned long log2roundup(unsigned long x)
{
	unsigned long ret = 0;

	while (ret < __riscv_xlen) {
		if (x <= (1UL << ret))
			break;
		ret++;
	}

	return ret;
}

void sbi_hart_pmp_dump(struct sbi_scratch *scratch)
{
	const struct sbi_platform *plat = sbi_platform_ptr(scratch);
	unsigned long prot, addr, size, l2l;
	unsigned int i;

	if (!sbi_platform_has_pmp(plat))
		return;

	for (i = 0; i < PMP_COUNT; i++) {
		pmp_get(i, &prot, &addr, &l2l);
		if (!(prot & PMP_A))
			continue;
		if (l2l < __riscv_xlen)
			size = (1UL << l2l);
		else
			size = 0;
#if __riscv_xlen == 32
		sbi_printf("PMP%d: 0x%08lx-0x%08lx (A",
#else
		sbi_printf("PMP%d: 0x%016lx-0x%016lx (A",
#endif
			   i, addr, addr + size - 1);
		if (prot & PMP_L)
			sbi_printf(",L");
		if (prot & PMP_R)
			sbi_printf(",R");
		if (prot & PMP_W)
			sbi_printf(",W");
		if (prot & PMP_X)
			sbi_printf(",X");
		sbi_printf(")\n");
	}
}

/*void sbi_hart_pmp_dump(struct sbi_scratch *scratch)
{

}*/

static int pmp_init(struct sbi_scratch *scratch, u32 hartid)
{
	u32 i, count;
	unsigned long fw_start, fw_size_log2;
	ulong prot, addr, log2size;
	sbi_Debug_puts("\n\rpmp_init 1");
	const struct sbi_platform *plat = sbi_platform_ptr(scratch);
	sbi_Debug_puts("\n\rpmp_init 2");
	if (!sbi_platform_has_pmp(plat))
		return 0;
	sbi_Debug_puts("\n\rpmp_init 3");
	fw_size_log2 = log2roundup(scratch->fw_size);
	fw_start     = scratch->fw_start & ~((1UL << fw_size_log2) - 1UL);
	sbi_Debug_puts("\n\rpmp_init 4");
	pmp_set(0, 0, fw_start, fw_size_log2);
	sbi_Debug_puts("\n\rpmp_init 5");
	count = sbi_platform_pmp_region_count(plat, hartid);
	if ((PMP_COUNT - 1) < count)
		count = (PMP_COUNT - 1);
	sbi_Debug_puts("\n\rpmp_init 6");
	for (i = 0; i < count; i++) {
		if (sbi_platform_pmp_region_info(plat, hartid, i, &prot, &addr,
						 &log2size))
			continue;
		pmp_set(i + 1, prot, addr, log2size);
	}
	sbi_Debug_puts("\n\rpmp_init 7");
	return 0;
}
/*static int pmp_init(struct sbi_scratch *scratch, u32 hartid)
{
	sbi_Debug_puts("\n\rskip: pmp_init");
	return 0;
}*/

static unsigned long trap_info_offset;

int sbi_hart_init(struct sbi_scratch *scratch, u32 hartid, bool cold_boot)
{
	int rc;
	sbi_Debug_puts("\n\rlib/sbi/sbi_hart.c: sbi_hart_init: sbi_hart_init start:");
	if (cold_boot) {
		trap_info_offset = sbi_scratch_alloc_offset(__SIZEOF_POINTER__,
							    "HART_TRAP_INFO");
		if (!trap_info_offset)
		{
			sbi_Debug_puts("\n\r SBI_ENOMEM");
			return SBI_ENOMEM;
		}
	}
	sbi_Debug_puts("\n\rlib/sbi/sbi_hart.c: sbi_hart_init: into: mstatus_init");
	mstatus_init(scratch, hartid);
	sbi_Debug_puts("\n\rlib/sbi/sbi_hart.c: sbi_hart_init: into: fp_init");
	rc = fp_init(hartid);
	if (rc)
	{
		sbi_Debug_puts("\n\rfp_init error");
		return rc;
	}
	sbi_Debug_puts("\n\rlib/sbi/sbi_hart.c: sbi_hart_init: into: delegate_traps");
	rc = delegate_traps(scratch, hartid);
	if (rc)
	{
		sbi_Debug_puts("\n\rdelegate_traps error");
		return rc;
	}
	return pmp_init(scratch, hartid);
}

void *sbi_hart_get_trap_info(struct sbi_scratch *scratch)
{
	unsigned long *trap_info;

	if (!trap_info_offset)
		return NULL;

	trap_info = sbi_scratch_offset_ptr(scratch, trap_info_offset);

	return (void *)(*trap_info);
}

void sbi_hart_set_trap_info(struct sbi_scratch *scratch, void *data)
{
	unsigned long *trap_info;

	if (!trap_info_offset)
		return;

	trap_info = sbi_scratch_offset_ptr(scratch, trap_info_offset);
	*trap_info = (unsigned long)data;
}

void __attribute__((noreturn)) sbi_hart_hang(void)
{
	while (1)
		wfi();
	__builtin_unreachable();
}

void __attribute__((noreturn))
sbi_hart_switch_mode(unsigned long arg0, unsigned long arg1,
		     unsigned long next_addr, unsigned long next_mode,
		     bool next_virt)
{
#if __riscv_xlen == 32
	unsigned long val, valH;
#else
	unsigned long val;
#endif
	sbi_printf("\n\rarg0:0x%lx",arg0);
	sbi_printf("\n\rarg1:0x%lx",arg1);
	switch (next_mode) {
	case PRV_M:
		break;
	case PRV_S:
		if (!misa_extension('S'))
			sbi_hart_hang();
		break;
	case PRV_U:
		if (!misa_extension('U'))
			sbi_hart_hang();
		break;
	default:
		sbi_hart_hang();
	}

	val = csr_read(CSR_MSTATUS);
	val = INSERT_FIELD(val, MSTATUS_MPP, next_mode);
	val = INSERT_FIELD(val, MSTATUS_MPIE, 0);
#if __riscv_xlen == 32
	if (misa_extension('H')) {
		valH = csr_read(CSR_MSTATUSH);
		valH = INSERT_FIELD(valH, MSTATUSH_MTL, 0);
		if (next_virt)
			valH = INSERT_FIELD(valH, MSTATUSH_MPV, 1);
		else
			valH = INSERT_FIELD(valH, MSTATUSH_MPV, 0);
		csr_write(CSR_MSTATUSH, valH);
	}
#else
	if (misa_extension('H')) {
		val = INSERT_FIELD(val, MSTATUS_MTL, 0);
		if (next_virt)
			val = INSERT_FIELD(val, MSTATUS_MPV, 1);
		else
			val = INSERT_FIELD(val, MSTATUS_MPV, 0);
	}
#endif
	csr_write(CSR_MSTATUS, val);
	csr_write(CSR_MEPC, next_addr);

	if (next_mode == PRV_S) {
		csr_write(CSR_STVEC, next_addr);
		csr_write(CSR_SSCRATCH, 0);
		csr_write(CSR_SIE, 0);
		csr_write(CSR_SATP, 0);
	} else if (next_mode == PRV_U) {
		csr_write(CSR_UTVEC, next_addr);
		csr_write(CSR_USCRATCH, 0);
		csr_write(CSR_UIE, 0);
	}

	//register unsigned long a0 asm("a0") = arg0;
	//register unsigned long a1 asm("a1") = arg1;

	sbi_Debug_puts("\n\rBefore mret, print some CSRs:");

	sbi_printf("\n\rCSR_Read: 0x%lx - CSR_mhartid\n\r",csr_read(CSR_MHARTID));

	sbi_printf("\n\rCSR_Read: 0x%lx - CSR_MSTATUS",csr_read(CSR_MSTATUS));	
        sbi_printf("\n\rCSR_Read: 0x%lx - CSR_SSTATUS",csr_read(CSR_SSTATUS));
        sbi_printf("\n\rCSR_Read: 0x%lx - CSR_MSCRATCH",csr_read(CSR_MSCRATCH));
        sbi_printf("\n\rCSR_Read: 0x%lx - CSR_SSCRATCH",csr_read(CSR_SSCRATCH));
        sbi_printf("\n\rCSR_Read: 0x%lx - CSR_STVEC",csr_read(CSR_STVEC));
        sbi_printf("\n\rCSR_Read: 0x%lx - CSR_MIE",csr_read(CSR_MIE));
	sbi_printf("\n\rCSR_Read: 0x%lx - CSR_MIP",csr_read(CSR_MIP));
        sbi_printf("\n\rCSR_Read: 0x%lx - CSR_MISA",csr_read(CSR_MISA));
        sbi_printf("\n\rCSR_Read: 0x%lx - CSR_MTVEC",csr_read(CSR_MTVEC));
        sbi_printf("\n\rCSR_Read: 0x%lx - CSR_STVEC",csr_read(CSR_STVEC));
        sbi_printf("\n\rCSR_Read: 0x%lx - CSR_MEDELEG",csr_read(CSR_MEDELEG));
        sbi_printf("\n\rCSR_Read: 0x%lx - CSR_MCOUNTEREN",csr_read(CSR_MCOUNTEREN));
        sbi_printf("\n\rCSR_Read: 0x%lx - CSR_SCOUNTEREN",csr_read(CSR_SCOUNTEREN));
        sbi_printf("\n\rCSR_Read: 0x%lx - CSR_PMPCFG0",csr_read(CSR_PMPCFG0));
        sbi_printf("\n\rCSR_Read: 0x%lx - CSR_MEPC",csr_read(CSR_MEPC));
        sbi_printf("\n\rCSR_Read: 0x%lx - CSR_MCAUSE",csr_read(CSR_MCAUSE));
        sbi_printf("\n\rCSR_Read: 0x%lx - CSR_MTVAL",csr_read(CSR_MTVAL));
        
	//sbi_printf("\n\ra0:0x%lx",a0);
	//sbi_printf("\n\ra1:0x%lx",a1);
	//sbi_printf("\n\rCSR_Read: 0x%lx - 
	//	sbi_hart_hang();
	
        register unsigned long a0 asm("a0") = arg0;
        register unsigned long a1 asm("a1") = arg1;

	__asm__ __volatile__("mret" : : "r"(a0), "r"(a1));
	__builtin_unreachable();
}

static spinlock_t avail_hart_mask_lock	      = SPIN_LOCK_INITIALIZER;
static volatile unsigned long avail_hart_mask = 0;

void sbi_hart_mark_available(u32 hartid)
{
	spin_lock(&avail_hart_mask_lock);
	avail_hart_mask |= (1UL << hartid);
	spin_unlock(&avail_hart_mask_lock);
}

void sbi_hart_unmark_available(u32 hartid)
{
	spin_lock(&avail_hart_mask_lock);
	avail_hart_mask &= ~(1UL << hartid);
	spin_unlock(&avail_hart_mask_lock);
}

ulong sbi_hart_available_mask(void)
{
	ulong ret;

	spin_lock(&avail_hart_mask_lock);
	ret = avail_hart_mask;
	spin_unlock(&avail_hart_mask_lock);

	return ret;
}

typedef struct sbi_scratch *(*h2s)(ulong hartid);

struct sbi_scratch *sbi_hart_id_to_scratch(struct sbi_scratch *scratch,
					   u32 hartid)
{
	return ((h2s)scratch->hartid_to_scratch)(hartid);
}

#define COLDBOOT_WAIT_BITMAP_SIZE __riscv_xlen
static spinlock_t coldboot_lock = SPIN_LOCK_INITIALIZER;
static unsigned long coldboot_done = 0;
static unsigned long coldboot_wait_bitmap = 0;

void sbi_hart_wait_for_coldboot(struct sbi_scratch *scratch, u32 hartid)
{
	const struct sbi_platform *plat = sbi_platform_ptr(scratch);

	if ((sbi_platform_hart_count(plat) <= hartid) ||
	    (COLDBOOT_WAIT_BITMAP_SIZE <= hartid))
		sbi_hart_hang();

	/* Set MSIE bit to receive IPI */
	csr_set(CSR_MIE, MIP_MSIP);

	/* Acquire coldboot lock */
	spin_lock(&coldboot_lock);

	/* Mark current HART as waiting */
	coldboot_wait_bitmap |= (1UL << hartid);

	/* Wait for coldboot to finish using WFI */
	while (!coldboot_done) {
		spin_unlock(&coldboot_lock);
		wfi();
		spin_lock(&coldboot_lock);
	};

	/* Unmark current HART as waiting */
	coldboot_wait_bitmap &= ~(1UL << hartid);

	/* Release coldboot lock */
	spin_unlock(&coldboot_lock);

	/* Clear current HART IPI */
	sbi_platform_ipi_clear(plat, hartid);
}

void sbi_hart_wake_coldboot_harts(struct sbi_scratch *scratch, u32 hartid)
{
	const struct sbi_platform *plat = sbi_platform_ptr(scratch);
	int max_hart			= sbi_platform_hart_count(plat);

	/* Acquire coldboot lock */
	spin_lock(&coldboot_lock);

	/* Mark coldboot done */
	coldboot_done = 1;

	/* Send an IPI to all HARTs waiting for coldboot */
	for (int i = 0; i < max_hart; i++) {
		if ((i != hartid) && (coldboot_wait_bitmap & (1UL << i)))
			sbi_platform_ipi_send(plat, i);
	}

	/* Release coldboot lock */
	spin_unlock(&coldboot_lock);
}
