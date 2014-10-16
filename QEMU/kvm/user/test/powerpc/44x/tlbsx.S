#define SPRN_MMUCR 0x3b2

#define TLBWORD0 0x10000210
#define TLBWORD1 0x10000000
#define TLBWORD2 0x00000003

.global _start
_start:
	li	r4, 0
	mtspr	SPRN_MMUCR, r4

	li	r3, 23

	lis	r4, TLBWORD0@h
	ori	r4, r4, TLBWORD0@l
	tlbwe	r4, r3, 0

	lis	r4, TLBWORD1@h
	ori	r4, r4, TLBWORD1@l
	tlbwe	r4, r3, 1

	lis	r4, TLBWORD2@h
	ori	r4, r4, TLBWORD2@l
	tlbwe	r4, r3, 2

	lis	r4, 0x1000
	tlbsx	r5, r4, r0
	cmpwi	r5, 23
	beq	good
	trap

good:
	b	.
