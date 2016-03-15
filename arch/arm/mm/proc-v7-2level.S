/*
 * arch/arm/mm/proc-v7-2level.S
 *
 * Copyright (C) 2001 Deep Blue Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define TTB_S		(1 << 1)
#define TTB_RGN_NC	(0 << 3)
#define TTB_RGN_OC_WBWA	(1 << 3)
#define TTB_RGN_OC_WT	(2 << 3)
#define TTB_RGN_OC_WB	(3 << 3)
#define TTB_NOS		(1 << 5)
#define TTB_IRGN_NC	((0 << 0) | (0 << 6))
#define TTB_IRGN_WBWA	((0 << 0) | (1 << 6))
#define TTB_IRGN_WT	((1 << 0) | (0 << 6))
#define TTB_IRGN_WB	((1 << 0) | (1 << 6))

/* PTWs cacheable, inner WB not shareable, outer WB not shareable */
#define TTB_FLAGS_UP	TTB_IRGN_WB|TTB_RGN_OC_WB
#define PMD_FLAGS_UP	PMD_SECT_WB

/* IAMROOT-12D (2016-03-12):
 * --------------------------
 * PTW : Page Table Walks
 * WBWA : Write Back Write Allocate
 */
/* PTWs cacheable, inner WBWA shareable, outer WBWA not shareable */
#define TTB_FLAGS_SMP	TTB_IRGN_WBWA|TTB_S|TTB_NOS|TTB_RGN_OC_WBWA
#define PMD_FLAGS_SMP	PMD_SECT_WBWA|PMD_SECT_S

/*
 *	cpu_v7_switch_mm(pgd_phys, tsk)
 *
 *	Set the translation table base pointer to be pgd_phys
 *
 *	- pgd_phys - physical address of new TTB
 *
 *	It is assumed that:
 *	- we are not using split page tables
 */
ENTRY(cpu_ca8_switch_mm)
#ifdef CONFIG_MMU
	mov	r2, #0
#ifdef CONFIG_ARM_ERRATA_430973
	mcr	p15, 0, r2, c7, c5, 6		@ flush BTAC/BTB
#endif
#endif
ENTRY(cpu_v7_switch_mm)
#ifdef CONFIG_MMU
	mmid	r1, r1				@ get mm->context.id
	ALT_SMP(orr	r0, r0, #TTB_FLAGS_SMP)
	ALT_UP(orr	r0, r0, #TTB_FLAGS_UP)
#ifdef CONFIG_PID_IN_CONTEXTIDR
	mrc	p15, 0, r2, c13, c0, 1		@ read current context ID
	lsr	r2, r2, #8			@ extract the PID
	bfi	r1, r2, #8, #24			@ insert into new context ID
#endif
#ifdef CONFIG_ARM_ERRATA_754322
	dsb
#endif
	mcr	p15, 0, r1, c13, c0, 1		@ set context ID
	isb
	mcr	p15, 0, r0, c2, c0, 0		@ set TTB 0
	isb
#endif
	bx	lr
ENDPROC(cpu_v7_switch_mm)
ENDPROC(cpu_ca8_switch_mm)

/*
 *	cpu_v7_set_pte_ext(ptep, pte)
 *
 *	Set a level 2 translation table entry.
 *
 *	- ptep  - pointer to level 2 translation table entry
 *		  (hardware version is stored at +2048 bytes)
 *	- pte   - PTE value to store
 *	- ext	- value for extended PTE bits
 */
ENTRY(cpu_v7_set_pte_ext)
#ifdef CONFIG_MMU
	str	r1, [r0]			@ linux version

	bic	r3, r1, #0x000003f0
	bic	r3, r3, #PTE_TYPE_MASK
	orr	r3, r3, r2
	orr	r3, r3, #PTE_EXT_AP0 | 2

	tst	r1, #1 << 4
	orrne	r3, r3, #PTE_EXT_TEX(1)

	eor	r1, r1, #L_PTE_DIRTY
	tst	r1, #L_PTE_RDONLY | L_PTE_DIRTY
	orrne	r3, r3, #PTE_EXT_APX

	tst	r1, #L_PTE_USER
	orrne	r3, r3, #PTE_EXT_AP1

	tst	r1, #L_PTE_XN
	orrne	r3, r3, #PTE_EXT_XN

	tst	r1, #L_PTE_YOUNG
	tstne	r1, #L_PTE_VALID
	eorne	r1, r1, #L_PTE_NONE
	tstne	r1, #L_PTE_NONE
	moveq	r3, #0

 ARM(	str	r3, [r0, #2048]! )
 THUMB(	add	r0, r0, #2048 )
 THUMB(	str	r3, [r0] )
	ALT_SMP(W(nop))
	ALT_UP (mcr	p15, 0, r0, c7, c10, 1)		@ flush_pte
#endif
	bx	lr
ENDPROC(cpu_v7_set_pte_ext)

	/*
	 * Memory region attributes with SCTLR.TRE=1
	 *
	 *   n = TEX[0],C,B
	 *   TR = PRRR[2n+1:2n]		- memory type
	 *   IR = NMRR[2n+1:2n]		- inner cacheable property
	 *   OR = NMRR[2n+17:2n+16]	- outer cacheable property
	 *
	 *			n	TR	IR	OR
	 *   UNCACHED		000	00
	 *   BUFFERABLE		001	10	00	00
	 *   WRITETHROUGH	010	10	10	10
	 *   WRITEBACK		011	10	11	11
	 *   reserved		110
	 *   WRITEALLOC		111	10	01	01
	 *   DEV_SHARED		100	01
	 *   DEV_NONSHARED	100	01
	 *   DEV_WC		001	10
	 *   DEV_CACHED		011	10
	 *
	 * Other attributes:
	 *
	 *   DS0 = PRRR[16] = 0		- device shareable property
	 *   DS1 = PRRR[17] = 1		- device shareable property
	 *   NS0 = PRRR[18] = 0		- normal shareable property
	 *   NS1 = PRRR[19] = 1		- normal shareable property
	 *   NOS = PRRR[24+n] = 1	- not outer shareable
	 */
.equ	PRRR,	0xff0a81a8
.equ	NMRR,	0x40e040e0

	/*
	 * Macro for setting up the TTBRx and TTBCR registers.
	 * - \ttb0 and \ttb1 updated with the corresponding flags.
	 */
	/* IAMROOT-12D (2016-03-12):
	 * --------------------------
	 * TTBCR을 0으로 세팅하여 page table boundary 사이즈를 16KB 설정한다.
	 * 왜냐하면 N(TTBCR[2:0])가 0이기 때문이다. (N이 0이면 이전 아키텍처 버
	 *  전과 호환됩니다.)
	 *
	 *   31    TTBCR       3 2 0
	 *  +-------------------+---+
	 *  | UNPREDICTABLE/SBZ | N |
	 *  +-------------------+---+
	 * 
	 * TTBR0, TTBR1
	 *  31        14-n 13-n 5 43 2 1 0
	 *  +-------------+------+--+-----+	S : Shareable
	 *  |    Base     |  SBZ |  |0|S|C|	C : inner cacheable
	 *  +-------------+------+-++-----+
	 *                         |	RGN : L1 cache이외의 cacheable상태
	 *     RGN  <--------------+	00: outer or memory - nonecacheable
	 *		01: x , 10: outer write-throuth, 11: outer write-back
	 *
	 * TTBR0와 TTBR1의 TTB_FLAG들을 설정한다.
	 * (ex : TTB_FLAGS_SMP	TTB_IRGN_WBWA|TTB_S|TTB_NOS|TTB_RGN_OC_WBWA)
	 * TTB : Translation Table Base
	 *
	 * WBWA : Write Back Write Allocate
	 * IRGN/RGN_OC : Inner/Outer Cache Region
	 * S : Shared
	 * NOS : Not Outer shareable
	 *
	 * 일반적으로 L1 캐시를 Inner, L2 캐시를 Outer.
	 *
	 * http://iamroot.org/wiki/doku.php?id=%EC%8A%A4%ED%84%B0%EB%94%94:2013.07.13_10%EC%B0%A8_arm_b
	 * Write back cache에서 Write buffer가 있을 때, write allocate 방식과
	 *   no write allocate 방식.
	 *     write allocate 방식 :
	 *         write miss가 발생했을 때, 우선 메인 메모리로부터 해당 block을
	 *         fetch하여 cache 내의 block에 allocate한다.  그러면 write hit
	 *         가 가능해지게 되는데 write hit에 의하여 cache 내 해당 block에
	 *         write를 수행하면 그 해당 block은 Modified된다.  향후, 다른
	 *         miss에 의하여 Modified 된 block을 replace하게 되는 경우, miss
	 *         로 인하여 main memory로부터 요청된 block을 cache로 읽어오는
	 *         동안, modified된 block은 write buffer에 write된다. 그리고
	 *         main memory write가 가능해질 때, write buffer는 그 modified된
	 *         block을 main memory에 write를 한다.
	 *     no write allocate 방식 :
	 *         write miss가 발생했을 때, 우선 write buffer에 write한다. 그
	 *         후, write buffer에 write된 데이터는 메인 메모리에 write를 수
	 *         행한다. (메인 메모리로부터 해당 block을 fetch하여 cache 내의
	 *         block에 allocate를 수행하지 않는다.)
	 *
	 * TTBR1을 page table address + TTB_FLAG로 설정한다.
	 */ 
	.macro	v7_ttb_setup, zero, ttbr0, ttbr1, tmp
	mcr	p15, 0, \zero, c2, c0, 2	@ TTB control register
	ALT_SMP(orr	\ttbr0, \ttbr0, #TTB_FLAGS_SMP)
	ALT_UP(orr	\ttbr0, \ttbr0, #TTB_FLAGS_UP)
	ALT_SMP(orr	\ttbr1, \ttbr1, #TTB_FLAGS_SMP)
	ALT_UP(orr	\ttbr1, \ttbr1, #TTB_FLAGS_UP)
	mcr	p15, 0, \ttbr1, c2, c0, 1	@ load TTB1
	.endm

	/*   AT
	 *  TFR   EV X F   I D LR    S
	 * .EEE ..EE PUI. .T.T 4RVI ZWRS BLDP WCAM
	 * rxxx rrxx xxx0 0101 xxxx xxxx x111 xxxx < forced
	 *   01    0 110       0011 1100 .111 1101 < we want
	 */
	.align	2
	.type	v7_crval, #object

/* IAMROOT-12D (2016-03-12):
 * --------------------------
 *	.macro	crval, clear, mmuset, ucset
 *	.word	\clear
 *	.word	\mmuset
 *	.endm
 */
	
v7_crval:
	crval	clear=0x2120c302, mmuset=0x10c03c7d, ucset=0x00c01c7c
