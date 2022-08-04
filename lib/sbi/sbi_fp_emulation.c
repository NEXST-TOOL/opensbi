/*
 * SPDX-License-Identifier: BSD-2-Clause
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

#include "../softfloat/softfloat.h"
#include "../softfloat/internals.h"

#define MASK_FMV_D_X  0xfff0707f
#define MATCH_FMV_D_X 0xf2000053
#define MASK_FMV_W_X  0xfff0707f
#define MATCH_FMV_W_X 0xf0000053

#define MATCH_FMV_X_W 0xe0000053
#define MASK_FMV_X_W 0xfff0707f
#define MATCH_FCLASS_S 0xe0001053
#define MASK_FCLASS_S  0xfff0707f
#define MATCH_FMV_X_D 0xe2000053
#define MASK_FMV_X_D  0xfff0707f
#define MATCH_FCLASS_D 0xe2001053
#define MASK_FCLASS_D  0xfff0707f


#define f32(x) ((float32_t){ .v = x })
#define f64(x) ((float64_t){ .v = x })

typedef int (*fp_emulation_func)(ulong insn, u32 hartid, ulong mcause,
				 struct sbi_trap_regs *regs,
				 struct sbi_scratch *scratch);

#define DECLARE_FP_EMULATION_FUNC(name) int name(ulong insn, u32 hartid, ulong mcause, \
                              struct sbi_trap_regs *regs, \
                              struct sbi_scratch *scratch)

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


DECLARE_FP_EMULATION_FUNC(emulate_fmv_fi)
{
  uintptr_t rs1 = GET_RS1(insn, regs);
  //sbi_printf("\n\r into emulate_fmv_fi!\n\r");
  if ((insn & MASK_FMV_W_X) == MATCH_FMV_W_X)
        SET_F32_RD(insn, regs, rs1);
#if __riscv_xlen == 64
  else if ((insn & MASK_FMV_D_X) == MATCH_FMV_D_X)
        SET_F64_RD(insn, regs, rs1);
#endif
  else
        return truly_illegal_insn(insn, hartid, mcause, regs, scratch);

  return 0;
}

DECLARE_FP_EMULATION_FUNC(emulate_fmv_if)
{
  uintptr_t result;
  if ((insn >> 20) & 0x1f)
        return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
  if (GET_PRECISION(insn) == PRECISION_S) {
    result = GET_F32_RS1(insn, regs);
    switch (GET_RM(insn)) {
      case GET_RM(MATCH_FMV_X_W): break;
      case GET_RM(MATCH_FCLASS_S): result = f32_classify(f32(result)); break;
      default: return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
    }
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    result = GET_F64_RS1(insn, regs);
    switch (GET_RM(insn)) {
      case GET_RM(MATCH_FMV_X_D): break;
      case GET_RM(MATCH_FCLASS_D): result = f64_classify(f64(result)); break;
      default: return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
    }
  } else {
        return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
  }

  SET_FS_DIRTY();
  SET_RD(insn, regs, result);
  return 0;
}


DECLARE_FP_EMULATION_FUNC(emulate_fcmp)
{
  int rm = GET_RM(insn);
  if (rm >= 3)
        return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
  //else
    //sbi_printf("\n\r into emulate_fcmp!\n\r");
//    while(1);
//    return 0;
  uintptr_t result;
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    if (rm != 1)
      result = f32_eq(f32(rs1), f32(rs2));
    if (rm == 1 || (rm == 0 && !result))
      result = f32_lt(f32(rs1), f32(rs2));
    goto success;
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    if (rm != 1)
      result = f64_eq(f64(rs1), f64(rs2));
    if (rm == 1 || (rm == 0 && !result))
      result = f64_lt(f64(rs1), f64(rs2));
    goto success;
  }
  return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
success:
  SET_FS_DIRTY();
  SET_RD(insn, regs, result);
  return 0;
}

DECLARE_FP_EMULATION_FUNC(emulate_fcvt_ff)
{
  int rs2_num = (insn >> 20) & 0x1f;
  if (GET_PRECISION(insn) == PRECISION_S) {
    if (rs2_num != 1)
      return truly_illegal_insn(insn, hartid, mcause, regs, scratch);  
  SET_F32_RD(insn, regs, f64_to_f32(f64(GET_F64_RS1(insn, regs))).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    if (rs2_num != 0)
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
    SET_F64_RD(insn, regs, f32_to_f64(f32(GET_F32_RS1(insn, regs))).v);
  } else {
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
  }
  return 0;
}

DECLARE_FP_EMULATION_FUNC(emulate_fcvt_fi)
{
  if (GET_PRECISION(insn) != PRECISION_S && GET_PRECISION(insn) != PRECISION_D)
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);

  int negative = 0;
  uint64_t uint_val = GET_RS1(insn, regs);

  switch ((insn >> 20) & 0x1f)
  {
    case 0: // int32
      negative = (int32_t)uint_val < 0;
      uint_val = (uint32_t)(negative ? -uint_val : uint_val);
      break;
    case 1: // uint32
      uint_val = (uint32_t)uint_val;
      break;
#if __riscv_xlen == 64
    case 2: // int64
      negative = (int64_t)uint_val < 0;
      uint_val = negative ? -uint_val : uint_val;
    case 3: // uint64
      break;
#endif
    default:
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
  }

  uint64_t float64 = ui64_to_f64(uint_val).v;
  if (negative)
    float64 ^= INT64_MIN;

  if (GET_PRECISION(insn) == PRECISION_S)
    SET_F32_RD(insn, regs, f64_to_f32(f64(float64)).v);
  else
    SET_F64_RD(insn, regs, float64);

  return 0;
}

DECLARE_FP_EMULATION_FUNC(emulate_fcvt_if)
{
  int rs2_num = (insn >> 20) & 0x1f;
#if __riscv_xlen == 64
  if (rs2_num >= 4)
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
#else
  if (rs2_num >= 2)
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
#endif

  int64_t float64;
  if (GET_PRECISION(insn) == PRECISION_S)
    float64 = f32_to_f64(f32(GET_F32_RS1(insn, regs))).v;
  else if (GET_PRECISION(insn) == PRECISION_D)
    float64 = GET_F64_RS1(insn, regs);
  else
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);

  int negative = 0;
  if (float64 < 0) {
    negative = 1;
    float64 ^= INT64_MIN;
  }
  uint64_t uint_val = f64_to_ui64(f64(float64), softfloat_roundingMode, true);
  uint64_t result, limit, limit_result;

  switch (rs2_num)
  {
    case 0: // int32
      if (negative) {
        result = (int32_t)-uint_val;
        limit_result = limit = (uint32_t)INT32_MIN;
      } else {
        result = (int32_t)uint_val;
        limit_result = limit = INT32_MAX;
      }
      break;

    case 1: // uint32
      limit = limit_result = UINT32_MAX;
      if (negative)
        result = limit = 0;
      else
        result = (uint32_t)uint_val;
      break;

    case 2: // int32
      if (negative) {
        result = (int64_t)-uint_val;
        limit_result = limit = (uint64_t)INT64_MIN;
      } else {
        result = (int64_t)uint_val;
        limit_result = limit = INT64_MAX;
      }
      break;

    case 3: // uint64
      limit = limit_result = UINT64_MAX;
      if (negative)
        result = limit = 0;
      else
        result = (uint64_t)uint_val;
      break;

    default:
      __builtin_unreachable();
  }

  if (uint_val > limit) {
    result = limit_result;
    softfloat_raiseFlags(softfloat_flag_invalid);
  }

  SET_FS_DIRTY();
  SET_RD(insn, regs, result);
  return 0;
}

//int emulate_any_fadd(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, ulong insn, int32_t neg_b)
int emulate_any_fadd(ulong insn, u32 hartid, ulong mcause, struct sbi_trap_regs *regs, struct sbi_scratch *scratch, int32_t neg_b)

{
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs) ^ neg_b;
    SET_F32_RD(insn, regs, f32_add(f32(rs1), f32(rs2)).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs) ^ ((uint64_t)neg_b << 32);
    SET_F64_RD(insn, regs, f64_add(f64(rs1), f64(rs2)).v);
  } else {
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
  }
  return 0;
}

DECLARE_FP_EMULATION_FUNC(emulate_fadd)
{
  //return emulate_any_fadd(regs, mcause, mepc, mstatus, insn, 0);
  return emulate_any_fadd(insn, hartid, mcause, regs, scratch, 0);
}

DECLARE_FP_EMULATION_FUNC(emulate_fsub)
{
  //return emulate_any_fadd(regs, mcause, mepc, mstatus, insn, INT32_MIN);
  return emulate_any_fadd(insn, hartid, mcause, regs, scratch, INT32_MIN);
}

DECLARE_FP_EMULATION_FUNC(emulate_fmul)
{
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    SET_F32_RD(insn, regs, f32_mul(f32(rs1), f32(rs2)).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    SET_F64_RD(insn, regs, f64_mul(f64(rs1), f64(rs2)).v);
  } else {
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
  }
  return 0;
}

DECLARE_FP_EMULATION_FUNC(emulate_fdiv)
{
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    SET_F32_RD(insn, regs, f32_div(f32(rs1), f32(rs2)).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    SET_F64_RD(insn, regs, f64_div(f64(rs1), f64(rs2)).v);
  } else {
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
  }
  return 0;
}

DECLARE_FP_EMULATION_FUNC(emulate_fmadd)
{
  // if FPU is disabled, punt back to the OS
  if (unlikely((regs->mstatus & MSTATUS_FS) == 0))
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);

  bool negA = (insn >> 3) & 1;
  bool negC = (insn >> 2) & 1;
  SETUP_STATIC_ROUNDING(insn);
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs) ^ (negA ? INT32_MIN : 0);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    uint32_t rs3 = GET_F32_RS3(insn, regs) ^ (negC ? INT32_MIN : 0);
    SET_F32_RD(insn, regs, softfloat_mulAddF32(rs1, rs2, rs3, 0).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs) ^ (negA ? INT64_MIN : 0);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    uint64_t rs3 = GET_F64_RS3(insn, regs) ^ (negC ? INT64_MIN : 0);
    SET_F64_RD(insn, regs, softfloat_mulAddF64(rs1, rs2, rs3, 0).v);
  } else {
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
  }
  return 0;
}

DECLARE_FP_EMULATION_FUNC(emulate_fmin)
{
  int rm = GET_RM(insn);
  if (rm >= 2)
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);

  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    uint32_t arg1 = rm ? rs2 : rs1;
    uint32_t arg2 = rm ? rs1 : rs2;
    int use_rs1 = f32_lt_quiet(f32(arg1), f32(arg2)) || isNaNF32UI(rs2);
    SET_F32_RD(insn, regs, use_rs1 ? rs1 : rs2);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    uint64_t arg1 = rm ? rs2 : rs1;
    uint64_t arg2 = rm ? rs1 : rs2;
    int use_rs1 = f64_lt_quiet(f64(arg1), f64(arg2)) || isNaNF64UI(rs2);
    SET_F64_RD(insn, regs, use_rs1 ? rs1 : rs2);
  } else {
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
  }
  return 0;
}

DECLARE_FP_EMULATION_FUNC(emulate_fsqrt)
{
  if ((insn >> 20) & 0x1f)
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);

  if (GET_PRECISION(insn) == PRECISION_S) {
    SET_F32_RD(insn, regs, f32_sqrt(f32(GET_F32_RS1(insn, regs))).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    SET_F64_RD(insn, regs, f64_sqrt(f64(GET_F64_RS1(insn, regs))).v);
  } else {
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
  }
  return 0;
}

DECLARE_FP_EMULATION_FUNC(emulate_fsgnj)
{
  int rm = GET_RM(insn);
  if (rm >= 3)
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);

  #define DO_FSGNJ(rs1, rs2, rm) ({ \
    typeof(rs1) rs1_sign = (rs1) >> (8*sizeof(rs1)-1); \
    typeof(rs1) rs2_sign = (rs2) >> (8*sizeof(rs1)-1); \
    rs1_sign &= (rm) >> 1; \
    rs1_sign ^= (rm) ^ rs2_sign; \
    ((rs1) << 1 >> 1) | (rs1_sign << (8*sizeof(rs1)-1)); })

  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    SET_F32_RD(insn, regs, DO_FSGNJ(rs1, rs2, rm));
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    SET_F64_RD(insn, regs, DO_FSGNJ(rs1, rs2, rm));
  } else {
    return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
  }
  return 0;
}


static fp_emulation_func fp_emulation_func_table[32] = {
        emulate_fadd, //truly_illegal_insn, /* 0 */
        emulate_fsub, //truly_illegal_insn, /* 1 */
        emulate_fmul, //truly_illegal_insn, /* 2 */
        emulate_fdiv, //truly_illegal_insn, /* 3 */
        emulate_fsgnj, //truly_illegal_insn, /* 4 */
        emulate_fmin, //truly_illegal_insn, /* 5 */
        truly_illegal_insn, /* 6 */
        truly_illegal_insn, /* 7 */
        emulate_fcvt_ff, //truly_illegal_insn, /* 8 */
        truly_illegal_insn, /* 9 */
        truly_illegal_insn, /* 10 */
        emulate_fsqrt, //truly_illegal_insn, /* 11 */
        truly_illegal_insn, /* 12 */
        truly_illegal_insn, /* 13 */
        truly_illegal_insn, /* 14 */
        truly_illegal_insn, /* 15 */
        truly_illegal_insn, /* 16 */
        truly_illegal_insn, /* 17 */
        truly_illegal_insn, /* 18 */
        truly_illegal_insn, /* 19 */
        emulate_fcmp, //truly_illegal_insn, /* 20 */
        truly_illegal_insn, /* 21 */
        truly_illegal_insn, /* 22 */
        truly_illegal_insn, /* 23 */
        emulate_fcvt_if, //truly_illegal_insn, /* 24 */
        truly_illegal_insn, /* 25 */
        emulate_fcvt_fi, //truly_illegal_insn, /* 26 */
        truly_illegal_insn, /* 27 */
        emulate_fmv_if, //truly_illegal_insn, /* 28 */
        truly_illegal_insn, /* 29 */
        emulate_fmv_fi, //truly_illegal_insn, /* 30 */
        truly_illegal_insn  /* 31 */
};
 

DECLARE_FP_EMULATION_FUNC(emulate_fp)
{
/*  asm (".pushsection .rodata\n"
       "fp_emulation_table:\n"
       "  .word emulate_fadd - fp_emulation_table\n"
       "  .word emulate_fsub - fp_emulation_table\n"
       "  .word emulate_fmul - fp_emulation_table\n"
       "  .word emulate_fdiv - fp_emulation_table\n"
       "  .word emulate_fsgnj - fp_emulation_table\n"
       "  .word emulate_fmin - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word emulate_fcvt_ff - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word emulate_fsqrt - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word emulate_fcmp - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word emulate_fcvt_if - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word emulate_fcvt_fi - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word emulate_fmv_if - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word emulate_fmv_fi - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .popsection");
*/
  // if FPU is disabled, punt back to the OS
  if (unlikely((regs->mstatus & MSTATUS_FS) == 0))
	return truly_illegal_insn(insn, hartid, mcause, regs, scratch);

  regs->mepc = regs->mepc + 4;
//  sbi_printf("\n\r Debug: into fp_emulation()\n\r");
//  int32_t* pf = (void*)fp_emulation_func_table + ((insn >> 25) & 0x7c);
//  fp_emulation_func f = (fp_emulation_func)((void*)fp_emulation_func_table + *pf);
//  sbi_printf("\n\r pf: %p\n\r",(void*)pf);
//  sbi_printf("\n\r f: %p\n\r",(void*)f);
//  while(1);
//  SETUP_STATIC_ROUNDING(insn);
//  return emulate_fmv_fi(insn, hartid, mcause, regs, scratch);
//  return f(insn, hartid, mcause, regs, scratch);
//  sbi_printf("\n\r fp_emulation_func_table: %p\n\r",(void*)fp_emulation_func_table);
//  sbi_printf("\n\r        insn: %lx \n\r",insn);
//  sbi_printf("\n\r func[((insn >> 25) & 0x7c)/4]: %p\n\r",(void*)fp_emulation_func_table[((insn >> 25) & 0x7c)/4]);
//  sbi_printf("\n\r func[30]: %p\n\r",(void*)fp_emulation_func_table[30]);
//  sbi_printf("\n\r regs->mepc:  %lx \n\r",regs->mepc);
  return fp_emulation_func_table[((insn >> 25) & 0x7c)/4](insn, hartid, mcause, regs, scratch);
}


/*
void emulate_any_fadd(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn, int32_t neg_b)
{
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs) ^ neg_b;
    SET_F32_RD(insn, regs, f32_add(f32(rs1), f32(rs2)).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs) ^ ((uint64_t)neg_b << 32);
    SET_F64_RD(insn, regs, f64_add(f64(rs1), f64(rs2)).v);
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_FP_EMULATION_FUNC(emulate_fadd)
{
  return emulate_any_fadd(regs, mcause, mepc, mstatus, insn, 0);
}

DECLARE_FP_EMULATION_FUNC(emulate_fsub)
{
  return emulate_any_fadd(regs, mcause, mepc, mstatus, insn, INT32_MIN);
}

DECLARE_FP_EMULATION_FUNC(emulate_fmul)
{
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    SET_F32_RD(insn, regs, f32_mul(f32(rs1), f32(rs2)).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    SET_F64_RD(insn, regs, f64_mul(f64(rs1), f64(rs2)).v);
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_FP_EMULATION_FUNC(emulate_fdiv)
{
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    SET_F32_RD(insn, regs, f32_div(f32(rs1), f32(rs2)).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    SET_F64_RD(insn, regs, f64_div(f64(rs1), f64(rs2)).v);
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_FP_EMULATION_FUNC(emulate_fsqrt)
{
  if ((insn >> 20) & 0x1f)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  if (GET_PRECISION(insn) == PRECISION_S) {
    SET_F32_RD(insn, regs, f32_sqrt(f32(GET_F32_RS1(insn, regs))).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    SET_F64_RD(insn, regs, f64_sqrt(f64(GET_F64_RS1(insn, regs))).v);
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_FP_EMULATION_FUNC(emulate_fsgnj)
{
  int rm = GET_RM(insn);
  if (rm >= 3)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  #define DO_FSGNJ(rs1, rs2, rm) ({ \
    typeof(rs1) rs1_sign = (rs1) >> (8*sizeof(rs1)-1); \
    typeof(rs1) rs2_sign = (rs2) >> (8*sizeof(rs1)-1); \
    rs1_sign &= (rm) >> 1; \
    rs1_sign ^= (rm) ^ rs2_sign; \
    ((rs1) << 1 >> 1) | (rs1_sign << (8*sizeof(rs1)-1)); })

  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    SET_F32_RD(insn, regs, DO_FSGNJ(rs1, rs2, rm));
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    SET_F64_RD(insn, regs, DO_FSGNJ(rs1, rs2, rm));
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_FP_EMULATION_FUNC(emulate_fmin)
{
  int rm = GET_RM(insn);
  if (rm >= 2)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    uint32_t arg1 = rm ? rs2 : rs1;
    uint32_t arg2 = rm ? rs1 : rs2;
    int use_rs1 = f32_lt_quiet(f32(arg1), f32(arg2)) || isNaNF32UI(rs2);
    SET_F32_RD(insn, regs, use_rs1 ? rs1 : rs2);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    uint64_t arg1 = rm ? rs2 : rs1;
    uint64_t arg2 = rm ? rs1 : rs2;
    int use_rs1 = f64_lt_quiet(f64(arg1), f64(arg2)) || isNaNF64UI(rs2);
    SET_F64_RD(insn, regs, use_rs1 ? rs1 : rs2);
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_FP_EMULATION_FUNC(emulate_fcvt_ff)
{
  int rs2_num = (insn >> 20) & 0x1f;
  if (GET_PRECISION(insn) == PRECISION_S) {
    if (rs2_num != 1)
      return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
    SET_F32_RD(insn, regs, f64_to_f32(f64(GET_F64_RS1(insn, regs))).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    if (rs2_num != 0)
      return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
    SET_F64_RD(insn, regs, f32_to_f64(f32(GET_F32_RS1(insn, regs))).v);
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_FP_EMULATION_FUNC(emulate_fcvt_fi)
{
  if (GET_PRECISION(insn) != PRECISION_S && GET_PRECISION(insn) != PRECISION_D)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  int negative = 0;
  uint64_t uint_val = GET_RS1(insn, regs);

  switch ((insn >> 20) & 0x1f)
  {
    case 0: // int32
      negative = (int32_t)uint_val < 0;
      uint_val = (uint32_t)(negative ? -uint_val : uint_val);
      break;
    case 1: // uint32
      uint_val = (uint32_t)uint_val;
      break;
#if __riscv_xlen == 64
    case 2: // int64
      negative = (int64_t)uint_val < 0;
      uint_val = negative ? -uint_val : uint_val;
    case 3: // uint64
      break;
#endif
    default:
      return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }

  uint64_t float64 = ui64_to_f64(uint_val).v;
  if (negative)
    float64 ^= INT64_MIN;

  if (GET_PRECISION(insn) == PRECISION_S)
    SET_F32_RD(insn, regs, f64_to_f32(f64(float64)).v);
  else
    SET_F64_RD(insn, regs, float64);
}

DECLARE_FP_EMULATION_FUNC(emulate_fcvt_if)
{
  int rs2_num = (insn >> 20) & 0x1f;
#if __riscv_xlen == 64
  if (rs2_num >= 4)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
#else
  if (rs2_num >= 2)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
#endif

  int64_t float64;
  if (GET_PRECISION(insn) == PRECISION_S)
    float64 = f32_to_f64(f32(GET_F32_RS1(insn, regs))).v;
  else if (GET_PRECISION(insn) == PRECISION_D)
    float64 = GET_F64_RS1(insn, regs);
  else
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  int negative = 0;
  if (float64 < 0) {
    negative = 1;
    float64 ^= INT64_MIN;
  }
  uint64_t uint_val = f64_to_ui64(f64(float64), softfloat_roundingMode, true);
  uint64_t result, limit, limit_result;

  switch (rs2_num)
  {
    case 0: // int32
      if (negative) {
        result = (int32_t)-uint_val;
        limit_result = limit = (uint32_t)INT32_MIN;
      } else {
        result = (int32_t)uint_val;
        limit_result = limit = INT32_MAX;
      }
      break;

    case 1: // uint32
      limit = limit_result = UINT32_MAX;
      if (negative)
        result = limit = 0;
      else
        result = (uint32_t)uint_val;
      break;

    case 2: // int32
      if (negative) {
        result = (int64_t)-uint_val;
        limit_result = limit = (uint64_t)INT64_MIN;
      } else {
        result = (int64_t)uint_val;
        limit_result = limit = INT64_MAX;
      }
      break;

    case 3: // uint64
      limit = limit_result = UINT64_MAX;
      if (negative)
        result = limit = 0;
      else
        result = (uint64_t)uint_val;
      break;

    default:
      __builtin_unreachable();
  }

  if (uint_val > limit) {
    result = limit_result;
    softfloat_raiseFlags(softfloat_flag_invalid);
  }

  SET_FS_DIRTY();
  SET_RD(insn, regs, result);
}

DECLARE_FP_EMULATION_FUNC(emulate_fcmp)
{
  int rm = GET_RM(insn);
  if (rm >= 3)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  uintptr_t result;
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    if (rm != 1)
      result = f32_eq(f32(rs1), f32(rs2));
    if (rm == 1 || (rm == 0 && !result))
      result = f32_lt(f32(rs1), f32(rs2));
    goto success;
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    if (rm != 1)
      result = f64_eq(f64(rs1), f64(rs2));
    if (rm == 1 || (rm == 0 && !result))
      result = f64_lt(f64(rs1), f64(rs2));
    goto success;
  }
  return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
success:
  SET_FS_DIRTY();
  SET_RD(insn, regs, result);
}
*/
/*
DECLARE_FP_EMULATION_FUNC(emulate_fmv_if)
{
  uintptr_t result;
  if ((insn >> 20) & 0x1f)
	return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
  if (GET_PRECISION(insn) == PRECISION_S) {
    result = GET_F32_RS1(insn, regs);
    switch (GET_RM(insn)) {
      case GET_RM(MATCH_FMV_X_W): break;
      case GET_RM(MATCH_FCLASS_S): result = f32_classify(f32(result)); break;
      default: return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
    }
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    result = GET_F64_RS1(insn, regs);
    switch (GET_RM(insn)) {
      case GET_RM(MATCH_FMV_X_D): break;
      case GET_RM(MATCH_FCLASS_D): result = f64_classify(f64(result)); break;
      default: return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
    }
  } else {
	return truly_illegal_insn(insn, hartid, mcause, regs, scratch);
  }

  SET_FS_DIRTY();
  SET_RD(insn, regs, result);
}

DECLARE_FP_EMULATION_FUNC(emulate_fmv_fi)
{
  uintptr_t rs1 = GET_RS1(insn, regs);

  if ((insn & MASK_FMV_W_X) == MATCH_FMV_W_X)
	SET_F32_RD(insn, regs, rs1);
#if __riscv_xlen == 64
  else if ((insn & MASK_FMV_D_X) == MATCH_FMV_D_X)
	SET_F64_RD(insn, regs, rs1);
#endif
  else
	return truly_illegal_insn(insn, hartid, mcause, regs, scratch);

  return 0;
}

DECLARE_FP_EMULATION_FUNC(emulate_fmadd)
{
  // if FPU is disabled, punt back to the OS
  if (unlikely((mstatus & MSTATUS_FS) == 0))
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  bool negA = (insn >> 3) & 1;
  bool negC = (insn >> 2) & 1;
  SETUP_STATIC_ROUNDING(insn);
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs) ^ (negA ? INT32_MIN : 0);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    uint32_t rs3 = GET_F32_RS3(insn, regs) ^ (negC ? INT32_MIN : 0);
    SET_F32_RD(insn, regs, softfloat_mulAddF32(rs1, rs2, rs3, 0).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs) ^ (negA ? INT64_MIN : 0);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    uint64_t rs3 = GET_F64_RS3(insn, regs) ^ (negC ? INT64_MIN : 0);
    SET_F64_RD(insn, regs, softfloat_mulAddF64(rs1, rs2, rs3, 0).v);
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}
*/
