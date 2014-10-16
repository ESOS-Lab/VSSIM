/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Copyright IBM Corp. 2008
 *
 * Authors: Hollis Blanchard <hollisb@us.ibm.com>
 */

#include "libcflat.h"

#define TLB_SIZE 64

extern void tlbwe(unsigned int index,
		  unsigned char tid,
		  unsigned int word0,
		  unsigned int word1,
		  unsigned int word2);

unsigned int next_free_index;

#define PAGE_SHIFT 12
#define PAGE_MASK (~((1<<PAGE_SHIFT)-1))

#define V (1<<9)

void map(unsigned long vaddr, unsigned long paddr)
{
	unsigned int w0, w1, w2;

	/* We don't install exception handlers, so we can't handle TLB misses,
	 * so we can't loop around and overwrite entry 0. */
	if (next_free_index++ >= TLB_SIZE)
		panic("TLB overflow");

	w0 = (vaddr & PAGE_MASK) | V;
	w1 = paddr & PAGE_MASK;
	w2 = 0x3;

	tlbwe(next_free_index, 0, w0, w1, w2);
}
