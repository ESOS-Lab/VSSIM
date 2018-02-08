/* ia64-dis.c -- Disassemble ia64 instructions
   Copyright 1998, 1999, 2000, 2002 Free Software Foundation, Inc.
   Contributed by David Mosberger-Tang <davidm@hpl.hp.com>

   This file is part of GDB, GAS, and the GNU binutils.

   GDB, GAS, and the GNU binutils are free software; you can redistribute
   them and/or modify them under the terms of the GNU General Public
   License as published by the Free Software Foundation; either version
   2, or (at your option) any later version.

   GDB, GAS, and the GNU binutils are distributed in the hope that they
   will be useful, but WITHOUT ANY WARRANTY; without even the implied
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this file; see the file COPYING.  If not, see
   <http://www.gnu.org/licenses/>. */

#include "qemu/osdep.h"

#include "disas/bfd.h"

/* ia64.h -- Header file for ia64 opcode table
   Copyright (C) 1998, 1999, 2000, 2002, 2005, 2006
   Free Software Foundation, Inc.
   Contributed by David Mosberger-Tang <davidm@hpl.hp.com> */


typedef uint64_t ia64_insn;

enum ia64_insn_type
  {
    IA64_TYPE_NIL = 0,	/* illegal type */
    IA64_TYPE_A,	/* integer alu (I- or M-unit) */
    IA64_TYPE_I,	/* non-alu integer (I-unit) */
    IA64_TYPE_M,	/* memory (M-unit) */
    IA64_TYPE_B,	/* branch (B-unit) */
    IA64_TYPE_F,	/* floating-point (F-unit) */
    IA64_TYPE_X,	/* long encoding (X-unit) */
    IA64_TYPE_DYN,	/* Dynamic opcode */
    IA64_NUM_TYPES
  };

enum ia64_unit
  {
    IA64_UNIT_NIL = 0,	/* illegal unit */
    IA64_UNIT_I,	/* integer unit */
    IA64_UNIT_M,	/* memory unit */
    IA64_UNIT_B,	/* branching unit */
    IA64_UNIT_F,	/* floating-point unit */
    IA64_UNIT_L,	/* long "unit" */
    IA64_UNIT_X,	/* may be integer or branch unit */
    IA64_NUM_UNITS
  };

/* Changes to this enumeration must be propagated to the operand table in
   bfd/cpu-ia64-opc.c
 */
enum ia64_opnd
  {
    IA64_OPND_NIL,	/* no operand---MUST BE FIRST!*/

    /* constants */
    IA64_OPND_AR_CSD,	/* application register csd (ar.csd) */
    IA64_OPND_AR_CCV,	/* application register ccv (ar.ccv) */
    IA64_OPND_AR_PFS,	/* application register pfs (ar.pfs) */
    IA64_OPND_C1,	/* the constant 1 */
    IA64_OPND_C8,	/* the constant 8 */
    IA64_OPND_C16,	/* the constant 16 */
    IA64_OPND_GR0,	/* gr0 */
    IA64_OPND_IP,	/* instruction pointer (ip) */
    IA64_OPND_PR,	/* predicate register (pr) */
    IA64_OPND_PR_ROT,	/* rotating predicate register (pr.rot) */
    IA64_OPND_PSR,	/* processor status register (psr) */
    IA64_OPND_PSR_L,	/* processor status register L (psr.l) */
    IA64_OPND_PSR_UM,	/* processor status register UM (psr.um) */

    /* register operands: */
    IA64_OPND_AR3,	/* third application register # (bits 20-26) */
    IA64_OPND_B1,	/* branch register # (bits 6-8) */
    IA64_OPND_B2,	/* branch register # (bits 13-15) */
    IA64_OPND_CR3,	/* third control register # (bits 20-26) */
    IA64_OPND_F1,	/* first floating-point register # */
    IA64_OPND_F2,	/* second floating-point register # */
    IA64_OPND_F3,	/* third floating-point register # */
    IA64_OPND_F4,	/* fourth floating-point register # */
    IA64_OPND_P1,	/* first predicate # */
    IA64_OPND_P2,	/* second predicate # */
    IA64_OPND_R1,	/* first register # */
    IA64_OPND_R2,	/* second register # */
    IA64_OPND_R3,	/* third register # */
    IA64_OPND_R3_2,	/* third register # (limited to gr0-gr3) */

    /* memory operands: */
    IA64_OPND_MR3,	/* memory at addr of third register # */

    /* indirect operands: */
    IA64_OPND_CPUID_R3,	/* cpuid[reg] */
    IA64_OPND_DBR_R3,	/* dbr[reg] */
    IA64_OPND_DTR_R3,	/* dtr[reg] */
    IA64_OPND_ITR_R3,	/* itr[reg] */
    IA64_OPND_IBR_R3,	/* ibr[reg] */
    IA64_OPND_MSR_R3,	/* msr[reg] */
    IA64_OPND_PKR_R3,	/* pkr[reg] */
    IA64_OPND_PMC_R3,	/* pmc[reg] */
    IA64_OPND_PMD_R3,	/* pmd[reg] */
    IA64_OPND_RR_R3,	/* rr[reg] */

    /* immediate operands: */
    IA64_OPND_CCNT5,	/* 5-bit count (31 - bits 20-24) */
    IA64_OPND_CNT2a,	/* 2-bit count (1 + bits 27-28) */
    IA64_OPND_CNT2b,	/* 2-bit count (bits 27-28): 1, 2, 3 */
    IA64_OPND_CNT2c,	/* 2-bit count (bits 30-31): 0, 7, 15, or 16 */
    IA64_OPND_CNT5,	/* 5-bit count (bits 14-18) */
    IA64_OPND_CNT6,	/* 6-bit count (bits 27-32) */
    IA64_OPND_CPOS6a,	/* 6-bit count (63 - bits 20-25) */
    IA64_OPND_CPOS6b,	/* 6-bit count (63 - bits 14-19) */
    IA64_OPND_CPOS6c,	/* 6-bit count (63 - bits 31-36) */
    IA64_OPND_IMM1,	/* signed 1-bit immediate (bit 36) */
    IA64_OPND_IMMU2,	/* unsigned 2-bit immediate (bits 13-14) */
    IA64_OPND_IMMU5b,	/* unsigned 5-bit immediate (32 + bits 14-18) */
    IA64_OPND_IMMU7a,	/* unsigned 7-bit immediate (bits 13-19) */
    IA64_OPND_IMMU7b,	/* unsigned 7-bit immediate (bits 20-26) */
    IA64_OPND_SOF,	/* 8-bit stack frame size */
    IA64_OPND_SOL,	/* 8-bit size of locals */
    IA64_OPND_SOR,	/* 6-bit number of rotating registers (scaled by 8) */
    IA64_OPND_IMM8,	/* signed 8-bit immediate (bits 13-19 & 36) */
    IA64_OPND_IMM8U4,	/* cmp4*u signed 8-bit immediate (bits 13-19 & 36) */
    IA64_OPND_IMM8M1,	/* signed 8-bit immediate -1 (bits 13-19 & 36) */
    IA64_OPND_IMM8M1U4,	/* cmp4*u signed 8-bit immediate -1 (bits 13-19 & 36)*/
    IA64_OPND_IMM8M1U8,	/* cmp*u signed 8-bit immediate -1 (bits 13-19 & 36) */
    IA64_OPND_IMMU9,	/* unsigned 9-bit immediate (bits 33-34, 20-26) */
    IA64_OPND_IMM9a,	/* signed 9-bit immediate (bits 6-12, 27, 36) */
    IA64_OPND_IMM9b,	/* signed 9-bit immediate (bits 13-19, 27, 36) */
    IA64_OPND_IMM14,	/* signed 14-bit immediate (bits 13-19, 27-32, 36) */
    IA64_OPND_IMM17,	/* signed 17-bit immediate (2*bits 6-12, 24-31, 36) */
    IA64_OPND_IMMU21,	/* unsigned 21-bit immediate (bits 6-25, 36) */
    IA64_OPND_IMM22,	/* signed 22-bit immediate (bits 13-19, 22-36) */
    IA64_OPND_IMMU24,	/* unsigned 24-bit immediate (bits 6-26, 31-32, 36) */
    IA64_OPND_IMM44,	/* signed 44-bit immediate (2^16*bits 6-32, 36) */
    IA64_OPND_IMMU62,	/* unsigned 62-bit immediate */
    IA64_OPND_IMMU64,	/* unsigned 64-bit immediate (lotsa bits...) */
    IA64_OPND_INC3,	/* signed 3-bit (bits 13-15): +/-1, 4, 8, 16 */
    IA64_OPND_LEN4,	/* 4-bit count (bits 27-30 + 1) */
    IA64_OPND_LEN6,	/* 6-bit count (bits 27-32 + 1) */
    IA64_OPND_MBTYPE4,	/* 4-bit mux type (bits 20-23) */
    IA64_OPND_MHTYPE8,	/* 8-bit mux type (bits 20-27) */
    IA64_OPND_POS6,	/* 6-bit count (bits 14-19) */
    IA64_OPND_TAG13,	/* signed 13-bit tag (ip + 16*bits 6-12, 33-34) */
    IA64_OPND_TAG13b,	/* signed 13-bit tag (ip + 16*bits 24-32) */
    IA64_OPND_TGT25,	/* signed 25-bit (ip + 16*bits 6-25, 36) */
    IA64_OPND_TGT25b,	/* signed 25-bit (ip + 16*bits 6-12, 20-32, 36) */
    IA64_OPND_TGT25c,	/* signed 25-bit (ip + 16*bits 13-32, 36) */
    IA64_OPND_TGT64,    /* 64-bit (ip + 16*bits 13-32, 36, 2-40(L)) */
    IA64_OPND_LDXMOV,	/* any symbol, generates R_IA64_LDXMOV.  */

    IA64_OPND_COUNT	/* # of operand types (MUST BE LAST!) */
  };

enum ia64_dependency_mode
{
  IA64_DV_RAW,
  IA64_DV_WAW,
  IA64_DV_WAR,
};

enum ia64_dependency_semantics
{
  IA64_DVS_NONE,
  IA64_DVS_IMPLIED,
  IA64_DVS_IMPLIEDF,
  IA64_DVS_DATA,
  IA64_DVS_INSTR,
  IA64_DVS_SPECIFIC,
  IA64_DVS_STOP,
  IA64_DVS_OTHER,
};

enum ia64_resource_specifier
{
  IA64_RS_ANY,
  IA64_RS_AR_K,
  IA64_RS_AR_UNAT,
  IA64_RS_AR, /* 8-15, 20, 22-23, 31, 33-35, 37-39, 41-43, 45-47, 67-111 */
  IA64_RS_ARb, /* 48-63, 112-127 */
  IA64_RS_BR,
  IA64_RS_CFM,
  IA64_RS_CPUID,
  IA64_RS_CR_IRR,
  IA64_RS_CR_LRR,
  IA64_RS_CR, /* 3-7,10-15,18,26-63,75-79,82-127 */
  IA64_RS_DBR,
  IA64_RS_FR,
  IA64_RS_FRb,
  IA64_RS_GR0,
  IA64_RS_GR,
  IA64_RS_IBR,
  IA64_RS_INSERVICE, /* CR[EOI] or CR[IVR] */
  IA64_RS_MSR,
  IA64_RS_PKR,
  IA64_RS_PMC,
  IA64_RS_PMD,
  IA64_RS_PR,  /* non-rotating, 1-15 */
  IA64_RS_PRr, /* rotating, 16-62 */
  IA64_RS_PR63,
  IA64_RS_RR,

  IA64_RS_ARX, /* ARs not in RS_AR or RS_ARb */
  IA64_RS_CRX, /* CRs not in RS_CR */
  IA64_RS_PSR, /* PSR bits */
  IA64_RS_RSE, /* implementation-specific RSE resources */
  IA64_RS_AR_FPSR,
};

enum ia64_rse_resource
{
  IA64_RSE_N_STACKED_PHYS,
  IA64_RSE_BOF,
  IA64_RSE_STORE_REG,
  IA64_RSE_LOAD_REG,
  IA64_RSE_BSPLOAD,
  IA64_RSE_RNATBITINDEX,
  IA64_RSE_CFLE,
  IA64_RSE_NDIRTY,
};

/* Information about a given resource dependency */
struct ia64_dependency
{
  /* Name of the resource */
  const char *name;
  /* Does this dependency need further specification? */
  enum ia64_resource_specifier specifier;
  /* Mode of dependency */
  enum ia64_dependency_mode mode;
  /* Dependency semantics */
  enum ia64_dependency_semantics semantics;
  /* Register index, if applicable (distinguishes AR, CR, and PSR deps) */
#define REG_NONE (-1)
  int regindex;
  /* Special info on semantics */
  const char *info;
};

/* Two arrays of indexes into the ia64_dependency table.
   chks are dependencies to check for conflicts when an opcode is
   encountered; regs are dependencies to register (mark as used) when an
   opcode is used.  chks correspond to readers (RAW) or writers (WAW or
   WAR) of a resource, while regs correspond to writers (RAW or WAW) and
   readers (WAR) of a resource.  */
struct ia64_opcode_dependency
{
  int nchks;
  const unsigned short *chks;
  int nregs;
  const unsigned short *regs;
};

/* encode/extract the note/index for a dependency */
#define RDEP(N,X) (((N)<<11)|(X))
#define NOTE(X) (((X)>>11)&0x1F)
#define DEP(X) ((X)&0x7FF)

/* A template descriptor describes the execution units that are active
   for each of the three slots.  It also specifies the location of
   instruction group boundaries that may be present between two slots.  */
struct ia64_templ_desc
  {
    int group_boundary;	/* 0=no boundary, 1=between slot 0 & 1, etc. */
    enum ia64_unit exec_unit[3];
    const char *name;
  };

/* The opcode table is an array of struct ia64_opcode.  */

struct ia64_opcode
  {
    /* The opcode name.  */
    const char *name;

    /* The type of the instruction: */
    enum ia64_insn_type type;

    /* Number of output operands: */
    int num_outputs;

    /* The opcode itself.  Those bits which will be filled in with
       operands are zeroes.  */
    ia64_insn opcode;

    /* The opcode mask.  This is used by the disassembler.  This is a
       mask containing ones indicating those bits which must match the
       opcode field, and zeroes indicating those bits which need not
       match (and are presumably filled in by operands).  */
    ia64_insn mask;

    /* An array of operand codes.  Each code is an index into the
       operand table.  They appear in the order which the operands must
       appear in assembly code, and are terminated by a zero.  */
    enum ia64_opnd operands[5];

    /* One bit flags for the opcode.  These are primarily used to
       indicate specific processors and environments support the
       instructions.  The defined values are listed below. */
    unsigned int flags;

    /* Used by ia64_find_next_opcode (). */
    short ent_index;

    /* Opcode dependencies. */
    const struct ia64_opcode_dependency *dependencies;
  };

/* Values defined for the flags field of a struct ia64_opcode.  */

#define IA64_OPCODE_FIRST	(1<<0)	/* must be first in an insn group */
#define IA64_OPCODE_X_IN_MLX	(1<<1)	/* insn is allowed in X slot of MLX */
#define IA64_OPCODE_LAST	(1<<2)	/* must be last in an insn group */
#define IA64_OPCODE_PRIV	(1<<3)	/* privileged instruct */
#define IA64_OPCODE_SLOT2	(1<<4)	/* insn allowed in slot 2 only */
#define IA64_OPCODE_NO_PRED	(1<<5)	/* insn cannot be predicated */
#define IA64_OPCODE_PSEUDO	(1<<6)	/* insn is a pseudo-op */
#define IA64_OPCODE_F2_EQ_F3	(1<<7)	/* constraint: F2 == F3 */
#define IA64_OPCODE_LEN_EQ_64MCNT	(1<<8)	/* constraint: LEN == 64-CNT */
#define IA64_OPCODE_MOD_RRBS    (1<<9)	/* modifies all rrbs in CFM */
#define IA64_OPCODE_POSTINC	(1<<10)	/* postincrement MR3 operand */

/* A macro to extract the major opcode from an instruction.  */
#define IA64_OP(i)	(((i) >> 37) & 0xf)

enum ia64_operand_class
  {
    IA64_OPND_CLASS_CST,	/* constant */
    IA64_OPND_CLASS_REG,	/* register */
    IA64_OPND_CLASS_IND,	/* indirect register */
    IA64_OPND_CLASS_ABS,	/* absolute value */
    IA64_OPND_CLASS_REL,	/* IP-relative value */
  };

/* The operands table is an array of struct ia64_operand.  */

struct ia64_operand
{
  enum ia64_operand_class class;

  /* Set VALUE as the operand bits for the operand of type SELF in the
     instruction pointed to by CODE.  If an error occurs, *CODE is not
     modified and the returned string describes the cause of the
     error.  If no error occurs, NULL is returned.  */
  const char *(*insert) (const struct ia64_operand *self, ia64_insn value,
			 ia64_insn *code);

  /* Extract the operand bits for an operand of type SELF from
     instruction CODE store them in *VALUE.  If an error occurs, the
     cause of the error is described by the string returned.  If no
     error occurs, NULL is returned.  */
  const char *(*extract) (const struct ia64_operand *self, ia64_insn code,
			  ia64_insn *value);

  /* A string whose meaning depends on the operand class.  */

  const char *str;

  struct bit_field
    {
      /* The number of bits in the operand.  */
      int bits;

      /* How far the operand is left shifted in the instruction.  */
      int shift;
    }
  field[4];		/* no operand has more than this many bit-fields */

  unsigned int flags;

  const char *desc;	/* brief description */
};

/* Values defined for the flags field of a struct ia64_operand.  */

/* Disassemble as signed decimal (instead of hex): */
#define IA64_OPND_FLAG_DECIMAL_SIGNED	(1<<0)
/* Disassemble as unsigned decimal (instead of hex): */
#define IA64_OPND_FLAG_DECIMAL_UNSIGNED	(1<<1)

#define NELEMS(a)	((int) (sizeof (a) / sizeof (a[0])))

static const char*
ins_rsvd (const struct ia64_operand *self ATTRIBUTE_UNUSED,
	  ia64_insn value ATTRIBUTE_UNUSED, ia64_insn *code ATTRIBUTE_UNUSED)
{
  return "internal error---this shouldn't happen";
}

static const char*
ext_rsvd (const struct ia64_operand *self ATTRIBUTE_UNUSED,
	  ia64_insn code ATTRIBUTE_UNUSED, ia64_insn *valuep ATTRIBUTE_UNUSED)
{
  return "internal error---this shouldn't happen";
}

static const char*
ins_const (const struct ia64_operand *self ATTRIBUTE_UNUSED,
	   ia64_insn value ATTRIBUTE_UNUSED, ia64_insn *code ATTRIBUTE_UNUSED)
{
  return 0;
}

static const char*
ext_const (const struct ia64_operand *self ATTRIBUTE_UNUSED,
	   ia64_insn code ATTRIBUTE_UNUSED, ia64_insn *valuep ATTRIBUTE_UNUSED)
{
  return 0;
}

static const char*
ins_reg (const struct ia64_operand *self, ia64_insn value, ia64_insn *code)
{
  if (value >= 1u << self->field[0].bits)
    return "register number out of range";

  *code |= value << self->field[0].shift;
  return 0;
}

static const char*
ext_reg (const struct ia64_operand *self, ia64_insn code, ia64_insn *valuep)
{
  *valuep = ((code >> self->field[0].shift)
	     & ((1u << self->field[0].bits) - 1));
  return 0;
}

static const char*
ins_immu (const struct ia64_operand *self, ia64_insn value, ia64_insn *code)
{
  ia64_insn new = 0;
  int i;

  for (i = 0; i < NELEMS (self->field) && self->field[i].bits; ++i)
    {
      new |= ((value & ((((ia64_insn) 1) << self->field[i].bits) - 1))
	      << self->field[i].shift);
      value >>= self->field[i].bits;
    }
  if (value)
    return "integer operand out of range";

  *code |= new;
  return 0;
}

static const char*
ext_immu (const struct ia64_operand *self, ia64_insn code, ia64_insn *valuep)
{
  uint64_t value = 0;
  int i, bits = 0, total = 0;

  for (i = 0; i < NELEMS (self->field) && self->field[i].bits; ++i)
    {
      bits = self->field[i].bits;
      value |= ((code >> self->field[i].shift)
		& ((((uint64_t) 1) << bits) - 1)) << total;
      total += bits;
    }
  *valuep = value;
  return 0;
}

static const char*
ins_immu5b (const struct ia64_operand *self, ia64_insn value,
	    ia64_insn *code)
{
  if (value < 32 || value > 63)
    return "value must be between 32 and 63";
  return ins_immu (self, value - 32, code);
}

static const char*
ext_immu5b (const struct ia64_operand *self, ia64_insn code,
	    ia64_insn *valuep)
{
  const char *result;

  result = ext_immu (self, code, valuep);
  if (result)
    return result;

  *valuep = *valuep + 32;
  return 0;
}

static const char*
ins_immus8 (const struct ia64_operand *self, ia64_insn value, ia64_insn *code)
{
  if (value & 0x7)
    return "value not an integer multiple of 8";
  return ins_immu (self, value >> 3, code);
}

static const char*
ext_immus8 (const struct ia64_operand *self, ia64_insn code, ia64_insn *valuep)
{
  const char *result;

  result = ext_immu (self, code, valuep);
  if (result)
    return result;

  *valuep = *valuep << 3;
  return 0;
}

static const char*
ins_imms_scaled (const struct ia64_operand *self, ia64_insn value,
		 ia64_insn *code, int scale)
{
  int64_t svalue = value, sign_bit = 0;
  ia64_insn new = 0;
  int i;

  svalue >>= scale;

  for (i = 0; i < NELEMS (self->field) && self->field[i].bits; ++i)
    {
      new |= ((svalue & ((((ia64_insn) 1) << self->field[i].bits) - 1))
	      << self->field[i].shift);
      sign_bit = (svalue >> (self->field[i].bits - 1)) & 1;
      svalue >>= self->field[i].bits;
    }
  if ((!sign_bit && svalue != 0) || (sign_bit && svalue != -1))
    return "integer operand out of range";

  *code |= new;
  return 0;
}

static const char*
ext_imms_scaled (const struct ia64_operand *self, ia64_insn code,
		 ia64_insn *valuep, int scale)
{
  int i, bits = 0, total = 0;
  int64_t val = 0, sign;

  for (i = 0; i < NELEMS (self->field) && self->field[i].bits; ++i)
    {
      bits = self->field[i].bits;
      val |= ((code >> self->field[i].shift)
	      & ((((uint64_t) 1) << bits) - 1)) << total;
      total += bits;
    }
  /* sign extend: */
  sign = (int64_t) 1 << (total - 1);
  val = (val ^ sign) - sign;

  *valuep = (val << scale);
  return 0;
}

static const char*
ins_imms (const struct ia64_operand *self, ia64_insn value, ia64_insn *code)
{
  return ins_imms_scaled (self, value, code, 0);
}

static const char*
ins_immsu4 (const struct ia64_operand *self, ia64_insn value, ia64_insn *code)
{
  value = ((value & 0xffffffff) ^ 0x80000000) - 0x80000000;

  return ins_imms_scaled (self, value, code, 0);
}

static const char*
ext_imms (const struct ia64_operand *self, ia64_insn code, ia64_insn *valuep)
{
  return ext_imms_scaled (self, code, valuep, 0);
}

static const char*
ins_immsm1 (const struct ia64_operand *self, ia64_insn value, ia64_insn *code)
{
  --value;
  return ins_imms_scaled (self, value, code, 0);
}

static const char*
ins_immsm1u4 (const struct ia64_operand *self, ia64_insn value,
	      ia64_insn *code)
{
  value = ((value & 0xffffffff) ^ 0x80000000) - 0x80000000;

  --value;
  return ins_imms_scaled (self, value, code, 0);
}

static const char*
ext_immsm1 (const struct ia64_operand *self, ia64_insn code, ia64_insn *valuep)
{
  const char *res = ext_imms_scaled (self, code, valuep, 0);

  ++*valuep;
  return res;
}

static const char*
ins_imms1 (const struct ia64_operand *self, ia64_insn value, ia64_insn *code)
{
  return ins_imms_scaled (self, value, code, 1);
}

static const char*
ext_imms1 (const struct ia64_operand *self, ia64_insn code, ia64_insn *valuep)
{
  return ext_imms_scaled (self, code, valuep, 1);
}

static const char*
ins_imms4 (const struct ia64_operand *self, ia64_insn value, ia64_insn *code)
{
  return ins_imms_scaled (self, value, code, 4);
}

static const char*
ext_imms4 (const struct ia64_operand *self, ia64_insn code, ia64_insn *valuep)
{
  return ext_imms_scaled (self, code, valuep, 4);
}

static const char*
ins_imms16 (const struct ia64_operand *self, ia64_insn value, ia64_insn *code)
{
  return ins_imms_scaled (self, value, code, 16);
}

static const char*
ext_imms16 (const struct ia64_operand *self, ia64_insn code, ia64_insn *valuep)
{
  return ext_imms_scaled (self, code, valuep, 16);
}

static const char*
ins_cimmu (const struct ia64_operand *self, ia64_insn value, ia64_insn *code)
{
  ia64_insn mask = (((ia64_insn) 1) << self->field[0].bits) - 1;
  return ins_immu (self, value ^ mask, code);
}

static const char*
ext_cimmu (const struct ia64_operand *self, ia64_insn code, ia64_insn *valuep)
{
  const char *result;
  ia64_insn mask;

  mask = (((ia64_insn) 1) << self->field[0].bits) - 1;
  result = ext_immu (self, code, valuep);
  if (!result)
    {
      mask = (((ia64_insn) 1) << self->field[0].bits) - 1;
      *valuep ^= mask;
    }
  return result;
}

static const char*
ins_cnt (const struct ia64_operand *self, ia64_insn value, ia64_insn *code)
{
  --value;
  if (value >= ((uint64_t) 1) << self->field[0].bits)
    return "count out of range";

  *code |= value << self->field[0].shift;
  return 0;
}

static const char*
ext_cnt (const struct ia64_operand *self, ia64_insn code, ia64_insn *valuep)
{
  *valuep = ((code >> self->field[0].shift)
	     & ((((uint64_t) 1) << self->field[0].bits) - 1)) + 1;
  return 0;
}

static const char*
ins_cnt2b (const struct ia64_operand *self, ia64_insn value, ia64_insn *code)
{
  --value;

  if (value > 2)
    return "count must be in range 1..3";

  *code |= value << self->field[0].shift;
  return 0;
}

static const char*
ext_cnt2b (const struct ia64_operand *self, ia64_insn code, ia64_insn *valuep)
{
  *valuep = ((code >> self->field[0].shift) & 0x3) + 1;
  return 0;
}

static const char*
ins_cnt2c (const struct ia64_operand *self, ia64_insn value, ia64_insn *code)
{
  switch (value)
    {
    case 0:	value = 0; break;
    case 7:	value = 1; break;
    case 15:	value = 2; break;
    case 16:	value = 3; break;
    default:	return "count must be 0, 7, 15, or 16";
    }
  *code |= value << self->field[0].shift;
  return 0;
}

static const char*
ext_cnt2c (const struct ia64_operand *self, ia64_insn code, ia64_insn *valuep)
{
  ia64_insn value;

  value = (code >> self->field[0].shift) & 0x3;
  switch (value)
    {
    case 0: value =  0; break;
    case 1: value =  7; break;
    case 2: value = 15; break;
    case 3: value = 16; break;
    }
  *valuep = value;
  return 0;
}

static const char*
ins_inc3 (const struct ia64_operand *self, ia64_insn value, ia64_insn *code)
{
  int64_t val = value;
  uint64_t sign = 0;

  if (val < 0)
    {
      sign = 0x4;
      value = -value;
    }
  switch (value)
    {
    case  1:	value = 3; break;
    case  4:	value = 2; break;
    case  8:	value = 1; break;
    case 16:	value = 0; break;
    default:	return "count must be +/- 1, 4, 8, or 16";
    }
  *code |= (sign | value) << self->field[0].shift;
  return 0;
}

static const char*
ext_inc3 (const struct ia64_operand *self, ia64_insn code, ia64_insn *valuep)
{
  int64_t val;
  int negate;

  val = (code >> self->field[0].shift) & 0x7;
  negate = val & 0x4;
  switch (val & 0x3)
    {
    case 0: val = 16; break;
    case 1: val =  8; break;
    case 2: val =  4; break;
    case 3: val =  1; break;
    }
  if (negate)
    val = -val;

  *valuep = val;
  return 0;
}

/* glib.h defines ABS so we must undefine it to avoid a clash */
#undef ABS

#define CST	IA64_OPND_CLASS_CST
#define REG	IA64_OPND_CLASS_REG
#define IND	IA64_OPND_CLASS_IND
#define ABS	IA64_OPND_CLASS_ABS
#define REL	IA64_OPND_CLASS_REL

#define SDEC	IA64_OPND_FLAG_DECIMAL_SIGNED
#define UDEC	IA64_OPND_FLAG_DECIMAL_UNSIGNED

const struct ia64_operand elf64_ia64_operands[IA64_OPND_COUNT] =
  {
    /* constants: */
    { CST, ins_const, ext_const, "NIL",		{{ 0, 0}}, 0, "<none>" },
    { CST, ins_const, ext_const, "ar.csd",	{{ 0, 0}}, 0, "ar.csd" },
    { CST, ins_const, ext_const, "ar.ccv",	{{ 0, 0}}, 0, "ar.ccv" },
    { CST, ins_const, ext_const, "ar.pfs",	{{ 0, 0}}, 0, "ar.pfs" },
    { CST, ins_const, ext_const, "1",		{{ 0, 0}}, 0, "1" },
    { CST, ins_const, ext_const, "8",		{{ 0, 0}}, 0, "8" },
    { CST, ins_const, ext_const, "16",		{{ 0, 0}}, 0, "16" },
    { CST, ins_const, ext_const, "r0",		{{ 0, 0}}, 0, "r0" },
    { CST, ins_const, ext_const, "ip",		{{ 0, 0}}, 0, "ip" },
    { CST, ins_const, ext_const, "pr",		{{ 0, 0}}, 0, "pr" },
    { CST, ins_const, ext_const, "pr.rot",	{{ 0, 0}}, 0, "pr.rot" },
    { CST, ins_const, ext_const, "psr",		{{ 0, 0}}, 0, "psr" },
    { CST, ins_const, ext_const, "psr.l",	{{ 0, 0}}, 0, "psr.l" },
    { CST, ins_const, ext_const, "psr.um",	{{ 0, 0}}, 0, "psr.um" },

    /* register operands: */
    { REG, ins_reg,   ext_reg,	"ar", {{ 7, 20}}, 0,		/* AR3 */
      "an application register" },
    { REG, ins_reg,   ext_reg,	 "b", {{ 3,  6}}, 0,		/* B1 */
      "a branch register" },
    { REG, ins_reg,   ext_reg,	 "b", {{ 3, 13}}, 0,		/* B2 */
      "a branch register"},
    { REG, ins_reg,   ext_reg,	"cr", {{ 7, 20}}, 0,		/* CR */
      "a control register"},
    { REG, ins_reg,   ext_reg,	 "f", {{ 7,  6}}, 0,		/* F1 */
      "a floating-point register" },
    { REG, ins_reg,   ext_reg,	 "f", {{ 7, 13}}, 0,		/* F2 */
      "a floating-point register" },
    { REG, ins_reg,   ext_reg,	 "f", {{ 7, 20}}, 0,		/* F3 */
      "a floating-point register" },
    { REG, ins_reg,   ext_reg,	 "f", {{ 7, 27}}, 0,		/* F4 */
      "a floating-point register" },
    { REG, ins_reg,   ext_reg,	 "p", {{ 6,  6}}, 0,		/* P1 */
      "a predicate register" },
    { REG, ins_reg,   ext_reg,	 "p", {{ 6, 27}}, 0,		/* P2 */
      "a predicate register" },
    { REG, ins_reg,   ext_reg,	 "r", {{ 7,  6}}, 0,		/* R1 */
      "a general register" },
    { REG, ins_reg,   ext_reg,	 "r", {{ 7, 13}}, 0,		/* R2 */
      "a general register" },
    { REG, ins_reg,   ext_reg,	 "r", {{ 7, 20}}, 0,		/* R3 */
      "a general register" },
    { REG, ins_reg,   ext_reg,	 "r", {{ 2, 20}}, 0,		/* R3_2 */
      "a general register r0-r3" },

    /* memory operands: */
    { IND, ins_reg,   ext_reg,	"",      {{7, 20}}, 0,		/* MR3 */
      "a memory address" },

    /* indirect operands: */
    { IND, ins_reg,   ext_reg,	"cpuid", {{7, 20}}, 0,		/* CPUID_R3 */
      "a cpuid register" },
    { IND, ins_reg,   ext_reg,	"dbr",   {{7, 20}}, 0,		/* DBR_R3 */
      "a dbr register" },
    { IND, ins_reg,   ext_reg,	"dtr",   {{7, 20}}, 0,		/* DTR_R3 */
      "a dtr register" },
    { IND, ins_reg,   ext_reg,	"itr",   {{7, 20}}, 0,		/* ITR_R3 */
      "an itr register" },
    { IND, ins_reg,   ext_reg,	"ibr",   {{7, 20}}, 0,		/* IBR_R3 */
      "an ibr register" },
    { IND, ins_reg,   ext_reg,	"msr",   {{7, 20}}, 0,		/* MSR_R3 */
      "an msr register" },
    { IND, ins_reg,   ext_reg,	"pkr",   {{7, 20}}, 0,		/* PKR_R3 */
      "a pkr register" },
    { IND, ins_reg,   ext_reg,	"pmc",   {{7, 20}}, 0,		/* PMC_R3 */
      "a pmc register" },
    { IND, ins_reg,   ext_reg,	"pmd",   {{7, 20}}, 0,		/* PMD_R3 */
      "a pmd register" },
    { IND, ins_reg,   ext_reg,	"rr",    {{7, 20}}, 0,		/* RR_R3 */
      "an rr register" },

    /* immediate operands: */
    { ABS, ins_cimmu, ext_cimmu, 0, {{ 5, 20 }}, UDEC,		/* CCNT5 */
      "a 5-bit count (0-31)" },
    { ABS, ins_cnt,   ext_cnt,   0, {{ 2, 27 }}, UDEC,		/* CNT2a */
      "a 2-bit count (1-4)" },
    { ABS, ins_cnt2b, ext_cnt2b, 0, {{ 2, 27 }}, UDEC,		/* CNT2b */
      "a 2-bit count (1-3)" },
    { ABS, ins_cnt2c, ext_cnt2c, 0, {{ 2, 30 }}, UDEC,		/* CNT2c */
      "a count (0, 7, 15, or 16)" },
    { ABS, ins_immu,  ext_immu,  0, {{ 5, 14}}, UDEC,		/* CNT5 */
      "a 5-bit count (0-31)" },
    { ABS, ins_immu,  ext_immu,  0, {{ 6, 27}}, UDEC,		/* CNT6 */
      "a 6-bit count (0-63)" },
    { ABS, ins_cimmu, ext_cimmu, 0, {{ 6, 20}}, UDEC,		/* CPOS6a */
      "a 6-bit bit pos (0-63)" },
    { ABS, ins_cimmu, ext_cimmu, 0, {{ 6, 14}}, UDEC,		/* CPOS6b */
      "a 6-bit bit pos (0-63)" },
    { ABS, ins_cimmu, ext_cimmu, 0, {{ 6, 31}}, UDEC,		/* CPOS6c */
      "a 6-bit bit pos (0-63)" },
    { ABS, ins_imms,  ext_imms,  0, {{ 1, 36}}, SDEC,		/* IMM1 */
      "a 1-bit integer (-1, 0)" },
    { ABS, ins_immu,  ext_immu,  0, {{ 2, 13}}, UDEC,		/* IMMU2 */
      "a 2-bit unsigned (0-3)" },
    { ABS, ins_immu5b,  ext_immu5b,  0, {{ 5, 14}}, UDEC,	/* IMMU5b */
      "a 5-bit unsigned (32 + (0-31))" },
    { ABS, ins_immu,  ext_immu,  0, {{ 7, 13}}, 0,		/* IMMU7a */
      "a 7-bit unsigned (0-127)" },
    { ABS, ins_immu,  ext_immu,  0, {{ 7, 20}}, 0,		/* IMMU7b */
      "a 7-bit unsigned (0-127)" },
    { ABS, ins_immu,  ext_immu,  0, {{ 7, 13}}, UDEC,		/* SOF */
      "a frame size (register count)" },
    { ABS, ins_immu,  ext_immu,  0, {{ 7, 20}}, UDEC,		/* SOL */
      "a local register count" },
    { ABS, ins_immus8,ext_immus8,0, {{ 4, 27}}, UDEC,		/* SOR */
      "a rotating register count (integer multiple of 8)" },
    { ABS, ins_imms,  ext_imms,  0,				/* IMM8 */
      {{ 7, 13}, { 1, 36}}, SDEC,
      "an 8-bit integer (-128-127)" },
    { ABS, ins_immsu4,  ext_imms,  0,				/* IMM8U4 */
      {{ 7, 13}, { 1, 36}}, SDEC,
      "an 8-bit signed integer for 32-bit unsigned compare (-128-127)" },
    { ABS, ins_immsm1,  ext_immsm1,  0,				/* IMM8M1 */
      {{ 7, 13}, { 1, 36}}, SDEC,
      "an 8-bit integer (-127-128)" },
    { ABS, ins_immsm1u4,  ext_immsm1,  0,			/* IMM8M1U4 */
      {{ 7, 13}, { 1, 36}}, SDEC,
      "an 8-bit integer for 32-bit unsigned compare (-127-(-1),1-128,0x100000000)" },
    { ABS, ins_immsm1,  ext_immsm1,  0,				/* IMM8M1U8 */
      {{ 7, 13}, { 1, 36}}, SDEC,
      "an 8-bit integer for 64-bit unsigned compare (-127-(-1),1-128,0x10000000000000000)" },
    { ABS, ins_immu,  ext_immu,  0, {{ 2, 33}, { 7, 20}}, 0,	/* IMMU9 */
      "a 9-bit unsigned (0-511)" },
    { ABS, ins_imms,  ext_imms,  0,				/* IMM9a */
      {{ 7,  6}, { 1, 27}, { 1, 36}}, SDEC,
      "a 9-bit integer (-256-255)" },
    { ABS, ins_imms,  ext_imms, 0,				/* IMM9b */
      {{ 7, 13}, { 1, 27}, { 1, 36}}, SDEC,
      "a 9-bit integer (-256-255)" },
    { ABS, ins_imms,  ext_imms, 0,				/* IMM14 */
      {{ 7, 13}, { 6, 27}, { 1, 36}}, SDEC,
      "a 14-bit integer (-8192-8191)" },
    { ABS, ins_imms1, ext_imms1, 0,				/* IMM17 */
      {{ 7,  6}, { 8, 24}, { 1, 36}}, 0,
      "a 17-bit integer (-65536-65535)" },
    { ABS, ins_immu,  ext_immu,  0, {{20,  6}, { 1, 36}}, 0,	/* IMMU21 */
      "a 21-bit unsigned" },
    { ABS, ins_imms,  ext_imms,  0,				/* IMM22 */
      {{ 7, 13}, { 9, 27}, { 5, 22}, { 1, 36}}, SDEC,
      "a 22-bit signed integer" },
    { ABS, ins_immu,  ext_immu,  0,				/* IMMU24 */
      {{21,  6}, { 2, 31}, { 1, 36}}, 0,
      "a 24-bit unsigned" },
    { ABS, ins_imms16,ext_imms16,0, {{27,  6}, { 1, 36}}, 0,	/* IMM44 */
      "a 44-bit unsigned (least 16 bits ignored/zeroes)" },
    { ABS, ins_rsvd,  ext_rsvd,	0, {{0,  0}}, 0,		/* IMMU62 */
      "a 62-bit unsigned" },
    { ABS, ins_rsvd,  ext_rsvd,	0, {{0,  0}}, 0,		/* IMMU64 */
      "a 64-bit unsigned" },
    { ABS, ins_inc3,  ext_inc3,  0, {{ 3, 13}}, SDEC,		/* INC3 */
      "an increment (+/- 1, 4, 8, or 16)" },
    { ABS, ins_cnt,   ext_cnt,   0, {{ 4, 27}}, UDEC,		/* LEN4 */
      "a 4-bit length (1-16)" },
    { ABS, ins_cnt,   ext_cnt,   0, {{ 6, 27}}, UDEC,		/* LEN6 */
      "a 6-bit length (1-64)" },
    { ABS, ins_immu,  ext_immu,  0, {{ 4, 20}},	0,		/* MBTYPE4 */
      "a mix type (@rev, @mix, @shuf, @alt, or @brcst)" },
    { ABS, ins_immu,  ext_immu,  0, {{ 8, 20}},	0,		/* MBTYPE8 */
      "an 8-bit mix type" },
    { ABS, ins_immu,  ext_immu,  0, {{ 6, 14}}, UDEC,		/* POS6 */
      "a 6-bit bit pos (0-63)" },
    { REL, ins_imms4, ext_imms4, 0, {{ 7,  6}, { 2, 33}}, 0,	/* TAG13 */
      "a branch tag" },
    { REL, ins_imms4, ext_imms4, 0, {{ 9, 24}}, 0,		/* TAG13b */
      "a branch tag" },
    { REL, ins_imms4, ext_imms4, 0, {{20,  6}, { 1, 36}}, 0,	/* TGT25 */
      "a branch target" },
    { REL, ins_imms4, ext_imms4, 0,				/* TGT25b */
      {{ 7,  6}, {13, 20}, { 1, 36}}, 0,
      "a branch target" },
    { REL, ins_imms4, ext_imms4, 0, {{20, 13}, { 1, 36}}, 0,	/* TGT25c */
      "a branch target" },
    { REL, ins_rsvd, ext_rsvd, 0, {{0, 0}}, 0,                  /* TGT64  */
      "a branch target" },

    { ABS, ins_const, ext_const, 0, {{0, 0}}, 0,		/* LDXMOV */
      "ldxmov target" },
  };


/* ia64-asmtab.h -- Header for compacted IA-64 opcode tables.
   Copyright 1999, 2000 Free Software Foundation, Inc.
   Contributed by Bob Manson of Cygnus Support <manson@cygnus.com>

   This file is part of GDB, GAS, and the GNU binutils.

   GDB, GAS, and the GNU binutils are free software; you can redistribute
   them and/or modify them under the terms of the GNU General Public
   License as published by the Free Software Foundation; either version
   2, or (at your option) any later version.

   GDB, GAS, and the GNU binutils are distributed in the hope that they
   will be useful, but WITHOUT ANY WARRANTY; without even the implied
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this file; see the file COPYING.  If not, see
   <http://www.gnu.org/licenses/>. */

/* The primary opcode table is made up of the following: */
struct ia64_main_table
{
  /* The entry in the string table that corresponds to the name of this
     opcode. */
  unsigned short name_index;

  /* The type of opcode; corresponds to the TYPE field in
     struct ia64_opcode. */
  unsigned char opcode_type;

  /* The number of outputs for this opcode. */
  unsigned char num_outputs;

  /* The base insn value for this opcode.  It may be modified by completers. */
  ia64_insn opcode;

  /* The mask of valid bits in OPCODE. Zeros indicate operand fields. */
  ia64_insn mask;

  /* The operands of this instruction.  Corresponds to the OPERANDS field
     in struct ia64_opcode. */
  unsigned char operands[5];

  /* The flags for this instruction.  Corresponds to the FLAGS field in
     struct ia64_opcode. */
  short flags;

  /* The tree of completers for this instruction; this is an offset into
     completer_table. */
  short completers;
};

/* Each instruction has a set of possible "completers", or additional
   suffixes that can alter the instruction's behavior, and which has
   potentially different dependencies.

   The completer entries modify certain bits in the instruction opcode.
   Which bits are to be modified are marked by the BITS, MASK and
   OFFSET fields.  The completer entry may also note dependencies for the
   opcode.

   These completers are arranged in a DAG; the pointers are indexes
   into the completer_table array.  The completer DAG is searched by
   find_completer () and ia64_find_matching_opcode ().

   Note that each completer needs to be applied in turn, so that if we
   have the instruction
	cmp.lt.unc
   the completer entries for both "lt" and "unc" would need to be applied
   to the opcode's value.

   Some instructions do not require any completers; these contain an
   empty completer entry.  Instructions that require a completer do
   not contain an empty entry.

   Terminal completers (those completers that validly complete an
   instruction) are marked by having the TERMINAL_COMPLETER flag set.

   Only dependencies listed in the terminal completer for an opcode are
   considered to apply to that opcode instance. */

struct ia64_completer_table
{
  /* The bit value that this completer sets. */
  unsigned int bits;

  /* And its mask. 1s are bits that are to be modified in the
     instruction. */
  unsigned int mask;

  /* The entry in the string table that corresponds to the name of this
     completer. */
  unsigned short name_index;

  /* An alternative completer, or -1 if this is the end of the chain. */
  short alternative;

  /* A pointer to the DAG of completers that can potentially follow
     this one, or -1. */
  short subentries;

  /* The bit offset in the instruction where BITS and MASK should be
     applied. */
  unsigned char offset : 7;

  unsigned char terminal_completer : 1;

  /* Index into the dependency list table */
  short dependencies;
};

/* This contains sufficient information for the disassembler to resolve
   the complete name of the original instruction.  */
struct ia64_dis_names
{
  /* COMPLETER_INDEX represents the tree of completers that make up
     the instruction.  The LSB represents the top of the tree for the
     specified instruction.

     A 0 bit indicates to go to the next alternate completer via the
     alternative field; a 1 bit indicates that the current completer
     is part of the instruction, and to go down the subentries index.
     We know we've reached the final completer when we run out of 1
     bits.

     There is always at least one 1 bit. */
  unsigned int completer_index : 20;

  /* The index in the main_table[] array for the instruction. */
  unsigned short insn_index : 11;

  /* If set, the next entry in this table is an alternate possibility
     for this instruction encoding.  Which one to use is determined by
     the instruction type and other factors (see opcode_verify ()).  */
  unsigned int next_flag : 1;

  /* The disassembly priority of this entry among instructions. */
  unsigned short priority;
};

static const char * const ia64_strings[] = {
  "", "0", "1", "a", "acq", "add", "addl", "addp4", "adds", "alloc", "and",
  "andcm", "b", "bias", "br", "break", "brl", "brp", "bsw", "c", "call",
  "cexit", "chk", "cloop", "clr", "clrrrb", "cmp", "cmp4", "cmp8xchg16",
  "cmpxchg1", "cmpxchg2", "cmpxchg4", "cmpxchg8", "cond", "cover", "ctop",
  "czx1", "czx2", "d", "dep", "dpnt", "dptk", "e", "epc", "eq", "excl",
  "exit", "exp", "extr", "f", "fabs", "fadd", "famax", "famin", "fand",
  "fandcm", "fault", "fc", "fchkf", "fclass", "fclrf", "fcmp", "fcvt",
  "fetchadd4", "fetchadd8", "few", "fill", "flushrs", "fma", "fmax",
  "fmerge", "fmin", "fmix", "fmpy", "fms", "fneg", "fnegabs", "fnma",
  "fnmpy", "fnorm", "for", "fpabs", "fpack", "fpamax", "fpamin", "fpcmp",
  "fpcvt", "fpma", "fpmax", "fpmerge", "fpmin", "fpmpy", "fpms", "fpneg",
  "fpnegabs", "fpnma", "fpnmpy", "fprcpa", "fprsqrta", "frcpa", "frsqrta",
  "fselect", "fsetc", "fsub", "fswap", "fsxt", "fwb", "fx", "fxor", "fxu",
  "g", "ga", "ge", "getf", "geu", "gt", "gtu", "h", "hint", "hu", "i", "ia",
  "imp", "invala", "itc", "itr", "l", "ld1", "ld16", "ld2", "ld4", "ld8",
  "ldf", "ldf8", "ldfd", "ldfe", "ldfp8", "ldfpd", "ldfps", "ldfs", "le",
  "leu", "lfetch", "loadrs", "loop", "lr", "lt", "ltu", "lu", "m", "many",
  "mf", "mix1", "mix2", "mix4", "mov", "movl", "mux1", "mux2", "nc", "ne",
  "neq", "nge", "ngt", "nl", "nle", "nlt", "nm", "nop", "nr", "ns", "nt1",
  "nt2", "nta", "nz", "or", "orcm", "ord", "pack2", "pack4", "padd1",
  "padd2", "padd4", "pavg1", "pavg2", "pavgsub1", "pavgsub2", "pcmp1",
  "pcmp2", "pcmp4", "pmax1", "pmax2", "pmin1", "pmin2", "pmpy2", "pmpyshr2",
  "popcnt", "pr", "probe", "psad1", "pshl2", "pshl4", "pshladd2", "pshr2",
  "pshr4", "pshradd2", "psub1", "psub2", "psub4", "ptc", "ptr", "r", "raz",
  "rel", "ret", "rfi", "rsm", "rum", "rw", "s", "s0", "s1", "s2", "s3",
  "sa", "se", "setf", "shl", "shladd", "shladdp4", "shr", "shrp", "sig",
  "spill", "spnt", "sptk", "srlz", "ssm", "sss", "st1", "st16", "st2",
  "st4", "st8", "stf", "stf8", "stfd", "stfe", "stfs", "sub", "sum", "sxt1",
  "sxt2", "sxt4", "sync", "tak", "tbit", "tf", "thash", "tnat", "tpa",
  "trunc", "ttag", "u", "unc", "unord", "unpack1", "unpack2", "unpack4",
  "uss", "uus", "uuu", "vmsw", "w", "wexit", "wtop", "x", "xchg1", "xchg2",
  "xchg4", "xchg8", "xf", "xma", "xmpy", "xor", "xuf", "z", "zxt1", "zxt2",
  "zxt4",
};

static const struct ia64_dependency
dependencies[] = {
  { "ALAT", 0, 0, 0, -1, NULL, },
  { "AR[BSP]", 26, 0, 2, 17, NULL, },
  { "AR[BSPSTORE]", 26, 0, 2, 18, NULL, },
  { "AR[CCV]", 26, 0, 2, 32, NULL, },
  { "AR[CFLG]", 26, 0, 2, 27, NULL, },
  { "AR[CSD]", 26, 0, 2, 25, NULL, },
  { "AR[EC]", 26, 0, 2, 66, NULL, },
  { "AR[EFLAG]", 26, 0, 2, 24, NULL, },
  { "AR[FCR]", 26, 0, 2, 21, NULL, },
  { "AR[FDR]", 26, 0, 2, 30, NULL, },
  { "AR[FIR]", 26, 0, 2, 29, NULL, },
  { "AR[FPSR].sf0.controls", 30, 0, 2, -1, NULL, },
  { "AR[FPSR].sf1.controls", 30, 0, 2, -1, NULL, },
  { "AR[FPSR].sf2.controls", 30, 0, 2, -1, NULL, },
  { "AR[FPSR].sf3.controls", 30, 0, 2, -1, NULL, },
  { "AR[FPSR].sf0.flags", 30, 0, 2, -1, NULL, },
  { "AR[FPSR].sf1.flags", 30, 0, 2, -1, NULL, },
  { "AR[FPSR].sf2.flags", 30, 0, 2, -1, NULL, },
  { "AR[FPSR].sf3.flags", 30, 0, 2, -1, NULL, },
  { "AR[FPSR].traps", 30, 0, 2, -1, NULL, },
  { "AR[FPSR].rv", 30, 0, 2, -1, NULL, },
  { "AR[FSR]", 26, 0, 2, 28, NULL, },
  { "AR[ITC]", 26, 0, 2, 44, NULL, },
  { "AR[K%], % in 0 - 7", 1, 0, 2, -1, NULL, },
  { "AR[LC]", 26, 0, 2, 65, NULL, },
  { "AR[PFS]", 26, 0, 2, 64, NULL, },
  { "AR[PFS]", 26, 0, 2, 64, NULL, },
  { "AR[PFS]", 26, 0, 0, 64, NULL, },
  { "AR[RNAT]", 26, 0, 2, 19, NULL, },
  { "AR[RSC]", 26, 0, 2, 16, NULL, },
  { "AR[SSD]", 26, 0, 2, 26, NULL, },
  { "AR[UNAT]{%}, % in 0 - 63", 2, 0, 2, -1, NULL, },
  { "AR%, % in 8-15, 20, 22-23, 31, 33-35, 37-39, 41-43, 45-47, 67-111", 3, 0, 0, -1, NULL, },
  { "AR%, % in 48-63, 112-127", 4, 0, 2, -1, NULL, },
  { "BR%, % in 0 - 7", 5, 0, 2, -1, NULL, },
  { "BR%, % in 0 - 7", 5, 0, 0, -1, NULL, },
  { "BR%, % in 0 - 7", 5, 0, 2, -1, NULL, },
  { "CFM", 6, 0, 2, -1, NULL, },
  { "CFM", 6, 0, 2, -1, NULL, },
  { "CFM", 6, 0, 2, -1, NULL, },
  { "CFM", 6, 0, 2, -1, NULL, },
  { "CFM", 6, 0, 0, -1, NULL, },
  { "CPUID#", 7, 0, 5, -1, NULL, },
  { "CR[CMCV]", 27, 0, 3, 74, NULL, },
  { "CR[DCR]", 27, 0, 3, 0, NULL, },
  { "CR[EOI]", 27, 0, 7, 67, "SC Section 5.8.3.4, \"End of External Interrupt Register (EOI Ð CR67)\" on page 2:119", },
  { "CR[GPTA]", 27, 0, 3, 9, NULL, },
  { "CR[IFA]", 27, 0, 1, 20, NULL, },
  { "CR[IFA]", 27, 0, 3, 20, NULL, },
  { "CR[IFS]", 27, 0, 3, 23, NULL, },
  { "CR[IFS]", 27, 0, 1, 23, NULL, },
  { "CR[IFS]", 27, 0, 1, 23, NULL, },
  { "CR[IHA]", 27, 0, 3, 25, NULL, },
  { "CR[IIM]", 27, 0, 3, 24, NULL, },
  { "CR[IIP]", 27, 0, 3, 19, NULL, },
  { "CR[IIP]", 27, 0, 1, 19, NULL, },
  { "CR[IIPA]", 27, 0, 3, 22, NULL, },
  { "CR[IPSR]", 27, 0, 3, 16, NULL, },
  { "CR[IPSR]", 27, 0, 1, 16, NULL, },
  { "CR[IRR%], % in 0 - 3", 8, 0, 3, -1, NULL, },
  { "CR[ISR]", 27, 0, 3, 17, NULL, },
  { "CR[ITIR]", 27, 0, 3, 21, NULL, },
  { "CR[ITIR]", 27, 0, 1, 21, NULL, },
  { "CR[ITM]", 27, 0, 3, 1, NULL, },
  { "CR[ITV]", 27, 0, 3, 72, NULL, },
  { "CR[IVA]", 27, 0, 4, 2, NULL, },
  { "CR[IVR]", 27, 0, 7, 65, "SC Section 5.8.3.2, \"External Interrupt Vector Register (IVR Ð CR65)\" on page 2:118", },
  { "CR[LID]", 27, 0, 7, 64, "SC Section 5.8.3.1, \"Local ID (LID Ð CR64)\" on page 2:117", },
  { "CR[LRR%], % in 0 - 1", 9, 0, 3, -1, NULL, },
  { "CR[PMV]", 27, 0, 3, 73, NULL, },
  { "CR[PTA]", 27, 0, 3, 8, NULL, },
  { "CR[TPR]", 27, 0, 3, 66, NULL, },
  { "CR[TPR]", 27, 0, 7, 66, "SC Section 5.8.3.3, \"Task Priority Register (TPR Ð CR66)\" on page 2:119", },
  { "CR[TPR]", 27, 0, 1, 66, NULL, },
  { "CR%, % in 3-7, 10-15, 18, 26-63, 75-79, 82-127", 10, 0, 0, -1, NULL, },
  { "DBR#", 11, 0, 2, -1, NULL, },
  { "DBR#", 11, 0, 3, -1, NULL, },
  { "DTC", 0, 0, 3, -1, NULL, },
  { "DTC", 0, 0, 2, -1, NULL, },
  { "DTC", 0, 0, 0, -1, NULL, },
  { "DTC", 0, 0, 2, -1, NULL, },
  { "DTC_LIMIT*", 0, 0, 2, -1, NULL, },
  { "DTR", 0, 0, 3, -1, NULL, },
  { "DTR", 0, 0, 2, -1, NULL, },
  { "DTR", 0, 0, 3, -1, NULL, },
  { "DTR", 0, 0, 0, -1, NULL, },
  { "DTR", 0, 0, 2, -1, NULL, },
  { "FR%, % in 0 - 1", 12, 0, 0, -1, NULL, },
  { "FR%, % in 2 - 127", 13, 0, 2, -1, NULL, },
  { "FR%, % in 2 - 127", 13, 0, 0, -1, NULL, },
  { "GR0", 14, 0, 0, -1, NULL, },
  { "GR%, % in 1 - 127", 15, 0, 0, -1, NULL, },
  { "GR%, % in 1 - 127", 15, 0, 2, -1, NULL, },
  { "IBR#", 16, 0, 2, -1, NULL, },
  { "InService*", 17, 0, 3, -1, NULL, },
  { "InService*", 17, 0, 2, -1, NULL, },
  { "InService*", 17, 0, 2, -1, NULL, },
  { "IP", 0, 0, 0, -1, NULL, },
  { "ITC", 0, 0, 4, -1, NULL, },
  { "ITC", 0, 0, 2, -1, NULL, },
  { "ITC", 0, 0, 0, -1, NULL, },
  { "ITC", 0, 0, 4, -1, NULL, },
  { "ITC", 0, 0, 2, -1, NULL, },
  { "ITC_LIMIT*", 0, 0, 2, -1, NULL, },
  { "ITR", 0, 0, 2, -1, NULL, },
  { "ITR", 0, 0, 4, -1, NULL, },
  { "ITR", 0, 0, 2, -1, NULL, },
  { "ITR", 0, 0, 0, -1, NULL, },
  { "ITR", 0, 0, 4, -1, NULL, },
  { "memory", 0, 0, 0, -1, NULL, },
  { "MSR#", 18, 0, 5, -1, NULL, },
  { "PKR#", 19, 0, 3, -1, NULL, },
  { "PKR#", 19, 0, 0, -1, NULL, },
  { "PKR#", 19, 0, 2, -1, NULL, },
  { "PKR#", 19, 0, 2, -1, NULL, },
  { "PMC#", 20, 0, 2, -1, NULL, },
  { "PMC#", 20, 0, 7, -1, "SC Section 7.2.1, \"Generic Performance Counter Registers\" for PMC[0].fr on page 2:150", },
  { "PMD#", 21, 0, 2, -1, NULL, },
  { "PR0", 0, 0, 0, -1, NULL, },
  { "PR%, % in 1 - 15", 22, 0, 2, -1, NULL, },
  { "PR%, % in 1 - 15", 22, 0, 2, -1, NULL, },
  { "PR%, % in 1 - 15", 22, 0, 0, -1, NULL, },
  { "PR%, % in 16 - 62", 23, 0, 2, -1, NULL, },
  { "PR%, % in 16 - 62", 23, 0, 2, -1, NULL, },
  { "PR%, % in 16 - 62", 23, 0, 0, -1, NULL, },
  { "PR63", 24, 0, 2, -1, NULL, },
  { "PR63", 24, 0, 2, -1, NULL, },
  { "PR63", 24, 0, 0, -1, NULL, },
  { "PSR.ac", 28, 0, 1, 3, NULL, },
  { "PSR.ac", 28, 0, 3, 3, NULL, },
  { "PSR.ac", 28, 0, 2, 3, NULL, },
  { "PSR.ac", 28, 0, 2, 3, NULL, },
  { "PSR.be", 28, 0, 1, 1, NULL, },
  { "PSR.be", 28, 0, 3, 1, NULL, },
  { "PSR.be", 28, 0, 2, 1, NULL, },
  { "PSR.be", 28, 0, 2, 1, NULL, },
  { "PSR.bn", 28, 0, 2, 44, NULL, },
  { "PSR.cpl", 28, 0, 1, 32, NULL, },
  { "PSR.cpl", 28, 0, 2, 32, NULL, },
  { "PSR.da", 28, 0, 2, 38, NULL, },
  { "PSR.db", 28, 0, 3, 24, NULL, },
  { "PSR.db", 28, 0, 2, 24, NULL, },
  { "PSR.db", 28, 0, 2, 24, NULL, },
  { "PSR.dd", 28, 0, 2, 39, NULL, },
  { "PSR.dfh", 28, 0, 3, 19, NULL, },
  { "PSR.dfh", 28, 0, 2, 19, NULL, },
  { "PSR.dfh", 28, 0, 2, 19, NULL, },
  { "PSR.dfl", 28, 0, 3, 18, NULL, },
  { "PSR.dfl", 28, 0, 2, 18, NULL, },
  { "PSR.dfl", 28, 0, 2, 18, NULL, },
  { "PSR.di", 28, 0, 3, 22, NULL, },
  { "PSR.di", 28, 0, 2, 22, NULL, },
  { "PSR.di", 28, 0, 2, 22, NULL, },
  { "PSR.dt", 28, 0, 3, 17, NULL, },
  { "PSR.dt", 28, 0, 2, 17, NULL, },
  { "PSR.dt", 28, 0, 2, 17, NULL, },
  { "PSR.ed", 28, 0, 2, 43, NULL, },
  { "PSR.i", 28, 0, 2, 14, NULL, },
  { "PSR.ia", 28, 0, 0, 14, NULL, },
  { "PSR.ic", 28, 0, 2, 13, NULL, },
  { "PSR.ic", 28, 0, 3, 13, NULL, },
  { "PSR.ic", 28, 0, 2, 13, NULL, },
  { "PSR.id", 28, 0, 0, 14, NULL, },
  { "PSR.is", 28, 0, 0, 14, NULL, },
  { "PSR.it", 28, 0, 2, 14, NULL, },
  { "PSR.lp", 28, 0, 2, 25, NULL, },
  { "PSR.lp", 28, 0, 3, 25, NULL, },
  { "PSR.lp", 28, 0, 2, 25, NULL, },
  { "PSR.mc", 28, 0, 2, 35, NULL, },
  { "PSR.mfh", 28, 0, 2, 5, NULL, },
  { "PSR.mfl", 28, 0, 2, 4, NULL, },
  { "PSR.pk", 28, 0, 3, 15, NULL, },
  { "PSR.pk", 28, 0, 2, 15, NULL, },
  { "PSR.pk", 28, 0, 2, 15, NULL, },
  { "PSR.pp", 28, 0, 2, 21, NULL, },
  { "PSR.ri", 28, 0, 0, 41, NULL, },
  { "PSR.rt", 28, 0, 2, 27, NULL, },
  { "PSR.rt", 28, 0, 3, 27, NULL, },
  { "PSR.rt", 28, 0, 2, 27, NULL, },
  { "PSR.si", 28, 0, 2, 23, NULL, },
  { "PSR.si", 28, 0, 3, 23, NULL, },
  { "PSR.si", 28, 0, 2, 23, NULL, },
  { "PSR.sp", 28, 0, 2, 20, NULL, },
  { "PSR.sp", 28, 0, 3, 20, NULL, },
  { "PSR.sp", 28, 0, 2, 20, NULL, },
  { "PSR.ss", 28, 0, 2, 40, NULL, },
  { "PSR.tb", 28, 0, 3, 26, NULL, },
  { "PSR.tb", 28, 0, 2, 26, NULL, },
  { "PSR.tb", 28, 0, 2, 26, NULL, },
  { "PSR.up", 28, 0, 2, 2, NULL, },
  { "PSR.vm", 28, 0, 1, 46, NULL, },
  { "PSR.vm", 28, 0, 2, 46, NULL, },
  { "RR#", 25, 0, 3, -1, NULL, },
  { "RR#", 25, 0, 2, -1, NULL, },
  { "RSE", 29, 0, 2, -1, NULL, },
  { "ALAT", 0, 1, 0, -1, NULL, },
  { "AR[BSP]", 26, 1, 2, 17, NULL, },
  { "AR[BSPSTORE]", 26, 1, 2, 18, NULL, },
  { "AR[CCV]", 26, 1, 2, 32, NULL, },
  { "AR[CFLG]", 26, 1, 2, 27, NULL, },
  { "AR[CSD]", 26, 1, 2, 25, NULL, },
  { "AR[EC]", 26, 1, 2, 66, NULL, },
  { "AR[EFLAG]", 26, 1, 2, 24, NULL, },
  { "AR[FCR]", 26, 1, 2, 21, NULL, },
  { "AR[FDR]", 26, 1, 2, 30, NULL, },
  { "AR[FIR]", 26, 1, 2, 29, NULL, },
  { "AR[FPSR].sf0.controls", 30, 1, 2, -1, NULL, },
  { "AR[FPSR].sf1.controls", 30, 1, 2, -1, NULL, },
  { "AR[FPSR].sf2.controls", 30, 1, 2, -1, NULL, },
  { "AR[FPSR].sf3.controls", 30, 1, 2, -1, NULL, },
  { "AR[FPSR].sf0.flags", 30, 1, 0, -1, NULL, },
  { "AR[FPSR].sf0.flags", 30, 1, 2, -1, NULL, },
  { "AR[FPSR].sf0.flags", 30, 1, 2, -1, NULL, },
  { "AR[FPSR].sf1.flags", 30, 1, 0, -1, NULL, },
  { "AR[FPSR].sf1.flags", 30, 1, 2, -1, NULL, },
  { "AR[FPSR].sf1.flags", 30, 1, 2, -1, NULL, },
  { "AR[FPSR].sf2.flags", 30, 1, 0, -1, NULL, },
  { "AR[FPSR].sf2.flags", 30, 1, 2, -1, NULL, },
  { "AR[FPSR].sf2.flags", 30, 1, 2, -1, NULL, },
  { "AR[FPSR].sf3.flags", 30, 1, 0, -1, NULL, },
  { "AR[FPSR].sf3.flags", 30, 1, 2, -1, NULL, },
  { "AR[FPSR].sf3.flags", 30, 1, 2, -1, NULL, },
  { "AR[FPSR].rv", 30, 1, 2, -1, NULL, },
  { "AR[FPSR].traps", 30, 1, 2, -1, NULL, },
  { "AR[FSR]", 26, 1, 2, 28, NULL, },
  { "AR[ITC]", 26, 1, 2, 44, NULL, },
  { "AR[K%], % in 0 - 7", 1, 1, 2, -1, NULL, },
  { "AR[LC]", 26, 1, 2, 65, NULL, },
  { "AR[PFS]", 26, 1, 0, 64, NULL, },
  { "AR[PFS]", 26, 1, 2, 64, NULL, },
  { "AR[PFS]", 26, 1, 2, 64, NULL, },
  { "AR[RNAT]", 26, 1, 2, 19, NULL, },
  { "AR[RSC]", 26, 1, 2, 16, NULL, },
  { "AR[SSD]", 26, 1, 2, 26, NULL, },
  { "AR[UNAT]{%}, % in 0 - 63", 2, 1, 2, -1, NULL, },
  { "AR%, % in 8-15, 20, 22-23, 31, 33-35, 37-39, 41-43, 45-47, 67-111", 3, 1, 0, -1, NULL, },
  { "AR%, % in 48 - 63, 112-127", 4, 1, 2, -1, NULL, },
  { "BR%, % in 0 - 7", 5, 1, 2, -1, NULL, },
  { "BR%, % in 0 - 7", 5, 1, 2, -1, NULL, },
  { "BR%, % in 0 - 7", 5, 1, 2, -1, NULL, },
  { "BR%, % in 0 - 7", 5, 1, 0, -1, NULL, },
  { "CFM", 6, 1, 2, -1, NULL, },
  { "CPUID#", 7, 1, 0, -1, NULL, },
  { "CR[CMCV]", 27, 1, 2, 74, NULL, },
  { "CR[DCR]", 27, 1, 2, 0, NULL, },
  { "CR[EOI]", 27, 1, 7, 67, "SC Section 5.8.3.4, \"End of External Interrupt Register (EOI Ð CR67)\" on page 2:119", },
  { "CR[GPTA]", 27, 1, 2, 9, NULL, },
  { "CR[IFA]", 27, 1, 2, 20, NULL, },
  { "CR[IFS]", 27, 1, 2, 23, NULL, },
  { "CR[IHA]", 27, 1, 2, 25, NULL, },
  { "CR[IIM]", 27, 1, 2, 24, NULL, },
  { "CR[IIP]", 27, 1, 2, 19, NULL, },
  { "CR[IIPA]", 27, 1, 2, 22, NULL, },
  { "CR[IPSR]", 27, 1, 2, 16, NULL, },
  { "CR[IRR%], % in 0 - 3", 8, 1, 2, -1, NULL, },
  { "CR[ISR]", 27, 1, 2, 17, NULL, },
  { "CR[ITIR]", 27, 1, 2, 21, NULL, },
  { "CR[ITM]", 27, 1, 2, 1, NULL, },
  { "CR[ITV]", 27, 1, 2, 72, NULL, },
  { "CR[IVA]", 27, 1, 2, 2, NULL, },
  { "CR[IVR]", 27, 1, 7, 65, "SC", },
  { "CR[LID]", 27, 1, 7, 64, "SC", },
  { "CR[LRR%], % in 0 - 1", 9, 1, 2, -1, NULL, },
  { "CR[PMV]", 27, 1, 2, 73, NULL, },
  { "CR[PTA]", 27, 1, 2, 8, NULL, },
  { "CR[TPR]", 27, 1, 2, 66, NULL, },
  { "CR%, % in 3-7, 10-15, 18, 26-63, 75-79, 82-127", 10, 1, 0, -1, NULL, },
  { "DBR#", 11, 1, 2, -1, NULL, },
  { "DTC", 0, 1, 0, -1, NULL, },
  { "DTC", 0, 1, 2, -1, NULL, },
  { "DTC", 0, 1, 2, -1, NULL, },
  { "DTC_LIMIT*", 0, 1, 2, -1, NULL, },
  { "DTR", 0, 1, 2, -1, NULL, },
  { "DTR", 0, 1, 2, -1, NULL, },
  { "DTR", 0, 1, 2, -1, NULL, },
  { "DTR", 0, 1, 0, -1, NULL, },
  { "FR%, % in 0 - 1", 12, 1, 0, -1, NULL, },
  { "FR%, % in 2 - 127", 13, 1, 2, -1, NULL, },
  { "GR0", 14, 1, 0, -1, NULL, },
  { "GR%, % in 1 - 127", 15, 1, 2, -1, NULL, },
  { "IBR#", 16, 1, 2, -1, NULL, },
  { "InService*", 17, 1, 7, -1, "SC", },
  { "IP", 0, 1, 0, -1, NULL, },
  { "ITC", 0, 1, 0, -1, NULL, },
  { "ITC", 0, 1, 2, -1, NULL, },
  { "ITC", 0, 1, 2, -1, NULL, },
  { "ITR", 0, 1, 2, -1, NULL, },
  { "ITR", 0, 1, 2, -1, NULL, },
  { "ITR", 0, 1, 0, -1, NULL, },
  { "memory", 0, 1, 0, -1, NULL, },
  { "MSR#", 18, 1, 7, -1, "SC", },
  { "PKR#", 19, 1, 0, -1, NULL, },
  { "PKR#", 19, 1, 0, -1, NULL, },
  { "PKR#", 19, 1, 2, -1, NULL, },
  { "PMC#", 20, 1, 2, -1, NULL, },
  { "PMD#", 21, 1, 2, -1, NULL, },
  { "PR0", 0, 1, 0, -1, NULL, },
  { "PR%, % in 1 - 15", 22, 1, 0, -1, NULL, },
  { "PR%, % in 1 - 15", 22, 1, 0, -1, NULL, },
  { "PR%, % in 1 - 15", 22, 1, 2, -1, NULL, },
  { "PR%, % in 1 - 15", 22, 1, 2, -1, NULL, },
  { "PR%, % in 16 - 62", 23, 1, 0, -1, NULL, },
  { "PR%, % in 16 - 62", 23, 1, 0, -1, NULL, },
  { "PR%, % in 16 - 62", 23, 1, 2, -1, NULL, },
  { "PR%, % in 16 - 62", 23, 1, 2, -1, NULL, },
  { "PR63", 24, 1, 0, -1, NULL, },
  { "PR63", 24, 1, 0, -1, NULL, },
  { "PR63", 24, 1, 2, -1, NULL, },
  { "PR63", 24, 1, 2, -1, NULL, },
  { "PSR.ac", 28, 1, 2, 3, NULL, },
  { "PSR.be", 28, 1, 2, 1, NULL, },
  { "PSR.bn", 28, 1, 2, 44, NULL, },
  { "PSR.cpl", 28, 1, 2, 32, NULL, },
  { "PSR.da", 28, 1, 2, 38, NULL, },
  { "PSR.db", 28, 1, 2, 24, NULL, },
  { "PSR.dd", 28, 1, 2, 39, NULL, },
  { "PSR.dfh", 28, 1, 2, 19, NULL, },
  { "PSR.dfl", 28, 1, 2, 18, NULL, },
  { "PSR.di", 28, 1, 2, 22, NULL, },
  { "PSR.dt", 28, 1, 2, 17, NULL, },
  { "PSR.ed", 28, 1, 2, 43, NULL, },
  { "PSR.i", 28, 1, 2, 14, NULL, },
  { "PSR.ia", 28, 1, 2, 14, NULL, },
  { "PSR.ic", 28, 1, 2, 13, NULL, },
  { "PSR.id", 28, 1, 2, 14, NULL, },
  { "PSR.is", 28, 1, 2, 14, NULL, },
  { "PSR.it", 28, 1, 2, 14, NULL, },
  { "PSR.lp", 28, 1, 2, 25, NULL, },
  { "PSR.mc", 28, 1, 2, 35, NULL, },
  { "PSR.mfh", 28, 1, 0, 5, NULL, },
  { "PSR.mfh", 28, 1, 2, 5, NULL, },
  { "PSR.mfh", 28, 1, 2, 5, NULL, },
  { "PSR.mfl", 28, 1, 0, 4, NULL, },
  { "PSR.mfl", 28, 1, 2, 4, NULL, },
  { "PSR.mfl", 28, 1, 2, 4, NULL, },
  { "PSR.pk", 28, 1, 2, 15, NULL, },
  { "PSR.pp", 28, 1, 2, 21, NULL, },
  { "PSR.ri", 28, 1, 2, 41, NULL, },
  { "PSR.rt", 28, 1, 2, 27, NULL, },
  { "PSR.si", 28, 1, 2, 23, NULL, },
  { "PSR.sp", 28, 1, 2, 20, NULL, },
  { "PSR.ss", 28, 1, 2, 40, NULL, },
  { "PSR.tb", 28, 1, 2, 26, NULL, },
  { "PSR.up", 28, 1, 2, 2, NULL, },
  { "PSR.vm", 28, 1, 2, 46, NULL, },
  { "RR#", 25, 1, 2, -1, NULL, },
  { "RSE", 29, 1, 2, -1, NULL, },
  { "PR63", 24, 2, 6, -1, NULL, },
};

static const unsigned short dep0[] = {
  97, 282, 2140, 2327,
};

static const unsigned short dep1[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173,
  2327, 4135, 20616,
};

static const unsigned short dep2[] = {
  97, 282, 2166, 2167, 2169, 2170, 2172, 2173, 2175, 2344, 2347, 2348, 2351,
  2352, 2355, 2356,
};

static const unsigned short dep3[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173,
  2344, 2347, 2348, 2351, 2352, 2355, 2356, 4135, 20616,
};

static const unsigned short dep4[] = {
  97, 282, 22646, 22647, 22649, 22650, 22652, 22653, 22655, 22824, 22827, 22828,
  22831, 22832, 22835, 22836,
};

static const unsigned short dep5[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173,
  4135, 20616, 22824, 22827, 22828, 22831, 22832, 22835, 22836,
};

static const unsigned short dep6[] = {
  97, 282, 2166, 2167, 2169, 2170, 2172, 2173, 2175, 2344, 2345, 2347, 2349,
  2351, 2353, 2355,
};

static const unsigned short dep7[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173,
  2344, 2345, 2348, 2349, 2352, 2353, 2356, 4135, 20616,
};

static const unsigned short dep8[] = {
  97, 282, 2166, 2167, 2169, 2170, 2172, 2173, 2175, 2344, 2346, 2348, 2350,
  2352, 2354, 2356,
};

static const unsigned short dep9[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173,
  2344, 2346, 2347, 2350, 2351, 2354, 2355, 4135, 20616,
};

static const unsigned short dep10[] = {
  97, 282, 2166, 2167, 2169, 2170, 2172, 2173, 2175, 2344, 2345, 2346, 2347,
  2348, 2349, 2350, 2351, 2352, 2353, 2354, 2355, 2356,
};

static const unsigned short dep11[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173,
  2344, 2345, 2346, 2347, 2348, 2349, 2350, 2351, 2352, 2353, 2354, 2355, 2356,
  4135, 20616,
};

static const unsigned short dep12[] = {
  97, 282, 2395,
};

static const unsigned short dep13[] = {
  40, 41, 97, 158, 162, 164, 175, 185, 186, 188, 282, 2082, 2083, 2166, 2168,
  2169, 2171, 2172, 2174, 2175, 4135,
};

static const unsigned short dep14[] = {
  97, 163, 282, 325, 2395, 28866, 29018,
};

static const unsigned short dep15[] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
  22, 23, 24, 25, 26, 28, 29, 30, 31, 32, 33, 40, 41, 97, 150, 152, 158, 162,
  164, 175, 185, 186, 188, 282, 325, 2082, 2083, 2166, 2168, 2169, 2171, 2172,
  2174, 2175, 4135, 28866, 29018,
};

static const unsigned short dep16[] = {
  1, 6, 40, 97, 137, 196, 201, 241, 282, 312, 2395, 28866, 29018,
};

static const unsigned short dep17[] = {
  1, 25, 27, 38, 40, 41, 97, 158, 162, 164, 166, 167, 175, 185, 186, 188, 196,
  201, 241, 282, 312, 2082, 2083, 2166, 2168, 2169, 2171, 2172, 2174, 2175,
  4135, 28866, 29018,
};

static const unsigned short dep18[] = {
  1, 40, 51, 97, 196, 241, 248, 282, 28866, 29018,
};

static const unsigned short dep19[] = {
  1, 38, 40, 41, 97, 158, 160, 161, 162, 175, 185, 190, 191, 196, 241, 248,
  282, 4135, 28866, 29018,
};

static const unsigned short dep20[] = {
  40, 97, 241, 282,
};

static const unsigned short dep21[] = {
  97, 158, 162, 175, 185, 241, 282,
};

static const unsigned short dep22[] = {
  1, 40, 97, 131, 135, 136, 138, 139, 142, 143, 146, 149, 152, 155, 156, 157,
  158, 161, 162, 163, 164, 167, 168, 169, 170, 173, 174, 175, 178, 181, 184,
  185, 188, 189, 191, 196, 241, 282, 309, 310, 311, 312, 313, 314, 315, 316,
  317, 318, 319, 320, 321, 322, 323, 324, 325, 326, 327, 328, 330, 331, 333,
  334, 335, 336, 337, 338, 339, 340, 341, 342, 343, 344, 28866, 29018,
};

static const unsigned short dep23[] = {
  1, 38, 40, 41, 50, 51, 55, 58, 73, 97, 137, 138, 158, 162, 175, 185, 190,
  191, 196, 241, 282, 309, 310, 311, 312, 313, 314, 315, 316, 317, 318, 319,
  320, 321, 322, 323, 324, 325, 326, 327, 328, 330, 331, 333, 334, 335, 336,
  337, 338, 339, 340, 341, 342, 343, 344, 4135, 28866, 29018,
};

static const unsigned short dep24[] = {
  97, 136, 282, 311,
};

static const unsigned short dep25[] = {
  97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 311,
};

static const unsigned short dep26[] = {
  97, 137, 282, 312,
};

static const unsigned short dep27[] = {
  25, 26, 97, 98, 101, 105, 108, 137, 138, 158, 162, 164, 175, 185, 282, 312,

};

static const unsigned short dep28[] = {
  97, 190, 282, 344,
};

static const unsigned short dep29[] = {
  97, 98, 101, 105, 108, 137, 138, 158, 162, 164, 175, 185, 282, 344,
};

static const unsigned short dep30[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2166, 2168, 2169, 2171, 2172, 2174, 2175,
  4135,
};

static const unsigned short dep31[] = {
  1, 25, 40, 97, 196, 228, 229, 241, 282, 2082, 2285, 2288, 2395, 28866, 29018,

};

static const unsigned short dep32[] = {
  1, 6, 38, 40, 41, 97, 137, 138, 158, 162, 164, 175, 185, 186, 188, 196, 228,
  230, 241, 282, 2082, 2083, 2166, 2168, 2169, 2171, 2172, 2174, 2175, 2286,
  2288, 4135, 28866, 29018,
};

static const unsigned short dep33[] = {
  97, 282,
};

static const unsigned short dep34[] = {
  97, 158, 162, 175, 185, 282, 2082, 2084,
};

static const unsigned short dep35[] = {
  40, 41, 97, 158, 162, 164, 175, 185, 186, 188, 282, 2166, 2168, 2169, 2171,
  2172, 2174, 2175, 4135,
};

static const unsigned short dep36[] = {
  6, 37, 38, 39, 97, 125, 126, 201, 241, 282, 307, 308, 2395,
};

static const unsigned short dep37[] = {
  6, 37, 40, 41, 97, 158, 162, 164, 175, 185, 186, 188, 201, 241, 282, 307,
  308, 347, 2166, 2168, 2169, 2171, 2172, 2174, 2175, 4135,
};

static const unsigned short dep38[] = {
  24, 97, 227, 282, 2395,
};

static const unsigned short dep39[] = {
  24, 40, 41, 97, 158, 162, 164, 175, 185, 186, 188, 227, 282, 2166, 2168, 2169,
  2171, 2172, 2174, 2175, 4135,
};

static const unsigned short dep40[] = {
  6, 24, 37, 38, 39, 97, 125, 126, 201, 227, 241, 282, 307, 308, 2395,
};

static const unsigned short dep41[] = {
  6, 24, 37, 40, 41, 97, 158, 162, 164, 175, 185, 186, 188, 201, 227, 241, 282,
  307, 308, 347, 2166, 2168, 2169, 2171, 2172, 2174, 2175, 4135,
};

static const unsigned short dep42[] = {
  1, 6, 38, 40, 41, 97, 137, 138, 158, 162, 164, 175, 185, 186, 188, 196, 228,
  230, 241, 282, 2166, 2168, 2169, 2171, 2172, 2174, 2175, 2286, 2288, 4135,
  28866, 29018,
};

static const unsigned short dep43[] = {
  97, 158, 162, 175, 185, 282,
};

static const unsigned short dep44[] = {
  15, 97, 210, 211, 282, 2136, 2325, 18601, 18602, 18761, 18762, 18764, 18765,
  22646, 22647, 22648, 22650, 22651, 22653, 22654, 22824, 22827, 22828, 22831,
  22832, 22835, 22836,
};

static const unsigned short dep45[] = {
  11, 19, 20, 40, 41, 97, 158, 162, 175, 185, 210, 212, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 2325, 4135, 16528, 16530, 16531, 16533, 18761, 18763,
  18764, 18766, 22824, 22827, 22828, 22831, 22832, 22835, 22836,
};

static const unsigned short dep46[] = {
  15, 16, 17, 18, 97, 210, 211, 213, 214, 216, 217, 219, 220, 282, 2136, 2325,
  18601, 18602, 18761, 18762, 18764, 18765, 22646, 22647, 22648, 22650, 22651,
  22653, 22654, 22824, 22827, 22828, 22831, 22832, 22835, 22836,
};

static const unsigned short dep47[] = {
  11, 12, 13, 14, 19, 20, 40, 41, 97, 158, 162, 175, 185, 210, 212, 213, 215,
  216, 218, 219, 221, 282, 2135, 2136, 2137, 2166, 2167, 2170, 2173, 2325, 4135,
  16528, 16530, 16531, 16533, 18761, 18763, 18764, 18766, 22824, 22827, 22828,
  22831, 22832, 22835, 22836,
};

static const unsigned short dep48[] = {
  16, 97, 213, 214, 282, 2136, 2325, 18601, 18602, 18761, 18762, 18764, 18765,
  22646, 22647, 22648, 22650, 22651, 22653, 22654, 22824, 22827, 22828, 22831,
  22832, 22835, 22836,
};

static const unsigned short dep49[] = {
  12, 19, 20, 40, 41, 97, 158, 162, 175, 185, 213, 215, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 2325, 4135, 16528, 16530, 16531, 16533, 18761, 18763,
  18764, 18766, 22824, 22827, 22828, 22831, 22832, 22835, 22836,
};

static const unsigned short dep50[] = {
  17, 97, 216, 217, 282, 2136, 2325, 18601, 18602, 18761, 18762, 18764, 18765,
  22646, 22647, 22648, 22650, 22651, 22653, 22654, 22824, 22827, 22828, 22831,
  22832, 22835, 22836,
};

static const unsigned short dep51[] = {
  13, 19, 20, 40, 41, 97, 158, 162, 175, 185, 216, 218, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 2325, 4135, 16528, 16530, 16531, 16533, 18761, 18763,
  18764, 18766, 22824, 22827, 22828, 22831, 22832, 22835, 22836,
};

static const unsigned short dep52[] = {
  18, 97, 219, 220, 282, 2136, 2325, 18601, 18602, 18761, 18762, 18764, 18765,
  22646, 22647, 22648, 22650, 22651, 22653, 22654, 22824, 22827, 22828, 22831,
  22832, 22835, 22836,
};

static const unsigned short dep53[] = {
  14, 19, 20, 40, 41, 97, 158, 162, 175, 185, 219, 221, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 2325, 4135, 16528, 16530, 16531, 16533, 18761, 18763,
  18764, 18766, 22824, 22827, 22828, 22831, 22832, 22835, 22836,
};

static const unsigned short dep54[] = {
  15, 97, 210, 211, 282, 2136, 2325, 18601, 18602, 18761, 18762, 18764, 18765,

};

static const unsigned short dep55[] = {
  11, 19, 20, 40, 41, 97, 158, 162, 175, 185, 210, 212, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 2325, 4135, 16528, 16530, 16531, 16533, 18761, 18763,
  18764, 18766,
};

static const unsigned short dep56[] = {
  15, 16, 17, 18, 97, 210, 211, 213, 214, 216, 217, 219, 220, 282, 2136, 2325,
  18601, 18602, 18761, 18762, 18764, 18765,
};

static const unsigned short dep57[] = {
  11, 12, 13, 14, 19, 20, 40, 41, 97, 158, 162, 175, 185, 210, 212, 213, 215,
  216, 218, 219, 221, 282, 2135, 2136, 2137, 2166, 2167, 2170, 2173, 2325, 4135,
  16528, 16530, 16531, 16533, 18761, 18763, 18764, 18766,
};

static const unsigned short dep58[] = {
  16, 97, 213, 214, 282, 2136, 2325, 18601, 18602, 18761, 18762, 18764, 18765,

};

static const unsigned short dep59[] = {
  12, 19, 20, 40, 41, 97, 158, 162, 175, 185, 213, 215, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 2325, 4135, 16528, 16530, 16531, 16533, 18761, 18763,
  18764, 18766,
};

static const unsigned short dep60[] = {
  17, 97, 216, 217, 282, 2136, 2325, 18601, 18602, 18761, 18762, 18764, 18765,

};

static const unsigned short dep61[] = {
  13, 19, 20, 40, 41, 97, 158, 162, 175, 185, 216, 218, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 2325, 4135, 16528, 16530, 16531, 16533, 18761, 18763,
  18764, 18766,
};

static const unsigned short dep62[] = {
  18, 97, 219, 220, 282, 2136, 2325, 18601, 18602, 18761, 18762, 18764, 18765,

};

static const unsigned short dep63[] = {
  14, 19, 20, 40, 41, 97, 158, 162, 175, 185, 219, 221, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 2325, 4135, 16528, 16530, 16531, 16533, 18761, 18763,
  18764, 18766,
};

static const unsigned short dep64[] = {
  97, 282, 2136, 2325, 18601, 18602, 18761, 18762, 18764, 18765,
};

static const unsigned short dep65[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2135, 2136, 2137, 2166, 2167, 2170, 2173,
  2325, 4135, 16528, 16530, 16531, 16533, 18761, 18763, 18764, 18766,
};

static const unsigned short dep66[] = {
  11, 97, 206, 282,
};

static const unsigned short dep67[] = {
  11, 40, 41, 97, 158, 162, 175, 185, 206, 282, 2166, 2167, 2170, 2173, 4135,

};

static const unsigned short dep68[] = {
  11, 40, 41, 97, 158, 162, 175, 185, 282, 2166, 2167, 2170, 2173, 4135,
};

static const unsigned short dep69[] = {
  12, 97, 207, 282,
};

static const unsigned short dep70[] = {
  11, 40, 41, 97, 158, 162, 175, 185, 207, 282, 2166, 2167, 2170, 2173, 4135,

};

static const unsigned short dep71[] = {
  13, 97, 208, 282,
};

static const unsigned short dep72[] = {
  11, 40, 41, 97, 158, 162, 175, 185, 208, 282, 2166, 2167, 2170, 2173, 4135,

};

static const unsigned short dep73[] = {
  14, 97, 209, 282,
};

static const unsigned short dep74[] = {
  11, 40, 41, 97, 158, 162, 175, 185, 209, 282, 2166, 2167, 2170, 2173, 4135,

};

static const unsigned short dep75[] = {
  15, 97, 211, 212, 282,
};

static const unsigned short dep76[] = {
  40, 41, 97, 158, 162, 175, 185, 211, 212, 282, 2166, 2167, 2170, 2173, 4135,

};

static const unsigned short dep77[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2166, 2167, 2170, 2173, 4135,
};

static const unsigned short dep78[] = {
  16, 97, 214, 215, 282,
};

static const unsigned short dep79[] = {
  40, 41, 97, 158, 162, 175, 185, 214, 215, 282, 2166, 2167, 2170, 2173, 4135,

};

static const unsigned short dep80[] = {
  17, 97, 217, 218, 282,
};

static const unsigned short dep81[] = {
  40, 41, 97, 158, 162, 175, 185, 217, 218, 282, 2166, 2167, 2170, 2173, 4135,

};

static const unsigned short dep82[] = {
  18, 97, 220, 221, 282,
};

static const unsigned short dep83[] = {
  40, 41, 97, 158, 162, 175, 185, 220, 221, 282, 2166, 2167, 2170, 2173, 4135,

};

static const unsigned short dep84[] = {
  15, 19, 20, 40, 41, 97, 158, 162, 164, 175, 185, 186, 188, 282, 2166, 2167,
  2170, 2173, 4135,
};

static const unsigned short dep85[] = {
  15, 16, 19, 20, 40, 41, 97, 158, 162, 164, 175, 185, 186, 188, 282, 2166,
  2167, 2170, 2173, 4135,
};

static const unsigned short dep86[] = {
  15, 17, 19, 20, 40, 41, 97, 158, 162, 164, 175, 185, 186, 188, 282, 2166,
  2167, 2170, 2173, 4135,
};

static const unsigned short dep87[] = {
  15, 18, 19, 20, 40, 41, 97, 158, 162, 164, 175, 185, 186, 188, 282, 2166,
  2167, 2170, 2173, 4135,
};

static const unsigned short dep88[] = {
  15, 97, 210, 211, 282,
};

static const unsigned short dep89[] = {
  11, 19, 20, 40, 41, 97, 158, 162, 175, 185, 210, 212, 282, 2166, 2167, 2170,
  2173, 4135,
};

static const unsigned short dep90[] = {
  15, 16, 17, 18, 97, 210, 211, 213, 214, 216, 217, 219, 220, 282,
};

static const unsigned short dep91[] = {
  11, 12, 13, 14, 19, 20, 40, 41, 97, 158, 162, 175, 185, 210, 212, 213, 215,
  216, 218, 219, 221, 282, 2166, 2167, 2170, 2173, 4135,
};

static const unsigned short dep92[] = {
  16, 97, 213, 214, 282,
};

static const unsigned short dep93[] = {
  12, 19, 20, 40, 41, 97, 158, 162, 175, 185, 213, 215, 282, 2166, 2167, 2170,
  2173, 4135,
};

static const unsigned short dep94[] = {
  17, 97, 216, 217, 282,
};

static const unsigned short dep95[] = {
  13, 19, 20, 40, 41, 97, 158, 162, 175, 185, 216, 218, 282, 2166, 2167, 2170,
  2173, 4135,
};

static const unsigned short dep96[] = {
  18, 97, 219, 220, 282,
};

static const unsigned short dep97[] = {
  14, 19, 20, 40, 41, 97, 158, 162, 175, 185, 219, 221, 282, 2166, 2167, 2170,
  2173, 4135,
};

static const unsigned short dep98[] = {
  15, 97, 210, 211, 282, 2166, 2167, 2168, 2170, 2171, 2173, 2174, 2344, 2347,
  2348, 2351, 2352, 2355, 2356,
};

static const unsigned short dep99[] = {
  11, 19, 20, 40, 41, 97, 158, 162, 175, 185, 210, 212, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 2344, 2347, 2348, 2351, 2352, 2355, 2356, 4135, 16528,
  16530, 16531, 16533,
};

static const unsigned short dep100[] = {
  15, 16, 17, 18, 97, 210, 211, 213, 214, 216, 217, 219, 220, 282, 2166, 2167,
  2168, 2170, 2171, 2173, 2174, 2344, 2347, 2348, 2351, 2352, 2355, 2356,
};

static const unsigned short dep101[] = {
  11, 12, 13, 14, 19, 20, 40, 41, 97, 158, 162, 175, 185, 210, 212, 213, 215,
  216, 218, 219, 221, 282, 2135, 2136, 2137, 2166, 2167, 2170, 2173, 2344, 2347,
  2348, 2351, 2352, 2355, 2356, 4135, 16528, 16530, 16531, 16533,
};

static const unsigned short dep102[] = {
  16, 97, 213, 214, 282, 2166, 2167, 2168, 2170, 2171, 2173, 2174, 2344, 2347,
  2348, 2351, 2352, 2355, 2356,
};

static const unsigned short dep103[] = {
  12, 19, 20, 40, 41, 97, 158, 162, 175, 185, 213, 215, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 2344, 2347, 2348, 2351, 2352, 2355, 2356, 4135, 16528,
  16530, 16531, 16533,
};

static const unsigned short dep104[] = {
  17, 97, 216, 217, 282, 2166, 2167, 2168, 2170, 2171, 2173, 2174, 2344, 2347,
  2348, 2351, 2352, 2355, 2356,
};

static const unsigned short dep105[] = {
  13, 19, 20, 40, 41, 97, 158, 162, 175, 185, 216, 218, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 2344, 2347, 2348, 2351, 2352, 2355, 2356, 4135, 16528,
  16530, 16531, 16533,
};

static const unsigned short dep106[] = {
  18, 97, 219, 220, 282, 2166, 2167, 2168, 2170, 2171, 2173, 2174, 2344, 2347,
  2348, 2351, 2352, 2355, 2356,
};

static const unsigned short dep107[] = {
  14, 19, 20, 40, 41, 97, 158, 162, 175, 185, 219, 221, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 2344, 2347, 2348, 2351, 2352, 2355, 2356, 4135, 16528,
  16530, 16531, 16533,
};

static const unsigned short dep108[] = {
  15, 97, 210, 211, 282, 22646, 22647, 22648, 22650, 22651, 22653, 22654, 22824,
  22827, 22828, 22831, 22832, 22835, 22836,
};

static const unsigned short dep109[] = {
  11, 19, 20, 40, 41, 97, 158, 162, 175, 185, 210, 212, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 4135, 16528, 16530, 16531, 16533, 22824, 22827, 22828,
  22831, 22832, 22835, 22836,
};

static const unsigned short dep110[] = {
  15, 16, 17, 18, 97, 210, 211, 213, 214, 216, 217, 219, 220, 282, 22646, 22647,
  22648, 22650, 22651, 22653, 22654, 22824, 22827, 22828, 22831, 22832, 22835,
  22836,
};

static const unsigned short dep111[] = {
  11, 12, 13, 14, 19, 20, 40, 41, 97, 158, 162, 175, 185, 210, 212, 213, 215,
  216, 218, 219, 221, 282, 2135, 2136, 2137, 2166, 2167, 2170, 2173, 4135, 16528,
  16530, 16531, 16533, 22824, 22827, 22828, 22831, 22832, 22835, 22836,
};

static const unsigned short dep112[] = {
  16, 97, 213, 214, 282, 22646, 22647, 22648, 22650, 22651, 22653, 22654, 22824,
  22827, 22828, 22831, 22832, 22835, 22836,
};

static const unsigned short dep113[] = {
  12, 19, 20, 40, 41, 97, 158, 162, 175, 185, 213, 215, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 4135, 16528, 16530, 16531, 16533, 22824, 22827, 22828,
  22831, 22832, 22835, 22836,
};

static const unsigned short dep114[] = {
  17, 97, 216, 217, 282, 22646, 22647, 22648, 22650, 22651, 22653, 22654, 22824,
  22827, 22828, 22831, 22832, 22835, 22836,
};

static const unsigned short dep115[] = {
  13, 19, 20, 40, 41, 97, 158, 162, 175, 185, 216, 218, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 4135, 16528, 16530, 16531, 16533, 22824, 22827, 22828,
  22831, 22832, 22835, 22836,
};

static const unsigned short dep116[] = {
  18, 97, 219, 220, 282, 22646, 22647, 22648, 22650, 22651, 22653, 22654, 22824,
  22827, 22828, 22831, 22832, 22835, 22836,
};

static const unsigned short dep117[] = {
  14, 19, 20, 40, 41, 97, 158, 162, 175, 185, 219, 221, 282, 2135, 2136, 2137,
  2166, 2167, 2170, 2173, 4135, 16528, 16530, 16531, 16533, 22824, 22827, 22828,
  22831, 22832, 22835, 22836,
};

static const unsigned short dep118[] = {
  97, 282, 2166, 2167, 2168, 2170, 2171, 2173, 2174, 2344, 2347, 2348, 2351,
  2352, 2355, 2356,
};

static const unsigned short dep119[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2135, 2136, 2137, 2166, 2167, 2170, 2173,
  2344, 2347, 2348, 2351, 2352, 2355, 2356, 4135, 16528, 16530, 16531, 16533,

};

static const unsigned short dep120[] = {
  97, 282, 22646, 22647, 22648, 22650, 22651, 22653, 22654, 22824, 22827, 22828,
  22831, 22832, 22835, 22836,
};

static const unsigned short dep121[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2135, 2136, 2137, 2166, 2167, 2170, 2173,
  4135, 16528, 16530, 16531, 16533, 22824, 22827, 22828, 22831, 22832, 22835,
  22836,
};

static const unsigned short dep122[] = {
  19, 20, 40, 41, 97, 158, 162, 175, 185, 282, 2135, 2136, 2137, 2166, 2167,
  2170, 2173, 2325, 4135, 16528, 16530, 16531, 16533, 18761, 18763, 18764, 18766,

};

static const unsigned short dep123[] = {
  40, 41, 97, 158, 162, 164, 175, 185, 186, 188, 282, 2138, 2139, 2140, 2166,
  2167, 2170, 2173, 4135, 20616,
};

static const unsigned short dep124[] = {
  97, 282, 2083, 2084, 2286, 2287,
};

static const unsigned short dep125[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173,
  2285, 2287, 4135, 20616,
};

static const unsigned short dep126[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2082, 2084, 2166, 2167, 2170, 2173, 2327,
  4135, 20616,
};

static const unsigned short dep127[] = {
  97, 282, 14455, 14457, 14458, 14460, 14461, 14463, 14635, 14636, 14639, 14640,
  14643, 14644,
};

static const unsigned short dep128[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2138, 2139, 2140, 4135, 14635, 14636,
  14639, 14640, 14643, 14644, 20616, 24694, 24695, 24698, 24701,
};

static const unsigned short dep129[] = {
  97, 122, 124, 125, 127, 282, 303, 304, 307, 308,
};

static const unsigned short dep130[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 303, 304, 307, 308, 4135, 24694, 24695,
  24698, 24701,
};

static const unsigned short dep131[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2166, 2167, 2170, 2173, 2327, 4135, 20616,

};

static const unsigned short dep132[] = {
  40, 41, 97, 119, 122, 125, 158, 162, 175, 185, 282, 2327, 4135, 20616, 24694,

};

static const unsigned short dep133[] = {
  6, 24, 26, 27, 97, 201, 227, 230, 282, 2081, 2284,
};

static const unsigned short dep134[] = {
  40, 41, 97, 158, 162, 175, 185, 201, 227, 229, 282, 2138, 2139, 2140, 2166,
  2167, 2170, 2173, 2284, 4135, 20616,
};

static const unsigned short dep135[] = {
  6, 24, 25, 26, 40, 41, 97, 158, 162, 175, 185, 282, 2081, 2166, 2167, 2170,
  2173, 2327, 4135, 20616,
};

static const unsigned short dep136[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2166, 2167, 2170, 2173, 2344, 2347, 2348,
  2351, 2352, 2355, 2356, 4135,
};

static const unsigned short dep137[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2166, 2167, 2170, 2173, 4135, 22824,
  22827, 22828, 22831, 22832, 22835, 22836,
};

static const unsigned short dep138[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2166, 2167, 2170, 2173, 2344, 2345, 2348,
  2349, 2352, 2353, 2356, 4135,
};

static const unsigned short dep139[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2166, 2167, 2170, 2173, 2344, 2346, 2347,
  2350, 2351, 2354, 2355, 4135,
};

static const unsigned short dep140[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2166, 2167, 2170, 2173, 2344, 2345, 2346,
  2347, 2348, 2349, 2350, 2351, 2352, 2353, 2354, 2355, 2356, 4135,
};

static const unsigned short dep141[] = {
  0, 40, 41, 97, 158, 162, 164, 175, 185, 186, 188, 282, 2166, 2167, 2170, 2173,
  4135,
};

static const unsigned short dep142[] = {
  0, 97, 195, 282,
};

static const unsigned short dep143[] = {
  0, 40, 41, 97, 158, 162, 164, 175, 185, 186, 188, 195, 282, 2166, 2167, 2170,
  2173, 4135,
};

static const unsigned short dep144[] = {
  40, 41, 97, 158, 162, 175, 185, 195, 282, 2166, 2167, 2170, 2173, 4135,
};

static const unsigned short dep145[] = {
  2, 28, 97, 197, 231, 282, 28866, 29018,
};

static const unsigned short dep146[] = {
  1, 2, 28, 29, 97, 158, 162, 175, 177, 178, 185, 197, 231, 282, 28866, 29018,

};

static const unsigned short dep147[] = {
  1, 28, 29, 38, 40, 41, 97, 158, 162, 175, 177, 178, 185, 197, 231, 282, 4135,
  28866, 29018,
};

static const unsigned short dep148[] = {
  0, 40, 41, 97, 158, 162, 175, 185, 195, 282, 2166, 2167, 2170, 2173, 4135,

};

static const unsigned short dep149[] = {
  1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
  28, 29, 30, 31, 97, 196, 197, 198, 199, 200, 202, 203, 204, 205, 206, 207,
  208, 209, 211, 212, 214, 215, 217, 218, 220, 221, 222, 223, 224, 225, 231,
  232, 233, 234, 282, 2071, 2081, 2274, 2284, 28866, 29018,
};

static const unsigned short dep150[] = {
  29, 40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 196, 197, 198, 199,
  200, 202, 203, 204, 205, 206, 207, 208, 209, 211, 212, 214, 215, 217, 218,
  220, 221, 222, 223, 224, 225, 231, 232, 233, 234, 282, 2138, 2139, 2140, 2166,
  2167, 2170, 2173, 2274, 2284, 4135, 20616, 28866, 29018,
};

static const unsigned short dep151[] = {
  97, 282, 14464, 14466, 14468, 14470, 14505, 14506, 14525, 14645, 14646, 14666,
  14667, 14669, 14670, 14679,
};

static const unsigned short dep152[] = {
  40, 41, 97, 158, 162, 175, 183, 184, 185, 282, 2166, 2167, 2170, 2173, 4135,
  14645, 14646, 14666, 14667, 14669, 14670, 14679,
};

static const unsigned short dep153[] = {
  14464, 14466, 14468, 14470, 14505, 14506, 14525, 14645, 14646, 14666, 14667,
  14669, 14670, 14679,
};

static const unsigned short dep154[] = {
  183, 184, 14645, 14646, 14666, 14667, 14669, 14670, 14679,
};

static const unsigned short dep155[] = {
  97, 282, 14465, 14466, 14469, 14470, 14480, 14481, 14483, 14484, 14486, 14487,
  14489, 14490, 14493, 14495, 14496, 14505, 14506, 14507, 14508, 14510, 14515,
  14516, 14518, 14519, 14525, 14645, 14646, 14652, 14653, 14654, 14655, 14657,
  14659, 14666, 14667, 14669, 14670, 14671, 14672, 14675, 14676, 14679,
};

static const unsigned short dep156[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 2166, 2167, 2170,
  2173, 4135, 14645, 14646, 14652, 14653, 14654, 14655, 14657, 14659, 14666,
  14667, 14669, 14670, 14671, 14672, 14675, 14676, 14679, 34888,
};

static const unsigned short dep157[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 2166, 2167, 2170,
  2173, 4135, 14645, 14646, 14652, 14653, 14654, 14655, 14657, 14659, 14666,
  14667, 14669, 14670, 14671, 14672, 14675, 14676, 14679,
};

static const unsigned short dep158[] = {
  1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
  28, 29, 30, 31, 40, 41, 97, 137, 138, 158, 162, 175, 180, 181, 185, 190, 191,
  282, 2071, 2081, 2166, 2167, 2170, 2173, 2327, 4135, 20616, 28866,
};

static const unsigned short dep159[] = {
  43, 44, 45, 46, 47, 48, 49, 50, 52, 53, 54, 55, 56, 57, 58, 60, 61, 62, 63,
  64, 65, 67, 69, 70, 71, 72, 73, 94, 96, 97, 243, 244, 245, 246, 247, 248,
  249, 250, 251, 252, 253, 255, 256, 257, 258, 259, 261, 263, 264, 265, 281,
  282, 2116, 2310,
};

static const unsigned short dep160[] = {
  40, 41, 96, 97, 137, 138, 158, 160, 161, 162, 175, 185, 190, 191, 243, 244,
  245, 246, 247, 248, 249, 250, 251, 252, 253, 255, 256, 257, 258, 259, 261,
  263, 264, 265, 281, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2310, 4135,
  20616,
};

static const unsigned short dep161[] = {
  59, 95, 97, 254, 281, 282, 2140, 2327,
};

static const unsigned short dep162[] = {
  40, 41, 43, 44, 46, 48, 49, 51, 52, 53, 54, 56, 57, 60, 61, 63, 64, 65, 66,
  67, 69, 70, 71, 94, 95, 97, 137, 138, 158, 160, 161, 162, 175, 185, 190, 191,
  254, 281, 282, 2107, 2116, 2166, 2167, 2170, 2173, 2327, 4135, 20616,
};

static const unsigned short dep163[] = {
  2, 28, 41, 97, 197, 231, 241, 282, 2140, 2327, 28866, 29018,
};

static const unsigned short dep164[] = {
  2, 25, 26, 28, 29, 38, 40, 41, 97, 158, 162, 175, 177, 178, 185, 197, 231,
  241, 282, 2327, 4135, 20616, 28866, 29018,
};

static const unsigned short dep165[] = {
  97, 129, 130, 133, 134, 140, 141, 144, 145, 147, 148, 150, 151, 153, 154,
  157, 159, 160, 165, 166, 169, 170, 171, 172, 174, 176, 177, 179, 180, 182,
  183, 186, 187, 189, 282, 309, 310, 314, 316, 317, 318, 319, 321, 323, 327,
  330, 331, 333, 334, 335, 336, 338, 339, 340, 342, 343,
};

static const unsigned short dep166[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 309, 310, 314, 316,
  317, 318, 319, 321, 323, 327, 330, 331, 333, 334, 335, 336, 338, 339, 340,
  342, 343, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 4135, 20616, 34888,
};

static const unsigned short dep167[] = {
  97, 128, 130, 132, 134, 169, 170, 189, 282, 309, 310, 330, 331, 333, 334,
  343,
};

static const unsigned short dep168[] = {
  40, 41, 97, 158, 162, 175, 183, 184, 185, 282, 309, 310, 330, 331, 333, 334,
  343, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 4135, 20616,
};

static const unsigned short dep169[] = {
  40, 41, 97, 130, 131, 134, 135, 137, 138, 141, 142, 145, 146, 148, 149, 151,
  152, 154, 155, 157, 158, 159, 161, 162, 164, 165, 167, 168, 169, 170, 172,
  173, 174, 175, 176, 178, 179, 181, 182, 184, 185, 187, 188, 189, 190, 191,
  282, 2166, 2167, 2170, 2173, 2327, 4135, 20616,
};

static const unsigned short dep170[] = {
  40, 41, 97, 130, 131, 134, 135, 158, 162, 169, 170, 175, 185, 189, 282, 2166,
  2167, 2170, 2173, 2327, 4135, 20616,
};

static const unsigned short dep171[] = {
  40, 41, 70, 76, 77, 82, 84, 97, 111, 137, 138, 153, 155, 158, 162, 171, 173,
  175, 185, 192, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2327, 4135,
  20616,
};

static const unsigned short dep172[] = {
  40, 41, 70, 76, 77, 82, 84, 97, 111, 137, 138, 139, 140, 142, 143, 153, 155,
  158, 162, 171, 173, 175, 185, 192, 282, 2138, 2139, 2140, 2166, 2167, 2170,
  2173, 4135, 20616,
};

static const unsigned short dep173[] = {
  77, 78, 97, 101, 102, 269, 270, 282, 284, 285,
};

static const unsigned short dep174[] = {
  40, 41, 47, 62, 78, 80, 86, 97, 99, 102, 137, 138, 158, 160, 161, 162, 175,
  185, 190, 191, 192, 269, 270, 282, 284, 285, 2138, 2139, 2140, 2166, 2167,
  2170, 2173, 4135, 20616,
};

static const unsigned short dep175[] = {
  40, 41, 47, 62, 78, 80, 97, 99, 102, 104, 106, 137, 138, 158, 160, 161, 162,
  175, 185, 190, 191, 192, 269, 270, 282, 284, 285, 2138, 2139, 2140, 2166,
  2167, 2170, 2173, 4135, 20616,
};

static const unsigned short dep176[] = {
  97, 282, 12480, 12481, 12633,
};

static const unsigned short dep177[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 4135, 12633, 20616,
};

static const unsigned short dep178[] = {
  97, 282, 6219, 6220, 6411,
};

static const unsigned short dep179[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 4135, 6411, 20616,
};

static const unsigned short dep180[] = {
  97, 282, 6237, 6424,
};

static const unsigned short dep181[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 4135, 6424, 20616,
};

static const unsigned short dep182[] = {
  97, 282, 6255, 6256, 6257, 6258, 6435, 6437, 8484,
};

static const unsigned short dep183[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 4135, 6258, 6436, 6437, 8304, 8483, 20616,
};

static const unsigned short dep184[] = {
  97, 282, 6259, 6260, 6438,
};

static const unsigned short dep185[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 4135, 6438, 20616,
};

static const unsigned short dep186[] = {
  97, 282, 6261, 6439,
};

static const unsigned short dep187[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 4135, 6439, 20616,
};

static const unsigned short dep188[] = {
  97, 282, 10350, 10530,
};

static const unsigned short dep189[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 4135, 10530, 20616,
};

static const unsigned short dep190[] = {
  77, 78, 82, 83, 97, 101, 102, 269, 270, 272, 273, 282, 284, 285,
};

static const unsigned short dep191[] = {
  40, 41, 47, 62, 78, 80, 83, 86, 97, 99, 102, 137, 138, 158, 160, 161, 162,
  175, 185, 190, 191, 192, 269, 270, 272, 274, 282, 284, 285, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 4135, 20616,
};

static const unsigned short dep192[] = {
  77, 78, 97, 101, 102, 104, 105, 269, 270, 282, 284, 285, 286, 287,
};

static const unsigned short dep193[] = {
  40, 41, 47, 62, 78, 80, 97, 99, 102, 104, 106, 137, 138, 158, 160, 161, 162,
  175, 185, 190, 191, 192, 269, 270, 282, 284, 285, 286, 287, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 4135, 20616,
};

static const unsigned short dep194[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 2327, 4135, 12481, 20616,
};

static const unsigned short dep195[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 2327, 4135, 6219, 20616,
};

static const unsigned short dep196[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 2327, 4135, 6237, 20616,
};

static const unsigned short dep197[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 2327, 4135, 6257, 8303, 20616,
};

static const unsigned short dep198[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 185, 190, 191, 282, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 2327, 4135, 6259, 20616,
};

static const unsigned short dep199[] = {
  40, 41, 97, 137, 138, 158, 162, 175, 183, 184, 185, 282, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 2327, 4135, 6260, 6261, 20616,
};

static const unsigned short dep200[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173,
  2327, 4135, 10350, 20616,
};

static const unsigned short dep201[] = {
  40, 41, 97, 158, 162, 175, 185, 190, 191, 282, 2138, 2139, 2140, 2166, 2167,
  2170, 2173, 2327, 4135, 6186, 20616,
};

static const unsigned short dep202[] = {
  77, 79, 80, 97, 98, 99, 100, 268, 269, 282, 283, 284,
};

static const unsigned short dep203[] = {
  40, 41, 78, 79, 83, 85, 97, 100, 102, 104, 107, 137, 138, 158, 162, 175, 185,
  190, 191, 192, 268, 270, 282, 283, 285, 2138, 2139, 2140, 2166, 2167, 2170,
  2173, 4135, 20616,
};

static const unsigned short dep204[] = {
  77, 79, 80, 81, 97, 98, 99, 100, 103, 268, 269, 271, 282, 283, 284,
};

static const unsigned short dep205[] = {
  40, 41, 78, 79, 81, 83, 85, 97, 100, 102, 103, 104, 107, 137, 138, 158, 162,
  175, 185, 190, 191, 192, 268, 270, 271, 282, 283, 285, 2138, 2139, 2140, 2166,
  2167, 2170, 2173, 4135, 20616,
};

static const unsigned short dep206[] = {
  77, 79, 80, 84, 85, 86, 97, 98, 99, 100, 268, 269, 274, 275, 282, 283, 284,

};

static const unsigned short dep207[] = {
  40, 41, 78, 79, 83, 85, 97, 100, 102, 137, 138, 158, 162, 175, 185, 190, 191,
  192, 268, 270, 273, 275, 282, 283, 285, 2138, 2139, 2140, 2166, 2167, 2170,
  2173, 4135, 20616,
};

static const unsigned short dep208[] = {
  77, 79, 80, 97, 98, 99, 100, 106, 107, 108, 268, 269, 282, 283, 284, 287,
  288,
};

static const unsigned short dep209[] = {
  40, 41, 78, 79, 97, 100, 102, 104, 107, 137, 138, 158, 162, 175, 185, 190,
  191, 192, 268, 270, 282, 283, 285, 286, 288, 2138, 2139, 2140, 2166, 2167,
  2170, 2173, 4135, 20616,
};

static const unsigned short dep210[] = {
  40, 41, 46, 70, 97, 158, 162, 175, 185, 190, 191, 192, 282, 2138, 2139, 2140,
  2166, 2167, 2170, 2173, 2327, 4135, 20616,
};

static const unsigned short dep211[] = {
  40, 41, 97, 158, 162, 175, 185, 190, 191, 192, 282, 2138, 2139, 2140, 2166,
  2167, 2170, 2173, 2327, 4135, 20616,
};

static const unsigned short dep212[] = {
  40, 41, 70, 77, 82, 84, 97, 137, 138, 153, 155, 158, 162, 175, 185, 190, 191,
  192, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2327, 4135, 20616,
};

static const unsigned short dep213[] = {
  40, 41, 97, 158, 162, 164, 175, 185, 186, 188, 282, 2135, 2136, 2137, 2138,
  2139, 2140, 2166, 2167, 2170, 2173, 4135, 16528, 16530, 16531, 16533, 20616,

};

static const unsigned short dep214[] = {
  40, 41, 70, 77, 82, 84, 97, 153, 155, 158, 162, 175, 185, 192, 282, 2138,
  2139, 2140, 2166, 2167, 2170, 2173, 4135, 20616,
};

static const unsigned short dep215[] = {
  40, 41, 78, 79, 97, 100, 137, 138, 158, 162, 175, 185, 190, 191, 268, 270,
  282, 283, 285, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 4135, 20616,
};

static const unsigned short dep216[] = {
  40, 41, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133, 135, 137,
  138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190, 191,
  192, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2327, 4135, 20616,
};

static const unsigned short dep217[] = {
  5, 97, 200, 282, 2140, 2327,
};

static const unsigned short dep218[] = {
  40, 41, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133, 135, 137,
  138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190, 191,
  192, 200, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2327, 4135, 20616,

};

static const unsigned short dep219[] = {
  40, 41, 44, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133, 135,
  137, 138, 139, 140, 142, 143, 153, 155, 156, 158, 162, 171, 173, 175, 185,
  190, 191, 192, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2327, 4135,
  20616,
};

static const unsigned short dep220[] = {
  0, 97, 195, 282, 2140, 2327,
};

static const unsigned short dep221[] = {
  0, 40, 41, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133, 135,
  137, 138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190,
  191, 192, 195, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2327, 4135,
  20616,
};

static const unsigned short dep222[] = {
  0, 40, 41, 44, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133,
  135, 137, 138, 139, 140, 142, 143, 153, 155, 156, 158, 162, 171, 173, 175,
  185, 190, 191, 192, 195, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2327,
  4135, 20616,
};

static const unsigned short dep223[] = {
  31, 40, 41, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133, 135,
  137, 138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190,
  191, 192, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2327, 4135, 20616,

};

static const unsigned short dep224[] = {
  0, 97, 195, 282, 2327, 26715,
};

static const unsigned short dep225[] = {
  0, 97, 109, 195, 282, 289,
};

static const unsigned short dep226[] = {
  0, 40, 41, 70, 76, 77, 82, 84, 97, 111, 128, 129, 131, 132, 133, 135, 137,
  138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190, 191,
  192, 195, 282, 289, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 4135, 20616,

};

static const unsigned short dep227[] = {
  0, 5, 40, 41, 70, 76, 77, 82, 84, 97, 111, 128, 129, 131, 132, 133, 135, 137,
  138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190, 191,
  192, 195, 282, 289, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 4135, 20616,

};

static const unsigned short dep228[] = {
  0, 31, 97, 109, 195, 234, 282, 289,
};

static const unsigned short dep229[] = {
  0, 40, 41, 70, 76, 77, 82, 84, 97, 111, 128, 129, 131, 132, 133, 135, 137,
  138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190, 191,
  192, 195, 234, 282, 289, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 4135, 20616,

};

static const unsigned short dep230[] = {
  0, 97, 109, 195, 282, 289, 2140, 2327,
};

static const unsigned short dep231[] = {
  0, 3, 40, 41, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133, 135,
  137, 138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190,
  191, 192, 195, 282, 289, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2327, 4135,
  20616,
};

static const unsigned short dep232[] = {
  0, 3, 5, 40, 41, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133,
  135, 137, 138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185,
  190, 191, 192, 195, 282, 289, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2327,
  4135, 20616,
};

static const unsigned short dep233[] = {
  0, 40, 41, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133, 135,
  137, 138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190,
  191, 192, 195, 282, 289, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2327, 4135,
  20616,
};

static const unsigned short dep234[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2135, 2136, 2137, 2166, 2167, 2170, 2173,
  2327, 4135, 16528, 16530, 16531, 16533, 20616,
};

static const unsigned short dep235[] = {
  0, 40, 41, 70, 76, 77, 82, 84, 97, 111, 128, 129, 131, 132, 133, 135, 137,
  138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190, 191,
  192, 195, 282, 289, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2327, 4135,
  20616,
};

static const unsigned short dep236[] = {
  0, 31, 97, 109, 195, 234, 282, 289, 2140, 2327,
};

static const unsigned short dep237[] = {
  0, 40, 41, 70, 76, 77, 82, 84, 97, 111, 128, 129, 131, 132, 133, 135, 137,
  138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190, 191,
  192, 195, 234, 282, 289, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2327, 4135,
  20616,
};

static const unsigned short dep238[] = {
  40, 41, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133, 135, 137,
  138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190, 191,
  192, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2325, 4135, 16528, 16530,
  16531, 16533, 18761, 18763, 18764, 18766, 20616,
};

static const unsigned short dep239[] = {
  40, 41, 44, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133, 135,
  137, 138, 139, 140, 142, 143, 153, 155, 156, 158, 162, 171, 173, 175, 185,
  190, 191, 192, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2325, 4135,
  16528, 16530, 16531, 16533, 18761, 18763, 18764, 18766, 20616,
};

static const unsigned short dep240[] = {
  0, 97, 195, 282, 2136, 2325, 18601, 18602, 18761, 18762, 18764, 18765,
};

static const unsigned short dep241[] = {
  0, 40, 41, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133, 135,
  137, 138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190,
  191, 192, 195, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2325, 4135,
  16528, 16530, 16531, 16533, 18761, 18763, 18764, 18766, 20616,
};

static const unsigned short dep242[] = {
  0, 40, 41, 44, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133,
  135, 137, 138, 139, 140, 142, 143, 153, 155, 156, 158, 162, 171, 173, 175,
  185, 190, 191, 192, 195, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2325,
  4135, 16528, 16530, 16531, 16533, 18761, 18763, 18764, 18766, 20616,
};

static const unsigned short dep243[] = {
  0, 97, 195, 282, 2137, 2325, 18601, 18602, 18761, 18762, 18764, 18765,
};

static const unsigned short dep244[] = {
  97, 282, 2136, 2140, 2325, 2327, 18601, 18602, 18761, 18762, 18764, 18765,

};

static const unsigned short dep245[] = {
  40, 41, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133, 135, 137,
  138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190, 191,
  192, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2325, 2327, 4135, 16528,
  16530, 16531, 16533, 18761, 18763, 18764, 18766, 20616,
};

static const unsigned short dep246[] = {
  40, 41, 44, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133, 135,
  137, 138, 139, 140, 142, 143, 153, 155, 156, 158, 162, 171, 173, 175, 185,
  190, 191, 192, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2325, 2327,
  4135, 16528, 16530, 16531, 16533, 18761, 18763, 18764, 18766, 20616,
};

static const unsigned short dep247[] = {
  0, 97, 195, 282, 2136, 2140, 2325, 2327, 18601, 18602, 18761, 18762, 18764,
  18765,
};

static const unsigned short dep248[] = {
  0, 40, 41, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133, 135,
  137, 138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190,
  191, 192, 195, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2325, 2327,
  4135, 16528, 16530, 16531, 16533, 18761, 18763, 18764, 18766, 20616,
};

static const unsigned short dep249[] = {
  0, 40, 41, 44, 70, 76, 77, 82, 84, 97, 109, 111, 128, 129, 131, 132, 133,
  135, 137, 138, 139, 140, 142, 143, 153, 155, 156, 158, 162, 171, 173, 175,
  185, 190, 191, 192, 195, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173, 2325,
  2327, 4135, 16528, 16530, 16531, 16533, 18761, 18763, 18764, 18766, 20616,

};

static const unsigned short dep250[] = {
  0, 97, 195, 282, 2137, 2140, 2325, 2327, 18601, 18602, 18761, 18762, 18764,
  18765,
};

static const unsigned short dep251[] = {
  0, 40, 41, 70, 76, 77, 82, 84, 97, 111, 128, 129, 131, 132, 133, 135, 137,
  138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190, 191,
  192, 195, 282, 289, 2135, 2136, 2137, 2138, 2139, 2140, 2166, 2167, 2170,
  2173, 4135, 16528, 16530, 16531, 16533, 20616,
};

static const unsigned short dep252[] = {
  40, 41, 70, 76, 77, 82, 84, 97, 137, 138, 139, 140, 142, 143, 153, 155, 156,
  158, 162, 171, 173, 175, 185, 192, 282, 2166, 2167, 2170, 2173, 4135,
};

static const unsigned short dep253[] = {
  40, 41, 70, 76, 77, 82, 84, 97, 137, 138, 139, 140, 142, 143, 153, 155, 156,
  158, 162, 171, 173, 175, 185, 192, 282, 2138, 2139, 2140, 2166, 2167, 2170,
  2173, 2327, 4135, 20616,
};

static const unsigned short dep254[] = {
  40, 41, 97, 158, 162, 175, 185, 282, 2138, 2139, 2140, 2166, 2167, 2170, 2173,
  2325, 4135, 16528, 16530, 16531, 16533, 18761, 18763, 18764, 18766, 20616,

};

static const unsigned short dep255[] = {
  0, 40, 41, 70, 76, 77, 82, 84, 97, 111, 128, 129, 131, 132, 133, 135, 137,
  138, 139, 140, 142, 143, 153, 155, 158, 162, 171, 173, 175, 185, 190, 191,
  192, 195, 282, 289, 2135, 2136, 2137, 2138, 2139, 2140, 2166, 2167, 2170,
  2173, 2327, 4135, 16528, 16530, 16531, 16533, 20616,
};

static const unsigned short dep256[] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
  22, 24, 26, 27, 28, 29, 30, 31, 97, 196, 197, 198, 199, 200, 201, 202, 203,
  204, 205, 206, 207, 208, 209, 211, 212, 214, 215, 217, 218, 220, 221, 222,
  223, 224, 225, 227, 230, 231, 232, 233, 234, 282, 2071, 2081, 2140, 2274,
  2284, 2327, 28866, 29018,
};

static const unsigned short dep257[] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
  22, 24, 25, 26, 28, 29, 30, 31, 40, 41, 97, 137, 138, 158, 162, 175, 180,
  181, 185, 190, 191, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206,
  207, 208, 209, 211, 212, 214, 215, 217, 218, 220, 221, 222, 223, 224, 225,
  227, 229, 231, 232, 233, 234, 282, 2071, 2081, 2138, 2139, 2140, 2166, 2167,
  2170, 2173, 2274, 2284, 2327, 4135, 20616, 28866, 29018,
};

#define NELS(X) (sizeof(X)/sizeof(X[0]))
static const struct ia64_opcode_dependency
op_dependencies[] = {
  { NELS(dep1), dep1, NELS(dep0), dep0, },
  { NELS(dep3), dep3, NELS(dep2), dep2, },
  { NELS(dep5), dep5, NELS(dep4), dep4, },
  { NELS(dep7), dep7, NELS(dep6), dep6, },
  { NELS(dep9), dep9, NELS(dep8), dep8, },
  { NELS(dep11), dep11, NELS(dep10), dep10, },
  { NELS(dep13), dep13, NELS(dep12), dep12, },
  { NELS(dep15), dep15, NELS(dep14), dep14, },
  { NELS(dep17), dep17, NELS(dep16), dep16, },
  { NELS(dep19), dep19, NELS(dep18), dep18, },
  { NELS(dep21), dep21, NELS(dep20), dep20, },
  { NELS(dep23), dep23, NELS(dep22), dep22, },
  { NELS(dep25), dep25, NELS(dep24), dep24, },
  { NELS(dep27), dep27, NELS(dep26), dep26, },
  { NELS(dep29), dep29, NELS(dep28), dep28, },
  { NELS(dep30), dep30, NELS(dep12), dep12, },
  { NELS(dep32), dep32, NELS(dep31), dep31, },
  { NELS(dep34), dep34, NELS(dep33), dep33, },
  { NELS(dep35), dep35, NELS(dep12), dep12, },
  { NELS(dep37), dep37, NELS(dep36), dep36, },
  { NELS(dep39), dep39, NELS(dep38), dep38, },
  { NELS(dep41), dep41, NELS(dep40), dep40, },
  { NELS(dep42), dep42, NELS(dep31), dep31, },
  { NELS(dep43), dep43, NELS(dep33), dep33, },
  { NELS(dep45), dep45, NELS(dep44), dep44, },
  { NELS(dep47), dep47, NELS(dep46), dep46, },
  { NELS(dep49), dep49, NELS(dep48), dep48, },
  { NELS(dep51), dep51, NELS(dep50), dep50, },
  { NELS(dep53), dep53, NELS(dep52), dep52, },
  { NELS(dep55), dep55, NELS(dep54), dep54, },
  { NELS(dep57), dep57, NELS(dep56), dep56, },
  { NELS(dep59), dep59, NELS(dep58), dep58, },
  { NELS(dep61), dep61, NELS(dep60), dep60, },
  { NELS(dep63), dep63, NELS(dep62), dep62, },
  { NELS(dep65), dep65, NELS(dep64), dep64, },
  { NELS(dep67), dep67, NELS(dep66), dep66, },
  { NELS(dep68), dep68, NELS(dep33), dep33, },
  { NELS(dep70), dep70, NELS(dep69), dep69, },
  { NELS(dep72), dep72, NELS(dep71), dep71, },
  { NELS(dep74), dep74, NELS(dep73), dep73, },
  { NELS(dep76), dep76, NELS(dep75), dep75, },
  { NELS(dep77), dep77, NELS(dep33), dep33, },
  { NELS(dep79), dep79, NELS(dep78), dep78, },
  { NELS(dep81), dep81, NELS(dep80), dep80, },
  { NELS(dep83), dep83, NELS(dep82), dep82, },
  { NELS(dep84), dep84, NELS(dep33), dep33, },
  { NELS(dep85), dep85, NELS(dep33), dep33, },
  { NELS(dep86), dep86, NELS(dep33), dep33, },
  { NELS(dep87), dep87, NELS(dep33), dep33, },
  { NELS(dep89), dep89, NELS(dep88), dep88, },
  { NELS(dep91), dep91, NELS(dep90), dep90, },
  { NELS(dep93), dep93, NELS(dep92), dep92, },
  { NELS(dep95), dep95, NELS(dep94), dep94, },
  { NELS(dep97), dep97, NELS(dep96), dep96, },
  { NELS(dep99), dep99, NELS(dep98), dep98, },
  { NELS(dep101), dep101, NELS(dep100), dep100, },
  { NELS(dep103), dep103, NELS(dep102), dep102, },
  { NELS(dep105), dep105, NELS(dep104), dep104, },
  { NELS(dep107), dep107, NELS(dep106), dep106, },
  { NELS(dep109), dep109, NELS(dep108), dep108, },
  { NELS(dep111), dep111, NELS(dep110), dep110, },
  { NELS(dep113), dep113, NELS(dep112), dep112, },
  { NELS(dep115), dep115, NELS(dep114), dep114, },
  { NELS(dep117), dep117, NELS(dep116), dep116, },
  { NELS(dep119), dep119, NELS(dep118), dep118, },
  { NELS(dep121), dep121, NELS(dep120), dep120, },
  { NELS(dep122), dep122, NELS(dep64), dep64, },
  { NELS(dep123), dep123, NELS(dep33), dep33, },
  { NELS(dep125), dep125, NELS(dep124), dep124, },
  { NELS(dep126), dep126, NELS(dep0), dep0, },
  { NELS(dep128), dep128, NELS(dep127), dep127, },
  { NELS(dep130), dep130, NELS(dep129), dep129, },
  { NELS(dep131), dep131, NELS(dep0), dep0, },
  { NELS(dep132), dep132, NELS(dep0), dep0, },
  { NELS(dep134), dep134, NELS(dep133), dep133, },
  { NELS(dep135), dep135, NELS(dep0), dep0, },
  { NELS(dep136), dep136, NELS(dep2), dep2, },
  { NELS(dep137), dep137, NELS(dep4), dep4, },
  { NELS(dep138), dep138, NELS(dep6), dep6, },
  { NELS(dep139), dep139, NELS(dep8), dep8, },
  { NELS(dep140), dep140, NELS(dep10), dep10, },
  { NELS(dep141), dep141, NELS(dep33), dep33, },
  { NELS(dep143), dep143, NELS(dep142), dep142, },
  { NELS(dep144), dep144, NELS(dep142), dep142, },
  { NELS(dep146), dep146, NELS(dep145), dep145, },
  { NELS(dep147), dep147, NELS(dep145), dep145, },
  { NELS(dep148), dep148, NELS(dep142), dep142, },
  { NELS(dep150), dep150, NELS(dep149), dep149, },
  { NELS(dep152), dep152, NELS(dep151), dep151, },
  { NELS(dep154), dep154, NELS(dep153), dep153, },
  { NELS(dep156), dep156, NELS(dep155), dep155, },
  { NELS(dep157), dep157, NELS(dep155), dep155, },
  { NELS(dep158), dep158, NELS(dep0), dep0, },
  { NELS(dep160), dep160, NELS(dep159), dep159, },
  { NELS(dep162), dep162, NELS(dep161), dep161, },
  { NELS(dep164), dep164, NELS(dep163), dep163, },
  { NELS(dep166), dep166, NELS(dep165), dep165, },
  { NELS(dep168), dep168, NELS(dep167), dep167, },
  { NELS(dep169), dep169, NELS(dep0), dep0, },
  { NELS(dep170), dep170, NELS(dep0), dep0, },
  { NELS(dep171), dep171, NELS(dep0), dep0, },
  { NELS(dep172), dep172, NELS(dep33), dep33, },
  { NELS(dep174), dep174, NELS(dep173), dep173, },
  { NELS(dep175), dep175, NELS(dep173), dep173, },
  { NELS(dep177), dep177, NELS(dep176), dep176, },
  { NELS(dep179), dep179, NELS(dep178), dep178, },
  { NELS(dep181), dep181, NELS(dep180), dep180, },
  { NELS(dep183), dep183, NELS(dep182), dep182, },
  { NELS(dep185), dep185, NELS(dep184), dep184, },
  { NELS(dep187), dep187, NELS(dep186), dep186, },
  { NELS(dep189), dep189, NELS(dep188), dep188, },
  { NELS(dep191), dep191, NELS(dep190), dep190, },
  { NELS(dep193), dep193, NELS(dep192), dep192, },
  { NELS(dep194), dep194, NELS(dep0), dep0, },
  { NELS(dep195), dep195, NELS(dep0), dep0, },
  { NELS(dep196), dep196, NELS(dep0), dep0, },
  { NELS(dep197), dep197, NELS(dep0), dep0, },
  { NELS(dep198), dep198, NELS(dep0), dep0, },
  { NELS(dep199), dep199, NELS(dep0), dep0, },
  { NELS(dep200), dep200, NELS(dep0), dep0, },
  { NELS(dep201), dep201, NELS(dep0), dep0, },
  { NELS(dep203), dep203, NELS(dep202), dep202, },
  { NELS(dep205), dep205, NELS(dep204), dep204, },
  { NELS(dep207), dep207, NELS(dep206), dep206, },
  { NELS(dep209), dep209, NELS(dep208), dep208, },
  { NELS(dep210), dep210, NELS(dep0), dep0, },
  { NELS(dep211), dep211, NELS(dep0), dep0, },
  { NELS(dep212), dep212, NELS(dep0), dep0, },
  { NELS(dep213), dep213, NELS(dep33), dep33, },
  { NELS(dep214), dep214, NELS(dep33), dep33, },
  { NELS(dep215), dep215, NELS(dep202), dep202, },
  { NELS(dep216), dep216, NELS(dep0), dep0, },
  { NELS(dep218), dep218, NELS(dep217), dep217, },
  { NELS(dep219), dep219, NELS(dep0), dep0, },
  { NELS(dep221), dep221, NELS(dep220), dep220, },
  { NELS(dep222), dep222, NELS(dep220), dep220, },
  { NELS(dep223), dep223, NELS(dep0), dep0, },
  { NELS(dep221), dep221, NELS(dep224), dep224, },
  { NELS(dep226), dep226, NELS(dep225), dep225, },
  { NELS(dep227), dep227, NELS(dep225), dep225, },
  { NELS(dep229), dep229, NELS(dep228), dep228, },
  { NELS(dep231), dep231, NELS(dep230), dep230, },
  { NELS(dep232), dep232, NELS(dep230), dep230, },
  { NELS(dep233), dep233, NELS(dep230), dep230, },
  { NELS(dep234), dep234, NELS(dep0), dep0, },
  { NELS(dep235), dep235, NELS(dep230), dep230, },
  { NELS(dep237), dep237, NELS(dep236), dep236, },
  { NELS(dep238), dep238, NELS(dep64), dep64, },
  { NELS(dep239), dep239, NELS(dep64), dep64, },
  { NELS(dep241), dep241, NELS(dep240), dep240, },
  { NELS(dep242), dep242, NELS(dep240), dep240, },
  { NELS(dep241), dep241, NELS(dep243), dep243, },
  { NELS(dep245), dep245, NELS(dep244), dep244, },
  { NELS(dep246), dep246, NELS(dep244), dep244, },
  { NELS(dep248), dep248, NELS(dep247), dep247, },
  { NELS(dep249), dep249, NELS(dep247), dep247, },
  { NELS(dep248), dep248, NELS(dep250), dep250, },
  { NELS(dep251), dep251, NELS(dep225), dep225, },
  { NELS(dep252), dep252, NELS(dep33), dep33, },
  { NELS(dep253), dep253, NELS(dep0), dep0, },
  { NELS(dep254), dep254, NELS(dep64), dep64, },
  { NELS(dep255), dep255, NELS(dep230), dep230, },
  { 0, NULL, 0, NULL, },
  { NELS(dep257), dep257, NELS(dep256), dep256, },
};

static const struct ia64_completer_table
completer_table[] = {
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 95 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 95 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, 594, -1, 0, 1, 6 },
  { 0x0, 0x0, 0, 657, -1, 0, 1, 18 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 162 },
  { 0x0, 0x0, 0, 756, -1, 0, 1, 18 },
  { 0x0, 0x0, 0, 2198, -1, 0, 1, 10 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 9 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 13 },
  { 0x1, 0x1, 0, -1, -1, 13, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 34 },
  { 0x0, 0x0, 0, 2406, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 34 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 34 },
  { 0x0, 0x0, 0, 1140, -1, 0, 1, 129 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 45 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 41 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 84 },
  { 0x0, 0x0, 0, 2246, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, 2473, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, 2250, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 34 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 34 },
  { 0x0, 0x0, 0, 2252, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, 2482, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, 2485, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 34 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 34 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 34 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, 2507, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 34 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 34 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, 2510, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 25 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 25 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 25 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 25 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 34 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 36 },
  { 0x0, 0x0, 0, 2518, -1, 0, 1, 30 },
  { 0x0, 0x0, 0, 1409, -1, 0, 1, 34 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 41 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 34 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 162 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 83 },
  { 0x0, 0x0, 0, 1457, -1, 0, 1, 131 },
  { 0x0, 0x0, 0, 1466, -1, 0, 1, 131 },
  { 0x0, 0x0, 0, 1475, -1, 0, 1, 131 },
  { 0x0, 0x0, 0, 1477, -1, 0, 1, 132 },
  { 0x0, 0x0, 0, 1479, -1, 0, 1, 132 },
  { 0x0, 0x0, 0, 1488, -1, 0, 1, 131 },
  { 0x0, 0x0, 0, 1497, -1, 0, 1, 131 },
  { 0x0, 0x0, 0, 1506, -1, 0, 1, 131 },
  { 0x0, 0x0, 0, 1515, -1, 0, 1, 131 },
  { 0x0, 0x0, 0, 1524, -1, 0, 1, 131 },
  { 0x0, 0x0, 0, 1533, -1, 0, 1, 131 },
  { 0x0, 0x0, 0, 1543, -1, 0, 1, 131 },
  { 0x0, 0x0, 0, 1553, -1, 0, 1, 131 },
  { 0x0, 0x0, 0, 1563, -1, 0, 1, 131 },
  { 0x0, 0x0, 0, 1572, -1, 0, 1, 147 },
  { 0x0, 0x0, 0, 1578, -1, 0, 1, 152 },
  { 0x0, 0x0, 0, 1584, -1, 0, 1, 152 },
  { 0x0, 0x0, 0, 1590, -1, 0, 1, 147 },
  { 0x0, 0x0, 0, 1596, -1, 0, 1, 152 },
  { 0x0, 0x0, 0, 1602, -1, 0, 1, 152 },
  { 0x0, 0x0, 0, 1608, -1, 0, 1, 147 },
  { 0x0, 0x0, 0, 1614, -1, 0, 1, 152 },
  { 0x0, 0x0, 0, 1620, -1, 0, 1, 152 },
  { 0x0, 0x0, 0, 1626, -1, 0, 1, 147 },
  { 0x0, 0x0, 0, 1632, -1, 0, 1, 152 },
  { 0x0, 0x0, 0, 1638, -1, 0, 1, 147 },
  { 0x0, 0x0, 0, 1644, -1, 0, 1, 152 },
  { 0x0, 0x0, 0, 1650, -1, 0, 1, 147 },
  { 0x0, 0x0, 0, 1656, -1, 0, 1, 152 },
  { 0x0, 0x0, 0, 1662, -1, 0, 1, 147 },
  { 0x0, 0x0, 0, 1668, -1, 0, 1, 152 },
  { 0x0, 0x0, 0, 1674, -1, 0, 1, 152 },
  { 0x0, 0x0, 0, 1678, -1, 0, 1, 158 },
  { 0x0, 0x0, 0, 1682, -1, 0, 1, 159 },
  { 0x0, 0x0, 0, 1686, -1, 0, 1, 159 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 85 },
  { 0x0, 0x0, 0, 258, -1, 0, 1, 41 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 34 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 68 },
  { 0x1, 0x1, 0, 1166, -1, 20, 1, 68 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 69 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 70 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 70 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 71 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 72 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 73 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 93 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 94 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 96 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 97 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 98 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 99 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 104 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 105 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 106 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 107 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 108 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 109 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 110 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 113 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 114 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 115 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 116 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 117 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 118 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 119 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 120 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 163 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 163 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 163 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 72 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 162 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, 2858, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, 2859, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, 2210, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, 2211, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, 2873, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, 2874, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, 2875, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, 2876, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, 2877, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, 2860, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, 2861, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 11 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 91 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 89 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x1, 0x1, 0, -1, -1, 13, 1, 0 },
  { 0x0, 0x0, 0, 2879, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 90 },
  { 0x0, 0x0, 0, 1966, -1, 0, 1, 138 },
  { 0x0, 0x0, 0, 1968, -1, 0, 1, 145 },
  { 0x0, 0x0, 0, 1970, -1, 0, 1, 139 },
  { 0x0, 0x0, 0, 1972, -1, 0, 1, 139 },
  { 0x0, 0x0, 0, 1974, -1, 0, 1, 138 },
  { 0x0, 0x0, 0, 1976, -1, 0, 1, 145 },
  { 0x0, 0x0, 0, 1978, -1, 0, 1, 138 },
  { 0x0, 0x0, 0, 1980, -1, 0, 1, 145 },
  { 0x0, 0x0, 0, 1983, -1, 0, 1, 138 },
  { 0x0, 0x0, 0, 1986, -1, 0, 1, 145 },
  { 0x0, 0x0, 0, 1989, -1, 0, 1, 157 },
  { 0x0, 0x0, 0, 1990, -1, 0, 1, 161 },
  { 0x0, 0x0, 0, 1991, -1, 0, 1, 157 },
  { 0x0, 0x0, 0, 1992, -1, 0, 1, 161 },
  { 0x0, 0x0, 0, 1993, -1, 0, 1, 157 },
  { 0x0, 0x0, 0, 1994, -1, 0, 1, 161 },
  { 0x0, 0x0, 0, 1995, -1, 0, 1, 157 },
  { 0x0, 0x0, 0, 1996, -1, 0, 1, 161 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 88 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 127 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 125 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 127 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 126 },
  { 0x0, 0x0, 0, 1687, -1, 0, 1, 143 },
  { 0x0, 0x0, 0, 1688, -1, 0, 1, 143 },
  { 0x0, 0x0, 0, 1689, -1, 0, 1, 143 },
  { 0x0, 0x0, 0, 1690, -1, 0, 1, 143 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 0, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 1, 224, -1, 0, 1, 12 },
  { 0x0, 0x0, 1, 225, -1, 0, 1, 14 },
  { 0x1, 0x1, 2, -1, -1, 27, 1, 12 },
  { 0x1, 0x1, 2, -1, -1, 27, 1, 14 },
  { 0x0, 0x0, 3, -1, 1340, 0, 0, -1 },
  { 0x0, 0x0, 3, -1, 1341, 0, 0, -1 },
  { 0x1, 0x1, 3, 2749, 1450, 33, 1, 134 },
  { 0x1, 0x1, 3, 2750, 1459, 33, 1, 134 },
  { 0x1, 0x1, 3, 2751, 1468, 33, 1, 134 },
  { 0x1, 0x1, 3, 2752, 1481, 33, 1, 134 },
  { 0x1, 0x1, 3, 2753, 1490, 33, 1, 134 },
  { 0x1, 0x1, 3, 2754, 1499, 33, 1, 134 },
  { 0x1, 0x1, 3, 2755, 1508, 33, 1, 134 },
  { 0x1, 0x1, 3, 2756, 1517, 33, 1, 134 },
  { 0x1, 0x1, 3, 2757, 1526, 33, 1, 134 },
  { 0x1, 0x1, 3, 2758, 1535, 33, 1, 134 },
  { 0x1, 0x1, 3, 2759, 1545, 33, 1, 134 },
  { 0x1, 0x1, 3, 2760, 1555, 33, 1, 134 },
  { 0x1, 0x1, 3, 2761, 1568, 33, 1, 149 },
  { 0x1, 0x1, 3, 2762, 1574, 33, 1, 154 },
  { 0x1, 0x1, 3, 2763, 1580, 33, 1, 154 },
  { 0x1, 0x1, 3, 2764, 1586, 33, 1, 149 },
  { 0x1, 0x1, 3, 2765, 1592, 33, 1, 154 },
  { 0x1, 0x1, 3, 2766, 1598, 33, 1, 154 },
  { 0x1, 0x1, 3, 2767, 1604, 33, 1, 149 },
  { 0x1, 0x1, 3, 2768, 1610, 33, 1, 154 },
  { 0x1, 0x1, 3, 2769, 1616, 33, 1, 154 },
  { 0x1, 0x1, 3, 2770, 1622, 33, 1, 149 },
  { 0x1, 0x1, 3, 2771, 1628, 33, 1, 154 },
  { 0x1, 0x1, 3, 2772, 1634, 33, 1, 149 },
  { 0x1, 0x1, 3, 2773, 1640, 33, 1, 154 },
  { 0x1, 0x1, 3, 2774, 1646, 33, 1, 149 },
  { 0x1, 0x1, 3, 2775, 1652, 33, 1, 154 },
  { 0x1, 0x1, 3, 2776, 1658, 33, 1, 149 },
  { 0x1, 0x1, 3, 2777, 1664, 33, 1, 154 },
  { 0x1, 0x1, 3, 2778, 1670, 33, 1, 154 },
  { 0x1, 0x1, 3, -1, -1, 27, 1, 41 },
  { 0x0, 0x0, 4, 2212, 1425, 0, 1, 142 },
  { 0x0, 0x0, 4, 2213, 1427, 0, 1, 142 },
  { 0x0, 0x0, 4, 2214, 1429, 0, 1, 141 },
  { 0x0, 0x0, 4, 2215, 1431, 0, 1, 141 },
  { 0x0, 0x0, 4, 2216, 1433, 0, 1, 141 },
  { 0x0, 0x0, 4, 2217, 1435, 0, 1, 141 },
  { 0x0, 0x0, 4, 2218, 1437, 0, 1, 141 },
  { 0x0, 0x0, 4, 2219, 1439, 0, 1, 141 },
  { 0x0, 0x0, 4, 2220, 1441, 0, 1, 141 },
  { 0x0, 0x0, 4, 2221, 1443, 0, 1, 141 },
  { 0x0, 0x0, 4, 2222, 1445, 0, 1, 143 },
  { 0x0, 0x0, 4, 2223, 1447, 0, 1, 143 },
  { 0x1, 0x1, 4, -1, 1454, 33, 1, 137 },
  { 0x5, 0x5, 4, 552, 1453, 32, 1, 131 },
  { 0x1, 0x1, 4, -1, 1463, 33, 1, 137 },
  { 0x5, 0x5, 4, 553, 1462, 32, 1, 131 },
  { 0x1, 0x1, 4, -1, 1472, 33, 1, 137 },
  { 0x5, 0x5, 4, 554, 1471, 32, 1, 131 },
  { 0x1, 0x1, 4, -1, 1476, 32, 1, 132 },
  { 0x1, 0x1, 4, -1, 1478, 32, 1, 132 },
  { 0x1, 0x1, 4, -1, 1485, 33, 1, 137 },
  { 0x5, 0x5, 4, 555, 1484, 32, 1, 131 },
  { 0x1, 0x1, 4, -1, 1494, 33, 1, 137 },
  { 0x5, 0x5, 4, 556, 1493, 32, 1, 131 },
  { 0x1, 0x1, 4, -1, 1503, 33, 1, 137 },
  { 0x5, 0x5, 4, 557, 1502, 32, 1, 131 },
  { 0x1, 0x1, 4, -1, 1512, 33, 1, 137 },
  { 0x5, 0x5, 4, 558, 1511, 32, 1, 131 },
  { 0x1, 0x1, 4, -1, 1521, 33, 1, 137 },
  { 0x5, 0x5, 4, 559, 1520, 32, 1, 131 },
  { 0x1, 0x1, 4, -1, 1530, 33, 1, 137 },
  { 0x5, 0x5, 4, 560, 1529, 32, 1, 131 },
  { 0x1, 0x1, 4, -1, 1540, 33, 1, 137 },
  { 0x5, 0x5, 4, 1036, 1538, 32, 1, 131 },
  { 0x1, 0x1, 4, -1, 1550, 33, 1, 137 },
  { 0x5, 0x5, 4, 1037, 1548, 32, 1, 131 },
  { 0x1, 0x1, 4, -1, 1560, 33, 1, 137 },
  { 0x5, 0x5, 4, 1038, 1558, 32, 1, 131 },
  { 0x1, 0x21, 10, 2013, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2014, -1, 12, 1, 3 },
  { 0x1, 0x21, 10, 420, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2074, -1, 12, 1, 3 },
  { 0x0, 0x0, 10, -1, 2075, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2076, 0, 0, -1 },
  { 0x0, 0x0, 10, 2017, -1, 0, 1, 3 },
  { 0x1, 0x1, 10, 2018, -1, 12, 1, 3 },
  { 0x1, 0x1, 10, 2019, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2020, -1, 12, 1, 3 },
  { 0x0, 0x0, 10, 430, -1, 0, 1, 3 },
  { 0x1, 0x1, 10, 2080, -1, 12, 1, 3 },
  { 0x1, 0x1, 10, 434, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2082, -1, 12, 1, 3 },
  { 0x0, 0x0, 10, 438, -1, 0, 1, 3 },
  { 0x1, 0x1, 10, 2084, -1, 12, 1, 3 },
  { 0x1, 0x1, 10, 442, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2086, -1, 12, 1, 3 },
  { 0x0, 0x0, 10, 446, -1, 0, 1, 3 },
  { 0x1, 0x1, 10, 2088, -1, 12, 1, 3 },
  { 0x1, 0x1, 10, 450, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2090, -1, 12, 1, 3 },
  { 0x1, 0x21, 10, 2033, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2034, -1, 12, 1, 3 },
  { 0x1, 0x21, 10, 460, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2096, -1, 12, 1, 3 },
  { 0x0, 0x0, 10, -1, 2097, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2098, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2101, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2102, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2103, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2104, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2105, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2106, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2107, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2108, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2109, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2110, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2111, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2112, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2113, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2114, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2115, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2116, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2117, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2118, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2119, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2120, 0, 0, -1 },
  { 0x1, 0x21, 10, 2037, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2038, -1, 12, 1, 3 },
  { 0x1, 0x21, 10, 468, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2122, -1, 12, 1, 3 },
  { 0x0, 0x0, 10, -1, 2123, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2124, 0, 0, -1 },
  { 0x0, 0x0, 10, 2041, -1, 0, 1, 3 },
  { 0x1, 0x1, 10, 2042, -1, 12, 1, 3 },
  { 0x1, 0x1, 10, 2043, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2044, -1, 12, 1, 3 },
  { 0x0, 0x0, 10, 478, -1, 0, 1, 3 },
  { 0x1, 0x1, 10, 2128, -1, 12, 1, 3 },
  { 0x1, 0x1, 10, 482, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2130, -1, 12, 1, 3 },
  { 0x0, 0x0, 10, 486, -1, 0, 1, 3 },
  { 0x1, 0x1, 10, 2132, -1, 12, 1, 3 },
  { 0x1, 0x1, 10, 490, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2134, -1, 12, 1, 3 },
  { 0x0, 0x0, 10, 494, -1, 0, 1, 3 },
  { 0x1, 0x1, 10, 2136, -1, 12, 1, 3 },
  { 0x1, 0x1, 10, 498, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2138, -1, 12, 1, 3 },
  { 0x1, 0x21, 10, 2057, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2058, -1, 12, 1, 3 },
  { 0x1, 0x21, 10, 508, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 10, 2144, -1, 12, 1, 3 },
  { 0x0, 0x0, 10, -1, 2145, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2146, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2149, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2150, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2151, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2152, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2153, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2154, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2155, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2156, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2157, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2158, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2159, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2160, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2161, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2162, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2163, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2164, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2165, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2166, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2167, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2168, 0, 0, -1 },
  { 0x1, 0x1, 10, 2061, -1, 36, 1, 3 },
  { 0x1000001, 0x1000001, 10, 2062, -1, 12, 1, 3 },
  { 0x1, 0x1, 10, 2063, -1, 36, 1, 3 },
  { 0x1000001, 0x1000001, 10, 2064, -1, 12, 1, 3 },
  { 0x0, 0x0, 10, -1, 2169, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2171, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2173, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2175, 0, 0, -1 },
  { 0x1, 0x1, 10, 2065, -1, 36, 1, 78 },
  { 0x1000001, 0x1000001, 10, 2066, -1, 12, 1, 78 },
  { 0x1, 0x1, 10, 2067, -1, 36, 1, 78 },
  { 0x1000001, 0x1000001, 10, 2068, -1, 12, 1, 78 },
  { 0x0, 0x0, 10, -1, 2177, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2179, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2181, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2183, 0, 0, -1 },
  { 0x1, 0x1, 10, 2069, -1, 36, 1, 3 },
  { 0x1000001, 0x1000001, 10, 2070, -1, 12, 1, 3 },
  { 0x1, 0x1, 10, 2071, -1, 36, 1, 3 },
  { 0x1000001, 0x1000001, 10, 2072, -1, 12, 1, 3 },
  { 0x0, 0x0, 10, -1, 2185, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2187, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2189, 0, 0, -1 },
  { 0x0, 0x0, 10, -1, 2191, 0, 0, -1 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x200001, 0x4200001, 11, 2015, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 300, -1, 33, 1, 3 },
  { 0x0, 0x0, 11, 2077, -1, 0, 1, 3 },
  { 0x1, 0x1, 11, 2078, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 2021, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x0, 0x0, 11, 308, -1, 0, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x200001, 0x200001, 11, 2023, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 310, -1, 33, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 2025, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x0, 0x0, 11, 312, -1, 0, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x200001, 0x200001, 11, 2027, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 314, -1, 33, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 2029, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x0, 0x0, 11, 316, -1, 0, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x200001, 0x200001, 11, 2031, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 318, -1, 33, 1, 3 },
  { 0x0, 0x0, 11, 2091, -1, 0, 1, 3 },
  { 0x1, 0x1, 11, 2092, -1, 12, 1, 3 },
  { 0x1, 0x1, 11, 2093, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 11, 2094, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x200001, 0x4200001, 11, 2035, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 322, -1, 33, 1, 3 },
  { 0x0, 0x0, 11, 2099, -1, 0, 1, 3 },
  { 0x1, 0x1, 11, 2100, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x200001, 0x4200001, 11, 2039, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 348, -1, 33, 1, 3 },
  { 0x0, 0x0, 11, 2125, -1, 0, 1, 3 },
  { 0x1, 0x1, 11, 2126, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 2045, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x0, 0x0, 11, 356, -1, 0, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x200001, 0x200001, 11, 2047, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 358, -1, 33, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 2049, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x0, 0x0, 11, 360, -1, 0, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x200001, 0x200001, 11, 2051, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 362, -1, 33, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 2053, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x0, 0x0, 11, 364, -1, 0, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x200001, 0x200001, 11, 2055, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 366, -1, 33, 1, 3 },
  { 0x0, 0x0, 11, 2139, -1, 0, 1, 3 },
  { 0x1, 0x1, 11, 2140, -1, 12, 1, 3 },
  { 0x1, 0x1, 11, 2141, -1, 33, 1, 3 },
  { 0x200001, 0x200001, 11, 2142, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x200001, 0x4200001, 11, 2059, -1, 12, 1, 3 },
  { 0x2, 0x3, 11, -1, -1, 37, 1, 5 },
  { 0x1, 0x1, 11, 370, -1, 33, 1, 3 },
  { 0x0, 0x0, 11, 2147, -1, 0, 1, 3 },
  { 0x1, 0x1, 11, 2148, -1, 12, 1, 3 },
  { 0x1, 0x1, 11, -1, -1, 36, 1, 5 },
  { 0x1, 0x1, 11, -1, -1, 36, 1, 5 },
  { 0x1, 0x1, 11, -1, -1, 36, 1, 5 },
  { 0x1, 0x1, 11, -1, -1, 36, 1, 5 },
  { 0x1, 0x1, 11, 2170, -1, 36, 1, 3 },
  { 0x1000001, 0x1000001, 11, 2172, -1, 12, 1, 3 },
  { 0x1, 0x1, 11, 2174, -1, 36, 1, 3 },
  { 0x1000001, 0x1000001, 11, 2176, -1, 12, 1, 3 },
  { 0x1, 0x1, 11, -1, -1, 36, 1, 80 },
  { 0x1, 0x1, 11, -1, -1, 36, 1, 80 },
  { 0x1, 0x1, 11, -1, -1, 36, 1, 80 },
  { 0x1, 0x1, 11, -1, -1, 36, 1, 80 },
  { 0x1, 0x1, 11, 2178, -1, 36, 1, 78 },
  { 0x1000001, 0x1000001, 11, 2180, -1, 12, 1, 78 },
  { 0x1, 0x1, 11, 2182, -1, 36, 1, 78 },
  { 0x1000001, 0x1000001, 11, 2184, -1, 12, 1, 78 },
  { 0x1, 0x1, 11, -1, -1, 36, 1, 5 },
  { 0x1, 0x1, 11, -1, -1, 36, 1, 5 },
  { 0x1, 0x1, 11, -1, -1, 36, 1, 5 },
  { 0x1, 0x1, 11, -1, -1, 36, 1, 5 },
  { 0x1, 0x1, 11, 2186, -1, 36, 1, 3 },
  { 0x1000001, 0x1000001, 11, 2188, -1, 12, 1, 3 },
  { 0x1, 0x1, 11, 2190, -1, 36, 1, 3 },
  { 0x1000001, 0x1000001, 11, 2192, -1, 12, 1, 3 },
  { 0x0, 0x0, 12, -1, -1, 0, 1, 15 },
  { 0x0, 0x0, 12, -1, -1, 0, 1, 15 },
  { 0x0, 0x0, 12, -1, -1, 0, 1, 15 },
  { 0x1, 0x1, 13, 272, 1452, 34, 1, 131 },
  { 0x1, 0x1, 13, 274, 1461, 34, 1, 131 },
  { 0x1, 0x1, 13, 276, 1470, 34, 1, 131 },
  { 0x1, 0x1, 13, 280, 1483, 34, 1, 131 },
  { 0x1, 0x1, 13, 282, 1492, 34, 1, 131 },
  { 0x1, 0x1, 13, 284, 1501, 34, 1, 131 },
  { 0x1, 0x1, 13, 286, 1510, 34, 1, 131 },
  { 0x1, 0x1, 13, 288, 1519, 34, 1, 131 },
  { 0x1, 0x1, 13, 290, 1528, 34, 1, 131 },
  { 0x1, 0x1, 13, 292, 1537, 34, 1, 131 },
  { 0x1, 0x1, 13, 294, 1547, 34, 1, 131 },
  { 0x1, 0x1, 13, 296, 1557, 34, 1, 131 },
  { 0x0, 0x0, 19, -1, 795, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 796, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 797, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 798, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 799, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 800, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 801, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 802, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 803, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 804, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 805, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 806, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 807, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 808, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 809, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 810, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 811, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 812, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 813, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 814, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 815, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 816, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 817, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 818, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 819, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 820, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 821, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 822, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 823, 0, 0, -1 },
  { 0x0, 0x0, 19, -1, 824, 0, 0, -1 },
  { 0x0, 0x0, 20, -1, 2827, 0, 0, -1 },
  { 0x0, 0x0, 20, -1, 2828, 0, 0, -1 },
  { 0x0, 0x0, 20, -1, 2843, 0, 0, -1 },
  { 0x0, 0x0, 20, -1, 2844, 0, 0, -1 },
  { 0x0, 0x0, 20, -1, 2849, 0, 0, -1 },
  { 0x0, 0x0, 20, -1, 2850, 0, 0, -1 },
  { 0x0, 0x0, 21, 831, 2839, 0, 0, -1 },
  { 0x0, 0x0, 21, 832, 2841, 0, 0, -1 },
  { 0x0, 0x0, 23, -1, 2837, 0, 0, -1 },
  { 0x0, 0x0, 23, -1, 2838, 0, 0, -1 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, 1272, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 7 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 7 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 7 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 7 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 7 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 7 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 7 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 7 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 6 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 7 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 7 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 7 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 7 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 8 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 8 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 8 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 8 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 8 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 8 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 8 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 8 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 8 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 8 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 8 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 8 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 16 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 16 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 16 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 16 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 16 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 16 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 16 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 16 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 16 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 16 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 16 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 16 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, 1293, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 19 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 20 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 20 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 20 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 20 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 20 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 20 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 20 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 20 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 20 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 20 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 20 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 20 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 21 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, 1326, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 18 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 35, 1, 22 },
  { 0x1, 0x1, 24, -1, -1, 33, 1, 82 },
  { 0x1, 0x1, 24, -1, -1, 33, 1, 82 },
  { 0x1, 0x1, 24, 1342, 1455, 35, 1, 137 },
  { 0x1, 0x1, 24, 1343, 1464, 35, 1, 137 },
  { 0x1, 0x1, 24, 1344, 1473, 35, 1, 137 },
  { 0x1, 0x1, 24, 1345, 1486, 35, 1, 137 },
  { 0x1, 0x1, 24, 1346, 1495, 35, 1, 137 },
  { 0x1, 0x1, 24, 1347, 1504, 35, 1, 137 },
  { 0x1, 0x1, 24, 1348, 1513, 35, 1, 137 },
  { 0x1, 0x1, 24, 1349, 1522, 35, 1, 137 },
  { 0x1, 0x1, 24, 1350, 1531, 35, 1, 137 },
  { 0x1, 0x1, 24, 1351, 1541, 35, 1, 137 },
  { 0x1, 0x1, 24, 1352, 1551, 35, 1, 137 },
  { 0x1, 0x1, 24, 1353, 1561, 35, 1, 137 },
  { 0x1, 0x1, 24, 1354, 1570, 35, 1, 151 },
  { 0x1, 0x1, 24, 1355, 1576, 35, 1, 156 },
  { 0x1, 0x1, 24, 1356, 1582, 35, 1, 156 },
  { 0x1, 0x1, 24, 1357, 1588, 35, 1, 151 },
  { 0x1, 0x1, 24, 1358, 1594, 35, 1, 156 },
  { 0x1, 0x1, 24, 1359, 1600, 35, 1, 156 },
  { 0x1, 0x1, 24, 1360, 1606, 35, 1, 151 },
  { 0x1, 0x1, 24, 1361, 1612, 35, 1, 156 },
  { 0x1, 0x1, 24, 1362, 1618, 35, 1, 156 },
  { 0x1, 0x1, 24, 1363, 1624, 35, 1, 151 },
  { 0x1, 0x1, 24, 1364, 1630, 35, 1, 156 },
  { 0x1, 0x1, 24, 1365, 1636, 35, 1, 151 },
  { 0x1, 0x1, 24, 1366, 1642, 35, 1, 156 },
  { 0x1, 0x1, 24, 1367, 1648, 35, 1, 151 },
  { 0x1, 0x1, 24, 1368, 1654, 35, 1, 156 },
  { 0x1, 0x1, 24, 1369, 1660, 35, 1, 151 },
  { 0x1, 0x1, 24, 1370, 1666, 35, 1, 156 },
  { 0x1, 0x1, 24, 1371, 1672, 35, 1, 156 },
  { 0x0, 0x0, 33, 2821, 2819, 0, 0, -1 },
  { 0x0, 0x0, 33, 2824, 2822, 0, 0, -1 },
  { 0x0, 0x0, 33, 2830, 2829, 0, 0, -1 },
  { 0x0, 0x0, 33, 2832, 2831, 0, 0, -1 },
  { 0x0, 0x0, 33, 2846, 2845, 0, 0, -1 },
  { 0x0, 0x0, 33, 2848, 2847, 0, 0, -1 },
  { 0x0, 0x0, 35, -1, 2840, 0, 0, -1 },
  { 0x0, 0x0, 35, -1, 2842, 0, 0, -1 },
  { 0x1, 0x1, 38, -1, 2290, 37, 1, 30 },
  { 0x1, 0x1, 38, -1, 2349, 37, 1, 30 },
  { 0x0, 0x0, 38, -1, 2352, 0, 0, -1 },
  { 0x1, 0x1, 38, -1, -1, 37, 1, 30 },
  { 0x1, 0x1, 38, -1, 2357, 37, 1, 30 },
  { 0x0, 0x0, 38, -1, 2360, 0, 0, -1 },
  { 0x1, 0x1, 38, -1, -1, 37, 1, 30 },
  { 0x0, 0x0, 38, -1, 2363, 0, 0, -1 },
  { 0x1, 0x1, 38, -1, -1, 37, 1, 30 },
  { 0x1, 0x1, 38, -1, 2366, 37, 1, 30 },
  { 0x1, 0x1, 38, -1, 2369, 37, 1, 30 },
  { 0x1, 0x1, 38, -1, 2402, 37, 1, 30 },
  { 0x3, 0x3, 38, -1, -1, 30, 1, 144 },
  { 0x0, 0x0, 38, 1142, -1, 0, 1, 102 },
  { 0x0, 0x0, 38, -1, -1, 0, 1, 111 },
  { 0x0, 0x0, 38, 1148, -1, 0, 1, 123 },
  { 0x3, 0x3, 38, -1, -1, 30, 1, 160 },
  { 0x0, 0x0, 38, 1149, -1, 0, 1, 41 },
  { 0x0, 0x0, 40, -1, 973, 0, 0, -1 },
  { 0x0, 0x0, 40, -1, 981, 0, 0, -1 },
  { 0x0, 0x0, 40, 1151, 977, 0, 0, -1 },
  { 0x3, 0x3, 40, -1, 622, 33, 1, 6 },
  { 0x18000001, 0x18000001, 40, -1, 630, 6, 1, 7 },
  { 0x3, 0x3, 40, 1152, 626, 33, 1, 6 },
  { 0x0, 0x0, 40, -1, 985, 0, 0, -1 },
  { 0x3, 0x3, 40, -1, 642, 33, 1, 8 },
  { 0x0, 0x0, 40, -1, 989, 0, 0, -1 },
  { 0x3, 0x3, 40, -1, 654, 33, 1, 16 },
  { 0x0, 0x0, 40, -1, 994, 0, 0, -1 },
  { 0x0, 0x0, 40, -1, 998, 0, 0, -1 },
  { 0x3, 0x3, 40, -1, 677, 33, 1, 18 },
  { 0x3, 0x3, 40, -1, 681, 33, 1, 18 },
  { 0x0, 0x0, 40, -1, 1002, 0, 0, -1 },
  { 0x0, 0x0, 40, -1, 1006, 0, 0, -1 },
  { 0x3, 0x3, 40, -1, 701, 33, 1, 19 },
  { 0x18000001, 0x18000001, 40, -1, 705, 6, 1, 19 },
  { 0x0, 0x0, 40, -1, 1010, 0, 0, -1 },
  { 0x3, 0x3, 40, -1, 717, 33, 1, 20 },
  { 0x0, 0x0, 40, -1, 1014, 0, 0, -1 },
  { 0x0, 0x0, 40, -1, 1018, 0, 0, -1 },
  { 0x3, 0x3, 40, -1, 737, 33, 1, 21 },
  { 0x18000001, 0x18000001, 40, -1, 741, 6, 1, 21 },
  { 0x0, 0x0, 40, -1, 1022, 0, 0, -1 },
  { 0x3, 0x3, 40, -1, 753, 33, 1, 22 },
  { 0x0, 0x0, 40, -1, 1027, 0, 0, -1 },
  { 0x0, 0x0, 40, -1, 1031, 0, 0, -1 },
  { 0x3, 0x3, 40, -1, 776, 33, 1, 18 },
  { 0x3, 0x3, 40, -1, 780, 33, 1, 18 },
  { 0x0, 0x0, 40, -1, 1035, 0, 0, -1 },
  { 0x3, 0x3, 40, -1, 792, 33, 1, 22 },
  { 0x0, 0x0, 41, 851, 972, 0, 0, -1 },
  { 0x0, 0x0, 41, 852, 980, 0, 0, -1 },
  { 0x0, 0x0, 41, 853, 976, 0, 0, -1 },
  { 0x1, 0x1, 41, 854, 621, 34, 1, 6 },
  { 0x10000001, 0x10000001, 41, 855, 629, 6, 1, 7 },
  { 0x1, 0x1, 41, 856, 625, 34, 1, 6 },
  { 0x0, 0x0, 41, 857, 984, 0, 0, -1 },
  { 0x1, 0x1, 41, 858, 641, 34, 1, 8 },
  { 0x0, 0x0, 41, 859, 988, 0, 0, -1 },
  { 0x1, 0x1, 41, 860, 653, 34, 1, 16 },
  { 0x0, 0x0, 41, 861, 993, 0, 0, -1 },
  { 0x0, 0x0, 41, 862, 997, 0, 0, -1 },
  { 0x1, 0x1, 41, 863, 676, 34, 1, 18 },
  { 0x1, 0x1, 41, 864, 680, 34, 1, 18 },
  { 0x0, 0x0, 41, 865, 1001, 0, 0, -1 },
  { 0x0, 0x0, 41, 866, 1005, 0, 0, -1 },
  { 0x1, 0x1, 41, 867, 700, 34, 1, 19 },
  { 0x10000001, 0x10000001, 41, 868, 704, 6, 1, 19 },
  { 0x0, 0x0, 41, 869, 1009, 0, 0, -1 },
  { 0x1, 0x1, 41, 870, 716, 34, 1, 20 },
  { 0x0, 0x0, 41, 871, 1013, 0, 0, -1 },
  { 0x0, 0x0, 41, 872, 1017, 0, 0, -1 },
  { 0x1, 0x1, 41, 873, 736, 34, 1, 21 },
  { 0x10000001, 0x10000001, 41, 874, 740, 6, 1, 21 },
  { 0x0, 0x0, 41, 875, 1021, 0, 0, -1 },
  { 0x1, 0x1, 41, 876, 752, 34, 1, 22 },
  { 0x0, 0x0, 41, 877, 1026, 0, 0, -1 },
  { 0x0, 0x0, 41, 878, 1030, 0, 0, -1 },
  { 0x1, 0x1, 41, 879, 775, 34, 1, 18 },
  { 0x1, 0x1, 41, 880, 779, 34, 1, 18 },
  { 0x0, 0x0, 41, 881, 1034, 0, 0, -1 },
  { 0x1, 0x1, 41, 882, 791, 34, 1, 22 },
  { 0x800001, 0x800001, 41, -1, 1156, 4, 1, 17 },
  { 0x1, 0x1, 41, 2236, 1154, 4, 1, 17 },
  { 0x1, 0x1, 41, 957, 1159, 4, 1, 23 },
  { 0x2, 0x3, 41, -1, 1164, 20, 1, 68 },
  { 0x1, 0x1, 41, 2237, 1162, 21, 1, 68 },
  { 0x0, 0x0, 42, -1, -1, 0, 1, 86 },
  { 0x0, 0x0, 42, -1, -1, 0, 1, 86 },
  { 0x0, 0x0, 42, -1, -1, 0, 1, 130 },
  { 0x1, 0x1, 44, 1372, 297, 38, 1, 1 },
  { 0x1, 0x1, 44, 1373, 299, 38, 1, 1 },
  { 0x0, 0x0, 44, -1, 302, 0, 0, -1 },
  { 0x0, 0x0, 44, -1, 424, 0, 0, -1 },
  { 0x1, 0x1, 44, 1377, 319, 38, 1, 1 },
  { 0x1, 0x1, 44, 1378, 321, 38, 1, 1 },
  { 0x0, 0x0, 44, -1, 324, 0, 0, -1 },
  { 0x0, 0x0, 44, -1, 464, 0, 0, -1 },
  { 0x0, 0x0, 44, -1, 326, 0, 0, -1 },
  { 0x0, 0x0, 44, -1, 344, 0, 0, -1 },
  { 0x1, 0x1, 44, 1384, 345, 38, 1, 1 },
  { 0x1, 0x1, 44, 1385, 347, 38, 1, 1 },
  { 0x0, 0x0, 44, -1, 350, 0, 0, -1 },
  { 0x0, 0x0, 44, -1, 472, 0, 0, -1 },
  { 0x1, 0x1, 44, 1389, 367, 38, 1, 1 },
  { 0x1, 0x1, 44, 1390, 369, 38, 1, 1 },
  { 0x0, 0x0, 44, -1, 372, 0, 0, -1 },
  { 0x0, 0x0, 44, -1, 512, 0, 0, -1 },
  { 0x0, 0x0, 44, -1, 374, 0, 0, -1 },
  { 0x0, 0x0, 44, -1, 392, 0, 0, -1 },
  { 0x0, 0x0, 44, 1248, 2297, 0, 0, -1 },
  { 0x0, 0x0, 44, 1249, 2305, 0, 1, 55 },
  { 0x0, 0x0, 44, 1250, 2972, 0, 1, 55 },
  { 0x0, 0x0, 44, 1251, 2373, 0, 0, -1 },
  { 0x0, 0x0, 44, 1252, -1, 0, 1, 50 },
  { 0x0, 0x0, 44, 1120, -1, 0, 1, 0 },
  { 0x0, 0x0, 44, 1121, -1, 0, 1, 0 },
  { 0x0, 0x0, 44, 1122, -1, 0, 1, 0 },
  { 0x1, 0x1, 45, -1, 1676, 30, 1, 158 },
  { 0x1, 0x1, 45, 963, 1675, 30, 1, 158 },
  { 0x1, 0x1, 45, -1, 1680, 30, 1, 159 },
  { 0x1, 0x1, 45, 964, 1679, 30, 1, 159 },
  { 0x1, 0x1, 45, -1, 1684, 30, 1, 159 },
  { 0x1, 0x1, 45, 965, 1683, 30, 1, 159 },
  { 0x3, 0x3, 46, -1, 1160, 3, 1, 23 },
  { 0x1, 0x1, 47, 2257, -1, 30, 1, 144 },
  { 0x1, 0x1, 47, 2288, -1, 30, 1, 160 },
  { 0x0, 0x0, 49, -1, -1, 0, 1, 41 },
  { 0x0, 0x0, 49, -1, -1, 0, 1, 41 },
  { 0x0, 0x0, 49, -1, -1, 0, 1, 41 },
  { 0x1, 0x1, 56, -1, 1677, 31, 1, 158 },
  { 0x1, 0x1, 56, -1, 1681, 31, 1, 159 },
  { 0x1, 0x1, 56, -1, 1685, 31, 1, 159 },
  { 0x0, 0x0, 56, -1, -1, 0, 1, 101 },
  { 0x2, 0x3, 56, -1, -1, 27, 1, 101 },
  { 0x1, 0x1, 56, -1, -1, 28, 1, 101 },
  { 0x0, 0x0, 65, 14, 592, 0, 1, 6 },
  { 0x0, 0x0, 65, 1273, 595, 0, 1, 6 },
  { 0x1, 0x1, 65, 1274, 597, 33, 1, 6 },
  { 0x1, 0x1, 65, 1275, 599, 34, 1, 6 },
  { 0x3, 0x3, 65, 1276, 601, 33, 1, 6 },
  { 0x0, 0x0, 65, 1277, 603, 0, 1, 6 },
  { 0x1, 0x1, 65, 1278, 605, 33, 1, 6 },
  { 0x1, 0x1, 65, 1279, 607, 34, 1, 6 },
  { 0x3, 0x3, 65, 1280, 609, 33, 1, 6 },
  { 0x1, 0x1, 65, 1281, 611, 6, 1, 7 },
  { 0x8000001, 0x8000001, 65, 1282, 613, 6, 1, 7 },
  { 0x10000001, 0x10000001, 65, 1283, 615, 6, 1, 7 },
  { 0x18000001, 0x18000001, 65, 1284, 617, 6, 1, 7 },
  { 0x0, 0x0, 65, 1285, 631, 0, 1, 8 },
  { 0x1, 0x1, 65, 1286, 633, 33, 1, 8 },
  { 0x1, 0x1, 65, 1287, 635, 34, 1, 8 },
  { 0x3, 0x3, 65, 1288, 637, 33, 1, 8 },
  { 0x0, 0x0, 65, 1289, 643, 0, 1, 16 },
  { 0x1, 0x1, 65, 1290, 645, 33, 1, 16 },
  { 0x1, 0x1, 65, 1291, 647, 34, 1, 16 },
  { 0x3, 0x3, 65, 1292, 649, 33, 1, 16 },
  { 0x0, 0x0, 65, 15, 655, 0, 1, 18 },
  { 0x0, 0x0, 65, 1294, 658, 0, 1, 18 },
  { 0x1, 0x1, 65, 1295, 660, 33, 1, 18 },
  { 0x1, 0x1, 65, 1296, 662, 34, 1, 18 },
  { 0x3, 0x3, 65, 1297, 664, 33, 1, 18 },
  { 0x0, 0x0, 65, 1298, 666, 0, 1, 18 },
  { 0x1, 0x1, 65, 1299, 668, 33, 1, 18 },
  { 0x1, 0x1, 65, 1300, 670, 34, 1, 18 },
  { 0x3, 0x3, 65, 1301, 672, 33, 1, 18 },
  { 0x0, 0x0, 65, 1302, 682, 0, 1, 19 },
  { 0x1, 0x1, 65, 1303, 684, 33, 1, 19 },
  { 0x1, 0x1, 65, 1304, 686, 34, 1, 19 },
  { 0x3, 0x3, 65, 1305, 688, 33, 1, 19 },
  { 0x1, 0x1, 65, 1306, 690, 6, 1, 19 },
  { 0x8000001, 0x8000001, 65, 1307, 692, 6, 1, 19 },
  { 0x10000001, 0x10000001, 65, 1308, 694, 6, 1, 19 },
  { 0x18000001, 0x18000001, 65, 1309, 696, 6, 1, 19 },
  { 0x0, 0x0, 65, 1310, 706, 0, 1, 20 },
  { 0x1, 0x1, 65, 1311, 708, 33, 1, 20 },
  { 0x1, 0x1, 65, 1312, 710, 34, 1, 20 },
  { 0x3, 0x3, 65, 1313, 712, 33, 1, 20 },
  { 0x0, 0x0, 65, 1314, 718, 0, 1, 21 },
  { 0x1, 0x1, 65, 1315, 720, 33, 1, 21 },
  { 0x1, 0x1, 65, 1316, 722, 34, 1, 21 },
  { 0x3, 0x3, 65, 1317, 724, 33, 1, 21 },
  { 0x1, 0x1, 65, 1318, 726, 6, 1, 21 },
  { 0x8000001, 0x8000001, 65, 1319, 728, 6, 1, 21 },
  { 0x10000001, 0x10000001, 65, 1320, 730, 6, 1, 21 },
  { 0x18000001, 0x18000001, 65, 1321, 732, 6, 1, 21 },
  { 0x0, 0x0, 65, 1322, 742, 0, 1, 22 },
  { 0x1, 0x1, 65, 1323, 744, 33, 1, 22 },
  { 0x1, 0x1, 65, 1324, 746, 34, 1, 22 },
  { 0x3, 0x3, 65, 1325, 748, 33, 1, 22 },
  { 0x0, 0x0, 65, 17, 754, 0, 1, 18 },
  { 0x0, 0x0, 65, 1327, 757, 0, 1, 18 },
  { 0x1, 0x1, 65, 1328, 759, 33, 1, 18 },
  { 0x1, 0x1, 65, 1329, 761, 34, 1, 18 },
  { 0x3, 0x3, 65, 1330, 763, 33, 1, 18 },
  { 0x0, 0x0, 65, 1331, 765, 0, 1, 18 },
  { 0x1, 0x1, 65, 1332, 767, 33, 1, 18 },
  { 0x1, 0x1, 65, 1333, 769, 34, 1, 18 },
  { 0x3, 0x3, 65, 1334, 771, 33, 1, 18 },
  { 0x0, 0x0, 65, 1335, 781, 0, 1, 22 },
  { 0x1, 0x1, 65, 1336, 783, 33, 1, 22 },
  { 0x1, 0x1, 65, 1337, 785, 34, 1, 22 },
  { 0x3, 0x3, 65, 1338, 787, 33, 1, 22 },
  { 0x3, 0x3, 66, 561, 1539, 33, 1, 136 },
  { 0x3, 0x3, 66, 562, 1549, 33, 1, 136 },
  { 0x3, 0x3, 66, 563, 1559, 33, 1, 136 },
  { 0x0, 0x0, 66, -1, 1564, 0, 1, 147 },
  { 0x0, 0x0, 66, -1, 1565, 0, 1, 152 },
  { 0x0, 0x0, 66, -1, 1566, 0, 1, 152 },
  { 0x0, 0x0, 107, 1046, 2345, 0, 0, -1 },
  { 0x0, 0x0, 107, 1047, 2864, 0, 1, 30 },
  { 0x0, 0x0, 107, 1048, 2386, 0, 0, -1 },
  { 0x0, 0x0, 107, 1049, 2868, 0, 1, 30 },
  { 0x0, 0x0, 109, -1, 2347, 0, 0, -1 },
  { 0x1, 0x1, 109, -1, 2865, 27, 1, 30 },
  { 0x0, 0x0, 109, -1, 2388, 0, 0, -1 },
  { 0x1, 0x1, 109, -1, 2869, 27, 1, 30 },
  { 0x0, 0x0, 110, 1051, -1, 0, 1, 122 },
  { 0x1, 0x1, 111, -1, -1, 27, 1, 122 },
  { 0x0, 0x0, 112, 1082, 2894, 0, 1, 1 },
  { 0x0, 0x0, 112, 1083, 2897, 0, 1, 1 },
  { 0x0, 0x0, 112, 1224, 305, 0, 0, -1 },
  { 0x0, 0x0, 112, 1225, 309, 0, 0, -1 },
  { 0x0, 0x0, 112, 1185, 440, 0, 0, -1 },
  { 0x0, 0x0, 112, 1186, 448, 0, 0, -1 },
  { 0x0, 0x0, 112, -1, 456, 0, 0, -1 },
  { 0x0, 0x0, 112, 1084, 2910, 0, 1, 1 },
  { 0x0, 0x0, 112, 1085, 2913, 0, 1, 1 },
  { 0x0, 0x0, 112, -1, 330, 0, 0, -1 },
  { 0x0, 0x0, 112, -1, 334, 0, 0, -1 },
  { 0x0, 0x0, 112, 1233, 335, 0, 0, -1 },
  { 0x0, 0x0, 112, 1234, 339, 0, 0, -1 },
  { 0x0, 0x0, 112, 1086, 2934, 0, 1, 1 },
  { 0x0, 0x0, 112, 1087, 2937, 0, 1, 1 },
  { 0x0, 0x0, 112, 1237, 353, 0, 0, -1 },
  { 0x0, 0x0, 112, 1238, 357, 0, 0, -1 },
  { 0x0, 0x0, 112, 1198, 488, 0, 0, -1 },
  { 0x0, 0x0, 112, 1199, 496, 0, 0, -1 },
  { 0x0, 0x0, 112, -1, 504, 0, 0, -1 },
  { 0x0, 0x0, 112, 1391, 2948, 0, 1, 1 },
  { 0x0, 0x0, 112, 1392, 2950, 0, 1, 1 },
  { 0x0, 0x0, 112, -1, 378, 0, 0, -1 },
  { 0x0, 0x0, 112, -1, 382, 0, 0, -1 },
  { 0x0, 0x0, 112, 1246, 383, 0, 0, -1 },
  { 0x0, 0x0, 112, 1247, 387, 0, 0, -1 },
  { 0x0, 0x0, 112, -1, 2315, 0, 0, -1 },
  { 0x1, 0x9, 112, -1, 2319, 33, 1, 55 },
  { 0x1, 0x9, 112, -1, 2981, 33, 1, 55 },
  { 0x2, 0x3, 112, 1408, 2382, 27, 1, 50 },
  { 0x1, 0x1, 114, 1374, 2895, 37, 1, 1 },
  { 0x1, 0x1, 114, 1375, 2898, 37, 1, 1 },
  { 0x1, 0x1, 114, 1379, 2911, 37, 1, 1 },
  { 0x1, 0x1, 114, 1380, 2914, 37, 1, 1 },
  { 0x1, 0x1, 114, 1386, 2935, 37, 1, 1 },
  { 0x1, 0x1, 114, 1387, 2938, 37, 1, 1 },
  { 0x0, 0x0, 114, -1, 2958, 0, 1, 1 },
  { 0x0, 0x0, 114, -1, 2959, 0, 1, 1 },
  { 0x0, 0x0, 115, 1123, 2890, 0, 1, 1 },
  { 0x0, 0x0, 115, 1124, 2892, 0, 1, 1 },
  { 0x0, 0x0, 115, 1183, 303, 0, 0, -1 },
  { 0x0, 0x0, 115, 1184, 307, 0, 0, -1 },
  { 0x0, 0x0, 115, -1, 444, 0, 0, -1 },
  { 0x0, 0x0, 115, -1, 452, 0, 0, -1 },
  { 0x0, 0x0, 115, 1228, 454, 0, 0, -1 },
  { 0x0, 0x0, 115, -1, 2908, 0, 1, 1 },
  { 0x0, 0x0, 115, -1, 2909, 0, 1, 1 },
  { 0x0, 0x0, 115, 1231, 328, 0, 0, -1 },
  { 0x0, 0x0, 115, 1232, 332, 0, 0, -1 },
  { 0x0, 0x0, 115, 1192, 337, 0, 0, -1 },
  { 0x0, 0x0, 115, 1193, 341, 0, 0, -1 },
  { 0x0, 0x0, 115, 1127, 2930, 0, 1, 1 },
  { 0x0, 0x0, 115, 1128, 2932, 0, 1, 1 },
  { 0x0, 0x0, 115, 1196, 351, 0, 0, -1 },
  { 0x0, 0x0, 115, 1197, 355, 0, 0, -1 },
  { 0x0, 0x0, 115, -1, 492, 0, 0, -1 },
  { 0x0, 0x0, 115, -1, 500, 0, 0, -1 },
  { 0x0, 0x0, 115, 1241, 502, 0, 0, -1 },
  { 0x0, 0x0, 115, -1, 2946, 0, 1, 1 },
  { 0x0, 0x0, 115, -1, 2947, 0, 1, 1 },
  { 0x0, 0x0, 115, 1244, 376, 0, 0, -1 },
  { 0x0, 0x0, 115, 1245, 380, 0, 0, -1 },
  { 0x0, 0x0, 115, 1205, 385, 0, 0, -1 },
  { 0x0, 0x0, 115, 1206, 389, 0, 0, -1 },
  { 0x0, 0x0, 115, 1078, 2313, 0, 0, -1 },
  { 0x0, 0x0, 115, 1079, 2317, 0, 1, 55 },
  { 0x0, 0x0, 115, 1080, 2980, 0, 1, 55 },
  { 0x0, 0x0, 115, 1081, 2381, 0, 1, 50 },
  { 0x1, 0x1, 115, -1, -1, 27, 1, 0 },
  { 0x1, 0x1, 115, -1, -1, 27, 1, 0 },
  { 0x1, 0x1, 115, -1, -1, 27, 1, 0 },
  { 0x1, 0x1, 116, -1, 2891, 37, 1, 1 },
  { 0x1, 0x1, 116, -1, 2893, 37, 1, 1 },
  { 0x0, 0x0, 116, -1, 2918, 0, 1, 1 },
  { 0x0, 0x0, 116, -1, 2919, 0, 1, 1 },
  { 0x1, 0x1, 116, -1, 2931, 37, 1, 1 },
  { 0x1, 0x1, 116, -1, 2933, 37, 1, 1 },
  { 0x0, 0x0, 116, -1, 2956, 0, 1, 1 },
  { 0x0, 0x0, 116, -1, 2957, 0, 1, 1 },
  { 0x0, 0x0, 117, 1176, -1, 0, 1, 0 },
  { 0x0, 0x0, 117, 1177, -1, 0, 1, 0 },
  { 0x0, 0x0, 117, 1178, -1, 0, 1, 0 },
  { 0x3, 0x3, 117, 1136, -1, 34, 1, 34 },
  { 0x3, 0x3, 117, 1137, -1, 34, 1, 41 },
  { 0x1, 0x1, 119, -1, -1, 35, 1, 34 },
  { 0x1, 0x1, 119, -1, -1, 35, 1, 41 },
  { 0x0, 0x0, 120, -1, -1, 0, 1, 41 },
  { 0x0, 0x0, 120, -1, -1, 0, 1, 67 },
  { 0x1, 0x1, 120, -1, -1, 36, 1, 129 },
  { 0x0, 0x0, 120, -1, -1, 0, 1, 41 },
  { 0x1, 0x1, 120, -1, -1, 27, 1, 103 },
  { 0x0, 0x0, 120, -1, -1, 0, 1, 112 },
  { 0x0, 0x0, 120, -1, -1, 0, 1, 74 },
  { 0x0, 0x0, 120, -1, -1, 0, 1, 74 },
  { 0x0, 0x0, 120, -1, -1, 0, 1, 75 },
  { 0x0, 0x0, 120, -1, -1, 0, 1, 41 },
  { 0x1, 0x1, 120, -1, -1, 27, 1, 124 },
  { 0x1, 0x1, 120, -1, -1, 27, 1, 41 },
  { 0x0, 0x0, 120, -1, -1, 0, 1, 41 },
  { 0x0, 0x0, 121, -1, 2820, 0, 0, -1 },
  { 0x0, 0x0, 121, -1, 2823, 0, 0, -1 },
  { 0x1, 0x1, 122, -1, -1, 35, 1, 17 },
  { 0x1, 0x1, 122, -1, -1, 35, 1, 17 },
  { 0x1, 0x1, 122, -1, -1, 35, 1, 17 },
  { 0x1, 0x1, 122, -1, -1, 35, 1, 17 },
  { 0x1, 0x1, 122, -1, -1, 35, 1, 23 },
  { 0x1, 0x1, 122, -1, -1, 35, 1, 23 },
  { 0x1, 0x1, 122, -1, -1, 35, 1, 23 },
  { 0x1, 0x1, 122, -1, -1, 35, 1, 23 },
  { 0x1, 0x1, 122, -1, -1, 23, 1, 68 },
  { 0x1, 0x1, 122, -1, -1, 23, 1, 68 },
  { 0x1, 0x1, 122, -1, -1, 23, 1, 68 },
  { 0x1, 0x1, 122, -1, -1, 23, 1, 68 },
  { 0x1, 0x1, 122, 918, -1, 23, 1, 68 },
  { 0x9, 0x9, 122, 919, -1, 20, 1, 68 },
  { 0x0, 0x0, 126, 2199, -1, 0, 1, 0 },
  { 0x0, 0x0, 126, 2200, -1, 0, 1, 0 },
  { 0x1, 0x1, 126, -1, -1, 28, 1, 34 },
  { 0x1, 0x1, 126, -1, -1, 27, 1, 34 },
  { 0x1, 0x1, 126, -1, -1, 29, 1, 0 },
  { 0x1, 0x1, 126, -1, -1, 29, 1, 0 },
  { 0x1, 0x1, 126, -1, -1, 29, 1, 0 },
  { 0x1, 0x1, 126, -1, -1, 29, 1, 0 },
  { 0x0, 0x0, 126, -1, -1, 0, 1, 121 },
  { 0x1, 0x1, 126, -1, -1, 29, 1, 0 },
  { 0x1, 0x1, 126, -1, -1, 29, 1, 0 },
  { 0x1, 0x1, 126, -1, -1, 29, 1, 0 },
  { 0x0, 0x0, 126, 1134, -1, 0, 1, 34 },
  { 0x0, 0x0, 126, 1262, -1, 0, 1, 41 },
  { 0x0, 0x0, 140, 1212, 2886, 0, 1, 1 },
  { 0x0, 0x0, 140, 1213, 2888, 0, 1, 1 },
  { 0x0, 0x0, 140, 1054, 304, 0, 0, -1 },
  { 0x0, 0x0, 140, 1055, 432, 0, 0, -1 },
  { 0x0, 0x0, 140, 1094, 313, 0, 0, -1 },
  { 0x0, 0x0, 140, 1095, 317, 0, 0, -1 },
  { 0x0, 0x0, 140, 1096, 453, 0, 0, -1 },
  { 0x0, 0x0, 140, -1, 2906, 0, 1, 1 },
  { 0x0, 0x0, 140, -1, 2907, 0, 1, 1 },
  { 0x0, 0x0, 140, 1099, 327, 0, 0, -1 },
  { 0x0, 0x0, 140, 1100, 331, 0, 0, -1 },
  { 0x0, 0x0, 140, -1, 338, 0, 0, -1 },
  { 0x0, 0x0, 140, -1, 342, 0, 0, -1 },
  { 0x0, 0x0, 140, 1216, 2926, 0, 1, 1 },
  { 0x0, 0x0, 140, 1217, 2928, 0, 1, 1 },
  { 0x0, 0x0, 140, 1067, 352, 0, 0, -1 },
  { 0x0, 0x0, 140, 1068, 480, 0, 0, -1 },
  { 0x0, 0x0, 140, 1107, 361, 0, 0, -1 },
  { 0x0, 0x0, 140, 1108, 365, 0, 0, -1 },
  { 0x0, 0x0, 140, 1109, 501, 0, 0, -1 },
  { 0x0, 0x0, 140, -1, 2944, 0, 1, 1 },
  { 0x0, 0x0, 140, -1, 2945, 0, 1, 1 },
  { 0x0, 0x0, 140, 1112, 375, 0, 0, -1 },
  { 0x0, 0x0, 140, 1113, 379, 0, 0, -1 },
  { 0x0, 0x0, 140, -1, 386, 0, 0, -1 },
  { 0x0, 0x0, 140, -1, 390, 0, 0, -1 },
  { 0x0, 0x0, 140, 3012, 2301, 0, 0, -1 },
  { 0x1, 0x1, 140, 3013, 2309, 33, 1, 55 },
  { 0x1, 0x1, 140, 3014, 2974, 33, 1, 55 },
  { 0x0, 0x0, 140, 3015, 2375, 0, 0, -1 },
  { 0x1, 0x1, 140, 3016, -1, 28, 1, 50 },
  { 0x1, 0x1, 141, -1, 2887, 37, 1, 1 },
  { 0x1, 0x1, 141, -1, 2889, 37, 1, 1 },
  { 0x0, 0x0, 141, -1, 2916, 0, 1, 1 },
  { 0x0, 0x0, 141, -1, 2917, 0, 1, 1 },
  { 0x1, 0x1, 141, -1, 2927, 37, 1, 1 },
  { 0x1, 0x1, 141, -1, 2929, 37, 1, 1 },
  { 0x0, 0x0, 141, -1, 2954, 0, 1, 1 },
  { 0x0, 0x0, 141, -1, 2955, 0, 1, 1 },
  { 0x1, 0x1, 144, 917, 1158, 3, 1, 23 },
  { 0x0, 0x0, 145, 2201, -1, 0, 1, 34 },
  { 0x0, 0x0, 146, 923, 2880, 0, 1, 1 },
  { 0x0, 0x0, 146, 924, 2883, 0, 1, 1 },
  { 0x0, 0x0, 146, -1, 306, 0, 0, -1 },
  { 0x0, 0x0, 146, -1, 436, 0, 0, -1 },
  { 0x0, 0x0, 146, 1056, 311, 0, 0, -1 },
  { 0x0, 0x0, 146, 1057, 315, 0, 0, -1 },
  { 0x0, 0x0, 146, 1058, 455, 0, 0, -1 },
  { 0x0, 0x0, 146, 927, 2900, 0, 1, 1 },
  { 0x0, 0x0, 146, 928, 2903, 0, 1, 1 },
  { 0x0, 0x0, 146, 1061, 329, 0, 0, -1 },
  { 0x0, 0x0, 146, 1062, 333, 0, 0, -1 },
  { 0x0, 0x0, 146, 1101, 336, 0, 0, -1 },
  { 0x0, 0x0, 146, 1102, 340, 0, 0, -1 },
  { 0x0, 0x0, 146, 933, 2920, 0, 1, 1 },
  { 0x0, 0x0, 146, 934, 2923, 0, 1, 1 },
  { 0x0, 0x0, 146, -1, 354, 0, 0, -1 },
  { 0x0, 0x0, 146, -1, 484, 0, 0, -1 },
  { 0x0, 0x0, 146, 1069, 359, 0, 0, -1 },
  { 0x0, 0x0, 146, 1070, 363, 0, 0, -1 },
  { 0x0, 0x0, 146, 1071, 503, 0, 0, -1 },
  { 0x0, 0x0, 146, 937, 2940, 0, 1, 1 },
  { 0x0, 0x0, 146, 938, 2942, 0, 1, 1 },
  { 0x0, 0x0, 146, 1074, 377, 0, 0, -1 },
  { 0x0, 0x0, 146, 1075, 381, 0, 0, -1 },
  { 0x0, 0x0, 146, 1114, 384, 0, 0, -1 },
  { 0x0, 0x0, 146, 1115, 388, 0, 0, -1 },
  { 0x0, 0x0, 146, 1207, 2299, 0, 0, -1 },
  { 0x1, 0x1, 146, 1208, 2307, 36, 1, 55 },
  { 0x1, 0x1, 146, 1209, 2973, 36, 1, 55 },
  { 0x0, 0x0, 146, 1210, 2374, 0, 0, -1 },
  { 0x1, 0x1, 146, 1211, -1, 27, 1, 50 },
  { 0x1, 0x1, 147, -1, 2882, 37, 1, 1 },
  { 0x1, 0x1, 147, -1, 2885, 37, 1, 1 },
  { 0x1, 0x1, 147, -1, 2902, 37, 1, 1 },
  { 0x1, 0x1, 147, -1, 2905, 37, 1, 1 },
  { 0x1, 0x1, 147, -1, 2922, 37, 1, 1 },
  { 0x1, 0x1, 147, -1, 2925, 37, 1, 1 },
  { 0x0, 0x0, 147, -1, 2952, 0, 1, 1 },
  { 0x0, 0x0, 147, -1, 2953, 0, 1, 1 },
  { 0x0, 0x0, 148, -1, -1, 0, 1, 34 },
  { 0x0, 0x0, 148, 1135, -1, 0, 1, 41 },
  { 0x0, 0x0, 149, -1, -1, 0, 1, 41 },
  { 0x0, 0x0, 149, -1, -1, 0, 1, 67 },
  { 0x0, 0x0, 149, -1, 2960, 0, 1, 64 },
  { 0x0, 0x0, 149, -1, 2961, 0, 1, 64 },
  { 0x0, 0x0, 149, -1, -1, 0, 1, 41 },
  { 0x0, 0x0, 149, -1, -1, 0, 1, 87 },
  { 0x0, 0x0, 149, -1, -1, 0, 1, 87 },
  { 0x0, 0x0, 149, -1, -1, 0, 1, 92 },
  { 0x0, 0x0, 149, -1, -1, 0, 1, 41 },
  { 0x1, 0x1, 150, -1, 593, 12, 1, 6 },
  { 0x1, 0x1, 150, -1, 596, 12, 1, 6 },
  { 0x200001, 0x200001, 150, -1, 598, 12, 1, 6 },
  { 0x400001, 0x400001, 150, -1, 600, 12, 1, 6 },
  { 0x600001, 0x600001, 150, -1, 602, 12, 1, 6 },
  { 0x1, 0x1, 150, -1, 604, 12, 1, 6 },
  { 0x200001, 0x200001, 150, -1, 606, 12, 1, 6 },
  { 0x400001, 0x400001, 150, -1, 608, 12, 1, 6 },
  { 0x600001, 0x600001, 150, -1, 610, 12, 1, 6 },
  { 0x41, 0x41, 150, -1, 612, 6, 1, 7 },
  { 0x8000041, 0x8000041, 150, -1, 614, 6, 1, 7 },
  { 0x10000041, 0x10000041, 150, -1, 616, 6, 1, 7 },
  { 0x18000041, 0x18000041, 150, -1, 618, 6, 1, 7 },
  { 0x1, 0x1, 150, -1, 632, 12, 1, 8 },
  { 0x200001, 0x200001, 150, -1, 634, 12, 1, 8 },
  { 0x400001, 0x400001, 150, -1, 636, 12, 1, 8 },
  { 0x600001, 0x600001, 150, -1, 638, 12, 1, 8 },
  { 0x1, 0x1, 150, -1, 644, 12, 1, 16 },
  { 0x200001, 0x200001, 150, -1, 646, 12, 1, 16 },
  { 0x400001, 0x400001, 150, -1, 648, 12, 1, 16 },
  { 0x600001, 0x600001, 150, -1, 650, 12, 1, 16 },
  { 0x1, 0x1, 150, -1, 656, 12, 1, 18 },
  { 0x1, 0x1, 150, -1, 659, 12, 1, 18 },
  { 0x200001, 0x200001, 150, -1, 661, 12, 1, 18 },
  { 0x400001, 0x400001, 150, -1, 663, 12, 1, 18 },
  { 0x600001, 0x600001, 150, -1, 665, 12, 1, 18 },
  { 0x1, 0x1, 150, -1, 667, 12, 1, 18 },
  { 0x200001, 0x200001, 150, -1, 669, 12, 1, 18 },
  { 0x400001, 0x400001, 150, -1, 671, 12, 1, 18 },
  { 0x600001, 0x600001, 150, -1, 673, 12, 1, 18 },
  { 0x1, 0x1, 150, -1, 683, 12, 1, 19 },
  { 0x200001, 0x200001, 150, -1, 685, 12, 1, 19 },
  { 0x400001, 0x400001, 150, -1, 687, 12, 1, 19 },
  { 0x600001, 0x600001, 150, -1, 689, 12, 1, 19 },
  { 0x41, 0x41, 150, -1, 691, 6, 1, 19 },
  { 0x8000041, 0x8000041, 150, -1, 693, 6, 1, 19 },
  { 0x10000041, 0x10000041, 150, -1, 695, 6, 1, 19 },
  { 0x18000041, 0x18000041, 150, -1, 697, 6, 1, 19 },
  { 0x1, 0x1, 150, -1, 707, 12, 1, 20 },
  { 0x200001, 0x200001, 150, -1, 709, 12, 1, 20 },
  { 0x400001, 0x400001, 150, -1, 711, 12, 1, 20 },
  { 0x600001, 0x600001, 150, -1, 713, 12, 1, 20 },
  { 0x1, 0x1, 150, -1, 719, 12, 1, 21 },
  { 0x200001, 0x200001, 150, -1, 721, 12, 1, 21 },
  { 0x400001, 0x400001, 150, -1, 723, 12, 1, 21 },
  { 0x600001, 0x600001, 150, -1, 725, 12, 1, 21 },
  { 0x41, 0x41, 150, -1, 727, 6, 1, 21 },
  { 0x8000041, 0x8000041, 150, -1, 729, 6, 1, 21 },
  { 0x10000041, 0x10000041, 150, -1, 731, 6, 1, 21 },
  { 0x18000041, 0x18000041, 150, -1, 733, 6, 1, 21 },
  { 0x1, 0x1, 150, -1, 743, 12, 1, 22 },
  { 0x200001, 0x200001, 150, -1, 745, 12, 1, 22 },
  { 0x400001, 0x400001, 150, -1, 747, 12, 1, 22 },
  { 0x600001, 0x600001, 150, -1, 749, 12, 1, 22 },
  { 0x1, 0x1, 150, -1, 755, 12, 1, 18 },
  { 0x1, 0x1, 150, -1, 758, 12, 1, 18 },
  { 0x200001, 0x200001, 150, -1, 760, 12, 1, 18 },
  { 0x400001, 0x400001, 150, -1, 762, 12, 1, 18 },
  { 0x600001, 0x600001, 150, -1, 764, 12, 1, 18 },
  { 0x1, 0x1, 150, -1, 766, 12, 1, 18 },
  { 0x200001, 0x200001, 150, -1, 768, 12, 1, 18 },
  { 0x400001, 0x400001, 150, -1, 770, 12, 1, 18 },
  { 0x600001, 0x600001, 150, -1, 772, 12, 1, 18 },
  { 0x1, 0x1, 150, -1, 782, 12, 1, 22 },
  { 0x200001, 0x200001, 150, -1, 784, 12, 1, 22 },
  { 0x400001, 0x400001, 150, -1, 786, 12, 1, 22 },
  { 0x600001, 0x600001, 150, -1, 788, 12, 1, 22 },
  { 0x0, 0x0, 155, -1, -1, 0, 1, 131 },
  { 0x0, 0x0, 159, 793, -1, 0, 1, 81 },
  { 0x0, 0x0, 159, 794, -1, 0, 1, 81 },
  { 0x9, 0x9, 159, -1, 1456, 32, 1, 137 },
  { 0x9, 0x9, 159, -1, 1465, 32, 1, 137 },
  { 0x9, 0x9, 159, -1, 1474, 32, 1, 137 },
  { 0x9, 0x9, 159, -1, 1487, 32, 1, 137 },
  { 0x9, 0x9, 159, -1, 1496, 32, 1, 137 },
  { 0x9, 0x9, 159, -1, 1505, 32, 1, 137 },
  { 0x9, 0x9, 159, -1, 1514, 32, 1, 137 },
  { 0x9, 0x9, 159, -1, 1523, 32, 1, 137 },
  { 0x9, 0x9, 159, -1, 1532, 32, 1, 137 },
  { 0x9, 0x9, 159, -1, 1542, 32, 1, 137 },
  { 0x9, 0x9, 159, -1, 1552, 32, 1, 137 },
  { 0x9, 0x9, 159, -1, 1562, 32, 1, 137 },
  { 0x9, 0x9, 159, -1, 1571, 32, 1, 151 },
  { 0x9, 0x9, 159, -1, 1577, 32, 1, 156 },
  { 0x9, 0x9, 159, -1, 1583, 32, 1, 156 },
  { 0x9, 0x9, 159, -1, 1589, 32, 1, 151 },
  { 0x9, 0x9, 159, -1, 1595, 32, 1, 156 },
  { 0x9, 0x9, 159, -1, 1601, 32, 1, 156 },
  { 0x9, 0x9, 159, -1, 1607, 32, 1, 151 },
  { 0x9, 0x9, 159, -1, 1613, 32, 1, 156 },
  { 0x9, 0x9, 159, -1, 1619, 32, 1, 156 },
  { 0x9, 0x9, 159, -1, 1625, 32, 1, 151 },
  { 0x9, 0x9, 159, -1, 1631, 32, 1, 156 },
  { 0x9, 0x9, 159, -1, 1637, 32, 1, 151 },
  { 0x9, 0x9, 159, -1, 1643, 32, 1, 156 },
  { 0x9, 0x9, 159, -1, 1649, 32, 1, 151 },
  { 0x9, 0x9, 159, -1, 1655, 32, 1, 156 },
  { 0x9, 0x9, 159, -1, 1661, 32, 1, 151 },
  { 0x9, 0x9, 159, -1, 1667, 32, 1, 156 },
  { 0x9, 0x9, 159, -1, 1673, 32, 1, 156 },
  { 0x0, 0x0, 160, 1253, 298, 0, 0, -1 },
  { 0x0, 0x0, 160, 1254, 422, 0, 0, -1 },
  { 0x1, 0x1, 160, -1, 2896, 38, 1, 1 },
  { 0x1, 0x1, 160, 925, 2899, 38, 1, 1 },
  { 0x0, 0x0, 160, 926, 423, 0, 0, -1 },
  { 0x0, 0x0, 160, 1255, 320, 0, 0, -1 },
  { 0x0, 0x0, 160, 1256, 462, 0, 0, -1 },
  { 0x1, 0x1, 160, -1, 2912, 38, 1, 1 },
  { 0x1, 0x1, 160, 929, 2915, 38, 1, 1 },
  { 0x0, 0x0, 160, 930, 463, 0, 0, -1 },
  { 0x0, 0x0, 160, 931, 325, 0, 0, -1 },
  { 0x0, 0x0, 160, 932, 343, 0, 0, -1 },
  { 0x0, 0x0, 160, 1257, 346, 0, 0, -1 },
  { 0x0, 0x0, 160, 1258, 470, 0, 0, -1 },
  { 0x1, 0x1, 160, -1, 2936, 38, 1, 1 },
  { 0x1, 0x1, 160, 935, 2939, 38, 1, 1 },
  { 0x0, 0x0, 160, 936, 471, 0, 0, -1 },
  { 0x0, 0x0, 160, -1, 368, 0, 0, -1 },
  { 0x0, 0x0, 160, -1, 510, 0, 0, -1 },
  { 0x1, 0x1, 160, -1, 2949, 38, 1, 1 },
  { 0x1, 0x1, 160, 939, 2951, 38, 1, 1 },
  { 0x0, 0x0, 160, 940, 511, 0, 0, -1 },
  { 0x0, 0x0, 160, 941, 373, 0, 0, -1 },
  { 0x0, 0x0, 160, 942, 391, 0, 0, -1 },
  { 0x0, 0x0, 161, 1415, 2321, 0, 0, -1 },
  { 0x0, 0x0, 161, 1416, 2329, 0, 1, 55 },
  { 0x0, 0x0, 161, 1417, 2990, 0, 1, 55 },
  { 0x0, 0x0, 161, 1418, 2377, 0, 0, -1 },
  { 0x1, 0x1, 161, 1419, -1, 29, 1, 50 },
  { 0x0, 0x0, 162, -1, 2339, 0, 0, -1 },
  { 0x1, 0x9, 162, -1, 2343, 33, 1, 55 },
  { 0x1, 0x9, 162, -1, 2999, 33, 1, 55 },
  { 0x6, 0x7, 162, -1, 2384, 27, 1, 50 },
  { 0x0, 0x0, 163, 1401, 2337, 0, 0, -1 },
  { 0x0, 0x0, 163, 1402, 2341, 0, 1, 55 },
  { 0x0, 0x0, 163, 1403, 2998, 0, 1, 55 },
  { 0x1, 0x1, 163, 1404, 2383, 29, 1, 50 },
  { 0x1, 0x1, 164, 1422, -1, 27, 1, 34 },
  { 0x0, 0x0, 165, 2193, 2325, 0, 0, -1 },
  { 0x1, 0x1, 165, 2194, 2333, 33, 1, 55 },
  { 0x1, 0x1, 165, 2195, 2992, 33, 1, 55 },
  { 0x0, 0x0, 165, 2196, 2379, 0, 0, -1 },
  { 0x3, 0x3, 165, 2197, -1, 28, 1, 50 },
  { 0x0, 0x0, 166, 1410, 2323, 0, 0, -1 },
  { 0x1, 0x1, 166, 1411, 2331, 36, 1, 55 },
  { 0x1, 0x1, 166, 1412, 2991, 36, 1, 55 },
  { 0x0, 0x0, 166, 1413, 2378, 0, 0, -1 },
  { 0x5, 0x5, 166, 1414, -1, 27, 1, 50 },
  { 0x0, 0x0, 167, -1, 2962, 0, 1, 64 },
  { 0x0, 0x0, 167, -1, 2963, 0, 1, 64 },
  { 0x1, 0x1, 169, -1, -1, 28, 1, 34 },
  { 0x1, 0x1, 170, 2779, -1, 27, 1, 34 },
  { 0x1, 0x1, 170, 2780, -1, 27, 1, 34 },
  { 0x1, 0x1, 171, 1703, -1, 28, 1, 142 },
  { 0x1, 0x1, 171, 1704, -1, 28, 1, 142 },
  { 0x1, 0x1, 171, 1705, -1, 28, 1, 142 },
  { 0x1, 0x1, 171, 1706, -1, 28, 1, 142 },
  { 0x1, 0x1, 171, 1707, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1708, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1709, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1710, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1711, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1712, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1713, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1714, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1715, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1716, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1717, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1718, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1719, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1720, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1721, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1722, -1, 28, 1, 141 },
  { 0x1, 0x1, 171, 1723, -1, 28, 1, 143 },
  { 0x1, 0x1, 171, 1724, -1, 28, 1, 143 },
  { 0x1, 0x1, 171, 1725, -1, 28, 1, 143 },
  { 0x1, 0x1, 171, 1726, -1, 28, 1, 143 },
  { 0x1, 0x1, 171, 1727, -1, 28, 1, 133 },
  { 0x1, 0x1, 171, 1728, -1, 28, 1, 134 },
  { 0x1, 0x1, 171, 1729, -1, 28, 1, 135 },
  { 0x1, 0x1, 171, 1730, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1731, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1732, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1733, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1734, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1735, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1736, -1, 28, 1, 133 },
  { 0x1, 0x1, 171, 1737, -1, 28, 1, 134 },
  { 0x1, 0x1, 171, 1738, -1, 28, 1, 135 },
  { 0x1, 0x1, 171, 1739, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1740, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1741, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1742, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1743, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1744, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1745, -1, 28, 1, 133 },
  { 0x1, 0x1, 171, 1746, -1, 28, 1, 134 },
  { 0x1, 0x1, 171, 1747, -1, 28, 1, 135 },
  { 0x1, 0x1, 171, 1748, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1749, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1750, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1751, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1752, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1753, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1754, -1, 28, 1, 132 },
  { 0x1, 0x1, 171, 1755, -1, 28, 1, 132 },
  { 0x1, 0x1, 171, 1756, -1, 28, 1, 132 },
  { 0x1, 0x1, 171, 1757, -1, 28, 1, 132 },
  { 0x1, 0x1, 171, 1758, -1, 28, 1, 133 },
  { 0x1, 0x1, 171, 1759, -1, 28, 1, 134 },
  { 0x1, 0x1, 171, 1760, -1, 28, 1, 135 },
  { 0x1, 0x1, 171, 1761, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1762, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1763, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1764, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1765, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1766, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1767, -1, 28, 1, 133 },
  { 0x1, 0x1, 171, 1768, -1, 28, 1, 134 },
  { 0x1, 0x1, 171, 1769, -1, 28, 1, 135 },
  { 0x1, 0x1, 171, 1770, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1771, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1772, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1773, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1774, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1775, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1776, -1, 28, 1, 133 },
  { 0x1, 0x1, 171, 1777, -1, 28, 1, 134 },
  { 0x1, 0x1, 171, 1778, -1, 28, 1, 135 },
  { 0x1, 0x1, 171, 1779, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1780, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1781, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1782, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1783, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1784, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1785, -1, 28, 1, 133 },
  { 0x1, 0x1, 171, 1786, -1, 28, 1, 134 },
  { 0x1, 0x1, 171, 1787, -1, 28, 1, 135 },
  { 0x1, 0x1, 171, 1788, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1789, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1790, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1791, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1792, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1793, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1794, -1, 28, 1, 133 },
  { 0x1, 0x1, 171, 1795, -1, 28, 1, 134 },
  { 0x1, 0x1, 171, 1796, -1, 28, 1, 135 },
  { 0x1, 0x1, 171, 1797, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1798, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1799, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1800, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1801, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1802, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1803, -1, 28, 1, 133 },
  { 0x1, 0x1, 171, 1804, -1, 28, 1, 134 },
  { 0x1, 0x1, 171, 1805, -1, 28, 1, 135 },
  { 0x1, 0x1, 171, 1806, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1807, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1808, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1809, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1810, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1811, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1812, -1, 28, 1, 133 },
  { 0x1, 0x1, 171, 1813, -1, 28, 1, 134 },
  { 0x1, 0x1, 171, 1814, -1, 28, 1, 135 },
  { 0x1, 0x1, 171, 1815, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1816, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1817, -1, 28, 1, 136 },
  { 0x1, 0x1, 171, 1818, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1819, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1820, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1821, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1822, -1, 28, 1, 133 },
  { 0x1, 0x1, 171, 1823, -1, 28, 1, 134 },
  { 0x1, 0x1, 171, 1824, -1, 28, 1, 135 },
  { 0x1, 0x1, 171, 1825, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1826, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1827, -1, 28, 1, 136 },
  { 0x1, 0x1, 171, 1828, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1829, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1830, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1831, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1832, -1, 28, 1, 133 },
  { 0x1, 0x1, 171, 1833, -1, 28, 1, 134 },
  { 0x1, 0x1, 171, 1834, -1, 28, 1, 135 },
  { 0x1, 0x1, 171, 1835, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1836, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1837, -1, 28, 1, 136 },
  { 0x1, 0x1, 171, 1838, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1839, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1840, -1, 28, 1, 137 },
  { 0x1, 0x1, 171, 1841, -1, 28, 1, 131 },
  { 0x1, 0x1, 171, 1842, -1, 28, 1, 147 },
  { 0x1, 0x1, 171, 1843, -1, 28, 1, 152 },
  { 0x1, 0x1, 171, 1844, -1, 28, 1, 152 },
  { 0x1, 0x1, 171, 1845, -1, 28, 1, 148 },
  { 0x1, 0x1, 171, 1846, -1, 28, 1, 149 },
  { 0x1, 0x1, 171, 1847, -1, 28, 1, 150 },
  { 0x1, 0x1, 171, 1848, -1, 28, 1, 151 },
  { 0x1, 0x1, 171, 1849, -1, 28, 1, 151 },
  { 0x1, 0x1, 171, 1850, -1, 28, 1, 147 },
  { 0x1, 0x1, 171, 1851, -1, 28, 1, 153 },
  { 0x1, 0x1, 171, 1852, -1, 28, 1, 154 },
  { 0x1, 0x1, 171, 1853, -1, 28, 1, 155 },
  { 0x1, 0x1, 171, 1854, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1855, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1856, -1, 28, 1, 152 },
  { 0x1, 0x1, 171, 1857, -1, 28, 1, 153 },
  { 0x1, 0x1, 171, 1858, -1, 28, 1, 154 },
  { 0x1, 0x1, 171, 1859, -1, 28, 1, 155 },
  { 0x1, 0x1, 171, 1860, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1861, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1862, -1, 28, 1, 152 },
  { 0x1, 0x1, 171, 1863, -1, 28, 1, 148 },
  { 0x1, 0x1, 171, 1864, -1, 28, 1, 149 },
  { 0x1, 0x1, 171, 1865, -1, 28, 1, 150 },
  { 0x1, 0x1, 171, 1866, -1, 28, 1, 151 },
  { 0x1, 0x1, 171, 1867, -1, 28, 1, 151 },
  { 0x1, 0x1, 171, 1868, -1, 28, 1, 147 },
  { 0x1, 0x1, 171, 1869, -1, 28, 1, 153 },
  { 0x1, 0x1, 171, 1870, -1, 28, 1, 154 },
  { 0x1, 0x1, 171, 1871, -1, 28, 1, 155 },
  { 0x1, 0x1, 171, 1872, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1873, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1874, -1, 28, 1, 152 },
  { 0x1, 0x1, 171, 1875, -1, 28, 1, 153 },
  { 0x1, 0x1, 171, 1876, -1, 28, 1, 154 },
  { 0x1, 0x1, 171, 1877, -1, 28, 1, 155 },
  { 0x1, 0x1, 171, 1878, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1879, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1880, -1, 28, 1, 152 },
  { 0x1, 0x1, 171, 1881, -1, 28, 1, 148 },
  { 0x1, 0x1, 171, 1882, -1, 28, 1, 149 },
  { 0x1, 0x1, 171, 1883, -1, 28, 1, 150 },
  { 0x1, 0x1, 171, 1884, -1, 28, 1, 151 },
  { 0x1, 0x1, 171, 1885, -1, 28, 1, 151 },
  { 0x1, 0x1, 171, 1886, -1, 28, 1, 147 },
  { 0x1, 0x1, 171, 1887, -1, 28, 1, 153 },
  { 0x1, 0x1, 171, 1888, -1, 28, 1, 154 },
  { 0x1, 0x1, 171, 1889, -1, 28, 1, 155 },
  { 0x1, 0x1, 171, 1890, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1891, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1892, -1, 28, 1, 152 },
  { 0x1, 0x1, 171, 1893, -1, 28, 1, 153 },
  { 0x1, 0x1, 171, 1894, -1, 28, 1, 154 },
  { 0x1, 0x1, 171, 1895, -1, 28, 1, 155 },
  { 0x1, 0x1, 171, 1896, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1897, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1898, -1, 28, 1, 152 },
  { 0x1, 0x1, 171, 1899, -1, 28, 1, 148 },
  { 0x1, 0x1, 171, 1900, -1, 28, 1, 149 },
  { 0x1, 0x1, 171, 1901, -1, 28, 1, 150 },
  { 0x1, 0x1, 171, 1902, -1, 28, 1, 151 },
  { 0x1, 0x1, 171, 1903, -1, 28, 1, 151 },
  { 0x1, 0x1, 171, 1904, -1, 28, 1, 147 },
  { 0x1, 0x1, 171, 1905, -1, 28, 1, 153 },
  { 0x1, 0x1, 171, 1906, -1, 28, 1, 154 },
  { 0x1, 0x1, 171, 1907, -1, 28, 1, 155 },
  { 0x1, 0x1, 171, 1908, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1909, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1910, -1, 28, 1, 152 },
  { 0x1, 0x1, 171, 1911, -1, 28, 1, 148 },
  { 0x1, 0x1, 171, 1912, -1, 28, 1, 149 },
  { 0x1, 0x1, 171, 1913, -1, 28, 1, 150 },
  { 0x1, 0x1, 171, 1914, -1, 28, 1, 151 },
  { 0x1, 0x1, 171, 1915, -1, 28, 1, 151 },
  { 0x1, 0x1, 171, 1916, -1, 28, 1, 147 },
  { 0x1, 0x1, 171, 1917, -1, 28, 1, 153 },
  { 0x1, 0x1, 171, 1918, -1, 28, 1, 154 },
  { 0x1, 0x1, 171, 1919, -1, 28, 1, 155 },
  { 0x1, 0x1, 171, 1920, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1921, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1922, -1, 28, 1, 152 },
  { 0x1, 0x1, 171, 1923, -1, 28, 1, 148 },
  { 0x1, 0x1, 171, 1924, -1, 28, 1, 149 },
  { 0x1, 0x1, 171, 1925, -1, 28, 1, 150 },
  { 0x1, 0x1, 171, 1926, -1, 28, 1, 151 },
  { 0x1, 0x1, 171, 1927, -1, 28, 1, 151 },
  { 0x1, 0x1, 171, 1928, -1, 28, 1, 147 },
  { 0x1, 0x1, 171, 1929, -1, 28, 1, 153 },
  { 0x1, 0x1, 171, 1930, -1, 28, 1, 154 },
  { 0x1, 0x1, 171, 1931, -1, 28, 1, 155 },
  { 0x1, 0x1, 171, 1932, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1933, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1934, -1, 28, 1, 152 },
  { 0x1, 0x1, 171, 1935, -1, 28, 1, 148 },
  { 0x1, 0x1, 171, 1936, -1, 28, 1, 149 },
  { 0x1, 0x1, 171, 1937, -1, 28, 1, 150 },
  { 0x1, 0x1, 171, 1938, -1, 28, 1, 151 },
  { 0x1, 0x1, 171, 1939, -1, 28, 1, 151 },
  { 0x1, 0x1, 171, 1940, -1, 28, 1, 147 },
  { 0x1, 0x1, 171, 1941, -1, 28, 1, 153 },
  { 0x1, 0x1, 171, 1942, -1, 28, 1, 154 },
  { 0x1, 0x1, 171, 1943, -1, 28, 1, 155 },
  { 0x1, 0x1, 171, 1944, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1945, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1946, -1, 28, 1, 152 },
  { 0x1, 0x1, 171, 1947, -1, 28, 1, 153 },
  { 0x1, 0x1, 171, 1948, -1, 28, 1, 154 },
  { 0x1, 0x1, 171, 1949, -1, 28, 1, 155 },
  { 0x1, 0x1, 171, 1950, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1951, -1, 28, 1, 156 },
  { 0x1, 0x1, 171, 1952, -1, 28, 1, 152 },
  { 0x1, 0x1, 171, 1691, -1, 28, 1, 158 },
  { 0x1, 0x1, 171, 1692, -1, 28, 1, 158 },
  { 0x1, 0x1, 171, 1693, -1, 28, 1, 158 },
  { 0x1, 0x1, 171, 1694, -1, 28, 1, 158 },
  { 0x1, 0x1, 171, 1695, -1, 28, 1, 159 },
  { 0x1, 0x1, 171, 1696, -1, 28, 1, 159 },
  { 0x1, 0x1, 171, 1697, -1, 28, 1, 159 },
  { 0x1, 0x1, 171, 1698, -1, 28, 1, 159 },
  { 0x1, 0x1, 171, 1699, -1, 28, 1, 159 },
  { 0x1, 0x1, 171, 1700, -1, 28, 1, 159 },
  { 0x1, 0x1, 171, 1701, -1, 28, 1, 159 },
  { 0x1, 0x1, 171, 1702, -1, 28, 1, 159 },
  { 0x1, 0x1, 171, 1997, -1, 28, 1, 143 },
  { 0x1, 0x1, 171, 1998, -1, 28, 1, 143 },
  { 0x1, 0x1, 171, 1999, -1, 28, 1, 143 },
  { 0x1, 0x1, 171, 2000, -1, 28, 1, 143 },
  { 0x1, 0x1, 172, 1953, -1, 29, 1, 158 },
  { 0x1, 0x1, 172, 1954, -1, 29, 1, 158 },
  { 0x1, 0x1, 172, 1955, -1, 29, 1, 158 },
  { 0x1, 0x1, 172, 1956, -1, 29, 1, 158 },
  { 0x1, 0x1, 172, 1957, -1, 29, 1, 159 },
  { 0x1, 0x1, 172, 1958, -1, 29, 1, 159 },
  { 0x1, 0x1, 172, 1959, -1, 29, 1, 159 },
  { 0x1, 0x1, 172, 1960, -1, 29, 1, 159 },
  { 0x1, 0x1, 172, 1961, -1, 29, 1, 159 },
  { 0x1, 0x1, 172, 1962, -1, 29, 1, 159 },
  { 0x1, 0x1, 172, 1963, -1, 29, 1, 159 },
  { 0x1, 0x1, 172, 1964, -1, 29, 1, 159 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 142 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 142 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 142 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 142 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 141 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 143 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 143 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 143 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 143 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 133 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 134 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 135 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 271, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 2258, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 133 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 134 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 135 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 273, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 2259, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 133 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 134 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 135 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 275, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 2260, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 132 },
  { 0x3, 0x3, 173, 277, -1, 28, 1, 132 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 132 },
  { 0x3, 0x3, 173, 278, -1, 28, 1, 132 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 133 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 134 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 135 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 279, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 2261, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 133 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 134 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 135 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 281, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 2262, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 133 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 134 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 135 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 283, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 2263, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 133 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 134 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 135 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 285, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 2264, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 133 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 134 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 135 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 287, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 2265, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 133 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 134 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 135 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 289, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 2266, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 133 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 134 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 135 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 136 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 291, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 2267, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 133 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 134 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 135 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 136 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 293, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 2268, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 133 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 134 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 135 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 136 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 295, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 137 },
  { 0x3, 0x3, 173, 2269, -1, 28, 1, 131 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 147 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 152 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 152 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 148 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 149 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 150 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 151 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 151 },
  { 0x3, 0x3, 173, 2270, -1, 28, 1, 147 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 153 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 154 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 155 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, 2271, -1, 28, 1, 152 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 153 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 154 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 155 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, 2272, -1, 28, 1, 152 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 148 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 149 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 150 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 151 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 151 },
  { 0x3, 0x3, 173, 2273, -1, 28, 1, 147 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 153 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 154 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 155 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, 2274, -1, 28, 1, 152 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 153 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 154 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 155 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, 2275, -1, 28, 1, 152 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 148 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 149 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 150 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 151 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 151 },
  { 0x3, 0x3, 173, 2276, -1, 28, 1, 147 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 153 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 154 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 155 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, 2277, -1, 28, 1, 152 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 153 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 154 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 155 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, 2278, -1, 28, 1, 152 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 148 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 149 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 150 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 151 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 151 },
  { 0x3, 0x3, 173, 2279, -1, 28, 1, 147 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 153 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 154 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 155 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, 2280, -1, 28, 1, 152 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 148 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 149 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 150 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 151 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 151 },
  { 0x3, 0x3, 173, 2281, -1, 28, 1, 147 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 153 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 154 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 155 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, 2282, -1, 28, 1, 152 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 148 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 149 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 150 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 151 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 151 },
  { 0x3, 0x3, 173, 2283, -1, 28, 1, 147 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 153 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 154 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 155 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, 2284, -1, 28, 1, 152 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 148 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 149 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 150 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 151 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 151 },
  { 0x3, 0x3, 173, 2285, -1, 28, 1, 147 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 153 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 154 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 155 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, 2286, -1, 28, 1, 152 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 153 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 154 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 155 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 156 },
  { 0x3, 0x3, 173, 2287, -1, 28, 1, 152 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 158 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 158 },
  { 0x3, 0x3, 173, 951, -1, 28, 1, 158 },
  { 0x3, 0x3, 173, 952, -1, 28, 1, 158 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 159 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 159 },
  { 0x3, 0x3, 173, 953, -1, 28, 1, 159 },
  { 0x3, 0x3, 173, 954, -1, 28, 1, 159 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 159 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 159 },
  { 0x3, 0x3, 173, 955, -1, 28, 1, 159 },
  { 0x3, 0x3, 173, 956, -1, 28, 1, 159 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 138 },
  { 0x3, 0x3, 173, 2224, -1, 28, 1, 138 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 145 },
  { 0x3, 0x3, 173, 2225, -1, 28, 1, 145 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 139 },
  { 0x3, 0x3, 173, 2226, -1, 28, 1, 139 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 139 },
  { 0x3, 0x3, 173, 2227, -1, 28, 1, 139 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 138 },
  { 0x3, 0x3, 173, 2228, -1, 28, 1, 138 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 145 },
  { 0x3, 0x3, 173, 2229, -1, 28, 1, 145 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 138 },
  { 0x3, 0x3, 173, 2230, -1, 28, 1, 138 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 145 },
  { 0x3, 0x3, 173, 2231, -1, 28, 1, 145 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 138 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 140 },
  { 0x3, 0x3, 173, 2232, -1, 28, 1, 138 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 145 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 146 },
  { 0x3, 0x3, 173, 2233, -1, 28, 1, 145 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 157 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 161 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 157 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 161 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 157 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 161 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 157 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 161 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 157 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 161 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 143 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 143 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 143 },
  { 0x3, 0x3, 173, -1, -1, 28, 1, 143 },
  { 0x0, 0x0, 174, -1, 394, 0, 0, -1 },
  { 0x0, 0x0, 174, -1, 396, 0, 0, -1 },
  { 0x0, 0x0, 174, 3042, 3002, 0, 1, 1 },
  { 0x0, 0x0, 174, 3043, 3003, 0, 1, 1 },
  { 0x0, 0x0, 174, -1, 402, 0, 0, -1 },
  { 0x0, 0x0, 174, -1, 404, 0, 0, -1 },
  { 0x0, 0x0, 174, 3046, 3006, 0, 1, 76 },
  { 0x0, 0x0, 174, 3047, 3007, 0, 1, 76 },
  { 0x0, 0x0, 174, -1, 410, 0, 0, -1 },
  { 0x0, 0x0, 174, -1, 412, 0, 0, -1 },
  { 0x0, 0x0, 174, 3050, 3010, 0, 1, 1 },
  { 0x0, 0x0, 174, 3051, 3011, 0, 1, 1 },
  { 0x11, 0x31, 175, 2881, 417, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 418, 12, 1, 4 },
  { 0x11, 0x31, 175, 2073, 419, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 421, 12, 1, 4 },
  { 0x1, 0x1, 175, -1, 425, 37, 1, 4 },
  { 0x2000001, 0x2000001, 175, -1, 426, 12, 1, 4 },
  { 0x11, 0x11, 175, -1, 427, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 428, 12, 1, 4 },
  { 0x1, 0x1, 175, 2079, 429, 37, 1, 4 },
  { 0x2000001, 0x2000001, 175, -1, 431, 12, 1, 4 },
  { 0x11, 0x11, 175, 2081, 433, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 435, 12, 1, 4 },
  { 0x1, 0x1, 175, 2083, 437, 37, 1, 4 },
  { 0x2000001, 0x2000001, 175, -1, 439, 12, 1, 4 },
  { 0x11, 0x11, 175, 2085, 441, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 443, 12, 1, 4 },
  { 0x1, 0x1, 175, 2087, 445, 37, 1, 4 },
  { 0x2000001, 0x2000001, 175, -1, 447, 12, 1, 4 },
  { 0x11, 0x11, 175, 2089, 449, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 451, 12, 1, 4 },
  { 0x11, 0x31, 175, 2901, 457, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 458, 12, 1, 4 },
  { 0x11, 0x31, 175, 2095, 459, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 461, 12, 1, 4 },
  { 0x11, 0x31, 175, 2921, 465, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 466, 12, 1, 4 },
  { 0x11, 0x31, 175, 2121, 467, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 469, 12, 1, 4 },
  { 0x1, 0x1, 175, -1, 473, 37, 1, 4 },
  { 0x2000001, 0x2000001, 175, -1, 474, 12, 1, 4 },
  { 0x11, 0x11, 175, -1, 475, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 476, 12, 1, 4 },
  { 0x1, 0x1, 175, 2127, 477, 37, 1, 4 },
  { 0x2000001, 0x2000001, 175, -1, 479, 12, 1, 4 },
  { 0x11, 0x11, 175, 2129, 481, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 483, 12, 1, 4 },
  { 0x1, 0x1, 175, 2131, 485, 37, 1, 4 },
  { 0x2000001, 0x2000001, 175, -1, 487, 12, 1, 4 },
  { 0x11, 0x11, 175, 2133, 489, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 491, 12, 1, 4 },
  { 0x1, 0x1, 175, 2135, 493, 37, 1, 4 },
  { 0x2000001, 0x2000001, 175, -1, 495, 12, 1, 4 },
  { 0x11, 0x11, 175, 2137, 497, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 499, 12, 1, 4 },
  { 0x11, 0x31, 175, 2941, 505, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 506, 12, 1, 4 },
  { 0x11, 0x31, 175, 2143, 507, 33, 1, 4 },
  { 0x2200001, 0x2200001, 175, -1, 509, 12, 1, 4 },
  { 0x1, 0x1, 175, -1, 513, 33, 1, 4 },
  { 0x200001, 0x200001, 175, -1, 514, 12, 1, 4 },
  { 0x1, 0x1, 175, -1, 515, 33, 1, 4 },
  { 0x200001, 0x200001, 175, -1, 516, 12, 1, 4 },
  { 0x1, 0x1, 175, -1, 521, 33, 1, 79 },
  { 0x200001, 0x200001, 175, -1, 522, 12, 1, 79 },
  { 0x1, 0x1, 175, -1, 523, 33, 1, 79 },
  { 0x200001, 0x200001, 175, -1, 524, 12, 1, 79 },
  { 0x1, 0x1, 175, -1, 529, 33, 1, 4 },
  { 0x200001, 0x200001, 175, -1, 530, 12, 1, 4 },
  { 0x1, 0x1, 175, -1, 531, 33, 1, 4 },
  { 0x200001, 0x200001, 175, -1, 532, 12, 1, 4 },
  { 0x2200001, 0x6200001, 176, 2884, -1, 12, 1, 4 },
  { 0x11, 0x11, 176, 2016, -1, 33, 1, 4 },
  { 0x1, 0x1, 176, -1, -1, 33, 1, 5 },
  { 0x4200001, 0x4200001, 176, -1, -1, 12, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 37, 1, 4 },
  { 0x2000001, 0x2000001, 176, -1, -1, 12, 1, 4 },
  { 0x2000001, 0x2000001, 176, -1, -1, 12, 1, 4 },
  { 0x1, 0x1, 176, 2022, -1, 37, 1, 4 },
  { 0x2200001, 0x2200001, 176, -1, -1, 12, 1, 4 },
  { 0x11, 0x11, 176, 2024, -1, 33, 1, 4 },
  { 0x2000001, 0x2000001, 176, -1, -1, 12, 1, 4 },
  { 0x1, 0x1, 176, 2026, -1, 37, 1, 4 },
  { 0x2200001, 0x2200001, 176, -1, -1, 12, 1, 4 },
  { 0x11, 0x11, 176, 2028, -1, 33, 1, 4 },
  { 0x2000001, 0x2000001, 176, -1, -1, 12, 1, 4 },
  { 0x1, 0x1, 176, 2030, -1, 37, 1, 4 },
  { 0x2200001, 0x2200001, 176, -1, -1, 12, 1, 4 },
  { 0x11, 0x11, 176, 2032, -1, 33, 1, 4 },
  { 0x1, 0x1, 176, -1, -1, 37, 1, 4 },
  { 0x2000001, 0x2000001, 176, -1, -1, 12, 1, 4 },
  { 0x11, 0x11, 176, -1, -1, 33, 1, 4 },
  { 0x2200001, 0x2200001, 176, -1, -1, 12, 1, 4 },
  { 0x2200001, 0x6200001, 176, 2904, -1, 12, 1, 4 },
  { 0x11, 0x11, 176, 2036, -1, 33, 1, 4 },
  { 0x1, 0x1, 176, -1, -1, 33, 1, 5 },
  { 0x4200001, 0x4200001, 176, -1, -1, 12, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 37, 1, 4 },
  { 0x2000001, 0x2000001, 176, -1, -1, 12, 1, 4 },
  { 0x0, 0x0, 176, -1, -1, 0, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 12, 1, 5 },
  { 0x0, 0x0, 176, -1, -1, 0, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 12, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 33, 1, 5 },
  { 0x200001, 0x200001, 176, -1, -1, 12, 1, 5 },
  { 0x0, 0x0, 176, -1, -1, 0, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 12, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 33, 1, 5 },
  { 0x200001, 0x200001, 176, -1, -1, 12, 1, 5 },
  { 0x0, 0x0, 176, -1, -1, 0, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 12, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 33, 1, 5 },
  { 0x200001, 0x200001, 176, -1, -1, 12, 1, 5 },
  { 0x0, 0x0, 176, -1, -1, 0, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 12, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 33, 1, 5 },
  { 0x200001, 0x200001, 176, -1, -1, 12, 1, 5 },
  { 0x0, 0x0, 176, -1, -1, 0, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 12, 1, 5 },
  { 0x2200001, 0x6200001, 176, 2924, -1, 12, 1, 4 },
  { 0x11, 0x11, 176, 2040, -1, 33, 1, 4 },
  { 0x1, 0x1, 176, -1, -1, 33, 1, 5 },
  { 0x4200001, 0x4200001, 176, -1, -1, 12, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 37, 1, 4 },
  { 0x2000001, 0x2000001, 176, -1, -1, 12, 1, 4 },
  { 0x2000001, 0x2000001, 176, -1, -1, 12, 1, 4 },
  { 0x1, 0x1, 176, 2046, -1, 37, 1, 4 },
  { 0x2200001, 0x2200001, 176, -1, -1, 12, 1, 4 },
  { 0x11, 0x11, 176, 2048, -1, 33, 1, 4 },
  { 0x2000001, 0x2000001, 176, -1, -1, 12, 1, 4 },
  { 0x1, 0x1, 176, 2050, -1, 37, 1, 4 },
  { 0x2200001, 0x2200001, 176, -1, -1, 12, 1, 4 },
  { 0x11, 0x11, 176, 2052, -1, 33, 1, 4 },
  { 0x2000001, 0x2000001, 176, -1, -1, 12, 1, 4 },
  { 0x1, 0x1, 176, 2054, -1, 37, 1, 4 },
  { 0x2200001, 0x2200001, 176, -1, -1, 12, 1, 4 },
  { 0x11, 0x11, 176, 2056, -1, 33, 1, 4 },
  { 0x1, 0x1, 176, -1, -1, 37, 1, 4 },
  { 0x2000001, 0x2000001, 176, -1, -1, 12, 1, 4 },
  { 0x11, 0x11, 176, -1, -1, 33, 1, 4 },
  { 0x2200001, 0x2200001, 176, -1, -1, 12, 1, 4 },
  { 0x2200001, 0x6200001, 176, 2943, -1, 12, 1, 4 },
  { 0x11, 0x11, 176, 2060, -1, 33, 1, 4 },
  { 0x1, 0x1, 176, -1, -1, 33, 1, 5 },
  { 0x4200001, 0x4200001, 176, -1, -1, 12, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 37, 1, 4 },
  { 0x2000001, 0x2000001, 176, -1, -1, 12, 1, 4 },
  { 0x0, 0x0, 176, -1, -1, 0, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 12, 1, 5 },
  { 0x0, 0x0, 176, -1, -1, 0, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 12, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 33, 1, 5 },
  { 0x200001, 0x200001, 176, -1, -1, 12, 1, 5 },
  { 0x0, 0x0, 176, -1, -1, 0, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 12, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 33, 1, 5 },
  { 0x200001, 0x200001, 176, -1, -1, 12, 1, 5 },
  { 0x0, 0x0, 176, -1, -1, 0, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 12, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 33, 1, 5 },
  { 0x200001, 0x200001, 176, -1, -1, 12, 1, 5 },
  { 0x0, 0x0, 176, -1, -1, 0, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 12, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 33, 1, 5 },
  { 0x200001, 0x200001, 176, -1, -1, 12, 1, 5 },
  { 0x0, 0x0, 176, -1, -1, 0, 1, 5 },
  { 0x1, 0x1, 176, -1, -1, 12, 1, 5 },
  { 0x9, 0x9, 176, -1, -1, 33, 1, 5 },
  { 0x1, 0x1, 176, 397, -1, 33, 1, 4 },
  { 0x1200001, 0x1200001, 176, -1, -1, 12, 1, 5 },
  { 0x200001, 0x200001, 176, 398, -1, 12, 1, 4 },
  { 0x9, 0x9, 176, -1, -1, 33, 1, 5 },
  { 0x1, 0x1, 176, 399, -1, 33, 1, 4 },
  { 0x1200001, 0x1200001, 176, -1, -1, 12, 1, 5 },
  { 0x200001, 0x200001, 176, 400, -1, 12, 1, 4 },
  { 0x9, 0x9, 176, -1, -1, 33, 1, 80 },
  { 0x1, 0x1, 176, 405, -1, 33, 1, 79 },
  { 0x1200001, 0x1200001, 176, -1, -1, 12, 1, 80 },
  { 0x200001, 0x200001, 176, 406, -1, 12, 1, 79 },
  { 0x9, 0x9, 176, -1, -1, 33, 1, 80 },
  { 0x1, 0x1, 176, 407, -1, 33, 1, 79 },
  { 0x1200001, 0x1200001, 176, -1, -1, 12, 1, 80 },
  { 0x200001, 0x200001, 176, 408, -1, 12, 1, 79 },
  { 0x9, 0x9, 176, -1, -1, 33, 1, 5 },
  { 0x1, 0x1, 176, 413, -1, 33, 1, 4 },
  { 0x1200001, 0x1200001, 176, -1, -1, 12, 1, 5 },
  { 0x200001, 0x200001, 176, 414, -1, 12, 1, 4 },
  { 0x9, 0x9, 176, -1, -1, 33, 1, 5 },
  { 0x1, 0x1, 176, 415, -1, 33, 1, 4 },
  { 0x1200001, 0x1200001, 176, -1, -1, 12, 1, 5 },
  { 0x200001, 0x200001, 176, 416, -1, 12, 1, 4 },
  { 0x0, 0x0, 177, -1, 2327, 0, 0, -1 },
  { 0x9, 0x9, 177, -1, 2335, 33, 1, 50 },
  { 0x9, 0x9, 177, -1, 2993, 33, 1, 50 },
  { 0x0, 0x0, 177, -1, 2380, 0, 0, -1 },
  { 0x7, 0x7, 177, -1, -1, 27, 1, 50 },
  { 0x1, 0x1, 197, -1, -1, 27, 1, 10 },
  { 0x1, 0x1, 211, -1, -1, 29, 1, 0 },
  { 0x1, 0x1, 211, -1, -1, 29, 1, 0 },
  { 0x2, 0x3, 211, 1169, -1, 27, 1, 34 },
  { 0x0, 0x0, 211, 1170, -1, 0, 1, 34 },
  { 0x0, 0x0, 211, 1171, -1, 0, 1, 0 },
  { 0x0, 0x0, 211, 1172, -1, 0, 1, 0 },
  { 0x0, 0x0, 211, 1173, -1, 0, 1, 0 },
  { 0x0, 0x0, 211, 1174, -1, 0, 1, 0 },
  { 0x0, 0x0, 211, 3026, -1, 0, 1, 100 },
  { 0x0, 0x0, 211, 3027, -1, 0, 1, 100 },
  { 0x0, 0x0, 211, 3028, 967, 0, 0, -1 },
  { 0x1, 0x1, 212, -1, -1, 27, 1, 0 },
  { 0x1, 0x1, 212, -1, -1, 27, 1, 0 },
  { 0x1, 0x1, 213, -1, 1426, 32, 1, 142 },
  { 0x1, 0x1, 213, -1, 1428, 32, 1, 142 },
  { 0x1, 0x1, 213, -1, 1430, 32, 1, 141 },
  { 0x1, 0x1, 213, -1, 1432, 32, 1, 141 },
  { 0x1, 0x1, 213, -1, 1434, 32, 1, 141 },
  { 0x1, 0x1, 213, -1, 1436, 32, 1, 141 },
  { 0x1, 0x1, 213, -1, 1438, 32, 1, 141 },
  { 0x1, 0x1, 213, -1, 1440, 32, 1, 141 },
  { 0x1, 0x1, 213, -1, 1442, 32, 1, 141 },
  { 0x1, 0x1, 213, -1, 1444, 32, 1, 141 },
  { 0x1, 0x1, 213, -1, 1446, 32, 1, 143 },
  { 0x1, 0x1, 213, -1, 1448, 32, 1, 143 },
  { 0x1, 0x1, 213, -1, 1965, 32, 1, 138 },
  { 0x1, 0x1, 213, -1, 1967, 32, 1, 145 },
  { 0x1, 0x1, 213, -1, 1969, 32, 1, 139 },
  { 0x1, 0x1, 213, -1, 1971, 32, 1, 139 },
  { 0x1, 0x1, 213, -1, 1973, 32, 1, 138 },
  { 0x1, 0x1, 213, -1, 1975, 32, 1, 145 },
  { 0x1, 0x1, 213, -1, 1977, 32, 1, 138 },
  { 0x1, 0x1, 213, -1, 1979, 32, 1, 145 },
  { 0x1, 0x1, 213, 2783, 1981, 32, 1, 138 },
  { 0x1, 0x1, 213, 2784, 1984, 32, 1, 145 },
  { 0x0, 0x0, 214, -1, 2825, 0, 0, -1 },
  { 0x0, 0x0, 214, -1, 2826, 0, 0, -1 },
  { 0x0, 0x0, 214, -1, 2851, 0, 0, -1 },
  { 0x5, 0x5, 214, -1, 2854, 20, 1, 68 },
  { 0x0, 0x0, 218, 2209, 966, 0, 0, -1 },
  { 0x0, 0x0, 219, -1, 1139, 0, 0, -1 },
  { 0x0, 0x0, 219, -1, 1264, 0, 0, -1 },
  { 0x0, 0x0, 219, -1, -1, 0, 1, 128 },
  { 0x0, 0x0, 219, -1, -1, 0, 1, 67 },
  { 0x1, 0x1, 219, 833, 2289, 36, 1, 66 },
  { 0x1, 0x1, 219, 834, 2348, 36, 1, 66 },
  { 0x0, 0x0, 219, 835, 2351, 0, 0, -1 },
  { 0x1, 0x1, 219, 836, -1, 36, 1, 66 },
  { 0x0, 0x0, 219, 1423, -1, 0, 1, 34 },
  { 0x1, 0x1, 219, 837, 2356, 36, 1, 66 },
  { 0x0, 0x0, 219, 838, 2359, 0, 0, -1 },
  { 0x1, 0x1, 219, 839, -1, 36, 1, 66 },
  { 0x0, 0x0, 219, 840, 2362, 0, 0, -1 },
  { 0x1, 0x1, 219, 841, -1, 36, 1, 66 },
  { 0x1, 0x1, 219, 842, 2365, 36, 1, 66 },
  { 0x1, 0x1, 219, 843, 2368, 36, 1, 66 },
  { 0x0, 0x0, 219, 1424, -1, 0, 1, 34 },
  { 0x1, 0x1, 219, 844, 2401, 36, 1, 66 },
  { 0x1, 0x1, 219, 845, -1, 31, 1, 144 },
  { 0x1, 0x1, 219, 228, 1449, 32, 1, 133 },
  { 0x1, 0x1, 219, 229, 1458, 32, 1, 133 },
  { 0x1, 0x1, 219, 230, 1467, 32, 1, 133 },
  { 0x1, 0x1, 219, 231, 1480, 32, 1, 133 },
  { 0x1, 0x1, 219, 232, 1489, 32, 1, 133 },
  { 0x1, 0x1, 219, 233, 1498, 32, 1, 133 },
  { 0x1, 0x1, 219, 234, 1507, 32, 1, 133 },
  { 0x1, 0x1, 219, 235, 1516, 32, 1, 133 },
  { 0x1, 0x1, 219, 236, 1525, 32, 1, 133 },
  { 0x1, 0x1, 219, 237, 1534, 32, 1, 133 },
  { 0x1, 0x1, 219, 238, 1544, 32, 1, 133 },
  { 0x1, 0x1, 219, 239, 1554, 32, 1, 133 },
  { 0x1, 0x1, 219, 240, 1567, 32, 1, 148 },
  { 0x1, 0x1, 219, 241, 1573, 32, 1, 153 },
  { 0x1, 0x1, 219, 242, 1579, 32, 1, 153 },
  { 0x1, 0x1, 219, 243, 1585, 32, 1, 148 },
  { 0x1, 0x1, 219, 244, 1591, 32, 1, 153 },
  { 0x1, 0x1, 219, 245, 1597, 32, 1, 153 },
  { 0x1, 0x1, 219, 246, 1603, 32, 1, 148 },
  { 0x1, 0x1, 219, 247, 1609, 32, 1, 153 },
  { 0x1, 0x1, 219, 248, 1615, 32, 1, 153 },
  { 0x1, 0x1, 219, 249, 1621, 32, 1, 148 },
  { 0x1, 0x1, 219, 250, 1627, 32, 1, 153 },
  { 0x1, 0x1, 219, 251, 1633, 32, 1, 148 },
  { 0x1, 0x1, 219, 252, 1639, 32, 1, 153 },
  { 0x1, 0x1, 219, 253, 1645, 32, 1, 148 },
  { 0x1, 0x1, 219, 254, 1651, 32, 1, 153 },
  { 0x1, 0x1, 219, 255, 1657, 32, 1, 148 },
  { 0x1, 0x1, 219, 256, 1663, 32, 1, 153 },
  { 0x1, 0x1, 219, 257, 1669, 32, 1, 153 },
  { 0x1, 0x1, 219, 849, -1, 31, 1, 160 },
  { 0x0, 0x0, 220, 2404, -1, 0, 1, 66 },
  { 0x0, 0x0, 220, 2405, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 25, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2407, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2408, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2409, -1, 0, 1, 45 },
  { 0x0, 0x0, 220, 2410, -1, 0, 1, 40 },
  { 0x1, 0x1, 220, 2411, -1, 12, 1, 59 },
  { 0x0, 0x0, 220, 2412, -1, 0, 1, 54 },
  { 0x1000001, 0x1000001, 220, 2413, -1, 12, 1, 59 },
  { 0x1, 0x1, 220, 2414, -1, 36, 1, 54 },
  { 0x200001, 0x200001, 220, 2415, -1, 12, 1, 59 },
  { 0x1, 0x1, 220, 2416, -1, 33, 1, 54 },
  { 0x1200001, 0x1200001, 220, 2417, -1, 12, 1, 49 },
  { 0x9, 0x9, 220, 2418, -1, 33, 1, 49 },
  { 0x0, 0x0, 220, 2419, -1, 0, 1, 59 },
  { 0x0, 0x0, 220, 2420, -1, 0, 1, 54 },
  { 0x0, 0x0, 220, 2421, -1, 0, 1, 59 },
  { 0x0, 0x0, 220, 2422, -1, 0, 1, 54 },
  { 0x0, 0x0, 220, 2423, -1, 0, 1, 59 },
  { 0x0, 0x0, 220, 2424, -1, 0, 1, 54 },
  { 0x0, 0x0, 220, 2425, -1, 0, 1, 49 },
  { 0x0, 0x0, 220, 2426, -1, 0, 1, 49 },
  { 0x1, 0x1, 220, 2427, -1, 12, 1, 59 },
  { 0x0, 0x0, 220, 2428, -1, 0, 1, 54 },
  { 0x200001, 0x1200001, 220, 2429, -1, 12, 1, 59 },
  { 0x1, 0x9, 220, 2430, -1, 33, 1, 54 },
  { 0x0, 0x0, 220, 2431, -1, 0, 1, 59 },
  { 0x0, 0x0, 220, 2432, -1, 0, 1, 54 },
  { 0x0, 0x0, 220, 2433, -1, 0, 1, 59 },
  { 0x0, 0x0, 220, 2434, -1, 0, 1, 54 },
  { 0x1, 0x1, 220, 2435, -1, 12, 1, 59 },
  { 0x0, 0x0, 220, 2436, -1, 0, 1, 54 },
  { 0x1000001, 0x1000001, 220, 2437, -1, 12, 1, 59 },
  { 0x1, 0x1, 220, 2438, -1, 36, 1, 54 },
  { 0x200001, 0x200001, 220, 2439, -1, 12, 1, 59 },
  { 0x1, 0x1, 220, 2440, -1, 33, 1, 54 },
  { 0x1200001, 0x1200001, 220, 2441, -1, 12, 1, 49 },
  { 0x9, 0x9, 220, 2442, -1, 33, 1, 49 },
  { 0x0, 0x0, 220, 2443, -1, 0, 1, 59 },
  { 0x0, 0x0, 220, 2444, -1, 0, 1, 54 },
  { 0x0, 0x0, 220, 2445, -1, 0, 1, 59 },
  { 0x0, 0x0, 220, 2446, -1, 0, 1, 54 },
  { 0x0, 0x0, 220, 2447, -1, 0, 1, 59 },
  { 0x0, 0x0, 220, 2448, -1, 0, 1, 54 },
  { 0x0, 0x0, 220, 2449, -1, 0, 1, 49 },
  { 0x0, 0x0, 220, 2450, -1, 0, 1, 49 },
  { 0x1, 0x1, 220, 2451, -1, 12, 1, 59 },
  { 0x0, 0x0, 220, 2452, -1, 0, 1, 54 },
  { 0x200001, 0x1200001, 220, 2453, -1, 12, 1, 59 },
  { 0x1, 0x9, 220, 2454, -1, 33, 1, 54 },
  { 0x0, 0x0, 220, 2455, -1, 0, 1, 59 },
  { 0x0, 0x0, 220, 2456, -1, 0, 1, 54 },
  { 0x0, 0x0, 220, 2457, -1, 0, 1, 59 },
  { 0x0, 0x0, 220, 2458, -1, 0, 1, 54 },
  { 0x1, 0x1, 220, 2459, -1, 28, 1, 29 },
  { 0x0, 0x0, 220, 2460, -1, 0, 1, 29 },
  { 0x3, 0x3, 220, 2461, -1, 27, 1, 29 },
  { 0x1, 0x1, 220, 2462, -1, 27, 1, 29 },
  { 0x0, 0x0, 220, 2463, -1, 0, 1, 66 },
  { 0x0, 0x0, 220, 2464, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2465, -1, 0, 1, 29 },
  { 0x1, 0x1, 220, 2466, -1, 36, 1, 66 },
  { 0x1, 0x1, 220, 2467, -1, 37, 1, 29 },
  { 0x0, 0x0, 220, 2468, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2469, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2470, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2471, -1, 0, 1, 66 },
  { 0x0, 0x0, 220, 2472, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 37, -1, 0, 1, 29 },
  { 0x1, 0x1, 220, 2474, -1, 36, 1, 66 },
  { 0x1, 0x1, 220, 2475, -1, 37, 1, 29 },
  { 0x0, 0x0, 220, 2476, -1, 0, 1, 29 },
  { 0x1, 0x1, 220, 2477, -1, 36, 1, 66 },
  { 0x1, 0x1, 220, 2478, -1, 37, 1, 29 },
  { 0x0, 0x0, 220, 2479, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2480, -1, 0, 1, 66 },
  { 0x0, 0x0, 220, 2481, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 42, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2483, -1, 0, 1, 66 },
  { 0x0, 0x0, 220, 2484, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 43, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2486, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2487, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2488, -1, 0, 1, 49 },
  { 0x1, 0x1, 220, 2489, -1, 27, 1, 49 },
  { 0x1, 0x1, 220, 2490, -1, 28, 1, 49 },
  { 0x3, 0x3, 220, 2491, -1, 27, 1, 49 },
  { 0x1, 0x1, 220, 2492, -1, 29, 1, 49 },
  { 0x5, 0x5, 220, 2493, -1, 27, 1, 49 },
  { 0x3, 0x3, 220, 2494, -1, 28, 1, 49 },
  { 0x7, 0x7, 220, 2495, -1, 27, 1, 49 },
  { 0x0, 0x0, 220, 2496, -1, 0, 1, 49 },
  { 0x0, 0x0, 220, 2497, -1, 0, 1, 49 },
  { 0x0, 0x0, 220, 2498, -1, 0, 1, 49 },
  { 0x0, 0x0, 220, 2499, -1, 0, 1, 49 },
  { 0x1, 0x1, 220, 2500, -1, 28, 1, 29 },
  { 0x0, 0x0, 220, 2501, -1, 0, 1, 29 },
  { 0x3, 0x3, 220, 2502, -1, 27, 1, 29 },
  { 0x1, 0x1, 220, 2503, -1, 27, 1, 29 },
  { 0x0, 0x0, 220, 2504, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2505, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2506, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 52, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2508, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2509, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 57, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 2511, -1, 0, 1, 24 },
  { 0x0, 0x0, 220, 2512, -1, 0, 1, 24 },
  { 0x0, 0x0, 220, 2513, -1, 0, 1, 24 },
  { 0x0, 0x0, 220, 2514, -1, 0, 1, 24 },
  { 0x0, 0x0, 220, 2515, -1, 0, 1, 35 },
  { 0x0, 0x0, 220, 2516, -1, 0, 1, 66 },
  { 0x0, 0x0, 220, 2517, -1, 0, 1, 29 },
  { 0x0, 0x0, 220, 64, -1, 0, 1, 29 },
  { 0x1, 0x1, 221, 2519, -1, 34, 1, 66 },
  { 0x1, 0x1, 221, 2520, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2521, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2522, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2523, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2524, -1, 34, 1, 46 },
  { 0x1, 0x1, 221, 2525, -1, 34, 1, 42 },
  { 0x400001, 0x400001, 221, 2526, -1, 12, 1, 61 },
  { 0x1, 0x1, 221, 2527, -1, 34, 1, 56 },
  { 0x1400001, 0x1400001, 221, 2528, -1, 12, 1, 61 },
  { 0x5, 0x5, 221, 2529, -1, 34, 1, 56 },
  { 0x600001, 0x600001, 221, 2530, -1, 12, 1, 61 },
  { 0x3, 0x3, 221, 2531, -1, 33, 1, 56 },
  { 0x1600001, 0x1600001, 221, 2532, -1, 12, 1, 51 },
  { 0xb, 0xb, 221, 2533, -1, 33, 1, 51 },
  { 0x1, 0x1, 221, 2534, -1, 34, 1, 61 },
  { 0x1, 0x1, 221, 2535, -1, 34, 1, 56 },
  { 0x1, 0x1, 221, 2536, -1, 34, 1, 61 },
  { 0x1, 0x1, 221, 2537, -1, 34, 1, 56 },
  { 0x1, 0x1, 221, 2538, -1, 34, 1, 61 },
  { 0x1, 0x1, 221, 2539, -1, 34, 1, 56 },
  { 0x1, 0x1, 221, 2540, -1, 34, 1, 51 },
  { 0x1, 0x1, 221, 2541, -1, 34, 1, 51 },
  { 0x400001, 0x400001, 221, 2542, -1, 12, 1, 61 },
  { 0x1, 0x1, 221, 2543, -1, 34, 1, 56 },
  { 0x600001, 0x1600001, 221, 2544, -1, 12, 1, 61 },
  { 0x3, 0xb, 221, 2545, -1, 33, 1, 56 },
  { 0x1, 0x1, 221, 2546, -1, 34, 1, 61 },
  { 0x1, 0x1, 221, 2547, -1, 34, 1, 56 },
  { 0x1, 0x1, 221, 2548, -1, 34, 1, 61 },
  { 0x1, 0x1, 221, 2549, -1, 34, 1, 56 },
  { 0x400001, 0x400001, 221, 2550, -1, 12, 1, 61 },
  { 0x1, 0x1, 221, 2551, -1, 34, 1, 56 },
  { 0x1400001, 0x1400001, 221, 2552, -1, 12, 1, 61 },
  { 0x5, 0x5, 221, 2553, -1, 34, 1, 56 },
  { 0x600001, 0x600001, 221, 2554, -1, 12, 1, 61 },
  { 0x3, 0x3, 221, 2555, -1, 33, 1, 56 },
  { 0x1600001, 0x1600001, 221, 2556, -1, 12, 1, 51 },
  { 0xb, 0xb, 221, 2557, -1, 33, 1, 51 },
  { 0x1, 0x1, 221, 2558, -1, 34, 1, 61 },
  { 0x1, 0x1, 221, 2559, -1, 34, 1, 56 },
  { 0x1, 0x1, 221, 2560, -1, 34, 1, 61 },
  { 0x1, 0x1, 221, 2561, -1, 34, 1, 56 },
  { 0x1, 0x1, 221, 2562, -1, 34, 1, 61 },
  { 0x1, 0x1, 221, 2563, -1, 34, 1, 56 },
  { 0x1, 0x1, 221, 2564, -1, 34, 1, 51 },
  { 0x1, 0x1, 221, 2565, -1, 34, 1, 51 },
  { 0x400001, 0x400001, 221, 2566, -1, 12, 1, 61 },
  { 0x1, 0x1, 221, 2567, -1, 34, 1, 56 },
  { 0x600001, 0x1600001, 221, 2568, -1, 12, 1, 61 },
  { 0x3, 0xb, 221, 2569, -1, 33, 1, 56 },
  { 0x1, 0x1, 221, 2570, -1, 34, 1, 61 },
  { 0x1, 0x1, 221, 2571, -1, 34, 1, 56 },
  { 0x1, 0x1, 221, 2572, -1, 34, 1, 61 },
  { 0x1, 0x1, 221, 2573, -1, 34, 1, 56 },
  { 0x41, 0x41, 221, 2574, -1, 28, 1, 31 },
  { 0x1, 0x1, 221, 2575, -1, 34, 1, 31 },
  { 0x83, 0x83, 221, 2576, -1, 27, 1, 31 },
  { 0x81, 0x81, 221, 2577, -1, 27, 1, 31 },
  { 0x1, 0x1, 221, 2578, -1, 34, 1, 66 },
  { 0x1, 0x1, 221, 2579, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2580, -1, 34, 1, 31 },
  { 0x5, 0x5, 221, 2581, -1, 34, 1, 66 },
  { 0x9, 0x9, 221, 2582, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2583, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2584, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2585, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2586, -1, 34, 1, 66 },
  { 0x1, 0x1, 221, 2587, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2588, -1, 34, 1, 31 },
  { 0x5, 0x5, 221, 2589, -1, 34, 1, 66 },
  { 0x9, 0x9, 221, 2590, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2591, -1, 34, 1, 31 },
  { 0x5, 0x5, 221, 2592, -1, 34, 1, 66 },
  { 0x9, 0x9, 221, 2593, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2594, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2595, -1, 34, 1, 66 },
  { 0x1, 0x1, 221, 2596, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2597, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2598, -1, 34, 1, 66 },
  { 0x1, 0x1, 221, 2599, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2600, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2601, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2602, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2603, -1, 34, 1, 51 },
  { 0x81, 0x81, 221, 2604, -1, 27, 1, 51 },
  { 0x41, 0x41, 221, 2605, -1, 28, 1, 51 },
  { 0x83, 0x83, 221, 2606, -1, 27, 1, 51 },
  { 0x21, 0x21, 221, 2607, -1, 29, 1, 51 },
  { 0x85, 0x85, 221, 2608, -1, 27, 1, 51 },
  { 0x43, 0x43, 221, 2609, -1, 28, 1, 51 },
  { 0x87, 0x87, 221, 2610, -1, 27, 1, 51 },
  { 0x1, 0x1, 221, 2611, -1, 34, 1, 51 },
  { 0x1, 0x1, 221, 2612, -1, 34, 1, 51 },
  { 0x1, 0x1, 221, 2613, -1, 34, 1, 51 },
  { 0x1, 0x1, 221, 2614, -1, 34, 1, 51 },
  { 0x41, 0x41, 221, 2615, -1, 28, 1, 31 },
  { 0x1, 0x1, 221, 2616, -1, 34, 1, 31 },
  { 0x83, 0x83, 221, 2617, -1, 27, 1, 31 },
  { 0x81, 0x81, 221, 2618, -1, 27, 1, 31 },
  { 0x1, 0x1, 221, 2619, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2620, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2621, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2622, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2623, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2624, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2625, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2626, -1, 34, 1, 26 },
  { 0x1, 0x1, 221, 2627, -1, 34, 1, 26 },
  { 0x1, 0x1, 221, 2628, -1, 34, 1, 26 },
  { 0x1, 0x1, 221, 2629, -1, 34, 1, 26 },
  { 0x1, 0x1, 221, 2630, -1, 34, 1, 37 },
  { 0x1, 0x1, 221, 2631, -1, 34, 1, 66 },
  { 0x1, 0x1, 221, 2632, -1, 34, 1, 31 },
  { 0x1, 0x1, 221, 2633, -1, 34, 1, 31 },
  { 0x1, 0x1, 222, 2634, -1, 35, 1, 66 },
  { 0x1, 0x1, 222, 2635, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2636, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2637, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2638, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2639, -1, 35, 1, 47 },
  { 0x1, 0x1, 222, 2640, -1, 35, 1, 43 },
  { 0x800001, 0x800001, 222, 2641, -1, 12, 1, 62 },
  { 0x1, 0x1, 222, 2642, -1, 35, 1, 57 },
  { 0x1800001, 0x1800001, 222, 2643, -1, 12, 1, 62 },
  { 0x3, 0x3, 222, 2644, -1, 35, 1, 57 },
  { 0xa00001, 0xa00001, 222, 2645, -1, 12, 1, 62 },
  { 0x5, 0x5, 222, 2646, -1, 33, 1, 57 },
  { 0x1a00001, 0x1a00001, 222, 2647, -1, 12, 1, 52 },
  { 0xd, 0xd, 222, 2648, -1, 33, 1, 52 },
  { 0x1, 0x1, 222, 2649, -1, 35, 1, 62 },
  { 0x1, 0x1, 222, 2650, -1, 35, 1, 57 },
  { 0x1, 0x1, 222, 2651, -1, 35, 1, 62 },
  { 0x1, 0x1, 222, 2652, -1, 35, 1, 57 },
  { 0x1, 0x1, 222, 2653, -1, 35, 1, 62 },
  { 0x1, 0x1, 222, 2654, -1, 35, 1, 57 },
  { 0x1, 0x1, 222, 2655, -1, 35, 1, 52 },
  { 0x1, 0x1, 222, 2656, -1, 35, 1, 52 },
  { 0x800001, 0x800001, 222, 2657, -1, 12, 1, 62 },
  { 0x1, 0x1, 222, 2658, -1, 35, 1, 57 },
  { 0xa00001, 0x1a00001, 222, 2659, -1, 12, 1, 62 },
  { 0x5, 0xd, 222, 2660, -1, 33, 1, 57 },
  { 0x1, 0x1, 222, 2661, -1, 35, 1, 62 },
  { 0x1, 0x1, 222, 2662, -1, 35, 1, 57 },
  { 0x1, 0x1, 222, 2663, -1, 35, 1, 62 },
  { 0x1, 0x1, 222, 2664, -1, 35, 1, 57 },
  { 0x800001, 0x800001, 222, 2665, -1, 12, 1, 62 },
  { 0x1, 0x1, 222, 2666, -1, 35, 1, 57 },
  { 0x1800001, 0x1800001, 222, 2667, -1, 12, 1, 62 },
  { 0x3, 0x3, 222, 2668, -1, 35, 1, 57 },
  { 0xa00001, 0xa00001, 222, 2669, -1, 12, 1, 62 },
  { 0x5, 0x5, 222, 2670, -1, 33, 1, 57 },
  { 0x1a00001, 0x1a00001, 222, 2671, -1, 12, 1, 52 },
  { 0xd, 0xd, 222, 2672, -1, 33, 1, 52 },
  { 0x1, 0x1, 222, 2673, -1, 35, 1, 62 },
  { 0x1, 0x1, 222, 2674, -1, 35, 1, 57 },
  { 0x1, 0x1, 222, 2675, -1, 35, 1, 62 },
  { 0x1, 0x1, 222, 2676, -1, 35, 1, 57 },
  { 0x1, 0x1, 222, 2677, -1, 35, 1, 62 },
  { 0x1, 0x1, 222, 2678, -1, 35, 1, 57 },
  { 0x1, 0x1, 222, 2679, -1, 35, 1, 52 },
  { 0x1, 0x1, 222, 2680, -1, 35, 1, 52 },
  { 0x800001, 0x800001, 222, 2681, -1, 12, 1, 62 },
  { 0x1, 0x1, 222, 2682, -1, 35, 1, 57 },
  { 0xa00001, 0x1a00001, 222, 2683, -1, 12, 1, 62 },
  { 0x5, 0xd, 222, 2684, -1, 33, 1, 57 },
  { 0x1, 0x1, 222, 2685, -1, 35, 1, 62 },
  { 0x1, 0x1, 222, 2686, -1, 35, 1, 57 },
  { 0x1, 0x1, 222, 2687, -1, 35, 1, 62 },
  { 0x1, 0x1, 222, 2688, -1, 35, 1, 57 },
  { 0x81, 0x81, 222, 2689, -1, 28, 1, 32 },
  { 0x1, 0x1, 222, 2690, -1, 35, 1, 32 },
  { 0x103, 0x103, 222, 2691, -1, 27, 1, 32 },
  { 0x101, 0x101, 222, 2692, -1, 27, 1, 32 },
  { 0x1, 0x1, 222, 2693, -1, 35, 1, 66 },
  { 0x1, 0x1, 222, 2694, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2695, -1, 35, 1, 32 },
  { 0x3, 0x3, 222, 2696, -1, 35, 1, 66 },
  { 0x5, 0x5, 222, 2697, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2698, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2699, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2700, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2701, -1, 35, 1, 66 },
  { 0x1, 0x1, 222, 2702, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2703, -1, 35, 1, 32 },
  { 0x3, 0x3, 222, 2704, -1, 35, 1, 66 },
  { 0x5, 0x5, 222, 2705, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2706, -1, 35, 1, 32 },
  { 0x3, 0x3, 222, 2707, -1, 35, 1, 66 },
  { 0x5, 0x5, 222, 2708, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2709, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2710, -1, 35, 1, 66 },
  { 0x1, 0x1, 222, 2711, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2712, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2713, -1, 35, 1, 66 },
  { 0x1, 0x1, 222, 2714, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2715, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2716, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2717, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2718, -1, 35, 1, 52 },
  { 0x101, 0x101, 222, 2719, -1, 27, 1, 52 },
  { 0x81, 0x81, 222, 2720, -1, 28, 1, 52 },
  { 0x103, 0x103, 222, 2721, -1, 27, 1, 52 },
  { 0x41, 0x41, 222, 2722, -1, 29, 1, 52 },
  { 0x105, 0x105, 222, 2723, -1, 27, 1, 52 },
  { 0x83, 0x83, 222, 2724, -1, 28, 1, 52 },
  { 0x107, 0x107, 222, 2725, -1, 27, 1, 52 },
  { 0x1, 0x1, 222, 2726, -1, 35, 1, 52 },
  { 0x1, 0x1, 222, 2727, -1, 35, 1, 52 },
  { 0x1, 0x1, 222, 2728, -1, 35, 1, 52 },
  { 0x1, 0x1, 222, 2729, -1, 35, 1, 52 },
  { 0x81, 0x81, 222, 2730, -1, 28, 1, 32 },
  { 0x1, 0x1, 222, 2731, -1, 35, 1, 32 },
  { 0x103, 0x103, 222, 2732, -1, 27, 1, 32 },
  { 0x101, 0x101, 222, 2733, -1, 27, 1, 32 },
  { 0x1, 0x1, 222, 2734, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2735, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2736, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2737, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2738, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2739, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2740, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2741, -1, 35, 1, 27 },
  { 0x1, 0x1, 222, 2742, -1, 35, 1, 27 },
  { 0x1, 0x1, 222, 2743, -1, 35, 1, 27 },
  { 0x1, 0x1, 222, 2744, -1, 35, 1, 27 },
  { 0x1, 0x1, 222, 2745, -1, 35, 1, 38 },
  { 0x1, 0x1, 222, 2746, -1, 35, 1, 66 },
  { 0x1, 0x1, 222, 2747, -1, 35, 1, 32 },
  { 0x1, 0x1, 222, 2748, -1, 35, 1, 32 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 66 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, 2243, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 48 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 44 },
  { 0xc00001, 0xc00001, 223, -1, -1, 12, 1, 63 },
  { 0x3, 0x3, 223, 2964, -1, 34, 1, 58 },
  { 0x1c00001, 0x1c00001, 223, -1, -1, 12, 1, 63 },
  { 0x7, 0x7, 223, 2965, -1, 34, 1, 58 },
  { 0xe00001, 0xe00001, 223, -1, -1, 12, 1, 63 },
  { 0x7, 0x7, 223, 2966, -1, 33, 1, 58 },
  { 0x1e00001, 0x1e00001, 223, -1, -1, 12, 1, 53 },
  { 0xf, 0xf, 223, 2967, -1, 33, 1, 53 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 63 },
  { 0x3, 0x3, 223, 2968, -1, 34, 1, 58 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 63 },
  { 0x3, 0x3, 223, 2969, -1, 34, 1, 58 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 63 },
  { 0x3, 0x3, 223, 2970, -1, 34, 1, 58 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 53 },
  { 0x3, 0x3, 223, 2971, -1, 34, 1, 53 },
  { 0xc00001, 0xc00001, 223, -1, -1, 12, 1, 63 },
  { 0x3, 0x3, 223, 2976, -1, 34, 1, 58 },
  { 0xe00001, 0x1e00001, 223, -1, -1, 12, 1, 63 },
  { 0x7, 0xf, 223, 2977, -1, 33, 1, 58 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 63 },
  { 0x3, 0x3, 223, 2978, -1, 34, 1, 58 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 63 },
  { 0x3, 0x3, 223, 2979, -1, 34, 1, 58 },
  { 0xc00001, 0xc00001, 223, -1, -1, 12, 1, 63 },
  { 0x3, 0x3, 223, 2982, -1, 34, 1, 58 },
  { 0x1c00001, 0x1c00001, 223, -1, -1, 12, 1, 63 },
  { 0x7, 0x7, 223, 2983, -1, 34, 1, 58 },
  { 0xe00001, 0xe00001, 223, -1, -1, 12, 1, 63 },
  { 0x7, 0x7, 223, 2984, -1, 33, 1, 58 },
  { 0x1e00001, 0x1e00001, 223, -1, -1, 12, 1, 53 },
  { 0xf, 0xf, 223, 2985, -1, 33, 1, 53 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 63 },
  { 0x3, 0x3, 223, 2986, -1, 34, 1, 58 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 63 },
  { 0x3, 0x3, 223, 2987, -1, 34, 1, 58 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 63 },
  { 0x3, 0x3, 223, 2988, -1, 34, 1, 58 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 53 },
  { 0x3, 0x3, 223, 2989, -1, 34, 1, 53 },
  { 0xc00001, 0xc00001, 223, -1, -1, 12, 1, 63 },
  { 0x3, 0x3, 223, 2994, -1, 34, 1, 58 },
  { 0xe00001, 0x1e00001, 223, -1, -1, 12, 1, 63 },
  { 0x7, 0xf, 223, 2995, -1, 33, 1, 58 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 63 },
  { 0x3, 0x3, 223, 2996, -1, 34, 1, 58 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 63 },
  { 0x3, 0x3, 223, 2997, -1, 34, 1, 58 },
  { 0xc1, 0xc1, 223, -1, -1, 28, 1, 33 },
  { 0x3, 0x3, 223, 2862, -1, 34, 1, 33 },
  { 0x183, 0x183, 223, -1, -1, 27, 1, 33 },
  { 0x181, 0x181, 223, 2863, -1, 27, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 66 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, 2244, -1, 34, 1, 33 },
  { 0x7, 0x7, 223, -1, -1, 34, 1, 66 },
  { 0xb, 0xb, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, 2245, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 66 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, 2248, -1, 34, 1, 33 },
  { 0x7, 0x7, 223, -1, -1, 34, 1, 66 },
  { 0xb, 0xb, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, 2249, -1, 34, 1, 33 },
  { 0x7, 0x7, 223, -1, -1, 34, 1, 66 },
  { 0xb, 0xb, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, 2251, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 66 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, 2253, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 66 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, 2254, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 53 },
  { 0x181, 0x181, 223, -1, -1, 27, 1, 53 },
  { 0xc1, 0xc1, 223, -1, -1, 28, 1, 53 },
  { 0x183, 0x183, 223, -1, -1, 27, 1, 53 },
  { 0x61, 0x61, 223, -1, -1, 29, 1, 53 },
  { 0x185, 0x185, 223, -1, -1, 27, 1, 53 },
  { 0xc3, 0xc3, 223, -1, -1, 28, 1, 53 },
  { 0x187, 0x187, 223, -1, -1, 27, 1, 53 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 53 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 53 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 53 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 53 },
  { 0xc1, 0xc1, 223, -1, -1, 28, 1, 33 },
  { 0x3, 0x3, 223, 2866, -1, 34, 1, 33 },
  { 0x183, 0x183, 223, -1, -1, 27, 1, 33 },
  { 0x181, 0x181, 223, 2867, -1, 27, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 28 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 28 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 28 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 28 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 39 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 66 },
  { 0x3, 0x3, 223, -1, -1, 34, 1, 33 },
  { 0x3, 0x3, 223, 2256, -1, 34, 1, 33 },
  { 0x3, 0x3, 224, 540, 1451, 32, 1, 135 },
  { 0x3, 0x3, 224, 541, 1460, 32, 1, 135 },
  { 0x3, 0x3, 224, 542, 1469, 32, 1, 135 },
  { 0x3, 0x3, 224, 543, 1482, 32, 1, 135 },
  { 0x3, 0x3, 224, 544, 1491, 32, 1, 135 },
  { 0x3, 0x3, 224, 545, 1500, 32, 1, 135 },
  { 0x3, 0x3, 224, 546, 1509, 32, 1, 135 },
  { 0x3, 0x3, 224, 547, 1518, 32, 1, 135 },
  { 0x3, 0x3, 224, 548, 1527, 32, 1, 135 },
  { 0x3, 0x3, 224, 549, 1536, 32, 1, 135 },
  { 0x3, 0x3, 224, 550, 1546, 32, 1, 135 },
  { 0x3, 0x3, 224, 551, 1556, 32, 1, 135 },
  { 0x3, 0x3, 224, 564, 1569, 32, 1, 150 },
  { 0x3, 0x3, 224, 565, 1575, 32, 1, 155 },
  { 0x3, 0x3, 224, 566, 1581, 32, 1, 155 },
  { 0x3, 0x3, 224, 567, 1587, 32, 1, 150 },
  { 0x3, 0x3, 224, 568, 1593, 32, 1, 155 },
  { 0x3, 0x3, 224, 569, 1599, 32, 1, 155 },
  { 0x3, 0x3, 224, 570, 1605, 32, 1, 150 },
  { 0x3, 0x3, 224, 571, 1611, 32, 1, 155 },
  { 0x3, 0x3, 224, 572, 1617, 32, 1, 155 },
  { 0x3, 0x3, 224, 573, 1623, 32, 1, 150 },
  { 0x3, 0x3, 224, 574, 1629, 32, 1, 155 },
  { 0x3, 0x3, 224, 575, 1635, 32, 1, 150 },
  { 0x3, 0x3, 224, 576, 1641, 32, 1, 155 },
  { 0x3, 0x3, 224, 577, 1647, 32, 1, 150 },
  { 0x3, 0x3, 224, 578, 1653, 32, 1, 155 },
  { 0x3, 0x3, 224, 579, 1659, 32, 1, 150 },
  { 0x3, 0x3, 224, 580, 1665, 32, 1, 155 },
  { 0x3, 0x3, 224, 581, 1671, 32, 1, 155 },
  { 0x1, 0x1, 225, -1, -1, 28, 1, 34 },
  { 0x1, 0x1, 225, -1, -1, 28, 1, 34 },
  { 0x0, 0x0, 232, 958, -1, 0, 1, 144 },
  { 0x0, 0x0, 232, 959, -1, 0, 1, 160 },
  { 0x1, 0x1, 233, -1, 1982, 33, 1, 140 },
  { 0x1, 0x1, 233, -1, 1985, 33, 1, 146 },
  { 0x0, 0x0, 233, -1, 1987, 0, 1, 157 },
  { 0x0, 0x0, 233, -1, 1988, 0, 1, 161 },
  { 0x0, 0x0, 234, 883, 971, 0, 0, -1 },
  { 0x0, 0x0, 234, 884, 979, 0, 0, -1 },
  { 0x0, 0x0, 234, 885, 975, 0, 0, -1 },
  { 0x1, 0x1, 234, 886, 620, 33, 1, 6 },
  { 0x8000001, 0x8000001, 234, 887, 628, 6, 1, 7 },
  { 0x1, 0x1, 234, 888, 624, 33, 1, 6 },
  { 0x0, 0x0, 234, 889, 983, 0, 0, -1 },
  { 0x1, 0x1, 234, 890, 640, 33, 1, 8 },
  { 0x0, 0x0, 234, 891, 987, 0, 0, -1 },
  { 0x1, 0x1, 234, 892, 652, 33, 1, 16 },
  { 0x0, 0x0, 234, 893, 992, 0, 0, -1 },
  { 0x0, 0x0, 234, 894, 996, 0, 0, -1 },
  { 0x1, 0x1, 234, 895, 675, 33, 1, 18 },
  { 0x1, 0x1, 234, 896, 679, 33, 1, 18 },
  { 0x0, 0x0, 234, 897, 1000, 0, 0, -1 },
  { 0x0, 0x0, 234, 898, 1004, 0, 0, -1 },
  { 0x1, 0x1, 234, 899, 699, 33, 1, 19 },
  { 0x8000001, 0x8000001, 234, 900, 703, 6, 1, 19 },
  { 0x0, 0x0, 234, 901, 1008, 0, 0, -1 },
  { 0x1, 0x1, 234, 902, 715, 33, 1, 20 },
  { 0x0, 0x0, 234, 903, 1012, 0, 0, -1 },
  { 0x0, 0x0, 234, 904, 1016, 0, 0, -1 },
  { 0x1, 0x1, 234, 905, 735, 33, 1, 21 },
  { 0x8000001, 0x8000001, 234, 906, 739, 6, 1, 21 },
  { 0x0, 0x0, 234, 907, 1020, 0, 0, -1 },
  { 0x1, 0x1, 234, 908, 751, 33, 1, 22 },
  { 0x0, 0x0, 234, 909, 1025, 0, 0, -1 },
  { 0x0, 0x0, 234, 910, 1029, 0, 0, -1 },
  { 0x1, 0x1, 234, 911, 774, 33, 1, 18 },
  { 0x1, 0x1, 234, 912, 778, 33, 1, 18 },
  { 0x0, 0x0, 234, 913, 1033, 0, 0, -1 },
  { 0x1, 0x1, 234, 914, 790, 33, 1, 22 },
  { 0x0, 0x0, 235, 2787, 970, 0, 0, -1 },
  { 0x0, 0x0, 235, 2788, 978, 0, 0, -1 },
  { 0x0, 0x0, 235, 2789, 974, 0, 0, -1 },
  { 0x0, 0x0, 235, 2790, 619, 0, 1, 6 },
  { 0x1, 0x1, 235, 2791, 627, 6, 1, 7 },
  { 0x0, 0x0, 235, 2792, 623, 0, 1, 6 },
  { 0x0, 0x0, 235, 2793, 982, 0, 0, -1 },
  { 0x0, 0x0, 235, 2794, 639, 0, 1, 8 },
  { 0x0, 0x0, 235, 2795, 986, 0, 0, -1 },
  { 0x0, 0x0, 235, 2796, 651, 0, 1, 16 },
  { 0x0, 0x0, 235, 2797, 991, 0, 0, -1 },
  { 0x0, 0x0, 235, 2798, 995, 0, 0, -1 },
  { 0x0, 0x0, 235, 2799, 674, 0, 1, 18 },
  { 0x0, 0x0, 235, 2800, 678, 0, 1, 18 },
  { 0x0, 0x0, 235, 2801, 999, 0, 0, -1 },
  { 0x0, 0x0, 235, 2802, 1003, 0, 0, -1 },
  { 0x0, 0x0, 235, 2803, 698, 0, 1, 19 },
  { 0x1, 0x1, 235, 2804, 702, 6, 1, 19 },
  { 0x0, 0x0, 235, 2805, 1007, 0, 0, -1 },
  { 0x0, 0x0, 235, 2806, 714, 0, 1, 20 },
  { 0x0, 0x0, 235, 2807, 1011, 0, 0, -1 },
  { 0x0, 0x0, 235, 2808, 1015, 0, 0, -1 },
  { 0x0, 0x0, 235, 2809, 734, 0, 1, 21 },
  { 0x1, 0x1, 235, 2810, 738, 6, 1, 21 },
  { 0x0, 0x0, 235, 2811, 1019, 0, 0, -1 },
  { 0x0, 0x0, 235, 2812, 750, 0, 1, 22 },
  { 0x0, 0x0, 235, 2813, 1024, 0, 0, -1 },
  { 0x0, 0x0, 235, 2814, 1028, 0, 0, -1 },
  { 0x0, 0x0, 235, 2815, 773, 0, 1, 18 },
  { 0x0, 0x0, 235, 2816, 777, 0, 1, 18 },
  { 0x0, 0x0, 235, 2817, 1032, 0, 0, -1 },
  { 0x0, 0x0, 235, 2818, 789, 0, 1, 22 },
  { 0x1, 0x1, 235, 915, 1155, 27, 1, 17 },
  { 0x0, 0x0, 235, 916, 1153, 0, 1, 17 },
  { 0x0, 0x0, 235, 1220, 1157, 0, 1, 23 },
  { 0x0, 0x1, 235, 1165, 1163, 20, 1, 68 },
  { 0x0, 0x0, 235, 111, 1161, 0, 1, 68 },
  { 0x1, 0x1, 238, -1, -1, 29, 1, 0 },
  { 0x0, 0x0, 238, -1, -1, 0, 1, 0 },
  { 0x1, 0x1, 238, 3022, -1, 27, 1, 0 },
  { 0x1, 0x1, 238, 3023, -1, 27, 1, 0 },
  { 0x1, 0x1, 238, 3024, -1, 27, 1, 0 },
  { 0x1, 0x1, 238, 3025, -1, 27, 1, 0 },
  { 0x0, 0x0, 261, -1, 2344, 0, 0, -1 },
  { 0x0, 0x0, 261, -1, 2346, 0, 0, -1 },
  { 0x1, 0x1, 261, -1, -1, 28, 1, 30 },
  { 0x1, 0x1, 261, -1, -1, 28, 1, 30 },
  { 0x0, 0x0, 261, -1, 2385, 0, 0, -1 },
  { 0x0, 0x0, 261, -1, 2387, 0, 0, -1 },
  { 0x1, 0x1, 261, -1, -1, 28, 1, 30 },
  { 0x1, 0x1, 261, -1, -1, 28, 1, 30 },
  { 0x0, 0x0, 263, 23, -1, 0, 1, 0 },
  { 0x0, 0x0, 263, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 263, -1, -1, 0, 1, 0 },
  { 0x0, 0x1, 263, -1, -1, 29, 1, 0 },
  { 0x0, 0x1, 263, -1, -1, 29, 1, 0 },
  { 0x0, 0x1, 263, -1, -1, 29, 1, 0 },
  { 0x0, 0x1, 263, -1, -1, 29, 1, 0 },
  { 0x0, 0x1, 263, -1, -1, 29, 1, 0 },
  { 0x0, 0x0, 263, 180, -1, 0, 1, 0 },
  { 0x0, 0x1, 263, -1, -1, 29, 1, 0 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, 301, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, 323, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, 349, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, 371, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 65 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 65 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 65 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 65 },
  { 0x0, 0x0, 264, -1, 2296, 0, 0, -1 },
  { 0x0, 0x0, 264, -1, 2298, 0, 0, -1 },
  { 0x0, 0x0, 264, -1, 2300, 0, 0, -1 },
  { 0x0, 0x0, 264, -1, 2302, 0, 0, -1 },
  { 0x1, 0x1, 264, -1, 2304, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, 2306, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, 2308, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, 2310, 12, 1, 50 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 50 },
  { 0x0, 0x0, 264, -1, 2312, 0, 0, -1 },
  { 0x0, 0x0, 264, -1, 2314, 0, 0, -1 },
  { 0x1, 0x1, 264, -1, 2316, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, 2318, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 60 },
  { 0x0, 0x0, 264, -1, 2320, 0, 0, -1 },
  { 0x0, 0x0, 264, -1, 2322, 0, 0, -1 },
  { 0x0, 0x0, 264, -1, 2324, 0, 0, -1 },
  { 0x0, 0x0, 264, -1, 2326, 0, 0, -1 },
  { 0x1, 0x1, 264, -1, 2328, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, 2330, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, 2332, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, 2334, 12, 1, 50 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 50 },
  { 0x0, 0x0, 264, -1, 2336, 0, 0, -1 },
  { 0x0, 0x0, 264, -1, 2338, 0, 0, -1 },
  { 0x1, 0x1, 264, -1, 2340, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, 2342, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 60 },
  { 0x1, 0x1, 264, -1, -1, 12, 1, 60 },
  { 0x1, 0x1, 264, 393, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, 395, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, 517, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, 519, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, 401, -1, 12, 1, 77 },
  { 0x1, 0x1, 264, 403, -1, 12, 1, 77 },
  { 0x1, 0x1, 264, 525, -1, 12, 1, 77 },
  { 0x1, 0x1, 264, 527, -1, 12, 1, 77 },
  { 0x1, 0x1, 264, 409, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, 411, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, 533, -1, 12, 1, 2 },
  { 0x1, 0x1, 264, 535, -1, 12, 1, 2 },
  { 0x0, 0x0, 265, -1, 2303, 0, 0, -1 },
  { 0x9, 0x9, 265, -1, 2311, 33, 1, 50 },
  { 0x9, 0x9, 265, -1, 2975, 33, 1, 50 },
  { 0x0, 0x0, 265, 1399, 2376, 0, 0, -1 },
  { 0x3, 0x3, 265, 1400, -1, 27, 1, 50 },
  { 0x0, 0x0, 269, 2856, -1, 0, 1, 0 },
  { 0x3, 0x3, 270, -1, -1, 27, 1, 0 },
  { 0x3, 0x3, 270, -1, -1, 27, 1, 0 },
  { 0x3, 0x3, 270, -1, -1, 27, 1, 0 },
  { 0x3, 0x3, 270, -1, -1, 27, 1, 0 },
  { 0x1, 0x1, 271, 3018, -1, 28, 1, 0 },
  { 0x1, 0x1, 271, 3019, -1, 28, 1, 0 },
  { 0x1, 0x1, 271, 3020, -1, 28, 1, 0 },
  { 0x1, 0x1, 271, 3021, -1, 28, 1, 0 },
  { 0x1, 0x1, 273, -1, -1, 27, 1, 100 },
  { 0x1, 0x1, 273, -1, -1, 27, 1, 100 },
  { 0x0, 0x0, 273, -1, 968, 0, 0, -1 },
  { 0x0, 0x0, 274, 3031, 2833, 0, 0, -1 },
  { 0x0, 0x0, 274, 3032, 2835, 0, 0, -1 },
  { 0x0, 0x0, 275, -1, 2834, 0, 0, -1 },
  { 0x0, 0x0, 275, -1, 2836, 0, 0, -1 },
  { 0x0, 0x0, 276, -1, -1, 0, 1, 41 },
  { 0x0, 0x0, 276, -1, -1, 0, 1, 41 },
  { 0x0, 0x0, 276, -1, -1, 0, 1, 41 },
  { 0x0, 0x0, 281, -1, -1, 0, 1, 34 },
  { 0x0, 0x0, 285, -1, 2350, 0, 1, 30 },
  { 0x0, 0x0, 286, -1, -1, 0, 1, 0 },
  { 0x0, 0x0, 286, -1, -1, 0, 1, 72 },
  { 0x0, 0x0, 286, 2001, 3000, 0, 1, 1 },
  { 0x0, 0x0, 286, 2002, 3001, 0, 1, 1 },
  { 0x0, 0x0, 286, -1, 518, 0, 0, -1 },
  { 0x0, 0x0, 286, -1, 520, 0, 0, -1 },
  { 0x0, 0x0, 286, 2005, 3004, 0, 1, 76 },
  { 0x0, 0x0, 286, 2006, 3005, 0, 1, 76 },
  { 0x0, 0x0, 286, -1, 526, 0, 0, -1 },
  { 0x0, 0x0, 286, -1, 528, 0, 0, -1 },
  { 0x0, 0x0, 286, 2009, 3008, 0, 1, 1 },
  { 0x0, 0x0, 286, 2010, 3009, 0, 1, 1 },
  { 0x0, 0x0, 286, -1, 534, 0, 0, -1 },
  { 0x0, 0x0, 286, -1, 536, 0, 0, -1 },
};

static const struct ia64_main_table
main_table[] = {
  { 5, 1, 1, 0x0000010000000000ull, 0x000001eff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 0, },
  { 5, 1, 1, 0x0000010008000000ull, 0x000001eff8000000ull, { 24, 25, 26, 4, 0 }, 0x0, 1, },
  { 5, 7, 1, 0x0000000000000000ull, 0x0000000000000000ull, { 24, 67, 27, 0, 0 }, 0x0, 2, },
  { 5, 7, 1, 0x0000000000000000ull, 0x0000000000000000ull, { 24, 64, 26, 0, 0 }, 0x0, 3, },
  { 6, 1, 1, 0x0000012000000000ull, 0x000001e000000000ull, { 24, 67, 27, 0, 0 }, 0x0, 4, },
  { 7, 1, 1, 0x0000010040000000ull, 0x000001eff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 5, },
  { 7, 1, 1, 0x0000010c00000000ull, 0x000001ee00000000ull, { 24, 64, 26, 0, 0 }, 0x0, 6, },
  { 8, 1, 1, 0x0000010800000000ull, 0x000001ee00000000ull, { 24, 64, 26, 0, 0 }, 0x0, 7, },
  { 9, 3, 1, 0x0000002c00000000ull, 0x000001ee00000000ull, { 24, 3, 53, 54, 55 }, 0x221, 8, },
  { 9, 3, 1, 0x0000002c00000000ull, 0x000001ee00000000ull, { 24, 53, 54, 55, 0 }, 0x261, 9, },
  { 10, 1, 1, 0x0000010060000000ull, 0x000001eff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 10, },
  { 10, 1, 1, 0x0000010160000000ull, 0x000001eff8000000ull, { 24, 56, 26, 0, 0 }, 0x0, 11, },
  { 11, 1, 1, 0x0000010068000000ull, 0x000001eff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 12, },
  { 11, 1, 1, 0x0000010168000000ull, 0x000001eff8000000ull, { 24, 56, 26, 0, 0 }, 0x0, 13, },
  { 14, 4, 0, 0x0000000100000000ull, 0x000001eff80011ffull, { 16, 0, 0, 0, 0 }, 0x40, 969, },
  { 14, 4, 0, 0x0000000100000000ull, 0x000001eff80011c0ull, { 16, 0, 0, 0, 0 }, 0x0, 825, },
  { 14, 4, 0, 0x0000000100000000ull, 0x000001eff80011c0ull, { 16, 0, 0, 0, 0 }, 0x40, 826, },
  { 14, 4, 0, 0x0000000108000100ull, 0x000001eff80011c0ull, { 16, 0, 0, 0, 0 }, 0x200, 2234, },
  { 14, 4, 0, 0x0000000108000100ull, 0x000001eff80011c0ull, { 16, 0, 0, 0, 0 }, 0x240, 2235, },
  { 14, 4, 1, 0x0000002100000000ull, 0x000001ef00001000ull, { 15, 16, 0, 0, 0 }, 0x0, 582, },
  { 14, 4, 1, 0x0000002100000000ull, 0x000001ef00001000ull, { 15, 16, 0, 0, 0 }, 0x40, 583, },
  { 14, 4, 0, 0x0000008000000000ull, 0x000001ee000011ffull, { 82, 0, 0, 0, 0 }, 0x40, 990, },
  { 14, 4, 0, 0x0000008000000000ull, 0x000001ee000011c0ull, { 82, 0, 0, 0, 0 }, 0x0, 827, },
  { 14, 4, 0, 0x0000008000000000ull, 0x000001ee000011c0ull, { 82, 0, 0, 0, 0 }, 0x40, 828, },
  { 14, 4, 0, 0x0000008000000080ull, 0x000001ee000011c0ull, { 82, 0, 0, 0, 0 }, 0x210, 3029, },
  { 14, 4, 0, 0x0000008000000080ull, 0x000001ee000011c0ull, { 82, 0, 0, 0, 0 }, 0x250, 3030, },
  { 14, 4, 0, 0x0000008000000140ull, 0x000001ee000011c0ull, { 82, 0, 0, 0, 0 }, 0x30, 590, },
  { 14, 4, 0, 0x0000008000000140ull, 0x000001ee000011c0ull, { 82, 0, 0, 0, 0 }, 0x70, 591, },
  { 14, 4, 0, 0x0000008000000180ull, 0x000001ee000011c0ull, { 82, 0, 0, 0, 0 }, 0x230, 588, },
  { 14, 4, 0, 0x0000008000000180ull, 0x000001ee000011c0ull, { 82, 0, 0, 0, 0 }, 0x270, 589, },
  { 14, 4, 1, 0x000000a000000000ull, 0x000001ee00001000ull, { 15, 82, 0, 0, 0 }, 0x0, 584, },
  { 14, 4, 1, 0x000000a000000000ull, 0x000001ee00001000ull, { 15, 82, 0, 0, 0 }, 0x40, 585, },
  { 15, 4, 0, 0x0000000000000000ull, 0x000001e1f8000000ull, { 66, 0, 0, 0, 0 }, 0x0, 537, },
  { 15, 5, 0, 0x0000000000000000ull, 0x000001e3f8000000ull, { 66, 0, 0, 0, 0 }, 0x0, 960, },
  { 15, 2, 0, 0x0000000000000000ull, 0x000001eff8000000ull, { 66, 0, 0, 0, 0 }, 0x2, 1138, },
  { 15, 3, 0, 0x0000000000000000ull, 0x000001eff8000000ull, { 66, 0, 0, 0, 0 }, 0x0, 1263, },
  { 15, 6, 0, 0x0000000000000000ull, 0x000001eff8000000ull, { 70, 0, 0, 0, 0 }, 0x0, 3033, },
  { 15, 7, 0, 0x0000000000000000ull, 0x0000000000000000ull, { 66, 0, 0, 0, 0 }, 0x0, 16, },
  { 16, 6, 0, 0x0000018000000000ull, 0x000001ee000011ffull, { 83, 0, 0, 0, 0 }, 0x40, 1023, },
  { 16, 6, 0, 0x0000018000000000ull, 0x000001ee000011c0ull, { 83, 0, 0, 0, 0 }, 0x0, 829, },
  { 16, 6, 0, 0x0000018000000000ull, 0x000001ee000011c0ull, { 83, 0, 0, 0, 0 }, 0x40, 830, },
  { 16, 6, 1, 0x000001a000000000ull, 0x000001ee00001000ull, { 15, 83, 0, 0, 0 }, 0x0, 586, },
  { 16, 6, 1, 0x000001a000000000ull, 0x000001ee00001000ull, { 15, 83, 0, 0, 0 }, 0x40, 587, },
  { 17, 4, 0, 0x0000004080000000ull, 0x000001e9f8000018ull, { 16, 78, 0, 0, 0 }, 0x20, 2852, },
  { 17, 4, 0, 0x000000e000000000ull, 0x000001e800000018ull, { 82, 78, 0, 0, 0 }, 0x20, 2853, },
  { 18, 4, 0, 0x0000000060000000ull, 0x000001e1f8000000ull, { 0, 0, 0, 0, 0 }, 0x2c, 222, },
  { 22, 2, 0, 0x0000000200000000ull, 0x000001ee00000000ull, { 25, 81, 0, 0, 0 }, 0x0, 2239, },
  { 22, 3, 0, 0x0000000800000000ull, 0x000001ee00000000ull, { 24, 82, 0, 0, 0 }, 0x0, 226, },
  { 22, 3, 0, 0x0000000c00000000ull, 0x000001ee00000000ull, { 18, 82, 0, 0, 0 }, 0x0, 227, },
  { 22, 3, 0, 0x0000002200000000ull, 0x000001ee00000000ull, { 25, 81, 0, 0, 0 }, 0x0, 2240, },
  { 22, 3, 0, 0x0000002600000000ull, 0x000001ee00000000ull, { 19, 81, 0, 0, 0 }, 0x0, 2241, },
  { 22, 7, 0, 0x0000000000000000ull, 0x0000000000000000ull, { 25, 81, 0, 0, 0 }, 0x0, 2242, },
  { 25, 4, 0, 0x0000000020000000ull, 0x000001e1f8000000ull, { 0, 0, 0, 0, 0 }, 0x224, 18, },
  { 26, 1, 2, 0x0000018000000000ull, 0x000001fe00001000ull, { 22, 23, 25, 26, 0 }, 0x0, 1222, },
  { 26, 1, 1, 0x0000018000000000ull, 0x000001fe00001000ull, { 22, 25, 26, 0, 0 }, 0x40, 1223, },
  { 26, 1, 2, 0x0000018000000000ull, 0x000001fe00001000ull, { 23, 22, 26, 25, 0 }, 0x0, 1181, },
  { 26, 1, 1, 0x0000018000000000ull, 0x000001fe00001000ull, { 23, 26, 25, 0, 0 }, 0x40, 1182, },
  { 26, 1, 2, 0x0000018000000000ull, 0x000001fe00001000ull, { 22, 23, 26, 25, 0 }, 0x0, 1090, },
  { 26, 1, 1, 0x0000018000000000ull, 0x000001fe00001000ull, { 22, 26, 25, 0, 0 }, 0x40, 1091, },
  { 26, 1, 2, 0x0000018000000000ull, 0x000001fe00001000ull, { 23, 22, 25, 26, 0 }, 0x0, 1052, },
  { 26, 1, 1, 0x0000018000000000ull, 0x000001fe00001000ull, { 23, 25, 26, 0, 0 }, 0x40, 1053, },
  { 26, 1, 2, 0x0000018200000000ull, 0x000001fe00001000ull, { 22, 23, 25, 26, 0 }, 0x40, 1376, },
  { 26, 1, 2, 0x0000019000000000ull, 0x000001fe00001000ull, { 22, 23, 7, 26, 0 }, 0x0, 1092, },
  { 26, 1, 1, 0x0000019000000000ull, 0x000001fe00001000ull, { 22, 7, 26, 0, 0 }, 0x40, 1093, },
  { 26, 1, 2, 0x0000019000000000ull, 0x000001fe00001000ull, { 22, 23, 26, 7, 0 }, 0x40, 1226, },
  { 26, 1, 1, 0x0000019000000000ull, 0x000001fe00001000ull, { 22, 26, 7, 0, 0 }, 0x40, 1227, },
  { 26, 1, 2, 0x0000019000000000ull, 0x000001fe00001000ull, { 22, 23, 7, 26, 0 }, 0x40, 1187, },
  { 26, 1, 2, 0x0000018800000000ull, 0x000001ee00001000ull, { 22, 23, 56, 26, 0 }, 0x0, 1229, },
  { 26, 1, 1, 0x0000018800000000ull, 0x000001ee00001000ull, { 22, 56, 26, 0, 0 }, 0x40, 1230, },
  { 26, 1, 2, 0x0000018800000000ull, 0x000001ee00001000ull, { 22, 23, 58, 26, 0 }, 0x0, 1188, },
  { 26, 1, 1, 0x0000018800000000ull, 0x000001ee00001000ull, { 22, 58, 26, 0, 0 }, 0x40, 1189, },
  { 26, 1, 2, 0x0000018800000000ull, 0x000001ee00001000ull, { 23, 22, 58, 26, 0 }, 0x0, 1097, },
  { 26, 1, 1, 0x0000018800000000ull, 0x000001ee00001000ull, { 23, 58, 26, 0, 0 }, 0x40, 1098, },
  { 26, 1, 2, 0x0000018800000000ull, 0x000001ee00001000ull, { 23, 22, 56, 26, 0 }, 0x0, 1059, },
  { 26, 1, 1, 0x0000018800000000ull, 0x000001ee00001000ull, { 23, 56, 26, 0, 0 }, 0x40, 1060, },
  { 26, 1, 2, 0x0000018a00000000ull, 0x000001ee00001000ull, { 22, 23, 56, 26, 0 }, 0x40, 1381, },
  { 26, 1, 2, 0x000001a800000000ull, 0x000001ee00001000ull, { 22, 23, 60, 26, 0 }, 0x0, 1214, },
  { 26, 1, 1, 0x000001a800000000ull, 0x000001ee00001000ull, { 22, 60, 26, 0, 0 }, 0x40, 1215, },
  { 26, 1, 2, 0x000001a800000000ull, 0x000001ee00001000ull, { 23, 22, 60, 26, 0 }, 0x0, 1125, },
  { 26, 1, 1, 0x000001a800000000ull, 0x000001ee00001000ull, { 23, 60, 26, 0, 0 }, 0x40, 1126, },
  { 26, 1, 2, 0x000001c200000000ull, 0x000001fe00001000ull, { 23, 22, 25, 26, 0 }, 0x40, 1382, },
  { 26, 1, 2, 0x000001d000000000ull, 0x000001fe00001000ull, { 23, 22, 7, 26, 0 }, 0x40, 1190, },
  { 26, 1, 1, 0x000001d000000000ull, 0x000001fe00001000ull, { 23, 7, 26, 0, 0 }, 0x40, 1191, },
  { 26, 1, 2, 0x000001d000000000ull, 0x000001fe00001000ull, { 23, 22, 26, 7, 0 }, 0x40, 1063, },
  { 26, 1, 1, 0x000001d000000000ull, 0x000001fe00001000ull, { 23, 26, 7, 0, 0 }, 0x40, 1064, },
  { 26, 1, 2, 0x000001ca00000000ull, 0x000001ee00001000ull, { 23, 22, 56, 26, 0 }, 0x40, 1383, },
  { 27, 1, 2, 0x0000018400000000ull, 0x000001fe00001000ull, { 22, 23, 25, 26, 0 }, 0x0, 1235, },
  { 27, 1, 1, 0x0000018400000000ull, 0x000001fe00001000ull, { 22, 25, 26, 0, 0 }, 0x40, 1236, },
  { 27, 1, 2, 0x0000018400000000ull, 0x000001fe00001000ull, { 23, 22, 26, 25, 0 }, 0x0, 1194, },
  { 27, 1, 1, 0x0000018400000000ull, 0x000001fe00001000ull, { 23, 26, 25, 0, 0 }, 0x40, 1195, },
  { 27, 1, 2, 0x0000018400000000ull, 0x000001fe00001000ull, { 22, 23, 26, 25, 0 }, 0x0, 1103, },
  { 27, 1, 1, 0x0000018400000000ull, 0x000001fe00001000ull, { 22, 26, 25, 0, 0 }, 0x40, 1104, },
  { 27, 1, 2, 0x0000018400000000ull, 0x000001fe00001000ull, { 23, 22, 25, 26, 0 }, 0x0, 1065, },
  { 27, 1, 1, 0x0000018400000000ull, 0x000001fe00001000ull, { 23, 25, 26, 0, 0 }, 0x40, 1066, },
  { 27, 1, 2, 0x0000018600000000ull, 0x000001fe00001000ull, { 22, 23, 25, 26, 0 }, 0x40, 1388, },
  { 27, 1, 2, 0x0000019400000000ull, 0x000001fe00001000ull, { 22, 23, 7, 26, 0 }, 0x0, 1105, },
  { 27, 1, 1, 0x0000019400000000ull, 0x000001fe00001000ull, { 22, 7, 26, 0, 0 }, 0x40, 1106, },
  { 27, 1, 2, 0x0000019400000000ull, 0x000001fe00001000ull, { 22, 23, 26, 7, 0 }, 0x40, 1239, },
  { 27, 1, 1, 0x0000019400000000ull, 0x000001fe00001000ull, { 22, 26, 7, 0, 0 }, 0x40, 1240, },
  { 27, 1, 2, 0x0000019400000000ull, 0x000001fe00001000ull, { 22, 23, 7, 26, 0 }, 0x40, 1200, },
  { 27, 1, 2, 0x0000018c00000000ull, 0x000001ee00001000ull, { 22, 23, 56, 26, 0 }, 0x0, 1242, },
  { 27, 1, 1, 0x0000018c00000000ull, 0x000001ee00001000ull, { 22, 56, 26, 0, 0 }, 0x40, 1243, },
  { 27, 1, 2, 0x0000018c00000000ull, 0x000001ee00001000ull, { 22, 23, 58, 26, 0 }, 0x0, 1201, },
  { 27, 1, 1, 0x0000018c00000000ull, 0x000001ee00001000ull, { 22, 58, 26, 0, 0 }, 0x40, 1202, },
  { 27, 1, 2, 0x0000018c00000000ull, 0x000001ee00001000ull, { 23, 22, 58, 26, 0 }, 0x0, 1110, },
  { 27, 1, 1, 0x0000018c00000000ull, 0x000001ee00001000ull, { 23, 58, 26, 0, 0 }, 0x40, 1111, },
  { 27, 1, 2, 0x0000018c00000000ull, 0x000001ee00001000ull, { 23, 22, 56, 26, 0 }, 0x0, 1072, },
  { 27, 1, 1, 0x0000018c00000000ull, 0x000001ee00001000ull, { 23, 56, 26, 0, 0 }, 0x40, 1073, },
  { 27, 1, 2, 0x0000018e00000000ull, 0x000001ee00001000ull, { 22, 23, 56, 26, 0 }, 0x40, 1393, },
  { 27, 1, 2, 0x000001ac00000000ull, 0x000001ee00001000ull, { 22, 23, 57, 26, 0 }, 0x0, 1259, },
  { 27, 1, 1, 0x000001ac00000000ull, 0x000001ee00001000ull, { 22, 57, 26, 0, 0 }, 0x40, 1260, },
  { 27, 1, 2, 0x000001ac00000000ull, 0x000001ee00001000ull, { 22, 23, 59, 26, 0 }, 0x0, 1218, },
  { 27, 1, 1, 0x000001ac00000000ull, 0x000001ee00001000ull, { 22, 59, 26, 0, 0 }, 0x40, 1219, },
  { 27, 1, 2, 0x000001ac00000000ull, 0x000001ee00001000ull, { 23, 22, 59, 26, 0 }, 0x0, 1129, },
  { 27, 1, 1, 0x000001ac00000000ull, 0x000001ee00001000ull, { 23, 59, 26, 0, 0 }, 0x40, 1130, },
  { 27, 1, 2, 0x000001ac00000000ull, 0x000001ee00001000ull, { 23, 22, 57, 26, 0 }, 0x0, 1088, },
  { 27, 1, 1, 0x000001ac00000000ull, 0x000001ee00001000ull, { 23, 57, 26, 0, 0 }, 0x40, 1089, },
  { 27, 1, 2, 0x000001c600000000ull, 0x000001fe00001000ull, { 23, 22, 25, 26, 0 }, 0x40, 1394, },
  { 27, 1, 2, 0x000001d400000000ull, 0x000001fe00001000ull, { 23, 22, 7, 26, 0 }, 0x40, 1203, },
  { 27, 1, 1, 0x000001d400000000ull, 0x000001fe00001000ull, { 23, 7, 26, 0, 0 }, 0x40, 1204, },
  { 27, 1, 2, 0x000001d400000000ull, 0x000001fe00001000ull, { 23, 22, 26, 7, 0 }, 0x40, 1076, },
  { 27, 1, 1, 0x000001d400000000ull, 0x000001fe00001000ull, { 23, 26, 7, 0, 0 }, 0x40, 1077, },
  { 27, 1, 2, 0x000001ce00000000ull, 0x000001ee00001000ull, { 23, 22, 56, 26, 0 }, 0x40, 1395, },
  { 28, 3, 1, 0x0000008808000000ull, 0x000001fff8000000ull, { 24, 28, 25, 1, 2 }, 0x0, 259, },
  { 28, 3, 1, 0x0000008808000000ull, 0x000001fff8000000ull, { 24, 28, 25, 0, 0 }, 0x40, 260, },
  { 29, 3, 1, 0x0000008008000000ull, 0x000001fff8000000ull, { 24, 28, 25, 2, 0 }, 0x0, 261, },
  { 29, 3, 1, 0x0000008008000000ull, 0x000001fff8000000ull, { 24, 28, 25, 0, 0 }, 0x40, 262, },
  { 30, 3, 1, 0x0000008048000000ull, 0x000001fff8000000ull, { 24, 28, 25, 2, 0 }, 0x0, 263, },
  { 30, 3, 1, 0x0000008048000000ull, 0x000001fff8000000ull, { 24, 28, 25, 0, 0 }, 0x40, 264, },
  { 31, 3, 1, 0x0000008088000000ull, 0x000001fff8000000ull, { 24, 28, 25, 2, 0 }, 0x0, 265, },
  { 31, 3, 1, 0x0000008088000000ull, 0x000001fff8000000ull, { 24, 28, 25, 0, 0 }, 0x40, 266, },
  { 32, 3, 1, 0x00000080c8000000ull, 0x000001fff8000000ull, { 24, 28, 25, 2, 0 }, 0x0, 267, },
  { 32, 3, 1, 0x00000080c8000000ull, 0x000001fff8000000ull, { 24, 28, 25, 0, 0 }, 0x40, 268, },
  { 34, 4, 0, 0x0000000010000000ull, 0x000001e1f8000000ull, { 0, 0, 0, 0, 0 }, 0x224, 19, },
  { 36, 2, 1, 0x00000000c0000000ull, 0x000001eff8000000ull, { 24, 26, 0, 0, 0 }, 0x0, 1167, },
  { 37, 2, 1, 0x00000000c8000000ull, 0x000001eff8000000ull, { 24, 26, 0, 0, 0 }, 0x0, 1168, },
  { 39, 2, 1, 0x0000008000000000ull, 0x000001e000000000ull, { 24, 25, 26, 47, 73 }, 0x0, 20, },
  { 39, 2, 1, 0x000000a600000000ull, 0x000001ee04000000ull, { 24, 25, 45, 74, 0 }, 0x0, 3038, },
  { 39, 2, 1, 0x000000a604000000ull, 0x000001ee04000000ull, { 24, 56, 45, 74, 0 }, 0x0, 3039, },
  { 39, 2, 1, 0x000000ae00000000ull, 0x000001ee00000000ull, { 24, 48, 26, 46, 74 }, 0x0, 21, },
  { 43, 4, 0, 0x0000000080000000ull, 0x000001e1f8000000ull, { 0, 0, 0, 0, 0 }, 0x20, 22, },
  { 48, 2, 1, 0x000000a400000000ull, 0x000001ee00002000ull, { 24, 26, 77, 74, 0 }, 0x0, 2870, },
  { 50, 5, 1, 0x0000000080000000ull, 0x000001e3f80fe000ull, { 18, 20, 0, 0, 0 }, 0x40, 24, },
  { 51, 5, 1, 0x0000010008000000ull, 0x000001fff8000000ull, { 18, 20, 19, 0, 0 }, 0x40, 2291, },
  { 52, 5, 1, 0x00000000b8000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 2292, },
  { 52, 5, 1, 0x00000000b8000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x40, 26, },
  { 53, 5, 1, 0x00000000b0000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 2293, },
  { 53, 5, 1, 0x00000000b0000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x40, 27, },
  { 54, 5, 1, 0x0000000160000000ull, 0x000001e3f8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 28, },
  { 55, 5, 1, 0x0000000168000000ull, 0x000001e3f8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 29, },
  { 57, 3, 0, 0x0000002180000000ull, 0x000001fff8000000ull, { 26, 0, 0, 0, 0 }, 0x0, 30, },
  { 58, 5, 0, 0x0000000040000000ull, 0x000001eff8000000ull, { 80, 0, 0, 0, 0 }, 0x0, 2294, },
  { 58, 5, 0, 0x0000000040000000ull, 0x000001eff8000000ull, { 80, 0, 0, 0, 0 }, 0x40, 31, },
  { 59, 5, 2, 0x000000a000000000ull, 0x000001e000001000ull, { 22, 23, 19, 61, 0 }, 0x0, 1265, },
  { 59, 5, 1, 0x000000a000000000ull, 0x000001e000001000ull, { 22, 19, 61, 0, 0 }, 0x40, 1266, },
  { 59, 5, 2, 0x000000a000000000ull, 0x000001e000001000ull, { 23, 22, 19, 61, 0 }, 0x40, 1420, },
  { 59, 5, 1, 0x000000a000000000ull, 0x000001e000001000ull, { 23, 19, 61, 0, 0 }, 0x40, 1421, },
  { 60, 5, 0, 0x0000000028000000ull, 0x000001eff8000000ull, { 0, 0, 0, 0, 0 }, 0x0, 2295, },
  { 60, 5, 0, 0x0000000028000000ull, 0x000001eff8000000ull, { 0, 0, 0, 0, 0 }, 0x40, 32, },
  { 61, 5, 2, 0x0000008000000000ull, 0x000001fe00001000ull, { 22, 23, 19, 20, 0 }, 0x0, 943, },
  { 61, 5, 1, 0x0000008000000000ull, 0x000001fe00001000ull, { 22, 19, 20, 0, 0 }, 0x40, 944, },
  { 61, 5, 2, 0x0000008000000000ull, 0x000001fe00001000ull, { 22, 23, 19, 20, 0 }, 0x40, 945, },
  { 61, 5, 2, 0x0000009000000000ull, 0x000001fe00001000ull, { 22, 23, 20, 19, 0 }, 0x0, 1116, },
  { 61, 5, 1, 0x0000009000000000ull, 0x000001fe00001000ull, { 22, 20, 19, 0, 0 }, 0x40, 1117, },
  { 61, 5, 2, 0x0000009000000000ull, 0x000001fe00001000ull, { 22, 23, 20, 19, 0 }, 0x40, 1118, },
  { 61, 5, 2, 0x0000008000000000ull, 0x000001fe00001000ull, { 23, 22, 19, 20, 0 }, 0x0, 1396, },
  { 61, 5, 1, 0x0000008000000000ull, 0x000001fe00001000ull, { 23, 19, 20, 0, 0 }, 0x40, 1397, },
  { 61, 5, 2, 0x0000008000000000ull, 0x000001fe00001000ull, { 23, 22, 19, 20, 0 }, 0x40, 1398, },
  { 61, 5, 2, 0x0000009000000000ull, 0x000001fe00001000ull, { 23, 22, 20, 19, 0 }, 0x0, 1405, },
  { 61, 5, 1, 0x0000009000000000ull, 0x000001fe00001000ull, { 23, 20, 19, 0, 0 }, 0x40, 1406, },
  { 61, 5, 2, 0x0000009000000000ull, 0x000001fe00001000ull, { 23, 22, 20, 19, 0 }, 0x40, 1407, },
  { 62, 5, 1, 0x00000000c0000000ull, 0x000001eff8000000ull, { 18, 19, 0, 0, 0 }, 0x0, 1042, },
  { 62, 5, 1, 0x00000000c0000000ull, 0x000001eff8000000ull, { 18, 19, 0, 0, 0 }, 0x40, 1043, },
  { 62, 5, 1, 0x00000000e0000000ull, 0x000001e3f8000000ull, { 18, 19, 0, 0, 0 }, 0x0, 3036, },
  { 62, 5, 1, 0x0000010008000000ull, 0x000001fff80fe000ull, { 18, 20, 0, 0, 0 }, 0x40, 3037, },
  { 63, 3, 1, 0x0000008488000000ull, 0x000001fff8000000ull, { 24, 28, 72, 0, 0 }, 0x0, 269, },
  { 64, 3, 1, 0x00000084c8000000ull, 0x000001fff8000000ull, { 24, 28, 72, 0, 0 }, 0x0, 270, },
  { 67, 3, 0, 0x0000000060000000ull, 0x000001eff8000000ull, { 0, 0, 0, 0, 0 }, 0x21, 33, },
  { 68, 5, 1, 0x0000010000000000ull, 0x000001fc00000000ull, { 18, 20, 21, 19, 0 }, 0x0, 2353, },
  { 68, 5, 1, 0x0000010000000000ull, 0x000001fc00000000ull, { 18, 20, 21, 19, 0 }, 0x40, 34, },
  { 69, 5, 1, 0x00000000a8000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 2354, },
  { 69, 5, 1, 0x00000000a8000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x40, 35, },
  { 70, 5, 1, 0x0000000080000000ull, 0x000001e3f8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 2247, },
  { 71, 5, 1, 0x00000000a0000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 2355, },
  { 71, 5, 1, 0x00000000a0000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x40, 36, },
  { 72, 5, 1, 0x00000001c8000000ull, 0x000001e3f8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 1221, },
  { 73, 5, 1, 0x0000010000000000ull, 0x000001fc000fe000ull, { 18, 20, 21, 0, 0 }, 0x40, 2358, },
  { 74, 5, 1, 0x0000014000000000ull, 0x000001fc00000000ull, { 18, 20, 21, 19, 0 }, 0x0, 2361, },
  { 74, 5, 1, 0x0000014000000000ull, 0x000001fc00000000ull, { 18, 20, 21, 19, 0 }, 0x40, 38, },
  { 75, 5, 1, 0x0000000088000000ull, 0x000001e3f8000000ull, { 18, 20, 0, 0, 0 }, 0xc0, 39, },
  { 76, 5, 1, 0x0000000088000000ull, 0x000001e3f80fe000ull, { 18, 20, 0, 0, 0 }, 0x40, 40, },
  { 77, 5, 1, 0x0000018000000000ull, 0x000001fc00000000ull, { 18, 20, 21, 19, 0 }, 0x0, 2364, },
  { 77, 5, 1, 0x0000018000000000ull, 0x000001fc00000000ull, { 18, 20, 21, 19, 0 }, 0x40, 41, },
  { 78, 5, 1, 0x0000018000000000ull, 0x000001fc000fe000ull, { 18, 20, 21, 0, 0 }, 0x40, 2367, },
  { 79, 5, 1, 0x0000010008000000ull, 0x000001fff80fe000ull, { 18, 20, 0, 0, 0 }, 0x40, 2370, },
  { 80, 5, 1, 0x0000000170000000ull, 0x000001e3f8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 44, },
  { 81, 5, 1, 0x0000002080000000ull, 0x000001e3f80fe000ull, { 18, 20, 0, 0, 0 }, 0x40, 45, },
  { 82, 5, 1, 0x0000000140000000ull, 0x000001e3f8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 46, },
  { 83, 5, 1, 0x00000020b8000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 2371, },
  { 83, 5, 1, 0x00000020b8000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x40, 47, },
  { 84, 5, 1, 0x00000020b0000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 2372, },
  { 84, 5, 1, 0x00000020b0000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x40, 48, },
  { 85, 5, 1, 0x0000002180000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 946, },
  { 85, 5, 1, 0x0000002180000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x40, 947, },
  { 85, 5, 1, 0x0000002188000000ull, 0x000001eff8000000ull, { 18, 20, 19, 0, 0 }, 0x40, 1119, },
  { 86, 5, 1, 0x00000020c0000000ull, 0x000001eff8000000ull, { 18, 19, 0, 0, 0 }, 0x0, 1044, },
  { 86, 5, 1, 0x00000020c0000000ull, 0x000001eff8000000ull, { 18, 19, 0, 0, 0 }, 0x40, 1045, },
  { 87, 5, 1, 0x0000013000000000ull, 0x000001fc00000000ull, { 18, 20, 21, 19, 0 }, 0x0, 2389, },
  { 87, 5, 1, 0x0000013000000000ull, 0x000001fc00000000ull, { 18, 20, 21, 19, 0 }, 0x40, 49, },
  { 88, 5, 1, 0x00000020a8000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 2390, },
  { 88, 5, 1, 0x00000020a8000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x40, 50, },
  { 89, 5, 1, 0x0000002080000000ull, 0x000001e3f8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 2255, },
  { 90, 5, 1, 0x00000020a0000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 2391, },
  { 90, 5, 1, 0x00000020a0000000ull, 0x000001eff8000000ull, { 18, 19, 20, 0, 0 }, 0x40, 51, },
  { 91, 5, 1, 0x0000013000000000ull, 0x000001fc000fe000ull, { 18, 20, 21, 0, 0 }, 0x40, 2392, },
  { 92, 5, 1, 0x0000017000000000ull, 0x000001fc00000000ull, { 18, 20, 21, 19, 0 }, 0x0, 2393, },
  { 92, 5, 1, 0x0000017000000000ull, 0x000001fc00000000ull, { 18, 20, 21, 19, 0 }, 0x40, 53, },
  { 93, 5, 1, 0x0000002088000000ull, 0x000001e3f8000000ull, { 18, 20, 0, 0, 0 }, 0xc0, 54, },
  { 94, 5, 1, 0x0000002088000000ull, 0x000001e3f80fe000ull, { 18, 20, 0, 0, 0 }, 0x40, 55, },
  { 95, 5, 1, 0x000001b000000000ull, 0x000001fc00000000ull, { 18, 20, 21, 19, 0 }, 0x0, 2394, },
  { 95, 5, 1, 0x000001b000000000ull, 0x000001fc00000000ull, { 18, 20, 21, 19, 0 }, 0x40, 56, },
  { 96, 5, 1, 0x000001b000000000ull, 0x000001fc000fe000ull, { 18, 20, 21, 0, 0 }, 0x40, 2395, },
  { 97, 5, 2, 0x0000002200000000ull, 0x000001fe00000000ull, { 18, 23, 19, 20, 0 }, 0x0, 2396, },
  { 97, 5, 2, 0x0000002200000000ull, 0x000001fe00000000ull, { 18, 23, 19, 20, 0 }, 0x40, 58, },
  { 98, 5, 2, 0x0000003200000000ull, 0x000001fe00000000ull, { 18, 23, 20, 0, 0 }, 0x0, 2397, },
  { 98, 5, 2, 0x0000003200000000ull, 0x000001fe00000000ull, { 18, 23, 20, 0, 0 }, 0x40, 59, },
  { 99, 5, 2, 0x0000000200000000ull, 0x000001fe00000000ull, { 18, 23, 19, 20, 0 }, 0x0, 2398, },
  { 99, 5, 2, 0x0000000200000000ull, 0x000001fe00000000ull, { 18, 23, 19, 20, 0 }, 0x40, 60, },
  { 100, 5, 2, 0x0000001200000000ull, 0x000001fe00000000ull, { 18, 23, 20, 0, 0 }, 0x0, 2399, },
  { 100, 5, 2, 0x0000001200000000ull, 0x000001fe00000000ull, { 18, 23, 20, 0, 0 }, 0x40, 61, },
  { 101, 5, 1, 0x000001c000000000ull, 0x000001f000000000ull, { 18, 20, 21, 19, 0 }, 0x0, 62, },
  { 102, 5, 0, 0x0000000020000000ull, 0x000001eff8000000ull, { 51, 52, 0, 0, 0 }, 0x0, 2400, },
  { 102, 5, 0, 0x0000000020000000ull, 0x000001eff8000000ull, { 51, 52, 0, 0, 0 }, 0x40, 63, },
  { 103, 5, 1, 0x0000014008000000ull, 0x000001fff8000000ull, { 18, 20, 19, 0, 0 }, 0x40, 2403, },
  { 104, 5, 1, 0x00000001a0000000ull, 0x000001e3f8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 65, },
  { 105, 5, 1, 0x00000001e0000000ull, 0x000001e3f8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 2202, },
  { 106, 3, 0, 0x0000000100000000ull, 0x000001eff8000000ull, { 0, 0, 0, 0, 0 }, 0x0, 66, },
  { 108, 5, 1, 0x0000000178000000ull, 0x000001e3f8000000ull, { 18, 19, 20, 0, 0 }, 0x0, 67, },
  { 113, 3, 1, 0x0000008708000000ull, 0x000001ffc8000000ull, { 24, 19, 0, 0, 0 }, 0x0, 2781, },
  { 118, 4, 0, 0x0000004008000000ull, 0x000001e1f8000000ull, { 66, 0, 0, 0, 0 }, 0x0, 538, },
  { 118, 5, 0, 0x000000000c000000ull, 0x000001e3fc000000ull, { 66, 0, 0, 0, 0 }, 0x0, 961, },
  { 118, 2, 0, 0x000000000c000000ull, 0x000001effc000000ull, { 66, 0, 0, 0, 0 }, 0x2, 1141, },
  { 118, 3, 0, 0x000000000c000000ull, 0x000001effc000000ull, { 66, 0, 0, 0, 0 }, 0x0, 1267, },
  { 118, 6, 0, 0x000000000c000000ull, 0x000001effc000000ull, { 70, 0, 0, 0, 0 }, 0x0, 3034, },
  { 118, 7, 0, 0x0000000000000000ull, 0x0000000000000000ull, { 66, 0, 0, 0, 0 }, 0x0, 68, },
  { 123, 3, 0, 0x0000000080000000ull, 0x000001eff8000000ull, { 0, 0, 0, 0, 0 }, 0x0, 69, },
  { 123, 3, 0, 0x0000000090000000ull, 0x000001eff8000000ull, { 24, 0, 0, 0, 0 }, 0x0, 920, },
  { 123, 3, 0, 0x0000000098000000ull, 0x000001eff8000000ull, { 18, 0, 0, 0, 0 }, 0x0, 921, },
  { 124, 3, 0, 0x0000002170000000ull, 0x000001eff8000000ull, { 25, 0, 0, 0, 0 }, 0xc, 846, },
  { 125, 3, 1, 0x0000002070000000ull, 0x000001eff8000000ull, { 31, 25, 0, 0, 0 }, 0x8, 847, },
  { 125, 3, 1, 0x0000002078000000ull, 0x000001eff8000000ull, { 32, 25, 0, 0, 0 }, 0x8, 1143, },
  { 127, 3, 1, 0x0000008000000000ull, 0x000001fff8000000ull, { 24, 28, 0, 0, 0 }, 0x0, 70, },
  { 127, 3, 1, 0x0000009000000000ull, 0x000001fff8000000ull, { 24, 28, 25, 0, 0 }, 0x400, 71, },
  { 127, 3, 1, 0x000000a000000000ull, 0x000001eff0000000ull, { 24, 28, 63, 0, 0 }, 0x400, 72, },
  { 128, 3, 2, 0x0000008a08000000ull, 0x000001fff8000000ull, { 24, 1, 28, 0, 0 }, 0x0, 73, },
  { 128, 3, 1, 0x0000008a08000000ull, 0x000001fff8000000ull, { 24, 28, 0, 0, 0 }, 0x40, 74, },
  { 129, 3, 1, 0x0000008040000000ull, 0x000001fff8000000ull, { 24, 28, 0, 0, 0 }, 0x0, 75, },
  { 129, 3, 1, 0x0000009040000000ull, 0x000001fff8000000ull, { 24, 28, 25, 0, 0 }, 0x400, 76, },
  { 129, 3, 1, 0x000000a040000000ull, 0x000001eff0000000ull, { 24, 28, 63, 0, 0 }, 0x400, 77, },
  { 130, 3, 1, 0x0000008080000000ull, 0x000001fff8000000ull, { 24, 28, 0, 0, 0 }, 0x0, 78, },
  { 130, 3, 1, 0x0000009080000000ull, 0x000001fff8000000ull, { 24, 28, 25, 0, 0 }, 0x400, 79, },
  { 130, 3, 1, 0x000000a080000000ull, 0x000001eff0000000ull, { 24, 28, 63, 0, 0 }, 0x400, 80, },
  { 131, 3, 1, 0x00000080c0000000ull, 0x000001fff8000000ull, { 24, 28, 0, 0, 0 }, 0x0, 81, },
  { 131, 3, 1, 0x00000080c0000000ull, 0x000001fff8000000ull, { 24, 28, 84, 0, 0 }, 0x0, 1339, },
  { 131, 3, 1, 0x00000090c0000000ull, 0x000001fff8000000ull, { 24, 28, 25, 0, 0 }, 0x400, 82, },
  { 131, 3, 1, 0x000000a0c0000000ull, 0x000001eff0000000ull, { 24, 28, 63, 0, 0 }, 0x400, 83, },
  { 132, 3, 1, 0x000000c6c0000000ull, 0x000001fff8000000ull, { 18, 28, 0, 0, 0 }, 0x0, 1039, },
  { 132, 3, 1, 0x000000d6c0000000ull, 0x000001fff8000000ull, { 18, 28, 25, 0, 0 }, 0x400, 1040, },
  { 132, 3, 1, 0x000000e6c0000000ull, 0x000001eff0000000ull, { 18, 28, 63, 0, 0 }, 0x400, 1041, },
  { 133, 3, 1, 0x000000c040000000ull, 0x000001fff8000000ull, { 18, 28, 0, 0, 0 }, 0x0, 84, },
  { 133, 3, 1, 0x000000d040000000ull, 0x000001fff8000000ull, { 18, 28, 25, 0, 0 }, 0x400, 85, },
  { 133, 3, 1, 0x000000e040000000ull, 0x000001eff0000000ull, { 18, 28, 63, 0, 0 }, 0x400, 86, },
  { 134, 3, 1, 0x000000c0c0000000ull, 0x000001fff8000000ull, { 18, 28, 0, 0, 0 }, 0x0, 87, },
  { 134, 3, 1, 0x000000d0c0000000ull, 0x000001fff8000000ull, { 18, 28, 25, 0, 0 }, 0x400, 88, },
  { 134, 3, 1, 0x000000e0c0000000ull, 0x000001eff0000000ull, { 18, 28, 63, 0, 0 }, 0x400, 89, },
  { 135, 3, 1, 0x000000c000000000ull, 0x000001fff8000000ull, { 18, 28, 0, 0, 0 }, 0x0, 90, },
  { 135, 3, 1, 0x000000d000000000ull, 0x000001fff8000000ull, { 18, 28, 25, 0, 0 }, 0x400, 91, },
  { 135, 3, 1, 0x000000e000000000ull, 0x000001eff0000000ull, { 18, 28, 63, 0, 0 }, 0x400, 92, },
  { 136, 3, 2, 0x000000c048000000ull, 0x000001fff8000000ull, { 18, 19, 28, 0, 0 }, 0x0, 93, },
  { 136, 3, 2, 0x000000d048000000ull, 0x000001fff8000000ull, { 18, 19, 28, 6, 0 }, 0x400, 94, },
  { 137, 3, 2, 0x000000c0c8000000ull, 0x000001fff8000000ull, { 18, 19, 28, 0, 0 }, 0x0, 95, },
  { 137, 3, 2, 0x000000d0c8000000ull, 0x000001fff8000000ull, { 18, 19, 28, 6, 0 }, 0x400, 96, },
  { 138, 3, 2, 0x000000c088000000ull, 0x000001fff8000000ull, { 18, 19, 28, 0, 0 }, 0x0, 97, },
  { 138, 3, 2, 0x000000d088000000ull, 0x000001fff8000000ull, { 18, 19, 28, 5, 0 }, 0x400, 98, },
  { 139, 3, 1, 0x000000c080000000ull, 0x000001fff8000000ull, { 18, 28, 0, 0, 0 }, 0x0, 99, },
  { 139, 3, 1, 0x000000d080000000ull, 0x000001fff8000000ull, { 18, 28, 25, 0, 0 }, 0x400, 100, },
  { 139, 3, 1, 0x000000e080000000ull, 0x000001eff0000000ull, { 18, 28, 63, 0, 0 }, 0x400, 101, },
  { 142, 3, 0, 0x000000cb00000000ull, 0x000001fff8000000ull, { 28, 0, 0, 0, 0 }, 0x0, 102, },
  { 142, 3, 0, 0x000000db00000000ull, 0x000001fff8000000ull, { 28, 25, 0, 0, 0 }, 0x400, 103, },
  { 142, 3, 0, 0x000000eb00000000ull, 0x000001eff0000000ull, { 28, 63, 0, 0, 0 }, 0x400, 104, },
  { 143, 3, 0, 0x0000000050000000ull, 0x000001eff8000000ull, { 0, 0, 0, 0, 0 }, 0x21, 105, },
  { 151, 3, 0, 0x0000000110000000ull, 0x000001eff8000000ull, { 0, 0, 0, 0, 0 }, 0x0, 106, },
  { 152, 2, 1, 0x000000e880000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 2203, },
  { 153, 2, 1, 0x000000ea80000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 2204, },
  { 154, 2, 1, 0x000000f880000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 2205, },
  { 155, 1, 1, 0x0000010800000000ull, 0x000001fff80fe000ull, { 24, 26, 0, 0, 0 }, 0x0, 107, },
  { 155, 1, 1, 0x0000012000000000ull, 0x000001e000300000ull, { 24, 67, 0, 0, 0 }, 0x40, 108, },
  { 155, 5, 1, 0x0000000080000000ull, 0x000001e3f8000000ull, { 18, 20, 0, 0, 0 }, 0xc0, 109, },
  { 155, 2, 1, 0x0000000e00100000ull, 0x000001ee00f00000ull, { 15, 25, 0, 0, 0 }, 0x40, 110, },
  { 155, 2, 1, 0x0000000e00000000ull, 0x000001ee00f00000ull, { 15, 25, 79, 0, 0 }, 0x0, 2855, },
  { 155, 2, 1, 0x0000000188000000ull, 0x000001eff8000000ull, { 24, 16, 0, 0, 0 }, 0x0, 112, },
  { 155, 2, 1, 0x0000000600000000ull, 0x000001ee00000000ull, { 9, 25, 65, 0, 0 }, 0x0, 113, },
  { 155, 2, 1, 0x00000016ff001fc0ull, 0x000001feff001fc0ull, { 9, 25, 0, 0, 0 }, 0x40, 114, },
  { 155, 2, 1, 0x0000000400000000ull, 0x000001ee00000000ull, { 10, 69, 0, 0, 0 }, 0x0, 115, },
  { 155, 2, 1, 0x0000000180000000ull, 0x000001eff8000000ull, { 24, 8, 0, 0, 0 }, 0x0, 116, },
  { 155, 2, 1, 0x0000000198000000ull, 0x000001eff8000000ull, { 24, 9, 0, 0, 0 }, 0x0, 117, },
  { 155, 2, 1, 0x0000000150000000ull, 0x000001eff8000000ull, { 14, 25, 0, 0, 0 }, 0x0, 1144, },
  { 155, 2, 1, 0x0000000050000000ull, 0x000001eff8000000ull, { 14, 56, 0, 0, 0 }, 0x0, 1145, },
  { 155, 2, 1, 0x0000000190000000ull, 0x000001eff8000000ull, { 24, 14, 0, 0, 0 }, 0x0, 1146, },
  { 155, 3, 1, 0x0000000140000000ull, 0x000001eff8000000ull, { 14, 56, 0, 0, 0 }, 0x0, 1268, },
  { 155, 3, 1, 0x0000002150000000ull, 0x000001eff8000000ull, { 14, 25, 0, 0, 0 }, 0x0, 1269, },
  { 155, 3, 1, 0x0000002110000000ull, 0x000001eff8000000ull, { 24, 14, 0, 0, 0 }, 0x0, 1270, },
  { 155, 3, 1, 0x0000002160000000ull, 0x000001eff8000000ull, { 17, 25, 0, 0, 0 }, 0x8, 118, },
  { 155, 3, 1, 0x0000002120000000ull, 0x000001eff8000000ull, { 24, 17, 0, 0, 0 }, 0x8, 119, },
  { 155, 3, 1, 0x0000002168000000ull, 0x000001eff8000000ull, { 12, 25, 0, 0, 0 }, 0x8, 120, },
  { 155, 3, 1, 0x0000002148000000ull, 0x000001eff8000000ull, { 13, 25, 0, 0, 0 }, 0x0, 121, },
  { 155, 3, 1, 0x0000002128000000ull, 0x000001eff8000000ull, { 24, 11, 0, 0, 0 }, 0x8, 122, },
  { 155, 3, 1, 0x0000002108000000ull, 0x000001eff8000000ull, { 24, 13, 0, 0, 0 }, 0x0, 123, },
  { 155, 3, 1, 0x0000002000000000ull, 0x000001eff8000000ull, { 38, 25, 0, 0, 0 }, 0x8, 124, },
  { 155, 3, 1, 0x0000002008000000ull, 0x000001eff8000000ull, { 30, 25, 0, 0, 0 }, 0x8, 125, },
  { 155, 3, 1, 0x0000002010000000ull, 0x000001eff8000000ull, { 33, 25, 0, 0, 0 }, 0x8, 126, },
  { 155, 3, 1, 0x0000002018000000ull, 0x000001eff8000000ull, { 35, 25, 0, 0, 0 }, 0x8, 127, },
  { 155, 3, 1, 0x0000002020000000ull, 0x000001eff8000000ull, { 36, 25, 0, 0, 0 }, 0x8, 128, },
  { 155, 3, 1, 0x0000002028000000ull, 0x000001eff8000000ull, { 37, 25, 0, 0, 0 }, 0x8, 129, },
  { 155, 3, 1, 0x0000002030000000ull, 0x000001eff8000000ull, { 34, 25, 0, 0, 0 }, 0x8, 130, },
  { 155, 3, 1, 0x0000002080000000ull, 0x000001eff8000000ull, { 24, 38, 0, 0, 0 }, 0x8, 131, },
  { 155, 3, 1, 0x0000002088000000ull, 0x000001eff8000000ull, { 24, 30, 0, 0, 0 }, 0x8, 132, },
  { 155, 3, 1, 0x0000002090000000ull, 0x000001eff8000000ull, { 24, 33, 0, 0, 0 }, 0x8, 133, },
  { 155, 3, 1, 0x0000002098000000ull, 0x000001eff8000000ull, { 24, 35, 0, 0, 0 }, 0x8, 134, },
  { 155, 3, 1, 0x00000020a0000000ull, 0x000001eff8000000ull, { 24, 36, 0, 0, 0 }, 0x8, 135, },
  { 155, 3, 1, 0x00000020a8000000ull, 0x000001eff8000000ull, { 24, 37, 0, 0, 0 }, 0x0, 136, },
  { 155, 3, 1, 0x00000020b0000000ull, 0x000001eff8000000ull, { 24, 34, 0, 0, 0 }, 0x8, 137, },
  { 155, 3, 1, 0x00000020b8000000ull, 0x000001eff8000000ull, { 24, 29, 0, 0, 0 }, 0x0, 138, },
  { 155, 7, 1, 0x0000000000000000ull, 0x0000000000000000ull, { 24, 14, 0, 0, 0 }, 0x0, 139, },
  { 155, 7, 1, 0x0000000000000000ull, 0x0000000000000000ull, { 14, 56, 0, 0, 0 }, 0x0, 140, },
  { 155, 7, 1, 0x0000000000000000ull, 0x0000000000000000ull, { 14, 25, 0, 0, 0 }, 0x0, 141, },
  { 156, 6, 1, 0x000000c000000000ull, 0x000001e000100000ull, { 24, 71, 0, 0, 0 }, 0x0, 142, },
  { 157, 2, 1, 0x000000eca0000000ull, 0x000001fff0000000ull, { 24, 25, 75, 0, 0 }, 0x0, 143, },
  { 158, 2, 1, 0x000000eea0000000ull, 0x000001fff0000000ull, { 24, 25, 76, 0, 0 }, 0x0, 144, },
  { 168, 4, 0, 0x0000004000000000ull, 0x000001e1f8000000ull, { 66, 0, 0, 0, 0 }, 0x0, 539, },
  { 168, 5, 0, 0x0000000008000000ull, 0x000001e3fc000000ull, { 66, 0, 0, 0, 0 }, 0x0, 962, },
  { 168, 2, 0, 0x0000000008000000ull, 0x000001effc000000ull, { 66, 0, 0, 0, 0 }, 0x2, 1147, },
  { 168, 3, 0, 0x0000000008000000ull, 0x000001effc000000ull, { 66, 0, 0, 0, 0 }, 0x0, 1271, },
  { 168, 6, 0, 0x0000000008000000ull, 0x000001effc000000ull, { 70, 0, 0, 0, 0 }, 0x0, 3035, },
  { 168, 7, 0, 0x0000000000000000ull, 0x0000000000000000ull, { 66, 0, 0, 0, 0 }, 0x0, 145, },
  { 175, 1, 1, 0x0000010070000000ull, 0x000001eff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 146, },
  { 175, 1, 1, 0x0000010170000000ull, 0x000001eff8000000ull, { 24, 56, 26, 0, 0 }, 0x0, 147, },
  { 178, 2, 1, 0x000000ea00000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 3017, },
  { 179, 2, 1, 0x000000f820000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 2857, },
  { 180, 1, 1, 0x0000010400000000ull, 0x000001fff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 148, },
  { 181, 1, 1, 0x0000010600000000ull, 0x000001fff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 149, },
  { 182, 1, 1, 0x0000011400000000ull, 0x000001fff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 150, },
  { 183, 1, 1, 0x0000010450000000ull, 0x000001fff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 151, },
  { 184, 1, 1, 0x0000010650000000ull, 0x000001fff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 152, },
  { 185, 1, 1, 0x0000010470000000ull, 0x000001fff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 153, },
  { 186, 1, 1, 0x0000010670000000ull, 0x000001fff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 154, },
  { 187, 1, 1, 0x0000010520000000ull, 0x000001fff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 948, },
  { 188, 1, 1, 0x0000010720000000ull, 0x000001fff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 949, },
  { 189, 1, 1, 0x0000011520000000ull, 0x000001fff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 950, },
  { 190, 2, 1, 0x000000e850000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 2871, },
  { 191, 2, 1, 0x000000ea70000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 155, },
  { 192, 2, 1, 0x000000e810000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 2872, },
  { 193, 2, 1, 0x000000ea30000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 156, },
  { 194, 2, 1, 0x000000ead0000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 2206, },
  { 195, 2, 1, 0x000000e230000000ull, 0x000001ff30000000ull, { 24, 25, 26, 42, 0 }, 0x0, 157, },
  { 196, 2, 1, 0x000000e690000000ull, 0x000001fff0000000ull, { 24, 26, 0, 0, 0 }, 0x0, 158, },
  { 198, 3, 1, 0x00000021c0000000ull, 0x000001eff8000000ull, { 24, 26, 25, 0, 0 }, 0x0, 2207, },
  { 198, 3, 1, 0x00000020c0000000ull, 0x000001eff8000000ull, { 24, 26, 49, 0, 0 }, 0x0, 2208, },
  { 198, 3, 0, 0x0000002188000000ull, 0x000001eff8000000ull, { 26, 49, 0, 0, 0 }, 0x0, 2238, },
  { 199, 2, 1, 0x000000e8b0000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 159, },
  { 200, 2, 1, 0x000000e240000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 160, },
  { 200, 2, 1, 0x000000ee50000000ull, 0x000001fff0000000ull, { 24, 25, 39, 0, 0 }, 0x0, 161, },
  { 201, 2, 1, 0x000000f040000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 162, },
  { 201, 2, 1, 0x000000fc50000000ull, 0x000001fff0000000ull, { 24, 25, 39, 0, 0 }, 0x0, 163, },
  { 202, 1, 1, 0x0000010680000000ull, 0x000001ffe0000000ull, { 24, 25, 41, 26, 0 }, 0x0, 164, },
  { 203, 2, 1, 0x000000e220000000ull, 0x000001fff0000000ull, { 24, 26, 25, 0, 0 }, 0x0, 165, },
  { 203, 2, 1, 0x000000e630000000ull, 0x000001fff0000000ull, { 24, 26, 43, 0, 0 }, 0x0, 166, },
  { 204, 2, 1, 0x000000f020000000ull, 0x000001fff0000000ull, { 24, 26, 25, 0, 0 }, 0x0, 167, },
  { 204, 2, 1, 0x000000f430000000ull, 0x000001fff0000000ull, { 24, 26, 43, 0, 0 }, 0x0, 168, },
  { 205, 1, 1, 0x00000106c0000000ull, 0x000001ffe0000000ull, { 24, 25, 41, 26, 0 }, 0x0, 169, },
  { 206, 1, 1, 0x0000010420000000ull, 0x000001fff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 170, },
  { 207, 1, 1, 0x0000010620000000ull, 0x000001fff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 171, },
  { 208, 1, 1, 0x0000011420000000ull, 0x000001fff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 172, },
  { 209, 3, 0, 0x0000002048000000ull, 0x000001eff8000000ull, { 26, 25, 0, 0, 0 }, 0x8, 1175, },
  { 209, 3, 0, 0x0000002050000000ull, 0x000001eff8000000ull, { 26, 25, 0, 0, 0 }, 0xc, 1050, },
  { 209, 3, 0, 0x00000021a0000000ull, 0x000001eff8000000ull, { 26, 0, 0, 0, 0 }, 0x8, 922, },
  { 210, 3, 0, 0x0000002060000000ull, 0x000001eff8000000ull, { 26, 25, 0, 0, 0 }, 0x8, 848, },
  { 215, 4, 0, 0x0000000040000000ull, 0x000001e1f8000000ull, { 0, 0, 0, 0, 0 }, 0x22c, 173, },
  { 216, 3, 0, 0x0000000038000000ull, 0x000001ee78000000ull, { 68, 0, 0, 0, 0 }, 0x8, 174, },
  { 217, 3, 0, 0x0000000028000000ull, 0x000001ee78000000ull, { 68, 0, 0, 0, 0 }, 0x0, 175, },
  { 226, 3, 1, 0x000000c708000000ull, 0x000001ffc8000000ull, { 18, 25, 0, 0, 0 }, 0x0, 2782, },
  { 227, 2, 1, 0x000000a600000000ull, 0x000001ee04000000ull, { 24, 25, 45, 0, 0 }, 0x140, 176, },
  { 227, 2, 1, 0x000000f240000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 177, },
  { 228, 1, 1, 0x0000010080000000ull, 0x000001efe0000000ull, { 24, 25, 40, 26, 0 }, 0x0, 178, },
  { 229, 1, 1, 0x00000100c0000000ull, 0x000001efe0000000ull, { 24, 25, 40, 26, 0 }, 0x0, 179, },
  { 230, 2, 1, 0x000000a400000000ull, 0x000001ee00002000ull, { 24, 26, 77, 0, 0 }, 0x140, 2878, },
  { 230, 2, 1, 0x000000f220000000ull, 0x000001fff0000000ull, { 24, 26, 25, 0, 0 }, 0x0, 181, },
  { 231, 2, 1, 0x000000ac00000000ull, 0x000001ee00000000ull, { 24, 25, 26, 44, 0 }, 0x0, 182, },
  { 236, 3, 0, 0x0000000180000000ull, 0x000001eff8000000ull, { 0, 0, 0, 0, 0 }, 0x0, 850, },
  { 237, 3, 0, 0x0000000030000000ull, 0x000001ee78000000ull, { 68, 0, 0, 0, 0 }, 0x8, 183, },
  { 239, 3, 1, 0x0000008c00000000ull, 0x000001fff8000000ull, { 28, 25, 0, 0, 0 }, 0x0, 184, },
  { 239, 3, 1, 0x000000ac00000000ull, 0x000001eff0000000ull, { 28, 25, 62, 0, 0 }, 0x400, 185, },
  { 240, 3, 1, 0x0000008c08000000ull, 0x000001fff8000000ull, { 28, 25, 1, 0, 0 }, 0x0, 186, },
  { 240, 3, 1, 0x0000008c08000000ull, 0x000001fff8000000ull, { 28, 25, 0, 0, 0 }, 0x40, 187, },
  { 241, 3, 1, 0x0000008c40000000ull, 0x000001fff8000000ull, { 28, 25, 0, 0, 0 }, 0x0, 188, },
  { 241, 3, 1, 0x000000ac40000000ull, 0x000001eff0000000ull, { 28, 25, 62, 0, 0 }, 0x400, 189, },
  { 242, 3, 1, 0x0000008c80000000ull, 0x000001fff8000000ull, { 28, 25, 0, 0, 0 }, 0x0, 190, },
  { 242, 3, 1, 0x000000ac80000000ull, 0x000001eff0000000ull, { 28, 25, 62, 0, 0 }, 0x400, 191, },
  { 243, 3, 1, 0x0000008cc0000000ull, 0x000001fff8000000ull, { 28, 25, 0, 0, 0 }, 0x0, 192, },
  { 243, 3, 1, 0x000000acc0000000ull, 0x000001eff0000000ull, { 28, 25, 62, 0, 0 }, 0x400, 193, },
  { 244, 3, 1, 0x000000cec0000000ull, 0x000001fff8000000ull, { 28, 19, 0, 0, 0 }, 0x0, 2785, },
  { 244, 3, 1, 0x000000eec0000000ull, 0x000001eff0000000ull, { 28, 19, 62, 0, 0 }, 0x400, 2786, },
  { 245, 3, 1, 0x000000cc40000000ull, 0x000001fff8000000ull, { 28, 19, 0, 0, 0 }, 0x0, 194, },
  { 245, 3, 1, 0x000000ec40000000ull, 0x000001eff0000000ull, { 28, 19, 62, 0, 0 }, 0x400, 195, },
  { 246, 3, 1, 0x000000ccc0000000ull, 0x000001fff8000000ull, { 28, 19, 0, 0, 0 }, 0x0, 196, },
  { 246, 3, 1, 0x000000ecc0000000ull, 0x000001eff0000000ull, { 28, 19, 62, 0, 0 }, 0x400, 197, },
  { 247, 3, 1, 0x000000cc00000000ull, 0x000001fff8000000ull, { 28, 19, 0, 0, 0 }, 0x0, 198, },
  { 247, 3, 1, 0x000000ec00000000ull, 0x000001eff0000000ull, { 28, 19, 62, 0, 0 }, 0x400, 199, },
  { 248, 3, 1, 0x000000cc80000000ull, 0x000001fff8000000ull, { 28, 19, 0, 0, 0 }, 0x0, 200, },
  { 248, 3, 1, 0x000000ec80000000ull, 0x000001eff0000000ull, { 28, 19, 62, 0, 0 }, 0x400, 201, },
  { 249, 1, 1, 0x0000010028000000ull, 0x000001eff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 202, },
  { 249, 1, 1, 0x0000010020000000ull, 0x000001eff8000000ull, { 24, 25, 26, 4, 0 }, 0x0, 203, },
  { 249, 1, 1, 0x0000010128000000ull, 0x000001eff8000000ull, { 24, 56, 26, 0, 0 }, 0x0, 204, },
  { 250, 3, 0, 0x0000000020000000ull, 0x000001ee78000000ull, { 68, 0, 0, 0, 0 }, 0x0, 205, },
  { 251, 2, 1, 0x00000000a0000000ull, 0x000001eff8000000ull, { 24, 26, 0, 0, 0 }, 0x0, 206, },
  { 252, 2, 1, 0x00000000a8000000ull, 0x000001eff8000000ull, { 24, 26, 0, 0, 0 }, 0x0, 207, },
  { 253, 2, 1, 0x00000000b0000000ull, 0x000001eff8000000ull, { 24, 26, 0, 0, 0 }, 0x0, 208, },
  { 254, 3, 0, 0x0000000198000000ull, 0x000001eff8000000ull, { 0, 0, 0, 0, 0 }, 0x0, 1150, },
  { 255, 3, 1, 0x00000020f8000000ull, 0x000001eff8000000ull, { 24, 26, 0, 0, 0 }, 0x8, 209, },
  { 256, 2, 2, 0x000000a000000000ull, 0x000001fe00003000ull, { 22, 23, 26, 77, 0 }, 0x0, 3040, },
  { 256, 2, 1, 0x000000a000000000ull, 0x000001fe00003000ull, { 22, 26, 77, 0, 0 }, 0x40, 3041, },
  { 256, 2, 2, 0x000000a000000000ull, 0x000001fe00003000ull, { 23, 22, 26, 77, 0 }, 0x40, 2003, },
  { 256, 2, 1, 0x000000a000000000ull, 0x000001fe00003000ull, { 23, 26, 77, 0, 0 }, 0x40, 2004, },
  { 257, 2, 2, 0x000000a000082000ull, 0x000001fe00083000ull, { 22, 23, 50, 0, 0 }, 0x0, 3044, },
  { 257, 2, 1, 0x000000a000082000ull, 0x000001fe00083000ull, { 22, 50, 0, 0, 0 }, 0x40, 3045, },
  { 257, 2, 2, 0x000000a000082000ull, 0x000001fe00083000ull, { 23, 22, 50, 0, 0 }, 0x40, 2007, },
  { 257, 2, 1, 0x000000a000082000ull, 0x000001fe00083000ull, { 23, 50, 0, 0, 0 }, 0x40, 2008, },
  { 258, 3, 1, 0x00000020d0000000ull, 0x000001eff8000000ull, { 24, 26, 0, 0, 0 }, 0x0, 210, },
  { 259, 2, 2, 0x000000a000002000ull, 0x000001fe00003000ull, { 22, 23, 26, 0, 0 }, 0x0, 3048, },
  { 259, 2, 1, 0x000000a000002000ull, 0x000001fe00003000ull, { 22, 26, 0, 0, 0 }, 0x40, 3049, },
  { 259, 2, 2, 0x000000a000002000ull, 0x000001fe00003000ull, { 23, 22, 26, 0, 0 }, 0x40, 2011, },
  { 259, 2, 1, 0x000000a000002000ull, 0x000001fe00003000ull, { 23, 26, 0, 0, 0 }, 0x40, 2012, },
  { 260, 3, 1, 0x00000020f0000000ull, 0x000001eff8000000ull, { 24, 26, 0, 0, 0 }, 0x8, 211, },
  { 262, 3, 1, 0x00000020d8000000ull, 0x000001eff8000000ull, { 24, 26, 0, 0, 0 }, 0x0, 212, },
  { 266, 2, 1, 0x000000e840000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 1131, },
  { 267, 2, 1, 0x000000ea40000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 1132, },
  { 268, 2, 1, 0x000000f840000000ull, 0x000001fff0000000ull, { 24, 25, 26, 0, 0 }, 0x0, 1133, },
  { 272, 4, 0, 0x00000000c0000000ull, 0x000001e1f8000000ull, { 0, 0, 0, 0, 0 }, 0x28, 223, },
  { 277, 3, 1, 0x0000008208000000ull, 0x000001fff8000000ull, { 24, 28, 25, 0, 0 }, 0x0, 213, },
  { 278, 3, 1, 0x0000008248000000ull, 0x000001fff8000000ull, { 24, 28, 25, 0, 0 }, 0x0, 214, },
  { 279, 3, 1, 0x0000008288000000ull, 0x000001fff8000000ull, { 24, 28, 25, 0, 0 }, 0x0, 215, },
  { 280, 3, 1, 0x00000082c8000000ull, 0x000001fff8000000ull, { 24, 28, 25, 0, 0 }, 0x0, 216, },
  { 282, 5, 1, 0x000001d000000000ull, 0x000001fc00000000ull, { 18, 20, 21, 19, 0 }, 0x0, 1179, },
  { 282, 5, 1, 0x000001d000000000ull, 0x000001fc00000000ull, { 18, 20, 21, 19, 0 }, 0x40, 1261, },
  { 283, 5, 1, 0x000001d000000000ull, 0x000001fc000fe000ull, { 18, 20, 21, 0, 0 }, 0x40, 1180, },
  { 284, 1, 1, 0x0000010078000000ull, 0x000001eff8000000ull, { 24, 25, 26, 0, 0 }, 0x0, 217, },
  { 284, 1, 1, 0x0000010178000000ull, 0x000001eff8000000ull, { 24, 56, 26, 0, 0 }, 0x0, 218, },
  { 287, 2, 1, 0x0000000080000000ull, 0x000001eff8000000ull, { 24, 26, 0, 0, 0 }, 0x0, 219, },
  { 288, 2, 1, 0x0000000088000000ull, 0x000001eff8000000ull, { 24, 26, 0, 0, 0 }, 0x0, 220, },
  { 289, 2, 1, 0x0000000090000000ull, 0x000001eff8000000ull, { 24, 26, 0, 0, 0 }, 0x0, 221, },
};

static const char dis_table[] = {
0xa0, 0xc7, 0xc8, 0xa0, 0x2e, 0xd8, 0xa0, 0x2c, 0xc0, 0xa0, 0x1c, 0x00,
0x98, 0xb0, 0x02, 0x50, 0x90, 0x50, 0x90, 0x28, 0x24, 0x39, 0x28, 0x24,
0x39, 0x20, 0x90, 0x28, 0x24, 0x39, 0x18, 0x24, 0x39, 0x10, 0x91, 0x60,
0x90, 0x28, 0x24, 0x39, 0x00, 0x10, 0x10, 0x58, 0x41, 0x61, 0xc7, 0xc0,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x10, 0x10, 0x52, 0xc0, 0xc0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x10, 0x10, 0x10, 0x24, 0x24, 0x70, 0x90, 0x28, 0x24, 0x38, 0xf0, 0x24,
0x38, 0xe8, 0xa8, 0x0b, 0x48, 0x15, 0x20, 0x97, 0x20, 0x95, 0xc8, 0x9a,
0xb8, 0x05, 0x38, 0x91, 0x18, 0x90, 0xa0, 0x90, 0x60, 0x80, 0x90, 0x20,
0x34, 0xa6, 0xa4, 0x25, 0x00, 0x34, 0xa3, 0x80, 0xa4, 0x36, 0xa0, 0x36,
0xd9, 0x90, 0x50, 0x90, 0x28, 0x80, 0x36, 0xcf, 0x80, 0x34, 0x86, 0x81,
0x33, 0xe2, 0x90, 0xe0, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x24, 0x10, 0x34,
0x83, 0xa4, 0x1f, 0x08, 0x34, 0x80, 0x90, 0x38, 0xa4, 0x38, 0xa0, 0x37,
0x1a, 0xa4, 0x38, 0x48, 0x37, 0x0e, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x37,
0x20, 0x36, 0xef, 0xa4, 0x36, 0xf8, 0x36, 0xea, 0x80, 0xa4, 0x23, 0xf0,
0x34, 0x7f, 0x92, 0x18, 0x91, 0xc0, 0x80, 0x91, 0x80, 0x90, 0xf8, 0xdb,
0x84, 0x60, 0xf9, 0x40, 0xc0, 0xc0, 0x80, 0xa4, 0x42, 0x68, 0x8c, 0x43,
0xc8, 0x84, 0x38, 0x83, 0xc0, 0xc0, 0x80, 0xa4, 0x42, 0x58, 0x8c, 0x43,
0xa8, 0x84, 0x38, 0x81, 0xd3, 0x82, 0x40, 0x50, 0xc0, 0xc0, 0x81, 0x38,
0x35, 0x50, 0xc0, 0xc0, 0x81, 0x38, 0x33, 0xa4, 0x1f, 0x18, 0x33, 0xe4,
0x80, 0x90, 0x28, 0x80, 0x33, 0xe0, 0x80, 0x34, 0x88, 0x81, 0x90, 0x38,
0xa4, 0x24, 0x80, 0x34, 0x8b, 0xa4, 0x24, 0x48, 0x34, 0x85, 0xc0, 0x40,
0x10, 0x10, 0x90, 0x38, 0xa4, 0x1e, 0xf0, 0x33, 0xdf, 0xa4, 0x1e, 0xe0,
0x33, 0xdd, 0x18, 0x24, 0x24, 0xf8, 0x83, 0x90, 0xa8, 0xd3, 0x82, 0xc0,
0xc0, 0xc0, 0x80, 0xa4, 0x42, 0x38, 0x38, 0x6d, 0xc0, 0xc0, 0x80, 0xa4,
0x42, 0x28, 0x38, 0x69, 0xd3, 0x82, 0x40, 0x50, 0xc0, 0xc0, 0x81, 0x38,
0x2f, 0x50, 0xc0, 0xc0, 0x81, 0x38, 0x2d, 0x92, 0xb8, 0x99, 0x84, 0x24,
0x68, 0x90, 0x78, 0x90, 0x50, 0x10, 0x10, 0x80, 0xa4, 0x36, 0x98, 0x36,
0xd8, 0x82, 0x36, 0xce, 0x90, 0x80, 0x10, 0x10, 0x90, 0x38, 0xa4, 0x38,
0x98, 0x37, 0x19, 0xa4, 0x38, 0x40, 0x37, 0x0d, 0x80, 0x90, 0x38, 0xa4,
0x37, 0x18, 0x36, 0xee, 0xa4, 0x36, 0xf0, 0x36, 0xe9, 0x83, 0x90, 0xa8,
0xd3, 0x82, 0xc0, 0xc0, 0xc0, 0x80, 0xa4, 0x42, 0x08, 0x38, 0x61, 0xc0,
0xc0, 0x80, 0xa4, 0x41, 0xf8, 0x38, 0x5d, 0xd3, 0x82, 0x40, 0x50, 0xc0,
0xc0, 0x81, 0x38, 0x29, 0x50, 0xc0, 0xc0, 0x81, 0x38, 0x27, 0x18, 0x24,
0x24, 0x78, 0x83, 0x90, 0xa8, 0xd3, 0x82, 0xc0, 0xc0, 0xc0, 0x80, 0xa4,
0x41, 0xd8, 0x38, 0x55, 0xc0, 0xc0, 0x80, 0xa4, 0x41, 0xc8, 0x38, 0x51,
0xd3, 0x82, 0x40, 0x50, 0xc0, 0xc0, 0x81, 0x38, 0x23, 0x50, 0xc0, 0xc0,
0x81, 0x38, 0x21, 0x94, 0x50, 0x92, 0xf8, 0x99, 0x84, 0x1f, 0x48, 0x90,
0x78, 0x90, 0x50, 0x10, 0x10, 0x80, 0xa4, 0x36, 0x90, 0x36, 0xd7, 0x82,
0x36, 0xcd, 0x90, 0x80, 0x10, 0x10, 0x90, 0x38, 0xa4, 0x38, 0x90, 0x37,
0x18, 0xa4, 0x38, 0x38, 0x37, 0x0c, 0x80, 0x90, 0x38, 0xa4, 0x37, 0x10,
0x36, 0xed, 0xa4, 0x36, 0xe8, 0x36, 0xe8, 0x83, 0x90, 0xe8, 0xd3, 0x83,
0xc0, 0xc0, 0xc0, 0x80, 0xa4, 0x42, 0x78, 0x8c, 0x43, 0xe8, 0x84, 0x38,
0x85, 0xc0, 0xc0, 0x80, 0xa4, 0x42, 0x60, 0x8c, 0x43, 0xb8, 0x84, 0x38,
0x82, 0xd3, 0x82, 0x40, 0x50, 0xc0, 0xc0, 0x81, 0x38, 0x37, 0x50, 0xc0,
0xc0, 0x81, 0x38, 0x34, 0x18, 0x24, 0x1f, 0x40, 0x83, 0x90, 0xa8, 0xd3,
0x82, 0xc0, 0xc0, 0xc0, 0x80, 0xa4, 0x42, 0x48, 0x38, 0x71, 0xc0, 0xc0,
0x80, 0xa4, 0x42, 0x30, 0x38, 0x6b, 0xd3, 0x82, 0x40, 0x50, 0xc0, 0xc0,
0x81, 0x38, 0x31, 0x50, 0xc0, 0xc0, 0x81, 0x38, 0x2e, 0x92, 0xb8, 0x99,
0x84, 0x1f, 0x38, 0x90, 0x78, 0x90, 0x50, 0x10, 0x10, 0x80, 0xa4, 0x36,
0x88, 0x36, 0xd6, 0x82, 0x36, 0xcc, 0x90, 0x80, 0x10, 0x10, 0x90, 0x38,
0xa4, 0x38, 0x88, 0x37, 0x17, 0xa4, 0x38, 0x30, 0x37, 0x0b, 0x80, 0x90,
0x38, 0xa4, 0x37, 0x08, 0x36, 0xec, 0xa4, 0x36, 0xe0, 0x36, 0xe7, 0x83,
0x90, 0xa8, 0xd3, 0x82, 0xc0, 0xc0, 0xc0, 0x80, 0xa4, 0x42, 0x18, 0x38,
0x65, 0xc0, 0xc0, 0x80, 0xa4, 0x42, 0x00, 0x38, 0x5f, 0xd3, 0x82, 0x40,
0x50, 0xc0, 0xc0, 0x81, 0x38, 0x2b, 0x50, 0xc0, 0xc0, 0x81, 0x38, 0x28,
0x18, 0x20, 0x01, 0x48, 0x83, 0x90, 0xa8, 0xd3, 0x82, 0xc0, 0xc0, 0xc0,
0x80, 0xa4, 0x41, 0xe8, 0x38, 0x59, 0xc0, 0xc0, 0x80, 0xa4, 0x41, 0xd0,
0x38, 0x53, 0xd3, 0x82, 0x40, 0x50, 0xc0, 0xc0, 0x81, 0x38, 0x25, 0x50,
0xc0, 0xc0, 0x81, 0x38, 0x22, 0xda, 0x06, 0xe0, 0xf9, 0x80, 0x90, 0x60,
0x90, 0x38, 0xa4, 0x24, 0xe8, 0x34, 0x9b, 0x80, 0x34, 0x98, 0x90, 0x38,
0xa4, 0x24, 0x90, 0x34, 0x96, 0x80, 0x34, 0x93, 0x90, 0x60, 0x90, 0x38,
0xa4, 0x24, 0xd0, 0x34, 0x9c, 0x80, 0x34, 0x99, 0x90, 0x38, 0xa4, 0x24,
0xa8, 0x34, 0x97, 0x80, 0x34, 0x94, 0xc8, 0x40, 0x19, 0x00, 0x91, 0x58,
0x90, 0x60, 0x82, 0x90, 0x20, 0x36, 0xcb, 0xa4, 0x36, 0x48, 0x36, 0xca,
0x90, 0xc0, 0x80, 0x90, 0x90, 0x90, 0x48, 0xc9, 0xe1, 0xc1, 0x00, 0x85,
0x37, 0x03, 0xc9, 0xe1, 0xc0, 0x40, 0x85, 0x37, 0x00, 0x80, 0x36, 0xff,
0x10, 0x10, 0x81, 0x36, 0xdb, 0x90, 0xa8, 0x10, 0x10, 0x90, 0x28, 0x81,
0x36, 0xf9, 0x90, 0x38, 0xa4, 0x37, 0xa0, 0x36, 0xf5, 0xa4, 0x37, 0x90,
0x36, 0xf3, 0x90, 0x70, 0x10, 0x10, 0x90, 0x38, 0xa4, 0x37, 0xb8, 0x36,
0xf8, 0x80, 0x36, 0xf6, 0x90, 0x60, 0x90, 0x28, 0x24, 0x37, 0xf0, 0xa4,
0x37, 0xe0, 0x36, 0xfd, 0x80, 0xa4, 0x37, 0xd0, 0x36, 0xfb, 0x80, 0x90,
0xf8, 0x90, 0x90, 0x90, 0x50, 0x90, 0x28, 0x80, 0x38, 0x17, 0x80, 0x38,
0x20, 0x80, 0xa4, 0x40, 0xf0, 0x38, 0x1f, 0x90, 0x28, 0x81, 0x38, 0x1d,
0x80, 0xa4, 0x40, 0xd8, 0x38, 0x1c, 0x90, 0x28, 0x82, 0x38, 0x1a, 0x81,
0xa4, 0x40, 0xc0, 0x38, 0x19, 0x98, 0xe8, 0x01, 0xb0, 0x90, 0x88, 0x90,
0x60, 0xa4, 0x36, 0x38, 0x10, 0x10, 0x10, 0x10, 0x83, 0x33, 0xb7, 0x24,
0x36, 0x30, 0x90, 0x28, 0x24, 0x36, 0x28, 0x24, 0x36, 0x20, 0x90, 0x88,
0x90, 0x60, 0xa4, 0x36, 0x10, 0x10, 0x10, 0x10, 0x10, 0x83, 0x33, 0xb6,
0x24, 0x36, 0x08, 0x90, 0x28, 0x24, 0x36, 0x00, 0x24, 0x35, 0xf8, 0xa8,
0x09, 0x00, 0x0e, 0x20, 0x96, 0x48, 0x95, 0xe8, 0x93, 0x38, 0x91, 0xa0,
0x90, 0xd0, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x1e, 0x60, 0x33, 0xcd, 0xa4,
0x1e, 0x50, 0x33, 0xcb, 0x90, 0x38, 0xa4, 0x1e, 0x40, 0x33, 0xc9, 0x80,
0x33, 0xc7, 0x90, 0x60, 0x90, 0x28, 0x24, 0x1e, 0x00, 0xa4, 0x1d, 0xf0,
0x33, 0xbf, 0x90, 0x38, 0xa4, 0x1d, 0xe0, 0x33, 0xbd, 0xa4, 0x1e, 0x28,
0x33, 0xc6, 0x90, 0xe0, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x1e, 0x18, 0x33,
0xc4, 0xa4, 0x1e, 0x08, 0x33, 0xc2, 0x90, 0x38, 0xa4, 0x35, 0xb0, 0x36,
0xbc, 0xa4, 0x35, 0x50, 0x36, 0xb0, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x32,
0x90, 0x36, 0x5e, 0xa4, 0x32, 0x60, 0x36, 0x58, 0x10, 0x10, 0xa4, 0x1d,
0xd0, 0x33, 0xbb, 0x99, 0x60, 0x02, 0x70, 0x90, 0x90, 0x90, 0x50, 0x90,
0x28, 0x24, 0x1e, 0x90, 0x80, 0x33, 0xda, 0x80, 0xa4, 0x1e, 0x98, 0x33,
0xd8, 0x90, 0x50, 0x90, 0x28, 0x24, 0x1e, 0xa0, 0x80, 0x33, 0xdb, 0x90,
0x38, 0xa4, 0x1e, 0xa8, 0x33, 0xd9, 0xa4, 0x1e, 0x70, 0x33, 0xcf, 0x90,
0xe0, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x34, 0xe8, 0x36, 0xa5, 0xa4, 0x34,
0x48, 0x36, 0x92, 0x90, 0x38, 0xa4, 0x33, 0xe0, 0x36, 0x83, 0xa4, 0x33,
0x50, 0x36, 0x72, 0x81, 0xa4, 0x1e, 0x80, 0x33, 0xd1, 0xe4, 0xa2, 0x04,
0x40, 0x38, 0x13, 0x18, 0x24, 0x1d, 0xc8, 0xe4, 0xe2, 0x02, 0xc0, 0x38,
0x0d, 0x92, 0x40, 0x91, 0x08, 0x10, 0x10, 0x90, 0x80, 0x10, 0x10, 0x90,
0x38, 0xa4, 0x35, 0xa8, 0x36, 0xbb, 0xa4, 0x35, 0x48, 0x36, 0xaf, 0x80,
0x90, 0x38, 0xa4, 0x32, 0x88, 0x36, 0x5d, 0xa4, 0x32, 0x58, 0x36, 0x57,
0x18, 0x20, 0x00, 0xf8, 0x80, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x34, 0xd8,
0x36, 0xa4, 0xa4, 0x34, 0x40, 0x36, 0x90, 0x90, 0x38, 0xa4, 0x33, 0xd0,
0x36, 0x82, 0xa4, 0x33, 0x48, 0x36, 0x70, 0xe4, 0xa2, 0x01, 0x40, 0x38,
0x07, 0x18, 0x24, 0x1d, 0xc0, 0xe4, 0xe1, 0xff, 0xc0, 0x38, 0x01, 0x92,
0x90, 0x92, 0x40, 0x91, 0x08, 0x10, 0x10, 0x90, 0x80, 0x10, 0x10, 0x90,
0x38, 0xa4, 0x35, 0xa0, 0x36, 0xba, 0xa4, 0x35, 0x40, 0x36, 0xae, 0x80,
0x90, 0x38, 0xa4, 0x32, 0x80, 0x36, 0x5c, 0xa4, 0x32, 0x50, 0x36, 0x56,
0x18, 0x20, 0x00, 0xf8, 0x80, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x34, 0xc8,
0x36, 0xa3, 0xa4, 0x34, 0x38, 0x36, 0x8e, 0x90, 0x38, 0xa4, 0x33, 0xc0,
0x36, 0x81, 0xa4, 0x33, 0x40, 0x36, 0x6e, 0xe4, 0xa2, 0x04, 0x80, 0x38,
0x15, 0x10, 0x10, 0xe4, 0xe2, 0x03, 0x00, 0x38, 0x0f, 0x92, 0x50, 0x99,
0x1c, 0x1e, 0xb0, 0x10, 0x10, 0x90, 0x80, 0x10, 0x10, 0x90, 0x38, 0xa4,
0x35, 0x98, 0x36, 0xb9, 0xa4, 0x35, 0x38, 0x36, 0xad, 0x80, 0x90, 0x38,
0xa4, 0x32, 0x78, 0x36, 0x5b, 0xa4, 0x32, 0x48, 0x36, 0x55, 0x18, 0x20,
0x00, 0xf8, 0x80, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x34, 0xb8, 0x36, 0xa2,
0xa4, 0x34, 0x30, 0x36, 0x8c, 0x90, 0x38, 0xa4, 0x33, 0xb0, 0x36, 0x80,
0xa4, 0x33, 0x38, 0x36, 0x6c, 0xe4, 0xa2, 0x01, 0x80, 0x38, 0x09, 0x10,
0x10, 0xe4, 0xe2, 0x00, 0x00, 0x38, 0x03, 0xc0, 0x40, 0x80, 0x10, 0x10,
0x81, 0x90, 0x90, 0x90, 0x48, 0xc9, 0xe1, 0x98, 0x80, 0x85, 0x36, 0x66,
0xc9, 0xe1, 0x99, 0x00, 0x85, 0x36, 0x63, 0x80, 0x36, 0x61, 0x80, 0xd8,
0x47, 0x80, 0x0d, 0xc0, 0xc0, 0x80, 0x10, 0x10, 0x82, 0x90, 0x58, 0xd5,
0x81, 0x80, 0x80, 0x37, 0xfd, 0x80, 0x37, 0xfb, 0xd5, 0x81, 0x80, 0x80,
0x37, 0xf9, 0x80, 0x37, 0xf7, 0xc0, 0x80, 0x10, 0x10, 0x82, 0x90, 0x58,
0xd5, 0x81, 0x80, 0x80, 0x37, 0xfe, 0x80, 0x37, 0xfc, 0xd5, 0x81, 0x80,
0x80, 0x37, 0xfa, 0x80, 0x37, 0xf8, 0xc0, 0x80, 0x83, 0xa4, 0x3f, 0xa8,
0x37, 0xf6, 0xa0, 0x59, 0x60, 0xa0, 0x41, 0xe0, 0xa8, 0x1e, 0xb0, 0x34,
0x88, 0xa0, 0x12, 0x38, 0xa0, 0x0b, 0x48, 0x96, 0x00, 0x9a, 0xf0, 0x05,
0xc0, 0x91, 0x70, 0x90, 0xb8, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x15, 0x58,
0x33, 0xb5, 0xa4, 0x15, 0x78, 0x33, 0xb4, 0x10, 0x10, 0xa4, 0x15, 0x68,
0x33, 0xb3, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x14, 0xf8, 0x33, 0x9a, 0xa4,
0x15, 0x18, 0x33, 0x99, 0x10, 0x10, 0xa4, 0x15, 0x08, 0x33, 0x98, 0x90,
0xb8, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x14, 0x98, 0x33, 0x7f, 0xa4, 0x14,
0xb8, 0x33, 0x7e, 0x10, 0x10, 0xa4, 0x14, 0xa8, 0x33, 0x7d, 0x90, 0x70,
0x90, 0x38, 0xa4, 0x14, 0x38, 0x33, 0x63, 0xa4, 0x14, 0x58, 0x33, 0x62,
0x10, 0x10, 0xa4, 0x14, 0x48, 0x33, 0x61, 0x91, 0x70, 0x90, 0xb8, 0x90,
0x70, 0x90, 0x38, 0xa4, 0x15, 0x28, 0x33, 0xb0, 0xa4, 0x15, 0x48, 0x33,
0xb2, 0x10, 0x10, 0xa4, 0x15, 0x38, 0x33, 0xb1, 0x90, 0x70, 0x90, 0x38,
0xa4, 0x14, 0xc8, 0x33, 0x95, 0xa4, 0x14, 0xe8, 0x33, 0x97, 0x10, 0x10,
0xa4, 0x14, 0xd8, 0x33, 0x96, 0x90, 0xb8, 0x90, 0x70, 0x90, 0x38, 0xa4,
0x14, 0x68, 0x33, 0x7a, 0xa4, 0x14, 0x88, 0x33, 0x7c, 0x10, 0x10, 0xa4,
0x14, 0x78, 0x33, 0x7b, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x14, 0x08, 0x33,
0x5e, 0xa4, 0x14, 0x28, 0x33, 0x60, 0x10, 0x10, 0xa4, 0x14, 0x18, 0x33,
0x5f, 0xe4, 0xe1, 0x8b, 0x40, 0x36, 0x41, 0x9a, 0xf0, 0x05, 0x00, 0x91,
0x70, 0x90, 0xb8, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x13, 0xa0, 0x33, 0xad,
0xa4, 0x13, 0x98, 0x33, 0xaf, 0x10, 0x10, 0xa4, 0x13, 0x90, 0x33, 0xae,
0x90, 0x70, 0x90, 0x38, 0xa4, 0x13, 0x88, 0x33, 0x92, 0xa4, 0x13, 0x80,
0x33, 0x94, 0x10, 0x10, 0xa4, 0x13, 0x78, 0x33, 0x93, 0x90, 0xb8, 0x90,
0x70, 0x90, 0x38, 0xa4, 0x13, 0x70, 0x33, 0x77, 0xa4, 0x13, 0x68, 0x33,
0x79, 0x10, 0x10, 0xa4, 0x13, 0x60, 0x33, 0x78, 0x90, 0x70, 0x90, 0x38,
0xa4, 0x13, 0x58, 0x33, 0x5b, 0xa4, 0x13, 0x50, 0x33, 0x5d, 0x10, 0x10,
0xa4, 0x13, 0x48, 0x33, 0x5c, 0x91, 0x10, 0x90, 0x88, 0x90, 0x50, 0x90,
0x28, 0x80, 0x33, 0xaa, 0x80, 0x33, 0xac, 0x10, 0x10, 0x80, 0x33, 0xab,
0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x8f, 0x80, 0x33, 0x91, 0x10, 0x10,
0x80, 0x33, 0x90, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x74,
0x80, 0x33, 0x76, 0x10, 0x10, 0x80, 0x33, 0x75, 0x90, 0x50, 0x90, 0x28,
0x80, 0x33, 0x58, 0x80, 0x33, 0x5a, 0x10, 0x10, 0x80, 0x33, 0x59, 0xe4,
0xe1, 0x66, 0x40, 0x35, 0xc1, 0x95, 0x40, 0x9a, 0x90, 0x05, 0x00, 0x91,
0x10, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0xa7, 0x80, 0x33,
0xa9, 0x10, 0x10, 0x80, 0x33, 0xa8, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33,
0x8c, 0x80, 0x33, 0x8e, 0x10, 0x10, 0x80, 0x33, 0x8d, 0x90, 0xb8, 0x90,
0x70, 0x90, 0x38, 0xa4, 0x13, 0x30, 0x33, 0x71, 0xa4, 0x13, 0x40, 0x33,
0x73, 0x10, 0x10, 0xa4, 0x13, 0x38, 0x33, 0x72, 0x90, 0x70, 0x90, 0x38,
0xa4, 0x13, 0x00, 0x33, 0x55, 0xa4, 0x13, 0x10, 0x33, 0x57, 0x10, 0x10,
0xa4, 0x13, 0x08, 0x33, 0x56, 0x91, 0x10, 0x90, 0x88, 0x90, 0x50, 0x90,
0x28, 0x80, 0x33, 0xa4, 0x80, 0x33, 0xa6, 0x10, 0x10, 0x80, 0x33, 0xa5,
0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x89, 0x80, 0x33, 0x8b, 0x10, 0x10,
0x80, 0x33, 0x8a, 0x90, 0xb8, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x13, 0x18,
0x33, 0x6e, 0xa4, 0x13, 0x28, 0x33, 0x70, 0x10, 0x10, 0xa4, 0x13, 0x20,
0x33, 0x6f, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x12, 0xe8, 0x33, 0x52, 0xa4,
0x12, 0xf8, 0x33, 0x54, 0x10, 0x10, 0xa4, 0x12, 0xf0, 0x33, 0x53, 0xe4,
0xe1, 0x8a, 0x40, 0x36, 0x3d, 0x98, 0xb8, 0x01, 0x68, 0x10, 0x10, 0x10,
0x10, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x4f, 0x80, 0x33, 0x51, 0x10,
0x10, 0x80, 0x33, 0x50, 0x90, 0x60, 0x90, 0x30, 0x60, 0xa0, 0x97, 0x00,
0x60, 0xa0, 0x96, 0xc0, 0x90, 0x30, 0x60, 0xa0, 0x96, 0x80, 0x60, 0xa0,
0x96, 0x40, 0xe4, 0xe1, 0x64, 0x40, 0x35, 0xb9, 0xa0, 0x08, 0x08, 0x94,
0xe0, 0x9a, 0x60, 0x04, 0xa0, 0x91, 0x40, 0x90, 0xb8, 0x90, 0x70, 0x90,
0x38, 0xa4, 0x13, 0xd8, 0x33, 0x9e, 0xa4, 0x13, 0xf8, 0x33, 0xa3, 0x10,
0x10, 0xa4, 0x13, 0xe8, 0x33, 0xa2, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33,
0x83, 0x80, 0x33, 0x88, 0x10, 0x10, 0x80, 0x33, 0x87, 0x90, 0x88, 0x90,
0x50, 0x90, 0x28, 0x80, 0x33, 0x68, 0x80, 0x33, 0x6d, 0x10, 0x10, 0x80,
0x33, 0x6c, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x49, 0x80, 0x33, 0x4e,
0x10, 0x10, 0x80, 0x33, 0x4d, 0x91, 0x40, 0x90, 0xb8, 0x90, 0x70, 0x90,
0x38, 0xa4, 0x13, 0xa8, 0x33, 0x9b, 0xa4, 0x13, 0xc8, 0x33, 0x9d, 0x10,
0x10, 0xa4, 0x13, 0xb8, 0x33, 0x9c, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33,
0x80, 0x80, 0x33, 0x82, 0x10, 0x10, 0x80, 0x33, 0x81, 0x90, 0x88, 0x90,
0x50, 0x90, 0x28, 0x80, 0x33, 0x65, 0x80, 0x33, 0x67, 0x10, 0x10, 0x80,
0x33, 0x66, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x46, 0x80, 0x33, 0x48,
0x10, 0x10, 0x80, 0x33, 0x47, 0xe4, 0xe1, 0x89, 0x40, 0x36, 0x39, 0x9a,
0x60, 0x02, 0xe0, 0x91, 0x40, 0x90, 0xb8, 0x90, 0x70, 0x90, 0x38, 0xa4,
0x1a, 0x20, 0x33, 0x9f, 0xa4, 0x1a, 0x10, 0x33, 0xa1, 0x10, 0x10, 0xa4,
0x1a, 0x00, 0x33, 0xa0, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x84, 0x80,
0x33, 0x86, 0x10, 0x10, 0x80, 0x33, 0x85, 0x90, 0x88, 0x90, 0x50, 0x90,
0x28, 0x80, 0x33, 0x69, 0x80, 0x33, 0x6b, 0x10, 0x10, 0x80, 0x33, 0x6a,
0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x4a, 0x80, 0x33, 0x4c, 0x10, 0x10,
0x80, 0x33, 0x4b, 0x81, 0x90, 0x50, 0x90, 0x28, 0x24, 0x19, 0xd0, 0x24,
0x19, 0xf0, 0x10, 0x10, 0x24, 0x19, 0xe0, 0xe4, 0xe1, 0x62, 0x40, 0x35,
0xb1, 0x93, 0x90, 0x99, 0xb8, 0x03, 0x50, 0x90, 0xe8, 0x90, 0x88, 0x90,
0x40, 0x80, 0xa4, 0x15, 0xb8, 0x32, 0xca, 0x10, 0x10, 0xa4, 0x15, 0xa8,
0x32, 0xc9, 0x90, 0x28, 0x81, 0x32, 0xc6, 0x10, 0x10, 0x80, 0x32, 0xc5,
0x90, 0x60, 0x90, 0x28, 0x81, 0x32, 0xc2, 0x10, 0x10, 0x80, 0x32, 0xc1,
0x90, 0x28, 0x81, 0x32, 0xbe, 0x10, 0x10, 0x80, 0x32, 0xbd, 0x90, 0xe8,
0x90, 0x88, 0x90, 0x40, 0x80, 0xa4, 0x15, 0x88, 0x32, 0xc7, 0x10, 0x10,
0xa4, 0x15, 0x98, 0x32, 0xc8, 0x90, 0x28, 0x81, 0x32, 0xc3, 0x10, 0x10,
0x80, 0x32, 0xc4, 0x90, 0x60, 0x90, 0x28, 0x81, 0x32, 0xbf, 0x10, 0x10,
0x80, 0x32, 0xc0, 0x90, 0x28, 0x81, 0x32, 0xbb, 0x10, 0x10, 0x80, 0x32,
0xbc, 0xe4, 0xe1, 0x88, 0x40, 0x36, 0x35, 0x88, 0x00, 0x88, 0x10, 0x10,
0x10, 0x10, 0x90, 0x28, 0x81, 0x32, 0xb9, 0x10, 0x10, 0x80, 0x32, 0xba,
0xe4, 0xe1, 0x60, 0x40, 0x35, 0xa9, 0xa0, 0x0e, 0x80, 0xa0, 0x09, 0x08,
0x94, 0x80, 0x9a, 0x30, 0x04, 0x40, 0x91, 0x10, 0x90, 0x88, 0x90, 0x50,
0x90, 0x28, 0x80, 0x33, 0x39, 0x80, 0x33, 0x38, 0x10, 0x10, 0x80, 0x33,
0x37, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x1e, 0x80, 0x33, 0x1d, 0x10,
0x10, 0x80, 0x33, 0x1c, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33,
0x03, 0x80, 0x33, 0x02, 0x10, 0x10, 0x80, 0x33, 0x01, 0x90, 0x50, 0x90,
0x28, 0x80, 0x32, 0xe8, 0x80, 0x32, 0xe7, 0x10, 0x10, 0x80, 0x32, 0xe6,
0x91, 0x10, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x34, 0x80,
0x33, 0x36, 0x10, 0x10, 0x80, 0x33, 0x35, 0x90, 0x50, 0x90, 0x28, 0x80,
0x33, 0x19, 0x80, 0x33, 0x1b, 0x10, 0x10, 0x80, 0x33, 0x1a, 0x90, 0x88,
0x90, 0x50, 0x90, 0x28, 0x80, 0x32, 0xfe, 0x80, 0x33, 0x00, 0x10, 0x10,
0x80, 0x32, 0xff, 0x90, 0x50, 0x90, 0x28, 0x80, 0x32, 0xe3, 0x80, 0x32,
0xe5, 0x10, 0x10, 0x80, 0x32, 0xe4, 0xe4, 0xe1, 0x7a, 0x40, 0x36, 0x11,
0x9a, 0x30, 0x04, 0x40, 0x91, 0x10, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28,
0x80, 0x33, 0x31, 0x80, 0x33, 0x33, 0x10, 0x10, 0x80, 0x33, 0x32, 0x90,
0x50, 0x90, 0x28, 0x80, 0x33, 0x16, 0x80, 0x33, 0x18, 0x10, 0x10, 0x80,
0x33, 0x17, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80, 0x32, 0xfb, 0x80,
0x32, 0xfd, 0x10, 0x10, 0x80, 0x32, 0xfc, 0x90, 0x50, 0x90, 0x28, 0x80,
0x32, 0xe0, 0x80, 0x32, 0xe2, 0x10, 0x10, 0x80, 0x32, 0xe1, 0x91, 0x10,
0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x2e, 0x80, 0x33, 0x30,
0x10, 0x10, 0x80, 0x33, 0x2f, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x13,
0x80, 0x33, 0x15, 0x10, 0x10, 0x80, 0x33, 0x14, 0x90, 0x88, 0x90, 0x50,
0x90, 0x28, 0x80, 0x32, 0xf8, 0x80, 0x32, 0xfa, 0x10, 0x10, 0x80, 0x32,
0xf9, 0x90, 0x50, 0x90, 0x28, 0x80, 0x32, 0xdd, 0x80, 0x32, 0xdf, 0x10,
0x10, 0x80, 0x32, 0xde, 0xe4, 0xe1, 0x59, 0x40, 0x35, 0x79, 0x94, 0x80,
0x9a, 0x30, 0x04, 0x40, 0x91, 0x10, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28,
0x80, 0x33, 0x2b, 0x80, 0x33, 0x2d, 0x10, 0x10, 0x80, 0x33, 0x2c, 0x90,
0x50, 0x90, 0x28, 0x80, 0x33, 0x10, 0x80, 0x33, 0x12, 0x10, 0x10, 0x80,
0x33, 0x11, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80, 0x32, 0xf5, 0x80,
0x32, 0xf7, 0x10, 0x10, 0x80, 0x32, 0xf6, 0x90, 0x50, 0x90, 0x28, 0x80,
0x32, 0xda, 0x80, 0x32, 0xdc, 0x10, 0x10, 0x80, 0x32, 0xdb, 0x91, 0x10,
0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x28, 0x80, 0x33, 0x2a,
0x10, 0x10, 0x80, 0x33, 0x29, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x0d,
0x80, 0x33, 0x0f, 0x10, 0x10, 0x80, 0x33, 0x0e, 0x90, 0x88, 0x90, 0x50,
0x90, 0x28, 0x80, 0x32, 0xf2, 0x80, 0x32, 0xf4, 0x10, 0x10, 0x80, 0x32,
0xf3, 0x90, 0x50, 0x90, 0x28, 0x80, 0x32, 0xd7, 0x80, 0x32, 0xd9, 0x10,
0x10, 0x80, 0x32, 0xd8, 0xe4, 0xe1, 0x78, 0x40, 0x36, 0x09, 0x88, 0x00,
0xb0, 0x10, 0x10, 0x10, 0x10, 0x90, 0x50, 0x90, 0x28, 0x80, 0x32, 0xd4,
0x80, 0x32, 0xd6, 0x10, 0x10, 0x80, 0x32, 0xd5, 0xe4, 0xe1, 0x58, 0x40,
0x35, 0x75, 0x96, 0xe8, 0x94, 0x80, 0x9a, 0x30, 0x04, 0x40, 0x91, 0x10,
0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x22, 0x80, 0x33, 0x27,
0x10, 0x10, 0x80, 0x33, 0x26, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x07,
0x80, 0x33, 0x0c, 0x10, 0x10, 0x80, 0x33, 0x0b, 0x90, 0x88, 0x90, 0x50,
0x90, 0x28, 0x80, 0x32, 0xec, 0x80, 0x32, 0xf1, 0x10, 0x10, 0x80, 0x32,
0xf0, 0x90, 0x50, 0x90, 0x28, 0x80, 0x32, 0xce, 0x80, 0x32, 0xd3, 0x10,
0x10, 0x80, 0x32, 0xd2, 0x91, 0x10, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28,
0x80, 0x33, 0x1f, 0x80, 0x33, 0x21, 0x10, 0x10, 0x80, 0x33, 0x20, 0x90,
0x50, 0x90, 0x28, 0x80, 0x33, 0x04, 0x80, 0x33, 0x06, 0x10, 0x10, 0x80,
0x33, 0x05, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80, 0x32, 0xe9, 0x80,
0x32, 0xeb, 0x10, 0x10, 0x80, 0x32, 0xea, 0x90, 0x50, 0x90, 0x28, 0x80,
0x32, 0xcb, 0x80, 0x32, 0xcd, 0x10, 0x10, 0x80, 0x32, 0xcc, 0xe4, 0xe1,
0x76, 0x40, 0x36, 0x01, 0x88, 0x02, 0x28, 0x91, 0x10, 0x90, 0x88, 0x90,
0x50, 0x90, 0x28, 0x80, 0x33, 0x23, 0x80, 0x33, 0x25, 0x10, 0x10, 0x80,
0x33, 0x24, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0x08, 0x80, 0x33, 0x0a,
0x10, 0x10, 0x80, 0x33, 0x09, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80,
0x32, 0xed, 0x80, 0x32, 0xef, 0x10, 0x10, 0x80, 0x32, 0xee, 0x90, 0x50,
0x90, 0x28, 0x80, 0x32, 0xcf, 0x80, 0x32, 0xd1, 0x10, 0x10, 0x80, 0x32,
0xd0, 0xe4, 0xe1, 0x57, 0x40, 0x35, 0x71, 0x90, 0x40, 0xe5, 0x21, 0x74,
0x40, 0x35, 0xf9, 0xe5, 0x21, 0x56, 0x40, 0x35, 0x6d, 0x9e, 0xb4, 0x23,
0xe8, 0x93, 0x70, 0x91, 0xd8, 0xd5, 0x07, 0x80, 0xd0, 0xc4, 0x40, 0x90,
0x48, 0x80, 0x8c, 0x3f, 0x38, 0x84, 0x37, 0xf1, 0xa4, 0x3d, 0x18, 0x37,
0xbb, 0x90, 0x28, 0x24, 0x3c, 0x58, 0xa4, 0x3a, 0xd8, 0x37, 0x73, 0xd0,
0xc4, 0x40, 0x90, 0x48, 0x80, 0x8c, 0x3f, 0x18, 0x84, 0x37, 0xef, 0xa4,
0x3d, 0x08, 0x37, 0xb9, 0x90, 0x28, 0x24, 0x3c, 0x48, 0xa4, 0x3a, 0xc8,
0x37, 0x71, 0xd5, 0x06, 0x80, 0xd0, 0xc3, 0x40, 0x90, 0x28, 0x80, 0x37,
0xdb, 0xa4, 0x3c, 0xe8, 0x37, 0xb5, 0x90, 0x28, 0x24, 0x3c, 0x28, 0xa4,
0x3a, 0xa8, 0x37, 0x6d, 0xd0, 0xc3, 0x40, 0x90, 0x28, 0x80, 0x37, 0xd7,
0xa4, 0x3c, 0xd8, 0x37, 0xb3, 0x90, 0x28, 0x24, 0x3c, 0x18, 0xa4, 0x3a,
0x98, 0x37, 0x6b, 0x91, 0x98, 0xd5, 0x06, 0x80, 0xd0, 0xc3, 0x40, 0x90,
0x28, 0x80, 0x37, 0xcf, 0xa4, 0x3c, 0xb8, 0x37, 0xaf, 0x90, 0x28, 0x24,
0x3b, 0xf8, 0xa4, 0x3a, 0x78, 0x37, 0x67, 0xd0, 0xc3, 0x40, 0x90, 0x28,
0x80, 0x37, 0xcb, 0xa4, 0x3c, 0xa8, 0x37, 0xad, 0x90, 0x28, 0x24, 0x3b,
0xe8, 0xa4, 0x3a, 0x68, 0x37, 0x65, 0xd5, 0x06, 0x80, 0xd0, 0xc3, 0x40,
0x90, 0x28, 0x80, 0x37, 0xc3, 0xa4, 0x3c, 0x88, 0x37, 0xa9, 0x90, 0x28,
0x24, 0x3b, 0xc8, 0xa4, 0x3a, 0x48, 0x37, 0x61, 0xd0, 0xc3, 0x40, 0x90,
0x28, 0x80, 0x37, 0xbf, 0xa4, 0x3c, 0x78, 0x37, 0xa7, 0x90, 0x28, 0x24,
0x3b, 0xb8, 0xa4, 0x3a, 0x38, 0x37, 0x5f, 0x93, 0x70, 0x91, 0xd8, 0xd5,
0x07, 0x80, 0xd0, 0xc4, 0x40, 0x90, 0x48, 0x80, 0x8c, 0x3f, 0x58, 0x84,
0x37, 0xf3, 0xa4, 0x3d, 0x28, 0x37, 0xbd, 0x90, 0x28, 0x24, 0x3c, 0x68,
0xa4, 0x3a, 0xe8, 0x37, 0x75, 0xd0, 0xc4, 0x40, 0x90, 0x48, 0x80, 0x8c,
0x3f, 0x28, 0x84, 0x37, 0xf0, 0xa4, 0x3d, 0x10, 0x37, 0xba, 0x90, 0x28,
0x24, 0x3c, 0x50, 0xa4, 0x3a, 0xd0, 0x37, 0x72, 0xd5, 0x06, 0x80, 0xd0,
0xc3, 0x40, 0x90, 0x28, 0x80, 0x37, 0xdf, 0xa4, 0x3c, 0xf8, 0x37, 0xb7,
0x90, 0x28, 0x24, 0x3c, 0x38, 0xa4, 0x3a, 0xb8, 0x37, 0x6f, 0xd0, 0xc3,
0x40, 0x90, 0x28, 0x80, 0x37, 0xd9, 0xa4, 0x3c, 0xe0, 0x37, 0xb4, 0x90,
0x28, 0x24, 0x3c, 0x20, 0xa4, 0x3a, 0xa0, 0x37, 0x6c, 0x91, 0x98, 0xd5,
0x06, 0x80, 0xd0, 0xc3, 0x40, 0x90, 0x28, 0x80, 0x37, 0xd3, 0xa4, 0x3c,
0xc8, 0x37, 0xb1, 0x90, 0x28, 0x24, 0x3c, 0x08, 0xa4, 0x3a, 0x88, 0x37,
0x69, 0xd0, 0xc3, 0x40, 0x90, 0x28, 0x80, 0x37, 0xcd, 0xa4, 0x3c, 0xb0,
0x37, 0xae, 0x90, 0x28, 0x24, 0x3b, 0xf0, 0xa4, 0x3a, 0x70, 0x37, 0x66,
0xd5, 0x06, 0x80, 0xd0, 0xc3, 0x40, 0x90, 0x28, 0x80, 0x37, 0xc7, 0xa4,
0x3c, 0x98, 0x37, 0xab, 0x90, 0x28, 0x24, 0x3b, 0xd8, 0xa4, 0x3a, 0x58,
0x37, 0x63, 0xd0, 0xc3, 0x40, 0x90, 0x28, 0x80, 0x37, 0xc1, 0xa4, 0x3c,
0x80, 0x37, 0xa8, 0x90, 0x28, 0x24, 0x3b, 0xc0, 0xa4, 0x3a, 0x40, 0x37,
0x60, 0x99, 0xd8, 0x03, 0x90, 0x81, 0x90, 0xe0, 0x5b, 0x41, 0x40, 0x03,
0x40, 0x51, 0x40, 0xc0, 0xa4, 0x23, 0x80, 0x34, 0x60, 0xd1, 0x42, 0x00,
0xa4, 0x22, 0x80, 0x34, 0x40, 0xa4, 0x21, 0x80, 0x34, 0x20, 0x5b, 0x41,
0x40, 0x03, 0x40, 0x51, 0x40, 0xc0, 0xa4, 0x22, 0xa0, 0x34, 0x64, 0xd1,
0x42, 0x00, 0xa4, 0x21, 0xa0, 0x34, 0x44, 0xa4, 0x20, 0xa0, 0x34, 0x24,
0x81, 0x90, 0xe0, 0x5b, 0x41, 0x40, 0x03, 0x40, 0x51, 0x40, 0xc0, 0xa4,
0x22, 0xe0, 0x34, 0x6c, 0xd1, 0x42, 0x00, 0xa4, 0x21, 0xe0, 0x34, 0x4c,
0xa4, 0x20, 0xe0, 0x34, 0x2c, 0x5b, 0x41, 0x40, 0x03, 0x40, 0x51, 0x40,
0xc0, 0xa4, 0x22, 0xc0, 0x34, 0x68, 0xd1, 0x42, 0x00, 0xa4, 0x21, 0xc0,
0x34, 0x48, 0xa4, 0x20, 0xc0, 0x34, 0x28, 0xa8, 0x0b, 0x18, 0x13, 0xa8,
0x96, 0x80, 0x93, 0x40, 0x99, 0x90, 0x03, 0x00, 0x90, 0xc0, 0x90, 0x60,
0x90, 0x38, 0xa4, 0x12, 0xb8, 0x32, 0x58, 0x24, 0x12, 0xb0, 0x90, 0x38,
0xa4, 0x11, 0xe0, 0x32, 0x3d, 0x24, 0x11, 0xd8, 0x90, 0x60, 0x90, 0x38,
0xa4, 0x11, 0x08, 0x32, 0x22, 0x24, 0x11, 0x00, 0x90, 0x38, 0xa4, 0x10,
0x30, 0x32, 0x07, 0x24, 0x10, 0x28, 0x90, 0xc0, 0x90, 0x60, 0x90, 0x38,
0xa4, 0x12, 0xa8, 0x32, 0x53, 0x24, 0x12, 0xa0, 0x90, 0x38, 0xa4, 0x11,
0xd0, 0x32, 0x38, 0x24, 0x11, 0xc8, 0x90, 0x60, 0x90, 0x38, 0xa4, 0x10,
0xf8, 0x32, 0x1d, 0x24, 0x10, 0xf0, 0x90, 0x38, 0xa4, 0x10, 0x20, 0x32,
0x02, 0x24, 0x10, 0x18, 0xe4, 0xe1, 0xd0, 0x40, 0x37, 0x43, 0x99, 0x90,
0x03, 0x00, 0x90, 0xc0, 0x90, 0x60, 0x90, 0x38, 0xa4, 0x12, 0x90, 0x32,
0x50, 0x24, 0x12, 0x88, 0x90, 0x38, 0xa4, 0x11, 0xb8, 0x32, 0x35, 0x24,
0x11, 0xb0, 0x90, 0x60, 0x90, 0x38, 0xa4, 0x10, 0xe0, 0x32, 0x1a, 0x24,
0x10, 0xd8, 0x90, 0x38, 0xa4, 0x10, 0x08, 0x31, 0xff, 0x24, 0x10, 0x00,
0x90, 0xc0, 0x90, 0x60, 0x90, 0x38, 0xa4, 0x12, 0x78, 0x32, 0x4d, 0x24,
0x12, 0x70, 0x90, 0x38, 0xa4, 0x11, 0xa0, 0x32, 0x32, 0x24, 0x11, 0x98,
0x90, 0x60, 0x90, 0x38, 0xa4, 0x10, 0xc8, 0x32, 0x17, 0x24, 0x10, 0xc0,
0x90, 0x38, 0xa4, 0x0f, 0xf0, 0x31, 0xfc, 0x24, 0x0f, 0xe8, 0xe4, 0xe1,
0xce, 0xc0, 0x37, 0x3d, 0x93, 0x78, 0x99, 0x90, 0x03, 0x00, 0x90, 0xc0,
0x90, 0x60, 0x90, 0x38, 0xa4, 0x12, 0x60, 0x32, 0x4a, 0x24, 0x12, 0x58,
0x90, 0x38, 0xa4, 0x11, 0x88, 0x32, 0x2f, 0x24, 0x11, 0x80, 0x90, 0x60,
0x90, 0x38, 0xa4, 0x10, 0xb0, 0x32, 0x14, 0x24, 0x10, 0xa8, 0x90, 0x38,
0xa4, 0x0f, 0xd8, 0x31, 0xf9, 0x24, 0x0f, 0xd0, 0x90, 0xc0, 0x90, 0x60,
0x90, 0x38, 0xa4, 0x12, 0x48, 0x32, 0x47, 0x24, 0x12, 0x40, 0x90, 0x38,
0xa4, 0x11, 0x70, 0x32, 0x2c, 0x24, 0x11, 0x68, 0x90, 0x60, 0x90, 0x38,
0xa4, 0x10, 0x98, 0x32, 0x11, 0x24, 0x10, 0x90, 0x90, 0x38, 0xa4, 0x0f,
0xc0, 0x31, 0xf6, 0x24, 0x0f, 0xb8, 0xec, 0xa1, 0x1e, 0x00, 0x02, 0x00,
0x34, 0x7a, 0xa4, 0x39, 0xa8, 0x37, 0x37, 0x88, 0x00, 0x88, 0x10, 0x10,
0x10, 0x10, 0x90, 0x38, 0xa4, 0x0f, 0xa8, 0x31, 0xf3, 0x24, 0x0f, 0xa0,
0xe9, 0x61, 0x1d, 0x40, 0x02, 0x00, 0x34, 0x76, 0xe3, 0x61, 0xcb, 0xc0,
0x37, 0x31, 0x95, 0x08, 0x93, 0x40, 0x99, 0x90, 0x03, 0x00, 0x90, 0xc0,
0x90, 0x60, 0x90, 0x38, 0xa4, 0x12, 0x30, 0x32, 0x41, 0x24, 0x12, 0x28,
0x90, 0x38, 0xa4, 0x11, 0x58, 0x32, 0x26, 0x24, 0x11, 0x50, 0x90, 0x60,
0x90, 0x38, 0xa4, 0x10, 0x80, 0x32, 0x0b, 0x24, 0x10, 0x78, 0x90, 0x38,
0xa4, 0x0f, 0x90, 0x31, 0xed, 0x24, 0x0f, 0x88, 0x90, 0xc0, 0x90, 0x60,
0x90, 0x38, 0xa4, 0x12, 0x00, 0x32, 0x3e, 0x24, 0x11, 0xf8, 0x90, 0x38,
0xa4, 0x11, 0x28, 0x32, 0x23, 0x24, 0x11, 0x20, 0x90, 0x60, 0x90, 0x38,
0xa4, 0x10, 0x50, 0x32, 0x08, 0x24, 0x10, 0x48, 0x90, 0x38, 0xa4, 0x0f,
0x60, 0x31, 0xea, 0x24, 0x0f, 0x58, 0xe4, 0xe1, 0xd0, 0x80, 0x37, 0x45,
0x88, 0x01, 0x88, 0x90, 0xc0, 0x90, 0x60, 0x90, 0x38, 0xa4, 0x12, 0x20,
0x32, 0x42, 0x24, 0x12, 0x18, 0x90, 0x38, 0xa4, 0x11, 0x48, 0x32, 0x27,
0x24, 0x11, 0x40, 0x90, 0x60, 0x90, 0x38, 0xa4, 0x10, 0x70, 0x32, 0x0c,
0x24, 0x10, 0x68, 0x90, 0x38, 0xa4, 0x0f, 0x80, 0x31, 0xee, 0x24, 0x0f,
0x78, 0xe4, 0xe1, 0xcf, 0x00, 0x37, 0x3f, 0x92, 0xd0, 0x99, 0x50, 0x02,
0x80, 0x90, 0xa0, 0x90, 0x50, 0x90, 0x28, 0x80, 0x31, 0xe9, 0x24, 0x0f,
0x40, 0x90, 0x28, 0x80, 0x31, 0xe5, 0x24, 0x0f, 0x20, 0x90, 0x50, 0x90,
0x28, 0x80, 0x31, 0xe1, 0x24, 0x0f, 0x00, 0x90, 0x28, 0x80, 0x31, 0xdd,
0x24, 0x0e, 0xe0, 0x90, 0xa0, 0x90, 0x50, 0x90, 0x28, 0x80, 0x31, 0xe6,
0x24, 0x0f, 0x38, 0x90, 0x28, 0x80, 0x31, 0xe2, 0x24, 0x0f, 0x18, 0x90,
0x50, 0x90, 0x28, 0x80, 0x31, 0xde, 0x24, 0x0e, 0xf8, 0x90, 0x28, 0x80,
0x31, 0xda, 0x24, 0x0e, 0xd8, 0xec, 0xe1, 0xcd, 0xa1, 0x1f, 0x00, 0x37,
0x39, 0x88, 0x00, 0x78, 0x10, 0x10, 0x10, 0x10, 0x90, 0x28, 0x80, 0x31,
0xd8, 0x24, 0x0e, 0xc8, 0xec, 0xe1, 0xcc, 0x21, 0x1d, 0x00, 0x37, 0x33,
0xe5, 0xa1, 0x55, 0x40, 0x35, 0x51, 0xa0, 0x2a, 0x10, 0xa8, 0x16, 0x60,
0x29, 0xd8, 0xa0, 0x0c, 0x48, 0xa0, 0x0a, 0xc8, 0x95, 0x60, 0x92, 0xb0,
0x91, 0x40, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80, 0x31, 0xa1, 0x80,
0x31, 0xa0, 0x10, 0x10, 0x80, 0x31, 0x9f, 0x90, 0x70, 0x90, 0x38, 0xa4,
0x08, 0x98, 0x31, 0xb3, 0xa4, 0x08, 0x90, 0x31, 0xb2, 0x10, 0x10, 0xa4,
0x08, 0x88, 0x31, 0xb1, 0x90, 0xb8, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x09,
0xb8, 0x31, 0xd7, 0xa4, 0x09, 0xb0, 0x31, 0xd6, 0x10, 0x10, 0xa4, 0x09,
0xa8, 0x31, 0xd5, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x09, 0x28, 0x31, 0xc5,
0xa4, 0x09, 0x20, 0x31, 0xc4, 0x10, 0x10, 0xa4, 0x09, 0x18, 0x31, 0xc3,
0x91, 0x40, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80, 0x31, 0x9c, 0x80,
0x31, 0x9e, 0x10, 0x10, 0x80, 0x31, 0x9d, 0x90, 0x70, 0x90, 0x38, 0xa4,
0x08, 0x70, 0x31, 0xae, 0xa4, 0x08, 0x80, 0x31, 0xb0, 0x10, 0x10, 0xa4,
0x08, 0x78, 0x31, 0xaf, 0x90, 0xb8, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x09,
0x90, 0x31, 0xd2, 0xa4, 0x09, 0xa0, 0x31, 0xd4, 0x10, 0x10, 0xa4, 0x09,
0x98, 0x31, 0xd3, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x09, 0x00, 0x31, 0xc0,
0xa4, 0x09, 0x10, 0x31, 0xc2, 0x10, 0x10, 0xa4, 0x09, 0x08, 0x31, 0xc1,
0x92, 0xb0, 0x91, 0x40, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80, 0x31,
0x99, 0x80, 0x31, 0x9b, 0x10, 0x10, 0x80, 0x31, 0x9a, 0x90, 0x70, 0x90,
0x38, 0xa4, 0x08, 0x58, 0x31, 0xab, 0xa4, 0x08, 0x68, 0x31, 0xad, 0x10,
0x10, 0xa4, 0x08, 0x60, 0x31, 0xac, 0x90, 0xb8, 0x90, 0x70, 0x90, 0x38,
0xa4, 0x09, 0x78, 0x31, 0xcf, 0xa4, 0x09, 0x88, 0x31, 0xd1, 0x10, 0x10,
0xa4, 0x09, 0x80, 0x31, 0xd0, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x08, 0xe8,
0x31, 0xbd, 0xa4, 0x08, 0xf8, 0x31, 0xbf, 0x10, 0x10, 0xa4, 0x08, 0xf0,
0x31, 0xbe, 0x91, 0x40, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80, 0x31,
0x96, 0x80, 0x31, 0x98, 0x10, 0x10, 0x80, 0x31, 0x97, 0x90, 0x70, 0x90,
0x38, 0xa4, 0x08, 0x40, 0x31, 0xa8, 0xa4, 0x08, 0x50, 0x31, 0xaa, 0x10,
0x10, 0xa4, 0x08, 0x48, 0x31, 0xa9, 0x90, 0xb8, 0x90, 0x70, 0x90, 0x38,
0xa4, 0x09, 0x60, 0x31, 0xcc, 0xa4, 0x09, 0x70, 0x31, 0xce, 0x10, 0x10,
0xa4, 0x09, 0x68, 0x31, 0xcd, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x08, 0xd0,
0x31, 0xba, 0xa4, 0x08, 0xe0, 0x31, 0xbc, 0x10, 0x10, 0xa4, 0x08, 0xd8,
0x31, 0xbb, 0x10, 0x10, 0x90, 0xa8, 0x10, 0x10, 0x10, 0x10, 0x90, 0x50,
0x90, 0x28, 0x80, 0x31, 0x8d, 0x80, 0x31, 0x8f, 0x10, 0x10, 0x80, 0x31,
0x8e, 0x90, 0x60, 0x90, 0x30, 0x60, 0xa0, 0x2a, 0xc0, 0x60, 0xa0, 0x2a,
0x80, 0x90, 0x30, 0x60, 0xa0, 0x2a, 0x40, 0x60, 0xa0, 0x2a, 0x00, 0x97,
0xf0, 0x95, 0x60, 0x92, 0xb0, 0x91, 0x40, 0x90, 0x88, 0x90, 0x50, 0x90,
0x28, 0x80, 0x31, 0x93, 0x80, 0x31, 0x95, 0x10, 0x10, 0x80, 0x31, 0x94,
0x90, 0x70, 0x90, 0x38, 0xa4, 0x08, 0x28, 0x31, 0xa5, 0xa4, 0x08, 0x38,
0x31, 0xa7, 0x10, 0x10, 0xa4, 0x08, 0x30, 0x31, 0xa6, 0x90, 0xb8, 0x90,
0x70, 0x90, 0x38, 0xa4, 0x09, 0x48, 0x31, 0xc9, 0xa4, 0x09, 0x58, 0x31,
0xcb, 0x10, 0x10, 0xa4, 0x09, 0x50, 0x31, 0xca, 0x90, 0x70, 0x90, 0x38,
0xa4, 0x08, 0xb8, 0x31, 0xb7, 0xa4, 0x08, 0xc8, 0x31, 0xb9, 0x10, 0x10,
0xa4, 0x08, 0xc0, 0x31, 0xb8, 0x91, 0x40, 0x90, 0x88, 0x90, 0x50, 0x90,
0x28, 0x80, 0x31, 0x90, 0x80, 0x31, 0x92, 0x10, 0x10, 0x80, 0x31, 0x91,
0x90, 0x70, 0x90, 0x38, 0xa4, 0x08, 0x10, 0x31, 0xa2, 0xa4, 0x08, 0x20,
0x31, 0xa4, 0x10, 0x10, 0xa4, 0x08, 0x18, 0x31, 0xa3, 0x90, 0xb8, 0x90,
0x70, 0x90, 0x38, 0xa4, 0x09, 0x30, 0x31, 0xc6, 0xa4, 0x09, 0x40, 0x31,
0xc8, 0x10, 0x10, 0xa4, 0x09, 0x38, 0x31, 0xc7, 0x90, 0x70, 0x90, 0x38,
0xa4, 0x08, 0xa0, 0x31, 0xb4, 0xa4, 0x08, 0xb0, 0x31, 0xb6, 0x10, 0x10,
0xa4, 0x08, 0xa8, 0x31, 0xb5, 0x10, 0x10, 0x91, 0x40, 0x90, 0xa0, 0x90,
0x50, 0x90, 0x28, 0x80, 0x30, 0xcb, 0x80, 0x30, 0xca, 0x90, 0x28, 0x80,
0x30, 0xc9, 0x80, 0x30, 0xc8, 0x90, 0x50, 0x90, 0x28, 0x80, 0x30, 0xc4,
0x80, 0x30, 0xc7, 0x90, 0x28, 0x80, 0x30, 0xc6, 0x80, 0x30, 0xc5, 0x90,
0xa0, 0x90, 0x50, 0x90, 0x28, 0x80, 0x30, 0xbc, 0x80, 0x30, 0xc3, 0x90,
0x28, 0x80, 0x30, 0xc2, 0x80, 0x30, 0xc1, 0x90, 0x50, 0x90, 0x28, 0x80,
0x30, 0xbd, 0x80, 0x30, 0xc0, 0x90, 0x28, 0x80, 0x30, 0xbf, 0x80, 0x30,
0xbe, 0x91, 0x88, 0x80, 0x90, 0xc0, 0x90, 0x60, 0x90, 0x28, 0x81, 0x31,
0x3b, 0x10, 0x10, 0x80, 0x31, 0x3a, 0x90, 0x28, 0x81, 0x31, 0x3d, 0x10,
0x10, 0x80, 0x31, 0x3c, 0x90, 0x60, 0x90, 0x28, 0x81, 0x31, 0x41, 0x10,
0x10, 0x80, 0x31, 0x40, 0x90, 0x28, 0x81, 0x31, 0x3f, 0x10, 0x10, 0x80,
0x31, 0x3e, 0x80, 0x10, 0x10, 0x10, 0x10, 0x90, 0x28, 0x81, 0x31, 0x38,
0x10, 0x10, 0x80, 0x31, 0x39, 0xa0, 0x0b, 0x90, 0xa0, 0x0a, 0xc8, 0x95,
0x60, 0x92, 0xb0, 0x91, 0x40, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80,
0x31, 0x56, 0x80, 0x31, 0x55, 0x10, 0x10, 0x80, 0x31, 0x54, 0x90, 0x70,
0x90, 0x38, 0xa4, 0x06, 0xe8, 0x31, 0x68, 0xa4, 0x06, 0xe0, 0x31, 0x67,
0x10, 0x10, 0xa4, 0x06, 0xd8, 0x31, 0x66, 0x90, 0xb8, 0x90, 0x70, 0x90,
0x38, 0xa4, 0x08, 0x08, 0x31, 0x8c, 0xa4, 0x08, 0x00, 0x31, 0x8b, 0x10,
0x10, 0xa4, 0x07, 0xf8, 0x31, 0x8a, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x07,
0x78, 0x31, 0x7a, 0xa4, 0x07, 0x70, 0x31, 0x79, 0x10, 0x10, 0xa4, 0x07,
0x68, 0x31, 0x78, 0x91, 0x40, 0x90, 0x88, 0x90, 0x50, 0x90, 0x28, 0x80,
0x31, 0x51, 0x80, 0x31, 0x53, 0x10, 0x10, 0x80, 0x31, 0x52, 0x90, 0x70,
0x90, 0x38, 0xa4, 0x06, 0xc0, 0x31, 0x63, 0xa4, 0x06, 0xd0, 0x31, 0x65,
0x10, 0x10, 0xa4, 0x06, 0xc8, 0x31, 0x64, 0x90, 0xb8, 0x90, 0x70, 0x90,
0x38, 0xa4, 0x07, 0xe0, 0x31, 0x87, 0xa4, 0x07, 0xf0, 0x31, 0x89, 0x10,
0x10, 0xa4, 0x07, 0xe8, 0x31, 0x88, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x07,
0x50, 0x31, 0x75, 0xa4, 0x07, 0x60, 0x31, 0x77, 0x10, 0x10, 0xa4, 0x07,
0x58, 0x31, 0x76, 0x92, 0xb0, 0x91, 0x40, 0x90, 0x88, 0x90, 0x50, 0x90,
0x28, 0x80, 0x31, 0x4e, 0x80, 0x31, 0x50, 0x10, 0x10, 0x80, 0x31, 0x4f,
0x90, 0x70, 0x90, 0x38, 0xa4, 0x06, 0xa8, 0x31, 0x60, 0xa4, 0x06, 0xb8,
0x31, 0x62, 0x10, 0x10, 0xa4, 0x06, 0xb0, 0x31, 0x61, 0x90, 0xb8, 0x90,
0x70, 0x90, 0x38, 0xa4, 0x07, 0xc8, 0x31, 0x84, 0xa4, 0x07, 0xd8, 0x31,
0x86, 0x10, 0x10, 0xa4, 0x07, 0xd0, 0x31, 0x85, 0x90, 0x70, 0x90, 0x38,
0xa4, 0x07, 0x38, 0x31, 0x72, 0xa4, 0x07, 0x48, 0x31, 0x74, 0x10, 0x10,
0xa4, 0x07, 0x40, 0x31, 0x73, 0x91, 0x40, 0x90, 0x88, 0x90, 0x50, 0x90,
0x28, 0x80, 0x31, 0x4b, 0x80, 0x31, 0x4d, 0x10, 0x10, 0x80, 0x31, 0x4c,
0x90, 0x70, 0x90, 0x38, 0xa4, 0x06, 0x90, 0x31, 0x5d, 0xa4, 0x06, 0xa0,
0x31, 0x5f, 0x10, 0x10, 0xa4, 0x06, 0x98, 0x31, 0x5e, 0x90, 0xb8, 0x90,
0x70, 0x90, 0x38, 0xa4, 0x07, 0xb0, 0x31, 0x81, 0xa4, 0x07, 0xc0, 0x31,
0x83, 0x10, 0x10, 0xa4, 0x07, 0xb8, 0x31, 0x82, 0x90, 0x70, 0x90, 0x38,
0xa4, 0x07, 0x20, 0x31, 0x6f, 0xa4, 0x07, 0x30, 0x31, 0x71, 0x10, 0x10,
0xa4, 0x07, 0x28, 0x31, 0x70, 0x10, 0x10, 0x80, 0x10, 0x10, 0x10, 0x10,
0x90, 0x50, 0x90, 0x28, 0x80, 0x31, 0x42, 0x80, 0x31, 0x44, 0x10, 0x10,
0x80, 0x31, 0x43, 0x80, 0x95, 0x60, 0x92, 0xb0, 0x91, 0x40, 0x90, 0x88,
0x90, 0x50, 0x90, 0x28, 0x80, 0x31, 0x48, 0x80, 0x31, 0x4a, 0x10, 0x10,
0x80, 0x31, 0x49, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x06, 0x78, 0x31, 0x5a,
0xa4, 0x06, 0x88, 0x31, 0x5c, 0x10, 0x10, 0xa4, 0x06, 0x80, 0x31, 0x5b,
0x90, 0xb8, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x07, 0x98, 0x31, 0x7e, 0xa4,
0x07, 0xa8, 0x31, 0x80, 0x10, 0x10, 0xa4, 0x07, 0xa0, 0x31, 0x7f, 0x90,
0x70, 0x90, 0x38, 0xa4, 0x07, 0x08, 0x31, 0x6c, 0xa4, 0x07, 0x18, 0x31,
0x6e, 0x10, 0x10, 0xa4, 0x07, 0x10, 0x31, 0x6d, 0x91, 0x40, 0x90, 0x88,
0x90, 0x50, 0x90, 0x28, 0x80, 0x31, 0x45, 0x80, 0x31, 0x47, 0x10, 0x10,
0x80, 0x31, 0x46, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x06, 0x60, 0x31, 0x57,
0xa4, 0x06, 0x70, 0x31, 0x59, 0x10, 0x10, 0xa4, 0x06, 0x68, 0x31, 0x58,
0x90, 0xb8, 0x90, 0x70, 0x90, 0x38, 0xa4, 0x07, 0x80, 0x31, 0x7b, 0xa4,
0x07, 0x90, 0x31, 0x7d, 0x10, 0x10, 0xa4, 0x07, 0x88, 0x31, 0x7c, 0x90,
0x70, 0x90, 0x38, 0xa4, 0x06, 0xf0, 0x31, 0x69, 0xa4, 0x07, 0x00, 0x31,
0x6b, 0x10, 0x10, 0xa4, 0x06, 0xf8, 0x31, 0x6a, 0x10, 0x10, 0x91, 0x40,
0x90, 0xa0, 0x90, 0x50, 0x90, 0x28, 0x80, 0x30, 0xbb, 0x80, 0x30, 0xba,
0x90, 0x28, 0x80, 0x30, 0xb9, 0x80, 0x30, 0xb8, 0x90, 0x50, 0x90, 0x28,
0x80, 0x30, 0xb4, 0x80, 0x30, 0xb7, 0x90, 0x28, 0x80, 0x30, 0xb6, 0x80,
0x30, 0xb5, 0x90, 0xa0, 0x90, 0x50, 0x90, 0x28, 0x80, 0x30, 0xac, 0x80,
0x30, 0xb3, 0x90, 0x28, 0x80, 0x30, 0xb2, 0x80, 0x30, 0xb1, 0x90, 0x50,
0x90, 0x28, 0x80, 0x30, 0xad, 0x80, 0x30, 0xb0, 0x90, 0x28, 0x80, 0x30,
0xaf, 0x80, 0x30, 0xae, 0xc3, 0xc0, 0x30, 0x42, 0x9c, 0xe8, 0x07, 0x60,
0x91, 0x90, 0x90, 0xf0, 0x10, 0x10, 0x80, 0x88, 0x00, 0x80, 0x90, 0x50,
0x90, 0x28, 0x80, 0x33, 0xf8, 0x80, 0x33, 0xf9, 0x81, 0x33, 0xef, 0xd0,
0x41, 0x80, 0x24, 0x20, 0x90, 0x24, 0x20, 0x98, 0x10, 0x10, 0x80, 0x90,
0x58, 0x80, 0x90, 0x28, 0x24, 0x1f, 0x90, 0x24, 0x1f, 0x98, 0x81, 0x24,
0x1f, 0x50, 0x92, 0x68, 0x91, 0x00, 0x80, 0x90, 0x90, 0x90, 0x30, 0x80,
0x24, 0x20, 0x00, 0x90, 0x38, 0xa4, 0x1f, 0xf8, 0x34, 0x06, 0x80, 0x34,
0x05, 0x80, 0x90, 0x28, 0x80, 0x34, 0x0f, 0xa4, 0x1f, 0xe0, 0x34, 0x0e,
0x80, 0x90, 0xc0, 0x90, 0x60, 0x90, 0x28, 0x80, 0x34, 0x09, 0xa4, 0x1f,
0xf0, 0x34, 0x08, 0x90, 0x28, 0x80, 0x34, 0x04, 0xa4, 0x1f, 0xe8, 0x34,
0x03, 0x90, 0x50, 0x90, 0x28, 0x80, 0x34, 0x0d, 0x80, 0x34, 0x0c, 0x90,
0x28, 0x24, 0x20, 0x88, 0x24, 0x20, 0x80, 0x90, 0x58, 0x80, 0x10, 0x10,
0x80, 0x10, 0x10, 0x80, 0x33, 0xfb, 0x80, 0x90, 0x40, 0x10, 0x10, 0x80,
0x24, 0x1f, 0x60, 0x80, 0x10, 0x10, 0x80, 0x33, 0xfa, 0x91, 0x58, 0x91,
0x00, 0x90, 0x80, 0x81, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33, 0xf6, 0x80,
0x33, 0xf7, 0x81, 0x33, 0xee, 0x81, 0x90, 0x50, 0x90, 0x28, 0x80, 0x33,
0xf4, 0x80, 0x33, 0xf5, 0x81, 0x33, 0xed, 0x83, 0x90, 0x28, 0x24, 0x1f,
0x80, 0x24, 0x1f, 0x88, 0x90, 0xe8, 0x81, 0x90, 0x88, 0x90, 0x38, 0x10,
0x10, 0x80, 0x34, 0x07, 0x90, 0x28, 0x80, 0x34, 0x02, 0x80, 0x34, 0x01,
0x80, 0x90, 0x28, 0x80, 0x34, 0x0b, 0x80, 0x34, 0x0a, 0x82, 0x10, 0x10,
0x80, 0x24, 0x1f, 0x58, 0x97, 0x10, 0x9e, 0x10, 0x06, 0x98, 0x93, 0x00,
0x91, 0x80, 0x90, 0xc0, 0x90, 0x60, 0x90, 0x38, 0xa4, 0x03, 0x80, 0x30,
0x71, 0x24, 0x03, 0x78, 0x90, 0x38, 0xa4, 0x04, 0x10, 0x30, 0x83, 0x24,
0x04, 0x08, 0x90, 0x60, 0x90, 0x38, 0xa4, 0x05, 0x30, 0x30, 0xa7, 0x24,
0x05, 0x28, 0x90, 0x38, 0xa4, 0x04, 0xa0, 0x30, 0x95, 0x24, 0x04, 0x98,
0x90, 0xc0, 0x90, 0x60, 0x90, 0x38, 0xa4, 0x03, 0x70, 0x30, 0x6c, 0x24,
0x03, 0x68, 0x90, 0x38, 0xa4, 0x04, 0x00, 0x30, 0x7e, 0x24, 0x03, 0xf8,
0x90, 0x60, 0x90, 0x38, 0xa4, 0x05, 0x20, 0x30, 0xa2, 0x24, 0x05, 0x18,
0x90, 0x38, 0xa4, 0x04, 0x90, 0x30, 0x90, 0x24, 0x04, 0x88, 0x91, 0x80,
0x90, 0xc0, 0x90, 0x60, 0x90, 0x38, 0xa4, 0x03, 0x58, 0x30, 0x69, 0x24,
0x03, 0x50, 0x90, 0x38, 0xa4, 0x03, 0xe8, 0x30, 0x7b, 0x24, 0x03, 0xe0,
0x90, 0x60, 0x90, 0x38, 0xa4, 0x05, 0x08, 0x30, 0x9f, 0x24, 0x05, 0x00,
0x90, 0x38, 0xa4, 0x04, 0x78, 0x30, 0x8d, 0x24, 0x04, 0x70, 0x90, 0xc0,
0x90, 0x60, 0x90, 0x38, 0xa4, 0x03, 0x40, 0x30, 0x66, 0x24, 0x03, 0x38,
0x90, 0x38, 0xa4, 0x03, 0xd0, 0x30, 0x78, 0x24, 0x03, 0xc8, 0x90, 0x60,
0x90, 0x38, 0xa4, 0x04, 0xf0, 0x30, 0x9c, 0x24, 0x04, 0xe8, 0x90, 0x38,
0xa4, 0x04, 0x60, 0x30, 0x8a, 0x24, 0x04, 0x58, 0x10, 0x10, 0x80, 0x10,
0x10, 0x10, 0x10, 0x90, 0x38, 0xa4, 0x02, 0xf8, 0x30, 0x5d, 0x24, 0x02,
0xf0, 0xd7, 0x42, 0x00, 0xa4, 0x39, 0x58, 0x37, 0x2d, 0xa4, 0x39, 0x38,
0x37, 0x29, 0x9c, 0xe0, 0x06, 0x90, 0x93, 0x00, 0x91, 0x80, 0x90, 0xc0,
0x90, 0x60, 0x90, 0x38, 0xa4, 0x03, 0x28, 0x30, 0x63, 0x24, 0x03, 0x20,
0x90, 0x38, 0xa4, 0x03, 0xb8, 0x30, 0x75, 0x24, 0x03, 0xb0, 0x90, 0x60,
0x90, 0x38, 0xa4, 0x04, 0xd8, 0x30, 0x99, 0x24, 0x04, 0xd0, 0x90, 0x38,
0xa4, 0x04, 0x48, 0x30, 0x87, 0x24, 0x04, 0x40, 0x90, 0xc0, 0x90, 0x60,
0x90, 0x38, 0xa4, 0x03, 0x10, 0x30, 0x60, 0x24, 0x03, 0x08, 0x90, 0x38,
0xa4, 0x03, 0xa0, 0x30, 0x72, 0x24, 0x03, 0x98, 0x90, 0x60, 0x90, 0x38,
0xa4, 0x04, 0xc0, 0x30, 0x96, 0x24, 0x04, 0xb8, 0x90, 0x38, 0xa4, 0x04,
0x30, 0x30, 0x84, 0x24, 0x04, 0x28, 0x10, 0x10, 0x90, 0xe0, 0x90, 0x70,
0x90, 0x38, 0xa4, 0x02, 0x88, 0x30, 0x52, 0xa4, 0x02, 0x78, 0x30, 0x50,
0x90, 0x38, 0xa4, 0x02, 0x70, 0x30, 0x4b, 0xa4, 0x02, 0x60, 0x30, 0x4d,
0x90, 0x70, 0x90, 0x38, 0xa4, 0x02, 0x50, 0x30, 0x43, 0xa4, 0x02, 0x40,
0x30, 0x49, 0x90, 0x38, 0xa4, 0x02, 0x38, 0x30, 0x44, 0xa4, 0x02, 0x28,
0x30, 0x46, 0x91, 0x48, 0x80, 0x90, 0xa0, 0x90, 0x50, 0x90, 0x28, 0x80,
0x30, 0x56, 0x24, 0x02, 0xa8, 0x90, 0x28, 0x80, 0x30, 0x58, 0x24, 0x02,
0xb8, 0x90, 0x50, 0x90, 0x28, 0x80, 0x30, 0x5c, 0x24, 0x02, 0xd8, 0x90,
0x28, 0x80, 0x30, 0x5a, 0x24, 0x02, 0xc8, 0x80, 0x10, 0x10, 0x10, 0x10,
0x90, 0x28, 0x80, 0x30, 0x53, 0x24, 0x02, 0xa0, 0xd7, 0x42, 0x00, 0xa4,
0x39, 0x60, 0x37, 0x2e, 0xa4, 0x39, 0x40, 0x37, 0x2a, 0xa0, 0x14, 0x68,
0xa0, 0x10, 0x90, 0xa0, 0x0c, 0x60, 0x9e, 0x88, 0x09, 0xd0, 0x94, 0xf0,
0x90, 0xb0, 0x88, 0x00, 0x68, 0x84, 0x10, 0x10, 0xc9, 0xe1, 0x4c, 0x40,
0x85, 0x35, 0x4d, 0xcb, 0x61, 0x45, 0x00, 0x85, 0x35, 0x23, 0x9a, 0x00,
0x03, 0xf8, 0x91, 0x98, 0x80, 0x91, 0x10, 0x90, 0xa0, 0x90, 0x68, 0x90,
0x20, 0x3a, 0x75, 0xc9, 0xe2, 0x9c, 0xc0, 0x85, 0x35, 0x4b, 0xa4, 0x53,
0x88, 0x3a, 0x72, 0x90, 0x38, 0xa4, 0x53, 0x50, 0x3a, 0x6b, 0xa4, 0x53,
0x40, 0x3a, 0x69, 0x90, 0x48, 0x10, 0x10, 0xa4, 0x53, 0x08, 0x3a, 0x62,
0x10, 0x10, 0x80, 0x3a, 0x5e, 0x81, 0x10, 0x10, 0x80, 0xa4, 0x52, 0xd8,
0x3a, 0x5c, 0x91, 0xb0, 0x91, 0x60, 0x90, 0xe0, 0x90, 0x70, 0x90, 0x38,
0xa4, 0x53, 0x78, 0x3a, 0x70, 0xa4, 0x53, 0x68, 0x3a, 0x6e, 0x90, 0x38,
0xa4, 0x53, 0x30, 0x3a, 0x67, 0xa4, 0x53, 0x20, 0x3a, 0x65, 0x90, 0x48,
0x10, 0x10, 0xa4, 0x52, 0xf8, 0x3a, 0x60, 0x10, 0x10, 0x80, 0x3a, 0x5d,
0x90, 0x28, 0x80, 0x3a, 0x56, 0x80, 0x3a, 0x55, 0x81, 0x10, 0x10, 0x80,
0xa4, 0x52, 0xc8, 0x3a, 0x5a, 0xcb, 0x61, 0x44, 0xc0, 0x85, 0x35, 0x22,
0x90, 0xd8, 0x88, 0x00, 0x90, 0x84, 0x90, 0x38, 0xc1, 0xc0, 0x85, 0x3a,
0x78, 0xc9, 0xe1, 0x4c, 0x00, 0x85, 0x35, 0x49, 0xcb, 0x61, 0x44, 0x80,
0x85, 0x35, 0x21, 0x88, 0x00, 0x68, 0x84, 0x10, 0x10, 0xc9, 0xe1, 0x4b,
0xc0, 0x85, 0x35, 0x47, 0xcb, 0x61, 0x44, 0x40, 0x85, 0x35, 0x20, 0x91,
0xf8, 0x90, 0xb0, 0x88, 0x00, 0x68, 0x84, 0x10, 0x10, 0xc9, 0xe1, 0x4b,
0x40, 0x85, 0x35, 0x43, 0xcb, 0x61, 0x43, 0xc0, 0x85, 0x35, 0x1e, 0x88,
0x01, 0x00, 0x90, 0xa0, 0x81, 0x90, 0x70, 0x80, 0x90, 0x20, 0x3a, 0x6c,
0xc9, 0xe1, 0x4b, 0x00, 0x85, 0x35, 0x41, 0x81, 0x3a, 0x63, 0x81, 0x10,
0x10, 0x80, 0xa4, 0x52, 0xb8, 0x3a, 0x58, 0xcb, 0x61, 0x43, 0x80, 0x85,
0x35, 0x1d, 0x90, 0xb0, 0x88, 0x00, 0x68, 0x84, 0x10, 0x10, 0xc9, 0xe1,
0x4a, 0xc0, 0x85, 0x35, 0x3f, 0xcb, 0x61, 0x43, 0x40, 0x85, 0x35, 0x1c,
0x88, 0x00, 0x68, 0x84, 0x10, 0x10, 0xc9, 0xe1, 0x4a, 0x80, 0x85, 0x35,
0x3d, 0xcb, 0x61, 0x43, 0x00, 0x85, 0x35, 0x1b, 0x92, 0x38, 0x81, 0x91,
0x68, 0x91, 0x18, 0x90, 0x80, 0x90, 0x40, 0x80, 0xa4, 0x54, 0x38, 0x3a,
0x88, 0x80, 0xa4, 0x54, 0x30, 0x3a, 0x85, 0x90, 0x28, 0x81, 0x3a, 0x84,
0x90, 0x38, 0xa4, 0x54, 0x10, 0x3a, 0x83, 0xa4, 0x54, 0x00, 0x3a, 0x81,
0x90, 0x28, 0x80, 0x3a, 0x7f, 0x80, 0x3a, 0x7e, 0x80, 0x90, 0x40, 0x10,
0x10, 0x80, 0x24, 0x53, 0xe8, 0x10, 0x10, 0x90, 0x38, 0xa4, 0x53, 0xd8,
0x3a, 0x7c, 0xa4, 0x53, 0xc8, 0x3a, 0x7a, 0x90, 0x28, 0x80, 0x3a, 0x77,
0x80, 0x3a, 0x76, 0x9a, 0xd0, 0x03, 0xe0, 0x91, 0x60, 0x90, 0xb0, 0x88,
0x00, 0x68, 0x84, 0x10, 0x10, 0xc9, 0xe1, 0x4a, 0x00, 0x85, 0x35, 0x39,
0xcb, 0x61, 0x42, 0x80, 0x85, 0x35, 0x19, 0x88, 0x00, 0x68, 0x84, 0x10,
0x10, 0xc9, 0xe1, 0x49, 0xc0, 0x85, 0x35, 0x37, 0xcb, 0x61, 0x42, 0x40,
0x85, 0x35, 0x18, 0x90, 0xb0, 0x88, 0x00, 0x68, 0x84, 0x10, 0x10, 0xc9,
0xe1, 0x49, 0x80, 0x85, 0x35, 0x35, 0xcb, 0x61, 0x42, 0x00, 0x85, 0x35,
0x17, 0x88, 0x00, 0x68, 0x84, 0x10, 0x10, 0xc9, 0xe1, 0x49, 0x40, 0x85,
0x35, 0x33, 0xcb, 0x61, 0x41, 0xc0, 0x85, 0x35, 0x16, 0x90, 0x90, 0x90,
0x48, 0xcb, 0xa1, 0x40, 0x00, 0x85, 0x35, 0x05, 0xcb, 0xa1, 0x3f, 0xc0,
0x85, 0x35, 0x04, 0x90, 0x48, 0xcb, 0xa1, 0x3f, 0x80, 0x85, 0x35, 0x03,
0xcb, 0xa1, 0x3f, 0x40, 0x85, 0x35, 0x02, 0xcb, 0xa2, 0x94, 0xc0, 0x80,
0x3a, 0x54, 0x92, 0x40, 0x91, 0x20, 0x90, 0x90, 0x90, 0x48, 0x8c, 0x27,
0x60, 0x84, 0x24, 0x27, 0xd8, 0x8c, 0x27, 0x58, 0x84, 0x24, 0x27, 0xd0,
0x90, 0x48, 0x8c, 0x27, 0x50, 0x84, 0x24, 0x27, 0xc8, 0x8c, 0x27, 0x48,
0x84, 0x24, 0x27, 0xc0, 0x90, 0x90, 0x90, 0x48, 0x8c, 0x27, 0x38, 0x84,
0x24, 0x27, 0xb0, 0x8c, 0x27, 0x30, 0x84, 0x24, 0x27, 0xa8, 0x90, 0x48,
0x8c, 0x27, 0x28, 0x84, 0x24, 0x27, 0xa0, 0x8c, 0x27, 0x20, 0x84, 0x24,
0x27, 0x98, 0x91, 0x20, 0x90, 0x90, 0x90, 0x48, 0x8c, 0x27, 0x10, 0x84,
0x24, 0x27, 0x88, 0x8c, 0x27, 0x08, 0x84, 0x24, 0x27, 0x80, 0x90, 0x48,
0x8c, 0x27, 0x00, 0x84, 0x24, 0x27, 0x78, 0x8c, 0x26, 0xf8, 0x84, 0x24,
0x27, 0x70, 0x90, 0x38, 0xa4, 0x26, 0xe0, 0x34, 0xdd, 0xa4, 0x26, 0xd0,
0x34, 0xdb, 0xa0, 0x0f, 0x50, 0xa0, 0x09, 0x08, 0x9a, 0x30, 0x04, 0x40,
0x91, 0x90, 0x90, 0xc8, 0x98, 0x50, 0x00, 0x80, 0xe5, 0x22, 0x92, 0xc0,
0x3a, 0x43, 0xe5, 0x22, 0x8a, 0xc0, 0x3a, 0x3f, 0xcb, 0x61, 0x32, 0x40,
0x85, 0x34, 0xd8, 0x98, 0x50, 0x00, 0x80, 0xe5, 0x22, 0x82, 0xc0, 0x3a,
0x03, 0xe5, 0x22, 0x7a, 0xc0, 0x39, 0xff, 0xcb, 0x61, 0x32, 0x00, 0x85,
0x34, 0xd7, 0x90, 0x48, 0xcb, 0xa1, 0x31, 0xc0, 0x85, 0x34, 0xd6, 0xcb,
0xa1, 0x31, 0x80, 0x85, 0x34, 0xd5, 0x91, 0x90, 0x90, 0xc8, 0x98, 0x50,
0x00, 0x80, 0xe5, 0x22, 0x6c, 0xc0, 0x39, 0xcb, 0xe5, 0x22, 0x60, 0xc0,
0x39, 0x9b, 0xcb, 0x61, 0x31, 0x00, 0x85, 0x34, 0xd3, 0x98, 0x50, 0x00,
0x80, 0xe5, 0x22, 0x54, 0xc0, 0x39, 0x6b, 0xe5, 0x22, 0x48, 0xc0, 0x39,
0x3b, 0xcb, 0x61, 0x30, 0xc0, 0x85, 0x34, 0xd2, 0x90, 0x48, 0xcb, 0xa1,
0x30, 0x80, 0x85, 0x34, 0xd1, 0xcb, 0xa1, 0x30, 0x40, 0x85, 0x34, 0xd0,
0x92, 0x20, 0x91, 0x30, 0x90, 0xb8, 0xd5, 0x03, 0x00, 0xc0, 0xc0, 0x81,
0x8c, 0x01, 0xa0, 0x84, 0x30, 0x3e, 0xc0, 0xc0, 0x81, 0x8c, 0x01, 0x80,
0x84, 0x30, 0x3c, 0xd5, 0x02, 0x00, 0xc0, 0xc0, 0x81, 0x30, 0x28, 0xc0,
0xc0, 0x81, 0x30, 0x24, 0x90, 0x78, 0xd5, 0x02, 0x00, 0xc0, 0xc0, 0x81,
0x30, 0x1c, 0xc0, 0xc0, 0x81, 0x30, 0x18, 0xd5, 0x02, 0x00, 0xc0, 0xc0,
0x81, 0x30, 0x10, 0xc0, 0xc0, 0x81, 0x30, 0x0c, 0x91, 0x70, 0x90, 0xd8,
0xd5, 0x03, 0x80, 0xc8, 0xe2, 0x40, 0xc0, 0x81, 0x8c, 0x01, 0xc0, 0x84,
0x30, 0x40, 0xc8, 0xe2, 0x42, 0xc0, 0x81, 0x8c, 0x01, 0x90, 0x84, 0x30,
0x3d, 0xd5, 0x02, 0x80, 0xc8, 0xe2, 0x3f, 0xc0, 0x81, 0x30, 0x2c, 0xc8,
0xe2, 0x3a, 0x40, 0x81, 0x30, 0x26, 0x90, 0x98, 0xd5, 0x02, 0x80, 0xc8,
0xe2, 0x2f, 0x40, 0x81, 0x30, 0x20, 0xc8, 0xe2, 0x31, 0x40, 0x81, 0x30,
0x1a, 0xd5, 0x02, 0x80, 0xc8, 0xe2, 0x2e, 0x40, 0x81, 0x30, 0x14, 0xc8,
0xe2, 0x28, 0xc0, 0x81, 0x30, 0x0e, 0x9a, 0x30, 0x04, 0x40, 0x91, 0x90,
0x90, 0xc8, 0x98, 0x50, 0x00, 0x80, 0xe5, 0x22, 0x86, 0xc0, 0x3a, 0x13,
0xe5, 0x22, 0x88, 0xc0, 0x3a, 0x37, 0xcb, 0x61, 0x2f, 0xc0, 0x85, 0x34,
0xce, 0x98, 0x50, 0x00, 0x80, 0xe5, 0x22, 0x76, 0xc0, 0x39, 0xd3, 0xe5,
0x22, 0x78, 0xc0, 0x39, 0xf7, 0xcb, 0x61, 0x2f, 0x80, 0x85, 0x34, 0xcd,
0x90, 0x48, 0xcb, 0xa1, 0x2f, 0x40, 0x85, 0x34, 0xcc, 0xcb, 0xa1, 0x2f,
0x00, 0x85, 0x34, 0xcb, 0x91, 0x90, 0x90, 0xc8, 0x98, 0x50, 0x00, 0x80,
0xe5, 0x22, 0x68, 0xc0, 0x39, 0xbb, 0xe5, 0x22, 0x5c, 0xc0, 0x39, 0x8b,
0xcb, 0x61, 0x2d, 0x40, 0x85, 0x34, 0xba, 0x98, 0x50, 0x00, 0x80, 0xe5,
0x22, 0x50, 0xc0, 0x39, 0x5b, 0xe5, 0x22, 0x44, 0xc0, 0x39, 0x2b, 0xcb,
0x61, 0x2d, 0x00, 0x85, 0x34, 0xb9, 0x90, 0x48, 0xcb, 0xa1, 0x2c, 0xc0,
0x85, 0x34, 0xb8, 0xcb, 0xa1, 0x2c, 0x80, 0x85, 0x34, 0xb7, 0x91, 0x00,
0x90, 0x80, 0x90, 0x40, 0xe5, 0x20, 0x02, 0x40, 0x30, 0x0a, 0xe5, 0x20,
0x01, 0x80, 0x30, 0x07, 0x90, 0x40, 0xe5, 0x20, 0x00, 0xc0, 0x30, 0x04,
0xe5, 0x20, 0x00, 0x00, 0x30, 0x01, 0x90, 0x80, 0x90, 0x40, 0xe5, 0x22,
0x35, 0xc0, 0x38, 0xcd, 0xe5, 0x22, 0x38, 0x00, 0x38, 0xf5, 0x90, 0x40,
0xe5, 0x22, 0x24, 0x40, 0x38, 0x87, 0xe5, 0x22, 0x26, 0x80, 0x38, 0xaf,
0x80, 0x99, 0x28, 0x02, 0xf0, 0x8c, 0x25, 0x48, 0x90, 0x80, 0x90, 0x40,
0xe5, 0x22, 0x8c, 0xc0, 0x3a, 0x2f, 0xe5, 0x22, 0x89, 0xc0, 0x3a, 0x3b,
0x90, 0x40, 0xe5, 0x22, 0x7c, 0xc0, 0x39, 0xef, 0xe5, 0x22, 0x79, 0xc0,
0x39, 0xfb, 0x91, 0x48, 0x90, 0xc8, 0x98, 0x50, 0x00, 0x80, 0xe5, 0x22,
0x6a, 0xc0, 0x39, 0xc3, 0xe5, 0x22, 0x5e, 0xc0, 0x39, 0x93, 0xcb, 0x61,
0x2b, 0x00, 0x85, 0x34, 0xb0, 0x90, 0x40, 0xe5, 0x22, 0x52, 0xc0, 0x39,
0x63, 0xe5, 0x22, 0x46, 0xc0, 0x39, 0x33, 0x90, 0x48, 0xcb, 0xa1, 0x2a,
0x80, 0x85, 0x34, 0xae, 0xcb, 0xa1, 0x2a, 0xc0, 0x85, 0x34, 0xaf, 0x10,
0x10, 0x90, 0x80, 0x90, 0x40, 0xe5, 0x22, 0x3c, 0x40, 0x38, 0xed, 0xe5,
0x22, 0x39, 0x40, 0x38, 0xfb, 0x90, 0x40, 0xe5, 0x22, 0x2a, 0xc0, 0x38,
0xa7, 0xe5, 0x22, 0x27, 0xc0, 0x38, 0xb5,
};

static const struct ia64_dis_names ia64_dis_names[] = {
{ 0x51, 41, 0, 10 },
{ 0x31, 41, 1, 20 },
{ 0x11, 42, 0, 19 },
{ 0x29, 41, 0, 12 },
{ 0x19, 41, 1, 24 },
{ 0x9, 42, 0, 23 },
{ 0x15, 41, 0, 14 },
{ 0xd, 41, 1, 28 },
{ 0x5, 42, 0, 27 },
{ 0xb, 41, 0, 16 },
{ 0x7, 41, 1, 32 },
{ 0x3, 42, 0, 31 },
{ 0x51, 39, 1, 58 },
{ 0x50, 39, 0, 34 },
{ 0xd1, 39, 1, 57 },
{ 0xd0, 39, 0, 33 },
{ 0x31, 39, 1, 68 },
{ 0x30, 39, 1, 44 },
{ 0x11, 40, 1, 67 },
{ 0x10, 40, 0, 43 },
{ 0x71, 39, 1, 66 },
{ 0x70, 39, 1, 42 },
{ 0x31, 40, 1, 65 },
{ 0x30, 40, 0, 41 },
{ 0x29, 39, 1, 60 },
{ 0x28, 39, 0, 36 },
{ 0x69, 39, 1, 59 },
{ 0x68, 39, 0, 35 },
{ 0x19, 39, 1, 72 },
{ 0x18, 39, 1, 48 },
{ 0x9, 40, 1, 71 },
{ 0x8, 40, 0, 47 },
{ 0x39, 39, 1, 70 },
{ 0x38, 39, 1, 46 },
{ 0x19, 40, 1, 69 },
{ 0x18, 40, 0, 45 },
{ 0x15, 39, 1, 62 },
{ 0x14, 39, 0, 38 },
{ 0x35, 39, 1, 61 },
{ 0x34, 39, 0, 37 },
{ 0xd, 39, 1, 76 },
{ 0xc, 39, 1, 52 },
{ 0x5, 40, 1, 75 },
{ 0x4, 40, 0, 51 },
{ 0x1d, 39, 1, 74 },
{ 0x1c, 39, 1, 50 },
{ 0xd, 40, 1, 73 },
{ 0xc, 40, 0, 49 },
{ 0xb, 39, 1, 64 },
{ 0xa, 39, 0, 40 },
{ 0x1b, 39, 1, 63 },
{ 0x1a, 39, 0, 39 },
{ 0x7, 39, 1, 80 },
{ 0x6, 39, 1, 56 },
{ 0x3, 40, 1, 79 },
{ 0x2, 40, 0, 55 },
{ 0xf, 39, 1, 78 },
{ 0xe, 39, 1, 54 },
{ 0x7, 40, 1, 77 },
{ 0x6, 40, 0, 53 },
{ 0x8, 38, 0, 82 },
{ 0x18, 38, 0, 81 },
{ 0x1, 38, 1, 86 },
{ 0x2, 38, 0, 85 },
{ 0x3, 38, 1, 84 },
{ 0x4, 38, 0, 83 },
{ 0x1, 336, 0, 87 },
{ 0x20, 289, 0, 98 },
{ 0x220, 289, 0, 94 },
{ 0x1220, 289, 0, 91 },
{ 0xa20, 289, 0, 92 },
{ 0x620, 289, 0, 93 },
{ 0x120, 289, 0, 95 },
{ 0xa0, 289, 0, 96 },
{ 0x60, 289, 0, 97 },
{ 0x10, 289, 0, 102 },
{ 0x90, 289, 0, 99 },
{ 0x50, 289, 0, 100 },
{ 0x30, 289, 0, 101 },
{ 0x8, 289, 0, 103 },
{ 0x4, 289, 0, 104 },
{ 0x2, 289, 0, 105 },
{ 0x1, 289, 0, 106 },
{ 0x1, 411, 0, 108 },
{ 0x3, 411, 0, 107 },
{ 0x2, 417, 0, 109 },
{ 0x1, 417, 0, 110 },
{ 0x2, 413, 0, 111 },
{ 0x1, 413, 0, 112 },
{ 0x2, 415, 0, 113 },
{ 0x1, 415, 0, 114 },
{ 0x2, 419, 0, 115 },
{ 0x1, 419, 0, 116 },
{ 0x1, 268, 0, 143 },
{ 0x5, 268, 0, 141 },
{ 0x3, 268, 0, 142 },
{ 0x140, 277, 0, 119 },
{ 0x540, 277, 0, 117 },
{ 0x340, 277, 0, 118 },
{ 0xc0, 277, 0, 131 },
{ 0x2c0, 277, 0, 129 },
{ 0x1c0, 277, 0, 130 },
{ 0x20, 277, 0, 146 },
{ 0xa0, 277, 0, 144 },
{ 0x60, 277, 0, 145 },
{ 0x10, 277, 0, 158 },
{ 0x50, 277, 0, 156 },
{ 0x30, 277, 0, 157 },
{ 0x8, 277, 0, 170 },
{ 0x28, 277, 0, 168 },
{ 0x18, 277, 0, 169 },
{ 0x4, 277, 0, 180 },
{ 0x2, 277, 0, 181 },
{ 0x1, 277, 0, 182 },
{ 0x140, 271, 0, 122 },
{ 0x540, 271, 0, 120 },
{ 0x340, 271, 0, 121 },
{ 0xc0, 271, 0, 134 },
{ 0x2c0, 271, 0, 132 },
{ 0x1c0, 271, 0, 133 },
{ 0x20, 271, 0, 149 },
{ 0xa0, 271, 0, 147 },
{ 0x60, 271, 0, 148 },
{ 0x10, 271, 0, 161 },
{ 0x50, 271, 0, 159 },
{ 0x30, 271, 0, 160 },
{ 0x8, 271, 0, 173 },
{ 0x28, 271, 0, 171 },
{ 0x18, 271, 0, 172 },
{ 0x4, 271, 0, 183 },
{ 0x2, 271, 0, 184 },
{ 0x1, 271, 0, 185 },
{ 0x140, 274, 0, 125 },
{ 0x540, 274, 0, 123 },
{ 0x340, 274, 0, 124 },
{ 0xc0, 274, 0, 137 },
{ 0x2c0, 274, 0, 135 },
{ 0x1c0, 274, 0, 136 },
{ 0x20, 274, 0, 152 },
{ 0xa0, 274, 0, 150 },
{ 0x60, 274, 0, 151 },
{ 0x10, 274, 0, 164 },
{ 0x50, 274, 0, 162 },
{ 0x30, 274, 0, 163 },
{ 0x8, 274, 0, 176 },
{ 0x28, 274, 0, 174 },
{ 0x18, 274, 0, 175 },
{ 0x4, 274, 0, 186 },
{ 0x2, 274, 0, 187 },
{ 0x1, 274, 0, 188 },
{ 0x140, 286, 0, 128 },
{ 0x540, 286, 0, 126 },
{ 0x340, 286, 0, 127 },
{ 0xc0, 286, 0, 140 },
{ 0x2c0, 286, 0, 138 },
{ 0x1c0, 286, 0, 139 },
{ 0x20, 286, 0, 155 },
{ 0xa0, 286, 0, 153 },
{ 0x60, 286, 0, 154 },
{ 0x10, 286, 0, 167 },
{ 0x50, 286, 0, 165 },
{ 0x30, 286, 0, 166 },
{ 0x8, 286, 0, 179 },
{ 0x28, 286, 0, 177 },
{ 0x18, 286, 0, 178 },
{ 0x4, 286, 0, 189 },
{ 0x2, 286, 0, 190 },
{ 0x1, 286, 0, 191 },
{ 0x8, 390, 0, 192 },
{ 0x4, 390, 0, 193 },
{ 0x2, 390, 0, 194 },
{ 0x1, 390, 0, 195 },
{ 0x20, 288, 0, 203 },
{ 0x220, 288, 0, 199 },
{ 0x1220, 288, 0, 196 },
{ 0xa20, 288, 0, 197 },
{ 0x620, 288, 0, 198 },
{ 0x120, 288, 0, 200 },
{ 0xa0, 288, 0, 201 },
{ 0x60, 288, 0, 202 },
{ 0x10, 288, 0, 207 },
{ 0x90, 288, 0, 204 },
{ 0x50, 288, 0, 205 },
{ 0x30, 288, 0, 206 },
{ 0x8, 288, 0, 208 },
{ 0x4, 288, 0, 209 },
{ 0x2, 288, 0, 210 },
{ 0x1, 288, 0, 211 },
{ 0x20, 287, 0, 219 },
{ 0x220, 287, 0, 215 },
{ 0x1220, 287, 0, 212 },
{ 0xa20, 287, 0, 213 },
{ 0x620, 287, 0, 214 },
{ 0x120, 287, 0, 216 },
{ 0xa0, 287, 0, 217 },
{ 0x60, 287, 0, 218 },
{ 0x10, 287, 0, 223 },
{ 0x90, 287, 0, 220 },
{ 0x50, 287, 0, 221 },
{ 0x30, 287, 0, 222 },
{ 0x8, 287, 0, 224 },
{ 0x4, 287, 0, 225 },
{ 0x2, 287, 0, 226 },
{ 0x1, 287, 0, 227 },
{ 0x140, 279, 0, 230 },
{ 0x540, 279, 0, 228 },
{ 0x340, 279, 0, 229 },
{ 0xc0, 279, 0, 239 },
{ 0x2c0, 279, 0, 237 },
{ 0x1c0, 279, 0, 238 },
{ 0x20, 279, 0, 248 },
{ 0xa0, 279, 0, 246 },
{ 0x60, 279, 0, 247 },
{ 0x10, 279, 0, 257 },
{ 0x50, 279, 0, 255 },
{ 0x30, 279, 0, 256 },
{ 0x8, 279, 0, 266 },
{ 0x28, 279, 0, 264 },
{ 0x18, 279, 0, 265 },
{ 0x4, 279, 0, 273 },
{ 0x2, 279, 0, 274 },
{ 0x1, 279, 0, 275 },
{ 0x140, 281, 0, 233 },
{ 0x540, 281, 0, 231 },
{ 0x340, 281, 0, 232 },
{ 0xc0, 281, 0, 242 },
{ 0x2c0, 281, 0, 240 },
{ 0x1c0, 281, 0, 241 },
{ 0x20, 281, 0, 251 },
{ 0xa0, 281, 0, 249 },
{ 0x60, 281, 0, 250 },
{ 0x10, 281, 0, 260 },
{ 0x50, 281, 0, 258 },
{ 0x30, 281, 0, 259 },
{ 0x8, 281, 0, 269 },
{ 0x28, 281, 0, 267 },
{ 0x18, 281, 0, 268 },
{ 0x4, 281, 0, 276 },
{ 0x2, 281, 0, 277 },
{ 0x1, 281, 0, 278 },
{ 0x140, 283, 0, 236 },
{ 0x540, 283, 0, 234 },
{ 0x340, 283, 0, 235 },
{ 0xc0, 283, 0, 245 },
{ 0x2c0, 283, 0, 243 },
{ 0x1c0, 283, 0, 244 },
{ 0x20, 283, 0, 254 },
{ 0xa0, 283, 0, 252 },
{ 0x60, 283, 0, 253 },
{ 0x10, 283, 0, 263 },
{ 0x50, 283, 0, 261 },
{ 0x30, 283, 0, 262 },
{ 0x8, 283, 0, 272 },
{ 0x28, 283, 0, 270 },
{ 0x18, 283, 0, 271 },
{ 0x4, 283, 0, 279 },
{ 0x2, 283, 0, 280 },
{ 0x1, 283, 0, 281 },
{ 0x140, 278, 0, 284 },
{ 0x540, 278, 0, 282 },
{ 0x340, 278, 0, 283 },
{ 0xc0, 278, 0, 293 },
{ 0x2c0, 278, 0, 291 },
{ 0x1c0, 278, 0, 292 },
{ 0x20, 278, 0, 302 },
{ 0xa0, 278, 0, 300 },
{ 0x60, 278, 0, 301 },
{ 0x10, 278, 0, 311 },
{ 0x50, 278, 0, 309 },
{ 0x30, 278, 0, 310 },
{ 0x8, 278, 0, 320 },
{ 0x28, 278, 0, 318 },
{ 0x18, 278, 0, 319 },
{ 0x4, 278, 0, 327 },
{ 0x2, 278, 0, 328 },
{ 0x1, 278, 0, 329 },
{ 0x140, 280, 0, 287 },
{ 0x540, 280, 0, 285 },
{ 0x340, 280, 0, 286 },
{ 0xc0, 280, 0, 296 },
{ 0x2c0, 280, 0, 294 },
{ 0x1c0, 280, 0, 295 },
{ 0x20, 280, 0, 305 },
{ 0xa0, 280, 0, 303 },
{ 0x60, 280, 0, 304 },
{ 0x10, 280, 0, 314 },
{ 0x50, 280, 0, 312 },
{ 0x30, 280, 0, 313 },
{ 0x8, 280, 0, 323 },
{ 0x28, 280, 0, 321 },
{ 0x18, 280, 0, 322 },
{ 0x4, 280, 0, 330 },
{ 0x2, 280, 0, 331 },
{ 0x1, 280, 0, 332 },
{ 0x140, 282, 0, 290 },
{ 0x540, 282, 0, 288 },
{ 0x340, 282, 0, 289 },
{ 0xc0, 282, 0, 299 },
{ 0x2c0, 282, 0, 297 },
{ 0x1c0, 282, 0, 298 },
{ 0x20, 282, 0, 308 },
{ 0xa0, 282, 0, 306 },
{ 0x60, 282, 0, 307 },
{ 0x10, 282, 0, 317 },
{ 0x50, 282, 0, 315 },
{ 0x30, 282, 0, 316 },
{ 0x8, 282, 0, 326 },
{ 0x28, 282, 0, 324 },
{ 0x18, 282, 0, 325 },
{ 0x4, 282, 0, 333 },
{ 0x2, 282, 0, 334 },
{ 0x1, 282, 0, 335 },
{ 0x1, 410, 0, 337 },
{ 0x3, 410, 0, 336 },
{ 0x2, 416, 0, 338 },
{ 0x1, 416, 0, 339 },
{ 0x2, 412, 0, 340 },
{ 0x1, 412, 0, 341 },
{ 0x2, 414, 0, 342 },
{ 0x1, 414, 0, 343 },
{ 0x2, 418, 0, 344 },
{ 0x1, 418, 0, 345 },
{ 0x1, 267, 0, 372 },
{ 0x5, 267, 0, 370 },
{ 0x3, 267, 0, 371 },
{ 0x140, 276, 0, 348 },
{ 0x540, 276, 0, 346 },
{ 0x340, 276, 0, 347 },
{ 0xc0, 276, 0, 360 },
{ 0x2c0, 276, 0, 358 },
{ 0x1c0, 276, 0, 359 },
{ 0x20, 276, 0, 375 },
{ 0xa0, 276, 0, 373 },
{ 0x60, 276, 0, 374 },
{ 0x10, 276, 0, 387 },
{ 0x50, 276, 0, 385 },
{ 0x30, 276, 0, 386 },
{ 0x8, 276, 0, 399 },
{ 0x28, 276, 0, 397 },
{ 0x18, 276, 0, 398 },
{ 0x4, 276, 0, 409 },
{ 0x2, 276, 0, 410 },
{ 0x1, 276, 0, 411 },
{ 0x140, 270, 0, 351 },
{ 0x540, 270, 0, 349 },
{ 0x340, 270, 0, 350 },
{ 0xc0, 270, 0, 363 },
{ 0x2c0, 270, 0, 361 },
{ 0x1c0, 270, 0, 362 },
{ 0x20, 270, 0, 378 },
{ 0xa0, 270, 0, 376 },
{ 0x60, 270, 0, 377 },
{ 0x10, 270, 0, 390 },
{ 0x50, 270, 0, 388 },
{ 0x30, 270, 0, 389 },
{ 0x8, 270, 0, 402 },
{ 0x28, 270, 0, 400 },
{ 0x18, 270, 0, 401 },
{ 0x4, 270, 0, 412 },
{ 0x2, 270, 0, 413 },
{ 0x1, 270, 0, 414 },
{ 0x140, 273, 0, 354 },
{ 0x540, 273, 0, 352 },
{ 0x340, 273, 0, 353 },
{ 0xc0, 273, 0, 366 },
{ 0x2c0, 273, 0, 364 },
{ 0x1c0, 273, 0, 365 },
{ 0x20, 273, 0, 381 },
{ 0xa0, 273, 0, 379 },
{ 0x60, 273, 0, 380 },
{ 0x10, 273, 0, 393 },
{ 0x50, 273, 0, 391 },
{ 0x30, 273, 0, 392 },
{ 0x8, 273, 0, 405 },
{ 0x28, 273, 0, 403 },
{ 0x18, 273, 0, 404 },
{ 0x4, 273, 0, 415 },
{ 0x2, 273, 0, 416 },
{ 0x1, 273, 0, 417 },
{ 0x140, 285, 0, 357 },
{ 0x540, 285, 0, 355 },
{ 0x340, 285, 0, 356 },
{ 0xc0, 285, 0, 369 },
{ 0x2c0, 285, 0, 367 },
{ 0x1c0, 285, 0, 368 },
{ 0x20, 285, 0, 384 },
{ 0xa0, 285, 0, 382 },
{ 0x60, 285, 0, 383 },
{ 0x10, 285, 0, 396 },
{ 0x50, 285, 0, 394 },
{ 0x30, 285, 0, 395 },
{ 0x8, 285, 0, 408 },
{ 0x28, 285, 0, 406 },
{ 0x18, 285, 0, 407 },
{ 0x4, 285, 0, 418 },
{ 0x2, 285, 0, 419 },
{ 0x1, 285, 0, 420 },
{ 0x1, 266, 0, 447 },
{ 0x5, 266, 0, 445 },
{ 0x3, 266, 0, 446 },
{ 0x140, 275, 0, 423 },
{ 0x540, 275, 0, 421 },
{ 0x340, 275, 0, 422 },
{ 0xc0, 275, 0, 435 },
{ 0x2c0, 275, 0, 433 },
{ 0x1c0, 275, 0, 434 },
{ 0x20, 275, 0, 450 },
{ 0xa0, 275, 0, 448 },
{ 0x60, 275, 0, 449 },
{ 0x10, 275, 0, 462 },
{ 0x50, 275, 0, 460 },
{ 0x30, 275, 0, 461 },
{ 0x8, 275, 0, 474 },
{ 0x28, 275, 0, 472 },
{ 0x18, 275, 0, 473 },
{ 0x4, 275, 0, 484 },
{ 0x2, 275, 0, 485 },
{ 0x1, 275, 0, 486 },
{ 0x140, 269, 0, 426 },
{ 0x540, 269, 0, 424 },
{ 0x340, 269, 0, 425 },
{ 0xc0, 269, 0, 438 },
{ 0x2c0, 269, 0, 436 },
{ 0x1c0, 269, 0, 437 },
{ 0x20, 269, 0, 453 },
{ 0xa0, 269, 0, 451 },
{ 0x60, 269, 0, 452 },
{ 0x10, 269, 0, 465 },
{ 0x50, 269, 0, 463 },
{ 0x30, 269, 0, 464 },
{ 0x8, 269, 0, 477 },
{ 0x28, 269, 0, 475 },
{ 0x18, 269, 0, 476 },
{ 0x4, 269, 0, 487 },
{ 0x2, 269, 0, 488 },
{ 0x1, 269, 0, 489 },
{ 0x140, 272, 0, 429 },
{ 0x540, 272, 0, 427 },
{ 0x340, 272, 0, 428 },
{ 0xc0, 272, 0, 441 },
{ 0x2c0, 272, 0, 439 },
{ 0x1c0, 272, 0, 440 },
{ 0x20, 272, 0, 456 },
{ 0xa0, 272, 0, 454 },
{ 0x60, 272, 0, 455 },
{ 0x10, 272, 0, 468 },
{ 0x50, 272, 0, 466 },
{ 0x30, 272, 0, 467 },
{ 0x8, 272, 0, 480 },
{ 0x28, 272, 0, 478 },
{ 0x18, 272, 0, 479 },
{ 0x4, 272, 0, 490 },
{ 0x2, 272, 0, 491 },
{ 0x1, 272, 0, 492 },
{ 0x140, 284, 0, 432 },
{ 0x540, 284, 0, 430 },
{ 0x340, 284, 0, 431 },
{ 0xc0, 284, 0, 444 },
{ 0x2c0, 284, 0, 442 },
{ 0x1c0, 284, 0, 443 },
{ 0x20, 284, 0, 459 },
{ 0xa0, 284, 0, 457 },
{ 0x60, 284, 0, 458 },
{ 0x10, 284, 0, 471 },
{ 0x50, 284, 0, 469 },
{ 0x30, 284, 0, 470 },
{ 0x8, 284, 0, 483 },
{ 0x28, 284, 0, 481 },
{ 0x18, 284, 0, 482 },
{ 0x4, 284, 0, 493 },
{ 0x2, 284, 0, 494 },
{ 0x1, 284, 0, 495 },
{ 0x8, 409, 0, 497 },
{ 0x18, 409, 0, 496 },
{ 0x4, 409, 0, 499 },
{ 0xc, 409, 0, 498 },
{ 0x2, 409, 0, 506 },
{ 0x1, 409, 0, 507 },
{ 0x4, 407, 0, 501 },
{ 0xc, 407, 0, 500 },
{ 0x2, 407, 0, 508 },
{ 0x1, 407, 0, 509 },
{ 0x4, 405, 0, 503 },
{ 0xc, 405, 0, 502 },
{ 0x2, 405, 0, 510 },
{ 0x1, 405, 0, 511 },
{ 0x4, 401, 0, 505 },
{ 0xc, 401, 0, 504 },
{ 0x2, 401, 0, 512 },
{ 0x1, 401, 0, 513 },
{ 0xa00, 265, 0, 528 },
{ 0x2a00, 265, 0, 526 },
{ 0x1a00, 265, 0, 527 },
{ 0x600, 265, 0, 540 },
{ 0x2600, 265, 0, 516 },
{ 0xa600, 265, 0, 514 },
{ 0x6600, 265, 0, 515 },
{ 0x1600, 265, 0, 538 },
{ 0xe00, 265, 0, 539 },
{ 0x100, 265, 0, 552 },
{ 0x500, 265, 0, 550 },
{ 0x300, 265, 0, 551 },
{ 0x80, 265, 0, 555 },
{ 0x280, 265, 0, 553 },
{ 0x180, 265, 0, 554 },
{ 0x40, 265, 0, 567 },
{ 0x140, 265, 0, 565 },
{ 0xc0, 265, 0, 566 },
{ 0x20, 265, 0, 579 },
{ 0xa0, 265, 0, 577 },
{ 0x60, 265, 0, 578 },
{ 0x10, 265, 0, 591 },
{ 0x50, 265, 0, 589 },
{ 0x30, 265, 0, 590 },
{ 0x8, 265, 0, 603 },
{ 0x28, 265, 0, 601 },
{ 0x18, 265, 0, 602 },
{ 0x4, 265, 0, 613 },
{ 0x2, 265, 0, 614 },
{ 0x1, 265, 0, 615 },
{ 0x500, 261, 0, 531 },
{ 0x1500, 261, 0, 529 },
{ 0xd00, 261, 0, 530 },
{ 0x300, 261, 0, 543 },
{ 0x1300, 261, 0, 519 },
{ 0x5300, 261, 0, 517 },
{ 0x3300, 261, 0, 518 },
{ 0xb00, 261, 0, 541 },
{ 0x700, 261, 0, 542 },
{ 0x80, 261, 0, 558 },
{ 0x280, 261, 0, 556 },
{ 0x180, 261, 0, 557 },
{ 0x40, 261, 0, 570 },
{ 0x140, 261, 0, 568 },
{ 0xc0, 261, 0, 569 },
{ 0x20, 261, 0, 582 },
{ 0xa0, 261, 0, 580 },
{ 0x60, 261, 0, 581 },
{ 0x10, 261, 0, 594 },
{ 0x50, 261, 0, 592 },
{ 0x30, 261, 0, 593 },
{ 0x8, 261, 0, 606 },
{ 0x28, 261, 0, 604 },
{ 0x18, 261, 0, 605 },
{ 0x4, 261, 0, 616 },
{ 0x2, 261, 0, 617 },
{ 0x1, 261, 0, 618 },
{ 0x500, 258, 0, 534 },
{ 0x1500, 258, 0, 532 },
{ 0xd00, 258, 0, 533 },
{ 0x300, 258, 0, 546 },
{ 0x1300, 258, 0, 522 },
{ 0x5300, 258, 0, 520 },
{ 0x3300, 258, 0, 521 },
{ 0xb00, 258, 0, 544 },
{ 0x700, 258, 0, 545 },
{ 0x80, 258, 0, 561 },
{ 0x280, 258, 0, 559 },
{ 0x180, 258, 0, 560 },
{ 0x40, 258, 0, 573 },
{ 0x140, 258, 0, 571 },
{ 0xc0, 258, 0, 572 },
{ 0x20, 258, 0, 585 },
{ 0xa0, 258, 0, 583 },
{ 0x60, 258, 0, 584 },
{ 0x10, 258, 0, 597 },
{ 0x50, 258, 0, 595 },
{ 0x30, 258, 0, 596 },
{ 0x8, 258, 0, 609 },
{ 0x28, 258, 0, 607 },
{ 0x18, 258, 0, 608 },
{ 0x4, 258, 0, 619 },
{ 0x2, 258, 0, 620 },
{ 0x1, 258, 0, 621 },
{ 0x500, 253, 0, 537 },
{ 0x1500, 253, 0, 535 },
{ 0xd00, 253, 0, 536 },
{ 0x300, 253, 0, 549 },
{ 0x1300, 253, 0, 525 },
{ 0x5300, 253, 0, 523 },
{ 0x3300, 253, 0, 524 },
{ 0xb00, 253, 0, 547 },
{ 0x700, 253, 0, 548 },
{ 0x80, 253, 0, 564 },
{ 0x280, 253, 0, 562 },
{ 0x180, 253, 0, 563 },
{ 0x40, 253, 0, 576 },
{ 0x140, 253, 0, 574 },
{ 0xc0, 253, 0, 575 },
{ 0x20, 253, 0, 588 },
{ 0xa0, 253, 0, 586 },
{ 0x60, 253, 0, 587 },
{ 0x10, 253, 0, 600 },
{ 0x50, 253, 0, 598 },
{ 0x30, 253, 0, 599 },
{ 0x8, 253, 0, 612 },
{ 0x28, 253, 0, 610 },
{ 0x18, 253, 0, 611 },
{ 0x4, 253, 0, 622 },
{ 0x2, 253, 0, 623 },
{ 0x1, 253, 0, 624 },
{ 0x8, 238, 0, 625 },
{ 0x4, 238, 0, 626 },
{ 0x2, 238, 0, 627 },
{ 0x1, 238, 0, 628 },
{ 0x2, 176, 0, 631 },
{ 0xa, 176, 0, 629 },
{ 0x6, 176, 0, 630 },
{ 0x1, 176, 0, 637 },
{ 0x5, 176, 0, 635 },
{ 0x3, 176, 0, 636 },
{ 0x2, 175, 0, 634 },
{ 0xa, 175, 0, 632 },
{ 0x6, 175, 0, 633 },
{ 0x1, 175, 0, 640 },
{ 0x5, 175, 0, 638 },
{ 0x3, 175, 0, 639 },
{ 0x4, 451, 0, 641 },
{ 0x2, 451, 0, 642 },
{ 0x1, 451, 0, 643 },
{ 0x4, 450, 0, 644 },
{ 0x2, 450, 0, 645 },
{ 0x1, 450, 0, 646 },
{ 0x4, 449, 0, 647 },
{ 0x2, 449, 0, 648 },
{ 0x1, 449, 0, 649 },
{ 0x4, 448, 0, 650 },
{ 0x2, 448, 0, 651 },
{ 0x1, 448, 0, 652 },
{ 0x2, 123, 1, 658 },
{ 0x2, 124, 0, 657 },
{ 0xa, 123, 1, 654 },
{ 0xa, 124, 0, 653 },
{ 0x6, 123, 1, 656 },
{ 0x6, 124, 0, 655 },
{ 0x1, 123, 1, 688 },
{ 0x1, 124, 0, 687 },
{ 0x5, 123, 1, 684 },
{ 0x5, 124, 0, 683 },
{ 0x3, 123, 1, 686 },
{ 0x3, 124, 0, 685 },
{ 0x2, 131, 1, 664 },
{ 0x2, 132, 0, 663 },
{ 0xa, 131, 1, 660 },
{ 0xa, 132, 0, 659 },
{ 0x6, 131, 1, 662 },
{ 0x6, 132, 0, 661 },
{ 0x1, 131, 1, 694 },
{ 0x1, 132, 0, 693 },
{ 0x5, 131, 1, 690 },
{ 0x5, 132, 0, 689 },
{ 0x3, 131, 1, 692 },
{ 0x3, 132, 0, 691 },
{ 0x2, 129, 1, 670 },
{ 0x2, 130, 0, 669 },
{ 0xa, 129, 1, 666 },
{ 0xa, 130, 0, 665 },
{ 0x6, 129, 1, 668 },
{ 0x6, 130, 0, 667 },
{ 0x1, 129, 1, 700 },
{ 0x1, 130, 0, 699 },
{ 0x5, 129, 1, 696 },
{ 0x5, 130, 0, 695 },
{ 0x3, 129, 1, 698 },
{ 0x3, 130, 0, 697 },
{ 0x2, 127, 1, 676 },
{ 0x2, 128, 0, 675 },
{ 0xa, 127, 1, 672 },
{ 0xa, 128, 0, 671 },
{ 0x6, 127, 1, 674 },
{ 0x6, 128, 0, 673 },
{ 0x1, 127, 1, 706 },
{ 0x1, 128, 0, 705 },
{ 0x5, 127, 1, 702 },
{ 0x5, 128, 0, 701 },
{ 0x3, 127, 1, 704 },
{ 0x3, 128, 0, 703 },
{ 0x2, 125, 1, 682 },
{ 0x2, 126, 0, 681 },
{ 0xa, 125, 1, 678 },
{ 0xa, 126, 0, 677 },
{ 0x6, 125, 1, 680 },
{ 0x6, 126, 0, 679 },
{ 0x1, 125, 1, 712 },
{ 0x1, 126, 0, 711 },
{ 0x5, 125, 1, 708 },
{ 0x5, 126, 0, 707 },
{ 0x3, 125, 1, 710 },
{ 0x3, 126, 0, 709 },
{ 0x4, 402, 1, 718 },
{ 0x4, 403, 0, 717 },
{ 0xc, 402, 1, 716 },
{ 0xc, 403, 0, 715 },
{ 0x2, 402, 1, 728 },
{ 0x2, 403, 0, 727 },
{ 0x1, 402, 1, 730 },
{ 0x1, 403, 0, 729 },
{ 0x8, 408, 0, 714 },
{ 0x18, 408, 0, 713 },
{ 0x4, 408, 0, 720 },
{ 0xc, 408, 0, 719 },
{ 0x2, 408, 0, 731 },
{ 0x1, 408, 0, 732 },
{ 0x4, 406, 0, 722 },
{ 0xc, 406, 0, 721 },
{ 0x2, 406, 0, 733 },
{ 0x1, 406, 0, 734 },
{ 0x4, 404, 0, 724 },
{ 0xc, 404, 0, 723 },
{ 0x2, 404, 0, 735 },
{ 0x1, 404, 0, 736 },
{ 0x4, 400, 0, 726 },
{ 0xc, 400, 0, 725 },
{ 0x2, 400, 0, 737 },
{ 0x1, 400, 0, 738 },
{ 0xa00, 264, 0, 753 },
{ 0x2a00, 264, 0, 751 },
{ 0x1a00, 264, 0, 752 },
{ 0x600, 264, 0, 765 },
{ 0x2600, 264, 0, 741 },
{ 0xa600, 264, 0, 739 },
{ 0x6600, 264, 0, 740 },
{ 0x1600, 264, 0, 763 },
{ 0xe00, 264, 0, 764 },
{ 0x100, 264, 0, 777 },
{ 0x500, 264, 0, 775 },
{ 0x300, 264, 0, 776 },
{ 0x80, 264, 0, 780 },
{ 0x280, 264, 0, 778 },
{ 0x180, 264, 0, 779 },
{ 0x40, 264, 0, 792 },
{ 0x140, 264, 0, 790 },
{ 0xc0, 264, 0, 791 },
{ 0x20, 264, 0, 804 },
{ 0xa0, 264, 0, 802 },
{ 0x60, 264, 0, 803 },
{ 0x10, 264, 0, 816 },
{ 0x50, 264, 0, 814 },
{ 0x30, 264, 0, 815 },
{ 0x8, 264, 0, 828 },
{ 0x28, 264, 0, 826 },
{ 0x18, 264, 0, 827 },
{ 0x4, 264, 0, 838 },
{ 0x2, 264, 0, 839 },
{ 0x1, 264, 0, 840 },
{ 0x500, 260, 0, 756 },
{ 0x1500, 260, 0, 754 },
{ 0xd00, 260, 0, 755 },
{ 0x300, 260, 0, 768 },
{ 0x1300, 260, 0, 744 },
{ 0x5300, 260, 0, 742 },
{ 0x3300, 260, 0, 743 },
{ 0xb00, 260, 0, 766 },
{ 0x700, 260, 0, 767 },
{ 0x80, 260, 0, 783 },
{ 0x280, 260, 0, 781 },
{ 0x180, 260, 0, 782 },
{ 0x40, 260, 0, 795 },
{ 0x140, 260, 0, 793 },
{ 0xc0, 260, 0, 794 },
{ 0x20, 260, 0, 807 },
{ 0xa0, 260, 0, 805 },
{ 0x60, 260, 0, 806 },
{ 0x10, 260, 0, 819 },
{ 0x50, 260, 0, 817 },
{ 0x30, 260, 0, 818 },
{ 0x8, 260, 0, 831 },
{ 0x28, 260, 0, 829 },
{ 0x18, 260, 0, 830 },
{ 0x4, 260, 0, 841 },
{ 0x2, 260, 0, 842 },
{ 0x1, 260, 0, 843 },
{ 0x500, 257, 0, 759 },
{ 0x1500, 257, 0, 757 },
{ 0xd00, 257, 0, 758 },
{ 0x300, 257, 0, 771 },
{ 0x1300, 257, 0, 747 },
{ 0x5300, 257, 0, 745 },
{ 0x3300, 257, 0, 746 },
{ 0xb00, 257, 0, 769 },
{ 0x700, 257, 0, 770 },
{ 0x80, 257, 0, 786 },
{ 0x280, 257, 0, 784 },
{ 0x180, 257, 0, 785 },
{ 0x40, 257, 0, 798 },
{ 0x140, 257, 0, 796 },
{ 0xc0, 257, 0, 797 },
{ 0x20, 257, 0, 810 },
{ 0xa0, 257, 0, 808 },
{ 0x60, 257, 0, 809 },
{ 0x10, 257, 0, 822 },
{ 0x50, 257, 0, 820 },
{ 0x30, 257, 0, 821 },
{ 0x8, 257, 0, 834 },
{ 0x28, 257, 0, 832 },
{ 0x18, 257, 0, 833 },
{ 0x4, 257, 0, 844 },
{ 0x2, 257, 0, 845 },
{ 0x1, 257, 0, 846 },
{ 0x500, 252, 0, 762 },
{ 0x1500, 252, 0, 760 },
{ 0xd00, 252, 0, 761 },
{ 0x300, 252, 0, 774 },
{ 0x1300, 252, 0, 750 },
{ 0x5300, 252, 0, 748 },
{ 0x3300, 252, 0, 749 },
{ 0xb00, 252, 0, 772 },
{ 0x700, 252, 0, 773 },
{ 0x80, 252, 0, 789 },
{ 0x280, 252, 0, 787 },
{ 0x180, 252, 0, 788 },
{ 0x40, 252, 0, 801 },
{ 0x140, 252, 0, 799 },
{ 0xc0, 252, 0, 800 },
{ 0x20, 252, 0, 813 },
{ 0xa0, 252, 0, 811 },
{ 0x60, 252, 0, 812 },
{ 0x10, 252, 0, 825 },
{ 0x50, 252, 0, 823 },
{ 0x30, 252, 0, 824 },
{ 0x8, 252, 0, 837 },
{ 0x28, 252, 0, 835 },
{ 0x18, 252, 0, 836 },
{ 0x4, 252, 0, 847 },
{ 0x2, 252, 0, 848 },
{ 0x1, 252, 0, 849 },
{ 0x8, 254, 1, 895 },
{ 0x8, 255, 0, 894 },
{ 0x28, 254, 1, 891 },
{ 0x28, 255, 0, 890 },
{ 0x18, 254, 1, 893 },
{ 0x18, 255, 0, 892 },
{ 0x4, 254, 1, 957 },
{ 0x4, 255, 0, 956 },
{ 0x2, 254, 1, 959 },
{ 0x2, 255, 0, 958 },
{ 0x1, 254, 1, 961 },
{ 0x1, 255, 0, 960 },
{ 0xa00, 262, 0, 865 },
{ 0x2a00, 262, 0, 863 },
{ 0x1a00, 262, 0, 864 },
{ 0x600, 262, 0, 877 },
{ 0x2600, 262, 0, 853 },
{ 0xa600, 262, 0, 851 },
{ 0x6600, 262, 0, 852 },
{ 0x1600, 262, 0, 875 },
{ 0xe00, 262, 0, 876 },
{ 0x100, 262, 0, 889 },
{ 0x500, 262, 0, 887 },
{ 0x300, 262, 0, 888 },
{ 0x80, 262, 0, 898 },
{ 0x280, 262, 0, 896 },
{ 0x180, 262, 0, 897 },
{ 0x40, 262, 0, 910 },
{ 0x140, 262, 0, 908 },
{ 0xc0, 262, 0, 909 },
{ 0x20, 262, 0, 922 },
{ 0xa0, 262, 0, 920 },
{ 0x60, 262, 0, 921 },
{ 0x10, 262, 0, 934 },
{ 0x50, 262, 0, 932 },
{ 0x30, 262, 0, 933 },
{ 0x8, 262, 0, 946 },
{ 0x28, 262, 0, 944 },
{ 0x18, 262, 0, 945 },
{ 0x4, 262, 0, 962 },
{ 0x2, 262, 0, 963 },
{ 0x1, 262, 1, 964 },
{ 0x1, 263, 0, 850 },
{ 0x500, 259, 0, 868 },
{ 0x1500, 259, 0, 866 },
{ 0xd00, 259, 0, 867 },
{ 0x300, 259, 0, 880 },
{ 0x1300, 259, 0, 856 },
{ 0x5300, 259, 0, 854 },
{ 0x3300, 259, 0, 855 },
{ 0xb00, 259, 0, 878 },
{ 0x700, 259, 0, 879 },
{ 0x80, 259, 0, 901 },
{ 0x280, 259, 0, 899 },
{ 0x180, 259, 0, 900 },
{ 0x40, 259, 0, 913 },
{ 0x140, 259, 0, 911 },
{ 0xc0, 259, 0, 912 },
{ 0x20, 259, 0, 925 },
{ 0xa0, 259, 0, 923 },
{ 0x60, 259, 0, 924 },
{ 0x10, 259, 0, 937 },
{ 0x50, 259, 0, 935 },
{ 0x30, 259, 0, 936 },
{ 0x8, 259, 0, 949 },
{ 0x28, 259, 0, 947 },
{ 0x18, 259, 0, 948 },
{ 0x4, 259, 0, 965 },
{ 0x2, 259, 0, 966 },
{ 0x1, 259, 0, 967 },
{ 0x500, 256, 0, 871 },
{ 0x1500, 256, 0, 869 },
{ 0xd00, 256, 0, 870 },
{ 0x300, 256, 0, 883 },
{ 0x1300, 256, 0, 859 },
{ 0x5300, 256, 0, 857 },
{ 0x3300, 256, 0, 858 },
{ 0xb00, 256, 0, 881 },
{ 0x700, 256, 0, 882 },
{ 0x80, 256, 0, 904 },
{ 0x280, 256, 0, 902 },
{ 0x180, 256, 0, 903 },
{ 0x40, 256, 0, 916 },
{ 0x140, 256, 0, 914 },
{ 0xc0, 256, 0, 915 },
{ 0x20, 256, 0, 928 },
{ 0xa0, 256, 0, 926 },
{ 0x60, 256, 0, 927 },
{ 0x10, 256, 0, 940 },
{ 0x50, 256, 0, 938 },
{ 0x30, 256, 0, 939 },
{ 0x8, 256, 0, 952 },
{ 0x28, 256, 0, 950 },
{ 0x18, 256, 0, 951 },
{ 0x4, 256, 0, 968 },
{ 0x2, 256, 0, 969 },
{ 0x1, 256, 0, 970 },
{ 0x500, 251, 0, 874 },
{ 0x1500, 251, 0, 872 },
{ 0xd00, 251, 0, 873 },
{ 0x300, 251, 0, 886 },
{ 0x1300, 251, 0, 862 },
{ 0x5300, 251, 0, 860 },
{ 0x3300, 251, 0, 861 },
{ 0xb00, 251, 0, 884 },
{ 0x700, 251, 0, 885 },
{ 0x80, 251, 0, 907 },
{ 0x280, 251, 0, 905 },
{ 0x180, 251, 0, 906 },
{ 0x40, 251, 0, 919 },
{ 0x140, 251, 0, 917 },
{ 0xc0, 251, 0, 918 },
{ 0x20, 251, 0, 931 },
{ 0xa0, 251, 0, 929 },
{ 0x60, 251, 0, 930 },
{ 0x10, 251, 0, 943 },
{ 0x50, 251, 0, 941 },
{ 0x30, 251, 0, 942 },
{ 0x8, 251, 0, 955 },
{ 0x28, 251, 0, 953 },
{ 0x18, 251, 0, 954 },
{ 0x4, 251, 0, 971 },
{ 0x2, 251, 0, 972 },
{ 0x1, 251, 0, 973 },
{ 0x2, 150, 0, 975 },
{ 0x1, 150, 0, 976 },
{ 0x1, 50, 0, 977 },
{ 0x3, 49, 0, 978 },
{ 0x1, 428, 0, 979 },
{ 0x1, 442, 0, 980 },
{ 0x2, 386, 0, 983 },
{ 0x1, 386, 0, 984 },
{ 0x2, 384, 0, 985 },
{ 0x1, 384, 0, 986 },
{ 0x1, 383, 0, 987 },
{ 0x1, 328, 0, 992 },
{ 0x1, 327, 0, 993 },
{ 0x1, 326, 0, 994 },
{ 0x1, 325, 0, 995 },
{ 0x1, 250, 0, 996 },
{ 0x1, 249, 0, 997 },
{ 0x1, 324, 0, 998 },
{ 0x1, 323, 0, 999 },
{ 0x1, 322, 0, 1000 },
{ 0x1, 321, 0, 1001 },
{ 0x1, 320, 0, 1002 },
{ 0x1, 319, 0, 1003 },
{ 0x1, 318, 0, 1004 },
{ 0x2, 248, 0, 1005 },
{ 0x1, 248, 0, 1006 },
{ 0x2, 366, 0, 1012 },
{ 0x1, 366, 0, 1013 },
{ 0x1, 317, 0, 1014 },
{ 0x1, 316, 0, 1015 },
{ 0x1, 315, 0, 1016 },
{ 0x1, 314, 0, 1017 },
{ 0x1, 8, 1, 1019 },
{ 0x1, 9, 0, 1018 },
{ 0x1, 313, 0, 1020 },
{ 0x1, 312, 0, 1021 },
{ 0x1, 311, 0, 1022 },
{ 0x1, 310, 0, 1023 },
{ 0x1, 388, 0, 1024 },
{ 0x1, 399, 0, 1025 },
{ 0x1, 389, 0, 1026 },
{ 0x1, 423, 0, 1027 },
{ 0x1, 309, 0, 1031 },
{ 0x1, 247, 0, 1032 },
{ 0x1, 177, 0, 1035 },
{ 0x2, 291, 0, 1039 },
{ 0x1, 291, 0, 1040 },
{ 0x1, 236, 0, 1041 },
{ 0x5, 48, 0, 1043 },
{ 0x3, 48, 0, 1044 },
{ 0x5, 47, 0, 1045 },
{ 0x3, 47, 0, 1046 },
{ 0x1, 365, 0, 1047 },
{ 0x1, 373, 0, 1048 },
{ 0x1, 371, 0, 1049 },
{ 0x1, 392, 0, 1050 },
{ 0x1, 372, 0, 1051 },
{ 0x1, 370, 0, 1052 },
{ 0x2, 378, 0, 1053 },
{ 0x1, 378, 0, 1055 },
{ 0x2, 376, 0, 1054 },
{ 0x1, 376, 0, 1056 },
{ 0x2, 396, 0, 1057 },
{ 0x1, 396, 0, 1060 },
{ 0x2, 377, 0, 1058 },
{ 0x1, 377, 0, 1061 },
{ 0x2, 375, 0, 1059 },
{ 0x1, 375, 0, 1062 },
{ 0x1, 338, 0, 1063 },
{ 0x1, 337, 0, 1064 },
{ 0x1, 369, 0, 1065 },
{ 0x1, 360, 0, 1066 },
{ 0x1, 362, 0, 1067 },
{ 0x1, 359, 0, 1068 },
{ 0x1, 361, 0, 1069 },
{ 0x2, 446, 0, 1070 },
{ 0x1, 446, 0, 1073 },
{ 0x2, 445, 0, 1071 },
{ 0x1, 445, 0, 1074 },
{ 0x2, 444, 0, 1072 },
{ 0x1, 444, 0, 1075 },
{ 0x1, 348, 0, 1076 },
{ 0x2, 347, 0, 1077 },
{ 0x1, 347, 0, 1078 },
{ 0x2, 294, 0, 1079 },
{ 0x1, 294, 0, 1082 },
{ 0x2, 293, 0, 1080 },
{ 0x1, 293, 0, 1083 },
{ 0x2, 292, 0, 1081 },
{ 0x1, 292, 0, 1084 },
{ 0x2, 363, 0, 1085 },
{ 0x1, 363, 0, 1086 },
{ 0x2, 364, 0, 1087 },
{ 0x1, 364, 0, 1088 },
{ 0xa, 438, 1, 1100 },
{ 0xa, 439, 1, 1099 },
{ 0xa, 440, 1, 1098 },
{ 0xa, 441, 0, 1097 },
{ 0x1a, 438, 1, 1092 },
{ 0x1a, 439, 1, 1091 },
{ 0x32, 440, 1, 1090 },
{ 0x32, 441, 0, 1089 },
{ 0x6, 438, 1, 1108 },
{ 0x6, 439, 1, 1107 },
{ 0x6, 440, 1, 1106 },
{ 0x6, 441, 0, 1105 },
{ 0x1, 438, 1, 1120 },
{ 0x1, 439, 1, 1119 },
{ 0x1, 440, 1, 1118 },
{ 0x1, 441, 0, 1117 },
{ 0x9, 438, 1, 1104 },
{ 0x9, 439, 1, 1103 },
{ 0x9, 440, 1, 1102 },
{ 0x9, 441, 0, 1101 },
{ 0x19, 438, 1, 1096 },
{ 0x19, 439, 1, 1095 },
{ 0x31, 440, 1, 1094 },
{ 0x31, 441, 0, 1093 },
{ 0x5, 438, 1, 1112 },
{ 0x5, 439, 1, 1111 },
{ 0x5, 440, 1, 1110 },
{ 0x5, 441, 0, 1109 },
{ 0x3, 438, 1, 1116 },
{ 0x3, 439, 1, 1115 },
{ 0x3, 440, 1, 1114 },
{ 0x3, 441, 0, 1113 },
{ 0xa, 429, 1, 1132 },
{ 0xa, 430, 1, 1131 },
{ 0xa, 431, 1, 1130 },
{ 0xa, 432, 0, 1129 },
{ 0x1a, 429, 1, 1124 },
{ 0x1a, 430, 1, 1123 },
{ 0x32, 431, 1, 1122 },
{ 0x32, 432, 0, 1121 },
{ 0x6, 429, 1, 1140 },
{ 0x6, 430, 1, 1139 },
{ 0x6, 431, 1, 1138 },
{ 0x6, 432, 0, 1137 },
{ 0x1, 429, 1, 1152 },
{ 0x1, 430, 1, 1151 },
{ 0x1, 431, 1, 1150 },
{ 0x1, 432, 0, 1149 },
{ 0x9, 429, 1, 1136 },
{ 0x9, 430, 1, 1135 },
{ 0x9, 431, 1, 1134 },
{ 0x9, 432, 0, 1133 },
{ 0x19, 429, 1, 1128 },
{ 0x19, 430, 1, 1127 },
{ 0x31, 431, 1, 1126 },
{ 0x31, 432, 0, 1125 },
{ 0x5, 429, 1, 1144 },
{ 0x5, 430, 1, 1143 },
{ 0x5, 431, 1, 1142 },
{ 0x5, 432, 0, 1141 },
{ 0x3, 429, 1, 1148 },
{ 0x3, 430, 1, 1147 },
{ 0x3, 431, 1, 1146 },
{ 0x3, 432, 0, 1145 },
{ 0xa, 433, 1, 1164 },
{ 0xa, 434, 1, 1163 },
{ 0xa, 435, 1, 1162 },
{ 0xa, 436, 0, 1161 },
{ 0x1a, 433, 1, 1156 },
{ 0x1a, 434, 1, 1155 },
{ 0x32, 435, 1, 1154 },
{ 0x32, 436, 0, 1153 },
{ 0x6, 433, 1, 1172 },
{ 0x6, 434, 1, 1171 },
{ 0x6, 435, 1, 1170 },
{ 0x6, 436, 0, 1169 },
{ 0x1, 433, 1, 1184 },
{ 0x1, 434, 1, 1183 },
{ 0x1, 435, 1, 1182 },
{ 0x1, 436, 0, 1181 },
{ 0x9, 433, 1, 1168 },
{ 0x9, 434, 1, 1167 },
{ 0x9, 435, 1, 1166 },
{ 0x9, 436, 0, 1165 },
{ 0x19, 433, 1, 1160 },
{ 0x19, 434, 1, 1159 },
{ 0x31, 435, 1, 1158 },
{ 0x31, 436, 0, 1157 },
{ 0x5, 433, 1, 1176 },
{ 0x5, 434, 1, 1175 },
{ 0x5, 435, 1, 1174 },
{ 0x5, 436, 0, 1173 },
{ 0x3, 433, 1, 1180 },
{ 0x3, 434, 1, 1179 },
{ 0x3, 435, 1, 1178 },
{ 0x3, 436, 0, 1177 },
{ 0x1, 139, 0, 1185 },
{ 0x1, 138, 0, 1186 },
{ 0x1, 391, 1, 1188 },
{ 0x1, 137, 0, 1187 },
{ 0x2, 395, 1, 1190 },
{ 0x2, 141, 0, 1189 },
{ 0x1, 395, 1, 1192 },
{ 0x1, 141, 0, 1191 },
{ 0x1, 397, 0, 1193 },
{ 0x1, 136, 0, 1194 },
{ 0x2, 135, 0, 1195 },
{ 0x2, 134, 0, 1196 },
{ 0x1, 459, 1, 1202 },
{ 0x1, 246, 0, 1033 },
{ 0x1, 458, 0, 1203 },
{ 0x1, 457, 1, 1204 },
{ 0x1, 245, 0, 1042 },
{ 0x1, 308, 0, 1205 },
{ 0x1, 307, 1, 1206 },
{ 0x1, 290, 0, 1034 },
{ 0x1, 306, 0, 1207 },
{ 0x1, 305, 1, 1208 },
{ 0x1, 427, 0, 1036 },
{ 0x1, 304, 1, 1209 },
{ 0x1, 398, 0, 1038 },
{ 0x1, 303, 0, 1210 },
{ 0x1, 302, 0, 1211 },
{ 0x1, 301, 0, 1212 },
{ 0x1, 300, 1, 1213 },
{ 0x2, 398, 0, 1037 },
{ 0x10, 299, 0, 1217 },
{ 0x90, 299, 0, 1215 },
{ 0x190, 299, 0, 1214 },
{ 0x50, 299, 0, 1216 },
{ 0x30, 299, 0, 1219 },
{ 0x70, 299, 0, 1218 },
{ 0x8, 299, 0, 1221 },
{ 0x18, 299, 0, 1220 },
{ 0x4, 299, 0, 1222 },
{ 0x1, 299, 0, 1225 },
{ 0x3, 299, 0, 1224 },
{ 0x1, 298, 1, 1226 },
{ 0x2, 299, 0, 1223 },
{ 0x3, 46, 0, 1227 },
{ 0x1, 241, 1, 1228 },
{ 0x1, 242, 1, 1028 },
{ 0x1, 243, 0, 88 },
{ 0x1, 341, 1, 1229 },
{ 0x1, 342, 1, 1029 },
{ 0x1, 343, 0, 89 },
{ 0x1, 34, 1, 1230 },
{ 0x1, 35, 1, 1030 },
{ 0x1, 36, 0, 90 },
{ 0x1, 230, 0, 1231 },
{ 0x4, 452, 0, 1232 },
{ 0x2, 452, 0, 1233 },
{ 0x1, 452, 1, 1235 },
{ 0x1, 453, 0, 1234 },
{ 0x8, 454, 0, 1236 },
{ 0x4, 454, 0, 1237 },
{ 0x1, 454, 1, 1239 },
{ 0x2, 454, 0, 1238 },
{ 0x8, 219, 0, 1240 },
{ 0x4, 219, 0, 1241 },
{ 0x2, 219, 0, 1242 },
{ 0x1, 219, 1, 1244 },
{ 0x1, 220, 0, 1243 },
{ 0x10, 221, 0, 1245 },
{ 0x8, 221, 0, 1246 },
{ 0x4, 221, 0, 1247 },
{ 0x1, 221, 1, 1249 },
{ 0x2, 221, 0, 1248 },
{ 0x220, 191, 0, 1250 },
{ 0x120, 191, 0, 1251 },
{ 0xa0, 191, 0, 1252 },
{ 0x60, 191, 1, 1254 },
{ 0x4, 192, 0, 1253 },
{ 0x110, 191, 0, 1260 },
{ 0x90, 191, 0, 1261 },
{ 0x50, 191, 0, 1262 },
{ 0x30, 191, 1, 1264 },
{ 0x2, 192, 0, 1263 },
{ 0x8, 191, 0, 1265 },
{ 0x4, 191, 0, 1266 },
{ 0x2, 191, 0, 1267 },
{ 0x1, 191, 1, 1269 },
{ 0x1, 192, 0, 1268 },
{ 0x440, 193, 0, 1255 },
{ 0x240, 193, 0, 1256 },
{ 0x140, 193, 0, 1257 },
{ 0xc0, 193, 1, 1259 },
{ 0x40, 193, 0, 1258 },
{ 0x220, 193, 0, 1270 },
{ 0x120, 193, 0, 1271 },
{ 0xa0, 193, 0, 1272 },
{ 0x60, 193, 1, 1274 },
{ 0x20, 193, 0, 1273 },
{ 0x10, 193, 0, 1275 },
{ 0x8, 193, 0, 1276 },
{ 0x4, 193, 0, 1277 },
{ 0x1, 193, 1, 1279 },
{ 0x2, 193, 0, 1278 },
{ 0x8, 215, 0, 1280 },
{ 0x4, 215, 0, 1281 },
{ 0x2, 215, 0, 1282 },
{ 0x1, 215, 1, 1284 },
{ 0x1, 216, 0, 1283 },
{ 0x220, 187, 0, 1285 },
{ 0x120, 187, 0, 1286 },
{ 0xa0, 187, 0, 1287 },
{ 0x60, 187, 1, 1289 },
{ 0x4, 188, 0, 1288 },
{ 0x110, 187, 0, 1295 },
{ 0x90, 187, 0, 1296 },
{ 0x50, 187, 0, 1297 },
{ 0x30, 187, 1, 1299 },
{ 0x2, 188, 0, 1298 },
{ 0x8, 187, 0, 1300 },
{ 0x4, 187, 0, 1301 },
{ 0x2, 187, 0, 1302 },
{ 0x1, 187, 1, 1304 },
{ 0x1, 188, 0, 1303 },
{ 0x440, 233, 0, 1290 },
{ 0x240, 233, 0, 1291 },
{ 0x140, 233, 0, 1292 },
{ 0xc0, 233, 1, 1294 },
{ 0x40, 233, 0, 1293 },
{ 0x220, 233, 0, 1305 },
{ 0x120, 233, 0, 1306 },
{ 0xa0, 233, 0, 1307 },
{ 0x60, 233, 1, 1309 },
{ 0x20, 233, 0, 1308 },
{ 0x10, 233, 0, 1310 },
{ 0x8, 233, 0, 1311 },
{ 0x4, 233, 0, 1312 },
{ 0x1, 233, 1, 1314 },
{ 0x2, 233, 0, 1313 },
{ 0x8, 207, 0, 1315 },
{ 0x4, 207, 0, 1316 },
{ 0x2, 207, 0, 1317 },
{ 0x1, 207, 1, 1319 },
{ 0x1, 208, 0, 1318 },
{ 0x10, 214, 0, 1320 },
{ 0x8, 214, 0, 1321 },
{ 0x4, 214, 0, 1322 },
{ 0x1, 214, 1, 1324 },
{ 0x2, 214, 0, 1323 },
{ 0x220, 178, 0, 1325 },
{ 0x120, 178, 0, 1326 },
{ 0xa0, 178, 0, 1327 },
{ 0x60, 178, 1, 1329 },
{ 0x4, 179, 0, 1328 },
{ 0x110, 178, 0, 1350 },
{ 0x90, 178, 0, 1351 },
{ 0x50, 178, 0, 1352 },
{ 0x30, 178, 1, 1354 },
{ 0x2, 179, 0, 1353 },
{ 0x8, 178, 0, 1355 },
{ 0x4, 178, 0, 1356 },
{ 0x2, 178, 0, 1357 },
{ 0x1, 178, 1, 1359 },
{ 0x1, 179, 0, 1358 },
{ 0x440, 186, 0, 1330 },
{ 0x240, 186, 0, 1331 },
{ 0x140, 186, 0, 1332 },
{ 0xc0, 186, 1, 1334 },
{ 0x40, 186, 0, 1333 },
{ 0x220, 186, 0, 1360 },
{ 0x120, 186, 0, 1361 },
{ 0xa0, 186, 0, 1362 },
{ 0x60, 186, 1, 1364 },
{ 0x20, 186, 0, 1363 },
{ 0x10, 186, 0, 1365 },
{ 0x8, 186, 0, 1366 },
{ 0x4, 186, 0, 1367 },
{ 0x1, 186, 1, 1369 },
{ 0x2, 186, 0, 1368 },
{ 0x440, 143, 0, 1335 },
{ 0x240, 143, 0, 1336 },
{ 0x140, 143, 0, 1337 },
{ 0xc0, 143, 1, 1339 },
{ 0x40, 143, 0, 1338 },
{ 0x220, 143, 0, 1370 },
{ 0x120, 143, 0, 1371 },
{ 0xa0, 143, 0, 1372 },
{ 0x60, 143, 1, 1374 },
{ 0x20, 143, 0, 1373 },
{ 0x10, 143, 0, 1375 },
{ 0x8, 143, 0, 1376 },
{ 0x1, 143, 1, 1379 },
{ 0x2, 143, 0, 1378 },
{ 0x440, 194, 1, 1345 },
{ 0x441, 174, 0, 1340 },
{ 0x240, 194, 1, 1346 },
{ 0x241, 174, 0, 1341 },
{ 0x140, 194, 1, 1347 },
{ 0x141, 174, 0, 1342 },
{ 0xc0, 194, 1, 1349 },
{ 0x40, 194, 1, 1348 },
{ 0xc1, 174, 1, 1344 },
{ 0x41, 174, 0, 1343 },
{ 0x220, 194, 1, 1390 },
{ 0x221, 174, 0, 1380 },
{ 0x120, 194, 1, 1391 },
{ 0x121, 174, 0, 1381 },
{ 0xa0, 194, 1, 1392 },
{ 0xa1, 174, 0, 1382 },
{ 0x60, 194, 1, 1394 },
{ 0x20, 194, 1, 1393 },
{ 0x61, 174, 1, 1384 },
{ 0x21, 174, 0, 1383 },
{ 0x10, 194, 1, 1395 },
{ 0x11, 174, 0, 1385 },
{ 0x8, 194, 1, 1396 },
{ 0x9, 174, 0, 1386 },
{ 0x4, 194, 1, 1397 },
{ 0x5, 174, 0, 1387 },
{ 0x1, 194, 1, 1399 },
{ 0x2, 194, 1, 1398 },
{ 0x3, 174, 1, 1389 },
{ 0x1, 174, 0, 1388 },
{ 0x1, 153, 1, 1407 },
{ 0x1, 154, 1, 1406 },
{ 0x1, 155, 1, 1405 },
{ 0x1, 156, 0, 1404 },
{ 0x3, 153, 1, 1403 },
{ 0x3, 154, 1, 1402 },
{ 0x3, 155, 1, 1401 },
{ 0x3, 156, 0, 1400 },
{ 0x1108, 159, 1, 1569 },
{ 0x1108, 160, 1, 1568 },
{ 0x1108, 165, 1, 1409 },
{ 0x1108, 166, 0, 1408 },
{ 0x908, 159, 1, 1571 },
{ 0x908, 160, 1, 1570 },
{ 0x908, 165, 1, 1411 },
{ 0x908, 166, 0, 1410 },
{ 0x508, 159, 1, 1573 },
{ 0x508, 160, 1, 1572 },
{ 0x508, 165, 1, 1413 },
{ 0x508, 166, 0, 1412 },
{ 0x308, 159, 1, 1577 },
{ 0x308, 160, 1, 1576 },
{ 0x108, 160, 1, 1574 },
{ 0x18, 161, 1, 1575 },
{ 0x308, 165, 1, 1417 },
{ 0x308, 166, 1, 1416 },
{ 0x108, 166, 1, 1414 },
{ 0x18, 167, 0, 1415 },
{ 0x88, 159, 1, 1609 },
{ 0x88, 160, 1, 1608 },
{ 0x88, 165, 1, 1489 },
{ 0x88, 166, 0, 1488 },
{ 0x48, 159, 1, 1611 },
{ 0x48, 160, 1, 1610 },
{ 0x48, 165, 1, 1491 },
{ 0x48, 166, 0, 1490 },
{ 0x28, 159, 1, 1613 },
{ 0x28, 160, 1, 1612 },
{ 0x28, 165, 1, 1493 },
{ 0x28, 166, 0, 1492 },
{ 0x18, 159, 1, 1617 },
{ 0x18, 160, 1, 1616 },
{ 0x8, 160, 1, 1614 },
{ 0x8, 161, 1, 1615 },
{ 0x18, 165, 1, 1497 },
{ 0x18, 166, 1, 1496 },
{ 0x8, 166, 1, 1494 },
{ 0x8, 167, 0, 1495 },
{ 0x884, 159, 1, 1579 },
{ 0x884, 160, 1, 1578 },
{ 0x442, 162, 1, 1469 },
{ 0x442, 163, 1, 1468 },
{ 0x884, 165, 1, 1439 },
{ 0x884, 166, 1, 1438 },
{ 0x442, 168, 1, 1419 },
{ 0x442, 169, 0, 1418 },
{ 0x484, 159, 1, 1581 },
{ 0x484, 160, 1, 1580 },
{ 0x242, 162, 1, 1471 },
{ 0x242, 163, 1, 1470 },
{ 0x484, 165, 1, 1441 },
{ 0x484, 166, 1, 1440 },
{ 0x242, 168, 1, 1421 },
{ 0x242, 169, 0, 1420 },
{ 0x284, 159, 1, 1583 },
{ 0x284, 160, 1, 1582 },
{ 0x142, 162, 1, 1473 },
{ 0x142, 163, 1, 1472 },
{ 0x284, 165, 1, 1443 },
{ 0x284, 166, 1, 1442 },
{ 0x142, 168, 1, 1423 },
{ 0x142, 169, 0, 1422 },
{ 0x184, 159, 1, 1587 },
{ 0x184, 160, 1, 1586 },
{ 0x84, 160, 1, 1584 },
{ 0xc, 161, 1, 1585 },
{ 0xc2, 162, 1, 1477 },
{ 0xc2, 163, 1, 1476 },
{ 0x42, 163, 1, 1474 },
{ 0x6, 164, 1, 1475 },
{ 0x184, 165, 1, 1447 },
{ 0x184, 166, 1, 1446 },
{ 0x84, 166, 1, 1444 },
{ 0xc, 167, 1, 1445 },
{ 0xc2, 168, 1, 1427 },
{ 0xc2, 169, 1, 1426 },
{ 0x42, 169, 1, 1424 },
{ 0x6, 170, 0, 1425 },
{ 0x44, 159, 1, 1619 },
{ 0x44, 160, 1, 1618 },
{ 0x22, 162, 1, 1549 },
{ 0x22, 163, 1, 1548 },
{ 0x44, 165, 1, 1519 },
{ 0x44, 166, 1, 1518 },
{ 0x22, 168, 1, 1499 },
{ 0x22, 169, 0, 1498 },
{ 0x24, 159, 1, 1621 },
{ 0x24, 160, 1, 1620 },
{ 0x12, 162, 1, 1551 },
{ 0x12, 163, 1, 1550 },
{ 0x24, 165, 1, 1521 },
{ 0x24, 166, 1, 1520 },
{ 0x12, 168, 1, 1501 },
{ 0x12, 169, 0, 1500 },
{ 0x14, 159, 1, 1623 },
{ 0x14, 160, 1, 1622 },
{ 0xa, 162, 1, 1553 },
{ 0xa, 163, 1, 1552 },
{ 0x14, 165, 1, 1523 },
{ 0x14, 166, 1, 1522 },
{ 0xa, 168, 1, 1503 },
{ 0xa, 169, 0, 1502 },
{ 0xc, 159, 1, 1627 },
{ 0xc, 160, 1, 1626 },
{ 0x4, 160, 1, 1624 },
{ 0x4, 161, 1, 1625 },
{ 0x6, 162, 1, 1557 },
{ 0x6, 163, 1, 1556 },
{ 0x2, 163, 1, 1554 },
{ 0x2, 164, 1, 1555 },
{ 0xc, 165, 1, 1527 },
{ 0xc, 166, 1, 1526 },
{ 0x4, 166, 1, 1524 },
{ 0x4, 167, 1, 1525 },
{ 0x6, 168, 1, 1507 },
{ 0x6, 169, 1, 1506 },
{ 0x2, 169, 1, 1504 },
{ 0x2, 170, 0, 1505 },
{ 0x442, 159, 1, 1589 },
{ 0x442, 160, 1, 1588 },
{ 0x221, 162, 1, 1479 },
{ 0x221, 163, 1, 1478 },
{ 0x442, 165, 1, 1449 },
{ 0x442, 166, 1, 1448 },
{ 0x221, 168, 1, 1429 },
{ 0x221, 169, 0, 1428 },
{ 0x242, 159, 1, 1591 },
{ 0x242, 160, 1, 1590 },
{ 0x121, 162, 1, 1481 },
{ 0x121, 163, 1, 1480 },
{ 0x242, 165, 1, 1451 },
{ 0x242, 166, 1, 1450 },
{ 0x121, 168, 1, 1431 },
{ 0x121, 169, 0, 1430 },
{ 0x142, 159, 1, 1593 },
{ 0x142, 160, 1, 1592 },
{ 0xa1, 162, 1, 1483 },
{ 0xa1, 163, 1, 1482 },
{ 0x142, 165, 1, 1453 },
{ 0x142, 166, 1, 1452 },
{ 0xa1, 168, 1, 1433 },
{ 0xa1, 169, 0, 1432 },
{ 0xc2, 159, 1, 1597 },
{ 0xc2, 160, 1, 1596 },
{ 0x42, 160, 1, 1594 },
{ 0x6, 161, 1, 1595 },
{ 0x61, 162, 1, 1487 },
{ 0x61, 163, 1, 1486 },
{ 0x21, 163, 1, 1484 },
{ 0x3, 164, 1, 1485 },
{ 0xc2, 165, 1, 1457 },
{ 0xc2, 166, 1, 1456 },
{ 0x42, 166, 1, 1454 },
{ 0x6, 167, 1, 1455 },
{ 0x61, 168, 1, 1437 },
{ 0x61, 169, 1, 1436 },
{ 0x21, 169, 1, 1434 },
{ 0x3, 170, 0, 1435 },
{ 0x22, 159, 1, 1629 },
{ 0x22, 160, 1, 1628 },
{ 0x11, 162, 1, 1559 },
{ 0x11, 163, 1, 1558 },
{ 0x22, 165, 1, 1529 },
{ 0x22, 166, 1, 1528 },
{ 0x11, 168, 1, 1509 },
{ 0x11, 169, 0, 1508 },
{ 0x12, 159, 1, 1631 },
{ 0x12, 160, 1, 1630 },
{ 0x9, 162, 1, 1561 },
{ 0x9, 163, 1, 1560 },
{ 0x12, 165, 1, 1531 },
{ 0x12, 166, 1, 1530 },
{ 0x9, 168, 1, 1511 },
{ 0x9, 169, 0, 1510 },
{ 0xa, 159, 1, 1633 },
{ 0xa, 160, 1, 1632 },
{ 0x5, 162, 1, 1563 },
{ 0x5, 163, 1, 1562 },
{ 0xa, 165, 1, 1533 },
{ 0xa, 166, 1, 1532 },
{ 0x5, 168, 1, 1513 },
{ 0x5, 169, 0, 1512 },
{ 0x6, 159, 1, 1637 },
{ 0x6, 160, 1, 1636 },
{ 0x2, 160, 1, 1634 },
{ 0x2, 161, 1, 1635 },
{ 0x3, 162, 1, 1567 },
{ 0x3, 163, 1, 1566 },
{ 0x1, 163, 1, 1564 },
{ 0x1, 164, 1, 1565 },
{ 0x6, 165, 1, 1537 },
{ 0x6, 166, 1, 1536 },
{ 0x2, 166, 1, 1534 },
{ 0x2, 167, 1, 1535 },
{ 0x3, 168, 1, 1517 },
{ 0x3, 169, 1, 1516 },
{ 0x1, 169, 1, 1514 },
{ 0x1, 170, 0, 1515 },
{ 0x221, 159, 1, 1599 },
{ 0x221, 160, 1, 1598 },
{ 0x221, 165, 1, 1459 },
{ 0x221, 166, 0, 1458 },
{ 0x121, 159, 1, 1601 },
{ 0x121, 160, 1, 1600 },
{ 0x121, 165, 1, 1461 },
{ 0x121, 166, 0, 1460 },
{ 0xa1, 159, 1, 1603 },
{ 0xa1, 160, 1, 1602 },
{ 0xa1, 165, 1, 1463 },
{ 0xa1, 166, 0, 1462 },
{ 0x61, 159, 1, 1607 },
{ 0x61, 160, 1, 1606 },
{ 0x21, 160, 1, 1604 },
{ 0x3, 161, 1, 1605 },
{ 0x61, 165, 1, 1467 },
{ 0x61, 166, 1, 1466 },
{ 0x21, 166, 1, 1464 },
{ 0x3, 167, 0, 1465 },
{ 0x11, 159, 1, 1639 },
{ 0x11, 160, 1, 1638 },
{ 0x11, 165, 1, 1539 },
{ 0x11, 166, 0, 1538 },
{ 0x9, 159, 1, 1641 },
{ 0x9, 160, 1, 1640 },
{ 0x9, 165, 1, 1541 },
{ 0x9, 166, 0, 1540 },
{ 0x5, 159, 1, 1643 },
{ 0x5, 160, 1, 1642 },
{ 0x5, 165, 1, 1543 },
{ 0x5, 166, 0, 1542 },
{ 0x3, 159, 1, 1647 },
{ 0x3, 160, 1, 1646 },
{ 0x1, 160, 1, 1644 },
{ 0x1, 161, 1, 1645 },
{ 0x3, 165, 1, 1547 },
{ 0x3, 166, 1, 1546 },
{ 0x1, 166, 1, 1544 },
{ 0x1, 167, 0, 1545 },
{ 0x442, 205, 0, 1648 },
{ 0x242, 205, 0, 1649 },
{ 0x142, 205, 0, 1650 },
{ 0xc2, 205, 1, 1652 },
{ 0x6, 206, 1, 1651 },
{ 0x1, 443, 0, 981 },
{ 0x22, 205, 0, 1658 },
{ 0x12, 205, 0, 1659 },
{ 0xa, 205, 0, 1660 },
{ 0x6, 205, 1, 1662 },
{ 0x2, 206, 1, 1661 },
{ 0x2, 367, 0, 1010 },
{ 0x221, 205, 0, 1653 },
{ 0x121, 205, 0, 1654 },
{ 0xa1, 205, 0, 1655 },
{ 0x61, 205, 1, 1657 },
{ 0x3, 206, 1, 1656 },
{ 0x1, 437, 0, 982 },
{ 0x11, 205, 0, 1663 },
{ 0x9, 205, 0, 1664 },
{ 0x5, 205, 0, 1665 },
{ 0x3, 205, 1, 1667 },
{ 0x1, 206, 1, 1666 },
{ 0x1, 367, 0, 1011 },
{ 0x4, 211, 0, 1668 },
{ 0x1, 211, 0, 1670 },
{ 0x1, 218, 0, 1671 },
{ 0x1, 217, 1, 1672 },
{ 0x2, 211, 0, 1669 },
{ 0x1, 196, 0, 1673 },
{ 0x880, 202, 0, 1674 },
{ 0x480, 202, 0, 1675 },
{ 0x280, 202, 0, 1676 },
{ 0x180, 202, 1, 1678 },
{ 0x80, 203, 0, 1677 },
{ 0x440, 202, 1, 1689 },
{ 0x88, 204, 0, 1679 },
{ 0x240, 202, 1, 1690 },
{ 0x48, 204, 0, 1680 },
{ 0x140, 202, 1, 1691 },
{ 0x28, 204, 0, 1681 },
{ 0xc0, 202, 1, 1693 },
{ 0x40, 203, 1, 1692 },
{ 0x18, 204, 1, 1683 },
{ 0x8, 204, 0, 1682 },
{ 0x220, 202, 1, 1694 },
{ 0x44, 204, 0, 1684 },
{ 0x120, 202, 1, 1695 },
{ 0x24, 204, 0, 1685 },
{ 0xa0, 202, 1, 1696 },
{ 0x14, 204, 0, 1686 },
{ 0x60, 202, 1, 1698 },
{ 0x20, 203, 1, 1697 },
{ 0xc, 204, 1, 1688 },
{ 0x4, 204, 0, 1687 },
{ 0x110, 202, 0, 1699 },
{ 0x90, 202, 0, 1700 },
{ 0x50, 202, 0, 1701 },
{ 0x30, 202, 1, 1703 },
{ 0x10, 203, 1, 1702 },
{ 0x1, 385, 0, 974 },
{ 0x88, 202, 0, 1704 },
{ 0x48, 202, 0, 1705 },
{ 0x28, 202, 0, 1706 },
{ 0x18, 202, 1, 1708 },
{ 0x8, 203, 1, 1707 },
{ 0xc, 368, 0, 1007 },
{ 0x44, 202, 1, 1719 },
{ 0x22, 204, 0, 1709 },
{ 0x24, 202, 1, 1720 },
{ 0x12, 204, 0, 1710 },
{ 0x14, 202, 1, 1721 },
{ 0xa, 204, 0, 1711 },
{ 0xc, 202, 1, 1723 },
{ 0x4, 203, 1, 1722 },
{ 0x6, 204, 1, 1713 },
{ 0x2, 204, 1, 1712 },
{ 0x6, 368, 0, 1008 },
{ 0x22, 202, 1, 1724 },
{ 0x11, 204, 0, 1714 },
{ 0x12, 202, 1, 1725 },
{ 0x9, 204, 0, 1715 },
{ 0xa, 202, 1, 1726 },
{ 0x5, 204, 0, 1716 },
{ 0x6, 202, 1, 1728 },
{ 0x2, 203, 1, 1727 },
{ 0x3, 204, 1, 1718 },
{ 0x1, 204, 1, 1717 },
{ 0x3, 368, 0, 1009 },
{ 0x11, 202, 0, 1729 },
{ 0x9, 202, 0, 1730 },
{ 0x5, 202, 0, 1731 },
{ 0x3, 202, 1, 1733 },
{ 0x1, 203, 0, 1732 },
{ 0x8, 198, 0, 1734 },
{ 0x4, 198, 0, 1735 },
{ 0x2, 198, 0, 1736 },
{ 0x1, 198, 1, 1738 },
{ 0x1, 199, 1, 1737 },
{ 0x1, 332, 0, 988 },
{ 0x8, 200, 0, 1739 },
{ 0x4, 200, 0, 1740 },
{ 0x2, 200, 0, 1741 },
{ 0x1, 200, 1, 1743 },
{ 0x1, 201, 1, 1742 },
{ 0x1, 331, 0, 989 },
{ 0x8, 209, 0, 1744 },
{ 0x4, 209, 0, 1745 },
{ 0x2, 209, 0, 1746 },
{ 0x1, 209, 1, 1748 },
{ 0x1, 210, 1, 1747 },
{ 0x1, 330, 0, 990 },
{ 0x8, 212, 0, 1749 },
{ 0x4, 212, 0, 1750 },
{ 0x2, 212, 0, 1751 },
{ 0x1, 212, 1, 1753 },
{ 0x1, 213, 1, 1752 },
{ 0x1, 329, 0, 991 },
{ 0x8, 224, 0, 1754 },
{ 0x4, 224, 0, 1755 },
{ 0x2, 224, 0, 1756 },
{ 0x1, 224, 1, 1758 },
{ 0x1, 225, 0, 1757 },
{ 0x8, 222, 0, 1759 },
{ 0x4, 222, 0, 1760 },
{ 0x2, 222, 0, 1761 },
{ 0x1, 222, 1, 1763 },
{ 0x1, 223, 0, 1762 },
{ 0x1, 240, 0, 1764 },
{ 0x1, 340, 0, 1765 },
{ 0x1, 33, 0, 1766 },
{ 0x8, 151, 0, 1767 },
{ 0x4, 151, 0, 1768 },
{ 0x2, 151, 0, 1769 },
{ 0x1, 151, 1, 1771 },
{ 0x1, 152, 0, 1770 },
{ 0x8, 157, 0, 1772 },
{ 0x4, 157, 0, 1773 },
{ 0x2, 157, 0, 1774 },
{ 0x1, 157, 1, 1776 },
{ 0x1, 158, 0, 1775 },
{ 0x8, 231, 0, 1777 },
{ 0x4, 231, 0, 1778 },
{ 0x2, 231, 0, 1779 },
{ 0x1, 231, 1, 1781 },
{ 0x1, 232, 0, 1780 },
{ 0x1, 173, 0, 1782 },
{ 0x442, 171, 0, 1783 },
{ 0x242, 171, 0, 1784 },
{ 0x142, 171, 0, 1785 },
{ 0xc2, 171, 1, 1787 },
{ 0x6, 172, 0, 1786 },
{ 0x22, 171, 0, 1793 },
{ 0x12, 171, 0, 1794 },
{ 0xa, 171, 0, 1795 },
{ 0x6, 171, 1, 1797 },
{ 0x2, 172, 1, 1796 },
{ 0x1, 135, 0, 1197 },
{ 0x221, 171, 0, 1788 },
{ 0x121, 171, 0, 1789 },
{ 0xa1, 171, 0, 1790 },
{ 0x61, 171, 1, 1792 },
{ 0x3, 172, 0, 1791 },
{ 0x11, 171, 0, 1798 },
{ 0x9, 171, 0, 1799 },
{ 0x5, 171, 0, 1800 },
{ 0x3, 171, 1, 1802 },
{ 0x1, 172, 1, 1801 },
{ 0x1, 134, 0, 1198 },
{ 0x1, 237, 0, 1803 },
{ 0x1, 195, 0, 1804 },
{ 0x1, 149, 0, 1805 },
{ 0x1, 148, 0, 1806 },
{ 0x4, 234, 0, 1807 },
{ 0x2, 234, 0, 1808 },
{ 0x1, 234, 0, 1809 },
{ 0x1, 197, 0, 1810 },
{ 0x2, 235, 0, 1811 },
{ 0x1, 235, 0, 1812 },
{ 0x4, 185, 0, 1813 },
{ 0x2, 185, 0, 1814 },
{ 0x1, 185, 0, 1815 },
{ 0x4, 182, 0, 1816 },
{ 0x1, 190, 0, 1819 },
{ 0x1, 189, 1, 1820 },
{ 0x2, 182, 0, 1817 },
{ 0x1, 142, 0, 1821 },
{ 0x1, 297, 1, 1822 },
{ 0x1, 182, 0, 1818 },
{ 0x8, 144, 0, 1823 },
{ 0x4, 144, 0, 1824 },
{ 0x2, 144, 0, 1825 },
{ 0x1, 144, 1, 1827 },
{ 0x1, 145, 0, 1826 },
{ 0x8, 146, 0, 1828 },
{ 0x4, 146, 0, 1829 },
{ 0x2, 146, 0, 1830 },
{ 0x1, 146, 1, 1832 },
{ 0x1, 147, 1, 1831 },
{ 0x1, 426, 0, 1199 },
{ 0x8, 180, 0, 1833 },
{ 0x4, 180, 0, 1834 },
{ 0x2, 180, 0, 1835 },
{ 0x1, 180, 1, 1837 },
{ 0x1, 181, 1, 1836 },
{ 0x1, 425, 0, 1200 },
{ 0x8, 183, 0, 1838 },
{ 0x4, 183, 0, 1839 },
{ 0x2, 183, 0, 1840 },
{ 0x1, 183, 1, 1842 },
{ 0x1, 184, 1, 1841 },
{ 0x1, 424, 0, 1201 },
{ 0x8, 228, 0, 1843 },
{ 0x4, 228, 0, 1844 },
{ 0x2, 228, 0, 1845 },
{ 0x1, 228, 1, 1847 },
{ 0x1, 229, 0, 1846 },
{ 0x8, 226, 0, 1848 },
{ 0x4, 226, 0, 1849 },
{ 0x2, 226, 0, 1850 },
{ 0x1, 226, 1, 1852 },
{ 0x1, 227, 0, 1851 },
{ 0x8, 44, 0, 1857 },
{ 0x18, 44, 0, 1853 },
{ 0x4, 44, 0, 1858 },
{ 0xc, 44, 0, 1854 },
{ 0x2, 44, 0, 1859 },
{ 0x6, 44, 0, 1855 },
{ 0x1, 44, 0, 1860 },
{ 0x3, 44, 0, 1856 },
{ 0x51, 30, 0, 1862 },
{ 0xd1, 30, 0, 1861 },
{ 0x31, 30, 1, 1872 },
{ 0x11, 31, 0, 1871 },
{ 0x71, 30, 1, 1870 },
{ 0x31, 31, 0, 1869 },
{ 0x29, 30, 0, 1864 },
{ 0x69, 30, 0, 1863 },
{ 0x19, 30, 1, 1876 },
{ 0x9, 31, 0, 1875 },
{ 0x39, 30, 1, 1874 },
{ 0x19, 31, 0, 1873 },
{ 0x15, 30, 0, 1866 },
{ 0x35, 30, 0, 1865 },
{ 0xd, 30, 1, 1880 },
{ 0x5, 31, 0, 1879 },
{ 0x1d, 30, 1, 1878 },
{ 0xd, 31, 0, 1877 },
{ 0xb, 30, 0, 1868 },
{ 0x1b, 30, 0, 1867 },
{ 0x7, 30, 1, 1884 },
{ 0x3, 31, 0, 1883 },
{ 0xf, 30, 1, 1882 },
{ 0x7, 31, 0, 1881 },
{ 0xa2, 28, 0, 1886 },
{ 0x1a2, 28, 0, 1885 },
{ 0x62, 28, 1, 1896 },
{ 0x22, 29, 0, 1895 },
{ 0xe2, 28, 1, 1894 },
{ 0x62, 29, 0, 1893 },
{ 0x52, 28, 0, 1888 },
{ 0xd2, 28, 0, 1887 },
{ 0x32, 28, 1, 1900 },
{ 0x12, 29, 0, 1899 },
{ 0x72, 28, 1, 1898 },
{ 0x32, 29, 0, 1897 },
{ 0x2a, 28, 0, 1890 },
{ 0x6a, 28, 0, 1889 },
{ 0x1a, 28, 1, 1904 },
{ 0xa, 29, 0, 1903 },
{ 0x3a, 28, 1, 1902 },
{ 0x1a, 29, 0, 1901 },
{ 0x16, 28, 0, 1892 },
{ 0x36, 28, 0, 1891 },
{ 0xe, 28, 1, 1908 },
{ 0x6, 29, 0, 1907 },
{ 0x1e, 28, 1, 1906 },
{ 0xe, 29, 0, 1905 },
{ 0x51, 28, 0, 1910 },
{ 0xd1, 28, 0, 1909 },
{ 0x31, 28, 1, 1920 },
{ 0x11, 29, 0, 1919 },
{ 0x71, 28, 1, 1918 },
{ 0x31, 29, 0, 1917 },
{ 0x29, 28, 0, 1912 },
{ 0x69, 28, 0, 1911 },
{ 0x19, 28, 1, 1924 },
{ 0x9, 29, 0, 1923 },
{ 0x39, 28, 1, 1922 },
{ 0x19, 29, 0, 1921 },
{ 0x15, 28, 0, 1914 },
{ 0x35, 28, 0, 1913 },
{ 0xd, 28, 1, 1928 },
{ 0x5, 29, 0, 1927 },
{ 0x1d, 28, 1, 1926 },
{ 0xd, 29, 0, 1925 },
{ 0xb, 28, 0, 1916 },
{ 0x1b, 28, 0, 1915 },
{ 0x7, 28, 1, 1932 },
{ 0x3, 29, 0, 1931 },
{ 0xf, 28, 1, 1930 },
{ 0x7, 29, 0, 1929 },
{ 0x51, 26, 0, 1934 },
{ 0xd1, 26, 0, 1933 },
{ 0x31, 26, 1, 1944 },
{ 0x11, 27, 0, 1943 },
{ 0x71, 26, 1, 1942 },
{ 0x31, 27, 0, 1941 },
{ 0x29, 26, 0, 1936 },
{ 0x69, 26, 0, 1935 },
{ 0x19, 26, 1, 1948 },
{ 0x9, 27, 0, 1947 },
{ 0x39, 26, 1, 1946 },
{ 0x19, 27, 0, 1945 },
{ 0x15, 26, 0, 1938 },
{ 0x35, 26, 0, 1937 },
{ 0xd, 26, 1, 1952 },
{ 0x5, 27, 0, 1951 },
{ 0x1d, 26, 1, 1950 },
{ 0xd, 27, 0, 1949 },
{ 0xb, 26, 0, 1940 },
{ 0x1b, 26, 0, 1939 },
{ 0x7, 26, 1, 1956 },
{ 0x3, 27, 0, 1955 },
{ 0xf, 26, 1, 1954 },
{ 0x7, 27, 0, 1953 },
{ 0xa2, 24, 0, 1958 },
{ 0x1a2, 24, 0, 1957 },
{ 0x62, 24, 1, 1968 },
{ 0x22, 25, 0, 1967 },
{ 0xe2, 24, 1, 1966 },
{ 0x62, 25, 0, 1965 },
{ 0x52, 24, 0, 1960 },
{ 0xd2, 24, 0, 1959 },
{ 0x32, 24, 1, 1972 },
{ 0x12, 25, 0, 1971 },
{ 0x72, 24, 1, 1970 },
{ 0x32, 25, 0, 1969 },
{ 0x2a, 24, 0, 1962 },
{ 0x6a, 24, 0, 1961 },
{ 0x1a, 24, 1, 1976 },
{ 0xa, 25, 0, 1975 },
{ 0x3a, 24, 1, 1974 },
{ 0x1a, 25, 0, 1973 },
{ 0x16, 24, 0, 1964 },
{ 0x36, 24, 0, 1963 },
{ 0xe, 24, 1, 1980 },
{ 0x6, 25, 0, 1979 },
{ 0x1e, 24, 1, 1978 },
{ 0xe, 25, 0, 1977 },
{ 0x51, 24, 0, 1982 },
{ 0xd1, 24, 0, 1981 },
{ 0x31, 24, 1, 1992 },
{ 0x11, 25, 0, 1991 },
{ 0x71, 24, 1, 1990 },
{ 0x31, 25, 0, 1989 },
{ 0x29, 24, 0, 1984 },
{ 0x69, 24, 0, 1983 },
{ 0x19, 24, 1, 1996 },
{ 0x9, 25, 0, 1995 },
{ 0x39, 24, 1, 1994 },
{ 0x19, 25, 0, 1993 },
{ 0x15, 24, 0, 1986 },
{ 0x35, 24, 0, 1985 },
{ 0xd, 24, 1, 2000 },
{ 0x5, 25, 0, 1999 },
{ 0x1d, 24, 1, 1998 },
{ 0xd, 25, 0, 1997 },
{ 0xb, 24, 0, 1988 },
{ 0x1b, 24, 0, 1987 },
{ 0x7, 24, 1, 2004 },
{ 0x3, 25, 0, 2003 },
{ 0xf, 24, 1, 2002 },
{ 0x7, 25, 0, 2001 },
{ 0x51, 22, 1, 2030 },
{ 0x50, 22, 0, 2006 },
{ 0xd1, 22, 1, 2029 },
{ 0xd0, 22, 0, 2005 },
{ 0x31, 22, 1, 2040 },
{ 0x30, 22, 1, 2016 },
{ 0x11, 23, 1, 2039 },
{ 0x10, 23, 0, 2015 },
{ 0x71, 22, 1, 2038 },
{ 0x70, 22, 1, 2014 },
{ 0x31, 23, 1, 2037 },
{ 0x30, 23, 0, 2013 },
{ 0x29, 22, 1, 2032 },
{ 0x28, 22, 0, 2008 },
{ 0x69, 22, 1, 2031 },
{ 0x68, 22, 0, 2007 },
{ 0x19, 22, 1, 2044 },
{ 0x18, 22, 1, 2020 },
{ 0x9, 23, 1, 2043 },
{ 0x8, 23, 0, 2019 },
{ 0x39, 22, 1, 2042 },
{ 0x38, 22, 1, 2018 },
{ 0x19, 23, 1, 2041 },
{ 0x18, 23, 0, 2017 },
{ 0x15, 22, 1, 2034 },
{ 0x14, 22, 0, 2010 },
{ 0x35, 22, 1, 2033 },
{ 0x34, 22, 0, 2009 },
{ 0xd, 22, 1, 2048 },
{ 0xc, 22, 1, 2024 },
{ 0x5, 23, 1, 2047 },
{ 0x4, 23, 0, 2023 },
{ 0x1d, 22, 1, 2046 },
{ 0x1c, 22, 1, 2022 },
{ 0xd, 23, 1, 2045 },
{ 0xc, 23, 0, 2021 },
{ 0xb, 22, 1, 2036 },
{ 0xa, 22, 0, 2012 },
{ 0x1b, 22, 1, 2035 },
{ 0x1a, 22, 0, 2011 },
{ 0x7, 22, 1, 2052 },
{ 0x6, 22, 1, 2028 },
{ 0x3, 23, 1, 2051 },
{ 0x2, 23, 0, 2027 },
{ 0xf, 22, 1, 2050 },
{ 0xe, 22, 1, 2026 },
{ 0x7, 23, 1, 2049 },
{ 0x6, 23, 0, 2025 },
{ 0x8, 21, 0, 2054 },
{ 0x18, 21, 0, 2053 },
{ 0x1, 21, 1, 2058 },
{ 0x2, 21, 0, 2057 },
{ 0x3, 21, 1, 2056 },
{ 0x4, 21, 0, 2055 },
{ 0x1, 239, 0, 2059 },
{ 0x1, 339, 0, 2060 },
{ 0x14, 43, 0, 2063 },
{ 0x34, 43, 0, 2061 },
{ 0xc, 43, 0, 2064 },
{ 0x1c, 43, 0, 2062 },
{ 0x2, 43, 0, 2067 },
{ 0x6, 43, 0, 2065 },
{ 0x1, 43, 0, 2068 },
{ 0x3, 43, 0, 2066 },
{ 0x51, 19, 0, 2070 },
{ 0xd1, 19, 0, 2069 },
{ 0x31, 19, 1, 2080 },
{ 0x11, 20, 0, 2079 },
{ 0x71, 19, 1, 2078 },
{ 0x31, 20, 0, 2077 },
{ 0x29, 19, 0, 2072 },
{ 0x69, 19, 0, 2071 },
{ 0x19, 19, 1, 2084 },
{ 0x9, 20, 0, 2083 },
{ 0x39, 19, 1, 2082 },
{ 0x19, 20, 0, 2081 },
{ 0x15, 19, 0, 2074 },
{ 0x35, 19, 0, 2073 },
{ 0xd, 19, 1, 2088 },
{ 0x5, 20, 0, 2087 },
{ 0x1d, 19, 1, 2086 },
{ 0xd, 20, 0, 2085 },
{ 0xb, 19, 0, 2076 },
{ 0x1b, 19, 0, 2075 },
{ 0x7, 19, 1, 2092 },
{ 0x3, 20, 0, 2091 },
{ 0xf, 19, 1, 2090 },
{ 0x7, 20, 0, 2089 },
{ 0x1, 32, 0, 2093 },
{ 0x2, 447, 0, 2094 },
{ 0x1, 447, 0, 2095 },
{ 0x1, 140, 0, 2096 },
{ 0x2, 45, 0, 2097 },
{ 0x1, 45, 0, 2098 },
{ 0x1, 387, 0, 2099 },
{ 0x2, 52, 0, 2100 },
{ 0x1, 52, 0, 2101 },
{ 0x1, 133, 0, 2102 },
{ 0x51, 17, 0, 2104 },
{ 0xd1, 17, 0, 2103 },
{ 0x31, 17, 1, 2114 },
{ 0x11, 18, 0, 2113 },
{ 0x71, 17, 1, 2112 },
{ 0x31, 18, 0, 2111 },
{ 0x29, 17, 0, 2106 },
{ 0x69, 17, 0, 2105 },
{ 0x19, 17, 1, 2118 },
{ 0x9, 18, 0, 2117 },
{ 0x39, 17, 1, 2116 },
{ 0x19, 18, 0, 2115 },
{ 0x15, 17, 0, 2108 },
{ 0x35, 17, 0, 2107 },
{ 0xd, 17, 1, 2122 },
{ 0x5, 18, 0, 2121 },
{ 0x1d, 17, 1, 2120 },
{ 0xd, 18, 0, 2119 },
{ 0xb, 17, 0, 2110 },
{ 0x1b, 17, 0, 2109 },
{ 0x7, 17, 1, 2126 },
{ 0x3, 18, 0, 2125 },
{ 0xf, 17, 1, 2124 },
{ 0x7, 18, 0, 2123 },
{ 0xa20, 15, 0, 2128 },
{ 0x1a20, 15, 0, 2127 },
{ 0x620, 15, 1, 2138 },
{ 0x220, 16, 0, 2137 },
{ 0xe20, 15, 1, 2136 },
{ 0x620, 16, 0, 2135 },
{ 0x520, 15, 0, 2130 },
{ 0xd20, 15, 0, 2129 },
{ 0x320, 15, 1, 2142 },
{ 0x120, 16, 0, 2141 },
{ 0x720, 15, 1, 2140 },
{ 0x320, 16, 0, 2139 },
{ 0x2a0, 15, 0, 2132 },
{ 0x6a0, 15, 0, 2131 },
{ 0x1a0, 15, 1, 2146 },
{ 0xa0, 16, 0, 2145 },
{ 0x3a0, 15, 1, 2144 },
{ 0x1a0, 16, 0, 2143 },
{ 0x160, 15, 0, 2134 },
{ 0x360, 15, 0, 2133 },
{ 0xe0, 15, 1, 2150 },
{ 0x60, 16, 0, 2149 },
{ 0x1e0, 15, 1, 2148 },
{ 0xe0, 16, 0, 2147 },
{ 0x51, 15, 1, 2176 },
{ 0x50, 15, 0, 2152 },
{ 0xd1, 15, 1, 2175 },
{ 0xd0, 15, 0, 2151 },
{ 0x31, 15, 1, 2186 },
{ 0x30, 15, 1, 2162 },
{ 0x11, 16, 1, 2185 },
{ 0x10, 16, 0, 2161 },
{ 0x71, 15, 1, 2184 },
{ 0x70, 15, 1, 2160 },
{ 0x31, 16, 1, 2183 },
{ 0x30, 16, 0, 2159 },
{ 0x29, 15, 1, 2178 },
{ 0x28, 15, 0, 2154 },
{ 0x69, 15, 1, 2177 },
{ 0x68, 15, 0, 2153 },
{ 0x19, 15, 1, 2190 },
{ 0x18, 15, 1, 2166 },
{ 0x9, 16, 1, 2189 },
{ 0x8, 16, 0, 2165 },
{ 0x39, 15, 1, 2188 },
{ 0x38, 15, 1, 2164 },
{ 0x19, 16, 1, 2187 },
{ 0x18, 16, 0, 2163 },
{ 0x15, 15, 1, 2180 },
{ 0x14, 15, 0, 2156 },
{ 0x35, 15, 1, 2179 },
{ 0x34, 15, 0, 2155 },
{ 0xd, 15, 1, 2194 },
{ 0xc, 15, 1, 2170 },
{ 0x5, 16, 1, 2193 },
{ 0x4, 16, 0, 2169 },
{ 0x1d, 15, 1, 2192 },
{ 0x1c, 15, 1, 2168 },
{ 0xd, 16, 1, 2191 },
{ 0xc, 16, 0, 2167 },
{ 0xb, 15, 1, 2182 },
{ 0xa, 15, 0, 2158 },
{ 0x1b, 15, 1, 2181 },
{ 0x1a, 15, 0, 2157 },
{ 0x7, 15, 1, 2198 },
{ 0x6, 15, 1, 2174 },
{ 0x3, 16, 1, 2197 },
{ 0x2, 16, 0, 2173 },
{ 0xf, 15, 1, 2196 },
{ 0xe, 15, 1, 2172 },
{ 0x7, 16, 1, 2195 },
{ 0x6, 16, 0, 2171 },
{ 0x8, 14, 0, 2200 },
{ 0x18, 14, 0, 2199 },
{ 0x1, 14, 1, 2204 },
{ 0x2, 14, 0, 2203 },
{ 0x3, 14, 1, 2202 },
{ 0x4, 14, 0, 2201 },
{ 0x1, 109, 1, 2356 },
{ 0x1, 110, 1, 2355 },
{ 0x1, 111, 1, 2354 },
{ 0x1, 112, 1, 2353 },
{ 0x1, 113, 1, 2352 },
{ 0x1, 114, 1, 2351 },
{ 0x1, 115, 1, 2350 },
{ 0x1, 116, 1, 2349 },
{ 0x39, 41, 1, 22 },
{ 0x19, 42, 0, 21 },
{ 0x3, 109, 1, 2348 },
{ 0x3, 110, 1, 2347 },
{ 0x3, 111, 1, 2346 },
{ 0x3, 112, 1, 2345 },
{ 0x3, 113, 1, 2344 },
{ 0x3, 114, 1, 2343 },
{ 0x3, 115, 1, 2342 },
{ 0x3, 116, 1, 2341 },
{ 0x69, 41, 0, 11 },
{ 0x14, 100, 1, 2336 },
{ 0x22, 101, 1, 2333 },
{ 0x44, 101, 1, 2335 },
{ 0xa, 108, 1, 2334 },
{ 0xd1, 41, 0, 9 },
{ 0x34, 100, 1, 2208 },
{ 0xc4, 101, 1, 2207 },
{ 0x1c, 107, 1, 2205 },
{ 0xe, 122, 0, 2206 },
{ 0xc, 100, 1, 2496 },
{ 0xa, 101, 1, 2493 },
{ 0x14, 101, 1, 2495 },
{ 0x6, 108, 0, 2494 },
{ 0x2, 100, 1, 2220 },
{ 0x2, 101, 1, 2219 },
{ 0x2, 106, 1, 2218 },
{ 0x2, 107, 0, 2217 },
{ 0x12, 100, 1, 2216 },
{ 0x42, 101, 1, 2215 },
{ 0x6, 106, 1, 2214 },
{ 0x6, 107, 0, 2213 },
{ 0xa, 100, 1, 2340 },
{ 0x12, 101, 1, 2339 },
{ 0x24, 101, 1, 2337 },
{ 0x5, 108, 1, 2338 },
{ 0x71, 41, 1, 18 },
{ 0x31, 42, 0, 17 },
{ 0x1a, 100, 1, 2212 },
{ 0x32, 101, 1, 2211 },
{ 0x1a, 107, 1, 2209 },
{ 0x7, 122, 0, 2210 },
{ 0x6, 100, 1, 2500 },
{ 0x6, 101, 1, 2499 },
{ 0xc, 101, 1, 2497 },
{ 0x3, 108, 0, 2498 },
{ 0x1, 100, 1, 2516 },
{ 0x1, 101, 1, 2515 },
{ 0x1, 102, 1, 2514 },
{ 0x1, 103, 1, 2513 },
{ 0x1, 104, 1, 2512 },
{ 0x1, 105, 1, 2511 },
{ 0x1, 106, 1, 2510 },
{ 0x1, 107, 0, 2509 },
{ 0x3, 100, 1, 2508 },
{ 0x3, 101, 1, 2507 },
{ 0x3, 102, 1, 2506 },
{ 0x3, 103, 1, 2505 },
{ 0x3, 104, 1, 2504 },
{ 0x3, 105, 1, 2503 },
{ 0x3, 106, 1, 2502 },
{ 0x3, 107, 0, 2501 },
{ 0x8, 67, 1, 2380 },
{ 0x8, 68, 1, 2379 },
{ 0x2, 73, 1, 2374 },
{ 0x2, 74, 1, 2373 },
{ 0x1, 76, 1, 2378 },
{ 0x1, 77, 1, 2377 },
{ 0x1, 78, 1, 2376 },
{ 0x1, 79, 1, 2375 },
{ 0xf, 41, 1, 30 },
{ 0x7, 42, 0, 29 },
{ 0x18, 67, 1, 2372 },
{ 0x18, 68, 1, 2371 },
{ 0x6, 73, 1, 2366 },
{ 0x6, 74, 1, 2365 },
{ 0x3, 76, 1, 2370 },
{ 0x3, 77, 1, 2369 },
{ 0x3, 78, 1, 2368 },
{ 0x3, 79, 1, 2367 },
{ 0x1b, 41, 0, 15 },
{ 0x14, 67, 1, 2360 },
{ 0x22, 68, 1, 2357 },
{ 0x44, 68, 1, 2359 },
{ 0xa, 75, 1, 2358 },
{ 0x35, 41, 0, 13 },
{ 0x34, 67, 1, 2224 },
{ 0xc4, 68, 1, 2223 },
{ 0x38, 74, 1, 2221 },
{ 0xe, 85, 0, 2222 },
{ 0xc, 67, 1, 2520 },
{ 0xa, 68, 1, 2517 },
{ 0x14, 68, 1, 2519 },
{ 0x6, 75, 0, 2518 },
{ 0x2, 67, 1, 2236 },
{ 0x2, 68, 1, 2235 },
{ 0x4, 73, 1, 2234 },
{ 0x4, 74, 0, 2233 },
{ 0x12, 67, 1, 2232 },
{ 0x42, 68, 1, 2231 },
{ 0xc, 73, 1, 2230 },
{ 0xc, 74, 0, 2229 },
{ 0xa, 67, 1, 2364 },
{ 0x12, 68, 1, 2363 },
{ 0x24, 68, 1, 2361 },
{ 0x5, 75, 1, 2362 },
{ 0x1d, 41, 1, 26 },
{ 0xd, 42, 0, 25 },
{ 0x1a, 67, 1, 2228 },
{ 0x32, 68, 1, 2227 },
{ 0x34, 74, 1, 2225 },
{ 0x7, 85, 0, 2226 },
{ 0x6, 67, 1, 2524 },
{ 0x6, 68, 1, 2523 },
{ 0xc, 68, 1, 2521 },
{ 0x3, 75, 0, 2522 },
{ 0x1, 67, 1, 2540 },
{ 0x1, 68, 1, 2539 },
{ 0x1, 69, 1, 2538 },
{ 0x1, 70, 1, 2537 },
{ 0x1, 71, 1, 2536 },
{ 0x1, 72, 1, 2535 },
{ 0x1, 73, 1, 2534 },
{ 0x1, 74, 0, 2533 },
{ 0x3, 67, 1, 2532 },
{ 0x3, 68, 1, 2531 },
{ 0x3, 69, 1, 2530 },
{ 0x3, 70, 1, 2529 },
{ 0x3, 71, 1, 2528 },
{ 0x3, 72, 1, 2527 },
{ 0x3, 73, 1, 2526 },
{ 0x3, 74, 0, 2525 },
{ 0x28, 95, 1, 2388 },
{ 0x44, 96, 1, 2383 },
{ 0x88, 96, 1, 2387 },
{ 0x44, 97, 1, 2382 },
{ 0x88, 97, 1, 2386 },
{ 0x44, 98, 1, 2381 },
{ 0x88, 98, 1, 2385 },
{ 0x28, 99, 0, 2384 },
{ 0x68, 95, 1, 2244 },
{ 0x188, 96, 1, 2243 },
{ 0x188, 97, 1, 2242 },
{ 0x188, 98, 1, 2241 },
{ 0x38, 118, 1, 2240 },
{ 0x38, 119, 1, 2239 },
{ 0x38, 120, 1, 2238 },
{ 0x38, 121, 0, 2237 },
{ 0x18, 95, 1, 2548 },
{ 0x14, 96, 1, 2543 },
{ 0x28, 96, 1, 2547 },
{ 0x14, 97, 1, 2542 },
{ 0x28, 97, 1, 2546 },
{ 0x14, 98, 1, 2541 },
{ 0x28, 98, 1, 2545 },
{ 0x18, 99, 0, 2544 },
{ 0x14, 95, 1, 2396 },
{ 0x24, 96, 1, 2395 },
{ 0x48, 96, 1, 2391 },
{ 0x24, 97, 1, 2394 },
{ 0x48, 97, 1, 2390 },
{ 0x24, 98, 1, 2393 },
{ 0x48, 98, 1, 2389 },
{ 0x14, 99, 0, 2392 },
{ 0x34, 95, 1, 2252 },
{ 0x64, 96, 1, 2251 },
{ 0x64, 97, 1, 2250 },
{ 0x64, 98, 1, 2249 },
{ 0x1c, 118, 1, 2248 },
{ 0x1c, 119, 1, 2247 },
{ 0x1c, 120, 1, 2246 },
{ 0x1c, 121, 0, 2245 },
{ 0xc, 95, 1, 2556 },
{ 0xc, 96, 1, 2555 },
{ 0x18, 96, 1, 2551 },
{ 0xc, 97, 1, 2554 },
{ 0x18, 97, 1, 2550 },
{ 0xc, 98, 1, 2553 },
{ 0x18, 98, 1, 2549 },
{ 0xc, 99, 0, 2552 },
{ 0xa, 95, 1, 2404 },
{ 0x11, 96, 1, 2399 },
{ 0x22, 96, 1, 2403 },
{ 0x11, 97, 1, 2398 },
{ 0x22, 97, 1, 2402 },
{ 0x11, 98, 1, 2397 },
{ 0x22, 98, 1, 2401 },
{ 0xa, 99, 0, 2400 },
{ 0x1a, 95, 1, 2260 },
{ 0x62, 96, 1, 2259 },
{ 0x62, 97, 1, 2258 },
{ 0x62, 98, 1, 2257 },
{ 0xe, 118, 1, 2256 },
{ 0xe, 119, 1, 2255 },
{ 0xe, 120, 1, 2254 },
{ 0xe, 121, 0, 2253 },
{ 0x6, 95, 1, 2564 },
{ 0x5, 96, 1, 2559 },
{ 0xa, 96, 1, 2563 },
{ 0x5, 97, 1, 2558 },
{ 0xa, 97, 1, 2562 },
{ 0x5, 98, 1, 2557 },
{ 0xa, 98, 1, 2561 },
{ 0x6, 99, 0, 2560 },
{ 0x5, 95, 1, 2412 },
{ 0x9, 96, 1, 2411 },
{ 0x12, 96, 1, 2407 },
{ 0x9, 97, 1, 2410 },
{ 0x12, 97, 1, 2406 },
{ 0x9, 98, 1, 2409 },
{ 0x12, 98, 1, 2405 },
{ 0x5, 99, 0, 2408 },
{ 0xd, 95, 1, 2268 },
{ 0x19, 96, 1, 2267 },
{ 0x19, 97, 1, 2266 },
{ 0x19, 98, 1, 2265 },
{ 0x7, 118, 1, 2264 },
{ 0x7, 119, 1, 2263 },
{ 0x7, 120, 1, 2262 },
{ 0x7, 121, 0, 2261 },
{ 0x3, 95, 1, 2572 },
{ 0x3, 96, 1, 2571 },
{ 0x6, 96, 1, 2567 },
{ 0x3, 97, 1, 2570 },
{ 0x6, 97, 1, 2566 },
{ 0x3, 98, 1, 2569 },
{ 0x6, 98, 1, 2565 },
{ 0x3, 99, 0, 2568 },
{ 0x28, 62, 1, 2420 },
{ 0x44, 63, 1, 2415 },
{ 0x88, 63, 1, 2419 },
{ 0x44, 64, 1, 2414 },
{ 0x88, 64, 1, 2418 },
{ 0x44, 65, 1, 2413 },
{ 0x88, 65, 1, 2417 },
{ 0x28, 66, 0, 2416 },
{ 0x68, 62, 1, 2276 },
{ 0x188, 63, 1, 2275 },
{ 0x188, 64, 1, 2274 },
{ 0x188, 65, 1, 2273 },
{ 0x38, 81, 1, 2272 },
{ 0x38, 82, 1, 2271 },
{ 0x38, 83, 1, 2270 },
{ 0x38, 84, 0, 2269 },
{ 0x18, 62, 1, 2580 },
{ 0x14, 63, 1, 2575 },
{ 0x28, 63, 1, 2579 },
{ 0x14, 64, 1, 2574 },
{ 0x28, 64, 1, 2578 },
{ 0x14, 65, 1, 2573 },
{ 0x28, 65, 1, 2577 },
{ 0x18, 66, 0, 2576 },
{ 0x14, 62, 1, 2428 },
{ 0x24, 63, 1, 2427 },
{ 0x48, 63, 1, 2423 },
{ 0x24, 64, 1, 2426 },
{ 0x48, 64, 1, 2422 },
{ 0x24, 65, 1, 2425 },
{ 0x48, 65, 1, 2421 },
{ 0x14, 66, 0, 2424 },
{ 0x34, 62, 1, 2284 },
{ 0x64, 63, 1, 2283 },
{ 0x64, 64, 1, 2282 },
{ 0x64, 65, 1, 2281 },
{ 0x1c, 81, 1, 2280 },
{ 0x1c, 82, 1, 2279 },
{ 0x1c, 83, 1, 2278 },
{ 0x1c, 84, 0, 2277 },
{ 0xc, 62, 1, 2588 },
{ 0xc, 63, 1, 2587 },
{ 0x18, 63, 1, 2583 },
{ 0xc, 64, 1, 2586 },
{ 0x18, 64, 1, 2582 },
{ 0xc, 65, 1, 2585 },
{ 0x18, 65, 1, 2581 },
{ 0xc, 66, 0, 2584 },
{ 0xa, 62, 1, 2436 },
{ 0x11, 63, 1, 2431 },
{ 0x22, 63, 1, 2435 },
{ 0x11, 64, 1, 2430 },
{ 0x22, 64, 1, 2434 },
{ 0x11, 65, 1, 2429 },
{ 0x22, 65, 1, 2433 },
{ 0xa, 66, 0, 2432 },
{ 0x1a, 62, 1, 2292 },
{ 0x62, 63, 1, 2291 },
{ 0x62, 64, 1, 2290 },
{ 0x62, 65, 1, 2289 },
{ 0xe, 81, 1, 2288 },
{ 0xe, 82, 1, 2287 },
{ 0xe, 83, 1, 2286 },
{ 0xe, 84, 0, 2285 },
{ 0x6, 62, 1, 2596 },
{ 0x5, 63, 1, 2591 },
{ 0xa, 63, 1, 2595 },
{ 0x5, 64, 1, 2590 },
{ 0xa, 64, 1, 2594 },
{ 0x5, 65, 1, 2589 },
{ 0xa, 65, 1, 2593 },
{ 0x6, 66, 0, 2592 },
{ 0x5, 62, 1, 2444 },
{ 0x9, 63, 1, 2443 },
{ 0x12, 63, 1, 2439 },
{ 0x9, 64, 1, 2442 },
{ 0x12, 64, 1, 2438 },
{ 0x9, 65, 1, 2441 },
{ 0x12, 65, 1, 2437 },
{ 0x5, 66, 0, 2440 },
{ 0xd, 62, 1, 2300 },
{ 0x19, 63, 1, 2299 },
{ 0x19, 64, 1, 2298 },
{ 0x19, 65, 1, 2297 },
{ 0x7, 81, 1, 2296 },
{ 0x7, 82, 1, 2295 },
{ 0x7, 83, 1, 2294 },
{ 0x7, 84, 0, 2293 },
{ 0x3, 62, 1, 2604 },
{ 0x3, 63, 1, 2603 },
{ 0x6, 63, 1, 2599 },
{ 0x3, 64, 1, 2602 },
{ 0x6, 64, 1, 2598 },
{ 0x3, 65, 1, 2601 },
{ 0x6, 65, 1, 2597 },
{ 0x3, 66, 0, 2600 },
{ 0x8, 86, 1, 2468 },
{ 0x8, 87, 1, 2467 },
{ 0x2, 88, 1, 2466 },
{ 0x2, 89, 1, 2465 },
{ 0x2, 90, 1, 2464 },
{ 0x2, 91, 1, 2463 },
{ 0x2, 92, 1, 2462 },
{ 0x2, 93, 0, 2461 },
{ 0x18, 86, 1, 2460 },
{ 0x18, 87, 1, 2459 },
{ 0x6, 88, 1, 2458 },
{ 0x6, 89, 1, 2457 },
{ 0x6, 90, 1, 2456 },
{ 0x6, 91, 1, 2455 },
{ 0x6, 92, 1, 2454 },
{ 0x6, 93, 0, 2453 },
{ 0x14, 86, 1, 2448 },
{ 0x22, 87, 1, 2445 },
{ 0x44, 87, 1, 2447 },
{ 0xa, 94, 0, 2446 },
{ 0x34, 86, 1, 2304 },
{ 0xc4, 87, 1, 2303 },
{ 0x38, 93, 1, 2301 },
{ 0xe, 117, 0, 2302 },
{ 0xc, 86, 1, 2608 },
{ 0xa, 87, 1, 2605 },
{ 0x14, 87, 1, 2607 },
{ 0x6, 94, 0, 2606 },
{ 0x2, 86, 1, 2316 },
{ 0x2, 87, 1, 2315 },
{ 0x4, 92, 1, 2314 },
{ 0x4, 93, 0, 2313 },
{ 0x12, 86, 1, 2312 },
{ 0x42, 87, 1, 2311 },
{ 0xc, 92, 1, 2310 },
{ 0xc, 93, 0, 2309 },
{ 0xa, 86, 1, 2452 },
{ 0x12, 87, 1, 2451 },
{ 0x24, 87, 1, 2449 },
{ 0x5, 94, 0, 2450 },
{ 0x1a, 86, 1, 2308 },
{ 0x32, 87, 1, 2307 },
{ 0x34, 93, 1, 2305 },
{ 0x7, 117, 0, 2306 },
{ 0x6, 86, 1, 2612 },
{ 0x6, 87, 1, 2611 },
{ 0xc, 87, 1, 2609 },
{ 0x3, 94, 0, 2610 },
{ 0x1, 86, 1, 2628 },
{ 0x1, 87, 1, 2627 },
{ 0x1, 88, 1, 2626 },
{ 0x1, 89, 1, 2625 },
{ 0x1, 90, 1, 2624 },
{ 0x1, 91, 1, 2623 },
{ 0x1, 92, 1, 2622 },
{ 0x1, 93, 0, 2621 },
{ 0x3, 86, 1, 2620 },
{ 0x3, 87, 1, 2619 },
{ 0x3, 88, 1, 2618 },
{ 0x3, 89, 1, 2617 },
{ 0x3, 90, 1, 2616 },
{ 0x3, 91, 1, 2615 },
{ 0x3, 92, 1, 2614 },
{ 0x3, 93, 0, 2613 },
{ 0x8, 53, 1, 2492 },
{ 0x8, 54, 1, 2491 },
{ 0x2, 55, 1, 2490 },
{ 0x2, 56, 1, 2489 },
{ 0x2, 57, 1, 2488 },
{ 0x2, 58, 1, 2487 },
{ 0x2, 59, 1, 2486 },
{ 0x2, 60, 0, 2485 },
{ 0x18, 53, 1, 2484 },
{ 0x18, 54, 1, 2483 },
{ 0x6, 55, 1, 2482 },
{ 0x6, 56, 1, 2481 },
{ 0x6, 57, 1, 2480 },
{ 0x6, 58, 1, 2479 },
{ 0x6, 59, 1, 2478 },
{ 0x6, 60, 0, 2477 },
{ 0x14, 53, 1, 2472 },
{ 0x22, 54, 1, 2469 },
{ 0x44, 54, 1, 2471 },
{ 0xa, 61, 0, 2470 },
{ 0x34, 53, 1, 2320 },
{ 0xc4, 54, 1, 2319 },
{ 0x38, 60, 1, 2317 },
{ 0xe, 80, 0, 2318 },
{ 0xc, 53, 1, 2632 },
{ 0xa, 54, 1, 2629 },
{ 0x14, 54, 1, 2631 },
{ 0x6, 61, 0, 2630 },
{ 0x2, 53, 1, 2332 },
{ 0x2, 54, 1, 2331 },
{ 0x4, 59, 1, 2330 },
{ 0x4, 60, 0, 2329 },
{ 0x12, 53, 1, 2328 },
{ 0x42, 54, 1, 2327 },
{ 0xc, 59, 1, 2326 },
{ 0xc, 60, 0, 2325 },
{ 0xa, 53, 1, 2476 },
{ 0x12, 54, 1, 2475 },
{ 0x24, 54, 1, 2473 },
{ 0x5, 61, 0, 2474 },
{ 0x1a, 53, 1, 2324 },
{ 0x32, 54, 1, 2323 },
{ 0x34, 60, 1, 2321 },
{ 0x7, 80, 0, 2322 },
{ 0x6, 53, 1, 2636 },
{ 0x6, 54, 1, 2635 },
{ 0xc, 54, 1, 2633 },
{ 0x3, 61, 0, 2634 },
{ 0x1, 53, 1, 2652 },
{ 0x1, 54, 1, 2651 },
{ 0x1, 55, 1, 2650 },
{ 0x1, 56, 1, 2649 },
{ 0x1, 57, 1, 2648 },
{ 0x1, 58, 1, 2647 },
{ 0x1, 59, 1, 2646 },
{ 0x1, 60, 0, 2645 },
{ 0x3, 53, 1, 2644 },
{ 0x3, 54, 1, 2643 },
{ 0x3, 55, 1, 2642 },
{ 0x3, 56, 1, 2641 },
{ 0x3, 57, 1, 2640 },
{ 0x3, 58, 1, 2639 },
{ 0x3, 59, 1, 2638 },
{ 0x3, 60, 0, 2637 },
{ 0x1, 4, 0, 2653 },
{ 0x1, 296, 0, 2654 },
{ 0x1, 379, 0, 2655 },
{ 0x1, 374, 0, 2656 },
{ 0x2, 358, 0, 2657 },
{ 0x1, 358, 0, 2660 },
{ 0x2, 357, 0, 2658 },
{ 0x1, 357, 0, 2661 },
{ 0x2, 356, 0, 2659 },
{ 0x1, 356, 0, 2662 },
{ 0x1, 355, 0, 2663 },
{ 0x1, 354, 0, 2664 },
{ 0x2, 353, 0, 2665 },
{ 0x1, 353, 0, 2667 },
{ 0x2, 352, 0, 2666 },
{ 0x1, 352, 0, 2668 },
{ 0x1, 382, 0, 2675 },
{ 0x8, 381, 0, 2669 },
{ 0x4, 381, 0, 2671 },
{ 0x2, 381, 0, 2673 },
{ 0x1, 381, 0, 2676 },
{ 0x8, 380, 0, 2670 },
{ 0x4, 380, 0, 2672 },
{ 0x2, 380, 0, 2674 },
{ 0x1, 380, 0, 2677 },
{ 0x1, 351, 0, 2684 },
{ 0x8, 350, 0, 2678 },
{ 0x4, 350, 0, 2680 },
{ 0x2, 350, 0, 2682 },
{ 0x1, 350, 0, 2685 },
{ 0x8, 349, 0, 2679 },
{ 0x4, 349, 0, 2681 },
{ 0x2, 349, 1, 2683 },
{ 0x4, 143, 0, 1377 },
{ 0x1, 349, 0, 2686 },
{ 0x1, 6, 0, 2687 },
{ 0x1, 7, 0, 2688 },
{ 0x1, 295, 0, 2689 },
{ 0x1, 456, 0, 2690 },
{ 0x1, 346, 0, 2691 },
{ 0x1, 13, 0, 2692 },
{ 0x1, 11, 0, 2693 },
{ 0x1, 422, 0, 2694 },
{ 0x1, 394, 0, 2695 },
{ 0x1, 393, 0, 2696 },
{ 0x1, 455, 0, 2697 },
{ 0x1, 345, 0, 2698 },
{ 0x1, 12, 0, 2699 },
{ 0x1, 10, 0, 2700 },
{ 0x1, 5, 0, 2701 },
{ 0x1, 421, 0, 2702 },
{ 0x1, 420, 0, 2703 },
{ 0x1, 1, 0, 2704 },
{ 0x1, 0, 0, 2705 },
};


/* ia64-opc.c -- Functions to access the compacted opcode table
   Copyright 1999, 2000, 2001, 2003, 2005 Free Software Foundation, Inc.
   Written by Bob Manson of Cygnus Solutions, <manson@cygnus.com>

   This file is part of GDB, GAS, and the GNU binutils.

   GDB, GAS, and the GNU binutils are free software; you can redistribute
   them and/or modify them under the terms of the GNU General Public
   License as published by the Free Software Foundation; either version
   2, or (at your option) any later version.

   GDB, GAS, and the GNU binutils are distributed in the hope that they
   will be useful, but WITHOUT ANY WARRANTY; without even the implied
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this file; see the file COPYING.  If not, see
   <http://www.gnu.org/licenses/>. */

static const struct ia64_templ_desc ia64_templ_desc[16] =
  {
    { 0, { IA64_UNIT_M, IA64_UNIT_I, IA64_UNIT_I }, "MII" },	/* 0 */
    { 2, { IA64_UNIT_M, IA64_UNIT_I, IA64_UNIT_I }, "MII" },
    { 0, { IA64_UNIT_M, IA64_UNIT_L, IA64_UNIT_X }, "MLX" },
    { 0, { 0, },				    "-3-" },
    { 0, { IA64_UNIT_M, IA64_UNIT_M, IA64_UNIT_I }, "MMI" },	/* 4 */
    { 1, { IA64_UNIT_M, IA64_UNIT_M, IA64_UNIT_I }, "MMI" },
    { 0, { IA64_UNIT_M, IA64_UNIT_F, IA64_UNIT_I }, "MFI" },
    { 0, { IA64_UNIT_M, IA64_UNIT_M, IA64_UNIT_F }, "MMF" },
    { 0, { IA64_UNIT_M, IA64_UNIT_I, IA64_UNIT_B }, "MIB" },	/* 8 */
    { 0, { IA64_UNIT_M, IA64_UNIT_B, IA64_UNIT_B }, "MBB" },
    { 0, { 0, },				    "-a-" },
    { 0, { IA64_UNIT_B, IA64_UNIT_B, IA64_UNIT_B }, "BBB" },
    { 0, { IA64_UNIT_M, IA64_UNIT_M, IA64_UNIT_B }, "MMB" },	/* c */
    { 0, { 0, },				    "-d-" },
    { 0, { IA64_UNIT_M, IA64_UNIT_F, IA64_UNIT_B }, "MFB" },
    { 0, { 0, },				    "-f-" },
  };

/* Apply the completer referred to by COMPLETER_INDEX to OPCODE, and
   return the result. */

static ia64_insn
apply_completer (ia64_insn opcode, int completer_index)
{
  ia64_insn mask = completer_table[completer_index].mask;
  ia64_insn bits = completer_table[completer_index].bits;
  int shiftamt = (completer_table[completer_index].offset & 63);

  mask = mask << shiftamt;
  bits = bits << shiftamt;
  opcode = (opcode & ~mask) | bits;
  return opcode;
}

/* Extract BITS number of bits starting from OP_POINTER + BITOFFSET in
   the dis_table array, and return its value.  (BITOFFSET is numbered
   starting from MSB to LSB, so a BITOFFSET of 0 indicates the MSB of the
   first byte in OP_POINTER.) */

static int
extract_op_bits (int op_pointer, int bitoffset, int bits)
{
  int res = 0;

  op_pointer += (bitoffset / 8);

  if (bitoffset % 8)
    {
      unsigned int op = dis_table[op_pointer++];
      int numb = 8 - (bitoffset % 8);
      int mask = (1 << numb) - 1;
      int bata = (bits < numb) ? bits : numb;
      int delta = numb - bata;

      res = (res << bata) | ((op & mask) >> delta);
      bitoffset += bata;
      bits -= bata;
    }
  while (bits >= 8)
    {
      res = (res << 8) | (dis_table[op_pointer++] & 255);
      bits -= 8;
    }
  if (bits > 0)
    {
      unsigned int op = (dis_table[op_pointer++] & 255);
      res = (res << bits) | (op >> (8 - bits));
    }
  return res;
}

/* Examine the state machine entry at OP_POINTER in the dis_table
   array, and extract its values into OPVAL and OP.  The length of the
   state entry in bits is returned. */

static int
extract_op (int op_pointer, int *opval, unsigned int *op)
{
  int oplen = 5;

  *op = dis_table[op_pointer];

  if ((*op) & 0x40)
    {
      opval[0] = extract_op_bits (op_pointer, oplen, 5);
      oplen += 5;
    }
  switch ((*op) & 0x30)
    {
    case 0x10:
      {
	opval[1] = extract_op_bits (op_pointer, oplen, 8);
	oplen += 8;
	opval[1] += op_pointer;
	break;
      }
    case 0x20:
      {
	opval[1] = extract_op_bits (op_pointer, oplen, 16);
	if (! (opval[1] & 32768))
	  {
	    opval[1] += op_pointer;
	  }
	oplen += 16;
	break;
      }
    case 0x30:
      {
	oplen--;
	opval[2] = extract_op_bits (op_pointer, oplen, 12);
	oplen += 12;
	opval[2] |= 32768;
	break;
      }
    }
  if (((*op) & 0x08) && (((*op) & 0x30) != 0x30))
    {
      opval[2] = extract_op_bits (op_pointer, oplen, 16);
      oplen += 16;
      if (! (opval[2] & 32768))
	{
	  opval[2] += op_pointer;
	}
    }
  return oplen;
}

/* Returns a non-zero value if the opcode in the main_table list at
   PLACE matches OPCODE and is of type TYPE. */

static int
opcode_verify (ia64_insn opcode, int place, enum ia64_insn_type type)
{
  if (main_table[place].opcode_type != type)
    {
      return 0;
    }
  if (main_table[place].flags
      & (IA64_OPCODE_F2_EQ_F3 | IA64_OPCODE_LEN_EQ_64MCNT))
    {
      const struct ia64_operand *o1, *o2;
      ia64_insn f2, f3;

      if (main_table[place].flags & IA64_OPCODE_F2_EQ_F3)
	{
	  o1 = elf64_ia64_operands + IA64_OPND_F2;
	  o2 = elf64_ia64_operands + IA64_OPND_F3;
	  (*o1->extract) (o1, opcode, &f2);
	  (*o2->extract) (o2, opcode, &f3);
	  if (f2 != f3)
	    return 0;
	}
      else
	{
	  ia64_insn len, count;

	  /* length must equal 64-count: */
	  o1 = elf64_ia64_operands + IA64_OPND_LEN6;
	  o2 = elf64_ia64_operands + main_table[place].operands[2];
	  (*o1->extract) (o1, opcode, &len);
	  (*o2->extract) (o2, opcode, &count);
	  if (len != 64 - count)
	    return 0;
	}
    }
  return 1;
}

/* Find an instruction entry in the ia64_dis_names array that matches
   opcode OPCODE and is of type TYPE.  Returns either a positive index
   into the array, or a negative value if an entry for OPCODE could
   not be found.  Checks all matches and returns the one with the highest
   priority. */

static int
locate_opcode_ent (ia64_insn opcode, enum ia64_insn_type type)
{
  int currtest[41];
  int bitpos[41];
  int op_ptr[41];
  int currstatenum = 0;
  short found_disent = -1;
  short found_priority = -1;

  currtest[currstatenum] = 0;
  op_ptr[currstatenum] = 0;
  bitpos[currstatenum] = 40;

  while (1)
    {
      int op_pointer = op_ptr[currstatenum];
      unsigned int op;
      int currbitnum = bitpos[currstatenum];
      int oplen;
      int opval[3] = {0};
      int next_op;
      int currbit;

      oplen = extract_op (op_pointer, opval, &op);

      bitpos[currstatenum] = currbitnum;

      /* Skip opval[0] bits in the instruction. */
      if (op & 0x40)
	{
	  currbitnum -= opval[0];
	}

      /* The value of the current bit being tested. */
      currbit = opcode & (((ia64_insn) 1) << currbitnum) ? 1 : 0;
      next_op = -1;

      /* We always perform the tests specified in the current state in
	 a particular order, falling through to the next test if the
	 previous one failed. */
      switch (currtest[currstatenum])
	{
	case 0:
	  currtest[currstatenum]++;
	  if (currbit == 0 && (op & 0x80))
	    {
	      /* Check for a zero bit.  If this test solely checks for
		 a zero bit, we can check for up to 8 consecutive zero
		 bits (the number to check is specified by the lower 3
		 bits in the state code.)

		 If the state instruction matches, we go to the very
		 next state instruction; otherwise, try the next test. */

	      if ((op & 0xf8) == 0x80)
		{
		  int count = op & 0x7;
		  int x;

		  for (x = 0; x <= count; x++)
		    {
		      int i =
			opcode & (((ia64_insn) 1) << (currbitnum - x)) ? 1 : 0;
		      if (i)
			{
			  break;
			}
		    }
		  if (x > count)
		    {
		      next_op = op_pointer + ((oplen + 7) / 8);
		      currbitnum -= count;
		      break;
		    }
		}
	      else if (! currbit)
		{
		  next_op = op_pointer + ((oplen + 7) / 8);
		  break;
		}
	    }
	  /* FALLTHROUGH */
	case 1:
	  /* If the bit in the instruction is one, go to the state
	     instruction specified by opval[1]. */
	  currtest[currstatenum]++;
	  if (currbit && (op & 0x30) != 0 && ((op & 0x30) != 0x30))
	    {
	      next_op = opval[1];
	      break;
	    }
	  /* FALLTHROUGH */
	case 2:
	  /* Don't care.  Skip the current bit and go to the state
	     instruction specified by opval[2].

	     An encoding of 0x30 is special; this means that a 12-bit
	     offset into the ia64_dis_names[] array is specified.  */
	  currtest[currstatenum]++;
	  if ((op & 0x08) || ((op & 0x30) == 0x30))
	    {
	      next_op = opval[2];
	      break;
	    }
	}

      /* If bit 15 is set in the address of the next state, an offset
	 in the ia64_dis_names array was specified instead.  We then
	 check to see if an entry in the list of opcodes matches the
	 opcode we were given; if so, we have succeeded.  */

      if ((next_op >= 0) && (next_op & 32768))
	{
	  short disent = next_op & 32767;
          short priority = -1;

	  if (next_op > 65535)
	    {
	      abort ();
	    }

	  /* Run through the list of opcodes to check, trying to find
	     one that matches.  */
	  while (disent >= 0)
	    {
	      int place = ia64_dis_names[disent].insn_index;

              priority = ia64_dis_names[disent].priority;

	      if (opcode_verify (opcode, place, type)
                  && priority > found_priority)
		{
		  break;
		}
	      if (ia64_dis_names[disent].next_flag)
		{
		  disent++;
		}
	      else
		{
		  disent = -1;
		}
	    }

	  if (disent >= 0)
	    {
              found_disent = disent;
              found_priority = priority;
	    }
          /* Try the next test in this state, regardless of whether a match
             was found. */
          next_op = -2;
	}

      /* next_op == -1 is "back up to the previous state".
	 next_op == -2 is "stay in this state and try the next test".
	 Otherwise, transition to the state indicated by next_op. */

      if (next_op == -1)
	{
	  currstatenum--;
	  if (currstatenum < 0)
	    {
              return found_disent;
	    }
	}
      else if (next_op >= 0)
	{
	  currstatenum++;
	  bitpos[currstatenum] = currbitnum - 1;
	  op_ptr[currstatenum] = next_op;
	  currtest[currstatenum] = 0;
	}
    }
}

/* Construct an ia64_opcode entry based on OPCODE, NAME and PLACE. */

static struct ia64_opcode *
make_ia64_opcode (ia64_insn opcode, const char *name, int place, int depind)
{
  struct ia64_opcode *res =
    (struct ia64_opcode *) malloc (sizeof (struct ia64_opcode));
  res->name = strdup (name);
  res->type = main_table[place].opcode_type;
  res->num_outputs = main_table[place].num_outputs;
  res->opcode = opcode;
  res->mask = main_table[place].mask;
  res->operands[0] = main_table[place].operands[0];
  res->operands[1] = main_table[place].operands[1];
  res->operands[2] = main_table[place].operands[2];
  res->operands[3] = main_table[place].operands[3];
  res->operands[4] = main_table[place].operands[4];
  res->flags = main_table[place].flags;
  res->ent_index = place;
  res->dependencies = &op_dependencies[depind];
  return res;
}

/* Determine the ia64_opcode entry for the opcode specified by INSN
   and TYPE.  If a valid entry is not found, return NULL. */
static struct ia64_opcode *
ia64_dis_opcode (ia64_insn insn, enum ia64_insn_type type)
{
  int disent = locate_opcode_ent (insn, type);

  if (disent < 0)
    {
      return NULL;
    }
  else
    {
      unsigned int cb = ia64_dis_names[disent].completer_index;
      static char name[128];
      int place = ia64_dis_names[disent].insn_index;
      int ci = main_table[place].completers;
      ia64_insn tinsn = main_table[place].opcode;

      strcpy (name, ia64_strings [main_table[place].name_index]);

      while (cb)
	{
	  if (cb & 1)
	    {
	      int cname = completer_table[ci].name_index;

	      tinsn = apply_completer (tinsn, ci);

	      if (ia64_strings[cname][0] != '\0')
		{
		  strcat (name, ".");
		  strcat (name, ia64_strings[cname]);
		}
	      if (cb != 1)
		{
		  ci = completer_table[ci].subentries;
		}
	    }
	  else
	    {
	      ci = completer_table[ci].alternative;
	    }
	  if (ci < 0)
	    {
	      abort ();
	    }
	  cb = cb >> 1;
	}
      if (tinsn != (insn & main_table[place].mask))
	{
	  abort ();
	}
      return make_ia64_opcode (insn, name, place,
                               completer_table[ci].dependencies);
    }
}

/* Free any resources used by ENT. */
static void
ia64_free_opcode (struct ia64_opcode *ent)
{
  free ((void *)ent->name);
  free (ent);
}

/* Disassemble ia64 instruction.  */

/* Return the instruction type for OPCODE found in unit UNIT. */

static enum ia64_insn_type
unit_to_type (ia64_insn opcode, enum ia64_unit unit)
{
  enum ia64_insn_type type;
  int op;

  op = IA64_OP (opcode);

  if (op >= 8 && (unit == IA64_UNIT_I || unit == IA64_UNIT_M))
    {
      type = IA64_TYPE_A;
    }
  else
    {
      switch (unit)
	{
	case IA64_UNIT_I:
	  type = IA64_TYPE_I; break;
	case IA64_UNIT_M:
	  type = IA64_TYPE_M; break;
	case IA64_UNIT_B:
	  type = IA64_TYPE_B; break;
	case IA64_UNIT_F:
	  type = IA64_TYPE_F; break;
        case IA64_UNIT_L:
	case IA64_UNIT_X:
	  type = IA64_TYPE_X; break;
	default:
	  type = -1;
	}
    }
  return type;
}

int
print_insn_ia64 (bfd_vma memaddr, struct disassemble_info *info)
{
  ia64_insn t0, t1, slot[3], template, s_bit, insn;
  int slotnum, j, status, need_comma, retval, slot_multiplier;
  const struct ia64_operand *odesc;
  const struct ia64_opcode *idesc;
  const char *err, *str, *tname;
  uint64_t value;
  bfd_byte bundle[16];
  enum ia64_unit unit;
  char regname[16];

  if (info->bytes_per_line == 0)
    info->bytes_per_line = 6;
  info->display_endian = info->endian;

  slot_multiplier = info->bytes_per_line;
  retval = slot_multiplier;

  slotnum = (((long) memaddr) & 0xf) / slot_multiplier;
  if (slotnum > 2)
    return -1;

  memaddr -= (memaddr & 0xf);
  status = (*info->read_memory_func) (memaddr, bundle, sizeof (bundle), info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, memaddr, info);
      return -1;
    }
  /* bundles are always in little-endian byte order */
  t0 = bfd_getl64 (bundle);
  t1 = bfd_getl64 (bundle + 8);
  s_bit = t0 & 1;
  template = (t0 >> 1) & 0xf;
  slot[0] = (t0 >>  5) & 0x1ffffffffffLL;
  slot[1] = ((t0 >> 46) & 0x3ffff) | ((t1 & 0x7fffff) << 18);
  slot[2] = (t1 >> 23) & 0x1ffffffffffLL;

  tname = ia64_templ_desc[template].name;
  if (slotnum == 0)
    (*info->fprintf_func) (info->stream, "[%s] ", tname);
  else
    (*info->fprintf_func) (info->stream, "      ");

  unit = ia64_templ_desc[template].exec_unit[slotnum];

  if (template == 2 && slotnum == 1)
    {
      /* skip L slot in MLI template: */
      slotnum = 2;
      retval += slot_multiplier;
    }

  insn = slot[slotnum];

  if (unit == IA64_UNIT_NIL)
    goto decoding_failed;

  idesc = ia64_dis_opcode (insn, unit_to_type (insn, unit));
  if (idesc == NULL)
    goto decoding_failed;

  /* print predicate, if any: */

  if ((idesc->flags & IA64_OPCODE_NO_PRED)
      || (insn & 0x3f) == 0)
    (*info->fprintf_func) (info->stream, "      ");
  else
    (*info->fprintf_func) (info->stream, "(p%02d) ", (int)(insn & 0x3f));

  /* now the actual instruction: */

  (*info->fprintf_func) (info->stream, "%s", idesc->name);
  if (idesc->operands[0])
    (*info->fprintf_func) (info->stream, " ");

  need_comma = 0;
  for (j = 0; j < NELEMS (idesc->operands) && idesc->operands[j]; ++j)
    {
      odesc = elf64_ia64_operands + idesc->operands[j];

      if (need_comma)
	(*info->fprintf_func) (info->stream, ",");

      if (odesc - elf64_ia64_operands == IA64_OPND_IMMU64)
	{
	  /* special case of 64 bit immediate load: */
	  value = ((insn >> 13) & 0x7f) | (((insn >> 27) & 0x1ff) << 7)
	    | (((insn >> 22) & 0x1f) << 16) | (((insn >> 21) & 0x1) << 21)
	    | (slot[1] << 22) | (((insn >> 36) & 0x1) << 63);
	}
      else if (odesc - elf64_ia64_operands == IA64_OPND_IMMU62)
        {
          /* 62-bit immediate for nop.x/break.x */
          value = ((slot[1] & 0x1ffffffffffLL) << 21)
            | (((insn >> 36) & 0x1) << 20)
            | ((insn >> 6) & 0xfffff);
        }
      else if (odesc - elf64_ia64_operands == IA64_OPND_TGT64)
	{
	  /* 60-bit immediate for long branches. */
	  value = (((insn >> 13) & 0xfffff)
		   | (((insn >> 36) & 1) << 59)
		   | (((slot[1] >> 2) & 0x7fffffffffLL) << 20)) << 4;
	}
      else
	{
	  err = (*odesc->extract) (odesc, insn, &value);
	  if (err)
	    {
	      (*info->fprintf_func) (info->stream, "%s", err);
	      goto done;
	    }
	}

	switch (odesc->class)
	  {
	  case IA64_OPND_CLASS_CST:
	    (*info->fprintf_func) (info->stream, "%s", odesc->str);
	    break;

	  case IA64_OPND_CLASS_REG:
	    if (odesc->str[0] == 'a' && odesc->str[1] == 'r')
	      {
		switch (value)
		  {
		  case 0: case 1: case 2: case 3:
		  case 4: case 5: case 6: case 7:
		    sprintf (regname, "ar.k%u", (unsigned int) value);
		    break;
		  case 16:	strcpy (regname, "ar.rsc"); break;
		  case 17:	strcpy (regname, "ar.bsp"); break;
		  case 18:	strcpy (regname, "ar.bspstore"); break;
		  case 19:	strcpy (regname, "ar.rnat"); break;
		  case 32:	strcpy (regname, "ar.ccv"); break;
		  case 36:	strcpy (regname, "ar.unat"); break;
		  case 40:	strcpy (regname, "ar.fpsr"); break;
		  case 44:	strcpy (regname, "ar.itc"); break;
		  case 64:	strcpy (regname, "ar.pfs"); break;
		  case 65:	strcpy (regname, "ar.lc"); break;
		  case 66:	strcpy (regname, "ar.ec"); break;
		  default:
		    sprintf (regname, "ar%u", (unsigned int) value);
		    break;
		  }
		(*info->fprintf_func) (info->stream, "%s", regname);
	      }
	    else
	      (*info->fprintf_func) (info->stream, "%s%d", odesc->str, (int)value);
	    break;

	  case IA64_OPND_CLASS_IND:
	    (*info->fprintf_func) (info->stream, "%s[r%d]", odesc->str, (int)value);
	    break;

	  case IA64_OPND_CLASS_ABS:
	    str = 0;
	    if (odesc - elf64_ia64_operands == IA64_OPND_MBTYPE4)
	      switch (value)
		{
		case 0x0: str = "@brcst"; break;
		case 0x8: str = "@mix"; break;
		case 0x9: str = "@shuf"; break;
		case 0xa: str = "@alt"; break;
		case 0xb: str = "@rev"; break;
		}

	    if (str)
	      (*info->fprintf_func) (info->stream, "%s", str);
	    else if (odesc->flags & IA64_OPND_FLAG_DECIMAL_SIGNED)
              (*info->fprintf_func) (info->stream, "%" PRId64,
                                     (int64_t) value);
	    else if (odesc->flags & IA64_OPND_FLAG_DECIMAL_UNSIGNED)
              (*info->fprintf_func) (info->stream, "%" PRIu64,
                                     (uint64_t) value);
	    else
              (*info->fprintf_func) (info->stream, "0x%" PRIx64,
                                     (uint64_t) value);
	    break;

	  case IA64_OPND_CLASS_REL:
	    (*info->print_address_func) (memaddr + value, info);
	    break;
	  }

      need_comma = 1;
      if (j + 1 == idesc->num_outputs)
	{
	  (*info->fprintf_func) (info->stream, "=");
	  need_comma = 0;
	}
    }
  if (slotnum + 1 == ia64_templ_desc[template].group_boundary
      || ((slotnum == 2) && s_bit))
    (*info->fprintf_func) (info->stream, ";;");

 done:
  ia64_free_opcode ((struct ia64_opcode *)idesc);
 failed:
  if (slotnum == 2)
    retval += 16 - 3*slot_multiplier;
  return retval;

 decoding_failed:
  (*info->fprintf_func) (info->stream, "      data8 %#011llx", (long long) insn);
  goto failed;
}
