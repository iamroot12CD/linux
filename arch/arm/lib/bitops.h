#include <asm/assembler.h>
#include <asm/unwind.h>

#if __LINUX_ARM_ARCH__ >= 6
	.macro	bitop, name, instr
/* IAMROOT-12D (2016-05-14):
 * --------------------------
 * r0 : 몇번째 비트를 세팅 할 것인지
 * r1 : 비트맵 시작 주소
 * 예) _set_bit(0,bits[1]) 인 경우
 *     bits[0] 의 0번째 비트를 세팅한다. (bit offset = 0, word offset = 0)
 *     _set_bit(33,bits[2]) 인 경우
 *     bits[1] 의 2번째 비트를 세팅한다. (bit offset = 2, word offset = 1)
 */
ENTRY(	\name		)
UNWIND(	.fnstart	)
	ands	ip, r1, #3
	strneb	r1, [ip]		@ assert word-aligned
	mov	r2, #1
	and	r3, r0, #31		@ Get bit offset
	mov	r0, r0, lsr #5
	add	r1, r1, r0, lsl #2	@ Get word offset
#if __LINUX_ARM_ARCH__ >= 7 && defined(CONFIG_SMP)
	.arch_extension	mp
	ALT_SMP(W(pldw)	[r1])
	ALT_UP(W(nop))
#endif
	mov	r3, r2, lsl r3
1:	ldrex	r2, [r1]
	\instr	r2, r2, r3
	strex	r0, r2, [r1]
	cmp	r0, #0
	bne	1b
	bx	lr
UNWIND(	.fnend		)
ENDPROC(\name		)
	.endm

	.macro	testop, name, instr, store
ENTRY(	\name		)
UNWIND(	.fnstart	)
	ands	ip, r1, #3
	strneb	r1, [ip]		@ assert word-aligned
	mov	r2, #1
	and	r3, r0, #31		@ Get bit offset
	mov	r0, r0, lsr #5
	add	r1, r1, r0, lsl #2	@ Get word offset
	mov	r3, r2, lsl r3		@ create mask
	smp_dmb
#if __LINUX_ARM_ARCH__ >= 7 && defined(CONFIG_SMP)
	.arch_extension	mp
	ALT_SMP(W(pldw)	[r1])
	ALT_UP(W(nop))
#endif
1:	ldrex	r2, [r1]
	ands	r0, r2, r3		@ save old value of bit
	\instr	r2, r2, r3		@ toggle bit
	strex	ip, r2, [r1]
	cmp	ip, #0
	bne	1b
	smp_dmb
	cmp	r0, #0
	movne	r0, #1
2:	bx	lr
UNWIND(	.fnend		)
ENDPROC(\name		)
	.endm
#else
	.macro	bitop, name, instr
ENTRY(	\name		)
UNWIND(	.fnstart	)
	ands	ip, r1, #3
	strneb	r1, [ip]		@ assert word-aligned
	and	r2, r0, #31
	mov	r0, r0, lsr #5
	mov	r3, #1
	mov	r3, r3, lsl r2
	save_and_disable_irqs ip
	ldr	r2, [r1, r0, lsl #2]
	\instr	r2, r2, r3
	str	r2, [r1, r0, lsl #2]
	restore_irqs ip
	ret	lr
UNWIND(	.fnend		)
ENDPROC(\name		)
	.endm

/**
 * testop - implement a test_and_xxx_bit operation.
 * @instr: operational instruction
 * @store: store instruction
 *
 * Note: we can trivially conditionalise the store instruction
 * to avoid dirtying the data cache.
 */
	.macro	testop, name, instr, store
ENTRY(	\name		)
UNWIND(	.fnstart	)
	ands	ip, r1, #3
	strneb	r1, [ip]		@ assert word-aligned
	and	r3, r0, #31
	mov	r0, r0, lsr #5
	save_and_disable_irqs ip
	ldr	r2, [r1, r0, lsl #2]!
	mov	r0, #1
	tst	r2, r0, lsl r3
	\instr	r2, r2, r0, lsl r3
	\store	r2, [r1]
	moveq	r0, #0
	restore_irqs ip
	ret	lr
UNWIND(	.fnend		)
ENDPROC(\name		)
	.endm
#endif
