#define SPRN_MMUCR 0x3b2

/* 16KB mapping at 4MB */
#define TLBWORD0 0x00400220
#define TLBWORD1 0x00400000
#define TLBWORD2 0x00000003

.global _start
_start:
	li	r4, 0
	mtspr	SPRN_MMUCR, r4

	li	r3, 5

	lis	r4, TLBWORD0@h
	ori	r4, r4, TLBWORD0@l
	tlbwe	r4, r3, 0

	lis	r4, TLBWORD1@h
	ori	r4, r4, TLBWORD1@l
	tlbwe	r4, r3, 1

	lis	r4, TLBWORD2@h
	ori	r4, r4, TLBWORD2@l
	tlbwe	r4, r3, 2

	/* load from 4MB */
	lis	r3, 0x0040
	lwz	r4, 0(r3)

	/* load from 4MB+8KB */
	ori	r3, r3, 0x2000
	lwz	r4, 0(r3)

	b	.
