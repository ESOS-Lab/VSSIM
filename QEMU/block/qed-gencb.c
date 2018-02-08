/*
 * QEMU Enhanced Disk Format
 *
 * Copyright IBM, Corp. 2010
 *
 * Authors:
 *  Stefan Hajnoczi   <stefanha@linux.vnet.ibm.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "qed.h"

void *gencb_alloc(size_t len, BlockCompletionFunc *cb, void *opaque)
{
    GenericCB *gencb = g_malloc(len);
    gencb->cb = cb;
    gencb->opaque = opaque;
    return gencb;
}

void gencb_complete(void *opaque, int ret)
{
    GenericCB *gencb = opaque;
    BlockCompletionFunc *cb = gencb->cb;
    void *user_opaque = gencb->opaque;

    g_free(gencb);
    cb(user_opaque, ret);
}
