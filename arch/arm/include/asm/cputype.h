#ifndef __ASM_ARM_CPUTYPE_H
#define __ASM_ARM_CPUTYPE_H

#include <linux/stringify.h>
#include <linux/kernel.h>

#define CPUID_ID	0
#define CPUID_CACHETYPE	1
#define CPUID_TCM	2
#define CPUID_TLBTYPE	3
#define CPUID_MPUIR	4
#define CPUID_MPIDR	5
#define CPUID_REVIDR	6

#ifdef CONFIG_CPU_V7M
#define CPUID_EXT_PFR0	0x40
#define CPUID_EXT_PFR1	0x44
#define CPUID_EXT_DFR0	0x48
#define CPUID_EXT_AFR0	0x4c
#define CPUID_EXT_MMFR0	0x50
#define CPUID_EXT_MMFR1	0x54
#define CPUID_EXT_MMFR2	0x58
#define CPUID_EXT_MMFR3	0x5c
#define CPUID_EXT_ISAR0	0x60
#define CPUID_EXT_ISAR1	0x64
#define CPUID_EXT_ISAR2	0x68
#define CPUID_EXT_ISAR3	0x6c
#define CPUID_EXT_ISAR4	0x70
#define CPUID_EXT_ISAR5	0x74
#else
#define CPUID_EXT_PFR0	"c1, 0"
#define CPUID_EXT_PFR1	"c1, 1"
#define CPUID_EXT_DFR0	"c1, 2"
#define CPUID_EXT_AFR0	"c1, 3"
#define CPUID_EXT_MMFR0	"c1, 4"
#define CPUID_EXT_MMFR1	"c1, 5"
#define CPUID_EXT_MMFR2	"c1, 6"
#define CPUID_EXT_MMFR3	"c1, 7"
#define CPUID_EXT_ISAR0	"c2, 0"
#define CPUID_EXT_ISAR1	"c2, 1"
#define CPUID_EXT_ISAR2	"c2, 2"
#define CPUID_EXT_ISAR3	"c2, 3"
#define CPUID_EXT_ISAR4	"c2, 4"
#define CPUID_EXT_ISAR5	"c2, 5"
#endif

#define MPIDR_SMP_BITMASK (0x3 << 30)
#define MPIDR_SMP_VALUE (0x2 << 30)

#define MPIDR_MT_BITMASK (0x1 << 24)

#define MPIDR_HWID_BITMASK 0xFFFFFF

#define MPIDR_INVALID (~MPIDR_HWID_BITMASK)

#define MPIDR_LEVEL_BITS 8
/* IAMROOT-12D (2016-03-26):
 * --------------------------
 * ((1 << 8) - 1) = (0x100 - 1) = 0xFF 
 */
#define MPIDR_LEVEL_MASK ((1 << MPIDR_LEVEL_BITS) - 1)

/* IAMROOT-12D (2016-03-26):
 * --------------------------
 * ((0xF00 >> 0) & 0xFF) = 0x0 
 */
#define MPIDR_AFFINITY_LEVEL(mpidr, level) \
	((mpidr >> (MPIDR_LEVEL_BITS * level)) & MPIDR_LEVEL_MASK)

#define ARM_CPU_IMP_ARM			0x41
#define ARM_CPU_IMP_INTEL		0x69

/* ARM implemented processors */
#define ARM_CPU_PART_ARM1136		0x4100b360
#define ARM_CPU_PART_ARM1156		0x4100b560
#define ARM_CPU_PART_ARM1176		0x4100b760
#define ARM_CPU_PART_ARM11MPCORE	0x4100b020
#define ARM_CPU_PART_CORTEX_A8		0x4100c080
#define ARM_CPU_PART_CORTEX_A9		0x4100c090
#define ARM_CPU_PART_CORTEX_A5		0x4100c050
#define ARM_CPU_PART_CORTEX_A7		0x4100c070
#define ARM_CPU_PART_CORTEX_A12		0x4100c0d0
#define ARM_CPU_PART_CORTEX_A17		0x4100c0e0
#define ARM_CPU_PART_CORTEX_A15		0x4100c0f0
#define ARM_CPU_PART_MASK		0xff00fff0

#define ARM_CPU_XSCALE_ARCH_MASK	0xe000
#define ARM_CPU_XSCALE_ARCH_V1		0x2000
#define ARM_CPU_XSCALE_ARCH_V2		0x4000
#define ARM_CPU_XSCALE_ARCH_V3		0x6000

extern unsigned int processor_id;

/* IAMROOT-12D (2016-03-26):
 * --------------------------
 * 라즈베리파이2 의 경우
 * MRC p15, 0, r0, c0, c0, 0 ---> 0x410FC075
 * MRC p15, 0, r0, c0, c0, 5 ---> 0x80000F00 
 */
#ifdef CONFIG_CPU_CP15
#define read_cpuid(reg)							\
	({								\
		unsigned int __val;					\
		asm("mrc	p15, 0, %0, c0, c0, " __stringify(reg)	\
		    : "=r" (__val)					\
		    :							\
		    : "cc");						\
		__val;							\
	})

/*
 * The memory clobber prevents gcc 4.5 from reordering the mrc before
 * any is_smp() tests, which can cause undefined instruction aborts on
 * ARM1136 r0 due to the missing extended CP15 registers.
 */
/* IAMROOT-12CD (2016-08-23):
 * --------------------------
 * CPUID_EXT_MMFR0 "c1, 4"  ->  0x10201105
 *	mrc        p15, 0, %0, c0, c1, 4
 */
#define read_cpuid_ext(ext_reg)						\
	({								\
		unsigned int __val;					\
		asm("mrc	p15, 0, %0, c0, " ext_reg		\
		    : "=r" (__val)					\
		    :							\
		    : "memory");					\
		__val;							\
	})

#elif defined(CONFIG_CPU_V7M)

#include <asm/io.h>
#include <asm/v7m.h>

#define read_cpuid(reg)							\
	({								\
		WARN_ON_ONCE(1);					\
		0;							\
	})

static inline unsigned int __attribute_const__ read_cpuid_ext(unsigned offset)
{
	return readl(BASEADDR_V7M_SCB + offset);
}

#else /* ifdef CONFIG_CPU_CP15 / elif defined (CONFIG_CPU_V7M) */

/*
 * read_cpuid and read_cpuid_ext should only ever be called on machines that
 * have cp15 so warn on other usages.
 */
#define read_cpuid(reg)							\
	({								\
		WARN_ON_ONCE(1);					\
		0;							\
	})

#define read_cpuid_ext(reg) read_cpuid(reg)

#endif /* ifdef CONFIG_CPU_CP15 / else */

#ifdef CONFIG_CPU_CP15
/*
 * The CPU ID never changes at run time, so we might as well tell the
 * compiler that it's constant.  Use this function to read the CPU ID
 * rather than directly reading processor_id or read_cpuid() directly.
 */
/* IAMROOT-12D (2016-05-23):
 * --------------------------
 * 프로세서에 대한 정보를 포함하는 디바이스 ID 코드를 반환한다.
 *
 * Main ID Register format
 *
 *  31         24 23     20 19          16 15          4 3       0
 *  +------------+---------+--------------+-------------+---------+
 *  |Implementer | Variant | Architecture | Part number | revison |
 *  +------------+---------+--------------+-------------+---------+
 *
 * 라즈베리파이2 의 경우
 *  MRC p15, 0, r0, c0, c0, 0 ---> 0x410FC075
 *
 *	Implementer :	0x41 (ARM)
 *	Variant :	0x0	
 *	Architecture :	0xF	 (ARMv7)
 *	Part number :	0xc07(Cortex-A7 MPcore)
 *	revision :	0x5
 */
static inline unsigned int __attribute_const__ read_cpuid_id(void)
{
	return read_cpuid(CPUID_ID);
}

#elif defined(CONFIG_CPU_V7M)

static inline unsigned int __attribute_const__ read_cpuid_id(void)
{
	return readl(BASEADDR_V7M_SCB + V7M_SCB_CPUID);
}

#else /* ifdef CONFIG_CPU_CP15 / elif defined(CONFIG_CPU_V7M) */

static inline unsigned int __attribute_const__ read_cpuid_id(void)
{
	return processor_id;
}

#endif /* ifdef CONFIG_CPU_CP15 / else */

static inline unsigned int __attribute_const__ read_cpuid_implementor(void)
{
	return (read_cpuid_id() & 0xFF000000) >> 24;
}

/*
 * The CPU part number is meaningless without referring to the CPU
 * implementer: implementers are free to define their own part numbers
 * which are permitted to clash with other implementer part numbers.
 */
static inline unsigned int __attribute_const__ read_cpuid_part(void)
{
	return read_cpuid_id() & ARM_CPU_PART_MASK;
}

static inline unsigned int __attribute_const__ __deprecated read_cpuid_part_number(void)
{
	return read_cpuid_id() & 0xFFF0;
}

static inline unsigned int __attribute_const__ xscale_cpu_arch_version(void)
{
	return read_cpuid_id() & ARM_CPU_XSCALE_ARCH_MASK;
}

/* IAMROOT-12D (2016-05-25):
 * --------------------------
 * Read Cache Type Register
 *  아키택처의 캐쉬 정보를 제공
 *  MRC p15, 0, r0, c0, c0, 1 --> 0x84448003 
 *
 * 31 29  27   23  19   15 13          3  0
 * +---+-+----+----+----+--+----------+----+
 * |100|0|CWG |ERG |DL  |L |0000000000|IL  |
 * +---+-+----+----+----+--+----------+----+
 *
 * [31:29] Format :	Indicates the CTR format:
 *	0x4	ARMv7 format.
 *
 * [27:24] CWG : Cache Write-Back granule. Log 2 of the number of words of the
 *		maximum size of memory that can be overwritten as a result of
 *		the eviction of a cache entry that has had a memory location in
 *		it modified:
 *	0x4	Cache Write-Back granule size is 16 words.
 *
 * [23:20] ERG : Exclusives Reservation Granule. Log 2 of the number of words of
 *		the maximum size of the reservation granule that has been
 *		implemented for the Load-Exclusive and Store-Exclusive
 *		instructions:
 *	0x4	Exclusive reservation granule size is 16 words.
 *
 * [19:16] DminLine : Log 2 of the number of words in the smallest cache line of
 *		all the data and unified caches that the processor controls:
 *	0x4	Smallest data cache line size is 16 words.
 *
 * [15:14] L1Ip : L1 instruction cache policy. Indicates the indexing and
 *		tagging policy for the L1 instruction cache:
 *	0b10	Virtually Indexed Physically Tagged (VIPT).
 *
 * [3:0] IminLine : Log 2 of the number of words in the smallest cache line of
 *		all the instruction caches that the processor controls.
 *	0x03	Smallest instruction cache line size is 8 words.
 */
static inline unsigned int __attribute_const__ read_cpuid_cachetype(void)
{
	return read_cpuid(CPUID_CACHETYPE);
}

static inline unsigned int __attribute_const__ read_cpuid_tcmstatus(void)
{
	return read_cpuid(CPUID_TCM);
}

/* IAMROOT-12D (2016-03-26):
 * --------------------------
 * MPIDR 사용 목적 ?
 * 멀티 프로세서 시스템에서 스케줄링을 위해 추가 프로세서 식별 메커니즘을
 * 제공한다.
 * CPUID_MPIDR = 5
 * read_cpuid(CPUID_MPIDR) 결과값 = 0x80000F00 
 */
static inline unsigned int __attribute_const__ read_cpuid_mpidr(void)
{
	return read_cpuid(CPUID_MPIDR);
}

/*
 * Intel's XScale3 core supports some v6 features (supersections, L2)
 * but advertises itself as v5 as it does not support the v6 ISA.  For
 * this reason, we need a way to explicitly test for this type of CPU.
 */
#ifndef CONFIG_CPU_XSC3
#define cpu_is_xsc3()	0
#else
static inline int cpu_is_xsc3(void)
{
	unsigned int id;
	id = read_cpuid_id() & 0xffffe000;
	/* It covers both Intel ID and Marvell ID */
	if ((id == 0x69056000) || (id == 0x56056000))
		return 1;

	return 0;
}
#endif

#if !defined(CONFIG_CPU_XSCALE) && !defined(CONFIG_CPU_XSC3)
#define	cpu_is_xscale()	0
#else
#define	cpu_is_xscale()	1
#endif

/*
 * Marvell's PJ4 and PJ4B cores are based on V7 version,
 * but require a specical sequence for enabling coprocessors.
 * For this reason, we need a way to distinguish them.
 */
#if defined(CONFIG_CPU_PJ4) || defined(CONFIG_CPU_PJ4B)
static inline int cpu_is_pj4(void)
{
	unsigned int id;

	id = read_cpuid_id();
	if ((id & 0xff0fff00) == 0x560f5800)
		return 1;

	return 0;
}
#else
#define cpu_is_pj4()	0
#endif

static inline int __attribute_const__ cpuid_feature_extract_field(u32 features,
								  int field)
{
	int feature = (features >> field) & 15;

	/* feature registers are signed values */
	if (feature > 8)
		feature -= 16;

	return feature;
}

#define cpuid_feature_extract(reg, field) \
	cpuid_feature_extract_field(read_cpuid_ext(reg), field)

#endif
