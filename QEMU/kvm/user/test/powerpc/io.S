#define SPRN_MMUCR 0x3b2

#define TLBWORD0 0xf0000210
#define TLBWORD1 0xf0000000
#define TLBWORD2 0x00000003

.global _start
_start:
	li	r4, 0
	mtspr	SPRN_MMUCR, r4

	li	r3, 2

	lis	r4, TLBWORD0@h
	ori	r4, r4, TLBWORD0@l
	tlbwe	r4, r3, 0

	lis	r4, TLBWORD1@h
	ori	r4, r4, TLBWORD1@l
	tlbwe	r4, r3, 1

	lis	r4, TLBWORD2@h
	ori	r4, r4, TLBWORD2@l
	tlbwe	r4, r3, 2

	lis	r3, 0xf000
	lis	r4, 0x1234
	ori	r4, r4, 0x5678
	stb	r4, 0(r3)
	lbz	r5, 0(r3)

	b	.
