
.literal_position
.literal .GPOS, 1610613508
.literal .GPOC, 1610613512

.global write
write:

	movi a4, 0x01
	rsil a5, 15

next_byte:

	l32i.n a5, a2, 0
	addi a2, a2, 4
	addi a3, a3, -4

	// Counter for all the bits.
	movi a6, 32

next_bit:

	extui a7, a5, 31, 1
	slli a5, a5, 1
	addi a6, a6, -1

	l32r a8, .GPOS
	s32i.n a4, a8, 0
	extw

	bnez a7, write_high

write_zero:

	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	l32r a8, .GPOC
	s32i.n a4, a8, 0
	extw

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

write_high:

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

	l32r a8, .GPOC
	s32i.n a4, a8, 0
	extw

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

next_bit_delay:
	nop
	j next_bit





