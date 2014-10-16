/*
 * Bochs/QEMU ACPI SSDT ASL definition
 *
 * Copyright (c) 2006 Fabrice Bellard
 * Copyright (c) 2009 SGI, Jes Sorensen <jes@sgi.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */
DefinitionBlock (
    "acpi-ssdt.aml",    // Output Filename
    "SSDT",             // Signature
    0x01,               // DSDT Compliance Revision
    "BXPC",             // OEMID
    "BXSSDT",           // TABLE ID
    0x1                 // OEM Revision
    )
{
   Scope (\_PR)
   {
	/* pointer to first element of MADT APIC structures */
	OperationRegion(ATPR, SystemMemory, 0x0514, 4)
	Field (ATPR, DwordAcc, NoLock, Preserve)
	{
		ATP, 32
	}

#define madt_addr(nr)  Add (ATP, Multiply(nr, 8))

#define gen_processor(nr, name) 				            \
	Processor (C##name, nr, 0x0000b010, 0x06) {                       \
	    OperationRegion (MATR, SystemMemory, madt_addr(nr), 8)          \
	    Field (MATR, ByteAcc, NoLock, Preserve)                         \
	    {                                                               \
	        MAT, 64                                                     \
	    }                                                               \
	    Field (MATR, ByteAcc, NoLock, Preserve)                         \
	    {                                                               \
	        Offset(4),                                                  \
	        FLG, 1                                                      \
	    }                                                               \
            Method(_MAT, 0) {                                               \
		Return(MAT)                                                 \
            }                                                               \
            Method (_STA) {                                                 \
                If (FLG) { Return(0xF) } Else { Return(0x9) }               \
            }                                                               \
        }                                                                   \


	gen_processor(0, 0)
	gen_processor(1, 1)
	gen_processor(2, 2)
	gen_processor(3, 3)
	gen_processor(4, 4)
	gen_processor(5, 5)
	gen_processor(6, 6)
	gen_processor(7, 7)
	gen_processor(8, 8)
	gen_processor(9, 9)
	gen_processor(10, A)
	gen_processor(11, B)
	gen_processor(12, C)
	gen_processor(13, D)
	gen_processor(14, E)

	Method (NTFY, 2) {
#define gen_ntfy(nr)                                        \
	If (LEqual(Arg0, 0x##nr)) {                         \
		If (LNotEqual(Arg1, \_PR.C##nr.FLG)) {      \
			Store (Arg1, \_PR.C##nr.FLG)        \
			If (LEqual(Arg1, 1)) {              \
				Notify(C##nr, 1)            \
			} Else {                            \
				Notify(C##nr, 3)            \
			}                                   \
		}                                           \
	}
		gen_ntfy(0)
		gen_ntfy(1)
		gen_ntfy(2)
		gen_ntfy(3)
		gen_ntfy(4)
		gen_ntfy(5)
		gen_ntfy(6)
		gen_ntfy(7)
		gen_ntfy(8)
		gen_ntfy(9)
		gen_ntfy(A)
		gen_ntfy(B)
		gen_ntfy(C)
		gen_ntfy(D)
		gen_ntfy(E)
		Return(One)
	}

	OperationRegion(PRST, SystemIO, 0xaf00, 32)
	Field (PRST, ByteAcc, NoLock, Preserve)
	{
		PRS, 256
	}

	Method(PRSC, 0) {
		Store(PRS, Local3)
		Store(Zero, Local0)
		While(LLess(Local0, 32)) {
			Store(Zero, Local1)
			Store(DerefOf(Index(Local3, Local0)), Local2)
			While(LLess(Local1, 8)) {
				NTFY(Add(Multiply(Local0, 8), Local1),
						And(Local2, 1))
				ShiftRight(Local2, 1, Local2)
				Increment(Local1)
			}
			Increment(Local0)
		}
		Return(One)
	}
    }

    /*
     * Add the missing _L02 method for CPU notification
     */
    Scope (\_GPE)
    {
        Method(_L02) {
	    Return(\_PR.PRSC())
        }
    }
}
