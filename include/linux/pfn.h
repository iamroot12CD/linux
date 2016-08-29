#ifndef _LINUX_PFN_H_
#define _LINUX_PFN_H_

#ifndef __ASSEMBLY__
#include <linux/types.h>
#endif

/* IAMROOT-12CD (2016-08-27):
 * --------------------------
 * Page Frame Number (PFN)
 */
#define PFN_ALIGN(x)	(((unsigned long)(x) + (PAGE_SIZE - 1)) & PAGE_MASK)
#define PFN_UP(x)	(((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
/* IAMROOT-12CD (2016-08-22):
 * --------------------------
 * 메모리 주소 를 page 단위로 변경 (하위 12비트 제거)
 */
#define PFN_DOWN(x)	((x) >> PAGE_SHIFT)
#define PFN_PHYS(x)	((phys_addr_t)(x) << PAGE_SHIFT)

#endif
