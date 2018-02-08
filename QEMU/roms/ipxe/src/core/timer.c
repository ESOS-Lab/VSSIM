/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <unistd.h>
#include <ipxe/process.h>
#include <ipxe/console.h>
#include <ipxe/keys.h>
#include <ipxe/nap.h>

/**
 * Delay for a fixed number of milliseconds
 *
 * @v msecs		Number of milliseconds for which to delay
 */
void mdelay ( unsigned long msecs ) {
	while ( msecs-- )
		udelay ( 1000 );
}

/**
 * Sleep (interruptibly) for a fixed number of seconds
 *
 * @v secs		Number of seconds for which to delay
 * @ret secs		Number of seconds remaining, if interrupted
 */
unsigned int sleep ( unsigned int secs ) {
	unsigned long start = currticks();
	unsigned long now;

	for ( ; secs ; secs-- ) {
		while ( ( ( now = currticks() ) - start ) < TICKS_PER_SEC ) {
			step();
			if ( iskey() && ( getchar() == CTRL_C ) )
				return secs;
			cpu_nap();
		}
		start = now;
	}

	return 0;
}
