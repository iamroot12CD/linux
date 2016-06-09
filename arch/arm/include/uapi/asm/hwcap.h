#ifndef _UAPI__ASMARM_HWCAP_H
#define _UAPI__ASMARM_HWCAP_H

/*
 * HWCAP flags - for elf_hwcap (in kernel) and AT_HWCAP
 */
#define HWCAP_SWP	(1 << 0)	/* IAMROOT-12D : SWP 명령어 지원 여부 */
#define HWCAP_HALF	(1 << 1)	/* IAMROOT-12D : 16bit 데이형 지원 */
#define HWCAP_THUMB	(1 << 2)	/* IAMROOT-12D : THUMB 명령어 지원 */
#define HWCAP_26BIT	(1 << 3)	/* Play it safe */
#define HWCAP_FAST_MULT	(1 << 4)	/* IAMROOT-12D : 빠른 곱셈 지원 */
#define HWCAP_FPA	(1 << 5)
#define HWCAP_VFP	(1 << 6)
#define HWCAP_EDSP	(1 << 7)	/* IAMROOT-12D : DSP extensions */
#define HWCAP_JAVA	(1 << 8)
#define HWCAP_IWMMXT	(1 << 9)
#define HWCAP_CRUNCH	(1 << 10)
#define HWCAP_THUMBEE	(1 << 11)
#define HWCAP_NEON	(1 << 12)
#define HWCAP_VFPv3	(1 << 13)
#define HWCAP_VFPv3D16	(1 << 14)	/* also set for VFPv4-D16 */
/* IAMROOT-12D (2016-05-28):
 * --------------------------
 * Thread Local Storage : Thread별로 다른 값을 가질수 있는 전역 변수
 */
#define HWCAP_TLS	(1 << 15)
#define HWCAP_VFPv4	(1 << 16)
#define HWCAP_IDIVA	(1 << 17)	/* IAMROOT-12D : SDIV,UDIV 명령어지원 */
#define HWCAP_IDIVT	(1 << 18)
#define HWCAP_VFPD32	(1 << 19)	/* set if VFP has 32 regs (not 16) */
#define HWCAP_IDIV	(HWCAP_IDIVA | HWCAP_IDIVT)
#define HWCAP_LPAE	(1 << 20)
#define HWCAP_EVTSTRM	(1 << 21)

/*
 * HWCAP2 flags - for elf_hwcap2 (in kernel) and AT_HWCAP2
 */
#define HWCAP2_AES	(1 << 0)
#define HWCAP2_PMULL	(1 << 1)
#define HWCAP2_SHA1	(1 << 2)
#define HWCAP2_SHA2	(1 << 3)
#define HWCAP2_CRC32	(1 << 4)

#endif /* _UAPI__ASMARM_HWCAP_H */
