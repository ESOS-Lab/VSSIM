/*
 * QEMU 8253/8254 interval timer emulation
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "hw.h"
#include "pc.h"
#include "isa.h"
#include "qemu-timer.h"
#include "i8254.h"
#include "qemu-kvm.h"

static PITState pit_state;

static void kvm_pit_save(QEMUFile *f, void *opaque)
{
    PITState *s = opaque;
    struct kvm_pit_state2 pit2;
    struct kvm_pit_channel_state *c;
    struct PITChannelState *sc;
    int i;

    if(qemu_kvm_has_pit_state2()) {
        kvm_get_pit2(kvm_context, &pit2);
        s->flags = pit2.flags;
    } else {
        /* pit2 is superset of pit struct so just cast it and use it */
        kvm_get_pit(kvm_context, (struct kvm_pit_state *)&pit2);
    }
    for (i = 0; i < 3; i++) {
	c = &pit2.channels[i];
	sc = &s->channels[i];
	sc->count = c->count;
	sc->latched_count = c->latched_count;
	sc->count_latched = c->count_latched;
	sc->status_latched = c->status_latched;
	sc->status = c->status;
	sc->read_state = c->read_state;
	sc->write_state = c->write_state;
	sc->write_latch = c->write_latch;
	sc->rw_mode = c->rw_mode;
	sc->mode = c->mode;
	sc->bcd = c->bcd;
	sc->gate = c->gate;
	sc->count_load_time = c->count_load_time;
    }

    pit_save(f, s);
}

static int kvm_pit_load(QEMUFile *f, void *opaque, int version_id)
{
    PITState *s = opaque;
    struct kvm_pit_state2 pit2;
    struct kvm_pit_channel_state *c;
    struct PITChannelState *sc;
    int i;

    pit_load(f, s, version_id);

    pit2.flags = s->flags;
    for (i = 0; i < 3; i++) {
	c = &pit2.channels[i];
	sc = &s->channels[i];
	c->count = sc->count;
	c->latched_count = sc->latched_count;
	c->count_latched = sc->count_latched;
	c->status_latched = sc->status_latched;
	c->status = sc->status;
	c->read_state = sc->read_state;
	c->write_state = sc->write_state;
	c->write_latch = sc->write_latch;
	c->rw_mode = sc->rw_mode;
	c->mode = sc->mode;
	c->bcd = sc->bcd;
	c->gate = sc->gate;
	c->count_load_time = sc->count_load_time;
    }

    if(qemu_kvm_has_pit_state2()) {
        kvm_set_pit2(kvm_context, &pit2);
    } else {
        kvm_set_pit(kvm_context, (struct kvm_pit_state *)&pit2);
    }
    return 0;
}

PITState *kvm_pit_init(int base, qemu_irq irq)
{
    PITState *pit = &pit_state;

    register_savevm(PIT_SAVEVM_NAME, base, PIT_SAVEVM_VERSION,
		    kvm_pit_save, kvm_pit_load, pit);

    qemu_register_reset(pit_reset, pit);
    pit_reset(pit);

    return pit;
}
