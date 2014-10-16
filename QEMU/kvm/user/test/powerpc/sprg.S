
.global _start
_start:
	li	r3, 42
	mtsprg	0, r3
	mfsprg	r4, 0
	b	.
