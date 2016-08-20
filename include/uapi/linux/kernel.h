#ifndef _UAPI_LINUX_KERNEL_H
#define _UAPI_LINUX_KERNEL_H

#include <linux/sysinfo.h>

/*
 * 'kernel.h' contains some often-used function prototypes etc
 */
/* IAMROOT-12CD (2016-08-20):
 * --------------------------
 * x = 5M, a = 4M
 * (5M + (4M-1)) & ~(4M-1))
 * (9M-1) & ~(0x3f ffff) = (9M-1) & 0xffc00000 = 8M
 */
#define __ALIGN_KERNEL(x, a)		__ALIGN_KERNEL_MASK(x, (typeof(x))(a) - 1)
#define __ALIGN_KERNEL_MASK(x, mask)	(((x) + (mask)) & ~(mask))


#endif /* _UAPI_LINUX_KERNEL_H */
