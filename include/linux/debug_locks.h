#ifndef __LINUX_DEBUG_LOCKING_H
#define __LINUX_DEBUG_LOCKING_H

#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/bug.h>

struct task_struct;

extern int debug_locks;
extern int debug_locks_silent;


/* IAMROOT-12D (2016-04-16):
 * --------------------------
 * debug_locks에 0을 설정하고 기존 debug_locks 값을 반환한다.
 */
static inline int __debug_locks_off(void)
{
	return xchg(&debug_locks, 0);
}

/*
 * Generic 'turn off all lock debugging' function:
 */
extern int debug_locks_off(void);

/* IAMROOT-12D (2016-04-16):
 * --------------------------
 * c 값이 true이면 WARN을 출력
 */
#define DEBUG_LOCKS_WARN_ON(c)						\
({									\
	int __ret = 0;							\
									\
	if (!oops_in_progress && unlikely(c)) {				\
		if (debug_locks_off() && !debug_locks_silent)		\
			WARN(1, "DEBUG_LOCKS_WARN_ON(%s)", #c);		\
		__ret = 1;						\
	}								\
	__ret;								\
})

#ifdef CONFIG_SMP
# define SMP_DEBUG_LOCKS_WARN_ON(c)			DEBUG_LOCKS_WARN_ON(c)
#else
# define SMP_DEBUG_LOCKS_WARN_ON(c)			do { } while (0)
#endif

#ifdef CONFIG_DEBUG_LOCKING_API_SELFTESTS
  extern void locking_selftest(void);
#else
# define locking_selftest()	do { } while (0)
#endif

struct task_struct;

#ifdef CONFIG_LOCKDEP
extern void debug_show_all_locks(void);
extern void debug_show_held_locks(struct task_struct *task);
extern void debug_check_no_locks_freed(const void *from, unsigned long len);
extern void debug_check_no_locks_held(void);
#else
static inline void debug_show_all_locks(void)
{
}

static inline void debug_show_held_locks(struct task_struct *task)
{
}

static inline void
debug_check_no_locks_freed(const void *from, unsigned long len)
{
}

static inline void
debug_check_no_locks_held(void)
{
}
#endif

#endif
