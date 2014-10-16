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

#define SPRN_MMUCR 0x3b2

/* tlbwe(uint index, uint8_t tid, uint word0, uint word1, uint word2) */
.global tlbwe
tlbwe:
	mtspr	SPRN_MMUCR, r4
	tlbwe	r5, r3, 0
	tlbwe	r6, r3, 1
	tlbwe	r7, r3, 2
	blr
