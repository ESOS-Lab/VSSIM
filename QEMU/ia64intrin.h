#ifndef IA64_INTRINSIC_H
#define IA64_INTRINSIC_H

/*
 * Compiler-dependent Intrinsics
 *
 * Copyright (C) 2002,2003 Jun Nakajima <jun.nakajima@intel.com>
 * Copyright (C) 2002,2003 Suresh Siddha <suresh.b.siddha@intel.com>
 *
 */
extern long ia64_cmpxchg_called_with_bad_pointer (void);
extern void ia64_bad_param_for_getreg (void);
#define ia64_cmpxchg(sem,ptr,o,n,s) ({					\
	uint64_t _o, _r;						\
	switch(s) {							\
		case 1: _o = (uint8_t)(long)(o); break;			\
		case 2: _o = (uint16_t)(long)(o); break;		\
		case 4: _o = (uint32_t)(long)(o); break;		\
		case 8: _o = (uint64_t)(long)(o); break;		\
		default: break;						\
	}								\
	switch(s) {							\
		case 1:							\
		_r = ia64_cmpxchg1_##sem((uint8_t*)ptr,n,_o); break;	\
		case 2:							\
		_r = ia64_cmpxchg2_##sem((uint16_t*)ptr,n,_o); break;	\
		case 4:							\
		_r = ia64_cmpxchg4_##sem((uint32_t*)ptr,n,_o); break;	\
		case 8:							\
		_r = ia64_cmpxchg8_##sem((uint64_t*)ptr,n,_o); break;	\
		default:						\
		_r = ia64_cmpxchg_called_with_bad_pointer(); break;	\
	}								\
	(__typeof__(o)) _r;						\
})

#define cmpxchg_acq(ptr,o,n) ia64_cmpxchg(acq,ptr,o,n,sizeof(*ptr))
#define cmpxchg_rel(ptr,o,n) ia64_cmpxchg(rel,ptr,o,n,sizeof(*ptr))

#ifdef __INTEL_COMPILER
void  __fc(uint64_t *addr);
void  __synci(void);
void __isrlz(void);
void __dsrlz(void);
uint64_t __getReg(const int whichReg);
uint64_t _InterlockedCompareExchange8_rel(volatile uint8_t *dest, uint64_t xchg, uint64_t comp);
uint64_t _InterlockedCompareExchange8_acq(volatile uint8_t *dest, uint64_t xchg, uint64_t comp);
uint64_t _InterlockedCompareExchange16_rel(volatile uint16_t *dest, uint64_t xchg, uint64_t comp);
uint64_t _InterlockedCompareExchange16_acq(volatile uint16_t *dest, uint64_t xchg, uint64_t comp);
uint64_t _InterlockedCompareExchange_rel(volatile uint32_t *dest, uint64_t xchg, uint64_t comp);
uint64_t _InterlockedCompareExchange_acq(volatile uint32_t *dest, uint64_t xchg, uint64_t comp);
uint64_t _InterlockedCompareExchange64_rel(volatile uint64_t *dest, uint64_t xchg, uint64_t comp);
u64_t _InterlockedCompareExchange64_acq(volatile uint64_t *dest, uint64_t xchg, uint64_t comp);

#define ia64_cmpxchg1_rel	_InterlockedCompareExchange8_rel
#define ia64_cmpxchg1_acq	_InterlockedCompareExchange8_acq
#define ia64_cmpxchg2_rel	_InterlockedCompareExchange16_rel
#define ia64_cmpxchg2_acq	_InterlockedCompareExchange16_acq
#define ia64_cmpxchg4_rel	_InterlockedCompareExchange_rel
#define ia64_cmpxchg4_acq	_InterlockedCompareExchange_acq
#define ia64_cmpxchg8_rel	_InterlockedCompareExchange64_rel
#define ia64_cmpxchg8_acq	_InterlockedCompareExchange64_acq

#define ia64_srlz_d		__dsrlz
#define ia64_srlz_i		__isrlz
#define __ia64_fc 		__fc
#define ia64_sync_i		__synci
#define __ia64_getreg		__getReg
#else /* __INTEL_COMPILER */
#define ia64_cmpxchg1_acq(ptr, new, old)						\
({											\
	uint64_t ia64_intri_res;							\
	asm volatile ("mov ar.ccv=%0;;" :: "rO"(old));					\
	asm volatile ("cmpxchg1.acq %0=[%1],%2,ar.ccv":					\
			      "=r"(ia64_intri_res) : "r"(ptr), "r"(new) : "memory");	\
	ia64_intri_res;									\
})

#define ia64_cmpxchg1_rel(ptr, new, old)						\
({											\
	uint64_t ia64_intri_res;							\
	asm volatile ("mov ar.ccv=%0;;" :: "rO"(old));					\
	asm volatile ("cmpxchg1.rel %0=[%1],%2,ar.ccv":					\
			      "=r"(ia64_intri_res) : "r"(ptr), "r"(new) : "memory");	\
	ia64_intri_res;									\
})

#define ia64_cmpxchg2_acq(ptr, new, old)						\
({											\
	uint64_t ia64_intri_res;							\
	asm volatile ("mov ar.ccv=%0;;" :: "rO"(old));					\
	asm volatile ("cmpxchg2.acq %0=[%1],%2,ar.ccv":					\
			      "=r"(ia64_intri_res) : "r"(ptr), "r"(new) : "memory");	\
	ia64_intri_res;									\
})

#define ia64_cmpxchg2_rel(ptr, new, old)						\
({											\
	uint64_t ia64_intri_res;							\
	asm volatile ("mov ar.ccv=%0;;" :: "rO"(old));					\
											\
	asm volatile ("cmpxchg2.rel %0=[%1],%2,ar.ccv":					\
			      "=r"(ia64_intri_res) : "r"(ptr), "r"(new) : "memory");	\
	ia64_intri_res;									\
})

#define ia64_cmpxchg4_acq(ptr, new, old)						\
({											\
	uint64_t ia64_intri_res;							\
	asm volatile ("mov ar.ccv=%0;;" :: "rO"(old));					\
	asm volatile ("cmpxchg4.acq %0=[%1],%2,ar.ccv":					\
			      "=r"(ia64_intri_res) : "r"(ptr), "r"(new) : "memory");	\
	ia64_intri_res;									\
})

#define ia64_cmpxchg4_rel(ptr, new, old)						\
({											\
	uint64_t ia64_intri_res;							\
	asm volatile ("mov ar.ccv=%0;;" :: "rO"(old));					\
	asm volatile ("cmpxchg4.rel %0=[%1],%2,ar.ccv":					\
			      "=r"(ia64_intri_res) : "r"(ptr), "r"(new) : "memory");	\
	ia64_intri_res;									\
})

#define ia64_cmpxchg8_acq(ptr, new, old)						\
({											\
	uint64_t ia64_intri_res;							\
	asm volatile ("mov ar.ccv=%0;;" :: "rO"(old));					\
	asm volatile ("cmpxchg8.acq %0=[%1],%2,ar.ccv":					\
			      "=r"(ia64_intri_res) : "r"(ptr), "r"(new) : "memory");	\
	ia64_intri_res;									\
})

#define ia64_cmpxchg8_rel(ptr, new, old)						\
({											\
	uint64_t ia64_intri_res;							\
	asm volatile ("mov ar.ccv=%0;;" :: "rO"(old));					\
											\
	asm volatile ("cmpxchg8.rel %0=[%1],%2,ar.ccv":					\
			      "=r"(ia64_intri_res) : "r"(ptr), "r"(new) : "memory");	\
	ia64_intri_res;									\
})

#define ia64_srlz_i()	asm volatile (";; srlz.i ;;" ::: "memory")
#define ia64_srlz_d()	asm volatile (";; srlz.d" ::: "memory");
#define __ia64_fc(addr)	asm volatile ("fc %0" :: "r"(addr) : "memory")
#define ia64_sync_i()	asm volatile (";; sync.i" ::: "memory")

#endif /* __INTEL_COMPILER */
#endif /* IA64_INTRINSIC_H */
