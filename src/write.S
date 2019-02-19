.literal_position
.literal .GPOS, 1610613508
.literal .GPOC, 1610613512

.global write
write:

	movi a4, 0x01
	rsil a5, 15

next_byte:

	l8ui a5, a2, 0
	addi a2, a2, 1
	addi a3, a3, -1

	// Counter for all the bits.
	movi a6, 8

next_bit:

	extui a7, a5, 7, 1
	slli a5, a5, 1
	addi a6, a6, -1

	// Writing to GPOS sets GPIO up.
	// Zero = 300 ns up, 900 ns low.
	// One = 600 ns up, 600 ns low.
	l32r a8, .GPOS
	s32i.n a4, a8, 0
	extw

	// Branch to writing one if register is not equal to zero.
	bnez a7, case_one

case_zero:

	// 9 nops; timings checked using logic analyzer.
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	// Writing to GPOC sets GPIO down.
	// At this point 300 ns should have passed since setting GPIO up.
	l32r a8, .GPOC
	s32i.n a4, a8, 0
	extw

	// 43 nops; timings checked using logic analyzer.
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	j end

case_one:

	// 32 nops; timings checked using logic analyzer.
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	// Writing to GPOC sets GPIO down.
	// At this point 600 ns should have passed since setting GPIO up.
	l32r a8, .GPOC
	s32i.n a4, a8, 0
	extw

	// 23 nops; timings checked using logic analyzer.
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

end:
	bnez a6, next_bit_delay
	bnez a3, next_byte

	rsil a5, 0
	ret

// This is to keep timings constant: as reading next byte takes some time, so should this.
next_bit_delay:
	nop
	j next_bit
