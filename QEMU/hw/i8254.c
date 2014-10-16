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
#include "qemu-kvm.h"
#include "i8254.h"

//#define DEBUG_PIT

static PITState pit_state;

static void pit_irq_timer_update(PITChannelState *s, int64_t current_time);

static int pit_get_count(PITChannelState *s)
{
    uint64_t d;
    int counter;

    d = muldiv64(qemu_get_clock(vm_clock) - s->count_load_time, PIT_FREQ, ticks_per_sec);
    switch(s->mode) {
    case 0:
    case 1:
    case 4:
    case 5:
        counter = (s->count - d) & 0xffff;
        break;
    case 3:
        /* XXX: may be incorrect for odd counts */
        counter = s->count - ((2 * d) % s->count);
        break;
    default:
        counter = s->count - (d % s->count);
        break;
    }
    return counter;
}

/* get pit output bit */
static int pit_get_out1(PITChannelState *s, int64_t current_time)
{
    uint64_t d;
    int out;

    d = muldiv64(current_time - s->count_load_time, PIT_FREQ, ticks_per_sec);
    switch(s->mode) {
    default:
    case 0:
        out = (d >= s->count);
        break;
    case 1:
        out = (d < s->count);
        break;
    case 2:
        if ((d % s->count) == 0 && d != 0)
            out = 1;
        else
            out = 0;
        break;
    case 3:
        out = (d % s->count) < ((s->count + 1) >> 1);
        break;
    case 4:
    case 5:
        out = (d == s->count);
        break;
    }
    return out;
}

int pit_get_out(PITState *pit, int channel, int64_t current_time)
{
    PITChannelState *s = &pit->channels[channel];
    return pit_get_out1(s, current_time);
}

/* return -1 if no transition will occur.  */
static int64_t pit_get_next_transition_time(PITChannelState *s,
                                            int64_t current_time)
{
    uint64_t d, next_time, base;
    int period2;

    d = muldiv64(current_time - s->count_load_time, PIT_FREQ, ticks_per_sec);
    switch(s->mode) {
    default:
    case 0:
    case 1:
        if (d < s->count)
            next_time = s->count;
        else
            return -1;
        break;
    case 2:
        base = (d / s->count) * s->count;
        if ((d - base) == 0 && d != 0)
            next_time = base + s->count;
        else
            next_time = base + s->count + 1;
        break;
    case 3:
        base = (d / s->count) * s->count;
        period2 = ((s->count + 1) >> 1);
        if ((d - base) < period2)
            next_time = base + period2;
        else
            next_time = base + s->count;
        break;
    case 4:
    case 5:
        if (d < s->count)
            next_time = s->count;
        else if (d == s->count)
            next_time = s->count + 1;
        else
            return -1;
        break;
    }
    /* convert to timer units */
    next_time = s->count_load_time + muldiv64(next_time, ticks_per_sec, PIT_FREQ);
    /* fix potential rounding problems */
    /* XXX: better solution: use a clock at PIT_FREQ Hz */
    if (next_time <= current_time)
        next_time = current_time + 1;
    return next_time;
}

/* val must be 0 or 1 */
void pit_set_gate(PITState *pit, int channel, int val)
{
    PITChannelState *s = &pit->channels[channel];

    switch(s->mode) {
    default:
    case 0:
    case 4:
        /* XXX: just disable/enable counting */
        break;
    case 1:
    case 5:
        if (s->gate < val) {
            /* restart counting on rising edge */
            s->count_load_time = qemu_get_clock(vm_clock);
            pit_irq_timer_update(s, s->count_load_time);
        }
        break;
    case 2:
    case 3:
        if (s->gate < val) {
            /* restart counting on rising edge */
            s->count_load_time = qemu_get_clock(vm_clock);
            pit_irq_timer_update(s, s->count_load_time);
        }
        /* XXX: disable/enable counting */
        break;
    }
    s->gate = val;
}

int pit_get_gate(PITState *pit, int channel)
{
    PITChannelState *s = &pit->channels[channel];
    return s->gate;
}

int pit_get_initial_count(PITState *pit, int channel)
{
    PITChannelState *s = &pit->channels[channel];
    return s->count;
}

int pit_get_mode(PITState *pit, int channel)
{
    PITChannelState *s = &pit->channels[channel];
    return s->mode;
}

static inline void pit_load_count(PITState *s, int val, int chan)
{
    if (val == 0)
        val = 0x10000;
    s->channels[chan].count_load_time = qemu_get_clock(vm_clock);
    s->channels[chan].count = val;
#ifdef TARGET_I386
    if (chan == 0 && pit_state.flags & PIT_FLAGS_HPET_LEGACY) {
        return;
    }
#endif
    pit_irq_timer_update(&s->channels[chan], s->channels[chan].count_load_time);
}

/* if already latched, do not latch again */
static void pit_latch_count(PITChannelState *s)
{
    if (!s->count_latched) {
        s->latched_count = pit_get_count(s);
        s->count_latched = s->rw_mode;
    }
}

static void pit_ioport_write(void *opaque, uint32_t addr, uint32_t val)
{
    PITState *pit = opaque;
    int channel, access;
    PITChannelState *s;

    addr &= 3;
    if (addr == 3) {
        channel = val >> 6;
        if (channel == 3) {
            /* read back command */
            for(channel = 0; channel < 3; channel++) {
                s = &pit->channels[channel];
                if (val & (2 << channel)) {
                    if (!(val & 0x20)) {
                        pit_latch_count(s);
                    }
                    if (!(val & 0x10) && !s->status_latched) {
                        /* status latch */
                        /* XXX: add BCD and null count */
                        s->status =  (pit_get_out1(s, qemu_get_clock(vm_clock)) << 7) |
                            (s->rw_mode << 4) |
                            (s->mode << 1) |
                            s->bcd;
                        s->status_latched = 1;
                    }
                }
            }
        } else {
            s = &pit->channels[channel];
            access = (val >> 4) & 3;
            if (access == 0) {
                pit_latch_count(s);
            } else {
                s->rw_mode = access;
                s->read_state = access;
                s->write_state = access;

                s->mode = (val >> 1) & 7;
                s->bcd = val & 1;
                /* XXX: update irq timer ? */
            }
        }
    } else {
        s = &pit->channels[addr];
        switch(s->write_state) {
        default:
        case RW_STATE_LSB:
            pit_load_count(pit, val, addr);
            break;
        case RW_STATE_MSB:
            pit_load_count(pit, val << 8, addr);
            break;
        case RW_STATE_WORD0:
            s->write_latch = val;
            s->write_state = RW_STATE_WORD1;
            break;
        case RW_STATE_WORD1:
            pit_load_count(pit, s->write_latch | (val << 8), addr);
            s->write_state = RW_STATE_WORD0;
            break;
        }
    }
}

static uint32_t pit_ioport_read(void *opaque, uint32_t addr)
{
    PITState *pit = opaque;
    int ret, count;
    PITChannelState *s;

    addr &= 3;
    s = &pit->channels[addr];
    if (s->status_latched) {
        s->status_latched = 0;
        ret = s->status;
    } else if (s->count_latched) {
        switch(s->count_latched) {
        default:
        case RW_STATE_LSB:
            ret = s->latched_count & 0xff;
            s->count_latched = 0;
            break;
        case RW_STATE_MSB:
            ret = s->latched_count >> 8;
            s->count_latched = 0;
            break;
        case RW_STATE_WORD0:
            ret = s->latched_count & 0xff;
            s->count_latched = RW_STATE_MSB;
            break;
        }
    } else {
        switch(s->read_state) {
        default:
        case RW_STATE_LSB:
            count = pit_get_count(s);
            ret = count & 0xff;
            break;
        case RW_STATE_MSB:
            count = pit_get_count(s);
            ret = (count >> 8) & 0xff;
            break;
        case RW_STATE_WORD0:
            count = pit_get_count(s);
            ret = count & 0xff;
            s->read_state = RW_STATE_WORD1;
            break;
        case RW_STATE_WORD1:
            count = pit_get_count(s);
            ret = (count >> 8) & 0xff;
            s->read_state = RW_STATE_WORD0;
            break;
        }
    }
    return ret;
}

/* global counters for time-drift fix */
int64_t timer_acks=0, timer_interrupts=0, timer_ints_to_push=0;

extern int time_drift_fix;

static void pit_irq_timer_update(PITChannelState *s, int64_t current_time)
{
    int64_t expire_time;
    int irq_level;

    if (!s->irq_timer)
        return;
    expire_time = pit_get_next_transition_time(s, current_time);
    irq_level = pit_get_out1(s, current_time);
    qemu_set_irq(s->irq, irq_level);
    if (time_drift_fix && irq_level==1) {
        /* FIXME: fine tune timer_max_fix (max fix per tick). 
         *        Should it be 1 (double time), 2 , 4, 10 ? 
         *        Currently setting it to 5% of PIT-ticks-per-second (per PIT-tick)
         */
        const long pit_ticks_per_sec = (s->count>0) ? (PIT_FREQ/s->count) : 0;
        const long timer_max_fix = pit_ticks_per_sec/20;
        const long delta = timer_interrupts - timer_acks;
        const long max_delta = pit_ticks_per_sec * 60; /* one minute */
        if ((delta >  max_delta) && (pit_ticks_per_sec > 0)) {
            printf("time drift is too long, %ld seconds were lost\n", delta/pit_ticks_per_sec);
            timer_acks = timer_interrupts;
            timer_ints_to_push = 0;
        } else if (delta > 0) {
            timer_ints_to_push = MIN(delta, timer_max_fix);
        }
        timer_interrupts++;
    }
#ifdef DEBUG_PIT
    printf("irq_level=%d next_delay=%f\n",
           irq_level,
           (double)(expire_time - current_time) / ticks_per_sec);
#endif
    s->next_transition_time = expire_time;
    if (expire_time != -1) {
        qemu_mod_timer(s->irq_timer, expire_time);
    } else {
        qemu_del_timer(s->irq_timer);
    }
}

static void pit_irq_timer(void *opaque)
{
    PITChannelState *s = opaque;

    pit_irq_timer_update(s, s->next_transition_time);
}

void pit_save(QEMUFile *f, void *opaque)
{
    PITState *pit = opaque;
    PITChannelState *s;
    int i;

    qemu_put_be32(f, pit->flags);
    for(i = 0; i < 3; i++) {
        s = &pit->channels[i];
        qemu_put_be32(f, s->count);
        qemu_put_be16s(f, &s->latched_count);
        qemu_put_8s(f, &s->count_latched);
        qemu_put_8s(f, &s->status_latched);
        qemu_put_8s(f, &s->status);
        qemu_put_8s(f, &s->read_state);
        qemu_put_8s(f, &s->write_state);
        qemu_put_8s(f, &s->write_latch);
        qemu_put_8s(f, &s->rw_mode);
        qemu_put_8s(f, &s->mode);
        qemu_put_8s(f, &s->bcd);
        qemu_put_8s(f, &s->gate);
        qemu_put_be64(f, s->count_load_time);
        if (s->irq_timer) {
            qemu_put_be64(f, s->next_transition_time);
            qemu_put_timer(f, s->irq_timer);
        }
    }
}

int pit_load(QEMUFile *f, void *opaque, int version_id)
{
    PITState *pit = opaque;
    PITChannelState *s;
    int i;

    if (version_id != PIT_SAVEVM_VERSION)
        return -EINVAL;

    pit->flags = qemu_get_be32(f);
    for(i = 0; i < 3; i++) {
        s = &pit->channels[i];
        s->count=qemu_get_be32(f);
        qemu_get_be16s(f, &s->latched_count);
        qemu_get_8s(f, &s->count_latched);
        qemu_get_8s(f, &s->status_latched);
        qemu_get_8s(f, &s->status);
        qemu_get_8s(f, &s->read_state);
        qemu_get_8s(f, &s->write_state);
        qemu_get_8s(f, &s->write_latch);
        qemu_get_8s(f, &s->rw_mode);
        qemu_get_8s(f, &s->mode);
        qemu_get_8s(f, &s->bcd);
        qemu_get_8s(f, &s->gate);
        s->count_load_time=qemu_get_be64(f);
        if (s->irq_timer) {
            s->next_transition_time=qemu_get_be64(f);
            qemu_get_timer(f, s->irq_timer);
        }
    }

    return 0;
}

void pit_reset(void *opaque)
{
    PITState *pit = opaque;
    PITChannelState *s;
    int i;

#ifdef TARGET_I386
    pit->flags &= ~PIT_FLAGS_HPET_LEGACY;
#endif
    for(i = 0;i < 3; i++) {
        s = &pit->channels[i];
        s->mode = 3;
        s->gate = (i != 2);
        pit_load_count(pit, 0, i);
    }
}

#ifdef TARGET_I386
/* When HPET is operating in legacy mode, i8254 timer0 is disabled */

void hpet_disable_pit(void)
{
    PITChannelState *s = &pit_state.channels[0];

    if (kvm_enabled() && qemu_kvm_pit_in_kernel()) {
        if (qemu_kvm_has_pit_state2()) {
            kvm_hpet_disable_kpit();
        } else {
             fprintf(stderr, "%s: kvm does not support pit_state2!\n", __FUNCTION__);
             exit(1);
        }
    } else {
        pit_state.flags |= PIT_FLAGS_HPET_LEGACY;
        if (s->irq_timer) {
            qemu_del_timer(s->irq_timer);
        }
    }
}

/* When HPET is reset or leaving legacy mode, it must reenable i8254
 * timer 0
 */

void hpet_enable_pit(void)
{
    PITState *pit = &pit_state;
    PITChannelState *s = &pit->channels[0];

    if (kvm_enabled() && qemu_kvm_pit_in_kernel()) {
        if (qemu_kvm_has_pit_state2()) {
            kvm_hpet_enable_kpit();
        } else {
             fprintf(stderr, "%s: kvm does not support pit_state2!\n", __FUNCTION__);
             exit(1);
        }
    } else {
        pit_state.flags &= ~PIT_FLAGS_HPET_LEGACY;
        pit_load_count(pit, s->count, 0);
    }
}
#endif

PITState *pit_init(int base, qemu_irq irq)
{
    PITState *pit = &pit_state;
    PITChannelState *s;

    s = &pit->channels[0];
    /* the timer 0 is connected to an IRQ */
    s->irq_timer = qemu_new_timer(vm_clock, pit_irq_timer, s);
    s->irq = irq;

    register_savevm(PIT_SAVEVM_NAME, base, PIT_SAVEVM_VERSION,
		    pit_save, pit_load, pit);

    qemu_register_reset(pit_reset, pit);
    register_ioport_write(base, 4, 1, pit_ioport_write, pit);
    register_ioport_read(base, 3, 1, pit_ioport_read, pit);

    pit_reset(pit);

    return pit;
}
