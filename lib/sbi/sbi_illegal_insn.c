/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_bits.h>
#include <sbi/sbi_emulate_csr.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_illegal_insn.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_unpriv.h>
#include <sbi/sbi_console.h>

#include <sbi/riscv_fp.h>

#define MATCH_FLD 0x3007
#define MASK_FLD  0x707f

#define MATCH_FMV_D_X 0xf2000053
#define MASK_FMV_D_X  0xfff0707f

typedef int (*illegal_insn_func)(ulong insn, u32 hartid, ulong mcause,
				 struct sbi_trap_regs *regs,
				 struct sbi_scratch *scratch);

static int truly_illegal_insn(ulong insn, u32 hartid, ulong mcause,
			      struct sbi_trap_regs *regs,
			      struct sbi_scratch *scratch)
{
	struct sbi_trap_info trap;

	trap.epc = regs->mepc;
	trap.cause = mcause;
	trap.tval = insn;
	return sbi_trap_redirect(regs, &trap, scratch);
}

static int system_opcode_insn(ulong insn, u32 hartid, ulong mcause,
			      struct sbi_trap_regs *regs,
			      struct sbi_scratch *scratch)
{
	int do_write, rs1_num = (insn >> 15) & 0x1f;
	ulong rs1_val = GET_RS1(insn, regs);
	int csr_num   = (u32)insn >> 20;
	ulong csr_val, new_csr_val;

	/*
	 * WFI always traps as illegal instruction when executed from
	 * VS/VU mode so we just forward it to HS-mode.
	 */
#if __riscv_xlen == 32
	if ((regs->mstatusH & MSTATUSH_MPV) &&
#else
	if ((regs->mstatus & MSTATUS_MPV) &&
#endif
	    (insn & INSN_MASK_WFI) == INSN_MATCH_WFI)
		return truly_illegal_insn(insn, hartid, mcause,
					  regs, scratch);

	if (sbi_emulate_csr_read(csr_num, hartid, regs, scratch, &csr_val))
		return truly_illegal_insn(insn, hartid, mcause,
					  regs, scratch);

	do_write = rs1_num;
	switch (GET_RM(insn)) {
	case 1:
		new_csr_val = rs1_val;
		do_write    = 1;
		break;
	case 2:
		new_csr_val = csr_val | rs1_val;
		break;
	case 3:
		new_csr_val = csr_val & ~rs1_val;
		break;
	case 5:
		new_csr_val = rs1_num;
		do_write    = 1;
		break;
	case 6:
		new_csr_val = csr_val | rs1_num;
		break;
	case 7:
		new_csr_val = csr_val & ~rs1_num;
		break;
	default:
		return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
	};

	if (do_write && sbi_emulate_csr_write(csr_num, hartid, regs,
					      scratch, new_csr_val))
		return truly_illegal_insn(insn, hartid, mcause, regs, scratch);

	SET_RD(insn, regs, csr_val);

	regs->mepc += 4;

	return 0;
}

static int sbi_emulate_float_load(ulong insn, u32 hartid, ulong mcause,
                              struct sbi_trap_regs *regs,
                              struct sbi_scratch *scratch)
{
        struct sbi_trap_info trap;

        trap.epc = regs->mepc;
        trap.cause = mcause;
        trap.tval = insn;


        if(csr_read(CSR_MSTATUS) & MSTATUS_FS)
        {
                if((insn & MASK_FLD) == MATCH_FLD)
                {
                        uintptr_t addr = GET_RS1(insn, regs) + IMM_I(insn);
                        SET_F64_RD(insn, regs, sbi_load_u64((void *)addr, scratch, &trap));
                        regs->mepc = regs->mepc + 4;
			return 0;
                }
        }
                        return truly_illegal_insn(insn, hartid, mcause, regs,
                                                scratch);


}

static illegal_insn_func illegal_insn_table[32] = {
	truly_illegal_insn, /* 0 */
	sbi_emulate_float_load,//truly_illegal_insn, /* 1 */
	truly_illegal_insn, /* 2 */
	truly_illegal_insn, /* 3 */
	truly_illegal_insn, /* 4 */
	truly_illegal_insn, /* 5 */
	truly_illegal_insn, /* 6 */
	truly_illegal_insn, /* 7 */
	truly_illegal_insn, /* 8 */
	truly_illegal_insn, /* 9 */
	truly_illegal_insn, /* 10 */
	truly_illegal_insn, /* 11 */
	truly_illegal_insn, /* 12 */
	truly_illegal_insn, /* 13 */
	truly_illegal_insn, /* 14 */
	truly_illegal_insn, /* 15 */
	truly_illegal_insn, /* 16 */
	truly_illegal_insn, /* 17 */
	truly_illegal_insn, /* 18 */
	truly_illegal_insn, /* 19 */
	truly_illegal_insn, /* 20 */
	truly_illegal_insn, /* 21 */
	truly_illegal_insn, /* 22 */
	truly_illegal_insn, /* 23 */
	truly_illegal_insn, /* 24 */
	truly_illegal_insn, /* 25 */
	truly_illegal_insn, /* 26 */
	truly_illegal_insn, /* 27 */
	system_opcode_insn, /* 28 */
	truly_illegal_insn, /* 29 */
	truly_illegal_insn, /* 30 */
	truly_illegal_insn  /* 31 */
};

int sbi_emulate_float_load_C(ulong insn, u32 hartid, ulong mcause,
				struct sbi_trap_regs *regs,
				struct sbi_scratch *scratch)
{

  struct sbi_trap_info trap;

  regs->mepc = regs->mepc + 2;

  trap.epc = regs->mepc;
  trap.cause = mcause;
  trap.tval = insn;

  if ((insn & INSN_MASK_C_FLD) == INSN_MATCH_C_FLD) {
    uintptr_t addr = GET_RS1S(insn, regs) + RVC_LD_IMM(insn);
    SET_F64_RD(RVC_RS2S(insn) << SH_RD, regs, sbi_load_u64((void *)addr, scratch, &trap));
    return 0;
  } 
  else if ((insn & INSN_MASK_C_FLDSP) == INSN_MATCH_C_FLDSP) {
    uintptr_t addr = GET_SP(regs) + RVC_LDSP_IMM(insn);
    SET_F64_RD(insn, regs, sbi_load_u64((void *)addr, scratch, &trap));
    return 0;
  } 
  else
  	return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
}



int sbi_illegal_insn_handler(u32 hartid, ulong mcause,
			     struct sbi_trap_regs *regs,
			     struct sbi_scratch *scratch)
{
	ulong insn = csr_read(CSR_MTVAL);
	struct sbi_trap_info uptrap;
	
	if (unlikely((insn & 3) != 3)) {
		if (insn == 0) {
			insn = sbi_get_insn(regs->mepc, scratch, &uptrap);
			if (uptrap.cause) {
				uptrap.epc = regs->mepc;
				return sbi_trap_redirect(regs, &uptrap,
							 scratch);
			}
		}
		if ((insn & 3) != 3)
		{
			if ( ((insn & INSN_MASK_C_FLD) == INSN_MATCH_C_FLD) || ((insn & INSN_MASK_C_FLDSP) == INSN_MATCH_C_FLDSP) )
			{
				return sbi_emulate_float_load_C(insn, hartid, mcause, regs, scratch);
			}
			else
				return truly_illegal_insn(insn, hartid, mcause, regs,
							  scratch);
		}
	}

	return illegal_insn_table[(insn & 0x7c) >> 2](insn, hartid, mcause,
						      regs, scratch);
}
