/*
 *  linux/arch/arm/kernel/head.S
 *
 *  Copyright (C) 1994-2002 Russell King
 *  Copyright (c) 2003 ARM Limited
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Kernel startup code for all 32-bit CPUs
 */
#include <linux/linkage.h>
#include <linux/init.h>

#include <asm/assembler.h>
#include <asm/cp15.h>
#include <asm/domain.h>
#include <asm/ptrace.h>
#include <asm/asm-offsets.h>
#include <asm/memory.h>
#include <asm/thread_info.h>
#include <asm/pgtable.h>

#if defined(CONFIG_DEBUG_LL) && !defined(CONFIG_DEBUG_SEMIHOSTING)
#include CONFIG_DEBUG_LL_INCLUDE
#endif

/*
 * swapper_pg_dir is the virtual address of the initial page table.
 * We place the page tables 16K below KERNEL_RAM_VADDR.  Therefore, we must
 * make sure that KERNEL_RAM_VADDR is correctly set.  Currently, we expect
 * the least significant 16 bits to be 0x8000, but we could probably
 * relax this restriction to KERNEL_RAM_VADDR >= PAGE_OFFSET + 0x4000.
 */
/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * KERNEL_RAM_VADDR : 커널의 가상 메모리 주소
 *  예를들어, 32bit에서 3/1GB 로 영역을 split했을 때, 1GB의 시작주소이다.
 *
 * 커널 스플릿 설정에 따라서 PAGE_OFFSET의 주소가 다른데, 
 *
 * default PHYS_OFFSET if !MMU
 * default 0x40000000 if VMSPLIT_1G
 * default 0x80000000 if VMSPLIT_2G
 * default 0xC0000000
 *
 * 라즈베리파이는 2/2GB 로 스플릿을 하기 때문에 0x80000000 로 설정된다.
 *
 * KERNEL_RAM_VADDR: 0x80000000 + 0x8000 = 0x80008000 가 실제 커널의 가상메모리
 * 시작주소가 된다
 *
 * 참고] 위의 설정을 보고 싶으면 /boot/config-리눅스버전 파일 안에 모두 기록되어
 *       있다.
 */
#define KERNEL_RAM_VADDR	(PAGE_OFFSET + TEXT_OFFSET)
#if (KERNEL_RAM_VADDR & 0xffff) != 0x8000
#error KERNEL_RAM_VADDR must start at 0xXXXX8000
#endif

/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * LPAE: 32bit에서 4GB이상 메모리를 지원하기 위해 사용했던 설정
 */
#ifdef CONFIG_ARM_LPAE
	/* LPAE requires an additional page for the PGD */
#define PG_DIR_SIZE	0x5000
#define PMD_ORDER	3
#else
/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * PG_DIR_SIZE: 32K (페이지테이블 사이즈)
 * 0x4000 = 2^15 = 2^5 * 2^10 = 32 * 1K 
 */
#define PG_DIR_SIZE	0x4000
#define PMD_ORDER	2
#endif

/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * swapper_pg_dir: 0x80004000 이다. 
 */
	.globl	swapper_pg_dir
	.equ	swapper_pg_dir, KERNEL_RAM_VADDR - PG_DIR_SIZE

/* IAMROOT-12D (2016-01-30):
 * --------------------------
 *  page table 물리주소를 구하기 위한 매크로
 *	rd = phys + TEXT_OFFSET - PG_DIR_SIZE;
 *	rd = phys + 0x8000 - 0x4000;
 *	rd = phys + 0x4000
 */
	.macro	pgtbl, rd, phys
	add	\rd, \phys, #TEXT_OFFSET
	sub	\rd, \rd, #PG_DIR_SIZE
	.endm

/*
 * Kernel startup entry point.
 * ---------------------------
 *
 * This is normally called from the decompressor code.  The requirements
 * are: MMU = off, D-cache = off, I-cache = dont care, r0 = 0,
 * r1 = machine nr, r2 = atags or dtb pointer.
 *
 * This code is mostly position independent, so if you link the kernel at
 * 0xc0008000, you call this at __pa(0xc0008000).
 *
 * See linux/arch/arm/tools/mach-types for the complete list of machine
 * numbers for r1.
 *
 * We're trying to keep crap to a minimum; DO NOT add any machine specific
 * crap here - that's what the boot loader (or in extreme, well justified
 * circumstances, zImage) is for.
 */
/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * 우리가 사용하는 라즈베리파이의 정보이다.
 * (linux/arch/arm/tools/mach-types)
 *
 * machine_is_xxx     CONFIG_xxxx      MACH_TYPE_xxx       number 
 *     bcm2709        MACH_BCM2709         BCM2709          3139
 *
 * .arm 모드로 시작
 *
 *   #define __HEAD		.section	".head.text","ax"
 *  __HEAD: 섹션을 정의
 */
	.arm

	__HEAD

/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * 여기부터 시작
 * #include <linux/linkage.h>
 * 
 * #ifndef ENTRY                                                                   
 * #define ENTRY(name) \                                                           
 *          .globl name ASM_NL \                                                    
 *          ALIGN ASM_NL \                                                          
 *          name:                                                                   
 * #endif 
 * 
 * .globl stext ;
 * ALIGN ;
 * stext:
 *
 * stext를 글로벌로 선언하고, 레이블을 생성한다.
 */
ENTRY(stext)
 ARM_BE8(setend	be )			@ ensure we are in BE8 mode

 THUMB(	adr	r9, BSYM(1f)	)	@ Kernel is always entered in ARM.
 THUMB(	bx	r9		)	@ If this is a Thumb-2 kernel,
 THUMB(	.thumb			)	@ switch to Thumb now.
 THUMB(1:			)

#ifdef CONFIG_ARM_VIRT_EXT
	bl	__hyp_stub_install
#endif

/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * r9은 스크래치 레지스터이다. 
 * 참고] 스크래치 레지스터: 고양이가 할퀴듯 아무렇게나 사용하는 레지스터
 *
 * safe_svcmode_maskall : 모든 인터럽트를 중단하고, 안전하게 svc모드로 변경된다. 
 *
 * __lookup_processor_type: cpuid를 r9으로 넘겨주고, r5에 프로세스정보를 얻는다.
 *
 * __error_p: 프로세서 타입 찾다가 에러
 * __error_lpae: LPAE 에러
 */
	@ ensure svc mode and all interrupts masked
	safe_svcmode_maskall r9

	mrc	p15, 0, r9, c0, c0		@ get processor id
	bl	__lookup_processor_type		@ r5=procinfo r9=cpuid
	movs	r10, r5				@ invalid processor (r5=0)?
 THUMB( it	eq )		@ force fixup-able long branch encoding
	beq	__error_p			@ yes, error 'p'

#ifdef CONFIG_ARM_LPAE
	mrc	p15, 0, r3, c0, c1, 4		@ read ID_MMFR0
	and	r3, r3, #0xf			@ extract VMSA support
	cmp	r3, #5				@ long-descriptor translation table format?
 THUMB( it	lo )				@ force fixup-able long branch encoding
	blo	__error_lpae			@ only classic page table format
#endif


/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * 참고: XIP - 커널을 압축하지않고 ROM FLASH에서 바로 실행되는 커널
 *
 * MMU가 Off상태이기 때문에 물리주소를 직접 이용한다.
 *
 *   r3: 2번 레이블의 물리주소가 담긴다. (예: 0x8080)
 *   r4: 2번 레이블의 가상주소 (0x80008080)
 *       (r4 = r3 - r4 = 0x8080 - 0x80008080 = 0x80000000)
 *   r8: 가상 메모리 시작 Offset (0x80000000)
 *       오버플로우 발생으로 인해 더한값이 0 이 된다.
 *       add 연산 후, 물리 메모리 Offset 이 된다.
 *       (r8 = r8 + r4 = 0x80000000 + 0x80000000 = 0x0)
 */
#ifndef CONFIG_XIP_KERNEL
	adr	r3, 2f
	ldmia	r3, {r4, r8}
	sub	r4, r3, r4			@ (PHYS_OFFSET - PAGE_OFFSET)
	add	r8, r8, r4			@ PHYS_OFFSET
#else
	ldr	r8, =PLAT_PHYS_OFFSET		@ always constant in this case
#endif

	/*
	 * r1 = machine no, r2 = atags or dtb,
	 * r8 = phys_offset, r9 = cpuid, r10 = procinfo
	 */

	/* IAMROOT-12D (2016-01-30):
	 * --------------------------
	 * atags 포인터의 유효성 검사
	 */
	bl	__vet_atags

	/* IAMROOT-12D (2016-01-30):
	 * --------------------------
	 * SMP: Symmetric Multi Processing
	 *
	 * __fixup_smp를 call 하는 목적
	 *  : UP환경에서 SMP명령어를 교체하기 위해
	 */
#ifdef CONFIG_SMP_ON_UP
	bl	__fixup_smp
#endif

	/* IAMROOT-12D (2016-02-13):
	 * --------------------------
	 * PV : Physical Virtual
	 *
	 * __fixup_pv_table를 call하는 목적
	 *  : TODO
	 */
#ifdef CONFIG_ARM_PATCH_PHYS_VIRT
	bl	__fixup_pv_table
#endif

	/* IAMROOT-12D (2016-02-13):
	 * --------------------------
	 * __create_page_talbes 를 call하는 목적
	 *  : page table들을 만들기 위해
	 */
	bl	__create_page_tables

	/*
	 * The following calls CPU specific code in a position independent
	 * manner.  See arch/arm/mm/proc-*.S for details.  r10 = base of
	 * xxx_proc_info structure selected by __lookup_processor_type
	 * above.  On return, the CPU will be ready for the MMU to be
	 * turned on, and r0 will hold the CPU control register value.
	 */
/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * r10에 담겨있는 구조체 형태
 *
 * struct proc_info_list {
 * 	unsigned int		cpu_val;
 * 	unsigned int		cpu_mask;
 * 	unsigned long		__cpu_mm_mmu_flags;	/ * used by head.S * /
 * 	unsigned long		__cpu_io_mmu_flags;	/ * used by head.S * /
 * 	unsigned long		__cpu_flush;		/ * used by head.S * /
 * 	const char		*arch_name;
 * 	const char		*elf_name;
 * 	unsigned int		elf_hwcap;
 * 	const char		*cpu_name;
 * 	struct processor	*proc;
 * 	struct cpu_tlb_fns	*tlb;
 * 	struct cpu_user_fns	*user;
 * 	struct cpu_cache_fns	*cache;
 * };
 *
 * TTBR1: 페이지테이블의 위치(주소)를 저장하는 CPU 레지스터
 * (Translation Table Base Register 1)
 * 
 */
	/* IAMROOT-12D (2016-03-05):
	 * --------------------------
	 * r13: __mmap_switched 레이블의 주소를 가져온다.
	 *      mmu가 켜진 이후 이곳으로 점프한다.
	 * 
	 * lr 레지스터에 b __enable_mmu 의 레이블의 주소를 담는다.
	 * r12 = *(r10 + 16)  // proc_info_list->cpu_flush 
	 * r10 + 16 = initfn  __v7_setup, __v7_proc_info
	 *
	 * ldr로 r12에 읽어온 값은 __v7_proc_info - __v7_setup의 값이 담긴다.
	 * 이때의 값은 음수값이다. (__v7_proc_info 주소 < __v7_setup)
	 * 이렇게 구해진 r12값에 r10을 더하게 되면 __v7_setup의 주소가 된다.
	 * 
	 * [결론] r12 = __v7_setup
	 *
	 * TODO :
	 * 	- TTBR0와 TTBR1이란 무엇인가?
	 * 	  : http://www.iamroot.org/xe/Kernel_10_ARM/176798
	 * 	- BSYM이란??
	 * 	  : http://www.iamroot.org/xe/Kernel_10_ARM/171562
	 * 	  : http://www.iamroot.org/xe/Kernel_10_ARM/173070
	 * 	- ret은 ARM 명령어??
	 * 	  : 무조건 분기하는 명령어
	 *
	 * adr 은 현재 pc값에 대한 상대 주소를 가져오고
	 * ldr 은 상수값 그대로를 가져온다.
	 * r13에는 0x80008???? 의 값이 들어 있다.
	 */
	ldr	r13, =__mmap_switched		@ address to jump to after
						@ mmu has been enabled
	adr	lr, BSYM(1f)			@ return (PIC) address
	mov	r8, r4				@ set TTBR1 to swapper_pg_dir
	ldr	r12, [r10, #PROCINFO_INITFUNC]
	add	r12, r12, r10
	ret	r12
/* IAMROOT-12D (2016-03-12):
 * --------------------------
 * 다음주 부터는 여기서 시작한다.
 */
1:	b	__enable_mmu
ENDPROC(stext)
	.ltorg
#ifndef CONFIG_XIP_KERNEL
/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * . 은 현재의 주소
 * PAGE_OFFSET은 0x80000000
 */
2:	.long	.
	.long	PAGE_OFFSET
#endif

/*
 * Setup the initial page tables.  We only setup the barest
 * amount which are required to get the kernel running, which
 * generally means mapping in the kernel code.
 *
 * r8 = phys_offset, r9 = cpuid, r10 = procinfo
 *
 * Returns:
 *  r0, r3, r5-r7 corrupted
 *  r4 = page table (see ARCH_PGD_SHIFT in asm/memory.h)
 */

	/* IAMROOT-12D (2016-02-13):
	 * --------------------------
	 * __create_page_table :
	 * 	memset(&page_table, 0, sizeof(&page_table))
	 * 	
	 * 		&page_tabe = 0x4000
	 * 		sizeof(&page_table) = 0x4000
	 * 
         * r8 = 0x0
         *
	 * pgtbl r4, r8 분석
	 *
	 * add	r4, r8, #TEXT_OFFSET
	 * sub	r4, r4, #PG_DIR_SIZE
	 *
         * rd = phys + TEXT_OFFSET - PG_DIR_SIZE;
         * rd = phys + 0x8000 - 0x4000;
         *
         * 계산 과정
	 * r4 = r8 + 0x8000 - 0x4000
	 * r4 = 0x0 + 0x8000 - 0x4000
	 * r4 = 0x4000
	 */

__create_page_tables:
	pgtbl	r4, r8				@ page table address

	/*
	 * Clear the swapper page table
	 */
	/* IAMROOT-12D (2016-02-20):
	 * --------------------------
	 * r0 = r4; ( 0x4000 페이지테이블 시작점 )
	 * r3 = 0;
	 * r6 = 0x8000 ( 페이지테이블 끝지점 )
	 * for(r0 = r4; r0<r6; r0++){
	 * 	*(r0) = 0;
	 * }
	 *
	 */
	mov	r0, r4
	mov	r3, #0
	add	r6, r0, #PG_DIR_SIZE
1:	str	r3, [r0], #4
	str	r3, [r0], #4
	str	r3, [r0], #4
	str	r3, [r0], #4
	teq	r0, r6
	bne	1b
	
	/* IAMROOT-12D (2016-02-13):
	 * --------------------------
	 * LPAE : 32bit 주소 범위를 넘어서는 address 접근을 위한
	 * address 확장 feature. raspberrypi는 스킵
	 */

#ifdef CONFIG_ARM_LPAE
	/*
	 * Build the PGD table (first level) to point to the PMD table. A PGD
	 * entry is 64-bit wide.
	 */
	mov	r0, r4
	add	r3, r4, #0x1000			@ first PMD table address
	orr	r3, r3, #3			@ PGD block type
	mov	r6, #4				@ PTRS_PER_PGD
	mov	r7, #1 << (55 - 32)		@ L_PGD_SWAPPER
1:
#ifdef CONFIG_CPU_ENDIAN_BE8
	str	r7, [r0], #4			@ set top PGD entry bits
	str	r3, [r0], #4			@ set bottom PGD entry bits
#else
	str	r3, [r0], #4			@ set bottom PGD entry bits
	str	r7, [r0], #4			@ set top PGD entry bits
#endif
	add	r3, r3, #0x1000			@ next PMD table
	subs	r6, r6, #1
	bne	1b

	add	r4, r4, #0x1000			@ point to the PMD tables
#ifdef CONFIG_CPU_ENDIAN_BE8
	add	r4, r4, #4			@ we only write the bottom word
#endif
#endif

	/* IAMROOT-12D (2016-02-13):
	 * --------------------------
	 * r10 : procinfo address
	 * r7 = proc_info->mm_mmuflags;
         *
         * #PROCINFO_MM_MMUFLAGS 를 따라가면, 아래와 같이 선언되어 있고,
         *
         * DEFINE(PROCINFO_MM_MMUFLAGS,
         *               offsetof(struct proc_info_list, __cpu_mm_mmu_flags));
         *
	 * #define DEFINE(sym, val) \
	 *        asm volatile("\n->" #sym " %0 " #val : : "i" (val))
	 *
	 * 결과적으로 다음 형태와 같은 뜻이 된다.
	 * #define PROCINFO_MM_MMUFLAGS  8
	 *
	 * r7 = 0x11c0e
	 * /arch/arm/mm/proc-v7.S 의 __v7_proc 매크로 주석 참고
         *
	 */
	ldr	r7, [r10, #PROCINFO_MM_MMUFLAGS] @ mm_mmuflags

	/*
	 * Create identity mapping to cater for __enable_mmu.
	 * This identity mapping will be removed by paging_init().
	 */
        /* IAMROOT-12D (2016-02-27):
         * --------------------------
         * r0 = __turn_mmu_on_loc 의 물리 주소값 (0x8134)
         * r3 = __turn_mmu_on_loc 의 가상 주소값 (0x80008134)
         * r5 = __turn_mmu_on 의 주소값 (0x80008240)
         * r6 = __turn_mmu_on_end 의 주소값 (0x80008260)
         *
         * r0 = r0 - r3 = 0x8134 - 0x80008134 = 0x80000000
         * r5 = r5 + r0 = 0x80008240 + 0x80000000 = 0x8240
         * r6 = r6 + r0 = 0x80008260 + 0x80000000 = 0x8260
         * r5 = r5 >> 20 = 0x0
         * r6 = r6 >> 20 = 0x0
         */
	adr	r0, __turn_mmu_on_loc
	ldmia	r0, {r3, r5, r6}
	sub	r0, r0, r3			@ virt->phys offset
	add	r5, r5, r0			@ phys __turn_mmu_on
	add	r6, r6, r0			@ phys __turn_mmu_on_end
	mov	r5, r5, lsr #SECTION_SHIFT
	mov	r6, r6, lsr #SECTION_SHIFT

        /* IAMROOT-12D (2016-02-27):
         * --------------------------
         * __turn_mmu_on 과 __turn_mmu_on_end 영역만 page table 매핑.
         *
         * r3 = r7 | (r5 << 20) = 0x11c0e | 0x0 = 0x11c0e
         * *(r4 + (r5 << 2) ) = r3
         * *(0x4000 + (0x0 << 2) ) = 0x11c0e
         * *(0x4000) = 0x11c0e
         *
         * if( 0x0 < 0x0 ){           분기문 진입하지  않음
         *      r5 = r5 + 1 = 0x1
         *      goto 1b
         * }
         *
         */
1:	orr	r3, r7, r5, lsl #SECTION_SHIFT	@ flags + kernel base
	str	r3, [r4, r5, lsl #PMD_ORDER]	@ identity mapping
	cmp	r5, r6
	addlo	r5, r5, #1			@ next section
	blo	1b


	/* IAMROOT-12D (2016-03-05):
	 * --------------------------
	 * 커널코드 시작부터 .bss 섹션의 끝까지 메모리를 매핑한다.
	 * r4: page table 시작위치 (0x4000)
	 * r4 + ( 0x80000000 >>  (20  -  2) )
	 *           2^31    / ( 2^20 * 2^-2)
	 * 즉,
	 * r4 + ( (2G / 1M) * 4 ) 로 이해하면 편하다.
	 *        2048 => 1M단위로 계산된 페이지 테이블의 Index이다.
	 *        2048 * 4 를 하게되면, 이 페이지 테이블 Index의 주소가 나온다.
	 *        그 주소는 0x2000이다.
	 *
	 * r0 = r4 + 0x2000 = 0x6000  >> 페이지테이블의 주소로 매핑한다.
	 *
	 * _end(.bss section의 끝) 0x8092378C - 1 상수값을 r6에 넣는다.
	 * ldr을 이용해서 담는 이유는, ARM 아키텍쳐에서 해당 상수를 담을수 있는 
	 * 방법이 없기때문이다. (mov로 담을 수 없는 상수)
	 *
	 * r6: 0x8092378B  => 약 9.2 MB
	 * r7: 0x11c0e (mmu_flags값)
	 * r8: 0x0 (현재 커널의 Base주소)
	 * 
	 * r6 = r4 + ( r6 >> (20 - 2) )
	 * .bss 섹션의 마지막이 매핑되는 페이지 테이블의 주소를 구한다.
	 *
	 * r6 = 0x809 (위를 계산하면)   =>  0x2024
	 * 즉, 0x6000 ~ 0x6024 까지 커널의 .bss 섹션까지의 영역과 매핑한다.
	 *
	 * 이 시점에서, r0: 0x6000, r6: 0x6024
	 * while( r0 < r6 )
	 * {
	 *    *r0 = r3;
	 *    r0++;	// 0x6000, 0x6004, 0x6008... 물리 주소가 증가한다.
	 *    r3++; 	// 실제로는 r3의 Base Address만 증가한다.
	 * }
	 */
	/*
	 * Map our RAM from the start to the end of the kernel .bss section.
	 */
	add	r0, r4, #PAGE_OFFSET >> (SECTION_SHIFT - PMD_ORDER)
	ldr	r6, =(_end - 1)
	orr	r3, r8, r7
	add	r6, r4, r6, lsr #(SECTION_SHIFT - PMD_ORDER)
1:	str	r3, [r0], #1 << PMD_ORDER
	add	r3, r3, #1 << SECTION_SHIFT
	cmp	r0, r6
	bls	1b

#ifdef CONFIG_XIP_KERNEL
	/*
	 * Map the kernel image separately as it is not located in RAM.
	 */
#define XIP_START XIP_VIRT_ADDR(CONFIG_XIP_PHYS_ADDR)
	mov	r3, pc
	mov	r3, r3, lsr #SECTION_SHIFT
	orr	r3, r7, r3, lsl #SECTION_SHIFT
	add	r0, r4,  #(XIP_START & 0xff000000) >> (SECTION_SHIFT - PMD_ORDER)
	str	r3, [r0, #((XIP_START & 0x00f00000) >> SECTION_SHIFT) << PMD_ORDER]!
	ldr	r6, =(_edata_loc - 1)
	add	r0, r0, #1 << PMD_ORDER
	add	r6, r4, r6, lsr #(SECTION_SHIFT - PMD_ORDER)
1:	cmp	r0, r6
	add	r3, r3, #1 << SECTION_SHIFT
	strls	r3, [r0], #1 << PMD_ORDER
	bls	1b
#endif

	/* IAMROOT-12D (2016-03-05):
	 * --------------------------
	 * 만약 ATAGs/DTB가 정의되어있다면 매핑한다.
	 *
	 * r2는 .bss 위에 올려놓은 DTB의 시작주소를 가지고 있다.
	 * r2 >> 20  =>  약 10이 나온다고 가정한다. (정확한 시작주소를 모른다.)
	 * 
	 * r2에 만약 atag(0x100)이 있었다면, movs에서 zero flag가 세팅될것이고,
	 * 아래의 코드는 전부 패스하게 된다.
	 *
	 * r8: Kernel의 물리 Base Address (현재 0x00)
	 *
	 * r0 = 0x00a00000
	 * r3 = 0x00a00000 + PAGE_OFFSET(0x80000000)
	 * r3 = 0x80a00000 (DTB의 주소)
	 * r7 = 0x11c0e (mmu_flag)
	 * 
	 * r3 >> ( 20 -2 )
	 *    1) r3 >> 20    => 0x80a
	 *    2) r3 << 2     => 0x2028
	 * r4: 0x4000 (PAGE_TABLE 시작주소)
	 *
	 * r3 = 0x6028 (이곳에다 DTB의 영역을 매핑할 예정이다.)
	 *
	 * r6 = 0x00a11c0e
	 * DTB의 사이즈는 최대 1MB이다. 1MB의 영역이 겹쳐있을 수 있으므로 
	 * 2MB영역을 페이지 테이블에 매핑한다.
	 *
	 * 참조: int atags_to_fdt(void *atag_list, void *fdt, int total_space)
	 */
	/*
	 * Then map boot params address in r2 if specified.
	 * We map 2 sections in case the ATAGs/DTB crosses a section boundary.
	 */
	mov	r0, r2, lsr #SECTION_SHIFT
	movs	r0, r0, lsl #SECTION_SHIFT
	subne	r3, r0, r8
	addne	r3, r3, #PAGE_OFFSET
	addne	r3, r4, r3, lsr #(SECTION_SHIFT - PMD_ORDER)
	orrne	r6, r7, r0
	strne	r6, [r3], #1 << PMD_ORDER
	addne	r6, r6, #1 << SECTION_SHIFT
	strne	r6, [r3]

#if defined(CONFIG_ARM_LPAE) && defined(CONFIG_CPU_ENDIAN_BE8)
	sub	r4, r4, #4			@ Fixup page table pointer
						@ for 64-bit descriptors
#endif

#ifdef CONFIG_DEBUG_LL
#if !defined(CONFIG_DEBUG_ICEDCC) && !defined(CONFIG_DEBUG_SEMIHOSTING)
	/* IAMROOT-12D (2016-03-05):
	 * --------------------------
	 * r7: 물리주소 0x20201000 
	 * r3: 가상주소 0xf0201000
	 *
	 * r0 = 0x7c08  (페이지 테이블의 주소)
	 * r7 = 0xc02 (io의 mmuflag 값)
	 * 
	 * r3 = 0x20200000 | 0xc02 = 0x20200c02
	 */
	/*
	 * Map in IO space for serial debugging.
	 * This allows debug messages to be output
	 * via a serial console before paging_init.
	 */
	addruart r7, r3, r0

	mov	r3, r3, lsr #SECTION_SHIFT
	mov	r3, r3, lsl #PMD_ORDER

	add	r0, r4, r3
	mov	r3, r7, lsr #SECTION_SHIFT
	ldr	r7, [r10, #PROCINFO_IO_MMUFLAGS] @ io_mmuflags
	orr	r3, r7, r3, lsl #SECTION_SHIFT
#ifdef CONFIG_ARM_LPAE
	mov	r7, #1 << (54 - 32)		@ XN
#ifdef CONFIG_CPU_ENDIAN_BE8
	str	r7, [r0], #4
	str	r3, [r0], #4
#else
	str	r3, [r0], #4
	str	r7, [r0], #4
#endif
#else
	/* IAMROOT-12D (2016-03-05):
	 * --------------------------
	 * XN : Excutable Not
	 * 시리얼포트영역이라, 실행되어서는 안되기때문에 XN을 세팅한다.
	 * 
	 * r0: 0x7c08 (페이지 테이블의 주소)
	 * r3: 0x20200c12
	 */
	orr	r3, r3, #PMD_SECT_XN
	str	r3, [r0], #4
#endif

#else /* CONFIG_DEBUG_ICEDCC || CONFIG_DEBUG_SEMIHOSTING */
	/* we don't need any serial debugging mappings */
	ldr	r7, [r10, #PROCINFO_IO_MMUFLAGS] @ io_mmuflags
#endif

#if defined(CONFIG_ARCH_NETWINDER) || defined(CONFIG_ARCH_CATS)
	/*
	 * If we're using the NetWinder or CATS, we also need to map
	 * in the 16550-type serial port for the debug messages
	 */
	add	r0, r4, #0xff000000 >> (SECTION_SHIFT - PMD_ORDER)
	orr	r3, r7, #0x7c000000
	str	r3, [r0]
#endif
#ifdef CONFIG_ARCH_RPC
	/*
	 * Map in screen at 0x02000000 & SCREEN2_BASE
	 * Similar reasons here - for debug.  This is
	 * only for Acorn RiscPC architectures.
	 */
	add	r0, r4, #0x02000000 >> (SECTION_SHIFT - PMD_ORDER)
	orr	r3, r7, #0x02000000
	str	r3, [r0]
	add	r0, r4, #0xd8000000 >> (SECTION_SHIFT - PMD_ORDER)
	str	r3, [r0]
#endif
#endif
#ifdef CONFIG_ARM_LPAE
	sub	r4, r4, #0x1000		@ point to the PGD table
	mov	r4, r4, lsr #ARCH_PGD_SHIFT
#endif
	ret	lr
ENDPROC(__create_page_tables)
	.ltorg
	.align
__turn_mmu_on_loc:
	.long	.
	.long	__turn_mmu_on
	.long	__turn_mmu_on_end

#if defined(CONFIG_SMP)
	.text
ENTRY(secondary_startup_arm)
	.arm
 THUMB(	adr	r9, BSYM(1f)	)	@ Kernel is entered in ARM.
 THUMB(	bx	r9		)	@ If this is a Thumb-2 kernel,
 THUMB(	.thumb			)	@ switch to Thumb now.
 THUMB(1:			)
ENTRY(secondary_startup)
	/*
	 * Common entry point for secondary CPUs.
	 *
	 * Ensure that we're in SVC mode, and IRQs are disabled.  Lookup
	 * the processor type - there is no need to check the machine type
	 * as it has already been validated by the primary processor.
	 */

 ARM_BE8(setend	be)				@ ensure we are in BE8 mode

#ifdef CONFIG_ARM_VIRT_EXT
	bl	__hyp_stub_install_secondary
#endif
	safe_svcmode_maskall r9

	mrc	p15, 0, r9, c0, c0		@ get processor id
	bl	__lookup_processor_type
	movs	r10, r5				@ invalid processor?
	moveq	r0, #'p'			@ yes, error 'p'
 THUMB( it	eq )		@ force fixup-able long branch encoding
	beq	__error_p

	/*
	 * Use the page tables supplied from  __cpu_up.
	 */
	adr	r4, __secondary_data
	ldmia	r4, {r5, r7, r12}		@ address to jump to after
	sub	lr, r4, r5			@ mmu has been enabled
	ldr	r4, [r7, lr]			@ get secondary_data.pgdir
	add	r7, r7, #4
	ldr	r8, [r7, lr]			@ get secondary_data.swapper_pg_dir
	adr	lr, BSYM(__enable_mmu)		@ return address
	mov	r13, r12			@ __secondary_switched address
	ldr	r12, [r10, #PROCINFO_INITFUNC]
	add	r12, r12, r10			@ initialise processor
						@ (return control reg)
	ret	r12
ENDPROC(secondary_startup)
ENDPROC(secondary_startup_arm)

	/*
	 * r6  = &secondary_data
	 */
ENTRY(__secondary_switched)
	ldr	sp, [r7, #4]			@ get secondary_data.stack
	mov	fp, #0
	b	secondary_start_kernel
ENDPROC(__secondary_switched)

	.align

	.type	__secondary_data, %object
__secondary_data:
	.long	.
	.long	secondary_data
	.long	__secondary_switched
#endif /* defined(CONFIG_SMP) */



/*
 * Setup common bits before finally enabling the MMU.  Essentially
 * this is just loading the page table pointer and domain access
 * registers.
 *
 *  r0  = cp#15 control register
 *  r1  = machine ID
 *  r2  = atags or dtb pointer
 *  r4  = page table (see ARCH_PGD_SHIFT in asm/memory.h)
 *  r9  = processor ID
 *  r13 = *virtual* address to jump to upon completion
 */
__enable_mmu:
#if defined(CONFIG_ALIGNMENT_TRAP) && __LINUX_ARM_ARCH__ < 6
	orr	r0, r0, #CR_A
#else
	bic	r0, r0, #CR_A
#endif
#ifdef CONFIG_CPU_DCACHE_DISABLE
	bic	r0, r0, #CR_C
#endif
	/* IAMROOT-12D (2016-03-17):
	 * --------------------------
	 * 분기 예측 disable
	 */
#ifdef CONFIG_CPU_BPREDICT_DISABLE
	bic	r0, r0, #CR_Z
#endif
#ifdef CONFIG_CPU_ICACHE_DISABLE
	bic	r0, r0, #CR_I
#endif
#ifndef CONFIG_ARM_LPAE
	/* IAMROOT-12D (2016-03-19):
	 * --------------------------
	 * #define domain_val(dom,type)	((type) << (2*(dom)))
	 * #define DOMAIN_KERNEL	0
	 * #define DOMAIN_TABLE	0
	 * #define DOMAIN_USER	1
	 * #define DOMAIN_IO	2
	 *
	 * #define DOMAIN_NOACCESS	0
	 * #define DOMAIN_CLIENT	1
	 * #define DOMAIN_MANAGER	1
	 *
	 * domain_val(1, 1) --> (1 << 2*1) --> 4
	 * domain_val(0, 1) --> (1 << 2*0) --> 1
	 * domain_val(0, 1) --> 1
	 * domain_val(2, 1) --> (1 << 2*2) --> 0x10
	 * 4 | 1 | 1 | 0x10 --> 0x15
	 *
	 * mov r5, 0x15
	 */
	mov	r5, #(domain_val(DOMAIN_USER, DOMAIN_MANAGER) | \
		      domain_val(DOMAIN_KERNEL, DOMAIN_MANAGER) | \
		      domain_val(DOMAIN_TABLE, DOMAIN_MANAGER) | \
		      domain_val(DOMAIN_IO, DOMAIN_CLIENT))
	mcr	p15, 0, r5, c3, c0, 0		@ load domain access register
	/* IAMROOT-12D (2016-03-17):
	 * --------------------------
	 * TTBR0 설정
	 * TTBR0 = r4(0x4000 - page table pointer)
	 */
	mcr	p15, 0, r4, c2, c0, 0		@ load page table pointer
#endif
	b	__turn_mmu_on
ENDPROC(__enable_mmu)

/*
 * Enable the MMU.  This completely changes the structure of the visible
 * memory space.  You will not be able to trace execution through this.
 * If you have an enquiry about this, *please* check the linux-arm-kernel
 * mailing list archives BEFORE sending another post to the list.
 *
 *  r0  = cp#15 control register
 *  r1  = machine ID
 *  r2  = atags or dtb pointer
 *  r9  = processor ID
 *  r13 = *virtual* address to jump to upon completion
 *
 * other registers depend on the function called upon completion
 */
/* IAMROOT-12D (2016-03-19):
* --------------------------
* MMU를 enable함.
*/
	.align	5
	.pushsection	.idmap.text, "ax"
ENTRY(__turn_mmu_on)
	mov	r0, r0
	instr_sync
	mcr	p15, 0, r0, c1, c0, 0		@ write control reg
	/* IAMROOT-12D (2016-03-19):
	 * --------------------------
	 * TODO: main id code register를 읽어왔는데 왜 사용하지 않는가?
	 */
	mrc	p15, 0, r3, c0, c0, 0		@ read id reg
	instr_sync
	mov	r3, r3
	/* IAMROOT-12D (2016-03-19):
	 * --------------------------
	 * r13 =__mmap_switched
	 */
	mov	r3, r13
	ret	r3
__turn_mmu_on_end:
ENDPROC(__turn_mmu_on)
	.popsection


/* IAMROOT-12D (2016-01-30):
 * --------------------------
 * #define ARM_CPU_PART_ARM11MPCORE	0x4100b020  
 */
#ifdef CONFIG_SMP_ON_UP
	__HEAD
__fixup_smp:
	and	r3, r9, #0x000f0000	@ architecture version
	teq	r3, #0x000f0000		@ CPU ID supported?
	bne	__fixup_smp_on_up	@ no, assume UP

	bic	r3, r9, #0x00ff0000
	bic	r3, r3, #0x0000000f	@ mask 0xff00fff0
	mov	r4, #0x41000000
	orr	r4, r4, #0x0000b000
	orr	r4, r4, #0x00000020	@ val 0x4100b020
	teq	r3, r4			@ ARM 11MPCore?
	reteq	lr			@ yes, assume SMP

	/* IAMROOT-12D (2016-01-30):
	 * --------------------------
	 * http://www.iamroot.org/xe/index.php?mid=Kernel_10_ARM&page=2&document_srl=181691
	 * 위 주소를 가면, 중간쯤에 MPIDR 관련 그림이 있다.
	 *
	 * MPDIR 구조
	 * +----------------------------------------------------------------+
	 * | 1 | U |                      ...                               |
	 * +----------------------------------------------------------------+
	 *   31  30                       ...                              0
	 *
	 *  30번째의 U가 uniprocessor를 판단할 수 있는 비트이다.
	 *    0: Multiprocessor
	 *    1: Uniprocessor
	 */
	mrc	p15, 0, r0, c0, c0, 5	@ read MPIDR
	and	r0, r0, #0xc0000000	@ multiprocessing extensions and
	teq	r0, #0x80000000		@ not part of a uniprocessor system?
	bne    __fixup_smp_on_up	@ no, assume UP

	/* IAMROOT-12D (2016-01-30):
	 * --------------------------
	 * rasberrypi2는 ARM Cortex-A7이다. Architecture는 ARMv7-A이다.
	 * 우리는 여기서 리턴될 것이다.
	 */
	@ Core indicates it is SMP. Check for Aegis SOC where a single
	@ Cortex-A9 CPU is present but SMP operations fault.
	mov	r4, #0x41000000
	orr	r4, r4, #0x0000c000
	orr	r4, r4, #0x00000090
	teq	r3, r4			@ Check for ARM Cortex-A9
	retne	lr			@ Not ARM Cortex-A9,

	/* IAMROOT-12D (2016-02-13):
	 * --------------------------
	 * 2016-02-13부터 행복하게 시작
	 * SCU (Snoop Control Unit) 이란?
	 * 	L1과 L2와의 일관성을 중재하기 위한 제어장치이다.
	 *
	 * 다만, raspberrypi는 ARM Cortex-A7이므로 아래 코드를 타지 않는다.
	 * 
	 * TODO: what is ACP?
	 * 	manage ACP accesses.
	 */
	@ If a future SoC *does* use 0x0 as the PERIPH_BASE, then the
	@ below address check will need to be #ifdef'd or equivalent
	@ for the Aegis platform.
	mrc	p15, 4, r0, c15, c0	@ get SCU base address
	teq	r0, #0x0		@ '0' on actual UP A9 hardware
	beq	__fixup_smp_on_up	@ So its an A9 UP
	ldr	r0, [r0, #4]		@ read SCU Config
ARM_BE8(rev	r0, r0)			@ byteswap if big endian
	and	r0, r0, #0x3		@ number of CPUs
	teq	r0, #0x0		@ is 1?
	retne	lr

	/* IAMROOT-12D (2016-02-13):
	 * --------------------------
	 * __fixup_smp_on_up은 UP(uni processor - core 1개) 로 설정되어 있을 때
	 * 반드시 실행되는 코드이다.
	 * 
	 * 역할 : SMP용 코드를 UP용 코드로 모두 교체한다.
	 */
__fixup_smp_on_up:
	adr	r0, 1f
	ldmia	r0, {r3 - r5}
	sub	r3, r0, r3
	add	r4, r4, r3
	add	r5, r5, r3
	b	__do_fixup_smp_on_up
ENDPROC(__fixup_smp)

	.align
1:	.word	.
	.word	__smpalt_begin
	.word	__smpalt_end

	.pushsection .data
	.globl	smp_on_up
smp_on_up:
	ALT_SMP(.long	1)
	ALT_UP(.long	0)
	.popsection
#endif

	.text
__do_fixup_smp_on_up:
	cmp	r4, r5
	reths	lr
	ldmia	r4!, {r0, r6}
 ARM(	str	r6, [r0, r3]	)
 THUMB(	add	r0, r0, r3	)
#ifdef __ARMEB__
 THUMB(	mov	r6, r6, ror #16	)	@ Convert word order for big-endian.
#endif
 THUMB(	strh	r6, [r0], #2	)	@ For Thumb-2, store as two halfwords
 THUMB(	mov	r6, r6, lsr #16	)	@ to be robust against misaligned r3.
 THUMB(	strh	r6, [r0]	)
	b	__do_fixup_smp_on_up
ENDPROC(__do_fixup_smp_on_up)

ENTRY(fixup_smp)
	stmfd	sp!, {r4 - r6, lr}
	mov	r4, r0
	add	r5, r0, r1
	mov	r3, #0
	bl	__do_fixup_smp_on_up
	ldmfd	sp!, {r4 - r6, pc}
ENDPROC(fixup_smp)

#ifdef __ARMEB__
#define LOW_OFFSET	0x4
#define HIGH_OFFSET	0x0
#else
#define LOW_OFFSET	0x0
#define HIGH_OFFSET	0x4
#endif

#ifdef CONFIG_ARM_PATCH_PHYS_VIRT

/* __fixup_pv_table - patch the stub instructions with the delta between
 * PHYS_OFFSET and PAGE_OFFSET, which is assumed to be 16MiB aligned and
 * can be expressed by an immediate shifter operand. The stub instruction
 * has a form of '(add|sub) rd, rn, #imm'.
 */
	__HEAD
__fixup_pv_table:
	adr	r0, 1f
	ldmia	r0, {r3-r7}
	mvn	ip, #0
	subs	r3, r0, r3	@ PHYS_OFFSET - PAGE_OFFSET
	add	r4, r4, r3	@ adjust table start address
	add	r5, r5, r3	@ adjust table end address
	add	r6, r6, r3	@ adjust __pv_phys_pfn_offset address
	add	r7, r7, r3	@ adjust __pv_offset address
	mov	r0, r8, lsr #PAGE_SHIFT	@ convert to PFN
	str	r0, [r6]	@ save computed PHYS_OFFSET to __pv_phys_pfn_offset
	strcc	ip, [r7, #HIGH_OFFSET]	@ save to __pv_offset high bits
	mov	r6, r3, lsr #24	@ constant for add/sub instructions
	teq	r3, r6, lsl #24 @ must be 16MiB aligned
THUMB(	it	ne		@ cross section branch )
	bne	__error
	str	r3, [r7, #LOW_OFFSET]	@ save to __pv_offset low bits
	b	__fixup_a_pv_table
ENDPROC(__fixup_pv_table)

	.align
1:	.long	.
	.long	__pv_table_begin
	.long	__pv_table_end
2:	.long	__pv_phys_pfn_offset
	.long	__pv_offset

	.text
__fixup_a_pv_table:
	adr	r0, 3f
	ldr	r6, [r0]
	add	r6, r6, r3
	ldr	r0, [r6, #HIGH_OFFSET]	@ pv_offset high word
	ldr	r6, [r6, #LOW_OFFSET]	@ pv_offset low word
	mov	r6, r6, lsr #24
	cmn	r0, #1
#ifdef CONFIG_THUMB2_KERNEL
	moveq	r0, #0x200000	@ set bit 21, mov to mvn instruction
	lsls	r6, #24
	beq	2f
	clz	r7, r6
	lsr	r6, #24
	lsl	r6, r7
	bic	r6, #0x0080
	lsrs	r7, #1
	orrcs	r6, #0x0080
	orr	r6, r6, r7, lsl #12
	orr	r6, #0x4000
	b	2f
1:	add     r7, r3
	ldrh	ip, [r7, #2]
ARM_BE8(rev16	ip, ip)
	tst	ip, #0x4000
	and	ip, #0x8f00
	orrne	ip, r6	@ mask in offset bits 31-24
	orreq	ip, r0	@ mask in offset bits 7-0
ARM_BE8(rev16	ip, ip)
	strh	ip, [r7, #2]
	bne	2f
	ldrh	ip, [r7]
ARM_BE8(rev16	ip, ip)
	bic	ip, #0x20
	orr	ip, ip, r0, lsr #16
ARM_BE8(rev16	ip, ip)
	strh	ip, [r7]
2:	cmp	r4, r5
	ldrcc	r7, [r4], #4	@ use branch for delay slot
	bcc	1b
	bx	lr
#else
#ifdef CONFIG_CPU_ENDIAN_BE8
	moveq	r0, #0x00004000	@ set bit 22, mov to mvn instruction
#else
	moveq	r0, #0x400000	@ set bit 22, mov to mvn instruction
#endif
	b	2f
1:	ldr	ip, [r7, r3]
#ifdef CONFIG_CPU_ENDIAN_BE8
	@ in BE8, we load data in BE, but instructions still in LE
	bic	ip, ip, #0xff000000
	tst	ip, #0x000f0000	@ check the rotation field
	orrne	ip, ip, r6, lsl #24 @ mask in offset bits 31-24
	biceq	ip, ip, #0x00004000 @ clear bit 22
	orreq	ip, ip, r0      @ mask in offset bits 7-0
#else
	bic	ip, ip, #0x000000ff
	tst	ip, #0xf00	@ check the rotation field
	orrne	ip, ip, r6	@ mask in offset bits 31-24
	biceq	ip, ip, #0x400000	@ clear bit 22
	orreq	ip, ip, r0	@ mask in offset bits 7-0
#endif
	str	ip, [r7, r3]
2:	cmp	r4, r5
	ldrcc	r7, [r4], #4	@ use branch for delay slot
	bcc	1b
	ret	lr
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
#endif
ENDPROC(__fixup_a_pv_table)

	.align
3:	.long __pv_offset

ENTRY(fixup_pv_table)
	stmfd	sp!, {r4 - r7, lr}
	mov	r3, #0			@ no offset
	mov	r4, r0			@ r0 = table start
	add	r5, r0, r1		@ r1 = table size
	bl	__fixup_a_pv_table
	ldmfd	sp!, {r4 - r7, pc}
ENDPROC(fixup_pv_table)

	.data
	.globl	__pv_phys_pfn_offset
	.type	__pv_phys_pfn_offset, %object
__pv_phys_pfn_offset:
	.word	0
	.size	__pv_phys_pfn_offset, . -__pv_phys_pfn_offset

	.globl	__pv_offset
	.type	__pv_offset, %object
__pv_offset:
	.quad	0
	.size	__pv_offset, . -__pv_offset
#endif

#include "head-common.S"
