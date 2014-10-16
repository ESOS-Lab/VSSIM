/*
 * Kernel-based Virtual Machine test driver
 *
 * This test driver provides a simple way of testing kvm, without a full
 * device model.
 *
 * Copyright (C) 2006 Qumranet
 *
 * Authors:
 *
 *  Avi Kivity <avi@qumranet.com>
 *  Yaniv Kamay <yaniv@qumranet.com>
 *
 * This work is licensed under the GNU LGPL license, version 2.
 */

#include <stdint.h>

#define MAX_IO_TABLE	50

typedef int (io_table_handler_t)(void *, int, int, uint64_t, uint64_t *);

struct io_table_entry
{
	uint64_t start;
	uint64_t end;
	io_table_handler_t *handler;
	void *opaque;
};

struct io_table
{
	int nr_entries;
	struct io_table_entry entries[MAX_IO_TABLE];
};

struct io_table_entry *io_table_lookup(struct io_table *io_table,
                                       uint64_t addr);
int io_table_register(struct io_table *io_table, uint64_t start, uint64_t size,
                      io_table_handler_t *handler, void *opaque);
