/*
 * Broadcom BCM63xx low-level UART debug
 *
 * Copyright (C) 2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/serial_bcm63xx.h>

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-09-05
 * ------------------------------------------------------------------
 * ldr: Load register
 *
 * BCM2835: 라즈베리파이1
 * BCM2836: 라즈베리파이2
 * 
 * @see: linux/arch/arm/Kconfig.debug
 *
 * CONFIG_DEBUG_UART_PHYS:
 *   default 0x20201000 if DEBUG_BCM2835
 *
 * CONFIG_DEBUG_UART_VIRT:
 *   default 0xf0201000 if DEBUG_BCM2835
 *
 * 정리: rp 레지스터에는 UART 물리 메모리 주소를 매핑
 *       rv 레지스터에는 UART 가상 메모리 주소를 매핑
 * ==================================================================
 */
	.macro	addruart, rp, rv, tmp
	ldr	\rp, =CONFIG_DEBUG_UART_PHYS
	ldr	\rv, =CONFIG_DEBUG_UART_VIRT
	.endm

	.macro	senduart, rd, rx
	/* word access do not work */
	strb	\rd, [\rx, #UART_FIFO_REG]
	.endm

	.macro	waituart, rd, rx
1001:	ldr	\rd, [\rx, #UART_IR_REG]
	tst	\rd, #(1 << UART_IR_TXEMPTY)
	beq	1001b
	.endm

	.macro	busyuart, rd, rx
1002:	ldr	\rd, [\rx, #UART_IR_REG]
	tst	\rd, #(1 << UART_IR_TXTRESH)
	beq	1002b
	.endm
