#include <sbi/sbi_ecall.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_trap.h>
#include <sbi/riscv_asm.h>

#define perf_list(_) \
	_(0xb00)     \
	_(0xb01)     \
	_(0xb02)     \
	_(0xb03)     \
	_(0xb04)     \
	_(0xb05)     \
	_(0xb06)     \
	_(0xb07)     \
	_(0xb08)     \
	_(0xb09)     \
	_(0xb0a)     \
	_(0xb0b)     \
	_(0xb0c)     \
	_(0xb0d)     \
	_(0xb0e)     \
	_(0xb0f)     \
	_(0xb10)     \
	_(0xb11)     \
	_(0xb12)     \
	_(0xb13)     \
	_(0xb14)     \
	_(0xb15)     \
	_(0xb16)     \
	_(0xb17)     \
	_(0xb18)     \
	_(0xb19)     \
	_(0xb1a)     \
	_(0xb1b)     \
	_(0xb1c)     \
	_(0xb1d)     \
	_(0xb1e)     \
	_(0xb1f)     \
	_(0x320)     \
	_(0x321)     \
	_(0x322)     \
	_(0x323)     \
	_(0x324)     \
	_(0x325)     \
	_(0x326)     \
	_(0x327)     \
	_(0x328)     \
	_(0x329)     \
	_(0x32a)     \
	_(0x32b)     \
	_(0x32c)     \
	_(0x32d)     \
	_(0x32e)     \
	_(0x32f)     \
	_(0x330)     \
	_(0x331)     \
	_(0x332)     \
	_(0x333)     \
	_(0x334)     \
	_(0x335)     \
	_(0x336)     \
	_(0x337)     \
	_(0x338)     \
	_(0x339)     \
	_(0x33a)     \
	_(0x33b)     \
	_(0x33c)     \
	_(0x33d)     \
	_(0x33e)     \
	_(0x33f)

static int sbi_ecall_exp_perf_handler(unsigned long extid, unsigned long funcid,
				      const struct sbi_trap_regs *regs,
				      unsigned long *out_val,
				      struct sbi_trap_info *out_trap)
{
	int ret = SBI_OK;

	switch (funcid) {

	/* set csr perf func id */
	case SBI_EXT_EXP_PERF_SET:
		switch (regs->a0) {

#define CASE_CSR(num)                               \
	case num:                                   \
		*out_val = csr_swap(num, regs->a1); \
		break;
			perf_list(CASE_CSR);
#undef CASE_CSR

		default:
			return SBI_EINVAL;
		}
		break;

	/* get csr perf func id */
	case SBI_EXT_EXP_PERF_GET:
		switch (regs->a0) {

#define CASE_CSR(num)                     \
	case num:                         \
		*out_val = csr_read(num); \
		break;
			perf_list(CASE_CSR);
#undef CASE_CSR

		default:
			return SBI_EINVAL;
		}
		break;

	/* func id is illegal */
	default:
		ret = SBI_ENOTSUPP;
	}
	return ret;
}

struct sbi_ecall_extension ecall_exp_perf = {
	.extid_start = SBI_EXT_EXP_PERF,
	.extid_end   = SBI_EXT_EXP_PERF,
	.handle	     = sbi_ecall_exp_perf_handler,
};
