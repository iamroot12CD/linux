#ifndef _LINUX_NUMA_H
#define _LINUX_NUMA_H


#ifdef CONFIG_NODES_SHIFT
#define NODES_SHIFT     CONFIG_NODES_SHIFT
#else
#define NODES_SHIFT     0	/* IAMROOT-12CD: 라즈베리파이 2 */
#endif

/* IAMROOT-12CD (2016-07-02):
 * --------------------------
 * MAX_NUMNODES = 1;
 */
#define MAX_NUMNODES    (1 << NODES_SHIFT)

#define	NUMA_NO_NODE	(-1)

#endif /* _LINUX_NUMA_H */
