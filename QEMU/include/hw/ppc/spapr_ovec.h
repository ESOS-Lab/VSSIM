/*
 * QEMU SPAPR Option/Architecture Vector Definitions
 *
 * Each architecture option is organized/documented by the following
 * in LoPAPR 1.1, Table 244:
 *
 *   <vector number>: the bit-vector in which the option is located
 *   <vector byte>: the byte offset of the vector entry
 *   <vector bit>: the bit offset within the vector entry
 *
 * where each vector entry can be one or more bytes.
 *
 * Firmware expects a somewhat literal encoding of this bit-vector
 * structure, where each entry is stored in little-endian so that the
 * byte ordering reflects that of the documentation, but where each bit
 * offset is from "left-to-right" in the traditional representation of
 * a byte value where the MSB is the left-most bit. Thus, each
 * individual byte encodes the option bits in reverse order of the
 * documented bit.
 *
 * These definitions/helpers attempt to abstract away this internal
 * representation so that we can define/set/test for individual option
 * bits using only the documented values. This is done mainly by relying
 * on a bitmap to approximate the documented "bit-vector" structure and
 * handling conversations to-from the internal representation under the
 * covers.
 *
 * Copyright IBM Corp. 2016
 *
 * Authors:
 *  Michael Roth      <mdroth@linux.vnet.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#ifndef _SPAPR_OVEC_H
#define _SPAPR_OVEC_H

#include "cpu.h"
#include "migration/vmstate.h"

typedef struct sPAPROptionVector sPAPROptionVector;

#define OV_BIT(byte, bit) ((byte - 1) * BITS_PER_BYTE + bit)

/* option vector 5 */
#define OV5_DRCONF_MEMORY       OV_BIT(2, 2)
#define OV5_FORM1_AFFINITY      OV_BIT(5, 0)
#define OV5_HP_EVT              OV_BIT(6, 5)

/* interfaces */
sPAPROptionVector *spapr_ovec_new(void);
sPAPROptionVector *spapr_ovec_clone(sPAPROptionVector *ov_orig);
void spapr_ovec_intersect(sPAPROptionVector *ov,
                          sPAPROptionVector *ov1,
                          sPAPROptionVector *ov2);
bool spapr_ovec_diff(sPAPROptionVector *ov,
                     sPAPROptionVector *ov_old,
                     sPAPROptionVector *ov_new);
void spapr_ovec_cleanup(sPAPROptionVector *ov);
void spapr_ovec_set(sPAPROptionVector *ov, long bitnr);
void spapr_ovec_clear(sPAPROptionVector *ov, long bitnr);
bool spapr_ovec_test(sPAPROptionVector *ov, long bitnr);
sPAPROptionVector *spapr_ovec_parse_vector(target_ulong table_addr, int vector);
int spapr_ovec_populate_dt(void *fdt, int fdt_offset,
                           sPAPROptionVector *ov, const char *name);

/* migration */
extern const VMStateDescription vmstate_spapr_ovec;

#endif /* !defined (_SPAPR_OVEC_H) */
