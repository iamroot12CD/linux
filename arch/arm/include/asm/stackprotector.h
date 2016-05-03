/*
 * GCC stack protector support.
 *
 * Stack protector works by putting predefined pattern at the start of
 * the stack frame and verifying that it hasn't been overwritten when
 * returning from the function.  The pattern is called stack canary
 * and gcc expects it to be defined by a global variable called
 * "__stack_chk_guard" on ARM.  This unfortunately means that on SMP
 * we cannot have a different canary value per task.
 */

#ifndef _ASM_STACKPROTECTOR_H
#define _ASM_STACKPROTECTOR_H 1

#include <linux/random.h>
#include <linux/version.h>

extern unsigned long __stack_chk_guard;

/*
 * Initialize the stackprotector canary value.
 *
 * NOTE: this must only be called from functions that never return,
 * and it must always be inlined.
 */

/* IAMROOT-12D (2016-04-30):
 * --------------------------
 * 기본적인 sequence :
 * 	- 랜덤한 canary 값 생성
 * 	- 생성한 값을 현재 스텍의 canary 영역에 넣는다.
 * 	- 앞서 구한 canary 값을 global 변수인 __stack_chk_guard에 넣는다.
 * 	  이 global 변수로 stack overflow를 체크한다.
 * 	  다만, __stack_chk_guard는 처음에 지정한 값으로 고정되기에
 * 	  SMP상에서 프로세스 별로 서로 다른 canary값을 사용할 수 없다.
 */
static __always_inline void boot_init_stack_canary(void)
{
	unsigned long canary;

	/* Try to get a semi random initial value. */
	get_random_bytes(&canary, sizeof(canary));
	canary ^= LINUX_VERSION_CODE;

	current->stack_canary = canary;
	__stack_chk_guard = current->stack_canary;
}

#endif	/* _ASM_STACKPROTECTOR_H */
