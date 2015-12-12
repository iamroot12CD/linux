/* ==================================================================
 * 파일명: linux/arch/arm/boot/compressed/head.S
 *
 * 요약:   커널 코드 시작 
 * 설명:   todo
 *
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-05
 * ==================================================================
 */
/* ==================================================================
 * 제 27주차 스터디
 * 날짜: 2015-11-07
 * 장소: 커널 연구회
 *
 * 팀:     Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 참석자: 곽희범 (andrew@norux.me)
 *	   임채훈 (im.fehead@gmail.com)
 *	   박종성 (@minidump)
 *	   안종찬 (ahnchan2@gmail.com)
 *	   김건용 (gykim0914@gmail.com)
 *	   권세홍 (sehongkwon2.24@gmail.com)
 *	   조훈근 (hoonycho12@gmail.com)
 *	   김민호 (8891m@naver.com)
 *	   정종채 (fynia@naver.com)
 *	   김문영 (m03y29@gmail.com)
 *
 *	   참석인원: 10 명
 * ==================================================================
 */
/*
 *  linux/arch/arm/boot/compressed/head.S
 *
 *  Copyright (C) 1996-2002 Russell King
 *  Copyright (C) 2004 Hyok S. Choi (MPU support)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/v7m.h>

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-05
 * ------------------------------------------------------------------
 *
 * a: application-cpu
 * m: micro controller
 * r: realtime
 *
 * AR_CLASS : CONFIG_CPU_V7M 일때 사용하지 않음
 * M_CLASS  : CONFIG_CPU_V7M 일때 사용
 *
 * ==================================================================
 */

 AR_CLASS(	.arch	armv7-a	)
 M_CLASS(	.arch	armv7-m	)

/*
 * Debugging stuff
 *
 * Note that these macros must not contain any code which is not
 * 100% relocatable.  Any attempt to do so will result in a crash.
 * Please select one of the following when turning on debugging.
 */
#ifdef DEBUG

#if defined(CONFIG_DEBUG_ICEDCC)

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-05
 * ------------------------------------------------------------------
 *
 * CONFIG_CPU_V6, CONFIG_CPU_V6K, CONFIG_CPU_V7 이면
 *
 * Coprocessor - CPU를 돕는 연산 장치
 *  - 예) 부동소수점 연산을 전문적으로 수행한다.
 * 
 * 매크로 loadsp 
 *  - rb, tmp는 arguments
 *  - 사용예: loadsp, 'a', 'b'
 *  - 여기서는 동작 안함
 * 
 * 매크로 writeb
 *  - mcr pc14: cp14 동작시키는 명령 
 *  - cN : Coprocessor 내부의 레지스터
 *  - todo 사용하는 부분 이후 다시 볼 것
 *
 * mcr: coprocessor 레지스터로 읽어옴
 * mcr{cond} <cp_num>, <op1>, <Rd>, <CRn>, <CRm>{,<op2>}
 *
 * mcr p14: 프로세스의 데이터를 코프로세서로 보냄
 *   - jtag에서 사용하기 위해
 *   - 그럼 jtag가 coprocessor의 레지스터에서 데이터를 읽어옴
 *
 * <puts 기준>
 * writeb, r2, r3 ; r2에 문자가 들어있음, r3에 출력할 주소가 있음
 *
 * ==================================================================
 */
#if defined(CONFIG_CPU_V6) || defined(CONFIG_CPU_V6K) || defined(CONFIG_CPU_V7)
		.macro	loadsp, rb, tmp
		.endm
		.macro	writeb, ch, rb
		mcr	p14, 0, \ch, c0, c5, 0
		.endm
#elif defined(CONFIG_CPU_XSCALE)
		.macro	loadsp, rb, tmp
		.endm
		.macro	writeb, ch, rb
		mcr	p14, 0, \ch, c8, c0, 0
		.endm
#else
		.macro	loadsp, rb, tmp
		.endm
		.macro	writeb, ch, rb
		mcr	p14, 0, \ch, c1, c0, 0
		.endm
#endif

// if CONFIG_DEBUG_ICEDCC
#else

#include CONFIG_DEBUG_LL_INCLUDE

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-05
 * ------------------------------------------------------------------
 *
 * senduart: uart로 보낸다. (출력) rs-232c (시리얼포트)
 *
 * c-style: #define writeb(ch, rb)  senduart \ch, \rb
 *
 * ==================================================================
 */
		.macro	writeb,	ch, rb
		senduart \ch, \rb
		.endm

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-05
 * ------------------------------------------------------------------
 *
 * 시리얼 포트의 메모리 주소
 * SA1100: Strong Arm 1100
 *  - SA1100기준에서 시리얼포트0: 0x80000000
 *  - USB로 사용하고 있음
 *
 * loadsp의 sp는 시리얼포트
 * 디버깅용으로 시리얼포트 출력을 위해 정의
 *
 * guess: addruart - address uart
 *
 * <putc 기준>
 * loadsp, r3, r1
 *
 * ==================================================================
 */
#if defined(CONFIG_ARCH_SA1100)
		.macro	loadsp, rb, tmp
		mov	\rb, #0x80000000	@ physical base address
#ifdef CONFIG_DEBUG_LL_SER3
		add	\rb, \rb, #0x00050000	@ Ser3
#else
		add	\rb, \rb, #0x00010000	@ Ser1
#endif //CONFIG_DEBUG_LL_SER3
		.endm
#else
		.macro	loadsp,	rb, tmp
		addruart \rb, \tmp
		.endm
#endif //CONFIG_ARCH_SA1100
#endif //CONFIG_DEBUG_LL_SER3
#endif //CONFIG_DEBUG_ICEDCC

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-05
 * ------------------------------------------------------------------
 * bl: 서브루틴 호출 분기 명령어 (함수 호출 이후 돌아오는 포인트)
 *   - lr 레지스터에 bl 다음의 pc를 저장한다.
 *   - lr 레지스터: 점프할 때, 돌아올 주소를 저장하는데 주로 사용됨
 *     (link register - r14)
 *
 * 함수호출 이후 돌아오는 역할을 수행한다.
 * ex-c) putc();
 * ==================================================================
 */
		.macro	kputc,val
		mov	r0, \val
		bl	putc
		.endm

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-05
 * ------------------------------------------------------------------
 * kphex매크로 정의
 *
 * ==================================================================
 */
		.macro	kphex,val,len
		mov	r0, \val
		mov	r1, #\len
		bl	phex
		.endm

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-12
 * ------------------------------------------------------------------
 * debug_reloc_start 매크로 정의
 * 이하는 디버그 모드일경우, 관련 정보를 출력함
 *
 * 		kputc	#'\n'
 *			;  new line하나 찍기
 *
 * 		kphex	r6, 8
 *			; r6에는 프로세서 id가 들어있는 레지스터
 *			; 그 값을 16진수로 찍기
 *			; 8은 32비트.(r6의 크기)
 *
 * #ifdef CONFIG_CPU_CP15
 * 		kputc	#':'
 * 			; ':' 찍기
 *
 * 		kphex	r7, 8
 *			; 아키텍처 id 찍기
 *
 * #ifdef CONFIG_CPU_CP15
 * 		kputc	#':'
 *			; ':' 찍기
 *
 * 		mrc	p15, 0, r0, c1, c0 
 *			; mrc(REG값을 코프로세서에서 읽어온다)
 *			; p15: 코프로세서 15(mmu나 캐시를 설정하는 역할) 
 *			; 0: 코프로세서의 명령어
 *			     이게 0일경우, 프로세서id를 가져오는 의미
 *			; r0: 저장할 레지스터(dest)
 *			; c1: 주-레지스터 lhs(코프로세서 레지스터 넘버?)
 *			      시스템 제어에 사용되는 레지스터
 *			      (mpu 또는 캐시를 활성/비활성화 하는 역할)
 *			; c0: 부-레지스터 rhs(코프로세서 레지스터 넘버?)
 *
 *			; 가정) 
 *			; c1과 c0를 0오퍼레이션으로 수행한 후, 
 *			  r0에 그 결과를 저장??
 *			; 가정2)
 *			; c1과 c0 코프로세서의 상태를 r0로 저장하는 것
 *
 *			; 참고) endian configuration and control
 *			; MRCp15, 0, r0, c1, c0  ; r0 := CP15 register 1
 *
 *			; 참고)
 *			; BAND의 "Summary of CP15 instructions"를 참고
 *			; 샘플의 주석: read control register cofiguration data
 *
 *			; 참고)
 *			; mcr p14: 프로세스의 데이터를 코프로세서로 보냄
 *			     - jtag에서 사용하기 위해
 *			     - 그럼 jtag가 coprocessor의 레지스터에서 
 *			       데이터를 읽어옴
 * 
 *			; 결국, 코프로세서의 값을 읽어와서 찍는다
 *
 *		 kphex	r0, 8
 *			; 컨트롤 레지스터 값이 r0에 들어와서 16진수로 찍는다.
 *
 *		 kputc	#'\n'
 *			; 커널의 시작주소, 끝주소, 실행주소르 찍는다
 *
 *		 kphex	r5, 8
 *		 kputc	#'-'
 *		 kphex	r9, 8
 *		 kputc	#'>'
 *		 kphex	r4, 8
 *		 kputc	#'\n'
 *
 *			; 질문) 주레지스터와 부레지스터가 어떤 의미/구분이 있냐?
 *
 * ==================================================================
 */
			.macro	debug_reloc_start
#ifdef DEBUG
		kputc	#'\n'
		kphex	r6, 8		/* processor id */
		kputc	#':'
		kphex	r7, 8		/* architecture id */
#ifdef CONFIG_CPU_CP15
		kputc	#':'
		mrc	p15, 0, r0, c1, c0
		kphex	r0, 8		/* control reg */
#endif
		kputc	#'\n'
		kphex	r5, 8		/* decompressed kernel start */
		kputc	#'-'
		kphex	r9, 8		/* decompressed kernel end  */
		kputc	#'>'
		kphex	r4, 8		/* kernel execution address */
		kputc	#'\n'
#endif
		.endm

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-12
 * ------------------------------------------------------------------
 *
 * 		kphex	r5, 8
 *			; 커널의 끝(?)을 16진수로찍기(end of kernel)
 *
 * 		mov	r0, r4
 *			; r4: zImage(compressed kernel) 압축이 풀릴 시작 위치를
 *			      r0로 복사
 *
 * 		bl	memdump
 *			; dump 256 bytes at start of kernel
 *
 * ==================================================================
 */

 .macro	debug_reloc_end
#ifdef DEBUG
		kphex	r5, 8		/* end of kernel */
		kputc	#'\n'
		mov	r0, r4
		bl	memdump		/* dump 256 bytes at start of kernel */
#endif
		.endm

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-19
 * ------------------------------------------------------------------
 * 
 *		.section ".start", #alloc, #execinstr
 *			; start section을 정의(start 분기 시작은 안함). 
 *			; 섹션 명칭 다음에 ,(콤마)로 구분하여 추가로 섹션의 
 *			  특성? (할당, 실행)을 정의가 가능.
 *
 *		.align
 *			; 기본 4byte 기준으로 정렬. 
 *
 * 		AR_CLASS(	.arm	)
 *			; Application/Realtime CPU일 때 .arm mode를 적용. 
 *
 *			; This performs the same action as .code 32.
 *			; .code[16/32] The value 16 selects Thumb, with the 
 *			  value 32 selecting ARM.
 *
 *			; [질문] .section 에 ".start"를 지정하는 것과 
 *			         start: label의 차이?
 *			; .start section을 실행가능하고 메모리 할당가능한 섹션에
 *			  위치시켜라.
 *			; start: 는 일반적인 브랜치 label.
 *
 *		start:
 *			; start label 시작.
 *
 *		.type	start,#function
 *			; start를 function으로 지정. 
 *
 *		.rept	7
 *			; 7번 반복매크로.
 *
 *		mov	r0, r0
 *			; r0 = r0
 *
 *			; 즉, r0 = r0를 7번 반복.
 *			; 7번 반복한 이유: 처음에는 사람들이 pipeline을 
 *			                   비워두기 위한 것이라고 추측.
 *			; 일반적인 start 주소는 0xnnnn8000에서 시작하는데
 *			  특정 부트로더에서는 0xnnnn8020에서 시작하므로 
 *			  0xnnnn8000으로 맞춰주기 위해서.
 *			; iamroot 10차팀 9주차 정리 link에 내용 나와 있음
 *			; (http://neuromancer.kr/t/009-2013-06-22-arm-system-developers-guide-ch14/35)
 *
 *		.endr
 * ARM(		mov	r0, r0		)
 *			; ARM mode에서 1번더 r0 = r0
 *
 * ARM(		b	1f		)
 *			; 브랜치 1f
 *
 * THUMB(	adr	r12, BSYM(1f)	)
 *			; THUMB mode에서 r12 = 1f + 1의 주소 
 *			; #define BSYM(sym)    sym + 1 
 *			  참조 => (/arch/arm/include/asm/unified.h)
 *
 *			; 왜 1을 더하는지에 대한 대답 아래 2개 주소사이트 참조
 *			; http://stackoverflow.com/questions/9368360/arm-thumb-using-bx-in-thumb-code-to-call-a-thumb-function-or-to-jump-to-a-thu
 *			; http://recipes.egloos.com/5032032
 *
 *			; +1을 더하는 이유는 THUMB 모드와 ARM 모드를 구분.
 *			; (CPSR의 T bit가 1 이면 THUMB, 0이면 ARM) 이므로 
 *			  최하위비트를 1로 만들어 THUMB 모드로 사용.
 *			; T bit를 1과 0으로 자동 setting해 주는 명령어 
 *			  bx(branch with exchange)
 *			; bx라는 명령어에 jump할 주소를 주게 되는데, 
 *			  그 주소가 홀수이면 T bit를 1로, 즉 Thumb mode로, 
 *			  그 주소가 짝이면 T bit를 0
 *
 * THUMB(	bx	r12		)
 *			; 위 BSYM을 통해 주소+1을 하여 홀수주소를 만들기 때문에
 *			  bx명령을 통해 CPSR의 T bit를 자동으로 1로 셋팅하여
 *			  THUMB 모드로 동작.
 *
 *		.word	_magic_sig	@ Magic numbers to help the loader
 *			; word단위로 _magic_sig를 주소저장: 0x016f2818 
 *
 *		.word	_magic_start	@ absolute load/run zImage address
 *			; word단위로 _maigc_start를 주소저장: zImage는 
 *			  compressed kernel의 start 주소.
 *
 *		.word	_magic_end	@ zImage end address
 *			; word단위로 _maigc_end를 주소저장: zImage는 compressed
 *			  kernel의 end 주소.
 *
 *		.word	0x04030201	@ endianness flag
 *			; word단위로 endiannesflag를 저장: 이 단계에서 
 *			  big/little endian을 알수 없고 flag가 메모리에 올라갔을
 *			  때 첫 바이트가 0x01이면 little endian, 0x04이면 big 
 *			  endian로 검사해야 알 수 있다.
 *
 * THUMB(	.thumb			)
 *			; THUMB mode를 사용.
 *
 * 1:
 * ARM_BE8(	setend	be		)	@ go BE8 if compiled for BE8
 *			; CONFIG_CPU_ENDIAN_BE8 일 때 setend be 명령이 실행되어
 *			  big endian으로 사용하도록 설정.
 *			; 기본적으로 ARM CPU는 little endian을 사용.
 *
 * AR_CLASS(	mrs	r9, cpsr	)
 *			; cpsr의 내용을 Arm register(r9)에 저장. 
 *
 * #ifdef CONFIG_ARM_VIRT_EXT
 *		bl	__hyp_stub_install	@ get into SVC mode, reversibly
 *			; to do 가상화
 *
 * #endif
 *		mov	r7, r1			@ save architecture ID
 *			; r7 = r1
 *
 *		mov	r8, r2			@ save atags pointer
 *			; bootloader로부터 atag 위치를 전달받음.
 *			; r8 = r2
 *
 *			; [질문] atags pointer는 무엇인가요?
 *			; => bootloader에서 linux kernel로 설정을 전달하는 
 *			     pointer.
 *			; [질문] r1, r2는 어디서 나온 것인가요?
 *			; => bootloader에서 linux kernel로 전달된다.
 *
 * #ifndef CONFIG_CPU_V7M
 *		mrs	r2, cpsr		@ get current mode
 *			; cpsr의 내용을 r2에 저장.
 *
 *		tst	r2, #3			@ not user?
 *			; cpsr의 하위 2bit(mode bit)를 비교하여 user mode인지 
 *			  체크. (tst :: test)
 *
 *		bne	not_angel
 *			; cpsr의 하위 2bit가 11이 아니면(user mode가 아니면
 *			  인터럽트 제외) not_angel로 브랜치.
 *
 *			; [질문] angel이 무엇인가?
 *			; => 부트로더 종류 중 하나로 angel 부트로더를 말함.
 *
 *		mov	r0, #0x17		@ angel_SWIreason_EnterSVC
 *			; r0 = 0x17
 *
 *			; [질문] # 또는 operand 2는 barrel shifter를 통해 
 *			  전달되는데 barrel shifter는 어떤 동작을 하나?
 *
 * 			; [질문] 왜 0x17값을 넣는가?
 *
 * ARM(		swi	0x123456	)	@ angel_SWI_ARM
 * THUMB(	svc	0xab		)	@ angel_SWI_THUMB
 *			; 링크 참조: 
 *			  1. http://trace32.com/wiki/index.php/SemiHosting_%EA%B3%BC_%EA%B7%B8_%ED%99%9C%EC%9A%A9
 *			  2. http://egloos.zum.com/recipes/v/5037342
 *			  3. http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0282b/BGBJHIEA.html
 *
 * not_angel:
 *		safe_svcmode_maskall r0	
 *			; Helper macro to enter SVC mode cleanly and mask 
 *			  interrupts.
 *		msr	spsr_cxsf, r9		@ Save the CPU boot mode in
 *						@ SPSR
 * #endif
 *
 * ==================================================================
 */

		.section ".start", #alloc, #execinstr
/*
 * sort out different calling conventions
 */
		.align
		/*
		 * Always enter in ARM state for CPUs that support the ARM ISA.
		 * As of today (2014) that's exactly the members of the A and R
		 * classes.
		 */
 AR_CLASS(	.arm	)
start:
		.type	start,#function
		.rept	7
		mov	r0, r0
		.endr
   ARM(		mov	r0, r0		)
   ARM(		b	1f		)
 THUMB(		adr	r12, BSYM(1f)	)
 THUMB(		bx	r12		)

		.word	_magic_sig	@ Magic numbers to help the loader
		.word	_magic_start	@ absolute load/run zImage address
		.word	_magic_end	@ zImage end address
		.word	0x04030201	@ endianness flag

 THUMB(		.thumb			)
1:
 ARM_BE8(	setend	be		)	@ go BE8 if compiled for BE8
 AR_CLASS(	mrs	r9, cpsr	)
#ifdef CONFIG_ARM_VIRT_EXT
		bl	__hyp_stub_install	@ get into SVC mode, reversibly
#endif
		mov	r7, r1			@ save architecture ID
		mov	r8, r2			@ save atags pointer

#ifndef CONFIG_CPU_V7M
		/*
		 * Booting from Angel - need to enter SVC mode and disable
		 * FIQs/IRQs (numeric definitions from angel arm.h source).
		 * We only do this if we were in user mode on entry.
		 */
		mrs	r2, cpsr		@ get current mode
		tst	r2, #3			@ not user?
		bne	not_angel
		mov	r0, #0x17		@ angel_SWIreason_EnterSVC
 ARM(		swi	0x123456	)	@ angel_SWI_ARM
 THUMB(		svc	0xab		)	@ angel_SWI_THUMB
not_angel:
		safe_svcmode_maskall r0
		msr	spsr_cxsf, r9		@ Save the CPU boot mode in
						@ SPSR
#endif
		/*
		 * Note that some cache flushing and other stuff may
		 * be needed here - is there an Angel SWI call for this?
		 */

		/*
		 * some architecture specific code can be inserted
		 * by the linker here, but it should preserve r7, r8, and r9.
		 */

		.text

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-10-03
 * ------------------------------------------------------------------
 *
 * 참고 URL : http://iamroot.org/wiki/doku.php?id=kernel_original_12_a:%EB%B6%80%ED%8A%B8_%EA%B4%80%EB%A0%A8#zimage%EC%9D%98_%EC%8B%9C%EC%9E%91_%EC%9C%84%EC%B9%98_offset
 * #ifdef CONFIG_AUTO_ZRELADDR 
 *			; Auto calculation of the decompressed kernel image 
 *			  address
 *
 *      		; 자동 재배치 소스
 *      		; http://stackcanary.com/?p=246
 *
 *		    / *
 *		      * <원문 주석>
 *		      * Set up a page table only if it won't overwrite ourself.
 *		      * That means r4 < pc || r4 - 16k page directory > &_end.
 *		      * Given that r4 > &_end is most unfrequent, we add a rough
 *		      * additional 1MB of room for a possible appended DTB.
 *		      * /
 *
 *			<번역>
 *			중복된 영역이 아니라면 page table를 설정한다.
 *			r4가 pc 보다 작거나  r4 - 16k page directory 가 
 *			&_end보다 크다는 뜻이다. r4가 &_end보다 큰일은 거의 
 *			없으며, DTB 추가가 가능하도록 1MB 정도의 빈공간(room)을
 *			추가한다.
 *
 * 		mov	r4, pc	
 *       	and	r4, r4, #0xf8000000
 *			; r4 = pc & 0xf8000000
 *
 *       	add	r4, r4, #TEXT_OFFSET
 *			; r4 += TEXT_OFFSET
 *      		; TEXT_OFFSET : 0x00008000
 *      		; arch/arm/Makefile
 *      		; TEXT_OFFSET := $(textofs-y)
 *
 * #else
 *		ldr	r4, =zreladdr
 *			; arch/arm/boot/compressed/Makefile
 *      		;	LDFLAGS_vmlinux += --defsym zreladdr=$(ZRELADDR)
 *      		; arch/arm/boot/Makefile
 *      		;	ZRELADDR := $(zreladdr-y)
 *      		; arch/arm/mach-bcm2709/Makefile.boot
 *      		;	zreladdr-y   := 0x00008000
 *
 * #endif // CONFIG_AUTO_ZRELADDR
 *
 *      	mov	r0, pc
 *			; r0 = pc
 *
 *      	cmp	r0, r4
 *			; cmp : r0 - r4 의 결과값을 cspr에 저장
 *
 *      		; Carry flag, Zero flag
 *      		;   X  |  Y  | X-Y |  C  |  Z
 *      		; -----+-----+-----+-----+-----
 *      		;   0  |  0  |  0  |  1  |  1
 *      		;   0  |  1  |  -1 |  0  |  0
 *      		;   1  |  0  |  1  |  1  |  0
 *      		;   1  |  1  |  0  |  1  |  1
 *      
 *      		; ldr :  load address
 *      		; cc : 조건문 carry clear 
 *      		; ex1)	r0 = 0000	r4 = 0001
 *      		;	r0 - r4 = -1(1111) 빌림 발생(Carray Clear)
 *      		;		0000 + 1111
 *      		; ex2)	r0 = 0001	r4 = 0000
 *      		;	r0 - r4 = 1(0001)빌림 발생 하지 않음(Carray Set)
 *      							0001 + 0000
 *      		; furyheimdall.tistory.com/30
 *
 *      	ldrcc	r0, LC0+32
 *		addcc	r0, r0, pc
 *		cmpcc	r4, r0		
 *		orrcc	r4, r4, #1	
 *		blcs	cache_on	
 *			; <C Style Code>
 *			; if(r0 < r4) {	
 *			; 	r0 = (LC0+32)
 *			;	r0 += pc
 *			;	if(r4 < r0)
 *			;		r4 |= 1; // remember we skipped cache_on
 *			;	else
 *			;		cache_on();
 *			; } else {
 *			;	cache_on();
 *			; }
 * 
 * ==================================================================
 */

#ifdef CONFIG_AUTO_ZRELADDR

/*
 * <원문>
 * ZRELADDR is the physical address where the decompressed kernel
 * image will be placed. If AUTO_ZRELADDR is selected, the address
 * will be determined at run-time by masking the current IP with
 * 0xf8000000. This assumes the zImage being placed in the first 128MB
 * from start of memory.
 *
 * <번역>
 * ZRELADDR는 압축풀린 커널이 놓여질 물리 주소이다. AUTO_ZRELADDR 가
 * 설정되어 있으면, 주소는 실시간에 현재 IP(pc ?)에 0xf8000000과 마스킹
 * 해서 결정된다. zImage가 0MB ~ 128MB 안에 놓여진다고 가정한다.
 * http://iamroot.org/wiki/doku.php?id=kernel_original_12_a:%EB%B6%80%ED%8A%B8_%EA%B4%80%EB%A0%A8#%EC%9D%BC%EB%B0%98_%EC%9D%B4%EB%AF%B8%EC%A7%80_vs_config_zboot_rom
 *
 */
		/*
		 * <원문 주석>
		 * Find the start of physical memory.  As we are executing
		 * without the MMU on, we are in the physical address space.
		 * We just need to get rid of any offset by aligning the
		 * address.
		 *
		 * This alignment is a balance between the requirements of
		 * different platforms - we have chosen 128MB to allow
		 * platforms which align the start of their physical memory
		 * to 128MB to use this feature, while allowing the zImage
		 * to be placed within the first 128MB of memory on other
		 * platforms.  Increasing the alignment means we place
		 * stricter alignment requirements on the start of physical
		 * memory, but relaxing it means that we break people who
		 * are already placing their zImage in (eg) the top 64MB
		 * of this range.
		 */
		/*
		 * <번역>
		 * 물리메모리의 시작 주소를 찾는다. MMU를 사용중이지 않기 때문에
		 * 현재 물리 주소를 다루고 있다.
		 * 주소정렬에 사용한 offset들을 제거할 필요가 있다.
		 *
		 * 위 정렬은 다른 플렛폼간의 호환을 위한 것이다 - 초기 128MB
  		 */
		mov	r4, pc
		and	r4, r4, #0xf8000000
		/* Determine final kernel image address. */
		add	r4, r4, #TEXT_OFFSET
#else
		ldr	r4, =zreladdr
#endif

		/*
		 * Set up a page table only if it won't overwrite ourself.
		 * That means r4 < pc || r4 - 16k page directory > &_end.
		 * Given that r4 > &_end is most unfrequent, we add a rough
		 * additional 1MB of room for a possible appended DTB.
		 */
		mov	r0, pc
		cmp	r0, r4
		ldrcc	r0, LC0+32
		addcc	r0, r0, pc
		cmpcc	r4, r0
		orrcc	r4, r4, #1		@ remember we skipped cache_on
		blcs	cache_on

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-10-03
 * ------------------------------------------------------------------
 * 날짜: 2015-10-31
 * 참석자: 곽희범 (andrew@norux.me)
 *	   임채훈 (im.fehead@gmail.com)
 *	   김영준 (iam.yeongjunkim@gmail.com)
 *	   박병배 (i.billpark@gmail.com)
 *	   안종찬 (ahnchan2@gmail.com)
 *	   권세홍 (sehongkwon2.24@gmail.com)
 *	   김민호 (8891m@naver.com)
 *	   곽승준 (rialtoksj@yahoo.com)
 *
 * 	   참석인원: 8 명
 *
 * IAMROOT-12A:
 * ------------
 * r0에 현재 실행되는 상태의 LC0의 주소 값이 담기고,
 * r1은 처음 이미지가 만들어질 때 당시의 주소 값(0을 기준으로)이 담겨 있고
 * 이 둘의 차이 값이 r0(delta) 값이 된다.이 델타 값으로 Image의 위치를
 * 가리키는 레지스터들을 보정하는데 사용한다.
 *
 * 코드가 재배치가 이루어진 후 이곳으로 재진입하게 되면 또 한 번 delta가
 * 바뀌고 이 때에도 바뀐 주소로인해 관련 레지스터들의 주소를 교정한다.
 *
 * -r6 = _edata + delta
 * -r10 = _input_data_end-4 + delta
 *
 *  _input_data_end-4 = 압축된 커널(piggy)의 마지막 어드레스를 의미함.
 *                      데이터 내용은 압축된 커널의 길이.
 *
 * (참고URL) r0와 r1값의 차이 - 메모리 주소와 offset 값
 * http://www.iamroot.org/xe/Kernel_10_ARM/176234 
 *
 *restart:	adr	r0, LC0	
 *			; r0 = &LC0
 *
 *		ldmia	r0, {r1, r2, r3, r6, r10, r11, r12}
 *			; ldmia r0 주소에 있는 값을 차례로 다음 레지스터들에
 *			; 값을 저장한다. 만약, r0 뒤에 ! 기호가 있는경우는
 *			; r0값을 증가시켜 저장한다. ia : Increase After
 *			; ldmia (load multiple increament after)
 *			
 *			; (컴파일하여 확인해본 각 레지스터 값)
 *			; r1 = LC0 (0x00000240 메모리주소)
 *			; r2 = __bss_start (0x003d8a60 파일 offset값)
 *			; r3 = _end (0x003d8a76 파일 offset값)
 *			; r6 = _edata; (0x003d8a60 파일 offset값)
 *			; r10 = input_data_end - 4 (0x003d8a22 파일 offset값)
 *			; r11 = _got_start (0x003d8a34 파일 offset값)
 *			; r12(ip) = _got_end (0x003d8a5c 파일 offset값)
 *
 *		ldr	sp, [r0, #28]
 *			; sp = .L_user_stack_end (0x003d9a80 파일 offset값)
 *
 *			<원문 주석>
 *			/ *
 *		 	  * We might be running at a different address.  We need
 *		 	  * to fix up various pointers.
 *		 	  * /	
 *			<번역>
 *		 	다른 주소에서 실행되고 있으므로 다양한 포인터등을 고칠
 *			필요가 있다.
 *
 *		sub	r0, r0, r1		@ calculate the delta offset
 *			; r0 = r0 - r1 : &LC0(메모리 주소) - &LC0(파일 offset값)
 *			; Q) delta 란 무엇인가?  A) 0번지부터 이미지 최초위치
 *			;   12 A팀 부팅관련 문서 참고
 *
 *		add	r6, r6, r0		@ _edata
 *			; r6 = _edata + delta
 *
 *		add	r10, r10, r0		@ inflated kernel size location
 *			; r10 = input_data_end - 4 + delta
 *
 *	     (참조: 커널 ELF 헤더 정보)
 *	     Section Headers:
 *	     [Nr] Name        Type       Addr     Off    Size   ES Flg Lk Inf Al
 *	     [ 0]             NULL       00000000 000000 000000 00      0   0  0
 *	     [ 1] .text       PROGBITS   00000000 008000 003a94 00  AX  0   0 32
 *	     [ 2] .rodata     PROGBITS   00003a94 00ba94 000cc4 00   A  0   0  4
 *	     [ 3] .piggydata  PROGBITS   00004758 00c758 3d42ce 00   A  0   0  1
 *	     [ 4] .got.plt    PROGBITS   003d8a28 3e0a28 00000c 04  WA  0   0  4
 *	     [ 5] .got        PROGBITS   003d8a34 3e0a34 000028 00  WA  0   0  4
 *	     [ 6] .pad        PROGBITS   003d8a5c 3e0a5c 000004 00  WA  0   0  1
 *	     [ 7] .bss        NOBITS     003d8a60 3e0a60 00001c 00  WA  0   0  4
 *	     [ 8] .stack      NOBITS     003d8a80 3e0a60 001000 00  WA  0   0  1
 *
 * 		/ *
 *		* The kernel build system appends the size of the
 *		* decompressed kernel at the end of the compressed data
 *		* in little-endian form.
 *		* /
 *
 *			; r10 이 가리키고 있는 메모리의 값을 big-endian 에서 
 *			  little-endian 포맷으로 변경하는 작업
 *			  (참고URL)
 *			  http://www.iamroot.org/xe/Kernel_10_ARM/176234
 *
 *		ldrb	r9, [r10, #0]
 *		ldrb	lr, [r10, #1]
 *		orr	r9, r9, lr, lsl #8
 *		ldrb	lr, [r10, #2]
 *		ldrb	r10, [r10, #3]
 *		orr	r9, r9, lr, lsl #16
 *		orr	r9, r9, r10, lsl #24
 *
 * #ifndef CONFIG_ZBOOT_ROM 
 *			; zImage(uImage, bzImage 포함)는 보통 SDRAM에 로딩하고
 *			; 그 후 실행되지만 그렇지 않고 바로 ROM/Flash에서
 *			; 동작시킬 수 있는데 이 때 커널 빌드 시
 *			; CONFIG_ZBOOT_ROM 옵션을 사용한다.
 *			; #ifndef 이므로 설정되어있지 않으면 이하 2라인이 
 *			  수행된다.
 *
 *		/ * malloc space is above the relocated stack (64k max) * /
 *
 *			; 압축 커널 이미지의 이동을 위해, Stack Pointer 위에
 *			; 64k 크기의 버퍼를 할당하여 여유공간을 두기 위함.
 *
 *		add	sp, sp, r0
 *		add	r10, sp, #0x10000
 * #else
 *		        
 *		/ *
 *		  * With ZBOOT_ROM the bss/stack is non relocatable,
 *	          * but someone could still run this code from RAM,
 *		  * in which case our reference is _edata.
 *		  * /
 *		mov	r10, r6
 * #endif
 *
 *		mov	r5, #0			@ init dtb size to 0
 *
 * #ifdef CONFIG_ARM_APPENDED_DTB
 * 		ldr	lr, [r6, #0]
 *			; lr = *(r6 + 0)
 *			; lr에는 DTB의 매직넘버가 저장된다.
 *
 * #ifndef __ARMEB__
 *		ldr	r1, =0xedfe0dd0		@ sig is 0xd00dfeed big endian
 *			; little endian 사용시
 *
 * #else
 *		ldr	r1, =0xd00dfeed
 *			; big endian 사용시
 *
 * #endif
 *		cmp	lr, r1 
 *			; if(lr != r1) goto dtb_check_done;
 *
 *		bne	dtb_check_done		@ not found 
 * 			; dtb_check_done은 dtb가 없는 경우이고, 아무작업도 하지
 *			  않음.
 *
 *
 * #ifdef CONFIG_ARM_ATAG_DTB_COMPAT
 * 	      / *
 * 		* OK... Let's do some funky business here.
 * 		* If we do have a DTB appended to zImage, and we do have
 * 		* an ATAG list around, we want the later to be translated
 * 		* and folded into the former here. No GOT fixup has occurred
 * 		* yet, but none of the code we're about to call uses any
 * 		* global variable.
 * 		* /
 * 
 * 	      / * Get the initial DTB size * /
 *
 *			; (참고URL) 현재까지의 메모리 맵 - 대박자료
 *			; http://www.iamroot.org/xe/Kernel_10_ARM/178300
 *
 * < 2015-11-07 시작예정 >
 *
 * 		ldr	r5, [r6, #4]
 *			;r5에 dtb의 첫번째 data값을 넣는다. 		
 *                      ;r6의 +4값은 dtb의 사이즈를 의미하는것으로 생각됨.
 * #ifndef __ARMEB__   
 *			;big endian이 아닐때 우리는 __ARMEB__가 정의되어 있지
 *                      ;않으므로 여기를 타게 됨
 * 		* convert to little endian *
 * 		eor	r1, r5, r5, ror #16
 *			;r1=(r5)^(r5>>16) ;rotate는 
 * 		bic	r1, r1, #0x00ff0000
 *			;r1 = r1 & !(0x00ff0000)
 * 		mov	r5, r5, ror #8
 *			;r5 = r5 >> 8
 * 		eor	r5, r5, r1, lsr #8
 *			;r5 = r5 ^ (r1 >> 8)
 *			;여기까지 r5의 빅엔디안 -> 리틀엔디안 수정작업
 *                      ;ex) r5 = 0x12345678 -> r5 = 0x78563412
 * #endif
 * 		* 50% DTB growth should be good enough *
 * 		add	r5, r5, r5, lsr #1
 *			;r5 = r5 + (r5 >> 1)  
 * 		* preserve 64-bit alignment *
 * 		add	r5, r5, #7
 *			;r5 = r5 + 7
 * 		bic	r5, r5, #7
 *			;r5 = r5 & !(0b...111)
 *                      ;8올림 연산을 수행하여 값이 8의 배수가 되게함
 * 		* clamp to 32KB min and 1MB max *
 * 		cmp	r5, #(1 << 15)
 *			;r5와 32k를 비교
 * 		movlo	r5, #(1 << 15)
 *			;r5 = 32k lo=미만
 * 		cmp	r5, #(1 << 20)
 *			; r5와 1MB를 비교
 * 		movhi	r5, #(1 << 20)
 *			;r5 = 1Mb 초과 hi=초과
 * 		* temporarily relocate the stack past the DTB work space *
 * 		add	sp, sp, r5
 *			;sp = sp+r5 DTB사이즈만큼 그위에 스택위치도 수정되야함
 *       	stmfd	sp!, {r0-r3, ip, lr}
 *
 *			;http://recipes.egloos.com/5059742 //stack명령 참고
 *
 *			;stmfd 
 *			;ex) sp = 0x5400, r0=1, r1= 2, r2=3, r3=4, ip=7, lr=8
 *                          stm명령 수행후 sp값은 아래와 같이 됩니다.
 *    	                       바뀐 sp=> | r0
 *		 			 | r1
 *					 | r2
 *					 | r3
 *					 | ip
 *					 | lr
 *		      기존 sp = 0x5400   |
 *
 *       	mov	r0, r8
 *			;r8은 기존에 atag pointer가 저장되어 있음.
 *       	mov	r1, r6
 *			;r6은 DTB를 가리키는  edata값
 *       	mov	r2, r5
 *			;r5는 DTB의 total space 
 *       	bl	atags_to_fdt
 *			;c함수로 점프
 *			; 2015-12-05 분석 완료.
 *			; atgs의 내용을 fdt 로 변환하여 내용을 채운다.
 *              ;int atags_to_fdt(void *atag_list, void *fdt, int total_space)
 *
 *       	
 *       	 * If returned value is 1, there is no ATAG at the location
 *       	 * pointed by r8.  Try the typical 0x100 offset from start
 *       	 * of RAM and hope for the best.
 *       	 *
 *       	cmp	r0, #1
 *			; atgs_to_fdt의 리턴값(r0)이 1인지 비교한다.
 *			; 1이 리턴되는 경우는, atag를 찾지 못했을 경우이다.
 *
 *       	sub	r0, r4, #TEXT_OFFSET
 * 			; r4에는 0x8000 정도가 들어있을 것이다.
 *			; r0 = r4 - TEXT_OFFSET  =>  0또는 1(캐시온 보류시)
 *
 *       	bic	r0, r0, #1
 *			; CacheOn 보류시 설정되는 1을 제거
 *
 *       	add	r0, r0, #0x100
 *			; 0x100: atag의 위치
 *
 *       	mov	r1, r6
 *			; r6은 DTB를 가리키는  edata값
 *
 *       	mov	r2, r5
 *			; r5는 DTB의 total space 
 *
 *       	bleq	atags_to_fdt
 *			; r8에 atag 포인터가 담겨있지 않았다면, 직접 0x100을
 *			; 세팅하여 재시도한다. 일반적으로 atag는 0x100에 있기
 *			; 때문이다.
 *			;
 *			; [Like C Style]
 *			; r0 = r8; r1 = r6; r2 = r5;
 *			; r0 = atags_to_fdt( r0, r1, r2 );
 *			; if( r0 == 1 )
 *			; {
 *			;	r0 = 0x100; r1 = r6; r2 = r5;
 *			;	r0 = atags_to_fdt( r0, r1, r2 );
 *			; }
 *
 *       	ldmfd	sp!, {r0-r3, ip, lr}
 * 			; 위에서 저장한 ( stmfd	sp!, {r0-r3, ip, lr} )
 *			; Register들을 복원한다. (push-pop)
 *
 *       	sub	sp, sp, r5
 *			; sp = sp - DTB Total Size
 * 			; 위에서 더했던 ( add	sp, sp, r5 )
 *			; sp를 복원한다.
 *#endif
 *
 *		mov	r8, r6			@ use the appended device tree
 *			; r6은 DTB를 가리키는  edata값
 *
 *		[원문 주석]
 *		 Make sure that the DTB doesn't end up in the final
 *		 kernel's .bss area. To do so, we adjust the decompressed
 *		 kernel size to compensate if that .bss size is larger
 *		 than the relocated code.
 *		
 *		[번역]
 *              DTB가 최종적인 커널의 .bss 영역에 포함되지 않는 것을 보장해야
 *              한다.
 *		.bss사이즈가 재배치될 코드보다 크다면, 이를 맞춰주기 위해
 *		decompressed 커널의 사이즈를 조정한다.
 *		
 *		ldr	r5, =_kernel_bss_size
 *			; kernel_bss_size(압축풀린 커널 bss size)를 r5에 담는다.
 *
 *		adr	r1, wont_overwrite
 *			; wont_overwrite(wont_overwrite 이하의 영역에는
 *                      ; 덮어쓰기 해도 상관 없음)의 주소를 r1에 로드한다.
 *
 *		sub	r1, r6, r1
 *		subs	r1, r5, r1
 *			; r6: 압축된 커널 bss_start
 *			; r5: 압축 풀린 커널 bss size
 *                      ; r1 = _kernel_bss_size - (bss_start - wont_overwrite)
 *		addhi	r9, r9, r1
 *                      ; if(r1 > 0)
 *                      ;       r9 = r9 + r1;
 *			; r9: decompressed kernel size
 *			; 압축풀린 커널 bss사이즈가 재배치될 zImage 사이즈보다
 *                      ; 크다면, r9을 조정한다.
 *
 *		/* Get the current DTB size */
 *		ldr	r5, [r6, #4]
 *			; r6의 첫 4바이트는 매직값,
 *			; 그 다음 4바이트가 dtb_total_size
 *
 *#ifndef __ARMEB__
 *		/* convert r5 (dtb size) to little endian */
 *		eor	r1, r5, r5, ror #16
 *		bic	r1, r1, #0x00ff0000
 *		mov	r5, r5, ror #8
 *		eor	r5, r5, r1, lsr #8
 *#endif
 *
 *		/* preserve 64-bit alignment */
 *		add	r5, r5, #7
 *		bic	r5, r5, #7
 *
 *		/* relocate some pointers past the appended dtb */
 *		add	r6, r6, r5
 *			; dtb 끝(dtb_start + dtb_size)
 *                      ; 기존 r6 는 bss 시작점인 동시에 DTB 시작점 이었으나,
 *                      ; 이 코드에서부터 bss 시작점만을 가리키게 된다.
 *
 *		add	r10, r10, r5
 *   			; r10 = end of this image, including  bss/stack/malloc 
 *			        space if non XIP
 *			; r5(dtb사이즈를 기존에는 알 수 없었기 때문에, r10에도
 *			  반영이 되어있는 상황은 아닐 것이다. 이제 dtb size를
 *			  구해와서 알게됐으므로, r10에 반영한다.)
 *		add	sp, sp, r5
 *			; sp도 마찬가지로, r5를 더해서 늘려준다.
 * 
 *
 * 
 *   +-----------------------+  <- r10
 *   | buffer(64K),heap 영역 |     (848라인) add r10, sp, #0x10000
 *   +-----------------------+  <- sp
 *   |        stack          |
 *   +-----------------------+
 *   |         bss           |
 *   +-----------------------+  <- r6
 *   |                       |
 *   |        D  T  B        |  r5 = 갱신된 DTB size
 *   |                       |
 *   |Magic|TotalSize|.. |.. |
 *   +-----------------------+  <- DTB 시작
 *   |                       |
 *   |                       |
 *   |                       |  - 해결되지 않은 의문점
 *   |      K e r n e l      |    _kernal_bss_size 가 zImage 보다 작을 경우,
 *   |                       |    DTB 가 복사될 위치가 _kernel_bss 마지막 영역과
 *   |      (compressed)     |    일치하지 않게 되는데, 이 때의 처리가 아직없다.
 *   |                       |
 *   |                       |
 *   +-----------------------+
 *   |      Page Table       |
 *   +-----+-----------------+
 *   |     |      Atag       |
 *   |     +-----------------+
 *   |                       |
 *   +-----------------------+
 *   
 *dtb_check_done:
 *#endif
 *
 * 		*
 * 		* Check to see if we will overwrite ourselves.
 * 		*   r4  = final kernel address (possibly with LSB set)
 * 		*   r9  = size of decompressed image
 * 		*   r10 = end of this image, including  bss/stack/malloc space if non XIP
 * 		* We basically want:
 * 		*   r4 - 16k page directory >= r10 -> OK
 * 		*   r4 + image length <= address of wont_overwrite -> OK
 * 		* Note: the possible LSB in r4 is harmless here.
 * 		*
 * 		add	r10, r10, #16384
 * 		cmp	r4, r10
 * 		bhs	wont_overwrite
 * 		add	r10, r4, r9
 * 		adr	r9, wont_overwrite
 * 		cmp	r10, r9
 * 		bls	wont_overwrite
 * 
 *		*
 *		* Relocate ourselves past the end of the decompressed kernel.
 *		*   r6  = _edata
 *		*   r10 = end of the decompressed kernel
 *		* Because we always copy ahead, we need to do it from the end and go
 *		* backward in case the source and destination overlap.
 *		*
 * 		*
 * 		* Bump to the next 256-byte boundary with the size of
 * 		* the relocation code added. This avoids overwriting
 * 		* ourself when the offset is small.
 * 		*
 *
 * 		add	r10, r10, #((reloc_code_end - restart + 256) & ~255)
 * 		bic	r10, r10, #255
 * 
 * 		* Get start of code we want to copy and align it down. *
 * 		adr	r5, restart
 * 		bic	r5, r5, #31
 * 
 * ==================================================================
 */

restart:	adr	r0, LC0
		ldmia	r0, {r1, r2, r3, r6, r10, r11, r12}
		ldr	sp, [r0, #28]

		/*
		 * We might be running at a different address.  We need
		 * to fix up various pointers.
		 */
		sub	r0, r0, r1		@ calculate the delta offset
		add	r6, r6, r0		@ _edata
		add	r10, r10, r0		@ inflated kernel size location

		/*
		 * The kernel build system appends the size of the
		 * decompressed kernel at the end of the compressed data
		 * in little-endian form.
		 */
		ldrb	r9, [r10, #0]
		ldrb	lr, [r10, #1]
		orr	r9, r9, lr, lsl #8
		ldrb	lr, [r10, #2]
		ldrb	r10, [r10, #3]
		orr	r9, r9, lr, lsl #16
		orr	r9, r9, r10, lsl #24

#ifndef CONFIG_ZBOOT_ROM
		/* malloc space is above the relocated stack (64k max) */
		add	sp, sp, r0
		add	r10, sp, #0x10000
#else
		/*
		 * With ZBOOT_ROM the bss/stack is non relocatable,
		 * but someone could still run this code from RAM,
		 * in which case our reference is _edata.
		 */
		mov	r10, r6
#endif

		mov	r5, #0			@ init dtb size to 0
#ifdef CONFIG_ARM_APPENDED_DTB
/*
 *   r0  = delta
 *   r2  = BSS start
 *   r3  = BSS end
 *   r4  = final kernel address (possibly with LSB set)
 *   r5  = appended dtb size (still unknown)
 *   r6  = _edata
 *   r7  = architecture ID
 *   r8  = atags/device tree pointer
 *   r9  = size of decompressed image
 *   r10 = end of this image, including  bss/stack/malloc space if non XIP
 *   r11 = GOT start
 *   r12 = GOT end
 *   sp  = stack pointer
 *
 * if there are device trees (dtb) appended to zImage, advance r10 so that the
 * dtb data will get relocated along with the kernel if necessary.
 */

		ldr	lr, [r6, #0]
#ifndef __ARMEB__
		ldr	r1, =0xedfe0dd0		@ sig is 0xd00dfeed big endian
#else
		ldr	r1, =0xd00dfeed
#endif
		cmp	lr, r1
		bne	dtb_check_done		@ not found

#ifdef CONFIG_ARM_ATAG_DTB_COMPAT
		/*
		 * OK... Let's do some funky business here.
		 * If we do have a DTB appended to zImage, and we do have
		 * an ATAG list around, we want the later to be translated
		 * and folded into the former here. No GOT fixup has occurred
		 * yet, but none of the code we're about to call uses any
		 * global variable.
		*/

		/* Get the initial DTB size */
		ldr	r5, [r6, #4]
#ifndef __ARMEB__
		/* convert to little endian */
		eor	r1, r5, r5, ror #16
		bic	r1, r1, #0x00ff0000
		mov	r5, r5, ror #8
		eor	r5, r5, r1, lsr #8
#endif
		/* 50% DTB growth should be good enough */
		add	r5, r5, r5, lsr #1
		/* preserve 64-bit alignment */
		add	r5, r5, #7
		bic	r5, r5, #7
		/* clamp to 32KB min and 1MB max */
		cmp	r5, #(1 << 15)
		movlo	r5, #(1 << 15)
		cmp	r5, #(1 << 20)
		movhi	r5, #(1 << 20)
		/* temporarily relocate the stack past the DTB work space */
		add	sp, sp, r5

		stmfd	sp!, {r0-r3, ip, lr}
		mov	r0, r8
		mov	r1, r6
		mov	r2, r5
		bl	atags_to_fdt

		/*
		 * If returned value is 1, there is no ATAG at the location
		 * pointed by r8.  Try the typical 0x100 offset from start
		 * of RAM and hope for the best.
		 */
		cmp	r0, #1
		sub	r0, r4, #TEXT_OFFSET
		bic	r0, r0, #1
		add	r0, r0, #0x100
		mov	r1, r6
		mov	r2, r5
		bleq	atags_to_fdt

		ldmfd	sp!, {r0-r3, ip, lr}
		sub	sp, sp, r5
#endif

		mov	r8, r6			@ use the appended device tree

		/*
		 * Make sure that the DTB doesn't end up in the final
		 * kernel's .bss area. To do so, we adjust the decompressed
		 * kernel size to compensate if that .bss size is larger
		 * than the relocated code.
		 */
		ldr	r5, =_kernel_bss_size
		adr	r1, wont_overwrite
		sub	r1, r6, r1
		subs	r1, r5, r1
		addhi	r9, r9, r1

		/* Get the current DTB size */
		ldr	r5, [r6, #4]
#ifndef __ARMEB__
		/* convert r5 (dtb size) to little endian */
		eor	r1, r5, r5, ror #16
		bic	r1, r1, #0x00ff0000
		mov	r5, r5, ror #8
		eor	r5, r5, r1, lsr #8
#endif

		/* preserve 64-bit alignment */
		add	r5, r5, #7
		bic	r5, r5, #7

		/* relocate some pointers past the appended dtb */
		add	r6, r6, r5
		add	r10, r10, r5
		add	sp, sp, r5
dtb_check_done:
#endif

/*
 * Check to see if we will overwrite ourselves.
 *   r4  = final kernel address (possibly with LSB set)
 *   r9  = size of decompressed image
 *   r10 = end of this image, including  bss/stack/malloc space if non XIP
 * We basically want:
 *   r4 - 16k page directory >= r10 -> OK
 *   r4 + image length <= address of wont_overwrite -> OK
 * Note: the possible LSB in r4 is harmless here.
 */
		add	r10, r10, #16384
		cmp	r4, r10
		bhs	wont_overwrite
		add	r10, r4, r9
		adr	r9, wont_overwrite
		cmp	r10, r9
		bls	wont_overwrite

/*
 * Relocate ourselves past the end of the decompressed kernel.
 *   r6  = _edata
 *   r10 = end of the decompressed kernel
 * Because we always copy ahead, we need to do it from the end and go
 * backward in case the source and destination overlap.
 */
		/*
		 * Bump to the next 256-byte boundary with the size of
		 * the relocation code added. This avoids overwriting
		 * ourself when the offset is small.
		 */
		add	r10, r10, #((reloc_code_end - restart + 256) & ~255)
		bic	r10, r10, #255

		/* Get start of code we want to copy and align it down. */
		adr	r5, restart
		bic	r5, r5, #31

/* Relocate the hyp vector base if necessary */
#ifdef CONFIG_ARM_VIRT_EXT
		mrs	r0, spsr
		and	r0, r0, #MODE_MASK
		cmp	r0, #HYP_MODE
		bne	1f

		bl	__hyp_get_vectors
		sub	r0, r0, r5
		add	r0, r0, r10
		bl	__hyp_set_vectors
1:
#endif

		sub	r9, r6, r5		@ size to copy
		add	r9, r9, #31		@ rounded up to a multiple
		bic	r9, r9, #31		@ ... of 32 bytes
		add	r6, r9, r5
		add	r9, r9, r10

1:		ldmdb	r6!, {r0 - r3, r10 - r12, lr}
		cmp	r6, r5
		stmdb	r9!, {r0 - r3, r10 - r12, lr}
		bhi	1b

		/* Preserve offset to relocated code. */
		sub	r6, r9, r6

#ifndef CONFIG_ZBOOT_ROM
		/* cache_clean_flush may use the stack, so relocate it */
		add	sp, sp, r6
#endif

		bl	cache_clean_flush

		adr	r0, BSYM(restart)
		add	r0, r0, r6
		mov	pc, r0

wont_overwrite:
/*
 * If delta is zero, we are running at the address we were linked at.
 *   r0  = delta
 *   r2  = BSS start
 *   r3  = BSS end
 *   r4  = kernel execution address (possibly with LSB set)
 *   r5  = appended dtb size (0 if not present)
 *   r7  = architecture ID
 *   r8  = atags pointer
 *   r11 = GOT start
 *   r12 = GOT end
 *   sp  = stack pointer
 */
		orrs	r1, r0, r5
		beq	not_relocated

		add	r11, r11, r0
		add	r12, r12, r0

#ifndef CONFIG_ZBOOT_ROM
		/*
		 * If we're running fully PIC === CONFIG_ZBOOT_ROM = n,
		 * we need to fix up pointers into the BSS region.
		 * Note that the stack pointer has already been fixed up.
		 */
		add	r2, r2, r0
		add	r3, r3, r0

		/*
		 * Relocate all entries in the GOT table.
		 * Bump bss entries to _edata + dtb size
		 */
1:		ldr	r1, [r11, #0]		@ relocate entries in the GOT
		add	r1, r1, r0		@ This fixes up C references
		cmp	r1, r2			@ if entry >= bss_start &&
		cmphs	r3, r1			@       bss_end > entry
		addhi	r1, r1, r5		@    entry += dtb size
		str	r1, [r11], #4		@ next entry
		cmp	r11, r12
		blo	1b

		/* bump our bss pointers too */
		add	r2, r2, r5
		add	r3, r3, r5

#else

		/*
		 * Relocate entries in the GOT table.  We only relocate
		 * the entries that are outside the (relocated) BSS region.
		 */
1:		ldr	r1, [r11, #0]		@ relocate entries in the GOT
		cmp	r1, r2			@ entry < bss_start ||
		cmphs	r3, r1			@ _end < entry
		addlo	r1, r1, r0		@ table.  This fixes up the
		str	r1, [r11], #4		@ C references.
		cmp	r11, r12
		blo	1b
#endif

not_relocated:	mov	r0, #0
1:		str	r0, [r2], #4		@ clear bss
		str	r0, [r2], #4
		str	r0, [r2], #4
		str	r0, [r2], #4
		cmp	r2, r3
		blo	1b

		/*
		 * Did we skip the cache setup earlier?
		 * That is indicated by the LSB in r4.
		 * Do it now if so.
		 */
		tst	r4, #1
		bic	r4, r4, #1
		blne	cache_on

/*
 * The C runtime environment should now be setup sufficiently.
 * Set up some pointers, and start decompressing.
 *   r4  = kernel execution address
 *   r7  = architecture ID
 *   r8  = atags pointer
 */
		mov	r0, r4
		mov	r1, sp			@ malloc space above stack
		add	r2, sp, #0x10000	@ 64k max
		mov	r3, r7
		bl	decompress_kernel
		bl	cache_clean_flush
		bl	cache_off
		mov	r1, r7			@ restore architecture number
		mov	r2, r8			@ restore atags pointer

#ifdef CONFIG_ARM_VIRT_EXT
		mrs	r0, spsr		@ Get saved CPU boot mode
		and	r0, r0, #MODE_MASK
		cmp	r0, #HYP_MODE		@ if not booted in HYP mode...
		bne	__enter_kernel		@ boot kernel directly

		adr	r12, .L__hyp_reentry_vectors_offset
		ldr	r0, [r12]
		add	r0, r0, r12

		bl	__hyp_set_vectors
		__HVC(0)			@ otherwise bounce to hyp mode

		b	.			@ should never be reached

		.align	2
.L__hyp_reentry_vectors_offset:	.long	__hyp_reentry_vectors - .
#else
		b	__enter_kernel
#endif

		.align	2
		.type	LC0, #object
LC0:		.word	LC0			@ r1
		.word	__bss_start		@ r2
		.word	_end			@ r3
		.word	_edata			@ r6
		.word	input_data_end - 4	@ r10 (inflated size location)
		.word	_got_start		@ r11
		.word	_got_end		@ ip
		.word	.L_user_stack_end	@ sp
		.word	_end - restart + 16384 + 1024*1024
		.size	LC0, . - LC0

#ifdef CONFIG_ARCH_RPC
		.globl	params
params:		ldr	r0, =0x10000100		@ params_phys for RPC
		mov	pc, lr
		.ltorg
		.align
#endif

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-10-03
 * ------------------------------------------------------------------
 *
 * Turn on the cache.  We need to setup some page tables so that we
 * can have both the I and D caches on.
 *
 * We place the page tables 16k down from the kernel execution address,
 * and we hope that nothing else is using it.  If we're using it, we
 * will go pop!
 *
 * On entry,
 *  r4 = kernel execution address
 *  r7 = architecture number
 *  r8 = atags pointer
 * On exit,
 *  r0, r1, r2, r3, r9, r10, r12 corrupted ; 사용되는 레지스터
 * This routine must preserve:
 *  r4, r7, r8	; 사용되지 않은 레지스터
 *
 *  cache 를 ON시킴. I(Instruction Cache), D(Data Cache)
 *  커널 실행 주소로부터 아래로 16k에 page table을위치시킨다.
 *
 *		.align	5
 *			; 32Byte 정렬
 *			; TODO: align이 뭔지 애매하다??
 *
 *cache_on:	mov	r3, #8			@ cache_on function
 *			; ? #8 : cache on
 *
 *		b	call_cache_fn
 *
 * ==================================================================
 */

		.align	5
cache_on:	mov	r3, #8			@ cache_on function
		b	call_cache_fn

/*
 * Initialize the highest priority protection region, PR7
 * to cover all 32bit address and cacheable and bufferable.
 */
__armv4_mpu_cache_on:
		mov	r0, #0x3f		@ 4G, the whole
		mcr	p15, 0, r0, c6, c7, 0	@ PR7 Area Setting
		mcr 	p15, 0, r0, c6, c7, 1

		mov	r0, #0x80		@ PR7
		mcr	p15, 0, r0, c2, c0, 0	@ D-cache on
		mcr	p15, 0, r0, c2, c0, 1	@ I-cache on
		mcr	p15, 0, r0, c3, c0, 0	@ write-buffer on

		mov	r0, #0xc000
		mcr	p15, 0, r0, c5, c0, 1	@ I-access permission
		mcr	p15, 0, r0, c5, c0, 0	@ D-access permission

		mov	r0, #0
		mcr	p15, 0, r0, c7, c10, 4	@ drain write buffer
		mcr	p15, 0, r0, c7, c5, 0	@ flush(inval) I-Cache
		mcr	p15, 0, r0, c7, c6, 0	@ flush(inval) D-Cache
		mrc	p15, 0, r0, c1, c0, 0	@ read control reg
						@ ...I .... ..D. WC.M
		orr	r0, r0, #0x002d		@ .... .... ..1. 11.1
		orr	r0, r0, #0x1000		@ ...1 .... .... ....

		mcr	p15, 0, r0, c1, c0, 0	@ write control reg

		mov	r0, #0
		mcr	p15, 0, r0, c7, c5, 0	@ flush(inval) I-Cache
		mcr	p15, 0, r0, c7, c6, 0	@ flush(inval) D-Cache
		mov	pc, lr

__armv3_mpu_cache_on:
		mov	r0, #0x3f		@ 4G, the whole
		mcr	p15, 0, r0, c6, c7, 0	@ PR7 Area Setting

		mov	r0, #0x80		@ PR7
		mcr	p15, 0, r0, c2, c0, 0	@ cache on
		mcr	p15, 0, r0, c3, c0, 0	@ write-buffer on

		mov	r0, #0xc000
		mcr	p15, 0, r0, c5, c0, 0	@ access permission

		mov	r0, #0
		mcr	p15, 0, r0, c7, c0, 0	@ invalidate whole cache v3
		/*
		 * ?? ARMv3 MMU does not allow reading the control register,
		 * does this really work on ARMv3 MPU?
		 */
		mrc	p15, 0, r0, c1, c0, 0	@ read control reg
						@ .... .... .... WC.M
		orr	r0, r0, #0x000d		@ .... .... .... 11.1
		/* ?? this overwrites the value constructed above? */
		mov	r0, #0
		mcr	p15, 0, r0, c1, c0, 0	@ write control reg

		/* ?? invalidate for the second time? */
		mcr	p15, 0, r0, c7, c0, 0	@ invalidate whole cache v3
		mov	pc, lr

#ifdef CONFIG_CPU_DCACHE_WRITETHROUGH
#define CB_BITS 0x08
#else
#define CB_BITS 0x0c
#endif

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-10-10
 * 날짜: 2015-10-17
 * ------------------------------------------------------------------
 *
 *  Cache Cash : http://recipes.egloos.com/5170809
 *  MMU 에 대한 설명 : http://recipes.egloos.com/5232056
 *  페이징 소스를 C 코드로 구현 : http://kth3321.blogspot.kr/2013/04/arm-cortex-1.html
 *  소스 관련 참고 : http://www.iamroot.org/xe/Kernel_10_ARM/174738
 *
 * __setup_mmu:	sub	r3, r4, #16384		@ Page directory size
 *			; r3 = r4 - 16384(16k)
 *			; r4 = cache_on 에서 설정된 kernel execution address
 *			; arm의 경우 1M 단위로 페이지 세팅하기 때문에 
 *			; 2^32 = 4G는 총4096개의 
 *			; 엔트리가 필요하고 한 엔트리는 32비트 워드이기 때문
 *			; 4096 * 4byte = 16k
 *			; 0x8000 - 16k = 0x4000 모기향책 그림5-4 참고(66p)
 *
 *		bic	r3, r3, #0xff		@ Align the pointer
 *			; bit clear r3 = r3 & ~0xff
 *
 *		bic	r3, r3, #0x3f00
 *			; bit clear r3 = r3 & ~0x3f00
 *			; 왜 r3 & ~0x3fff 로 하지 않는가?
 *			; 답) 명령어 operand로 8bit만 받을 수있어 
 *			;     부득이 하게 2번 처리하는 것
 *			; 결국 하위 14bit를 지움, 16kB align을 위해
 *			; if r4 = 0x0000 8000, 최종적으로 r3 = 0x0000 4000
 *			; 참조)https://github.com/iamroot12b/kernel/issues/4
 *
 * Initialise the page tables, turning on the cacheable and bufferable
 * bits for the RAM area only.
 * 오직 메모리 영역만에만 cacheable bufferable로 설정 이것은 커널의 압축해제시,
 * 캐시와 쓰기버퍼사용을 통해서 압축해제 성능을 높이기 위함
 * 모기향 책 65p
 *
 *		mov	r0, r3
 *			; r0 = r3
 *			; r3 페이지 디렉토리 시작주소  0x0000 4000
 *			; 0100 0000 0000 0000
 *
 * 		mov	r9, r0, lsr #18
 *			; 000 0000 0000 0000
 *			; 2^18 = 256k
 *			; r9 = r0 >> 18
 *			; r9 = 0
 *
 *		mov	r9, r9, lsl #18		@ start of RAM
 *			; r9 = r9 << 18
 *			; r9 = 0
 *
 * 		add	r10, r9, #0x10000000	@ a reasonable RAM size
 *			; why? 265MB가 리눅스 구동하기 위한 최소한 메모리??
 *			; r10 = r9 + 256M
 *			; r10 = End Of Ram
 *
 *		mov	r1, #0x12		@ XN|U + section mapping
 *			; XN : http://www.iamroot.org/xe/Kernel_10_ARM/174738
 *			; #define XN (XN_MASK << XN_SHIFT)
 *			; XN_MASK = 0x01, XN_SHIFT = 4
 *			; XN : MMU 접근권한 설정 
 *			; r1 = 0x12 (10010)로 section entry table를 나타냄
 *			; When the XN bit is 1, a Permission fault is 
 *			; generated if the processor attempts to execute an
 *			; instruction fetched from the corresponding memory 
 *			; region.
 *			; Lv.1 Page Table Entry에서 U는 entry의 종류를 나타냄
 *			; 0b10은 section entry table을 나타냄
 *			; 0b11은 fine entry table을 나타냄
 *			; 0b01은 coarse entry table을 나타냄
 *			; 0b00은 page fault를 나타냄
 *
 *		orr	r1, r1, #3 << 10	@ AP=11
 *			; AP (Access Permition)를 설정
 *			; AP는 11,10번 비트, Read & Write된다는 뜻
 *			; r1 = 0b10010 | 0b110000000000
 *			; r1 = 0xc12 (0b1100 0001 0010)
 *
 * 		add	r2, r3, #16384
 *			; r2 = r3 + 16k
 *			; r2 = Lv.1 PageTable의 끝, 밑 1:루프의 끝을 설정
 *
 * 1:		cmp	r1, r9			@ if virt > start of RAM
 *			; r9은 시작주소
 *			; 0xc12(AP = 11, XN|U = 10010)  > r9
 *
 *		cmphs	r10, r1			@   && end of RAM > virt
 *			; r10 = 256MB
 *			; hs (higher or same)
 *			; if (r9 <= r1)면 수행됨
 *
 *		bic	r1, r1, #0x1c		@ clear XN|U + C + B
 *			; r1 = 0xc12 (0b1100 0001 0010)
 *			;      0x01c (0b0000 0001 1100)
 *			; r1 = 0xc02 (0b1100 0000 0010)
 *
 *		orrlo	r1, r1, #0x10		@ Set XN|U for non-RAM
 *			; if(r10 < r1)면 수행됨, r10 = end of Ram
 *			; post fix lo는 Unsigned lower (identical to CC)
 *
 *		orrhs	r1, r1, r6		@ set RAM section settings
 *			; if (r9 <= r1 < r10)면 orrhs가 수행됨
 *			; arch/arm/mm/Kconfig파일에 CPU_DCACHE_WRITETHROUGH
 *			; 셋되어 있으면 CB_BITS = 0x08, 아니면 0x0c 
 *			; r6 = #CB_BITS | 0x02로 0x1E아니면 0x1A값을 가짐  
 *
 *		str	r1, [r0], #4		@ 1:1 mapping
 *			; r0는 페이지 디렉토리 시작주소  0x0000 4000 
 *			; *(r0 + 4) = r1 
 *
 *		add	r1, r1, #1048576
 *			; 1048576 = 0x100000
 *			; section table entry의 base address는 1M단위
 *
 *		teq	r0, r2
 *			; r2 = Lv.1 PageTable의 끝, 끝까지 루프를 돌았나?
 *
 *		bne	1b
 *			; 아니면 1로 점프
 *			; r0 = r3
 *			; r3 페이지 디렉토리 시작주소  0x0000 4000
 *			; 0100 0000 0000 0000
 *
 * 		mov	r9, r0, lsr #18
 *			; 000 0000 0000 0000
 *			; 2^18 = 256k
 *			; r9 = r0 >> 18
 *			; r9 = 0
 *
 *		mov	r9, r9, lsl #18		@ start of RAM
 *			; r9 = r9 << 18
 *			; r9 = 0
 *
 * 		add	r10, r9, #0x10000000	@ a reasonable RAM size
 *			; why? 265MB가 리눅스 구동하기 위한 최소한 메모리??
 *			; r10 = r9 + 256M
 *			; r10 = End Of Ram
 *
 *		mov	r1, #0x12		@ XN|U + section mapping
 *			; XN : http://www.iamroot.org/xe/Kernel_10_ARM/174738
 *			; #define XN (XN_MASK << XN_SHIFT)
 *			; XN_MASK = 0x01, XN_SHIFT = 4
 *			; XN : MMU 접근권한 설정
 *			; r1 = 0x12 (10010)로 section entry table를 나타냄
 *			; When the XN bit is 1, a Permission fault is
 *			; generated if the processor attempts to execute an
 *			; instruction fetched from the corresponding memory
 *			; region.
 *			; Lv.1 Page Table Entry에서 U는 entry의 종류를 나타냄
 *			; 0b10은 section entry table을 나타냄
 *			; 0b11은 fine entry table을 나타냄
 *			; 0b01은 coarse entry table을 나타냄
 *			; 0b00은 page fault를 나타냄
 *
 *		orr	r1, r1, #3 << 10	@ AP=11
 *			; AP (Access Permition)를 설정
 *			; AP는 11,10번 비트, Read & Write된다는 뜻
 *			; r1 = 0b10010 | 0b110000000000
 *			; r1 = 0xc12 (0b1100 0001 0010)
 *
 * 		add	r2, r3, #16384
 *			; r2 = r3 + 16k
 *			; r2 = Lv.1 PageTable의 끝, 밑 1:루프의 끝을 설정
 *
 * 1:		cmp	r1, r9			@ if virt > start of RAM
 *			; r9은 시작주소
 *			; 0xc12(AP = 11, XN|U = 10010)  > r9
 *
 *		cmphs	r10, r1			@   && end of RAM > virt
 *			; r10 = 256MB
 *			; hs (higher or same)
 *			; if (r9 <= r1)면 수행됨
 *
 *		bic	r1, r1, #0x1c		@ clear XN|U + C + B
 *			; r1 = 0xc12 (0b1100 0001 0010)
 *			;      0x01c (0b0000 0001 1100)
 * 			; r1 = 0xc02 (0b1100 0000 0010)
 *
 *		orrlo	r1, r1, #0x10		@ Set XN|U for non-RAM
 *			; if(r10 < r1)면 수행됨, r10 = end of Ram
 *			; post fix lo는 Unsigned lower (identical to CC)
 *
 *		orrhs	r1, r1, r6		@ set RAM section settings
 *			; if (r9 <= r1 < r10)면 orrhs가 수행됨
 *			; arch/arm/mm/Kconfig파일에 CPU_DCACHE_WRITETHROUGH
 *			; 셋되어 있으면 CB_BITS = 0x08, 아니면 0x0c
 *			; r6 = #CB_BITS | 0x02로 0xE아니면 0xA값을 가짐
 *
 *		str	r1, [r0], #4		@ 1:1 mapping
 *			; r0는 페이지 디렉토리 시작주소  0x0000 4000
 *			; *(r0 + 4) = r1
 *
 *		add	r1, r1, #1048576
 *			; 1048576 = 0x100000
 *			; section table entry의 base address는 1M단위
 *
 *		teq	r0, r2
 *			; r2 = Lv.1 PageTable의 끝, 끝까지 루프를 돌았나?
 *
 *		bne	1b
 *			; 아니면 1로 점프
 *
 *
 * If ever we are running from Flash, then we surely want the cache
 * to be enabled also for our execution instance...  We map 2MB of it
 * so there is no map overlap problem for up to 1 MB compressed kernel.
 * If the execution is in RAM then we would only be duplicating the above.
 *
 * 		orr	r1, r6, #0x04		@ ensure B is set for this
 *			; B(Write Buffer)를 on
 *			; r1 = 0x0E
 *			; CPU_DCACHE_WRITETHROUGH에 독립적으로 B를 활성화
 *
 *		orr	r1, r1, #3 << 10
 *			; AP를 rw활성화
 *
 *		mov	r2, pc
 *			; promgram counter를 r2에 대입
 *
 *		mov	r2, r2, lsr #20
 *			; r2 = r2 >> 20
 *			; base 주소를 얻기 위한 과정
 *
 *		orr	r1, r1, r2, lsl #20
 * 			; r1 |= (r2 << 20)
 *			; r1(0xC0E)
 * 			; 설정값과 base 주소를 조합하는 과정
 *
 *		add	r0, r3, r2, lsl #2
 *			; r3 = 0x4000 (page directory 시작 주소)
 * 			; r2 = base address
 *			; r0 += (r2 << 2)
 *			; result : r0 = 0x4000 + pc의 page entry의 주소
 *
 *		str	r1, [r0], #4
 *			; *r0 = r1 ; r0 += 4
 *
 * 		add	r1, r1, #1048576
 *			; 다음 page entry 값 세팅
 * 			; 1MB = #1048576
 *
 *		str	r1, [r0]
 *			; *r0 = r1
 *
 *		mov	pc, lr
 *			; C언어의 return
 *			; pc = link register
 *
 * ENDPROC(__setup_mmu)
 *
 * =============================================================================
 */

/* ==========================================================================
 * 제 27주차 스터디
 * 날짜: 2015-10-24
 * 장소: 커널 연구회
 *
 * 팀:     Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 참석자: 곽희범 (andrew@norux.me)
 *	   임채훈 (im.fehead@gmail.com)
 *	   민혜미 (mhyem2@naver.com)
 *	   김영준 (iam.yeongjunkim@gmail.com)
 *	   박종성 (@minidump)
 *	   박병배 (i.billpark@gmail.com)
 *	   안종찬 (ahnchan2@gmail.com)
 *	   김건용 (gykim0914@gmail.com)
 *	   권세홍 (sehongkwon2.24@gmail.com)
 *	   조훈근 (hoonycho12@gmail.com)
 *
 *	   참석인원: 10 명
 * ==========================================================================*/

__setup_mmu:	sub	r3, r4, #16384		@ Page directory size
		bic	r3, r3, #0xff		@ Align the pointer
		bic	r3, r3, #0x3f00
/*
 * Initialise the page tables, turning on the cacheable and bufferable
 * bits for the RAM area only.
 */
		mov	r0, r3
		mov	r9, r0, lsr #18
		mov	r9, r9, lsl #18		@ start of RAM
		add	r10, r9, #0x10000000	@ a reasonable RAM size
		mov	r1, #0x12		@ XN|U + section mapping
		orr	r1, r1, #3 << 10	@ AP=11
		add	r2, r3, #16384
1:		cmp	r1, r9			@ if virt > start of RAM
		cmphs	r10, r1			@   && end of RAM > virt
		bic	r1, r1, #0x1c		@ clear XN|U + C + B
		orrlo	r1, r1, #0x10		@ Set XN|U for non-RAM
		orrhs	r1, r1, r6		@ set RAM section settings
		str	r1, [r0], #4		@ 1:1 mapping
		add	r1, r1, #1048576
		teq	r0, r2
		bne	1b
/*
 * If ever we are running from Flash, then we surely want the cache
 * to be enabled also for our execution instance...  We map 2MB of it
 * so there is no map overlap problem for up to 1 MB compressed kernel.
 * If the execution is in RAM then we would only be duplicating the above.
 */
		orr	r1, r6, #0x04		@ ensure B is set for this
		orr	r1, r1, #3 << 10
		mov	r2, pc
		mov	r2, r2, lsr #20
		orr	r1, r1, r2, lsl #20
		add	r0, r3, r2, lsl #2
		str	r1, [r0], #4
		add	r1, r1, #1048576
		str	r1, [r0]
		mov	pc, lr
ENDPROC(__setup_mmu)

@ Enable unaligned access on v6, to allow better code generation
@ for the decompressor C code:
__armv6_mmu_cache_on:
		mrc	p15, 0, r0, c1, c0, 0	@ read SCTLR
		bic	r0, r0, #2		@ A (no unaligned access fault)
		orr	r0, r0, #1 << 22	@ U (v6 unaligned access model)
		mcr	p15, 0, r0, c1, c0, 0	@ write SCTLR
		b	__armv4_mmu_cache_on

__arm926ejs_mmu_cache_on:
#ifdef CONFIG_CPU_DCACHE_WRITETHROUGH
		mov	r0, #4			@ put dcache in WT mode
		mcr	p15, 7, r0, c15, c0, 0
#endif

__armv4_mmu_cache_on:
		mov	r12, lr
#ifdef CONFIG_MMU
		mov	r6, #CB_BITS | 0x12	@ U
		bl	__setup_mmu
		mov	r0, #0
		mcr	p15, 0, r0, c7, c10, 4	@ drain write buffer
		mcr	p15, 0, r0, c8, c7, 0	@ flush I,D TLBs
		mrc	p15, 0, r0, c1, c0, 0	@ read control reg
		orr	r0, r0, #0x5000		@ I-cache enable, RR cache replacement
		orr	r0, r0, #0x0030
 ARM_BE8(	orr	r0, r0, #1 << 25 )	@ big-endian page tables
		bl	__common_mmu_cache_on
		mov	r0, #0
		mcr	p15, 0, r0, c8, c7, 0	@ flush I,D TLBs
#endif
		mov	pc, r12

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-10-10
 * ------------------------------------------------------------------
 * __armv7_mmu_cache_on:
 * 		mov	r12, lr
 *			; r12 = lr
 *
 * #ifdef CONFIG_MMU
 *			; arch/arm/Kconfig
 *			; config MMU
 *			; bool "MMU-based Paged Memory Management Support"
 *			; default y
 * 			; help
 *	  		; Select if you want MMU-based virtualised 
 *			; addressing space
 *	  		; support by paged memory management. 
 *			; If unsure, say 'Y'.
 *
 *		mrc	p15, 0, r11, c0, c1, 4	@ read ID_MMFR0
 *			; ID_MMFR0 : 메모리 모델 정보, 메모리관리 서포트 정보
 *			; http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0438e/BABEHBEB.html
 *
 *		tst	r11, #0xf		@ VMSA
 *			; 하위 4비트 VMSA 지원여부 체크 
 * 			; 버추얼 메모리 시스템 아키텍쳐 = MMU? 
 *
 *		movne	r6, #CB_BITS | 0x02	@ !XN
 *			; z 플래그가 0아닐 때(지원하면) 실행
 *			; arch/arm/mm/Kconfig파일에 CPU_DCACHE_WRITETHROUGH
 *			; 셋되어 있으면 CB_BITS = 0x08, 아니면 0x0c 
 *			; r6 = #CB_BITS | 0x02  
 *
 * 		blne	__setup_mmu
 *
 * ==================================================================
 */

/* ==========================================================================
 * Team:           Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * Date:           24.10.2015
 *
 *		clean과 flush 차이 : http://linuxphil.blogspot.kr/2011/01/arm-clean-flush-invalidate.html ;
 * This code is...
 *		mov	r0, #0
 *			; r0 = 0
 *
 *		mcr	p15, 0, r0, c7, c10, 4	@ drain write buffer
 *			; cache의 clean == WriteBuffer의 drain (메모리 반영x)
 *			; mcr (move to co-processor from resgister)
 *
 *		tst	r11, #0xf		@ VMSA
 *			; VMSA 지원가능 여부 check
 *
 *		mcrne	p15, 0, r0, c8, c7, 0	@ flush I,D TLBs
 * 			; 메모리 반영시키고 MMU의 페이지 테이블의 cache 비움
 *
 * #endif
 *		mrc	p15, 0, r0, c1, c0, 0	@ read control reg
 *			; r0로 control register를 가져옴.
 *			; http://infocenter.arm.com/help/topic/com.arm.doc.ddi0464f/DDI0464F_cortex_a7_mpcore_r0p5_trm.pdf p.108
 *
 * 		bic	r0, r0, #1 << 28	@ clear SCTLR.TRE
 *			; TRE : tex remap enable / disable
 *			; text remap disable
 *
 *		orr	r0, r0, #0x5000		@ I-cache enable, RR cache replacement
 *			; r0 |= 0x5000
 *			; round
 *
 *		orr	r0, r0, #0x003c		@ write buffer
 *			; r0 |= 0x003c
 *
 *		bic	r0, r0, #2		@ A (no unaligned access fault)
 *			; r0 &= ~2
 *			; SCTLR의 2번째 비트를 clear하면, alignment fault check
 *			  disable
 *			; ex) ldrh : align checking을 half word 단위로 하는데,
 *			; SCTLR의 2번째 비트가 0으로 되어 있으면, 사용가능하고
 *			; 1로 되어 있으면, alignment fault가 발생한다.
 *			; link : http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0338g/Cdffhdje.html
 *
 *		orr	r0, r0, #1 << 22	@ U (v6 unaligned access model)
 *			; arm v7의 경우 항상 1
 *
 *						@ (needed for ARM1176)
 * 
 * ==================================================================
 */

/* ==========================================================================
 * Team:           Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * Date:           24.10.2015
 *
 * This code is...
 *
 * #ifdef CONFIG_MMU
 * ARM_BE8(orr	r0, r0, #1 << 25 )	@ big-endian page tables
 *			; ARM_BE8일 경우에 내부 명령어 실행
 *			; SCTLR의 25번째 비트가 EE
 *			; 용도: exception이 발생시, SCTLR.EE를 통해 CPSR.E를 
 *			  설정
 *
 *		mrcne   p15, 0, r6, c2, c0, 2   @ read ttb control reg
 *			; if (VMSA)
 *			; r6  <- ttb(translation table base register)
 *			; ttbr0 = user용
 *			; ttbr1 = kernel용
 *			; link : http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0211k/Bihgfcgf.html
 *
 *		orrne	r0, r0, #1		@ MMU enabled
 *			; if (VMSA)
 *			; r0 |= 1
 *
 *		movne	r1, #0xfffffffd		@ domain 0 = client
 *			; DACR의 값을 설정
 *			; 00 : No access	01: Client
 *			; 10 : Reserved		11: Master
 *
 *		bic     r6, r6, #1 << 31        @ 32-bit translation system
 * 			; r6의 2^32자리를 clear
 *			; 31번 bit ->  EAE (extended address enable)
 *			; EAE가 0일 경우, 32bit를 이용한다.
 *			; EAE가 1일 경우, 40bit를 이용한다.
 *			; ref : PAE
 *
 *		bic     r6, r6, #3 << 0         @ use only ttbr0
 *			; 하위 2비트를 clear
 *			; TTBCR의 N[2:0]으로 TTBR0의 base address field
 *			; 길이를 설정한다.
 *			; base address field 길이 : 31 ~ 14-N
 *			; N : 0 ~ 7
 *			; link : http://liris.cnrs.fr/~mmrissa/lib/exe/fetch.php?media=armv7-a-r-manual.pdf
 *			; N이 0일경우 ARM v5와 ARM v6가 호환이 되고
 *			; 그외의 값의 경우 base address field의 길이를 설정한다
 *
 *		mcrne	p15, 0, r3, c2, c0, 0	@ load page table pointer
 *			; r3 <- page table pointer (0x4000)
 *
 *		mcrne	p15, 0, r1, c3, c0, 0	@ load domain access control
 *			;  r1 <- domain access control
 *
 *		mcrne   p15, 0, r6, c2, c0, 2   @ load ttb control
 * #endif
 *		mcr	p15, 0, r0, c7, c5, 4	@ ISB
 *			; Instruction Syncronization Barrier
 *			; ISB 수행 전의 내용을 파이프라인에서 모두 반영하여 
 *			  비운다.
 *			; 파이프라인을 비워서, 모든 명령들이 cache 나 memory 로
 *			  부터 fetch 되도록 한다.
 *			; 따라서, ISB 이전 수행이 완료되었음을 보장한다.
 *			
 *			; (참고URL)
 *			; http://forum.falinux.com/zbxe/index.php?document_srl=534002&mid=Kernel_API
 *			; http://www.iamroot.org/xe/Kernel_8_ARM/58427
 *			; (참고) 이전 버전의 커널에서는 ISB를 사용하지 않고,
 *			; 다음과 같이 처리함.
 *			;   mcr	p15, 0, r0, c1, c0, 0  @ load control register
 *			;   mrc	p15, 0, r0, c1, c0, 0  @ and read it back to
 *			;   sub	pc, lr, r0, lsr #32    @ properly flush pipeline
 *
 *		mcr	p15, 0, r0, c1, c0, 0	@ load control register
 *		mrc	p15, 0, r0, c1, c0, 0	@ and read it back
 *		mov	r0, #0
 *		mcr	p15, 0, r0, c7, c5, 4	@ ISB
 *		mov	pc, r12
 *
 * ==========================================================================*/

__armv7_mmu_cache_on:
		mov	r12, lr
#ifdef CONFIG_MMU
		mrc	p15, 0, r11, c0, c1, 4	@ read ID_MMFR0
		tst	r11, #0xf		@ VMSA
		movne	r6, #CB_BITS | 0x02	@ !XN
		blne	__setup_mmu
		mov	r0, #0
		mcr	p15, 0, r0, c7, c10, 4	@ drain write buffer
		tst	r11, #0xf		@ VMSA
		mcrne	p15, 0, r0, c8, c7, 0	@ flush I,D TLBs
#endif
		mrc	p15, 0, r0, c1, c0, 0	@ read control reg
		bic	r0, r0, #1 << 28	@ clear SCTLR.TRE
		orr	r0, r0, #0x5000		@ I-cache enable, RR cache replacement
		orr	r0, r0, #0x003c		@ write buffer
		bic	r0, r0, #2		@ A (no unaligned access fault)
		orr	r0, r0, #1 << 22	@ U (v6 unaligned access model)
						@ (needed for ARM1176)
#ifdef CONFIG_MMU
 ARM_BE8(	orr	r0, r0, #1 << 25 )	@ big-endian page tables
		mrcne   p15, 0, r6, c2, c0, 2   @ read ttb control reg
		orrne	r0, r0, #1		@ MMU enabled
		movne	r1, #0xfffffffd		@ domain 0 = client
		bic     r6, r6, #1 << 31        @ 32-bit translation system
		bic     r6, r6, #3 << 0         @ use only ttbr0
		mcrne	p15, 0, r3, c2, c0, 0	@ load page table pointer
		mcrne	p15, 0, r1, c3, c0, 0	@ load domain access control
		mcrne   p15, 0, r6, c2, c0, 2   @ load ttb control
#endif
		mcr	p15, 0, r0, c7, c5, 4	@ ISB
		mcr	p15, 0, r0, c1, c0, 0	@ load control register
		mrc	p15, 0, r0, c1, c0, 0	@ and read it back
		mov	r0, #0
		mcr	p15, 0, r0, c7, c5, 4	@ ISB
		mov	pc, r12

__fa526_cache_on:
		mov	r12, lr
		mov	r6, #CB_BITS | 0x12	@ U
		bl	__setup_mmu
		mov	r0, #0
		mcr	p15, 0, r0, c7, c7, 0	@ Invalidate whole cache
		mcr	p15, 0, r0, c7, c10, 4	@ drain write buffer
		mcr	p15, 0, r0, c8, c7, 0	@ flush UTLB
		mrc	p15, 0, r0, c1, c0, 0	@ read control reg
		orr	r0, r0, #0x1000		@ I-cache enable
		bl	__common_mmu_cache_on
		mov	r0, #0
		mcr	p15, 0, r0, c8, c7, 0	@ flush UTLB
		mov	pc, r12

__common_mmu_cache_on:
#ifndef CONFIG_THUMB2_KERNEL
#ifndef DEBUG
		orr	r0, r0, #0x000d		@ Write buffer, mmu
#endif
		mov	r1, #-1
		mcr	p15, 0, r3, c2, c0, 0	@ load page table pointer
		mcr	p15, 0, r1, c3, c0, 0	@ load domain access control
		b	1f
		.align	5			@ cache line aligned
1:		mcr	p15, 0, r0, c1, c0, 0	@ load control register
		mrc	p15, 0, r0, c1, c0, 0	@ and read it back to
		sub	pc, lr, r0, lsr #32	@ properly flush pipeline
#endif

#define PROC_ENTRY_SIZE (4*5)

/*
 * ====================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * --------------------------------------------------------------------
 *
 * Here follow the relocatable cache support functions for the
 * various processors.  This is a generic hook for locating an
 * entry and jumping to an instruction at the specified offset
 * from the start of the block.  Please note this is all position
 * independent code.
 *
 *  r1  = corrupted
 *  r2  = corrupted
 *  r3  = block offset
 *  r9  = corrupted
 *  r12 = corrupted
 *
 *  cache on : r3 = 8
 *   On entry,
 *    r4 = kernel execution address
 *    r7 = architecture number
 *    r8 = atags pointer
 *
 *
 * call_cache_fn:	adr	r12, proc_types
 * 			; r12 = &proc_types;
 *
 * #ifdef CONFIG_CPU_CP15
 * 		mrc	p15, 0, r9, c0, c0	@ get processor ID
 *			; r9 = processor ID
 *
 * #elif defined(CONFIG_CPU_V7M) ; cp15 코프로쎄서가 없으며 V7M cpu일때
 * 		/ *
 * 		 * On v7-M the processor id is located in the V7M_SCB_CPUID
 * 		 * register, but as cache handling is IMPLEMENTATION DEFINED on
 * 		 * v7-M (if existant at all) we just return early here.
 * 		 * If V7M_SCB_CPUID were used the cpu ID functions (i.e.
 * 		 * __armv7_mmu_cache_{on,off,flush}) would be selected which
 * 		 * use cp15 registers that are not implemented on v7-M.
 * 		 * /
 * 		bx	lr
 * #else
 * 		ldr	r9, =CONFIG_PROCESSOR_ID
 *			; r9 = CONFIG_PROCESSOR_ID
 *
 * #endif // CONFIG_CPU_CP15
 *
 * 1:		ldr	r1, [r12, #0]		@ get value
 * 			; r1 = cpu ID : 0x410fc075 (라즈베리파이 2 CPU ID)
 *
 * 		ldr	r2, [r12, #4]		@ get mask
 * 			; r2 = cpu ID mask : 0x000f0000
 *
 * 		eor	r1, r1, r9		@ (real ^ match)
 * 			; r1 = r1 ^ r9
 *
 * 		tst	r1, r2			@       & mask
 *			; cpsr = r1 & 0xff00f000
 *			; 정리 --> cpsr = (r9 ^ cpuID) & cpuMask
 *			;  0 이면 : 같음, 1이외값 : 틀림
 *
 *  ARM(	addeq	pc, r12, r3		) @ call cache function
 *			; r3=(cache온, cache오프, cache 클리어)
 *			; tst r1, r2 값이 0으로 세팅되어있으면 pc = r12+r3
 *
 *  THUMB(	addeq	r12, r3			)
 *  THUMB(	moveq	pc, r12			) @ call cache function
 * 		add	r12, r12, #PROC_ENTRY_SIZE
 * 		b	1b
 * 
 * ====================================================================
 */

call_cache_fn:	adr	r12, proc_types
#ifdef CONFIG_CPU_CP15
		mrc	p15, 0, r9, c0, c0	@ get processor ID
#elif defined(CONFIG_CPU_V7M)
		/*
		 * On v7-M the processor id is located in the V7M_SCB_CPUID
		 * register, but as cache handling is IMPLEMENTATION DEFINED on
		 * v7-M (if existant at all) we just return early here.
		 * If V7M_SCB_CPUID were used the cpu ID functions (i.e.
		 * __armv7_mmu_cache_{on,off,flush}) would be selected which
		 * use cp15 registers that are not implemented on v7-M.
		 */
		bx	lr
#else
		ldr	r9, =CONFIG_PROCESSOR_ID
#endif
1:		ldr	r1, [r12, #0]		@ get value
		ldr	r2, [r12, #4]		@ get mask
		eor	r1, r1, r9		@ (real ^ match)
		tst	r1, r2			@       & mask
 ARM(		addeq	pc, r12, r3		) @ call cache function
 THUMB(		addeq	r12, r3			)
 THUMB(		moveq	pc, r12			) @ call cache function
		add	r12, r12, #PROC_ENTRY_SIZE
		b	1b

/*
 * Table for cache operations.  This is basically:
 *   - CPU ID match
 *   - CPU ID mask
 *   - 'cache on' method instruction
 *   - 'cache off' method instruction
 *   - 'cache flush' method instruction
 *
 * We match an entry using: ((real_id ^ match) & mask) == 0
 *
 * Writethrough caches generally only need 'on' and 'off'
 * methods.  Writeback caches _must_ have the flush method
 * defined.
 */
		.align	2
		.type	proc_types,#object
proc_types:
		.word	0x41000000		@ old ARM ID
		.word	0xff00f000
		mov	pc, lr
 THUMB(		nop				)
		mov	pc, lr
 THUMB(		nop				)
		mov	pc, lr
 THUMB(		nop				)

		.word	0x41007000		@ ARM7/710
		.word	0xfff8fe00
		mov	pc, lr
 THUMB(		nop				)
		mov	pc, lr
 THUMB(		nop				)
		mov	pc, lr
 THUMB(		nop				)

		.word	0x41807200		@ ARM720T (writethrough)
		.word	0xffffff00
		W(b)	__armv4_mmu_cache_on
		W(b)	__armv4_mmu_cache_off
		mov	pc, lr
 THUMB(		nop				)

		.word	0x41007400		@ ARM74x
		.word	0xff00ff00
		W(b)	__armv3_mpu_cache_on
		W(b)	__armv3_mpu_cache_off
		W(b)	__armv3_mpu_cache_flush
		
		.word	0x41009400		@ ARM94x
		.word	0xff00ff00
		W(b)	__armv4_mpu_cache_on
		W(b)	__armv4_mpu_cache_off
		W(b)	__armv4_mpu_cache_flush

		.word	0x41069260		@ ARM926EJ-S (v5TEJ)
		.word	0xff0ffff0
		W(b)	__arm926ejs_mmu_cache_on
		W(b)	__armv4_mmu_cache_off
		W(b)	__armv5tej_mmu_cache_flush

		.word	0x00007000		@ ARM7 IDs
		.word	0x0000f000
		mov	pc, lr
 THUMB(		nop				)
		mov	pc, lr
 THUMB(		nop				)
		mov	pc, lr
 THUMB(		nop				)

		@ Everything from here on will be the new ID system.

		.word	0x4401a100		@ sa110 / sa1100
		.word	0xffffffe0
		W(b)	__armv4_mmu_cache_on
		W(b)	__armv4_mmu_cache_off
		W(b)	__armv4_mmu_cache_flush

		.word	0x6901b110		@ sa1110
		.word	0xfffffff0
		W(b)	__armv4_mmu_cache_on
		W(b)	__armv4_mmu_cache_off
		W(b)	__armv4_mmu_cache_flush

		.word	0x56056900
		.word	0xffffff00		@ PXA9xx
		W(b)	__armv4_mmu_cache_on
		W(b)	__armv4_mmu_cache_off
		W(b)	__armv4_mmu_cache_flush

		.word	0x56158000		@ PXA168
		.word	0xfffff000
		W(b)	__armv4_mmu_cache_on
		W(b)	__armv4_mmu_cache_off
		W(b)	__armv5tej_mmu_cache_flush

		.word	0x56050000		@ Feroceon
		.word	0xff0f0000
		W(b)	__armv4_mmu_cache_on
		W(b)	__armv4_mmu_cache_off
		W(b)	__armv5tej_mmu_cache_flush

#ifdef CONFIG_CPU_FEROCEON_OLD_ID
		/* this conflicts with the standard ARMv5TE entry */
		.long	0x41009260		@ Old Feroceon
		.long	0xff00fff0
		b	__armv4_mmu_cache_on
		b	__armv4_mmu_cache_off
		b	__armv5tej_mmu_cache_flush
#endif

		.word	0x66015261		@ FA526
		.word	0xff01fff1
		W(b)	__fa526_cache_on
		W(b)	__armv4_mmu_cache_off
		W(b)	__fa526_cache_flush

		@ These match on the architecture ID

		.word	0x00020000		@ ARMv4T
		.word	0x000f0000
		W(b)	__armv4_mmu_cache_on
		W(b)	__armv4_mmu_cache_off
		W(b)	__armv4_mmu_cache_flush

		.word	0x00050000		@ ARMv5TE
		.word	0x000f0000
		W(b)	__armv4_mmu_cache_on
		W(b)	__armv4_mmu_cache_off
		W(b)	__armv4_mmu_cache_flush

		.word	0x00060000		@ ARMv5TEJ
		.word	0x000f0000
		W(b)	__armv4_mmu_cache_on
		W(b)	__armv4_mmu_cache_off
		W(b)	__armv5tej_mmu_cache_flush

		.word	0x0007b000		@ ARMv6
		.word	0x000ff000
		W(b)	__armv6_mmu_cache_on
		W(b)	__armv4_mmu_cache_off
		W(b)	__armv6_mmu_cache_flush

		.word	0x000f0000		@ new CPU Id
		.word	0x000f0000
		W(b)	__armv7_mmu_cache_on
		W(b)	__armv7_mmu_cache_off
		W(b)	__armv7_mmu_cache_flush

		.word	0			@ unrecognised type
		.word	0
		mov	pc, lr
 THUMB(		nop				)
		mov	pc, lr
 THUMB(		nop				)
		mov	pc, lr
 THUMB(		nop				)

		.size	proc_types, . - proc_types

		/*
		 * If you get a "non-constant expression in ".if" statement"
		 * error from the assembler on this line, check that you have
		 * not accidentally written a "b" instruction where you should
		 * have written W(b).
		 */
		.if (. - proc_types) % PROC_ENTRY_SIZE != 0
		.error "The size of one or more proc_types entries is wrong."
		.endif

/*
 * Turn off the Cache and MMU.  ARMv3 does not support
 * reading the control register, but ARMv4 does.
 *
 * On exit,
 *  r0, r1, r2, r3, r9, r12 corrupted
 * This routine must preserve:
 *  r4, r7, r8
 */
		.align	5
cache_off:	mov	r3, #12			@ cache_off function
		b	call_cache_fn

__armv4_mpu_cache_off:
		mrc	p15, 0, r0, c1, c0
		bic	r0, r0, #0x000d
		mcr	p15, 0, r0, c1, c0	@ turn MPU and cache off
		mov	r0, #0
		mcr	p15, 0, r0, c7, c10, 4	@ drain write buffer
		mcr	p15, 0, r0, c7, c6, 0	@ flush D-Cache
		mcr	p15, 0, r0, c7, c5, 0	@ flush I-Cache
		mov	pc, lr

__armv3_mpu_cache_off:
		mrc	p15, 0, r0, c1, c0
		bic	r0, r0, #0x000d
		mcr	p15, 0, r0, c1, c0, 0	@ turn MPU and cache off
		mov	r0, #0
		mcr	p15, 0, r0, c7, c0, 0	@ invalidate whole cache v3
		mov	pc, lr

__armv4_mmu_cache_off:
#ifdef CONFIG_MMU
		mrc	p15, 0, r0, c1, c0
		bic	r0, r0, #0x000d
		mcr	p15, 0, r0, c1, c0	@ turn MMU and cache off
		mov	r0, #0
		mcr	p15, 0, r0, c7, c7	@ invalidate whole cache v4
		mcr	p15, 0, r0, c8, c7	@ invalidate whole TLB v4
#endif
		mov	pc, lr

__armv7_mmu_cache_off:
		mrc	p15, 0, r0, c1, c0
#ifdef CONFIG_MMU
		bic	r0, r0, #0x000d
#else
		bic	r0, r0, #0x000c
#endif
		mcr	p15, 0, r0, c1, c0	@ turn MMU and cache off
		mov	r12, lr
		bl	__armv7_mmu_cache_flush
		mov	r0, #0
#ifdef CONFIG_MMU
		mcr	p15, 0, r0, c8, c7, 0	@ invalidate whole TLB
#endif
		mcr	p15, 0, r0, c7, c5, 6	@ invalidate BTC
		mcr	p15, 0, r0, c7, c10, 4	@ DSB
		mcr	p15, 0, r0, c7, c5, 4	@ ISB
		mov	pc, r12

/*
 * Clean and flush the cache to maintain consistency.
 *
 * On exit,
 *  r1, r2, r3, r9, r10, r11, r12 corrupted
 * This routine must preserve:
 *  r4, r6, r7, r8
 */
		.align	5
cache_clean_flush:
		mov	r3, #16
		b	call_cache_fn

__armv4_mpu_cache_flush:
		tst	r4, #1
		movne	pc, lr
		mov	r2, #1
		mov	r3, #0
		mcr	p15, 0, ip, c7, c6, 0	@ invalidate D cache
		mov	r1, #7 << 5		@ 8 segments
1:		orr	r3, r1, #63 << 26	@ 64 entries
2:		mcr	p15, 0, r3, c7, c14, 2	@ clean & invalidate D index
		subs	r3, r3, #1 << 26
		bcs	2b			@ entries 63 to 0
		subs 	r1, r1, #1 << 5
		bcs	1b			@ segments 7 to 0

		teq	r2, #0
		mcrne	p15, 0, ip, c7, c5, 0	@ invalidate I cache
		mcr	p15, 0, ip, c7, c10, 4	@ drain WB
		mov	pc, lr
		
__fa526_cache_flush:
		tst	r4, #1
		movne	pc, lr
		mov	r1, #0
		mcr	p15, 0, r1, c7, c14, 0	@ clean and invalidate D cache
		mcr	p15, 0, r1, c7, c5, 0	@ flush I cache
		mcr	p15, 0, r1, c7, c10, 4	@ drain WB
		mov	pc, lr

__armv6_mmu_cache_flush:
		mov	r1, #0
		tst	r4, #1
		mcreq	p15, 0, r1, c7, c14, 0	@ clean+invalidate D
		mcr	p15, 0, r1, c7, c5, 0	@ invalidate I+BTB
		mcreq	p15, 0, r1, c7, c15, 0	@ clean+invalidate unified
		mcr	p15, 0, r1, c7, c10, 4	@ drain WB
		mov	pc, lr

__armv7_mmu_cache_flush:
		tst	r4, #1
		bne	iflush
		mrc	p15, 0, r10, c0, c1, 5	@ read ID_MMFR1
		tst	r10, #0xf << 16		@ hierarchical cache (ARMv7)
		mov	r10, #0
		beq	hierarchical
		mcr	p15, 0, r10, c7, c14, 0	@ clean+invalidate D
		b	iflush
hierarchical:
		mcr	p15, 0, r10, c7, c10, 5	@ DMB
		stmfd	sp!, {r0-r7, r9-r11}
		mrc	p15, 1, r0, c0, c0, 1	@ read clidr
		ands	r3, r0, #0x7000000	@ extract loc from clidr
		mov	r3, r3, lsr #23		@ left align loc bit field
		beq	finished		@ if loc is 0, then no need to clean
		mov	r10, #0			@ start clean at cache level 0
loop1:
		add	r2, r10, r10, lsr #1	@ work out 3x current cache level
		mov	r1, r0, lsr r2		@ extract cache type bits from clidr
		and	r1, r1, #7		@ mask of the bits for current cache only
		cmp	r1, #2			@ see what cache we have at this level
		blt	skip			@ skip if no cache, or just i-cache
		mcr	p15, 2, r10, c0, c0, 0	@ select current cache level in cssr
		mcr	p15, 0, r10, c7, c5, 4	@ isb to sych the new cssr&csidr
		mrc	p15, 1, r1, c0, c0, 0	@ read the new csidr
		and	r2, r1, #7		@ extract the length of the cache lines
		add	r2, r2, #4		@ add 4 (line length offset)
		ldr	r4, =0x3ff
		ands	r4, r4, r1, lsr #3	@ find maximum number on the way size
		clz	r5, r4			@ find bit position of way size increment
		ldr	r7, =0x7fff
		ands	r7, r7, r1, lsr #13	@ extract max number of the index size
loop2:
		mov	r9, r4			@ create working copy of max way size
loop3:
 ARM(		orr	r11, r10, r9, lsl r5	) @ factor way and cache number into r11
 ARM(		orr	r11, r11, r7, lsl r2	) @ factor index number into r11
 THUMB(		lsl	r6, r9, r5		)
 THUMB(		orr	r11, r10, r6		) @ factor way and cache number into r11
 THUMB(		lsl	r6, r7, r2		)
 THUMB(		orr	r11, r11, r6		) @ factor index number into r11
		mcr	p15, 0, r11, c7, c14, 2	@ clean & invalidate by set/way
		subs	r9, r9, #1		@ decrement the way
		bge	loop3
		subs	r7, r7, #1		@ decrement the index
		bge	loop2
skip:
		add	r10, r10, #2		@ increment cache number
		cmp	r3, r10
		bgt	loop1
finished:
		ldmfd	sp!, {r0-r7, r9-r11}
		mov	r10, #0			@ swith back to cache level 0
		mcr	p15, 2, r10, c0, c0, 0	@ select current cache level in cssr
iflush:
		mcr	p15, 0, r10, c7, c10, 4	@ DSB
		mcr	p15, 0, r10, c7, c5, 0	@ invalidate I+BTB
		mcr	p15, 0, r10, c7, c10, 4	@ DSB
		mcr	p15, 0, r10, c7, c5, 4	@ ISB
		mov	pc, lr

__armv5tej_mmu_cache_flush:
		tst	r4, #1
		movne	pc, lr
1:		mrc	p15, 0, r15, c7, c14, 3	@ test,clean,invalidate D cache
		bne	1b
		mcr	p15, 0, r0, c7, c5, 0	@ flush I cache
		mcr	p15, 0, r0, c7, c10, 4	@ drain WB
		mov	pc, lr

__armv4_mmu_cache_flush:
		tst	r4, #1
		movne	pc, lr
		mov	r2, #64*1024		@ default: 32K dcache size (*2)
		mov	r11, #32		@ default: 32 byte line size
		mrc	p15, 0, r3, c0, c0, 1	@ read cache type
		teq	r3, r9			@ cache ID register present?
		beq	no_cache_id
		mov	r1, r3, lsr #18
		and	r1, r1, #7
		mov	r2, #1024
		mov	r2, r2, lsl r1		@ base dcache size *2
		tst	r3, #1 << 14		@ test M bit
		addne	r2, r2, r2, lsr #1	@ +1/2 size if M == 1
		mov	r3, r3, lsr #12
		and	r3, r3, #3
		mov	r11, #8
		mov	r11, r11, lsl r3	@ cache line size in bytes
no_cache_id:
		mov	r1, pc
		bic	r1, r1, #63		@ align to longest cache line
		add	r2, r1, r2
1:
 ARM(		ldr	r3, [r1], r11		) @ s/w flush D cache
 THUMB(		ldr     r3, [r1]		) @ s/w flush D cache
 THUMB(		add     r1, r1, r11		)
		teq	r1, r2
		bne	1b

		mcr	p15, 0, r1, c7, c5, 0	@ flush I cache
		mcr	p15, 0, r1, c7, c6, 0	@ flush D cache
		mcr	p15, 0, r1, c7, c10, 4	@ drain WB
		mov	pc, lr

__armv3_mmu_cache_flush:
__armv3_mpu_cache_flush:
		tst	r4, #1
		movne	pc, lr
		mov	r1, #0
		mcr	p15, 0, r1, c7, c0, 0	@ invalidate whole cache v3
		mov	pc, lr

/*
 * Various debugging routines for printing hex characters and
 * memory, which again must be relocatable.
 */
/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-05
 * ------------------------------------------------------------------
 *
 * TODO 
 *  .align <number> : 디폴트는 4byte이다.
 *  .type: phexbuf 는 #object이다. 
 *         #object는 데이터 형태
 *  .space: 예약된 공간 (12byte)
 *  .size:  현재 주소 - phexbuf 주소 = phexbuf 사이즈
 *
 * c-style: char phexbuf[12];
 *          int size = &phexbuf[12] - &phexbuf[0]
 *
 * ==================================================================
 */
#ifdef DEBUG
		.align	2
		.type	phexbuf,#object
phexbuf:	.space	12
		.size	phexbuf, . - phexbuf

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-12
 * ------------------------------------------------------------------
 *
 * 함수명: phex(put hexa?)
 * 설명: 10진수를 받아들여서 16진수로 변환함
 * 인자
 * 	r0: value,
 * 	r1: length (offset)
 *
 *		adr r3, phexbuf
 *			; r3 = &phexbuf
 *
 *		mov r2, #0
 *			; r2 = 0
 *
 *		strb r2, [r3, r1]
 *			; *(r3+r1) = r2;
 *
 *		subs r1, r1, #1
 *			; r1 = r1 - 1
 *			; subs의 s가 왠지 플래그값을 바꾸는것 같아서
 *			; 그 결과값을 가지고 이하의 명령어들을 참고합니다.
 *			; (r1값이 음수면 N플래그가 1로 설정된다고 가정)
 *		
 *		movmi r0, r3
 *			; r0 = ( r1 < 0 ) ? r3 : ;
 *		       	; movmi : r1안의 값이 마이너스면(음수면?) mov해라
 *			  (r0에 넣어라)
 *		
 *		bmi	puts
 *			; bmi(branch if minus?) r1값이 음수면 puts 호출한다
 *		
 *		and	r2, r0, #15
 *			; r2 = r0 & 0xF
 *		       	; and(AND연산?) : 0xF 마스크 연산을 하는거. 하위 비트 
 *			  잘라내기
 *		
 *		mov	r0, r0, lsr #4
 *			; r0 = r0 >> 4
 *			; lsr(logical shift right): 논리연산인데 unsigned 연산
 *			;			  : 참고로 asr는 signed 연산
 *			; 결국 4비트씩 자르는 과정
 *		
 *		cmp	r2, #10
 *			; cmp(compare?) : r2와 10을 비교
 *		
 *		addge r2, r2, #7
 *			; r2 = ( r2 >= 10 ) ? r2+7 : ;
 *			; addge(add if greater and equal to) 
 *			; 질문)r2 + 7 : 왜 7을 더해주나?
 *		
 *		add	r2, r2, #'0'
 *			; '0'은 ascii code로 48, 아까 위에서 연산한 r2에 48을 
 *			  더해서 결국 'A'로 변환해 줌
 *		
 *		strb	r2, [r3, r1]
 *			; *(r3+r1) = r2
 *		       	; strb(store byte?) 8비트값을 메모리에 저장
 *		
 *		b	1b
 *			; branch 1b (1b는 가장 가까운 레이블1로 back해라)
 *			; (참고로 1f라면 가장 가까운 레이블1로 forward해라)
 *	
 * ==================================================================
 */

@ phex corrupts {r0, r1, r2, r3}
phex:		adr	r3, phexbuf
		mov	r2, #0
		strb	r2, [r3, r1]
1:		subs	r1, r1, #1
		movmi	r0, r3
		bmi	puts
		and	r2, r0, #15
		mov	r0, r0, lsr #4
		cmp	r2, #10
		addge	r2, r2, #7
		add	r2, r2, #'0'
		strb	r2, [r3, r1]
		b	1b

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-05
 * ------------------------------------------------------------------
 * 1:
 * 		ldrb r2, [r0], #1
 *			;load register byte. r2 = r0 + 1
 *			;  참고: r0 = char *str. 즉, str + 1
 *
 * 2:
 * 		writeb r2, r3
 *			; r2(문자)를 r3(출력될 주소)로 보낸다.
 *
 * 		mov r1, #0x00020000
 *			; r1 = 0x00020000 
 *
 * 3:
 *		subs r1, r1, #1
 *			; r1 = r1 - 1
 *
 * 		bne 3b
 *			; for( r1 = 20000 ; r1 != 0; r-- )
 *			  puts, 문자 하나를 전송하고 약간의 딜레이 시간이
 *			  필요할 수도 있다.
 *			  TODO 20000 값이 baud rate 값과 같은지
 *
 *		teq r2, \n
 *			; teq : XOR 연산을 이용한 비교 연산
 *			  결과에 따라 n, z flag를 업데이트한다.
 *			    - n flag: 계산한 결과가 음수면 세팅됨
 *			    - z flag: 계산한 결과가 0일때, set됨
 *			  r2 == \n 이면, z flag가 세팅됨.
 *
 *			; puts!! null문자를 만날 때까지 반복
 *
 *		moveq r2, \r
 *			; if(r2 == '\n') r2 = '\r';
 *			  z flag가 세팅되어 있으면 mov 수행
 * 		beq 2b
 *			; z flag가 세팅되어 있으면 branch 수행
 *		...
 *
 * 		mov	pc, lr
 *			; pc = lr, 돌아갈 장소를 pc에 담는다.
 *
 * 			; 정리: \n은 \r 로 변경하고, NULL을 만날때까지, 
 *			  writeb를 수행한다. writeb 이후에, 약간의 딜레이를 
 *			  주어 받는쪽에서 처리를 완료할 수 있게 한다.
 *
 * ==================================================================
 */
 
@ puts corrupts {r0, r1, r2, r3}
puts:		loadsp	r3, r1
1:		ldrb	r2, [r0], #1
		teq	r2, #0
		moveq	pc, lr
2:		writeb	r2, r3
		mov	r1, #0x00020000
3:		subs	r1, r1, #1
		bne	3b
		teq	r2, #'\n'
		moveq	r2, #'\r'
		beq	2b
		teq	r0, #0
		bne	1b
		mov	pc, lr

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-05
 * ------------------------------------------------------------------
 *
 *		mov r2, r0
 *			; c-style: r2 = r0;
 *
 *		mov r0, #0
 *			; c-style: r0 = 0;
 *
 *		loadsp r3, r1
 *			; 문자 하나를 시리얼 포트로 보낸다. (디버그 모드일 때)
 *			  r3에 물리메모리 주소가 매핑된다.
 *			  r1은 tmp용으로 사용
 *		b  2b
 *			; puts의 2: 라벨로 브렌치, 여기서 문자가 쓰여진다.
 *
 * ==================================================================
 */

@ putc corrupts {r0, r1, r2, r3}
putc:
		mov	r2, r0
		mov	r0, #0
		loadsp	r3, r1
		b	2b

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-12
 * ------------------------------------------------------------------
 *
 * bl(branch after saving lr?) : 호출하기 전에 lr레지스터값에 다음 실행할 
 *				 pc값을 저장하고 브랜치함
 *
 *
 * memdump:	mov	r12, r0	
 *			; r0는 아까 zImage의 압축이 풀릴 시작주소를 r12로 복사
 *
 * 		mov	r10, lr	
 *			; lr(link register)를 r10에 복사
 * 		     	; 아래에서 bl로 분기가 생기니까 lr을 임시 저장
 *
 * 		mov	r11, #0
 *			; #0은 10진수 0. 이걸 r11에 복사
 *
 * 2: 		mov r0, r11, lsl #2
 *			; 레이블2 시작. (헤더주소 찍기)
 *			; 00000000: deadbeef deadbeef deadbeef ... 뭐 이런 느낌?
 *			; 참고) $xxd -l 256 sh 쉘을 256바이트 덤프하는 예
 *			; r0 = r11 << 2
 *			; lsl(logical shift left) lsl #2는 곱하기 4
 *
 * 		add	r0, r0, r12
 *			; r0 = r0 + r12
 *
 * 		mov	r1, #8
 *			; r1 = 8
 *
 * 		bl	phex
 *			; lr에 돌아올위치를 저장하고 phex로 브랜치(점프)
 *			; 인자로는 r0(val), r1(len)
 *			; 결국, r0부터 8 글자를 16진수로 찍기
 *
 * 		mov	r0, #':'
 *			; ": " 찍기
 *
 * 		bl	putc
 *
 * 1:		mov r0, #'
 *			; 레이블1 시작. (바디 64바이트 덤프)
 *
 * 		bl	putc
 *
 * 		ldr	r0, [r12, r11, lsl #2]	
 *			; ldr(load register. 32비트 값을 레지스터로 일어들이기) 
 *			; r0 = *(r12 + (r11 << 2))
 *			; []이게 나오면 주소의 의미
 *
 * 		mov	r1, #8
 *			; r1은 lenght
 *
 * 		bl	phex
 *			; 8개 찍기
 *
 * 		and	r0, r11, #7
 *			; r0 = r11 & 0x07 (00000111b)
 *
 * 		teq	r0, #3	
 *			; teq(test if equal). (r0 == 3)면 z플래그가 세운다
 *			; 여기서 3과 비교하는 이유는 4덩어리씩 끊어주기 위해서 
 *
 *			; 질문)
 *			; 근데 지금 몇 글자씩 찍고있나?
 *
 * 		moveq r0, #' '
 *			; z플래그가 서있으면 r0에 ' '를 복사한다
 *
 * 		bleq	putc
 *			; bleq: z플래그가 1이면 브랜치
 *
 * 		and	r0, r11, #7
 *			; r0 = r11 & 0x07 다시 마스킹하고
 *
 * 		add	r11, r11, #1
 *			; r11 = r11 + 1
 *
 * 		teq	r0, #7
 *			; r0와 7이 같은지 테스트. 같으면 z플래그가 서짐
 *			  (1로 세팅됨)
 *
 * 		bne	1b
 *			; 8(덩어리)이 아니라면 레이블1로 브랜치
 *
 * 		mov	r0, #'\n'
 *			; 8덩어리니까 new line을 찍는다
 *
 * 		bl	putc
 *
 * 		cmp	r11, #64
 *			; r11이 64와 비교(비교 방법은 그 다음 명령어를 참고)
 *
 * 		blt	2b
 *			; blt(branch if less than): r11이 그 대상(64)보다 
 *			  작으면 브랜치
 *
 * 		mov	pc, r10
 *			; 임시 저장했던 lr을 pc에 복사(결국 리턴)
 *
 * ==================================================================
 */
@ memdump corrupts {r0, r1, r2, r3, r10, r11, r12, lr}
memdump:	mov	r12, r0
		mov	r10, lr
		mov	r11, #0
2:		mov	r0, r11, lsl #2
		add	r0, r0, r12
		mov	r1, #8
		bl	phex
		mov	r0, #':'
		bl	putc
1:		mov	r0, #' '
		bl	putc
		ldr	r0, [r12, r11, lsl #2]
		mov	r1, #8
		bl	phex
		and	r0, r11, #7
		teq	r0, #3
		moveq	r0, #' '
		bleq	putc
		and	r0, r11, #7
		add	r11, r11, #1
		teq	r0, #7
		bne	1b
		mov	r0, #'\n'
		bl	putc
		cmp	r11, #64
		blt	2b
		mov	pc, r10
#endif

		.ltorg

#ifdef CONFIG_ARM_VIRT_EXT
.align 5
__hyp_reentry_vectors:
		W(b)	.			@ reset
		W(b)	.			@ undef
		W(b)	.			@ svc
		W(b)	.			@ pabort
		W(b)	.			@ dabort
		W(b)	__enter_kernel		@ hyp
		W(b)	.			@ irq
		W(b)	.			@ fiq
#endif /* CONFIG_ARM_VIRT_EXT */

__enter_kernel:
		mov	r0, #0			@ must be 0
 ARM(		mov	pc, r4		)	@ call kernel
 M_CLASS(	add	r4, r4, #1	)	@ enter in Thumb mode for M class
 THUMB(		bx	r4		)	@ entry point is always ARM for A/R classes

reloc_code_end:

		.align
		.section ".stack", "aw", %nobits
.L_user_stack:	.space	4096
.L_user_stack_end:
