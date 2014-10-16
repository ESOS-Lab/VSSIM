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

#ifndef __LIBCFLAT_H
#define __LIBCFLAT_H

#include <stdarg.h>

extern void exit(int code);
extern void panic(char *fmt, ...);

extern unsigned long strlen(const char *buf);
extern char *strcat(char *dest, const char *src);

extern int printf(const char *fmt, ...);
extern int vsnprintf(char *buf, int size, const char *fmt, va_list va);

extern void puts(const char *s);

#endif
