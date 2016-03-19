/*
 *  linux/arch/arm/kernel/head-common.S
 *
 *  Copyright (C) 1994-2002 Russell King
 *  Copyright (c) 2003 ARM Limited
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <asm/assembler.h>

#define ATAG_CORE 0x54410001
#define ATAG_CORE_SIZE ((2*4 + 3*4) >> 2)
#define ATAG_CORE_SIZE_EMPTY ((2*4) >> 2)

#ifdef CONFIG_CPU_BIG_ENDIAN
#define OF_DT_MAGIC 0xd00dfeed
#else
#define OF_DT_MAGIC 0xedfe0dd0 /* 0xd00dfeed in big-endian */
#endif

/*
 * Exception handling.  Something went wrong and we can't proceed.  We
 * ought to tell the user, but since we don't have any guarantee that
 * we're even running on the right architecture, we do virtually nothing.
 *
 * If CONFIG_DEBUG_LL is set we try to print out something about the error
 * and hope for the best (useful if bootloader fails to pass a proper
 * machine ID for example).
 */
	__HEAD

/* Determine validity of the r2 atags pointer.  The heuristic requires
 * that the pointer be aligned, in the first 16k of physical RAM and
 * that the ATAG_CORE marker is first and present.  If CONFIG_OF_FLATTREE
 * is selected, then it will also accept a dtb pointer.  Future revisions
 * of this function may be more lenient with the physical address and
 * may also be able to move the ATAGS block if necessary.
 *
 * Returns:
 *  r2 either valid atags pointer, valid dtb pointer, or zero
 *  r5, r6 corrupted
 */
/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * r2에는 DTB 가 있다.
 *
 * 검사하는 목록
 *   1. align 검사
 *   2. DTB인지 검사 (magic값으로 체크) DTB라면 리턴
 *   3. (DTB가 아닌 경우) 
 *      atag는 첫번째에 매직값이 담겨있는게 아니라, 아래 구조체의 size가 있다.
 *
 * 	struct tag_header {                                                             
 *		__u32 size;                                                             
 * 		__u32 tag;                                                              
 * 	};
 *      union{
 * 		struct tag_core {
 * 			__u32 flags;            / * bit 0 = read-only * /
 * 			__u32 pagesize;
 * 			__u32 rootdev;                                                          
 * 		};
 * 		...
 *	};
 *      이때의 size는 word기준으로 담겨있다.
 * 
 *      #define ATAG_CORE_SIZE ((2*4 + 3*4) >> 2)
 *      2*4 의 2는 struct tag_header의 엔트리, 4는 byte이다.
 *	3*4 의 3은 tag_core의 엔트리이다. 4는 바이트를 의미한다.
 *	마지막에 >> 2 를 하는 이유는, 워드단위로 맞춰주기 위함이다.
 *
 *      이후, r5에 담은 정보는 tag_header.tag 이다. TAG_CORE와 비교한다.
 *      #define ATAG_CORE 0x54410001
 */
__vet_atags:
	tst	r2, #0x3			@ aligned?
	bne	1f

	ldr	r5, [r2, #0]
#ifdef CONFIG_OF_FLATTREE
	ldr	r6, =OF_DT_MAGIC		@ is it a DTB?
	cmp	r5, r6
	beq	2f
#endif
	cmp	r5, #ATAG_CORE_SIZE		@ is first tag ATAG_CORE?
	cmpne	r5, #ATAG_CORE_SIZE_EMPTY
	bne	1f
	ldr	r5, [r2, #4]
	ldr	r6, =ATAG_CORE
	cmp	r5, r6
	bne	1f

2:	ret	lr				@ atag/dtb pointer is ok

1:	mov	r2, #0
	ret	lr
ENDPROC(__vet_atags)

/*
 * The following fragment of code is executed with the MMU on in MMU mode,
 * and uses absolute addresses; this is not position independent.
 *
 *  r0  = cp#15 control register
 *  r1  = machine ID
 *  r2  = atags/dtb pointer
 *  r9  = processor ID
 */
/* IAMROOT-12D (2016-03-19):
* --------------------------
* 여기서 부터는 0x80000000 으로 시작되는 절대 주소번지를 사용한다.
*/
	__INIT
__mmap_switched:
	adr	r3, __mmap_switched_data

	ldmia	r3!, {r4, r5, r6, r7}
	cmp	r4, r5				@ Copy data segment if needed
1:	cmpne	r5, r6
	ldrne	fp, [r4], #4
	strne	fp, [r5], #4
	bne	1b

	/* IAMROOT-12D (2016-03-19):
	 * --------------------------
	 * fp(r11) frame pointer
	 * ip(r12) scratch register
	 * sp(r13) stack pointer
	 * lr(r14) link register
	 * pc(r15) program counter
	 *
	 * cc : carry clear : 보다 작음
	 * cs : carry set : 크거나 같음.
	 *
	 * memset(r6, 0, r7 - r6);
	 * memset(__bss_start, 0, _end - __bss_start);
	 *
	 * fp = 0; r6 = __bss_start ; r7 = _end;
	 * while(r6 < r7) {
	 * 	*r6 = 0;
	 * 	r6 += 4;
	 * }
	 */
	mov	fp, #0				@ Clear BSS (and zero fp)
1:	cmp	r6, r7
	strcc	fp, [r6],#4
	bcc	1b

 ARM(	ldmia	r3, {r4, r5, r6, r7, sp})
 THUMB(	ldmia	r3, {r4, r5, r6, r7}	)
 THUMB(	ldr	sp, [r3, #16]		)
	str	r9, [r4]			@ Save processor ID
	str	r1, [r5]			@ Save machine type
	str	r2, [r6]			@ Save atags pointer
	cmp	r7, #0
	strne	r0, [r7]			@ Save control register values
	b	start_kernel
ENDPROC(__mmap_switched)

	/* IAMROOT-12D (2016-03-19):
	 * --------------------------
	 * __data_loc 	: .data section 시작위치
	 * _sdata	: .data section 시작위치
	 * __bss_start	: .bss section 시작위치
	 * _end		: .bss section 끝위치
	 */
	.align	2
	.type	__mmap_switched_data, %object
__mmap_switched_data:
	.long	__data_loc			@ r4
	.long	_sdata				@ r5
	.long	__bss_start			@ r6
	.long	_end				@ r7
	.long	processor_id			@ r4
	.long	__machine_arch_type		@ r5
	.long	__atags_pointer			@ r6
#ifdef CONFIG_CPU_CP15
	.long	cr_alignment			@ r7
#else
	.long	0				@ r7
#endif
	.long	init_thread_union + THREAD_START_SP @ sp
	.size	__mmap_switched_data, . - __mmap_switched_data

/*
 * This provides a C-API version of __lookup_processor_type
 */
/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * linux/arch/arm/kernel/setup.c 에 함수가 선언되어있다.
 * extern struct proc_info_list *lookup_processor_type(unsigned int);
 * C에서 이용할 수 있는 API로 정의되어 있다.
 */
ENTRY(lookup_processor_type)
	stmfd	sp!, {r4 - r6, r9, lr}
	mov	r9, r0
	bl	__lookup_processor_type
	mov	r0, r5
	ldmfd	sp!, {r4 - r6, r9, pc}
ENDPROC(lookup_processor_type)

	__FINIT
	.text

/*
 * Read processor ID register (CP#15, CR0), and look up in the linker-built
 * supported processor list.  Note that we can't use the absolute addresses
 * for the __proc_info lists since we aren't running with the MMU on
 * (and therefore, we are not in the correct address space).  We have to
 * calculate the offset.
 *
 *	r9 = cpuid
 * Returns:
 *	r3, r4, r6 corrupted
 *	r5 = proc_info pointer in physical address space
 *	r9 = cpuid (preserved)
 */
/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * r4: . 
 * r5: __proc_info_begin
 * r6: __proc_info_end
 *
 * r3 - r4 => 물리 메모리에서의 Offset을 구한다.
 *   r3는 __lookup_processor_type_data의 물리주소이고,
 *   r4는 파일에서의 offset을 가지고 있다.
 *   두 값의 차이를 구하면 물리메모리에서의 위치를 계산할 수 있는 offset을
 *   구할 수 있다.
 *
 * proc_info_list에서 v7의 cpu 정보를 읽어와 구조체에 담는다.
 * 이 채워진 구조체 정보의 주소를 r5에 저장한다.
 */
__lookup_processor_type:
	adr	r3, __lookup_processor_type_data
	ldmia	r3, {r4 - r6}
	sub	r3, r3, r4			@ get offset between virt&phys
	add	r5, r5, r3			@ convert virt addresses to
	add	r6, r6, r3			@ physical address space
1:	ldmia	r5, {r3, r4}			@ value, mask
	and	r4, r4, r9			@ mask wanted bits
	teq	r3, r4
	beq	2f
	add	r5, r5, #PROC_INFO_SZ		@ sizeof(proc_info_list)
	cmp	r5, r6
	blo	1b
	mov	r5, #0				@ unknown processor
2:	ret	lr
ENDPROC(__lookup_processor_type)

/*
 * Look in <asm/procinfo.h> for information about the __proc_info structure.
 */
/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * /__proc_info_begin은 linux/arch/arm/kernel/vmlinux.lds.S에 아래와 같이 
 * 디파인되어있다.
 * #define PROC_INFO							\
 * 	. = ALIGN(4);							\
 * 	VMLINUX_SYMBOL(__proc_info_begin) = .;				\
 * 	*(.proc.info.init)						\
 * 	VMLINUX_SYMBOL(__proc_info_end) = .;
 *
 * .proc.info.init은 /linux/arch/arm/mm/proc-v7.S 을 참고하라.
 * .proc.info.init의 섹션이 정의되어 있고 프로세서 정보를 담은 레이블들이 존재
 * 한다.
 *
 * 우리는 라즈베리파이니까, 아래의 정보를 가져올 것이다.
 *
 *          .type   __v7_proc_info, #object
 *  __v7_proc_info:                                                                 
 *          .long   0x000f0000              @ Required ID value                     
 *          .long   0x000f0000              @ Mask for ID                           
 *          __v7_proc __v7_proc_info, __v7_setup                                    
 *          .size   __v7_proc_info, . - __v7_proc_info                              
 *
 * 참고] 라즈베리 파이 proc_types                                                     
 *       라즈베리파이 2 cpu ID : 0x410fc075                                      
 *       ( real_cpu_id ^ proc_types_cpu_id ) & mask = 0                          
 *       ( 0x410fc075 ^ 0x000f0000 ) & 0x000f0000 = 0                            
 */
	.align	2
	.type	__lookup_processor_type_data, %object
__lookup_processor_type_data:
	.long	.
	.long	__proc_info_begin
	.long	__proc_info_end
	.size	__lookup_processor_type_data, . - __lookup_processor_type_data

__error_lpae:
#ifdef CONFIG_DEBUG_LL
	adr	r0, str_lpae
	bl 	printascii
	b	__error
str_lpae: .asciz "\nError: Kernel with LPAE support, but CPU does not support LPAE.\n"
#else
	b	__error
#endif
	.align
ENDPROC(__error_lpae)

__error_p:
#ifdef CONFIG_DEBUG_LL
	adr	r0, str_p1
	bl	printascii
	mov	r0, r9
	bl	printhex8
	adr	r0, str_p2
	bl	printascii
	b	__error
str_p1:	.asciz	"\nError: unrecognized/unsupported processor variant (0x"
str_p2:	.asciz	").\n"
	.align
#endif
ENDPROC(__error_p)

__error:
#ifdef CONFIG_ARCH_RPC
/*
 * Turn the screen red on a error - RiscPC only.
 */
	mov	r0, #0x02000000
	mov	r3, #0x11
	orr	r3, r3, r3, lsl #8
	orr	r3, r3, r3, lsl #16
	str	r3, [r0], #4
	str	r3, [r0], #4
	str	r3, [r0], #4
	str	r3, [r0], #4
#endif
/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * Error나면 무한루프.. nop
 */
1:	mov	r0, r0
	b	1b
ENDPROC(__error)
