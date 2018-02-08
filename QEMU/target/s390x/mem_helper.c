/*
 *  S/390 memory access helper routines
 *
 *  Copyright (c) 2009 Ulrich Hecht
 *  Copyright (c) 2009 Alexander Graf
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/exec-all.h"
#include "exec/cpu_ldst.h"

#if !defined(CONFIG_USER_ONLY)
#include "hw/s390x/storage-keys.h"
#endif

/*****************************************************************************/
/* Softmmu support */
#if !defined(CONFIG_USER_ONLY)

/* try to fill the TLB and return an exception if error. If retaddr is
   NULL, it means that the function was called in C code (i.e. not
   from generated code or from helper.c) */
/* XXX: fix it to restore all registers */
void tlb_fill(CPUState *cs, target_ulong addr, MMUAccessType access_type,
              int mmu_idx, uintptr_t retaddr)
{
    int ret;

    ret = s390_cpu_handle_mmu_fault(cs, addr, access_type, mmu_idx);
    if (unlikely(ret != 0)) {
        if (likely(retaddr)) {
            /* now we have a real cpu fault */
            cpu_restore_state(cs, retaddr);
        }
        cpu_loop_exit(cs);
    }
}

#endif

/* #define DEBUG_HELPER */
#ifdef DEBUG_HELPER
#define HELPER_LOG(x...) qemu_log(x)
#else
#define HELPER_LOG(x...)
#endif

/* Reduce the length so that addr + len doesn't cross a page boundary.  */
static inline uint64_t adj_len_to_page(uint64_t len, uint64_t addr)
{
#ifndef CONFIG_USER_ONLY
    if ((addr & ~TARGET_PAGE_MASK) + len - 1 >= TARGET_PAGE_SIZE) {
        return -addr & ~TARGET_PAGE_MASK;
    }
#endif
    return len;
}

static void fast_memset(CPUS390XState *env, uint64_t dest, uint8_t byte,
                        uint32_t l)
{
    int mmu_idx = cpu_mmu_index(env, false);

    while (l > 0) {
        void *p = tlb_vaddr_to_host(env, dest, MMU_DATA_STORE, mmu_idx);
        if (p) {
            /* Access to the whole page in write mode granted.  */
            int l_adj = adj_len_to_page(l, dest);
            memset(p, byte, l_adj);
            dest += l_adj;
            l -= l_adj;
        } else {
            /* We failed to get access to the whole page. The next write
               access will likely fill the QEMU TLB for the next iteration.  */
            cpu_stb_data(env, dest, byte);
            dest++;
            l--;
        }
    }
}

static void fast_memmove(CPUS390XState *env, uint64_t dest, uint64_t src,
                         uint32_t l)
{
    int mmu_idx = cpu_mmu_index(env, false);

    while (l > 0) {
        void *src_p = tlb_vaddr_to_host(env, src, MMU_DATA_LOAD, mmu_idx);
        void *dest_p = tlb_vaddr_to_host(env, dest, MMU_DATA_STORE, mmu_idx);
        if (src_p && dest_p) {
            /* Access to both whole pages granted.  */
            int l_adj = adj_len_to_page(l, src);
            l_adj = adj_len_to_page(l_adj, dest);
            memmove(dest_p, src_p, l_adj);
            src += l_adj;
            dest += l_adj;
            l -= l_adj;
        } else {
            /* We failed to get access to one or both whole pages. The next
               read or write access will likely fill the QEMU TLB for the
               next iteration.  */
            cpu_stb_data(env, dest, cpu_ldub_data(env, src));
            src++;
            dest++;
            l--;
        }
    }
}

/* and on array */
uint32_t HELPER(nc)(CPUS390XState *env, uint32_t l, uint64_t dest,
                    uint64_t src)
{
    int i;
    unsigned char x;
    uint32_t cc = 0;

    HELPER_LOG("%s l %d dest %" PRIx64 " src %" PRIx64 "\n",
               __func__, l, dest, src);
    for (i = 0; i <= l; i++) {
        x = cpu_ldub_data(env, dest + i) & cpu_ldub_data(env, src + i);
        if (x) {
            cc = 1;
        }
        cpu_stb_data(env, dest + i, x);
    }
    return cc;
}

/* xor on array */
uint32_t HELPER(xc)(CPUS390XState *env, uint32_t l, uint64_t dest,
                    uint64_t src)
{
    int i;
    unsigned char x;
    uint32_t cc = 0;

    HELPER_LOG("%s l %d dest %" PRIx64 " src %" PRIx64 "\n",
               __func__, l, dest, src);

    /* xor with itself is the same as memset(0) */
    if (src == dest) {
        fast_memset(env, dest, 0, l + 1);
        return 0;
    }

    for (i = 0; i <= l; i++) {
        x = cpu_ldub_data(env, dest + i) ^ cpu_ldub_data(env, src + i);
        if (x) {
            cc = 1;
        }
        cpu_stb_data(env, dest + i, x);
    }
    return cc;
}

/* or on array */
uint32_t HELPER(oc)(CPUS390XState *env, uint32_t l, uint64_t dest,
                    uint64_t src)
{
    int i;
    unsigned char x;
    uint32_t cc = 0;

    HELPER_LOG("%s l %d dest %" PRIx64 " src %" PRIx64 "\n",
               __func__, l, dest, src);
    for (i = 0; i <= l; i++) {
        x = cpu_ldub_data(env, dest + i) | cpu_ldub_data(env, src + i);
        if (x) {
            cc = 1;
        }
        cpu_stb_data(env, dest + i, x);
    }
    return cc;
}

/* memmove */
void HELPER(mvc)(CPUS390XState *env, uint32_t l, uint64_t dest, uint64_t src)
{
    int i = 0;

    HELPER_LOG("%s l %d dest %" PRIx64 " src %" PRIx64 "\n",
               __func__, l, dest, src);

    /* mvc with source pointing to the byte after the destination is the
       same as memset with the first source byte */
    if (dest == (src + 1)) {
        fast_memset(env, dest, cpu_ldub_data(env, src), l + 1);
        return;
    }

    /* mvc and memmove do not behave the same when areas overlap! */
    if ((dest < src) || (src + l < dest)) {
        fast_memmove(env, dest, src, l + 1);
        return;
    }

    /* slow version with byte accesses which always work */
    for (i = 0; i <= l; i++) {
        cpu_stb_data(env, dest + i, cpu_ldub_data(env, src + i));
    }
}

/* compare unsigned byte arrays */
uint32_t HELPER(clc)(CPUS390XState *env, uint32_t l, uint64_t s1, uint64_t s2)
{
    int i;
    unsigned char x, y;
    uint32_t cc;

    HELPER_LOG("%s l %d s1 %" PRIx64 " s2 %" PRIx64 "\n",
               __func__, l, s1, s2);
    for (i = 0; i <= l; i++) {
        x = cpu_ldub_data(env, s1 + i);
        y = cpu_ldub_data(env, s2 + i);
        HELPER_LOG("%02x (%c)/%02x (%c) ", x, x, y, y);
        if (x < y) {
            cc = 1;
            goto done;
        } else if (x > y) {
            cc = 2;
            goto done;
        }
    }
    cc = 0;
 done:
    HELPER_LOG("\n");
    return cc;
}

/* compare logical under mask */
uint32_t HELPER(clm)(CPUS390XState *env, uint32_t r1, uint32_t mask,
                     uint64_t addr)
{
    uint8_t r, d;
    uint32_t cc;

    HELPER_LOG("%s: r1 0x%x mask 0x%x addr 0x%" PRIx64 "\n", __func__, r1,
               mask, addr);
    cc = 0;
    while (mask) {
        if (mask & 8) {
            d = cpu_ldub_data(env, addr);
            r = (r1 & 0xff000000UL) >> 24;
            HELPER_LOG("mask 0x%x %02x/%02x (0x%" PRIx64 ") ", mask, r, d,
                       addr);
            if (r < d) {
                cc = 1;
                break;
            } else if (r > d) {
                cc = 2;
                break;
            }
            addr++;
        }
        mask = (mask << 1) & 0xf;
        r1 <<= 8;
    }
    HELPER_LOG("\n");
    return cc;
}

static inline uint64_t fix_address(CPUS390XState *env, uint64_t a)
{
    /* 31-Bit mode */
    if (!(env->psw.mask & PSW_MASK_64)) {
        a &= 0x7fffffff;
    }
    return a;
}

static inline uint64_t get_address(CPUS390XState *env, int x2, int b2, int d2)
{
    uint64_t r = d2;
    if (x2) {
        r += env->regs[x2];
    }
    if (b2) {
        r += env->regs[b2];
    }
    return fix_address(env, r);
}

static inline uint64_t get_address_31fix(CPUS390XState *env, int reg)
{
    return fix_address(env, env->regs[reg]);
}

/* search string (c is byte to search, r2 is string, r1 end of string) */
uint64_t HELPER(srst)(CPUS390XState *env, uint64_t r0, uint64_t end,
                      uint64_t str)
{
    uint32_t len;
    uint8_t v, c = r0;

    str = fix_address(env, str);
    end = fix_address(env, end);

    /* Assume for now that R2 is unmodified.  */
    env->retxl = str;

    /* Lest we fail to service interrupts in a timely manner, limit the
       amount of work we're willing to do.  For now, let's cap at 8k.  */
    for (len = 0; len < 0x2000; ++len) {
        if (str + len == end) {
            /* Character not found.  R1 & R2 are unmodified.  */
            env->cc_op = 2;
            return end;
        }
        v = cpu_ldub_data(env, str + len);
        if (v == c) {
            /* Character found.  Set R1 to the location; R2 is unmodified.  */
            env->cc_op = 1;
            return str + len;
        }
    }

    /* CPU-determined bytes processed.  Advance R2 to next byte to process.  */
    env->retxl = str + len;
    env->cc_op = 3;
    return end;
}

/* unsigned string compare (c is string terminator) */
uint64_t HELPER(clst)(CPUS390XState *env, uint64_t c, uint64_t s1, uint64_t s2)
{
    uint32_t len;

    c = c & 0xff;
    s1 = fix_address(env, s1);
    s2 = fix_address(env, s2);

    /* Lest we fail to service interrupts in a timely manner, limit the
       amount of work we're willing to do.  For now, let's cap at 8k.  */
    for (len = 0; len < 0x2000; ++len) {
        uint8_t v1 = cpu_ldub_data(env, s1 + len);
        uint8_t v2 = cpu_ldub_data(env, s2 + len);
        if (v1 == v2) {
            if (v1 == c) {
                /* Equal.  CC=0, and don't advance the registers.  */
                env->cc_op = 0;
                env->retxl = s2;
                return s1;
            }
        } else {
            /* Unequal.  CC={1,2}, and advance the registers.  Note that
               the terminator need not be zero, but the string that contains
               the terminator is by definition "low".  */
            env->cc_op = (v1 == c ? 1 : v2 == c ? 2 : v1 < v2 ? 1 : 2);
            env->retxl = s2 + len;
            return s1 + len;
        }
    }

    /* CPU-determined bytes equal; advance the registers.  */
    env->cc_op = 3;
    env->retxl = s2 + len;
    return s1 + len;
}

/* move page */
void HELPER(mvpg)(CPUS390XState *env, uint64_t r0, uint64_t r1, uint64_t r2)
{
    /* XXX missing r0 handling */
    env->cc_op = 0;
    fast_memmove(env, r1, r2, TARGET_PAGE_SIZE);
}

/* string copy (c is string terminator) */
uint64_t HELPER(mvst)(CPUS390XState *env, uint64_t c, uint64_t d, uint64_t s)
{
    uint32_t len;

    c = c & 0xff;
    d = fix_address(env, d);
    s = fix_address(env, s);

    /* Lest we fail to service interrupts in a timely manner, limit the
       amount of work we're willing to do.  For now, let's cap at 8k.  */
    for (len = 0; len < 0x2000; ++len) {
        uint8_t v = cpu_ldub_data(env, s + len);
        cpu_stb_data(env, d + len, v);
        if (v == c) {
            /* Complete.  Set CC=1 and advance R1.  */
            env->cc_op = 1;
            env->retxl = s;
            return d + len;
        }
    }

    /* Incomplete.  Set CC=3 and signal to advance R1 and R2.  */
    env->cc_op = 3;
    env->retxl = s + len;
    return d + len;
}

static uint32_t helper_icm(CPUS390XState *env, uint32_t r1, uint64_t address,
                           uint32_t mask)
{
    int pos = 24; /* top of the lower half of r1 */
    uint64_t rmask = 0xff000000ULL;
    uint8_t val = 0;
    int ccd = 0;
    uint32_t cc = 0;

    while (mask) {
        if (mask & 8) {
            env->regs[r1] &= ~rmask;
            val = cpu_ldub_data(env, address);
            if ((val & 0x80) && !ccd) {
                cc = 1;
            }
            ccd = 1;
            if (val && cc == 0) {
                cc = 2;
            }
            env->regs[r1] |= (uint64_t)val << pos;
            address++;
        }
        mask = (mask << 1) & 0xf;
        pos -= 8;
        rmask >>= 8;
    }

    return cc;
}

/* execute instruction
   this instruction executes an insn modified with the contents of r1
   it does not change the executed instruction in memory
   it does not change the program counter
   in other words: tricky...
   currently implemented by interpreting the cases it is most commonly used in
*/
uint32_t HELPER(ex)(CPUS390XState *env, uint32_t cc, uint64_t v1,
                    uint64_t addr, uint64_t ret)
{
    S390CPU *cpu = s390_env_get_cpu(env);
    uint16_t insn = cpu_lduw_code(env, addr);

    HELPER_LOG("%s: v1 0x%lx addr 0x%lx insn 0x%x\n", __func__, v1, addr,
               insn);
    if ((insn & 0xf0ff) == 0xd000) {
        uint32_t l, insn2, b1, b2, d1, d2;

        l = v1 & 0xff;
        insn2 = cpu_ldl_code(env, addr + 2);
        b1 = (insn2 >> 28) & 0xf;
        b2 = (insn2 >> 12) & 0xf;
        d1 = (insn2 >> 16) & 0xfff;
        d2 = insn2 & 0xfff;
        switch (insn & 0xf00) {
        case 0x200:
            helper_mvc(env, l, get_address(env, 0, b1, d1),
                       get_address(env, 0, b2, d2));
            break;
        case 0x400:
            cc = helper_nc(env, l, get_address(env, 0, b1, d1),
                            get_address(env, 0, b2, d2));
            break;
        case 0x500:
            cc = helper_clc(env, l, get_address(env, 0, b1, d1),
                            get_address(env, 0, b2, d2));
            break;
        case 0x600:
            cc = helper_oc(env, l, get_address(env, 0, b1, d1),
                            get_address(env, 0, b2, d2));
            break;
        case 0x700:
            cc = helper_xc(env, l, get_address(env, 0, b1, d1),
                           get_address(env, 0, b2, d2));
            break;
        case 0xc00:
            helper_tr(env, l, get_address(env, 0, b1, d1),
                      get_address(env, 0, b2, d2));
            break;
        case 0xd00:
            cc = helper_trt(env, l, get_address(env, 0, b1, d1),
                            get_address(env, 0, b2, d2));
            break;
        default:
            goto abort;
        }
    } else if ((insn & 0xff00) == 0x0a00) {
        /* supervisor call */
        HELPER_LOG("%s: svc %ld via execute\n", __func__, (insn | v1) & 0xff);
        env->psw.addr = ret - 4;
        env->int_svc_code = (insn | v1) & 0xff;
        env->int_svc_ilen = 4;
        helper_exception(env, EXCP_SVC);
    } else if ((insn & 0xff00) == 0xbf00) {
        uint32_t insn2, r1, r3, b2, d2;

        insn2 = cpu_ldl_code(env, addr + 2);
        r1 = (insn2 >> 20) & 0xf;
        r3 = (insn2 >> 16) & 0xf;
        b2 = (insn2 >> 12) & 0xf;
        d2 = insn2 & 0xfff;
        cc = helper_icm(env, r1, get_address(env, 0, b2, d2), r3);
    } else {
    abort:
        cpu_abort(CPU(cpu), "EXECUTE on instruction prefix 0x%x not implemented\n",
                  insn);
    }
    return cc;
}

/* load access registers r1 to r3 from memory at a2 */
void HELPER(lam)(CPUS390XState *env, uint32_t r1, uint64_t a2, uint32_t r3)
{
    int i;

    for (i = r1;; i = (i + 1) % 16) {
        env->aregs[i] = cpu_ldl_data(env, a2);
        a2 += 4;

        if (i == r3) {
            break;
        }
    }
}

/* store access registers r1 to r3 in memory at a2 */
void HELPER(stam)(CPUS390XState *env, uint32_t r1, uint64_t a2, uint32_t r3)
{
    int i;

    for (i = r1;; i = (i + 1) % 16) {
        cpu_stl_data(env, a2, env->aregs[i]);
        a2 += 4;

        if (i == r3) {
            break;
        }
    }
}

/* move long */
uint32_t HELPER(mvcl)(CPUS390XState *env, uint32_t r1, uint32_t r2)
{
    uint64_t destlen = env->regs[r1 + 1] & 0xffffff;
    uint64_t dest = get_address_31fix(env, r1);
    uint64_t srclen = env->regs[r2 + 1] & 0xffffff;
    uint64_t src = get_address_31fix(env, r2);
    uint8_t pad = env->regs[r2 + 1] >> 24;
    uint8_t v;
    uint32_t cc;

    if (destlen == srclen) {
        cc = 0;
    } else if (destlen < srclen) {
        cc = 1;
    } else {
        cc = 2;
    }

    if (srclen > destlen) {
        srclen = destlen;
    }

    for (; destlen && srclen; src++, dest++, destlen--, srclen--) {
        v = cpu_ldub_data(env, src);
        cpu_stb_data(env, dest, v);
    }

    for (; destlen; dest++, destlen--) {
        cpu_stb_data(env, dest, pad);
    }

    env->regs[r1 + 1] = destlen;
    /* can't use srclen here, we trunc'ed it */
    env->regs[r2 + 1] -= src - env->regs[r2];
    env->regs[r1] = dest;
    env->regs[r2] = src;

    return cc;
}

/* move long extended another memcopy insn with more bells and whistles */
uint32_t HELPER(mvcle)(CPUS390XState *env, uint32_t r1, uint64_t a2,
                       uint32_t r3)
{
    uint64_t destlen = env->regs[r1 + 1];
    uint64_t dest = env->regs[r1];
    uint64_t srclen = env->regs[r3 + 1];
    uint64_t src = env->regs[r3];
    uint8_t pad = a2 & 0xff;
    uint8_t v;
    uint32_t cc;

    if (!(env->psw.mask & PSW_MASK_64)) {
        destlen = (uint32_t)destlen;
        srclen = (uint32_t)srclen;
        dest &= 0x7fffffff;
        src &= 0x7fffffff;
    }

    if (destlen == srclen) {
        cc = 0;
    } else if (destlen < srclen) {
        cc = 1;
    } else {
        cc = 2;
    }

    if (srclen > destlen) {
        srclen = destlen;
    }

    for (; destlen && srclen; src++, dest++, destlen--, srclen--) {
        v = cpu_ldub_data(env, src);
        cpu_stb_data(env, dest, v);
    }

    for (; destlen; dest++, destlen--) {
        cpu_stb_data(env, dest, pad);
    }

    env->regs[r1 + 1] = destlen;
    /* can't use srclen here, we trunc'ed it */
    /* FIXME: 31-bit mode! */
    env->regs[r3 + 1] -= src - env->regs[r3];
    env->regs[r1] = dest;
    env->regs[r3] = src;

    return cc;
}

/* compare logical long extended memcompare insn with padding */
uint32_t HELPER(clcle)(CPUS390XState *env, uint32_t r1, uint64_t a2,
                       uint32_t r3)
{
    uint64_t destlen = env->regs[r1 + 1];
    uint64_t dest = get_address_31fix(env, r1);
    uint64_t srclen = env->regs[r3 + 1];
    uint64_t src = get_address_31fix(env, r3);
    uint8_t pad = a2 & 0xff;
    uint8_t v1 = 0, v2 = 0;
    uint32_t cc = 0;

    if (!(destlen || srclen)) {
        return cc;
    }

    if (srclen > destlen) {
        srclen = destlen;
    }

    for (; destlen || srclen; src++, dest++, destlen--, srclen--) {
        v1 = srclen ? cpu_ldub_data(env, src) : pad;
        v2 = destlen ? cpu_ldub_data(env, dest) : pad;
        if (v1 != v2) {
            cc = (v1 < v2) ? 1 : 2;
            break;
        }
    }

    env->regs[r1 + 1] = destlen;
    /* can't use srclen here, we trunc'ed it */
    env->regs[r3 + 1] -= src - env->regs[r3];
    env->regs[r1] = dest;
    env->regs[r3] = src;

    return cc;
}

/* checksum */
uint64_t HELPER(cksm)(CPUS390XState *env, uint64_t r1,
                      uint64_t src, uint64_t src_len)
{
    uint64_t max_len, len;
    uint64_t cksm = (uint32_t)r1;

    /* Lest we fail to service interrupts in a timely manner, limit the
       amount of work we're willing to do.  For now, let's cap at 8k.  */
    max_len = (src_len > 0x2000 ? 0x2000 : src_len);

    /* Process full words as available.  */
    for (len = 0; len + 4 <= max_len; len += 4, src += 4) {
        cksm += (uint32_t)cpu_ldl_data(env, src);
    }

    switch (max_len - len) {
    case 1:
        cksm += cpu_ldub_data(env, src) << 24;
        len += 1;
        break;
    case 2:
        cksm += cpu_lduw_data(env, src) << 16;
        len += 2;
        break;
    case 3:
        cksm += cpu_lduw_data(env, src) << 16;
        cksm += cpu_ldub_data(env, src + 2) << 8;
        len += 3;
        break;
    }

    /* Fold the carry from the checksum.  Note that we can see carry-out
       during folding more than once (but probably not more than twice).  */
    while (cksm > 0xffffffffull) {
        cksm = (uint32_t)cksm + (cksm >> 32);
    }

    /* Indicate whether or not we've processed everything.  */
    env->cc_op = (len == src_len ? 0 : 3);

    /* Return both cksm and processed length.  */
    env->retxl = cksm;
    return len;
}

void HELPER(unpk)(CPUS390XState *env, uint32_t len, uint64_t dest,
                  uint64_t src)
{
    int len_dest = len >> 4;
    int len_src = len & 0xf;
    uint8_t b;
    int second_nibble = 0;

    dest += len_dest;
    src += len_src;

    /* last byte is special, it only flips the nibbles */
    b = cpu_ldub_data(env, src);
    cpu_stb_data(env, dest, (b << 4) | (b >> 4));
    src--;
    len_src--;

    /* now pad every nibble with 0xf0 */

    while (len_dest > 0) {
        uint8_t cur_byte = 0;

        if (len_src > 0) {
            cur_byte = cpu_ldub_data(env, src);
        }

        len_dest--;
        dest--;

        /* only advance one nibble at a time */
        if (second_nibble) {
            cur_byte >>= 4;
            len_src--;
            src--;
        }
        second_nibble = !second_nibble;

        /* digit */
        cur_byte = (cur_byte & 0xf);
        /* zone bits */
        cur_byte |= 0xf0;

        cpu_stb_data(env, dest, cur_byte);
    }
}

void HELPER(tr)(CPUS390XState *env, uint32_t len, uint64_t array,
                uint64_t trans)
{
    int i;

    for (i = 0; i <= len; i++) {
        uint8_t byte = cpu_ldub_data(env, array + i);
        uint8_t new_byte = cpu_ldub_data(env, trans + byte);

        cpu_stb_data(env, array + i, new_byte);
    }
}

uint64_t HELPER(tre)(CPUS390XState *env, uint64_t array,
                     uint64_t len, uint64_t trans)
{
    uint8_t end = env->regs[0] & 0xff;
    uint64_t l = len;
    uint64_t i;

    if (!(env->psw.mask & PSW_MASK_64)) {
        array &= 0x7fffffff;
        l = (uint32_t)l;
    }

    /* Lest we fail to service interrupts in a timely manner, limit the
       amount of work we're willing to do.  For now, let's cap at 8k.  */
    if (l > 0x2000) {
        l = 0x2000;
        env->cc_op = 3;
    } else {
        env->cc_op = 0;
    }

    for (i = 0; i < l; i++) {
        uint8_t byte, new_byte;

        byte = cpu_ldub_data(env, array + i);

        if (byte == end) {
            env->cc_op = 1;
            break;
        }

        new_byte = cpu_ldub_data(env, trans + byte);
        cpu_stb_data(env, array + i, new_byte);
    }

    env->retxl = len - i;
    return array + i;
}

uint32_t HELPER(trt)(CPUS390XState *env, uint32_t len, uint64_t array,
                     uint64_t trans)
{
    uint32_t cc = 0;
    int i;

    for (i = 0; i <= len; i++) {
        uint8_t byte = cpu_ldub_data(env, array + i);
        uint8_t sbyte = cpu_ldub_data(env, trans + byte);

        if (sbyte != 0) {
            env->regs[1] = array + i;
            env->regs[2] = (env->regs[2] & ~0xff) | sbyte;
            cc = (i == len) ? 2 : 1;
            break;
        }
    }

    return cc;
}

#if !defined(CONFIG_USER_ONLY)
void HELPER(lctlg)(CPUS390XState *env, uint32_t r1, uint64_t a2, uint32_t r3)
{
    S390CPU *cpu = s390_env_get_cpu(env);
    bool PERchanged = false;
    int i;
    uint64_t src = a2;
    uint64_t val;

    for (i = r1;; i = (i + 1) % 16) {
        val = cpu_ldq_data(env, src);
        if (env->cregs[i] != val && i >= 9 && i <= 11) {
            PERchanged = true;
        }
        env->cregs[i] = val;
        HELPER_LOG("load ctl %d from 0x%" PRIx64 " == 0x%" PRIx64 "\n",
                   i, src, env->cregs[i]);
        src += sizeof(uint64_t);

        if (i == r3) {
            break;
        }
    }

    if (PERchanged && env->psw.mask & PSW_MASK_PER) {
        s390_cpu_recompute_watchpoints(CPU(cpu));
    }

    tlb_flush(CPU(cpu));
}

void HELPER(lctl)(CPUS390XState *env, uint32_t r1, uint64_t a2, uint32_t r3)
{
    S390CPU *cpu = s390_env_get_cpu(env);
    bool PERchanged = false;
    int i;
    uint64_t src = a2;
    uint32_t val;

    for (i = r1;; i = (i + 1) % 16) {
        val = cpu_ldl_data(env, src);
        if ((uint32_t)env->cregs[i] != val && i >= 9 && i <= 11) {
            PERchanged = true;
        }
        env->cregs[i] = (env->cregs[i] & 0xFFFFFFFF00000000ULL) | val;
        src += sizeof(uint32_t);

        if (i == r3) {
            break;
        }
    }

    if (PERchanged && env->psw.mask & PSW_MASK_PER) {
        s390_cpu_recompute_watchpoints(CPU(cpu));
    }

    tlb_flush(CPU(cpu));
}

void HELPER(stctg)(CPUS390XState *env, uint32_t r1, uint64_t a2, uint32_t r3)
{
    int i;
    uint64_t dest = a2;

    for (i = r1;; i = (i + 1) % 16) {
        cpu_stq_data(env, dest, env->cregs[i]);
        dest += sizeof(uint64_t);

        if (i == r3) {
            break;
        }
    }
}

void HELPER(stctl)(CPUS390XState *env, uint32_t r1, uint64_t a2, uint32_t r3)
{
    int i;
    uint64_t dest = a2;

    for (i = r1;; i = (i + 1) % 16) {
        cpu_stl_data(env, dest, env->cregs[i]);
        dest += sizeof(uint32_t);

        if (i == r3) {
            break;
        }
    }
}

uint32_t HELPER(tprot)(uint64_t a1, uint64_t a2)
{
    /* XXX implement */

    return 0;
}

/* insert storage key extended */
uint64_t HELPER(iske)(CPUS390XState *env, uint64_t r2)
{
    static S390SKeysState *ss;
    static S390SKeysClass *skeyclass;
    uint64_t addr = get_address(env, 0, 0, r2);
    uint8_t key;

    if (addr > ram_size) {
        return 0;
    }

    if (unlikely(!ss)) {
        ss = s390_get_skeys_device();
        skeyclass = S390_SKEYS_GET_CLASS(ss);
    }

    if (skeyclass->get_skeys(ss, addr / TARGET_PAGE_SIZE, 1, &key)) {
        return 0;
    }
    return key;
}

/* set storage key extended */
void HELPER(sske)(CPUS390XState *env, uint64_t r1, uint64_t r2)
{
    static S390SKeysState *ss;
    static S390SKeysClass *skeyclass;
    uint64_t addr = get_address(env, 0, 0, r2);
    uint8_t key;

    if (addr > ram_size) {
        return;
    }

    if (unlikely(!ss)) {
        ss = s390_get_skeys_device();
        skeyclass = S390_SKEYS_GET_CLASS(ss);
    }

    key = (uint8_t) r1;
    skeyclass->set_skeys(ss, addr / TARGET_PAGE_SIZE, 1, &key);
}

/* reset reference bit extended */
uint32_t HELPER(rrbe)(CPUS390XState *env, uint64_t r2)
{
    static S390SKeysState *ss;
    static S390SKeysClass *skeyclass;
    uint8_t re, key;

    if (r2 > ram_size) {
        return 0;
    }

    if (unlikely(!ss)) {
        ss = s390_get_skeys_device();
        skeyclass = S390_SKEYS_GET_CLASS(ss);
    }

    if (skeyclass->get_skeys(ss, r2 / TARGET_PAGE_SIZE, 1, &key)) {
        return 0;
    }

    re = key & (SK_R | SK_C);
    key &= ~SK_R;

    if (skeyclass->set_skeys(ss, r2 / TARGET_PAGE_SIZE, 1, &key)) {
        return 0;
    }

    /*
     * cc
     *
     * 0  Reference bit zero; change bit zero
     * 1  Reference bit zero; change bit one
     * 2  Reference bit one; change bit zero
     * 3  Reference bit one; change bit one
     */

    return re >> 1;
}

/* compare and swap and purge */
uint32_t HELPER(csp)(CPUS390XState *env, uint32_t r1, uint64_t r2)
{
    S390CPU *cpu = s390_env_get_cpu(env);
    uint32_t cc;
    uint32_t o1 = env->regs[r1];
    uint64_t a2 = r2 & ~3ULL;
    uint32_t o2 = cpu_ldl_data(env, a2);

    if (o1 == o2) {
        cpu_stl_data(env, a2, env->regs[(r1 + 1) & 15]);
        if (r2 & 0x3) {
            /* flush TLB / ALB */
            tlb_flush(CPU(cpu));
        }
        cc = 0;
    } else {
        env->regs[r1] = (env->regs[r1] & 0xffffffff00000000ULL) | o2;
        cc = 1;
    }

    return cc;
}

uint32_t HELPER(mvcs)(CPUS390XState *env, uint64_t l, uint64_t a1, uint64_t a2)
{
    int cc = 0, i;

    HELPER_LOG("%s: %16" PRIx64 " %16" PRIx64 " %16" PRIx64 "\n",
               __func__, l, a1, a2);

    if (l > 256) {
        /* max 256 */
        l = 256;
        cc = 3;
    }

    /* XXX replace w/ memcpy */
    for (i = 0; i < l; i++) {
        cpu_stb_secondary(env, a1 + i, cpu_ldub_primary(env, a2 + i));
    }

    return cc;
}

uint32_t HELPER(mvcp)(CPUS390XState *env, uint64_t l, uint64_t a1, uint64_t a2)
{
    int cc = 0, i;

    HELPER_LOG("%s: %16" PRIx64 " %16" PRIx64 " %16" PRIx64 "\n",
               __func__, l, a1, a2);

    if (l > 256) {
        /* max 256 */
        l = 256;
        cc = 3;
    }

    /* XXX replace w/ memcpy */
    for (i = 0; i < l; i++) {
        cpu_stb_primary(env, a1 + i, cpu_ldub_secondary(env, a2 + i));
    }

    return cc;
}

/* invalidate pte */
void HELPER(ipte)(CPUS390XState *env, uint64_t pte_addr, uint64_t vaddr)
{
    CPUState *cs = CPU(s390_env_get_cpu(env));
    uint64_t page = vaddr & TARGET_PAGE_MASK;
    uint64_t pte = 0;

    /* XXX broadcast to other CPUs */

    /* XXX Linux is nice enough to give us the exact pte address.
       According to spec we'd have to find it out ourselves */
    /* XXX Linux is fine with overwriting the pte, the spec requires
       us to only set the invalid bit */
    stq_phys(cs->as, pte_addr, pte | _PAGE_INVALID);

    /* XXX we exploit the fact that Linux passes the exact virtual
       address here - it's not obliged to! */
    tlb_flush_page(cs, page);

    /* XXX 31-bit hack */
    if (page & 0x80000000) {
        tlb_flush_page(cs, page & ~0x80000000);
    } else {
        tlb_flush_page(cs, page | 0x80000000);
    }
}

/* flush local tlb */
void HELPER(ptlb)(CPUS390XState *env)
{
    S390CPU *cpu = s390_env_get_cpu(env);

    tlb_flush(CPU(cpu));
}

/* load using real address */
uint64_t HELPER(lura)(CPUS390XState *env, uint64_t addr)
{
    CPUState *cs = CPU(s390_env_get_cpu(env));

    return (uint32_t)ldl_phys(cs->as, get_address(env, 0, 0, addr));
}

uint64_t HELPER(lurag)(CPUS390XState *env, uint64_t addr)
{
    CPUState *cs = CPU(s390_env_get_cpu(env));

    return ldq_phys(cs->as, get_address(env, 0, 0, addr));
}

/* store using real address */
void HELPER(stura)(CPUS390XState *env, uint64_t addr, uint64_t v1)
{
    CPUState *cs = CPU(s390_env_get_cpu(env));

    stl_phys(cs->as, get_address(env, 0, 0, addr), (uint32_t)v1);

    if ((env->psw.mask & PSW_MASK_PER) &&
        (env->cregs[9] & PER_CR9_EVENT_STORE) &&
        (env->cregs[9] & PER_CR9_EVENT_STORE_REAL)) {
        /* PSW is saved just before calling the helper.  */
        env->per_address = env->psw.addr;
        env->per_perc_atmid = PER_CODE_EVENT_STORE_REAL | get_per_atmid(env);
    }
}

void HELPER(sturg)(CPUS390XState *env, uint64_t addr, uint64_t v1)
{
    CPUState *cs = CPU(s390_env_get_cpu(env));

    stq_phys(cs->as, get_address(env, 0, 0, addr), v1);

    if ((env->psw.mask & PSW_MASK_PER) &&
        (env->cregs[9] & PER_CR9_EVENT_STORE) &&
        (env->cregs[9] & PER_CR9_EVENT_STORE_REAL)) {
        /* PSW is saved just before calling the helper.  */
        env->per_address = env->psw.addr;
        env->per_perc_atmid = PER_CODE_EVENT_STORE_REAL | get_per_atmid(env);
    }
}

/* load real address */
uint64_t HELPER(lra)(CPUS390XState *env, uint64_t addr)
{
    CPUState *cs = CPU(s390_env_get_cpu(env));
    uint32_t cc = 0;
    int old_exc = cs->exception_index;
    uint64_t asc = env->psw.mask & PSW_MASK_ASC;
    uint64_t ret;
    int flags;

    /* XXX incomplete - has more corner cases */
    if (!(env->psw.mask & PSW_MASK_64) && (addr >> 32)) {
        program_interrupt(env, PGM_SPECIAL_OP, 2);
    }

    cs->exception_index = old_exc;
    if (mmu_translate(env, addr, 0, asc, &ret, &flags, true)) {
        cc = 3;
    }
    if (cs->exception_index == EXCP_PGM) {
        ret = env->int_pgm_code | 0x80000000;
    } else {
        ret |= addr & ~TARGET_PAGE_MASK;
    }
    cs->exception_index = old_exc;

    env->cc_op = cc;
    return ret;
}
#endif
