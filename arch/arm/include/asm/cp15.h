#ifndef __ASM_ARM_CP15_H
#define __ASM_ARM_CP15_H

#include <asm/barrier.h>

/*
 * CR1 bits (CP#15 CR1)
 */
#define CR_M	(1 << 0)	/* MMU enable				*/
#define CR_A	(1 << 1)	/* Alignment abort enable		*/
#define CR_C	(1 << 2)	/* Dcache enable			*/
#define CR_W	(1 << 3)	/* Write buffer enable			*/
#define CR_P	(1 << 4)	/* 32-bit exception handler		*/
#define CR_D	(1 << 5)	/* 32-bit data address range		*/
#define CR_L	(1 << 6)	/* Implementation defined		*/
#define CR_B	(1 << 7)	/* Big endian				*/
#define CR_S	(1 << 8)	/* System MMU protection		*/
#define CR_R	(1 << 9)	/* ROM MMU protection			*/
#define CR_F	(1 << 10)	/* Implementation defined		*/
#define CR_Z	(1 << 11)	/* Implementation defined		*/
#define CR_I	(1 << 12)	/* Icache enable			*/
#define CR_V	(1 << 13)	/* Vectors relocated to 0xffff0000	*/
#define CR_RR	(1 << 14)	/* Round Robin cache replacement	*/
#define CR_L4	(1 << 15)	/* LDR pc can set T bit			*/
#define CR_DT	(1 << 16)
#ifdef CONFIG_MMU
#define CR_HA	(1 << 17)	/* Hardware management of Access Flag   */
#else
#define CR_BR	(1 << 17)	/* MPU Background region enable (PMSA)  */
#endif
#define CR_IT	(1 << 18)
#define CR_ST	(1 << 19)
#define CR_FI	(1 << 21)	/* Fast interrupt (lower latency mode)	*/
#define CR_U	(1 << 22)	/* Unaligned access operation		*/
#define CR_XP	(1 << 23)	/* Extended page tables			*/
#define CR_VE	(1 << 24)	/* Vectored interrupts			*/
#define CR_EE	(1 << 25)	/* Exception (Big) Endian		*/
#define CR_TRE	(1 << 28)	/* TEX(Type EXtension field) remap enable */
#define CR_AFE	(1 << 29)	/* Access flag enable			*/
#define CR_TE	(1 << 30)	/* Thumb exception enable		*/

#ifndef __ASSEMBLY__

#if __LINUX_ARM_ARCH__ >= 4
/* IAMROOT-12CD (2016-09-24):
 * --------------------------
 * cr(0x10c5387d & (1 << 13)) = 0x2000
 */
#define vectors_high()	(get_cr() & CR_V)
#else
#define vectors_high()	(0)
#endif

#ifdef CONFIG_CPU_CP15

extern unsigned long cr_alignment;	/* defined in entry-armv.S */

/* IAMROOT-12D (2016-06-09):
 * --------------------------
 * 라즈베리파이2 기본값 : 0x10c5387d
 * 
 * System control register (SCTLR)
 *  The SCTLR is another of a number of registers that are accessed using CP15,
 *  and controls standard memory, system facilities and provides status
 *  information for functions implemented in the core.
 * SCTLR는 CP15를 사용하여 액세스되는 레지스터의 중 하나 이며, 표준 메모리 시스
 * 템의 기능을 제어하고, 상기 코어에서 구현되는 기능에 대한 상태 정보를 제공한다
 *
 *    30                20            10           0
 *  1 0 9 8 7 6 5 4  2 1 0     4 3 2 1 0      3 2 1 0
 * +-+-+-+-+-+-+-+--+-+-+-------+-+-+-+--------+-+-+-+
 * | |T|A|T|N| |E|  |U|F|       |V|I|Z|        |C|A|M|
 * | |E|F|R|M| |E|  | |I|       | | | |        | | | |
 * +-+-+-+-+-+-+-+--+-+-+-------+-+-+-+--------+-+-+-+
 *
 * [30] TE – Thumb exception enable. This controls whether exceptions are taken
 *	 in ARM or Thumb state.
 * [29] AFE Access flag enable bit. This bit enables use of the AP[0] bit in the
	translation table descriptors as the Access flag.
 *  0 In the translation table descriptors, AP[0] is an access permissions bit.
 *	The full range of access permissions is supported. No Access flag is
 *	implemented. This is the reset value.
 *  1 In the translation table descrip
 *
 * [28] TRE TEX remap enable bit. This bit enables remapping of the TEX[2:1]
 *	bits for use as two translation table bits that can be managed by the
 *	operating system. Enabling this remapping also changes the scheme used
 *	to describe the memory region attributes in the VMSA:
 *  0 TEX remap disabled. TEX[2:0] are used, with the C and B bits, to describe
 *	the memory region attributes. This is the reset value.
 *  1 TEX remap enabled. TEX[2:1] are reassigned for use as bits managed by the
 *	operating system.  The TEX[0], C and B bits are used to describe the
 *	memory region attributes, with the MMU remap registers.
 *
 *  [27] NMFI(NM) – Non-maskable FIQ (NMFI) support.
 * 
 *  [25] EE Exception Endianness bit. The value of this bit defines the value of
 *	the CPSR.E bit on entry to an exception vector, including reset. This
 *	value also indicates the endianness of the translation table data for
 *	translation table
 *   lookups:
 *   0 Little endian.
 *   1 Big endian.
 *   The primary input CFGEND defines the reset value of the EE bit.
 *
 *  [22] U – Indicates use of the alignment model.
 *  [21] FI – FIQ configuration enable.
 *  - V – This bit selects the base address of the exception vector table.
 *  - I – Instruction cache enable bit.
 *  - Z – Branch prediction enable bit.
 *  - C – Cache enable bit.
 *  - A – Alignment check enable bit.
 *  - M – Enable the MMU.
 */
static inline unsigned long get_cr(void)
{
	unsigned long val;
	asm("mrc p15, 0, %0, c1, c0, 0	@ get CR" : "=r" (val) : : "cc");
	return val;
}

static inline void set_cr(unsigned long val)
{
	asm volatile("mcr p15, 0, %0, c1, c0, 0	@ set CR"
	  : : "r" (val) : "cc");
	isb();
}

static inline unsigned int get_auxcr(void)
{
	unsigned int val;
	asm("mrc p15, 0, %0, c1, c0, 1	@ get AUXCR" : "=r" (val));
	return val;
}

static inline void set_auxcr(unsigned int val)
{
	asm volatile("mcr p15, 0, %0, c1, c0, 1	@ set AUXCR"
	  : : "r" (val));
	isb();
}

#define CPACC_FULL(n)		(3 << (n * 2))
#define CPACC_SVC(n)		(1 << (n * 2))
#define CPACC_DISABLE(n)	(0 << (n * 2))

static inline unsigned int get_copro_access(void)
{
	unsigned int val;
	asm("mrc p15, 0, %0, c1, c0, 2 @ get copro access"
	  : "=r" (val) : : "cc");
	return val;
}

static inline void set_copro_access(unsigned int val)
{
	asm volatile("mcr p15, 0, %0, c1, c0, 2 @ set copro access"
	  : : "r" (val) : "cc");
	isb();
}

#else /* ifdef CONFIG_CPU_CP15 */

/*
 * cr_alignment is tightly coupled to cp15 (at least in the minds of the
 * developers). Yielding 0 for machines without a cp15 (and making it
 * read-only) is fine for most cases and saves quite some #ifdeffery.
 */
#define cr_alignment	UL(0)

static inline unsigned long get_cr(void)
{
	return 0;
}

#endif /* ifdef CONFIG_CPU_CP15 / else */

#endif /* ifndef __ASSEMBLY__ */

#endif
