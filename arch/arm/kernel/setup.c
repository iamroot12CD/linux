/*
 *  linux/arch/arm/kernel/setup.c
 *
 *  Copyright (C) 1995-2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/utsname.h>
#include <linux/initrd.h>
#include <linux/console.h>
#include <linux/bootmem.h>
#include <linux/seq_file.h>
#include <linux/screen_info.h>
#include <linux/of_iommu.h>
#include <linux/of_platform.h>
#include <linux/init.h>
#include <linux/kexec.h>
#include <linux/of_fdt.h>
#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/proc_fs.h>
#include <linux/memblock.h>
#include <linux/bug.h>
#include <linux/compiler.h>
#include <linux/sort.h>

#include <asm/unified.h>
#include <asm/cp15.h>
#include <asm/cpu.h>
#include <asm/cputype.h>
#include <asm/elf.h>
#include <asm/procinfo.h>
#include <asm/psci.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/smp_plat.h>
#include <asm/mach-types.h>
#include <asm/cacheflush.h>
#include <asm/cachetype.h>
#include <asm/tlbflush.h>

#include <asm/prom.h>
#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/mach/time.h>
#include <asm/system_info.h>
#include <asm/system_misc.h>
#include <asm/traps.h>
#include <asm/unwind.h>
#include <asm/memblock.h>
#include <asm/virt.h>

#include "atags.h"


#if defined(CONFIG_FPE_NWFPE) || defined(CONFIG_FPE_FASTFPE)
char fpe_type[8];

static int __init fpe_setup(char *line)
{
	memcpy(fpe_type, line, 8);
	return 1;
}

__setup("fpe=", fpe_setup);
#endif

extern void init_default_cache_policy(unsigned long);
extern void paging_init(const struct machine_desc *desc);
extern void early_paging_init(const struct machine_desc *,
			      struct proc_info_list *);
extern void sanity_check_meminfo(void);
extern enum reboot_mode reboot_mode;
extern void setup_dma_zone(const struct machine_desc *desc);

unsigned int processor_id;
EXPORT_SYMBOL(processor_id);
unsigned int __machine_arch_type __read_mostly;
EXPORT_SYMBOL(__machine_arch_type);
/* IAMROOT-12D (2016-05-25):
 * --------------------------
 * cacheid = CACHEID_VIPT_NONALIASING | CACHEID_VIPT_I_ALIASING  = 0x12
 *
 * L1 i-cache의 타입은 CACHEID_VIPT_I_ALIASING
 * cacheid에 d-cache + i-cache 플래그들의 특성을 담는다.
 * 	CACHEID_VIPT_NONALIASING(b1) | CACHEID_VIPT_I_ALIASING(b4)
 * 	cacheid = 0x12
 */
unsigned int cacheid __read_mostly;

EXPORT_SYMBOL(cacheid);

/* IAMROOT-12D (2016-06-11):
 * --------------------------
 * __atags_pointer 에는 atags 주소가 들어 가지만 dtb가 있을경우
 * dtb 주소가 들어간다. 이값은 0x8000????로 시작된다.
 */
unsigned int __atags_pointer __initdata;

unsigned int system_rev;
EXPORT_SYMBOL(system_rev);

unsigned int system_serial_low;
EXPORT_SYMBOL(system_serial_low);

unsigned int system_serial_high;
EXPORT_SYMBOL(system_serial_high);

/* IAMROOT-12D (2016-05-25):
 * --------------------------
 * elf_hwcap = HWCAP_HALF | HWCAP_THUMB | HWCAP_FAST_MULT |
 *		HWCAP_EDSP | HWCAP_TLS | HWCAP_IDIVA | HWCAP_LPAE;
 *	HWCAP_SWP 는 elf_hwcap_fixup() 함수에서 제거됨.
 * h/w capability
 */
unsigned int elf_hwcap __read_mostly;
EXPORT_SYMBOL(elf_hwcap);

unsigned int elf_hwcap2 __read_mostly;
EXPORT_SYMBOL(elf_hwcap2);


#ifdef MULTI_CPU
/* IAMROOT-12D (2016-05-25):
 * --------------------------
 *  - proc  :	process 초기화 관련 함수 목록(arch/arm/mm/proc-macros.S 참고
 *	v7_early_abort, v7_pabort, cpu_v7_proc_init, cpu_v7_proc_fin
 *	, cpu_v7_reset, cpu_v7_do_idle, cpu_v7_dcache_clean_area
 *	, cpu_v7_switch_mm, cpu_v7_set_pte_ext, cpu_v7_suspend_size등의 함수
 * 
 */
struct processor processor __read_mostly;
#endif
#ifdef MULTI_TLB
/* IAMROOT-12D (2016-05-25):
 * --------------------------
 *  - tlb   :	tlb table flush 관련 함수 목록 arch/arm/mm/tlb-v7.S 참고
 *	v7wbi_flush_kern_tlb_range, v7wbi_tlb_flags_smp, v7wbi_tlb_flags_up
 */
struct cpu_tlb_fns cpu_tlb __read_mostly;
#endif
#ifdef MULTI_USER
/* IAMROOT-12D (2016-05-25):
 * --------------------------
 * clear page, or copy page(??)
 * 	struct cpu_user_fns	*user = v6_user_fns;
 *
 * struct cpu_user_fns v6_user_fns __initdata = {
 * 	.cpu_clear_user_highpage = v6_clear_user_highpage_nonaliasing,
 * 	.cpu_copy_user_highpage	= v6_copy_user_highpage_nonaliasing,
};
 */
struct cpu_user_fns cpu_user __read_mostly;
#endif
#ifdef MULTI_CACHE
/* IAMROOT-12D (2016-05-25):
 * --------------------------
 * arch/arm/mm/cache-v7.S 참고
 *   v7_flush_icache_all, v7_flush_kern_cache_all, v7_flush_kern_cache_louis,
 *   v7_flush_user_cache_all, v7_flush_user_cache_range, v7_coherent_kern_range,
 *   v7_coherent_user_range, v7_flush_kern_dcache_area, v7_dma_map_area,
 *   v7_dma_unmap_area, v7_dma_flush_range
 */
struct cpu_cache_fns cpu_cache __read_mostly;
#endif
#ifdef CONFIG_OUTER_CACHE
struct outer_cache_fns outer_cache __read_mostly;
EXPORT_SYMBOL(outer_cache);
#endif

/*
 * Cached cpu_architecture() result for use by assembler code.
 * C code should use the cpu_architecture() function instead of accessing this
 * variable directly.
 */
/* IAMROOT-12D (2016-05-25):
 * --------------------------
 * 라즈베리파이2 는 
 * __cpu_architecture = CPU_ARCH_ARMv7; // 9
 */
int __cpu_architecture __read_mostly = CPU_ARCH_UNKNOWN;

struct stack {
	u32 irq[3];
	u32 abt[3];
	u32 und[3];
	u32 fiq[3];
} ____cacheline_aligned;

#ifndef CONFIG_CPU_V7M
static struct stack stacks[NR_CPUS];
#endif

char elf_platform[ELF_PLATFORM_SIZE];	/* IAMROOT-12D : "v7l" */
EXPORT_SYMBOL(elf_platform);

/* IAMROOT-12CD (2016-06-18):
 * --------------------------
 * cpu_name = "ARMv7 Processor";
 */
static const char *cpu_name;
/* IAMROOT-12D (2016-06-09):
 * --------------------------
 * machine_name = "BCM_2709"
 */
static const char *machine_name;
static char __initdata cmd_line[COMMAND_LINE_SIZE];
/* IAMROOT-12D (2016-06-09):
 * --------------------------
 * arch/arm/mach-bcm2709/bcm2709.c
 * 
 * static const struct machine_desc __mach_desc_BCM_2709
 * _used
 *  __attribute__((__section__(".arch.info.init"))) = {
 *		.nr     = MACH_TYPE_BCM_2709
 *		.name   = "BCM_2709",
 *		.smp		= smp_ops(bcm2709_smp_ops),
 *		.map_io = bcm2709_map_io,
 *		.init_irq = bcm2709_init_irq,
 *		.init_time = bcm2709_timer_init,
 *		.init_machine = bcm2709_init,
 *		.init_early = bcm2709_init_early,
 *		.reserve = board_reserve,
 *		.restart	= bcm2709_restart,
 *		.dt_compat = bcm2709_compat,
 * };
 */
const struct machine_desc *machine_desc __initdata;

static union { char c[4]; unsigned long l; } endian_test __initdata = { { 'l', '?', '?', 'b' } };
#define ENDIANNESS ((char)endian_test.l)

DEFINE_PER_CPU(struct cpuinfo_arm, cpu_data);

/*
 * Standard memory resources
 */
static struct resource mem_res[] = {
	{
		.name = "Video RAM",
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_MEM
	},
	{
		.name = "Kernel code",
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_MEM
	},
	{
		.name = "Kernel data",
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_MEM
	}
};

#define video_ram   mem_res[0]
#define kernel_code mem_res[1]
#define kernel_data mem_res[2]

static struct resource io_res[] = {
	{
		.name = "reserved",
		.start = 0x3bc,
		.end = 0x3be,
		.flags = IORESOURCE_IO | IORESOURCE_BUSY
	},
	{
		.name = "reserved",
		.start = 0x378,
		.end = 0x37f,
		.flags = IORESOURCE_IO | IORESOURCE_BUSY
	},
	{
		.name = "reserved",
		.start = 0x278,
		.end = 0x27f,
		.flags = IORESOURCE_IO | IORESOURCE_BUSY
	}
};

#define lp0 io_res[0]
#define lp1 io_res[1]
#define lp2 io_res[2]

static const char *proc_arch[] = {
	"undefined/unknown",
	"3",
	"4",
	"4T",
	"5",
	"5T",
	"5TE",
	"5TEJ",
	"6TEJ",
	"7",
	"7M",
	"?(12)",
	"?(13)",
	"?(14)",
	"?(15)",
	"?(16)",
	"?(17)",
};

#ifdef CONFIG_CPU_V7M
static int __get_cpu_architecture(void)
{
	return CPU_ARCH_ARMv7M;
}
#else
/* IAMROOT-12D (2016-05-24):
 * --------------------------
 * 라즈베리파이는 CPU_ARCH_ARMv7를 반환
 * #define CPU_ARCH_ARMv7		9
 */
static int __get_cpu_architecture(void)
{
	int cpu_arch;

	/* IAMROOT-12D (2016-05-24):
	 * --------------------------
	 *  라즈베리파이2
	 *  read_cpuid_id() = MRC p15, 0, r0, c0, c0, 0 ---> 0x410FC075
	 */
	if ((read_cpuid_id() & 0x0008f000) == 0) {
		cpu_arch = CPU_ARCH_UNKNOWN;
	} else if ((read_cpuid_id() & 0x0008f000) == 0x00007000) {
		cpu_arch = (read_cpuid_id() & (1 << 23)) ? CPU_ARCH_ARMv4T : CPU_ARCH_ARMv3;
	} else if ((read_cpuid_id() & 0x00080000) == 0x00000000) {
		cpu_arch = (read_cpuid_id() >> 16) & 7;
		if (cpu_arch)
		    cpu_arch += CPU_ARCH_ARMv3;
	} else if ((read_cpuid_id() & 0x000f0000) == 0x000f0000) {
	    /* Revised CPUID format. Read the Memory Model Feature
	     * Register 0 and check for VMSAv7 or PMSAv7 */
	    /* IAMROOT-12D (2016-05-21):
	     * --------------------------
	     * 우리 라즈베리파이에 해당하는 cpu_architecture이다.
	     * : CPU_ARCH_ARMv7
	     *
	     * #define CPUID_EXT_MMFR0	"c1, 4"
	     * MMFR0 :  Memory Model Feature Register 0
	     *	MRC p15, 0, r0, c0, c1, 4 --> 0x10101105
	     */
	    unsigned int mmfr0 = read_cpuid_ext(CPUID_EXT_MMFR0);
	    /* IAMROOT-12D (2016-05-24):
	     * --------------------------
	     * (mmfr0 & 0x0000000f) >= 0x00000003 : VMSA support 지원 여부 체크
	     * (mmfr0 & 0x000000f0) >= 0x00000030 : Outermost shareability 지원범위
	     *   Outermost (L2캐쉬 공유 여부)
	     */
	    if ((mmfr0 & 0x0000000f) >= 0x00000003 ||
		    (mmfr0 & 0x000000f0) >= 0x00000030)
		cpu_arch = CPU_ARCH_ARMv7;
	    else if ((mmfr0 & 0x0000000f) == 0x00000002 ||
		    (mmfr0 & 0x000000f0) == 0x00000020)
		cpu_arch = CPU_ARCH_ARMv6;
	    else
		cpu_arch = CPU_ARCH_UNKNOWN;
	} else
		cpu_arch = CPU_ARCH_UNKNOWN;

	return cpu_arch;
}
#endif

int __pure cpu_architecture(void)
{
	BUG_ON(__cpu_architecture == CPU_ARCH_UNKNOWN);

	return __cpu_architecture;
}

/* IAMROOT-12D (2016-05-28):
 * --------------------------
 * 라즈베리 파이2는 true를 반환함.
 */
/* IAMROOT-12A:
 * ------------
 * aliasing이 필요한 경우는 캐시 단면의 사이즈가 
 * 한 개의 페이지 사이즈(4K)를 초과하는 경우 aliasing이 필요
 */
static int cpu_has_aliasing_icache(unsigned int arch)
{
	int aliasing_icache;
	unsigned int id_reg, num_sets, line_size;

	/* PIPT caches never alias. */
	if (icache_is_pipt())
		return 0;

	/* arch specifies the register format */
	switch (arch) {
	case CPU_ARCH_ARMv7:
		/* IAMROOT-12D (2016-05-25):
		 * --------------------------
		 * CSSELR : Write Cache Size Selection Register
		 *  MCR p15, 2, r0, c0, c0, 0; 
		 *
		 * r0 = 1 : Select Instruction cache.
		 */
		asm("mcr	p15, 2, %0, c0, c0, 0 @ set CSSELR"
		    : /* No output operands */
		    : "r" (1));
		isb();
		/* IAMROOT-12D (2016-05-25):
		 * --------------------------
		 * CCSIDR : Cache Size ID Register
		 *  Provides information about the architecture of the caches.
		 *  라즈베리파이2
		 *	CCSIDR = LineSize = 0b001	
		 *  L1 Cache :	I-32K(TCM, VIPT) 2way, 32B cache line
		 *		D-32K(PIPT, 4way, 64B cache line
		 *	line_size = 32
		 *	num_sets = 0x200
		 *	aliasing_icache = (32 * 512) > 4096 (true)
		 */
		asm("mrc	p15, 1, %0, c0, c0, 0 @ read CCSIDR"
		    : "=r" (id_reg));
		line_size = 4 << ((id_reg & 0x7) + 2);
		num_sets = ((id_reg >> 13) & 0x7fff) + 1;
		/* IAMROOT-12A:
		 * ------------
		 * aliasing이 필요한 경우는 캐시 단면의 사이즈가 
		 * 한 개의 페이지 사이즈(4K)를 초과하는 경우 aliasing이 필요
		 */
		aliasing_icache = (line_size * num_sets) > PAGE_SIZE;
		break;
	case CPU_ARCH_ARMv6:
		aliasing_icache = read_cpuid_cachetype() & (1 << 11);
		break;
	default:
		/* I-cache aliases will be handled by D-cache aliasing code */
		aliasing_icache = 0;
	}

	return aliasing_icache;
}

/* IAMROOT-12A:
 * ------------
 * 전역 변수 cacheid에 L1 cache 타입 설정
 */
static void __init cacheid_init(void)
{
	/* IAMROOT-12D (2016-05-25):
	 * --------------------------
	 * CPU_ARCH_ARMv7; // 9
	 */
	unsigned int arch = cpu_architecture();

	if (arch == CPU_ARCH_ARMv7M) {
		cacheid = 0;
	} else if (arch >= CPU_ARCH_ARMv6) {
		/* IAMROOT-12D (2016-05-25):
		 * --------------------------
		 * cachetype = 0x84448003
		 * [31:29] Format :	Indicates the CTR format:
		 *	0x4	ARMv7 format.
		 *
		 * [15:14] L1Ip : L1 instruction cache policy. Indicates the
		 *		indexing and tagging policy for the L1
		 *		instruction cache:
		 *	0b10	Virtually Indexed Physically Tagged (VIPT).
		 */
		/* IAMROOT-12A:
		 * ------------
		 * cachetype = CTR(Cache Type Register)
		 * CTR.Format을 읽어 0x4 인경우 ARMv7이다.
		 *	- L1 d-cache를 CACHEID_VIPT_NONALIASING로 설정한다.
		 *	  (참고로 ARMv7의 실제 L1 d-cache 타입은 PIPT)
		 * CTR.L1IP(L1 instruction cache policy)
		 *	값이 2인 경우는 i-cache 관련하여 VIPT
		 */
		unsigned int cachetype = read_cpuid_cachetype();
		if ((cachetype & (7 << 29)) == 4 << 29) {
			/* ARMv7 register format */
			arch = CPU_ARCH_ARMv7;
			cacheid = CACHEID_VIPT_NONALIASING;
			switch (cachetype & (3 << 14)) {
			case (1 << 14):
				cacheid |= CACHEID_ASID_TAGGED;
				break;
			case (3 << 14):
				cacheid |= CACHEID_PIPT;
				break;
			}
		} else {
			arch = CPU_ARCH_ARMv6;
			if (cachetype & (1 << 23))
				cacheid = CACHEID_VIPT_ALIASING;
			else
				cacheid = CACHEID_VIPT_NONALIASING;
		}
		if (cpu_has_aliasing_icache(arch))
			cacheid |= CACHEID_VIPT_I_ALIASING;
	} else {
		cacheid = CACHEID_VIVT;
	}

	/* IAMROOT-12D (2016-05-25):
	 * --------------------------
	 * CPU: PIPT / VIPT nonaliasing data cache, VIPT aliasing instruction cache
	 */
	pr_info("CPU: %s data cache, %s instruction cache\n",
		cache_is_vivt() ? "VIVT" :
		cache_is_vipt_aliasing() ? "VIPT aliasing" :
		cache_is_vipt_nonaliasing() ? "PIPT / VIPT nonaliasing" : "unknown",
		cache_is_vivt() ? "VIVT" :
		icache_is_vivt_asid_tagged() ? "VIVT ASID tagged" :
		icache_is_vipt_aliasing() ? "VIPT aliasing" :
		icache_is_pipt() ? "PIPT" :
		cache_is_vipt_nonaliasing() ? "VIPT nonaliasing" : "unknown");
}

/*
 * These functions re-use the assembly code in head.S, which
 * already provide the required functionality.
 */
extern struct proc_info_list *lookup_processor_type(unsigned int);

void __init early_print(const char *str, ...)
{
	extern void printascii(const char *);
	char buf[256];
	va_list ap;

	va_start(ap, str);
	vsnprintf(buf, sizeof(buf), str, ap);
	va_end(ap);

#ifdef CONFIG_DEBUG_LL
	printascii(buf);
#endif
	printk("%s", buf);
}
/* IAMROOT-12D (2016-05-21):
 * --------------------------
 * TODO : isar5는 무엇인가?
 *  v8 Crypto 명령어 셋 정보이며 라즈베리파이는 지원하지 않음.
 * 
 * 반환 : elf_hwcap |= (HWCAP_IDIVA | HWCAP_LPAE)
 */
static void __init cpuid_init_hwcaps(void)
{
	int block;
	u32 isar5;

	if (cpu_architecture() < CPU_ARCH_ARMv7)
		return;

	/* IAMROOT-12D (2016-05-25):
	 * --------------------------
	 * Read Instruction Set Attribute Register 0
	 *	MRC p15, 0, r0, c0, c2, 0 --> 0x02101110
	 *  24~27번 비트는 나누기 명령어 셋 지원 여부를 가르키며 라즈베리파이2는
	 *  0x2 이며 SDIV and UDIV Thumb, ARM 명령어 셋을 지원한다.
	 */
	block = cpuid_feature_extract(CPUID_EXT_ISAR0, 24);
	if (block >= 2)
		elf_hwcap |= HWCAP_IDIVA;	/* IAMROOT-12D : 라즈베리 파이2 */
	if (block >= 1)
		elf_hwcap |= HWCAP_IDIVT;

	/* LPAE implies atomic ldrd/strd instructions */
	/* IAMROOT-12D (2016-05-25):
	 * --------------------------
	 * Read Memory Model Feature Register 0
	 *  MRC p15, 0, r0, c0, c1, 4 --> 0x10101105
	 *  0~3비트는 VMSA(Virtual Memory System Architecture)지원 여부를 나타낸
	 *  다.
	 */
	block = cpuid_feature_extract(CPUID_EXT_MMFR0, 0);
	if (block >= 5)
		elf_hwcap |= HWCAP_LPAE;	/* IAMROOT-12D : 라즈베리 파이2 */

	/* check for supported v8 Crypto instructions */
	/* IAMROOT-12D (2016-05-25):
	 * --------------------------
	 * ISAR5 Instruction Set Attribute Register 5
	 *  MRC p15, 0, r0, c0, c2, 5 --> 0x00000000
	 */
	isar5 = read_cpuid_ext(CPUID_EXT_ISAR5);

	block = cpuid_feature_extract_field(isar5, 4);
	if (block >= 2)
		elf_hwcap2 |= HWCAP2_PMULL;
	if (block >= 1)
		elf_hwcap2 |= HWCAP2_AES;

	block = cpuid_feature_extract_field(isar5, 8);
	if (block >= 1)
		elf_hwcap2 |= HWCAP2_SHA1;

	block = cpuid_feature_extract_field(isar5, 12);
	if (block >= 1)
		elf_hwcap2 |= HWCAP2_SHA2;

	block = cpuid_feature_extract_field(isar5, 16);
	if (block >= 1)
		elf_hwcap2 |= HWCAP2_CRC32;
}

/* IAMROOT-12D (2016-05-28):
 * --------------------------
 * 라즈베리파이2는 HWCAP_SWP 명령어를 제외한다.
 *  (ldrex, strex 명령어-atomic 메모리 접근 ARM 명령어-가 있기때문에)
 */
static void __init elf_hwcap_fixup(void)
{
	unsigned id = read_cpuid_id();	/* IAMROOT-12D : 0x410FC075 */

	/*
	 * HWCAP_TLS is available only on 1136 r1p0 and later,
	 * see also kuser_get_tls_init.
	 */
	/* IAMROOT-12A:
	 * ------------
	 * CPU 아키텍처가 ARM1136 r1p0 이상에서는 TLS 기능이 있다.
	 * 아래의 루틴같이 ARM1136 r1p0에서 MIDR.variant = 0인 경우 
	 * TLS 기능이 없다고 판단하면 뒤 루틴을 생략하고 빠져나간다. 
	 */
	/* IAMROOT-12D (2016-05-25):
	 * --------------------------
	 * read_cpuid_part() = 0x410FC075 & 0xff00fff0
	 * #define ARM_CPU_PART_ARM1136	0x4100b360
	 */
	if (read_cpuid_part() == ARM_CPU_PART_ARM1136 &&
	    ((id >> 20) & 3) == 0) {
		elf_hwcap &= ~HWCAP_TLS;
		return;
	}

	/* Verify if CPUID scheme is implemented */
	/* IAMROOT-12D (2016-05-25):
	 * --------------------------
	 * 0x410FC075 & 0x000f0000 
	 */
	if ((id & 0x000f0000) != 0x000f0000)
		return;

	/*
	 * If the CPU supports LDREX/STREX and LDREXB/STREXB,
	 * avoid advertising SWP; it may not be atomic with
	 * multiprocessing cores.
	 */
	/* IAMROOT-12D (2016-05-25):
	 * --------------------------
	 * ISAR3 : Instruction Set Attribute Register 3
	 * 프로세서가 기본적으로 제공하는 명령어 셋 정보를 제공한다.
	 *
	 *  MRC p15, 0, r0, c0, c2, 3 --> 0x11112131
	 *  [15:12] SynchPrim_instrs 동기화 명령어 셋 지원 여부(라즈베리파이2는
	 *	0x02로 ldrex, strex, clrex, ldrexb, ldrexh, strexb, strexh,
	 *	ldrexd, strexd 명령어를 지원 함)
	 *  [23:20] ThumbCopy_instrs thumb move 명령어 지원 여부.
	 */
	if (cpuid_feature_extract(CPUID_EXT_ISAR3, 12) > 1 ||
	    (cpuid_feature_extract(CPUID_EXT_ISAR3, 12) == 1 &&
	     cpuid_feature_extract(CPUID_EXT_ISAR3, 20) >= 3))
		elf_hwcap &= ~HWCAP_SWP;
}

/*
 * cpu_init - initialise one CPU.
 *
 * cpu_init sets up the per-CPU stacks.
 */
void notrace cpu_init(void)
{
#ifndef CONFIG_CPU_V7M
	unsigned int cpu = smp_processor_id();
	struct stack *stk = &stacks[cpu];

	if (cpu >= NR_CPUS) {
		pr_crit("CPU%u: bad primary CPU number\n", cpu);
		BUG();
	}

	/*
	 * This only works on resume and secondary cores. For booting on the
	 * boot cpu, smp_prepare_boot_cpu is called after percpu area setup.
	 */
	set_my_cpu_offset(per_cpu_offset(cpu));

	/* IAMROOT-12A:
	 * ------------
	 * 특정 CPU 아키텍처에서 필요한 코드를 실행한다.
	 *	v7 아케텍처에서는 아무것도 하지 않고 그냥 return 한다.
	 *
	 * 라즈베리파이2:
	 *	MULTI_CPU가 동작 중이어서 processor._proc_init()를 호출하는데 
	 *	이 변수에는 cpu_v7_proc_init의 주소가 담김 (../mm/proc-v7.S)
	 */
	/* IAMROOT-12D (2016-05-26):
	 * --------------------------
	 * cpu_proc_init 함수는 cpu_v7_proc_init를 호출하며
	 *  cpu_v7_proc_init 함수는 그냥 아무것도 하지 않고 리턴함
	 */
	cpu_proc_init();

	/*
	 * Define the placement constraint for the inline asm directive below.
	 * In Thumb-2, msr with an immediate value is not allowed.
	 */
#ifdef CONFIG_THUMB2_KERNEL
#define PLC	"r"
#else
#define PLC	"I"		/* IAMROOT-12D : 라즈베리파2 */
#endif

	/*
	 * setup stacks for re-entrant exception handlers
	 */
	/* IAMROOT-12D (2016-05-26):
	 * --------------------------
	 * 1. IRQ_MODE 스택설정
	 *    인터럽트 disable + IRQ_MODE 진입후 sp를 stack[0].irq[0]주소로 설정
	 * 2. ABT_MODE 스택설정
	 *    인터럽트 disable + ABT_MODE 진입후 sp를 stack.abt[0]주소로 설정
	 * 3. UND_MODE, FIQ_MODE 각각 sp 설정
	 * 4. SVC_MODE로 복귀.
	 */
	__asm__ (
	"msr	cpsr_c, %1\n\t"
	"add	r14, %0, %2\n\t"
	"mov	sp, r14\n\t"
	"msr	cpsr_c, %3\n\t"
	"add	r14, %0, %4\n\t"
	"mov	sp, r14\n\t"
	"msr	cpsr_c, %5\n\t"
	"add	r14, %0, %6\n\t"
	"mov	sp, r14\n\t"
	"msr	cpsr_c, %7\n\t"
	"add	r14, %0, %8\n\t"
	"mov	sp, r14\n\t"
	"msr	cpsr_c, %9"
	    :
	    : "r" (stk),
	      PLC (PSR_F_BIT | PSR_I_BIT | IRQ_MODE),
	      "I" (offsetof(struct stack, irq[0])),
	      PLC (PSR_F_BIT | PSR_I_BIT | ABT_MODE),
	      "I" (offsetof(struct stack, abt[0])),
	      PLC (PSR_F_BIT | PSR_I_BIT | UND_MODE),
	      "I" (offsetof(struct stack, und[0])),
	      PLC (PSR_F_BIT | PSR_I_BIT | FIQ_MODE),
	      "I" (offsetof(struct stack, fiq[0])),
	      PLC (PSR_F_BIT | PSR_I_BIT | SVC_MODE)
	    : "r14");
#endif
}

u32 __cpu_logical_map[NR_CPUS] = { [0 ... NR_CPUS-1] = MPIDR_INVALID };

/* IAMROOT-12D (2016-03-26):
 * --------------------------
 * is_smp() = 1
 * read_cpuid_mpidr() = 0x80000F00
 * MPIDR_HWID_BITMASK = 0x00FFFFFF
 * read_cpuid_mpidr() & MPIDR_HWID_BITMASK = 0x00000F00
 *
 * cpu = 0x0
 */
void __init smp_setup_processor_id(void)
{
	int i;
	u32 mpidr = is_smp() ? read_cpuid_mpidr() & MPIDR_HWID_BITMASK : 0;
	u32 cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);

	cpu_logical_map(0) = cpu;
	for (i = 1; i < nr_cpu_ids; ++i)
		cpu_logical_map(i) = i == cpu ? 0 : i;

	/*
	 * clear __my_cpu_offset on boot CPU to avoid hang caused by
	 * using percpu variable early, for example, lockdep will
	 * access percpu variable inside lock_release
	 */
	set_my_cpu_offset(0);

	/* IAMROOT-12D (2016-04-12):
	 * --------------------------
	 * 라즈베리파이 실제 로그(dmesg)
	 * 	[    0.000000] Booting Linux on physical CPU 0xf00
	 */
	pr_info("Booting Linux on physical CPU 0x%x\n", mpidr);
}

struct mpidr_hash mpidr_hash;
#ifdef CONFIG_SMP
/**
 * smp_build_mpidr_hash - Pre-compute shifts required at each affinity
 *			  level in order to build a linear index from an
 *			  MPIDR value. Resulting algorithm is a collision
 *			  free hash carried out through shifting and ORing
 */
static void __init smp_build_mpidr_hash(void)
{
	u32 i, affinity;
	u32 fs[3], bits[3], ls, mask = 0;
	/*
	 * Pre-scan the list of MPIDRS and filter out bits that do
	 * not contribute to affinity levels, ie they never toggle.
	 */
	for_each_possible_cpu(i)
		mask |= (cpu_logical_map(i) ^ cpu_logical_map(0));
	pr_debug("mask of set bits 0x%x\n", mask);
	/*
	 * Find and stash the last and first bit set at all affinity levels to
	 * check how many bits are required to represent them.
	 */
	for (i = 0; i < 3; i++) {
		affinity = MPIDR_AFFINITY_LEVEL(mask, i);
		/*
		 * Find the MSB bit and LSB bits position
		 * to determine how many bits are required
		 * to express the affinity level.
		 */
		ls = fls(affinity);
		fs[i] = affinity ? ffs(affinity) - 1 : 0;
		bits[i] = ls - fs[i];
	}
	/*
	 * An index can be created from the MPIDR by isolating the
	 * significant bits at each affinity level and by shifting
	 * them in order to compress the 24 bits values space to a
	 * compressed set of values. This is equivalent to hashing
	 * the MPIDR through shifting and ORing. It is a collision free
	 * hash though not minimal since some levels might contain a number
	 * of CPUs that is not an exact power of 2 and their bit
	 * representation might contain holes, eg MPIDR[7:0] = {0x2, 0x80}.
	 */
	mpidr_hash.shift_aff[0] = fs[0];
	mpidr_hash.shift_aff[1] = MPIDR_LEVEL_BITS + fs[1] - bits[0];
	mpidr_hash.shift_aff[2] = 2*MPIDR_LEVEL_BITS + fs[2] -
						(bits[1] + bits[0]);
	mpidr_hash.mask = mask;
	mpidr_hash.bits = bits[2] + bits[1] + bits[0];
	pr_debug("MPIDR hash: aff0[%u] aff1[%u] aff2[%u] mask[0x%x] bits[%u]\n",
				mpidr_hash.shift_aff[0],
				mpidr_hash.shift_aff[1],
				mpidr_hash.shift_aff[2],
				mpidr_hash.mask,
				mpidr_hash.bits);
	/*
	 * 4x is an arbitrary value used to warn on a hash table much bigger
	 * than expected on most systems.
	 */
	if (mpidr_hash_size() > 4 * num_possible_cpus())
		pr_warn("Large number of MPIDR hash buckets detected\n");
	sync_cache_w(&mpidr_hash);
}
#endif

/* IAMROOT-12D (2016-05-21):
 * --------------------------
 * Start_kernel 이전 시점에 우리는 proc_info_list정보를 모두 담아왔다.
 * 이미 담겨져 있는 정보를 우리는 lookup_processor_type()을 통해 가져온다.
 * --------------------------
 * 참고 사항 : arch/arm/mm/proc-v7.S
 * struct proc_info_list {
 * 	unsigned int	cpu_val = 0x000f0000;
 * 	unsigned int	cpu_mask = 0x000f0000;
 * 	unsigned long	__cpu_mm_mmu_flags = PMD_TYPE_SECT | PMD_SECT_AP_WRITE |
 *				PMD_SECT_AP_READ | PMD_SECT_AF | PMD_FLAGS_SMP;
 * 	unsigned long	__cpu_io_mmu_flags = PMD_TYPE_SECT | PMD_SECT_AP_WRITE |
 *				PMD_SECT_AP_READ | PMD_SECT_AF;
 * 	unsigned long	__cpu_flush = __v7_setup;	// arch/arm/mm/proc-v7.S
 * 	const char		*arch_name = "armv7";
 * 	const char		*elf_name = "v7";
 * 	unsigned int	elf_hwcap = HWCAP_SWP | HWCAP_HALF | HWCAP_THUMB |
 *				HWCAP_FAST_MULT | HWCAP_EDSP | HWCAP_TLS;
 * 	const char		*cpu_name = "ARMv7 Processor";
 * 	struct processor	*proc = v7_processor_functions;
 * 	struct cpu_tlb_fns	*tlb = v7wbi_tlb_fns;
 * 	struct cpu_user_fns	*user = v6_user_fns;
 * 	struct cpu_cache_fns	*cache = v7_cache_fns;
 * };
 */
static void __init setup_processor(void)
{
	struct proc_info_list *list;

	/*
	 * locate processor in the list of supported processor
	 * types.  The linker builds this table for us from the
	 * entries in arch/arm/mm/proc-*.S
	 */
	list = lookup_processor_type(read_cpuid_id());
	if (!list) {
		pr_err("CPU configuration botched (ID %08x), unable to continue.\n",
		       read_cpuid_id());
		while (1);
	}

	/* IAMROOT-12D (2016-05-24):
	 * --------------------------
	 * 	cpu_name = "ARMv7 Processor";
	 *	__cpu_architecture = CPU_ARCH_ARMv7
	 */
	cpu_name = list->cpu_name;
	__cpu_architecture = __get_cpu_architecture();

	/* IAMROOT-12D (2016-05-21):
	 * --------------------------
	 * - proc: process 초기화 관련 함수 목록(arch/arm/mm/proc-macros.S 참고)
	 *	v7_early_abort, v7_pabort, cpu_v7_proc_init, cpu_v7_proc_fin
	 *	, cpu_v7_reset, cpu_v7_do_idle, cpu_v7_dcache_clean_area
	 *	, cpu_v7_switch_mm, cpu_v7_set_pte_ext, cpu_v7_suspend_size등의 함수
	 * - tlb: tlb table flush 관련 함수 목록 arch/arm/mm/tlb-v7.S 참고
	 *     	v7wbi_flush_kern_tlb_range, v7wbi_tlb_flags_smp, v7wbi_tlb_flags_up
	 * - user: 사용자 메모리 할당과 해제(?)
	 * - cache: cache 정책 함수들.
	 * - hwcap:
	 */
#ifdef MULTI_CPU
	/* IAMROOT-12D (2016-05-24):
	 * --------------------------
	 * 	struct processor	*proc = v7_processor_functions;
	 */
	processor = *list->proc;
#endif
#ifdef MULTI_TLB
	/* IAMROOT-12D (2016-05-24):
	 * --------------------------
	 * 	struct cpu_tlb_fns	*tlb = v7wbi_tlb_fns;
	 */
	cpu_tlb = *list->tlb;
#endif
#ifdef MULTI_USER
	/* IAMROOT-12D (2016-05-24):
	 * --------------------------
	 * 	struct cpu_user_fns	*user = v6_user_fns;
	 */
	cpu_user = *list->user;
#endif
#ifdef MULTI_CACHE
	/* IAMROOT-12D (2016-05-24):
	 * --------------------------
	 * 	struct cpu_cache_fns	*cache = v7_cache_fns;
	 */
	cpu_cache = *list->cache;
#endif
	/* IAMROOT-12D (2016-05-21):
	 * --------------------------
	 * CPU: ARMv7 Processor [410fc075] revision 5 (ARMv7), cr=10c5387d
	 */
	pr_info("CPU: %s [%08x] revision %d (ARMv%s), cr=%08lx\n",
		cpu_name, read_cpuid_id(), read_cpuid_id() & 15,
		proc_arch[cpu_architecture()], get_cr());

	/* IAMROOT-12D (2016-05-25):
	 * --------------------------
	 * machine = "armv7l"
	 * elf_platform = "v7l"
	 */
	snprintf(init_utsname()->machine, __NEW_UTS_LEN + 1, "%s%c",
		list->arch_name, ENDIANNESS);
	snprintf(elf_platform, ELF_PLATFORM_SIZE, "%s%c",
		list->elf_name, ENDIANNESS);
	/* IAMROOT-12D (2016-05-25):
	 * --------------------------
	 * list->elf_hwcap = HWCAP_SWP | HWCAP_HALF | HWCAP_THUMB |
	 *		HWCAP_FAST_MULT | HWCAP_EDSP | HWCAP_TLS;
	 * h/w capability
	 */
	elf_hwcap = list->elf_hwcap;

	cpuid_init_hwcaps();

#ifndef CONFIG_ARM_THUMB
	elf_hwcap &= ~(HWCAP_THUMB | HWCAP_IDIVT);
#endif
#ifdef CONFIG_MMU
	init_default_cache_policy(list->__cpu_mm_mmu_flags);
#endif
	erratum_a15_798181_init();

	elf_hwcap_fixup();

	cacheid_init();
	cpu_init();
}

void __init dump_machine_table(void)
{
	const struct machine_desc *p;

	early_print("Available machine support:\n\nID (hex)\tNAME\n");
	for_each_machine_desc(p)
		early_print("%08x\t%s\n", p->nr, p->name);

	early_print("\nPlease check your kernel config and/or bootloader.\n");

	while (true)
		/* can't use cpu_relax() here as it may require MMU setup */;
}

int __init arm_add_memory(u64 start, u64 size)
{
	u64 aligned_start;

	/*
	 * Ensure that start/size are aligned to a page boundary.
	 * Size is rounded down, start is rounded up.
	 */
	aligned_start = PAGE_ALIGN(start);
	if (aligned_start > start + size)
		size = 0;
	else
		size -= aligned_start - start;

#ifndef CONFIG_ARCH_PHYS_ADDR_T_64BIT
	if (aligned_start > ULONG_MAX) {
		pr_crit("Ignoring memory at 0x%08llx outside 32-bit physical address space\n",
			(long long)start);
		return -EINVAL;
	}

	if (aligned_start + size > ULONG_MAX) {
		pr_crit("Truncating memory at 0x%08llx to fit in 32-bit physical address space\n",
			(long long)start);
		/*
		 * To ensure bank->start + bank->size is representable in
		 * 32 bits, we use ULONG_MAX as the upper limit rather than 4GB.
		 * This means we lose a page after masking.
		 */
		size = ULONG_MAX - aligned_start;
	}
#endif

	if (aligned_start < PHYS_OFFSET) {
		if (aligned_start + size <= PHYS_OFFSET) {
			pr_info("Ignoring memory below PHYS_OFFSET: 0x%08llx-0x%08llx\n",
				aligned_start, aligned_start + size);
			return -EINVAL;
		}

		pr_info("Ignoring memory below PHYS_OFFSET: 0x%08llx-0x%08llx\n",
			aligned_start, (u64)PHYS_OFFSET);

		size -= PHYS_OFFSET - aligned_start;
		aligned_start = PHYS_OFFSET;
	}

	start = aligned_start;
	size = size & ~(phys_addr_t)(PAGE_SIZE - 1);

	/*
	 * Check whether this memory region has non-zero size or
	 * invalid node number.
	 */
	if (size == 0)
		return -EINVAL;

	memblock_add(start, size);
	return 0;
}

/*
 * Pick out the memory size.  We look for mem=size@start,
 * where start and size are "size[KkMm]"
 */

static int __init early_mem(char *p)
{
	static int usermem __initdata = 0;
	u64 size;
	u64 start;
	char *endp;

	/*
	 * If the user specifies memory size, we
	 * blow away any automatically generated
	 * size.
	 */
	if (usermem == 0) {
		usermem = 1;
		memblock_remove(memblock_start_of_DRAM(),
			memblock_end_of_DRAM() - memblock_start_of_DRAM());
	}

	start = PHYS_OFFSET;
	size  = memparse(p, &endp);
	if (*endp == '@')
		start = memparse(endp + 1, NULL);

	arm_add_memory(start, size);

	return 0;
}
early_param("mem", early_mem);

static void __init request_standard_resources(const struct machine_desc *mdesc)
{
	struct memblock_region *region;
	struct resource *res;

	kernel_code.start   = virt_to_phys(_text);
	kernel_code.end     = virt_to_phys(_etext - 1);
	kernel_data.start   = virt_to_phys(_sdata);
	kernel_data.end     = virt_to_phys(_end - 1);

	for_each_memblock(memory, region) {
		res = memblock_virt_alloc(sizeof(*res), 0);
		res->name  = "System RAM";
		res->start = __pfn_to_phys(memblock_region_memory_base_pfn(region));
		res->end = __pfn_to_phys(memblock_region_memory_end_pfn(region)) - 1;
		res->flags = IORESOURCE_MEM | IORESOURCE_BUSY;

		request_resource(&iomem_resource, res);

		if (kernel_code.start >= res->start &&
		    kernel_code.end <= res->end)
			request_resource(res, &kernel_code);
		if (kernel_data.start >= res->start &&
		    kernel_data.end <= res->end)
			request_resource(res, &kernel_data);
	}

	if (mdesc->video_start) {
		video_ram.start = mdesc->video_start;
		video_ram.end   = mdesc->video_end;
		request_resource(&iomem_resource, &video_ram);
	}

	/*
	 * Some machines don't have the possibility of ever
	 * possessing lp0, lp1 or lp2
	 */
	if (mdesc->reserve_lp0)
		request_resource(&ioport_resource, &lp0);
	if (mdesc->reserve_lp1)
		request_resource(&ioport_resource, &lp1);
	if (mdesc->reserve_lp2)
		request_resource(&ioport_resource, &lp2);
}

#if defined(CONFIG_VGA_CONSOLE) || defined(CONFIG_DUMMY_CONSOLE)
struct screen_info screen_info = {
 .orig_video_lines	= 30,
 .orig_video_cols	= 80,
 .orig_video_mode	= 0,
 .orig_video_ega_bx	= 0,
 .orig_video_isVGA	= 1,
 .orig_video_points	= 8
};
#endif

static int __init customize_machine(void)
{
	/*
	 * customizes platform devices, or adds new ones
	 * On DT based machines, we fall back to populating the
	 * machine from the device tree, if no callback is provided,
	 * otherwise we would always need an init_machine callback.
	 */
	of_iommu_init();
	if (machine_desc->init_machine)
		machine_desc->init_machine();
#ifdef CONFIG_OF
	else
		of_platform_populate(NULL, of_default_bus_match_table,
					NULL, NULL);
#endif
	return 0;
}
arch_initcall(customize_machine);

static int __init init_machine_late(void)
{
	if (machine_desc->init_late)
		machine_desc->init_late();
	return 0;
}
late_initcall(init_machine_late);

#ifdef CONFIG_KEXEC
static inline unsigned long long get_total_mem(void)
{
	unsigned long total;

	total = max_low_pfn - min_low_pfn;
	return total << PAGE_SHIFT;
}

/**
 * reserve_crashkernel() - reserves memory are for crash kernel
 *
 * This function reserves memory area given in "crashkernel=" kernel command
 * line parameter. The memory reserved is used by a dump capture kernel when
 * primary kernel is crashing.
 */
static void __init reserve_crashkernel(void)
{
	unsigned long long crash_size, crash_base;
	unsigned long long total_mem;
	int ret;

	total_mem = get_total_mem();
	ret = parse_crashkernel(boot_command_line, total_mem,
				&crash_size, &crash_base);
	if (ret)
		return;

	ret = memblock_reserve(crash_base, crash_size);
	if (ret < 0) {
		pr_warn("crashkernel reservation failed - memory is in use (0x%lx)\n",
			(unsigned long)crash_base);
		return;
	}

	pr_info("Reserving %ldMB of memory at %ldMB for crashkernel (System RAM: %ldMB)\n",
		(unsigned long)(crash_size >> 20),
		(unsigned long)(crash_base >> 20),
		(unsigned long)(total_mem >> 20));

	crashk_res.start = crash_base;
	crashk_res.end = crash_base + crash_size - 1;
	insert_resource(&iomem_resource, &crashk_res);
}
#else
static inline void reserve_crashkernel(void) {}
#endif /* CONFIG_KEXEC */

void __init hyp_mode_check(void)
{
#ifdef CONFIG_ARM_VIRT_EXT
	sync_boot_mode();

	if (is_hyp_mode_available()) {
		pr_info("CPU: All CPU(s) started in HYP mode.\n");
		pr_info("CPU: Virtualization extensions available.\n");
	} else if (is_hyp_mode_mismatched()) {
		pr_warn("CPU: WARNING: CPU(s) started in wrong/inconsistent modes (primary CPU mode 0x%x)\n",
			__boot_cpu_mode & MODE_MASK);
		pr_warn("CPU: This may indicate a broken bootloader or firmware.\n");
	} else
		pr_info("CPU: All CPU(s) started in SVC mode.\n");
#endif
}

/* IAMROOT-12D (2016-05-21):
 * --------------------------
 * setup_processor()까지 함.
 */
void __init setup_arch(char **cmdline_p)
{
	const struct machine_desc *mdesc;

	setup_processor();
	/* IAMROOT-12A:
	 * ------------
	 * 처음에 DT_START_MACHINE에서 등록한 머신 디스크립터 테이블에서 이름으로 
	 * 검색해보고 없으면 START_MACHINE에서 등록한 머신 디스크립터 테이블에서
	 * 머신 번호로 검색한다.
	 *
	 * setup_machine_fdt()
	 *   - dtb로부터  boot_cmd_line과 memblock에 메모리 노드의 reg 영역을 추가.
	 */
	mdesc = setup_machine_fdt(__atags_pointer);
	if (!mdesc)
		mdesc = setup_machine_tags(__atags_pointer, __machine_arch_type);
	machine_desc = mdesc;

	/* IAMROOT-12D (2016-06-09):
	 * --------------------------
	 * machine_name   = "BCM_2709"
	 */
	machine_name = mdesc->name;
	dump_stack_set_arch_desc("%s", mdesc->name);

	/* IAMROOT-12D (2016-06-09):
	 * --------------------------
	 * default : REBOOT_COLD = 0,
	 */
	if (mdesc->reboot_mode != REBOOT_HARD)
		reboot_mode = mdesc->reboot_mode;

	init_mm.start_code = (unsigned long) _text;
	init_mm.end_code   = (unsigned long) _etext;
	init_mm.end_data   = (unsigned long) _edata;
	init_mm.brk	   = (unsigned long) _end;

	/* populate cmd_line too for later use, preserving boot_command_line */
	strlcpy(cmd_line, boot_command_line, COMMAND_LINE_SIZE);
	*cmdline_p = cmd_line;

	/* IAMROOT-12AB:
	 * -------------
	 * cmdline에서 입력된 모든 파라메터에 대응하는 early 함수를 찾아 호출한다.
	 * 일반 파라메터 함수 등록 매크로: __setup()        -> __setup_param(,,0)
	 * early 파라메터 함수 등록 매크로: __early_param() -> __setup_param(,,1)
	 * earlycon 파라메터 함수 등록 매크로: EARLYCON_DECLARE() -> __early_param() -> ..
	 *
	 * rpi2:
	 *	- setup_of_earlycon()
	 *	- pl011_early_console_setup()
	 *	- uart_setup_earlycon()
	 *	- uart8250_setup_earlycon()
	 */
	parse_early_param();

	early_paging_init(mdesc, lookup_processor_type(read_cpuid_id()));
	setup_dma_zone(mdesc);
	sanity_check_meminfo();
	arm_memblock_init(mdesc);

	paging_init(mdesc);
	request_standard_resources(mdesc);

	if (mdesc->restart)
		arm_pm_restart = mdesc->restart;

	unflatten_device_tree();

	arm_dt_init_cpu_maps();
	psci_init();
#ifdef CONFIG_SMP
	if (is_smp()) {
		if (!mdesc->smp_init || !mdesc->smp_init()) {
			if (psci_smp_available())
				smp_set_ops(&psci_smp_ops);
			else if (mdesc->smp)
				smp_set_ops(mdesc->smp);
		}
		smp_init_cpus();
		smp_build_mpidr_hash();
	}
#endif

	if (!is_smp())
		hyp_mode_check();

	reserve_crashkernel();

#ifdef CONFIG_MULTI_IRQ_HANDLER
	handle_arch_irq = mdesc->handle_irq;
#endif

#ifdef CONFIG_VT
#if defined(CONFIG_VGA_CONSOLE)
	conswitchp = &vga_con;
#elif defined(CONFIG_DUMMY_CONSOLE)
	conswitchp = &dummy_con;
#endif
#endif

	if (mdesc->init_early)
		mdesc->init_early();
}


static int __init topology_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct cpuinfo_arm *cpuinfo = &per_cpu(cpu_data, cpu);
		cpuinfo->cpu.hotpluggable = 1;
		register_cpu(&cpuinfo->cpu, cpu);
	}

	return 0;
}
subsys_initcall(topology_init);

#ifdef CONFIG_HAVE_PROC_CPU
static int __init proc_cpu_init(void)
{
	struct proc_dir_entry *res;

	res = proc_mkdir("cpu", NULL);
	if (!res)
		return -ENOMEM;
	return 0;
}
fs_initcall(proc_cpu_init);
#endif

static const char *hwcap_str[] = {
	"swp",
	"half",
	"thumb",
	"26bit",
	"fastmult",
	"fpa",
	"vfp",
	"edsp",
	"java",
	"iwmmxt",
	"crunch",
	"thumbee",
	"neon",
	"vfpv3",
	"vfpv3d16",
	"tls",
	"vfpv4",
	"idiva",
	"idivt",
	"vfpd32",
	"lpae",
	"evtstrm",
	NULL
};

static const char *hwcap2_str[] = {
	"aes",
	"pmull",
	"sha1",
	"sha2",
	"crc32",
	NULL
};

static int c_show(struct seq_file *m, void *v)
{
	int i, j;
	u32 cpuid;

	for_each_online_cpu(i) {
		/*
		 * glibc reads /proc/cpuinfo to determine the number of
		 * online processors, looking for lines beginning with
		 * "processor".  Give glibc what it expects.
		 */
		seq_printf(m, "processor\t: %d\n", i);
		cpuid = is_smp() ? per_cpu(cpu_data, i).cpuid : read_cpuid_id();
		seq_printf(m, "model name\t: %s rev %d (%s)\n",
			   cpu_name, cpuid & 15, elf_platform);

#if defined(CONFIG_SMP)
		seq_printf(m, "BogoMIPS\t: %lu.%02lu\n",
			   per_cpu(cpu_data, i).loops_per_jiffy / (500000UL/HZ),
			   (per_cpu(cpu_data, i).loops_per_jiffy / (5000UL/HZ)) % 100);
#else
		seq_printf(m, "BogoMIPS\t: %lu.%02lu\n",
			   loops_per_jiffy / (500000/HZ),
			   (loops_per_jiffy / (5000/HZ)) % 100);
#endif
		/* dump out the processor features */
		seq_puts(m, "Features\t: ");

		for (j = 0; hwcap_str[j]; j++)
			if (elf_hwcap & (1 << j))
				seq_printf(m, "%s ", hwcap_str[j]);

		for (j = 0; hwcap2_str[j]; j++)
			if (elf_hwcap2 & (1 << j))
				seq_printf(m, "%s ", hwcap2_str[j]);

		seq_printf(m, "\nCPU implementer\t: 0x%02x\n", cpuid >> 24);
		seq_printf(m, "CPU architecture: %s\n",
			   proc_arch[cpu_architecture()]);

		if ((cpuid & 0x0008f000) == 0x00000000) {
			/* pre-ARM7 */
			seq_printf(m, "CPU part\t: %07x\n", cpuid >> 4);
		} else {
			if ((cpuid & 0x0008f000) == 0x00007000) {
				/* ARM7 */
				seq_printf(m, "CPU variant\t: 0x%02x\n",
					   (cpuid >> 16) & 127);
			} else {
				/* post-ARM7 */
				seq_printf(m, "CPU variant\t: 0x%x\n",
					   (cpuid >> 20) & 15);
			}
			seq_printf(m, "CPU part\t: 0x%03x\n",
				   (cpuid >> 4) & 0xfff);
		}
		seq_printf(m, "CPU revision\t: %d\n\n", cpuid & 15);
	}

	seq_printf(m, "Hardware\t: %s\n", machine_name);
	seq_printf(m, "Revision\t: %04x\n", system_rev);
	seq_printf(m, "Serial\t\t: %08x%08x\n",
		   system_serial_high, system_serial_low);

	return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void c_stop(struct seq_file *m, void *v)
{
}

const struct seq_operations cpuinfo_op = {
	.start	= c_start,
	.next	= c_next,
	.stop	= c_stop,
	.show	= c_show
};
