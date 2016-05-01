#ifndef __ASM_ARM_IRQFLAGS_H
#define __ASM_ARM_IRQFLAGS_H

#ifdef __KERNEL__

#include <asm/ptrace.h>

/*
 * CPU interrupt mask handling.
 */
#ifdef CONFIG_CPU_V7M
#define IRQMASK_REG_NAME_R "primask"
#define IRQMASK_REG_NAME_W "primask"
#define IRQMASK_I_BIT	1
#else
#define IRQMASK_REG_NAME_R "cpsr"
#define IRQMASK_REG_NAME_W "cpsr_c"
#define IRQMASK_I_BIT	PSR_I_BIT
#endif

#if __LINUX_ARM_ARCH__ >= 6

/* IAMROOT-12D (2016-04-16):
 * --------------------------
 * 현재 상태 레지스터를 flags에 저장하고 irq를 disable 시킨다.
 * @return	현재 상태 레지스터.
 */
static inline unsigned long arch_local_irq_save(void)
{
	unsigned long flags;

	/* IAMROOT-12D (2016-04-16):
	 * --------------------------
	 * mrs		flags, cpsr
	 * cpsid	i		@ irq 인터럽트를 금지한다.
	 *
	 * CPS (프로세서 상태 변경) 는 CPSR에서 하나 이상의 모드와 A, I 및 F
	 * 비트를 변경 하고 다른 CPSR 비트는 변경하지 않습니다.
	 *
	 * CPSeffect iflags{, #mode}
	 *  인수 설명
	 *  effect 다음 중 하나입니다.
	 *  	IE 인터럽트 또는 어보트 사용
	 *  	ID 인터럽트 또는 어보트 사용 안 함
	 *
	 *  iflags 다음 중 하나 이상의 시퀀스입니다.
	 *	a 부정확한 어보트를 사용하거나 사용하지 않습니다.
	 *	i IRQ 인터럽트를 사용하거나 사용하지 않습니다.
	 *	f FIQ 인터럽트를 사용하거나 사용하지 않습니다.
	 *
	 * CPSID i 는 CPS + ID + i
	 *  ID : 다음에 나오는 iflags를 사용 하지 않음
	 *  i : 인터럽트이므로
	 *  인터럽트를 사용하지 않게 셋팅
	 */
	asm volatile(
		"	mrs	%0, " IRQMASK_REG_NAME_R "	@ arch_local_irq_save\n"
		"	cpsid	i"
		: "=r" (flags) : : "memory", "cc");
	return flags;
}

static inline void arch_local_irq_enable(void)
{
	asm volatile(
		"	cpsie i			@ arch_local_irq_enable"
		:
		:
		: "memory", "cc");
}

static inline void arch_local_irq_disable(void)
{
	asm volatile(
		"	cpsid i			@ arch_local_irq_disable"
		:
		:
		: "memory", "cc");
}

#define local_fiq_enable()  __asm__("cpsie f	@ __stf" : : : "memory", "cc")
#define local_fiq_disable() __asm__("cpsid f	@ __clf" : : : "memory", "cc")
#else

/*
 * Save the current interrupt enable state & disable IRQs
 */
static inline unsigned long arch_local_irq_save(void)
{
	unsigned long flags, temp;

	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_save\n"
		"	orr	%1, %0, #128\n"
		"	msr	cpsr_c, %1"
		: "=r" (flags), "=r" (temp)
		:
		: "memory", "cc");
	return flags;
}

/*
 * Enable IRQs
 */
static inline void arch_local_irq_enable(void)
{
	unsigned long temp;
	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_enable\n"
		"	bic	%0, %0, #128\n"
		"	msr	cpsr_c, %0"
		: "=r" (temp)
		:
		: "memory", "cc");
}

/*
 * Disable IRQs
 */
static inline void arch_local_irq_disable(void)
{
	unsigned long temp;
	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_disable\n"
		"	orr	%0, %0, #128\n"
		"	msr	cpsr_c, %0"
		: "=r" (temp)
		:
		: "memory", "cc");
}

/*
 * Enable FIQs
 */
#define local_fiq_enable()					\
	({							\
		unsigned long temp;				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ stf\n"		\
"	bic	%0, %0, #64\n"					\
"	msr	cpsr_c, %0"					\
	: "=r" (temp)						\
	:							\
	: "memory", "cc");					\
	})

/*
 * Disable FIQs
 */
#define local_fiq_disable()					\
	({							\
		unsigned long temp;				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ clf\n"		\
"	orr	%0, %0, #64\n"					\
"	msr	cpsr_c, %0"					\
	: "=r" (temp)						\
	:							\
	: "memory", "cc");					\
	})

#endif

/*
 * Save the current interrupt enable state.
 */
/* IAMROOT-12D (2016-04-16):
 * --------------------------
 * mrs	flags, cpsr
 * 현재의 상태 플레그 값을 가져온다.
 */
static inline unsigned long arch_local_save_flags(void)
{
	unsigned long flags;
	asm volatile(
		"	mrs	%0, " IRQMASK_REG_NAME_R "	@ local_save_flags"
		: "=r" (flags) : : "memory", "cc");
	return flags;
}

/*
 * restore saved IRQ state
 */
static inline void arch_local_irq_restore(unsigned long flags)
{
	unsigned long temp = 0;
	flags &= ~(1 << 6);
	asm volatile (
		" mrs %0, cpsr"
		: "=r" (temp)
		:
		: "memory", "cc");
		/* Preserve FIQ bit */
		temp &= (1 << 6);
		flags = flags | temp;
	asm volatile (
		"    msr    cpsr_c, %0    @ local_irq_restore"
		:
		: "r" (flags)
		: "memory", "cc");
}

/* IAMROOT-12D (2016-04-16):
 * --------------------------
 * IRQ 가 disable인지 알아냄.
 * IRQMASK_I_BIT	0x00000080
 */
static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return flags & IRQMASK_I_BIT;
}

#endif /* ifdef __KERNEL__ */
#endif /* ifndef __ASM_ARM_IRQFLAGS_H */
