/*
 * lib/debug_locks.c
 *
 * Generic place for common debugging facilities for various locks:
 * spinlocks, rwlocks, mutexes and rwsems.
 *
 * Started by Ingo Molnar:
 *
 *  Copyright (C) 2006 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 */
#include <linux/rwsem.h>
#include <linux/mutex.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/debug_locks.h>

/*
 * We want to turn all lock-debugging facilities on/off at once,
 * via a global flag. The reason is that once a single bug has been
 * detected and reported, there might be cascade of followup bugs
 * that would just muddy the log. So we report the first one and
 * shut up after that.
 */
/* IAMROOT-12D (2016-04-23):
 * --------------------------
 * 한번에 모든 debug관련한 lock를 off/on 시키는 전역 변수.
 * 1 : 로그 출력(debug_locks 가 걸려있지 않다.)
 * 0 : 로그 미출력(debug_locks가 걸려 있다.)
 *
 * 이 변수를 사용하므로써 첫번째 버그의 로그를 출력시키고 나머지 버그(첫번째 버
 * 그로 유발 될수 있는 버그 포함)는 닥치게 만들 목적이다.
 */
int debug_locks = 1;
EXPORT_SYMBOL_GPL(debug_locks);

/*
 * The locking-testsuite uses <debug_locks_silent> to get a
 * 'silent failure': nothing is printed to the console when
 * a locking bug is detected.
 */
int debug_locks_silent;
EXPORT_SYMBOL_GPL(debug_locks_silent);

/*
 * Generic 'turn off all lock debugging' function:
 */
/* IAMROOT-12D (2016-04-16):
 * --------------------------
 * debug_lock을 off(0으로 셋팅)시키고
 * 기존 debug_lock이 설정되어 있고 debug_locks_silent 가 설정 되어 있지 않으면
 * console_loglevel를 verbose로 설정.
 */
int debug_locks_off(void)
{
	if (__debug_locks_off()) {
		if (!debug_locks_silent) {
			console_verbose();
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(debug_locks_off);
