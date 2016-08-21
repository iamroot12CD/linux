/*
 * Functions for working with the Flattened Device Tree data format
 *
 * Copyright 2009 Benjamin Herrenschmidt, IBM Corp
 * benh@kernel.crashing.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/crc32.h>
#include <linux/kernel.h>
#include <linux/initrd.h>
#include <linux/memblock.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/sizes.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/libfdt.h>
#include <linux/debugfs.h>
#include <linux/serial_core.h>
#include <linux/sysfs.h>

#include <asm/setup.h>  /* for COMMAND_LINE_SIZE */
#include <asm/page.h>

/*
 * of_fdt_limit_memory - limit the number of regions in the /memory node
 * @limit: maximum entries
 *
 * Adjust the flattened device tree to have at most 'limit' number of
 * memory entries in the /memory node. This function may be called
 * any time after initial_boot_param is set.
 */
void of_fdt_limit_memory(int limit)
{
	int memory;
	int len;
	const void *val;
	int nr_address_cells = OF_ROOT_NODE_ADDR_CELLS_DEFAULT;
	int nr_size_cells = OF_ROOT_NODE_SIZE_CELLS_DEFAULT;
	const uint32_t *addr_prop;
	const uint32_t *size_prop;
	int root_offset;
	int cell_size;

	root_offset = fdt_path_offset(initial_boot_params, "/");
	if (root_offset < 0)
		return;

	addr_prop = fdt_getprop(initial_boot_params, root_offset,
				"#address-cells", NULL);
	if (addr_prop)
		nr_address_cells = fdt32_to_cpu(*addr_prop);

	size_prop = fdt_getprop(initial_boot_params, root_offset,
				"#size-cells", NULL);
	if (size_prop)
		nr_size_cells = fdt32_to_cpu(*size_prop);

	cell_size = sizeof(uint32_t)*(nr_address_cells + nr_size_cells);

	memory = fdt_path_offset(initial_boot_params, "/memory");
	if (memory > 0) {
		val = fdt_getprop(initial_boot_params, memory, "reg", &len);
		if (len > limit*cell_size) {
			len = limit*cell_size;
			pr_debug("Limiting number of entries to %d\n", limit);
			fdt_setprop(initial_boot_params, memory, "reg", val,
					len);
		}
	}
}

/**
 * of_fdt_is_compatible - Return true if given node from the given blob has
 * compat in its compatible list
 * @blob: A device tree blob
 * @node: node to test
 * @compat: compatible string to compare with compatible list.
 *
 * On match, returns a non-zero value with smaller values returned for more
 * specific compatible values.
 */
int of_fdt_is_compatible(const void *blob,
		      unsigned long node, const char *compat)
{
	const char *cp;
	int cplen;
	unsigned long l, score = 0;

	cp = fdt_getprop(blob, node, "compatible", &cplen);
	if (cp == NULL)
		return 0;
	while (cplen > 0) {
		score++;
		if (of_compat_cmp(cp, compat, strlen(compat)) == 0)
			return score;
		l = strlen(cp) + 1;
		cp += l;
		cplen -= l;
	}

	return 0;
}

/**
 * of_fdt_is_big_endian - Return true if given node needs BE MMIO accesses
 * @blob: A device tree blob
 * @node: node to test
 *
 * Returns true if the node has a "big-endian" property, or if the kernel
 * was compiled for BE *and* the node has a "native-endian" property.
 * Returns false otherwise.
 */
bool of_fdt_is_big_endian(const void *blob, unsigned long node)
{
	if (fdt_getprop(blob, node, "big-endian", NULL))
		return true;
	if (IS_ENABLED(CONFIG_CPU_BIG_ENDIAN) &&
	    fdt_getprop(blob, node, "native-endian", NULL))
		return true;
	return false;
}

/**
 * of_fdt_match - Return true if node matches a list of compatible values
 */
int of_fdt_match(const void *blob, unsigned long node,
                 const char *const *compat)
{
	unsigned int tmp, score = 0;

	if (!compat)
		return 0;

	while (*compat) {
		tmp = of_fdt_is_compatible(blob, node, *compat);
		if (tmp && (score == 0 || (tmp < score)))
			score = tmp;
		compat++;
	}

	return score;
}

static void *unflatten_dt_alloc(void **mem, unsigned long size,
				       unsigned long align)
{
	void *res;

	*mem = PTR_ALIGN(*mem, align);
	res = *mem;
	*mem += size;

	return res;
}

/**
 * unflatten_dt_node - Alloc and populate a device_node from the flat tree
 * @blob: The parent device tree blob
 * @mem: Memory chunk to use for allocating device nodes and properties
 * @p: pointer to node in flat tree
 * @dad: Parent struct device_node
 * @fpsize: Size of the node path up at the current depth.
 */
static void * unflatten_dt_node(void *blob,
				void *mem,
				int *poffset,
				struct device_node *dad,
				struct device_node **nodepp,
				unsigned long fpsize,
				bool dryrun)
{
	const __be32 *p;
	struct device_node *np;
	struct property *pp, **prev_pp = NULL;
	const char *pathp;
	unsigned int l, allocl;
	static int depth = 0;
	int old_depth;
	int offset;
	int has_name = 0;
	int new_format = 0;

	pathp = fdt_get_name(blob, *poffset, &l);
	if (!pathp)
		return mem;

	allocl = ++l;

	/* version 0x10 has a more compact unit name here instead of the full
	 * path. we accumulate the full path size using "fpsize", we'll rebuild
	 * it later. We detect this because the first character of the name is
	 * not '/'.
	 */
	if ((*pathp) != '/') {
		new_format = 1;
		if (fpsize == 0) {
			/* root node: special case. fpsize accounts for path
			 * plus terminating zero. root node only has '/', so
			 * fpsize should be 2, but we want to avoid the first
			 * level nodes to have two '/' so we use fpsize 1 here
			 */
			fpsize = 1;
			allocl = 2;
			l = 1;
			pathp = "";
		} else {
			/* account for '/' and path size minus terminal 0
			 * already in 'l'
			 */
			fpsize += l;
			allocl = fpsize;
		}
	}

	np = unflatten_dt_alloc(&mem, sizeof(struct device_node) + allocl,
				__alignof__(struct device_node));
	if (!dryrun) {
		char *fn;
		of_node_init(np);
		np->full_name = fn = ((char *)np) + sizeof(*np);
		if (new_format) {
			/* rebuild full path for new format */
			if (dad && dad->parent) {
				strcpy(fn, dad->full_name);
#ifdef DEBUG
				if ((strlen(fn) + l + 1) != allocl) {
					pr_debug("%s: p: %d, l: %d, a: %d\n",
						pathp, (int)strlen(fn),
						l, allocl);
				}
#endif
				fn += strlen(fn);
			}
			*(fn++) = '/';
		}
		memcpy(fn, pathp, l);

		prev_pp = &np->properties;
		if (dad != NULL) {
			np->parent = dad;
			np->sibling = dad->child;
			dad->child = np;
		}
	}
	/* process properties */
	for (offset = fdt_first_property_offset(blob, *poffset);
	     (offset >= 0);
	     (offset = fdt_next_property_offset(blob, offset))) {
		const char *pname;
		u32 sz;

		if (!(p = fdt_getprop_by_offset(blob, offset, &pname, &sz))) {
			offset = -FDT_ERR_INTERNAL;
			break;
		}

		if (pname == NULL) {
			pr_info("Can't find property name in list !\n");
			break;
		}
		if (strcmp(pname, "name") == 0)
			has_name = 1;
		pp = unflatten_dt_alloc(&mem, sizeof(struct property),
					__alignof__(struct property));
		if (!dryrun) {
			/* We accept flattened tree phandles either in
			 * ePAPR-style "phandle" properties, or the
			 * legacy "linux,phandle" properties.  If both
			 * appear and have different values, things
			 * will get weird.  Don't do that. */
			if ((strcmp(pname, "phandle") == 0) ||
			    (strcmp(pname, "linux,phandle") == 0)) {
				if (np->phandle == 0)
					np->phandle = be32_to_cpup(p);
			}
			/* And we process the "ibm,phandle" property
			 * used in pSeries dynamic device tree
			 * stuff */
			if (strcmp(pname, "ibm,phandle") == 0)
				np->phandle = be32_to_cpup(p);
			pp->name = (char *)pname;
			pp->length = sz;
			pp->value = (__be32 *)p;
			*prev_pp = pp;
			prev_pp = &pp->next;
		}
	}
	/* with version 0x10 we may not have the name property, recreate
	 * it here from the unit name if absent
	 */
	if (!has_name) {
		const char *p1 = pathp, *ps = pathp, *pa = NULL;
		int sz;

		while (*p1) {
			if ((*p1) == '@')
				pa = p1;
			if ((*p1) == '/')
				ps = p1 + 1;
			p1++;
		}
		if (pa < ps)
			pa = p1;
		sz = (pa - ps) + 1;
		pp = unflatten_dt_alloc(&mem, sizeof(struct property) + sz,
					__alignof__(struct property));
		if (!dryrun) {
			pp->name = "name";
			pp->length = sz;
			pp->value = pp + 1;
			*prev_pp = pp;
			prev_pp = &pp->next;
			memcpy(pp->value, ps, sz - 1);
			((char *)pp->value)[sz - 1] = 0;
			pr_debug("fixed up name for %s -> %s\n", pathp,
				(char *)pp->value);
		}
	}
	if (!dryrun) {
		*prev_pp = NULL;
		np->name = of_get_property(np, "name", NULL);
		np->type = of_get_property(np, "device_type", NULL);

		if (!np->name)
			np->name = "<NULL>";
		if (!np->type)
			np->type = "<NULL>";
	}

	old_depth = depth;
	*poffset = fdt_next_node(blob, *poffset, &depth);
	if (depth < 0)
		depth = 0;
	while (*poffset > 0 && depth > old_depth)
		mem = unflatten_dt_node(blob, mem, poffset, np, NULL,
					fpsize, dryrun);

	if (*poffset < 0 && *poffset != -FDT_ERR_NOTFOUND)
		pr_err("unflatten: error %d processing FDT\n", *poffset);

	/*
	 * Reverse the child list. Some drivers assumes node order matches .dts
	 * node order
	 */
	if (!dryrun && np->child) {
		struct device_node *child = np->child;
		np->child = NULL;
		while (child) {
			struct device_node *next = child->sibling;
			child->sibling = np->child;
			np->child = child;
			child = next;
		}
	}

	if (nodepp)
		*nodepp = np;

	return mem;
}

/**
 * __unflatten_device_tree - create tree of device_nodes from flat blob
 *
 * unflattens a device-tree, creating the
 * tree of struct device_node. It also fills the "name" and "type"
 * pointers of the nodes so the normal device-tree walking functions
 * can be used.
 * @blob: The blob to expand
 * @mynodes: The device_node tree created by the call
 * @dt_alloc: An allocator that provides a virtual address to memory
 * for the resulting tree
 */
static void __unflatten_device_tree(void *blob,
			     struct device_node **mynodes,
			     void * (*dt_alloc)(u64 size, u64 align))
{
	unsigned long size;
	int start;
	void *mem;

	pr_debug(" -> unflatten_device_tree()\n");

	if (!blob) {
		pr_debug("No device tree pointer\n");
		return;
	}

	pr_debug("Unflattening device tree:\n");
	pr_debug("magic: %08x\n", fdt_magic(blob));
	pr_debug("size: %08x\n", fdt_totalsize(blob));
	pr_debug("version: %08x\n", fdt_version(blob));

	if (fdt_check_header(blob)) {
		pr_err("Invalid device tree blob header\n");
		return;
	}

	/* First pass, scan for size */
	start = 0;
	size = (unsigned long)unflatten_dt_node(blob, NULL, &start, NULL, NULL, 0, true);
	size = ALIGN(size, 4);

	pr_debug("  size is %lx, allocating...\n", size);

	/* Allocate memory for the expanded device tree */
	mem = dt_alloc(size + 4, __alignof__(struct device_node));
	memset(mem, 0, size);

	*(__be32 *)(mem + size) = cpu_to_be32(0xdeadbeef);

	pr_debug("  unflattening %p...\n", mem);

	/* Second pass, do actual unflattening */
	start = 0;
	unflatten_dt_node(blob, mem, &start, NULL, mynodes, 0, false);
	if (be32_to_cpup(mem + size) != 0xdeadbeef)
		pr_warning("End of tree marker overwritten: %08x\n",
			   be32_to_cpup(mem + size));

	pr_debug(" <- unflatten_device_tree()\n");
}

static void *kernel_tree_alloc(u64 size, u64 align)
{
	return kzalloc(size, GFP_KERNEL);
}

/**
 * of_fdt_unflatten_tree - create tree of device_nodes from flat blob
 *
 * unflattens the device-tree passed by the firmware, creating the
 * tree of struct device_node. It also fills the "name" and "type"
 * pointers of the nodes so the normal device-tree walking functions
 * can be used.
 */
void of_fdt_unflatten_tree(unsigned long *blob,
			struct device_node **mynodes)
{
	__unflatten_device_tree(blob, mynodes, &kernel_tree_alloc);
}
EXPORT_SYMBOL_GPL(of_fdt_unflatten_tree);

/* Everything below here references initial_boot_params directly. */
/* IAMROOT-12CD (2016-06-17):
 * --------------------------
 * dt_root_size_cells = \ { #address-cells }	= 1
 * dt_root_addr_cells = \ { #size-cells }	= 1
 *
 * arch/arm/boot/dts/skeleton.dtsi
 * / {
 * 	#address-cells = <1>;
 * 	#size-cells = <1>;
 *	...
 * };
 */
int __initdata dt_root_addr_cells;
int __initdata dt_root_size_cells;

/* IAMROOT-12D (2016-05-26):
 * --------------------------
 * Setup flat device-tree pointer
 * fdt 주소로 설정 되어 있다.(가상주소 0x8xxxxxxx 대역)
 */
void *initial_boot_params;

#ifdef CONFIG_OF_EARLY_FLATTREE

/* IAMROOT-12D (2016-05-26):
 * --------------------------
 * fdt 테이블 데이터 crc32값
 * CRC(Cyclic Redundancy Check)는 시리얼 전송에서 데이타의 신뢰성을 검증하기
 *	위한 에러 검출 방법의 일종이다.
 */
static u32 of_fdt_crc32;

/**
 * res_mem_reserve_reg() - reserve all memory described in 'reg' property
 */
static int __init __reserved_mem_reserve_reg(unsigned long node,
					     const char *uname)
{
	int t_len = (dt_root_addr_cells + dt_root_size_cells) * sizeof(__be32);
	phys_addr_t base, size;
	int len;
	const __be32 *prop;
	int nomap, first = 1;

	prop = of_get_flat_dt_prop(node, "reg", &len);
	if (!prop)
		return -ENOENT;

	if (len && len % t_len != 0) {
		pr_err("Reserved memory: invalid reg property in '%s', skipping node.\n",
		       uname);
		return -EINVAL;
	}

	nomap = of_get_flat_dt_prop(node, "no-map", NULL) != NULL;

	while (len >= t_len) {
		base = dt_mem_next_cell(dt_root_addr_cells, &prop);
		size = dt_mem_next_cell(dt_root_size_cells, &prop);

		if (size &&
		    early_init_dt_reserve_memory_arch(base, size, nomap) == 0)
			pr_debug("Reserved memory: reserved region for node '%s': base %pa, size %ld MiB\n",
				uname, &base, (unsigned long)size / SZ_1M);
		else
			pr_info("Reserved memory: failed to reserve memory for node '%s': base %pa, size %ld MiB\n",
				uname, &base, (unsigned long)size / SZ_1M);

		len -= t_len;
		if (first) {
			fdt_reserved_mem_save_node(node, uname, base, size);
			first = 0;
		}
	}
	return 0;
}

/**
 * __reserved_mem_check_root() - check if #size-cells, #address-cells provided
 * in /reserved-memory matches the values supported by the current implementation,
 * also check if ranges property has been provided
 */
static int __init __reserved_mem_check_root(unsigned long node)
{
	const __be32 *prop;

	prop = of_get_flat_dt_prop(node, "#size-cells", NULL);
	if (!prop || be32_to_cpup(prop) != dt_root_size_cells)
		return -EINVAL;

	prop = of_get_flat_dt_prop(node, "#address-cells", NULL);
	if (!prop || be32_to_cpup(prop) != dt_root_addr_cells)
		return -EINVAL;

	prop = of_get_flat_dt_prop(node, "ranges", NULL);
	if (!prop)
		return -EINVAL;
	return 0;
}

/**
 * fdt_scan_reserved_mem() - scan a single FDT node for reserved memory
 */
static int __init __fdt_scan_reserved_mem(unsigned long node, const char *uname,
					  int depth, void *data)
{
	static int found;
	const char *status;
	int err;

	if (!found && depth == 1 && strcmp(uname, "reserved-memory") == 0) {
		if (__reserved_mem_check_root(node) != 0) {
			pr_err("Reserved memory: unsupported node format, ignoring\n");
			/* break scan */
			return 1;
		}
		found = 1;
		/* scan next node */
		return 0;
	} else if (!found) {
		/* scan next node */
		return 0;
	} else if (found && depth < 2) {
		/* scanning of /reserved-memory has been finished */
		return 1;
	}

	status = of_get_flat_dt_prop(node, "status", NULL);
	if (status && strcmp(status, "okay") != 0 && strcmp(status, "ok") != 0)
		return 0;

	err = __reserved_mem_reserve_reg(node, uname);
	if (err == -ENOENT && of_get_flat_dt_prop(node, "size", NULL))
		fdt_reserved_mem_save_node(node, uname, 0, 0);

	/* scan next node */
	return 0;
}

/**
 * early_init_fdt_scan_reserved_mem() - create reserved memory regions
 *
 * This function grabs memory from early allocator for device exclusive use
 * defined in device tree structures. It should be called by arch specific code
 * once the early allocator (i.e. memblock) has been fully activated.
 */
void __init early_init_fdt_scan_reserved_mem(void)
{
	int n;
	u64 base, size;

	if (!initial_boot_params)
		return;

	/* Reserve the dtb region */
	/* IAMROOT-12CD (2016-08-20):
	 * --------------------------
	 * dtb 영역을 예약영역(memblock.reserved)에 설정한다.(추가한다)
	 */
	early_init_dt_reserve_memory_arch(__pa(initial_boot_params),
					  fdt_totalsize(initial_boot_params),
					  0);

	/* Process header /memreserve/ fields */
	for (n = 0; ; n++) {
		fdt_get_mem_rsv(initial_boot_params, n, &base, &size);
		if (!size)
			break;
		early_init_dt_reserve_memory_arch(base, size, 0);
	}

	/* IAMROOT-12CD (2016-08-20):
	 * --------------------------
	 * __fdt_scan_reserved_mem에서 아무것도 하지 않는다.
	 */
	of_scan_flat_dt(__fdt_scan_reserved_mem, NULL);

	/* IAMROOT-12CD (2016-08-20):
	 * --------------------------
	 * 아무것도 하지않음.
	 */
	fdt_init_reserved_mem();
}

/**
 * of_scan_flat_dt - scan flattened tree blob and call callback on each.
 * @it: callback function
 * @data: context data pointer
 *
 * This function is used to scan the flattened device-tree, it is
 * used to extract the memory information at boot before we can
 * unflatten the tree
 */
/* IAMROOT-12D (2016-06-11):
 * --------------------------
 * of_scan_flat_dt(early_init_dt_scan_chosen, boot_command_line);
 */
int __init of_scan_flat_dt(int (*it)(unsigned long node,
				     const char *uname, int depth,
				     void *data),
			   void *data)
{
	const void *blob = initial_boot_params;
	const char *pathp;
	int offset, rc = 0, depth = -1;

        for (offset = fdt_next_node(blob, -1, &depth);
             offset >= 0 && depth >= 0 && !rc;
             offset = fdt_next_node(blob, offset, &depth)) {

		pathp = fdt_get_name(blob, offset, NULL);
		if (*pathp == '/')
			pathp = kbasename(pathp);
		rc = it(offset, pathp, depth, data);
	}
	return rc;
}

/**
 * of_get_flat_dt_root - find the root node in the flat blob
 */
unsigned long __init of_get_flat_dt_root(void)
{
	return 0;
}

/**
 * of_get_flat_dt_size - Return the total size of the FDT
 */
int __init of_get_flat_dt_size(void)
{
	return fdt_totalsize(initial_boot_params);
}

/**
 * of_get_flat_dt_prop - Given a node in the flat blob, return the property ptr
 *
 * This function can be used within scan_flattened_dt callback to get
 * access to properties
 */
/* IAMROOT-12CD (2016-07-02):
 * --------------------------
 * const char *type = of_get_flat_dt_prop(node, "device_type", NULL);
 *	node = root node 어디쯤 offset..
 */
const void *__init of_get_flat_dt_prop(unsigned long node, const char *name,
				       int *size)
{
	return fdt_getprop(initial_boot_params, node, name, size);
}

/**
 * of_flat_dt_is_compatible - Return true if given node has compat in compatible list
 * @node: node to test
 * @compat: compatible string to compare with compatible list.
 */
int __init of_flat_dt_is_compatible(unsigned long node, const char *compat)
{
	return of_fdt_is_compatible(initial_boot_params, node, compat);
}

/**
 * of_flat_dt_match - Return true if node matches a list of compatible values
 */
int __init of_flat_dt_match(unsigned long node, const char *const *compat)
{
	return of_fdt_match(initial_boot_params, node, compat);
}

struct fdt_scan_status {
	const char *name;
	int namelen;
	int depth;
	int found;
	int (*iterator)(unsigned long node, const char *uname, int depth, void *data);
	void *data;
};

/* IAMROOT-12D (2016-06-09):
 * --------------------------
 * arch/arm/boot/dts/bcm2709-rpi-2-b.dts 참고
 * {	compatible = "brcm,bcm2709";
 *	model = "Raspberry Pi 2 Model B";
 * }
 */
const char * __init of_flat_dt_get_machine_name(void)
{
	const char *name;
	unsigned long dt_root = of_get_flat_dt_root();

	name = of_get_flat_dt_prop(dt_root, "model", NULL);
	if (!name)
		name = of_get_flat_dt_prop(dt_root, "compatible", NULL);
	return name;
}

/**
 * of_flat_dt_match_machine - Iterate match tables to find matching machine.
 *
 * @default_match: A machine specific ptr to return in case of no match.
 * @get_next_compat: callback function to return next compatible match table.
 *
 * Iterate through machine match tables to find the best match for the machine
 * compatible string in the FDT.
 */
/* IAMROOT-12D (2016-05-26):
 * --------------------------
 * setup_machine_fdt에서 호출 됨
 *  of_flat_dt_match_machine(mdesc_best=NULL, arch_get_next_mach);
 */
const void * __init of_flat_dt_match_machine(const void *default_match,
		const void * (*get_next_compat)(const char * const**))
{
	const void *data = NULL;
	const void *best_data = default_match;
	const char *const *compat;
	unsigned long dt_root;
	unsigned int best_score = ~1, score = 0;

	/* IAMROOT-12D (2016-05-26):
	 * --------------------------
	 * 초기값 dt_root = 0
	 * get_next_compat --> arch_get_next_mach
	 */
	dt_root = of_get_flat_dt_root();
	while ((data = get_next_compat(&compat))) {
		score = of_flat_dt_match(dt_root, compat);
		if (score > 0 && score < best_score) {
			best_data = data;
			best_score = score;
		}
	}
	if (!best_data) {
		const char *prop;
		int size;

		pr_err("\n unrecognized device tree list:\n[ ");

		prop = of_get_flat_dt_prop(dt_root, "compatible", &size);
		if (prop) {
			while (size > 0) {
				printk("'%s' ", prop);
				size -= strlen(prop) + 1;
				prop += strlen(prop) + 1;
			}
		}
		printk("]\n\n");
		return NULL;
	}

	/* IAMROOT-12D (2016-06-09):
	 * --------------------------
	 * Machine model: Raspberry Pi 2 Model B Rev 1.1
	 */
	pr_info("Machine model: %s\n", of_flat_dt_get_machine_name());

	return best_data;
}

/* IAMROOT-12CD (2016-06-25):
 * --------------------------
 * 라즈베리파이는 CONFIG_BLK_DEV_INITRD 를 지원하며
 * CONFIG_BLK_DEV_INITRD
 *	Initial RAM filesystem and RAM disk (initramfs/initrd) support
 *	initramfs, initrd(Initial RAM disk)를 지원
 * initrd : 부팅하는 동안 임시적으로 루트 파일시스템으로 쓰이는 RAM disk.
 * initramfs : initrd의 계승자(successor).
 */
#ifdef CONFIG_BLK_DEV_INITRD
/**
 * early_init_dt_check_for_initrd - Decode initrd location from flat tree
 * @node: reference to node containing initrd location ('chosen')
 */
static void __init early_init_dt_check_for_initrd(unsigned long node)
{
	u64 start, end;
	int len;
	const __be32 *prop;

	pr_debug("Looking for initrd properties... ");

	/* IAMROOT-12CD (2016-06-25):
	 * --------------------------
	 * 라즈베리파이2는 해당사항 없음.
	 * 
	 * tegra114 soc의 경우..
	 * chosen {
	 *   / * SHIELD's bootloader's arguments need to be overridden * /
	 *   bootargs = "console=ttyS0,115200n8 console=tty1 gpt fbcon=rotate:1";
	 *   / * SHIELD's bootloader will place initrd at this address * /
	 *   linux,initrd-start = <0x82000000>;
	 *   linux,initrd-end = <0x82800000>;
	 * };
	 */
	prop = of_get_flat_dt_prop(node, "linux,initrd-start", &len);
	if (!prop)
		return;
	start = of_read_number(prop, len/4);

	prop = of_get_flat_dt_prop(node, "linux,initrd-end", &len);
	if (!prop)
		return;
	end = of_read_number(prop, len/4);

	initrd_start = (unsigned long)__va(start);
	initrd_end = (unsigned long)__va(end);
	initrd_below_start_ok = 1;

	pr_debug("initrd_start=0x%llx  initrd_end=0x%llx\n",
		 (unsigned long long)start, (unsigned long long)end);
}
#else
static inline void early_init_dt_check_for_initrd(unsigned long node)
{
}
#endif /* CONFIG_BLK_DEV_INITRD */

#ifdef CONFIG_SERIAL_EARLYCON
extern struct of_device_id __earlycon_of_table[];

/* IAMROOT-12CD (2016-07-09):
 * --------------------------
 * 라즈베리 파이2에서는 /chosen/stdout-path, /chosen/linux,stdout-path 값이 없으
 * 므로 아무것도 하지 않고 리턴(-ENOENT)한다.
 */
static int __init early_init_dt_scan_chosen_serial(void)
{
	int offset;
	const char *p;
	int l;
	const struct of_device_id *match = __earlycon_of_table;
	const void *fdt = initial_boot_params;

	offset = fdt_path_offset(fdt, "/chosen");
	if (offset < 0)
		offset = fdt_path_offset(fdt, "/chosen@0");
	if (offset < 0)
		return -ENOENT;

	p = fdt_getprop(fdt, offset, "stdout-path", &l);
	if (!p)
		p = fdt_getprop(fdt, offset, "linux,stdout-path", &l);
	if (!p || !l)
		return -ENOENT;

	/* Get the node specified by stdout-path */
	offset = fdt_path_offset(fdt, p);
	if (offset < 0)
		return -ENODEV;

	while (match->compatible[0]) {
		unsigned long addr;
		if (fdt_node_check_compatible(fdt, offset, match->compatible)) {
			match++;
			continue;
		}

		addr = fdt_translate_address(fdt, offset);
		if (!addr)
			return -ENXIO;

		of_setup_earlycon(addr, match->data);
		return 0;
	}
	return -ENODEV;
}

static int __init setup_of_earlycon(char *buf)
{
	if (buf)
		return 0;

	return early_init_dt_scan_chosen_serial();
}
early_param("earlycon", setup_of_earlycon);
#endif

/**
 * early_init_dt_scan_root - fetch the top level address and size cells
 */
int __init early_init_dt_scan_root(unsigned long node, const char *uname,
				   int depth, void *data)
{
	const __be32 *prop;

	if (depth != 0)
		return 0;

	dt_root_size_cells = OF_ROOT_NODE_SIZE_CELLS_DEFAULT;
	dt_root_addr_cells = OF_ROOT_NODE_ADDR_CELLS_DEFAULT;

	prop = of_get_flat_dt_prop(node, "#size-cells", NULL);
	if (prop)
		dt_root_size_cells = be32_to_cpup(prop);
	pr_debug("dt_root_size_cells = %x\n", dt_root_size_cells);

	prop = of_get_flat_dt_prop(node, "#address-cells", NULL);
	if (prop)
		dt_root_addr_cells = be32_to_cpup(prop);
	pr_debug("dt_root_addr_cells = %x\n", dt_root_addr_cells);

	/* break now */
	return 1;
}

/* IAMROOT-12CD (2016-07-02):
 * --------------------------
 * s = 1, cellp = "reg : <0 0>;" 시작지점"
 */
u64 __init dt_mem_next_cell(int s, const __be32 **cellp)
{
	const __be32 *p = *cellp;

	/* IAMROOT-12CD (2016-07-02):
	 * --------------------------
	 * *cellp 는 "reg : <0 0>;" 값중 두번째 0을 가르키게 한다.
	 */
	*cellp = p + s;
	return of_read_number(p, s);
}

/**
 * early_init_dt_scan_memory - Look for an parse memory nodes
 */
/* IAMROOT-12CD (2016-06-17):
 * --------------------------
 * arch/arm/boot/dts/skeleton.dtsi
 * / {
 * 	#address-cells = <1>;
 * 	#size-cells = <1>;
 * 	chosen { };
 * 	aliases { };
 * 	memory { device_type = "memory"; reg = <0 0>; };
 * };
 *
 * 결론 : 라즈베리파이2에서는 reg = <0 0> 이므로 아무것도 하지 않고 끝낸다.
 */
int __init early_init_dt_scan_memory(unsigned long node, const char *uname,
				     int depth, void *data)
{
	const char *type = of_get_flat_dt_prop(node, "device_type", NULL);
	const __be32 *reg, *endp;
	int l;

	/* We are scanning "memory" nodes only */
	if (type == NULL) {
		/*
		 * The longtrail doesn't have a device_type on the
		 * /memory node, so look for the node called /memory@0.
		 */
		if (!IS_ENABLED(CONFIG_PPC32) || depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
	} else if (strcmp(type, "memory") != 0)
		return 0;

	/* IAMROOT-12CD (2016-06-25):
	 * --------------------------
	 * reg 는 아래의 "reg = <0 0>" 값으로 설정 된다.
	 * / {
	 * 	#address-cells = <1>;
	 * 	#size-cells = <1>;
	 * 	chosen { };
	 * 	aliases { };
	 * 	memory { device_type = "memory"; reg = <0 0>; };
	 * };
	 */
	reg = of_get_flat_dt_prop(node, "linux,usable-memory", &l);
	if (reg == NULL)
		reg = of_get_flat_dt_prop(node, "reg", &l);
	if (reg == NULL)
		return 0;

	/* IAMROOT-12CD (2016-07-02):
	 * --------------------------
	 * 라즈베리파이 2의 경우 reg = <0 0> 이기 때문에 (l / sizeof(__be32)) 값
	 * 은 2이기 때문에 l = 8
	 */
	endp = reg + (l / sizeof(__be32));

	pr_debug("memory scan node %s, reg size %d,\n", uname, l);

	while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
		u64 base, size;

		base = dt_mem_next_cell(dt_root_addr_cells, &reg);
		size = dt_mem_next_cell(dt_root_size_cells, &reg);

		if (size == 0)
			continue;
		pr_debug(" - %llx ,  %llx\n", (unsigned long long)base,
		    (unsigned long long)size);

		early_init_dt_add_memory_arch(base, size);
	}

	return 0;
}

int __init early_init_dt_scan_chosen(unsigned long node, const char *uname,
				     int depth, void *data)
{
	int l;
	const char *p;

	pr_debug("search \"chosen\", depth: %d, uname: %s\n", depth, uname);

	if (depth != 1 || !data ||
	    (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

	early_init_dt_check_for_initrd(node);

	/* IAMROOT-12CD (2016-06-25):
	 * --------------------------
	 * bootargs 는 atags 에 있는 cmdline 값이다.
	 * arch/arm/boot/compressed/atags_to_fdt.c 에서 atags에 있는 값을 fdt에
	 * 복사 했다.
	 */
	/* Retrieve command line */
	p = of_get_flat_dt_prop(node, "bootargs", &l);

	/*
	 * CONFIG_CMDLINE is meant to be a default in case nothing else
	 * managed to set the command line, unless CONFIG_CMDLINE_FORCE
	 * is set in which case we override whatever was found earlier.
	 *
	 * However, it can be useful to be able to treat the default as
	 * a starting point to be extended using CONFIG_CMDLINE_EXTEND.
	 */
	((char *)data)[0] = '\0';

/* IAMROOT-12CD (2016-06-17):
 * --------------------------
 * 라즈베리파이2
 *  CONFIG_CMDLINE="console=ttyAMA0,115200 kgdboc=ttyAMA0,115200 root=/dev/mmcblk0p2 rootfstype=ext4 rootwait"
 *  CONFIG_CMDLINE_EXTEND is not set
 *  CONFIG_CMDLINE_FORCE is not set
 */
#ifdef CONFIG_CMDLINE
	strlcpy(data, CONFIG_CMDLINE, COMMAND_LINE_SIZE);

	if (p != NULL && l > 0)	{
#if defined(CONFIG_CMDLINE_EXTEND)
		int len = strlen(data);
		if (len > 0) {
			strlcat(data, " ", COMMAND_LINE_SIZE);
			len++;
		}
		strlcpy((char *)data + len, p, min((int)l, COMMAND_LINE_SIZE - len));
#elif defined(CONFIG_CMDLINE_FORCE)
		pr_warning("Ignoring bootargs property (using the default kernel command line)\n");
#else
		/* IAMROOT-12CD (2016-06-25):
		 * --------------------------
		 * 위의 CONFIG_CMDLINE 값을 무시하고 atags에 있는 정보가 복사됨
		 */
		/* Neither extend nor force - just override */
		strlcpy(data, p, min((int)l, COMMAND_LINE_SIZE));
#endif
	}
#else /* CONFIG_CMDLINE */
	if (p != NULL && l > 0)	{
		strlcpy(data, p, min((int)l, COMMAND_LINE_SIZE));
#endif /* CONFIG_CMDLINE */

	pr_debug("Command line is: %s\n", (char*)data);

	/* break now */
	return 1;
}

#ifdef CONFIG_HAVE_MEMBLOCK
#define MAX_PHYS_ADDR	((phys_addr_t)~0)

void __init __weak early_init_dt_add_memory_arch(u64 base, u64 size)
{
	const u64 phys_offset = __pa(PAGE_OFFSET);

	/* IAMROOT-12CD (2016-07-02):
	 * --------------------------
	 * PAGE_SIZE 단위로 align을 맞쳐준다.
	 */
	if (!PAGE_ALIGNED(base)) {
		if (size < PAGE_SIZE - (base & ~PAGE_MASK)) {
			pr_warn("Ignoring memory block 0x%llx - 0x%llx\n",
				base, base + size);
			return;
		}
		size -= PAGE_SIZE - (base & ~PAGE_MASK);
		base = PAGE_ALIGN(base);
	}
	size &= PAGE_MASK;

	if (base > MAX_PHYS_ADDR) {
		pr_warning("Ignoring memory block 0x%llx - 0x%llx\n",
				base, base + size);
		return;
	}

	if (base + size - 1 > MAX_PHYS_ADDR) {
		pr_warning("Ignoring memory range 0x%llx - 0x%llx\n",
				((u64)MAX_PHYS_ADDR) + 1, base + size);
		size = MAX_PHYS_ADDR - base + 1;
	}

	if (base + size < phys_offset) {
		pr_warning("Ignoring memory block 0x%llx - 0x%llx\n",
			   base, base + size);
		return;
	}
	if (base < phys_offset) {
		pr_warning("Ignoring memory range 0x%llx - 0x%llx\n",
			   base, phys_offset);
		size -= phys_offset - base;
		base = phys_offset;
	}
	memblock_add(base, size);
}

int __init __weak early_init_dt_reserve_memory_arch(phys_addr_t base,
					phys_addr_t size, bool nomap)
{
	if (nomap)
		return memblock_remove(base, size);
	return memblock_reserve(base, size);
}

/*
 * called from unflatten_device_tree() to bootstrap devicetree itself
 * Architectures can override this definition if memblock isn't used
 */
void * __init __weak early_init_dt_alloc_memory_arch(u64 size, u64 align)
{
	return __va(memblock_alloc(size, align));
}
#else
int __init __weak early_init_dt_reserve_memory_arch(phys_addr_t base,
					phys_addr_t size, bool nomap)
{
	pr_err("Reserved memory not supported, ignoring range 0x%pa - 0x%pa%s\n",
		  &base, &size, nomap ? " (nomap)" : "");
	return -ENOSYS;
}
#endif

/* IAMROOT-12D (2016-05-26):
 * --------------------------
 * 간단한 fdt헤더 검사와 crc32을 만든다.
 */
bool __init early_init_dt_verify(void *params)
{
	if (!params)
		return false;

	/* check device tree validity */
	if (fdt_check_header(params))
		return false;

	/* Setup flat device-tree pointer */
	initial_boot_params = params;
	of_fdt_crc32 = crc32_be(~0, initial_boot_params,
				fdt_totalsize(initial_boot_params));
	return true;
}


void __init early_init_dt_scan_nodes(void)
{
	/* IAMROOT-12CD (2016-06-25):
	 * --------------------------
	 * atags에 있던 cmdline를 boot_command_line에 복사.
	 */
	/* Retrieve various information from the /chosen node */
	of_scan_flat_dt(early_init_dt_scan_chosen, boot_command_line);

	/* Initialize {size,address}-cells info */
	of_scan_flat_dt(early_init_dt_scan_root, NULL);

	/* Setup memory, calling early_init_dt_add_memory_arch */
	of_scan_flat_dt(early_init_dt_scan_memory, NULL);
}

bool __init early_init_dt_scan(void *params)
{
	bool status;

	status = early_init_dt_verify(params);
	if (!status)
		return false;

	early_init_dt_scan_nodes();
	return true;
}

/**
 * unflatten_device_tree - create tree of device_nodes from flat blob
 *
 * unflattens the device-tree passed by the firmware, creating the
 * tree of struct device_node. It also fills the "name" and "type"
 * pointers of the nodes so the normal device-tree walking functions
 * can be used.
 */
void __init unflatten_device_tree(void)
{
	__unflatten_device_tree(initial_boot_params, &of_root,
				early_init_dt_alloc_memory_arch);

	/* Get pointer to "/chosen" and "/aliases" nodes for use everywhere */
	of_alias_scan(early_init_dt_alloc_memory_arch);
}

/**
 * unflatten_and_copy_device_tree - copy and create tree of device_nodes from flat blob
 *
 * Copies and unflattens the device-tree passed by the firmware, creating the
 * tree of struct device_node. It also fills the "name" and "type"
 * pointers of the nodes so the normal device-tree walking functions
 * can be used. This should only be used when the FDT memory has not been
 * reserved such is the case when the FDT is built-in to the kernel init
 * section. If the FDT memory is reserved already then unflatten_device_tree
 * should be used instead.
 */
void __init unflatten_and_copy_device_tree(void)
{
	int size;
	void *dt;

	if (!initial_boot_params) {
		pr_warn("No valid device tree found, continuing without\n");
		return;
	}

	size = fdt_totalsize(initial_boot_params);
	dt = early_init_dt_alloc_memory_arch(size,
					     roundup_pow_of_two(FDT_V17_SIZE));

	if (dt) {
		memcpy(dt, initial_boot_params, size);
		initial_boot_params = dt;
	}
	unflatten_device_tree();
}

#ifdef CONFIG_SYSFS
static ssize_t of_fdt_raw_read(struct file *filp, struct kobject *kobj,
			       struct bin_attribute *bin_attr,
			       char *buf, loff_t off, size_t count)
{
	memcpy(buf, initial_boot_params + off, count);
	return count;
}

static int __init of_fdt_raw_init(void)
{
	static struct bin_attribute of_fdt_raw_attr =
		__BIN_ATTR(fdt, S_IRUSR, of_fdt_raw_read, NULL, 0);

	if (!initial_boot_params)
		return 0;

	if (of_fdt_crc32 != crc32_be(~0, initial_boot_params,
				     fdt_totalsize(initial_boot_params))) {
		pr_warn("fdt: not creating '/sys/firmware/fdt': CRC check failed\n");
		return 0;
	}
	of_fdt_raw_attr.size = fdt_totalsize(initial_boot_params);
	return sysfs_create_bin_file(firmware_kobj, &of_fdt_raw_attr);
}
late_initcall(of_fdt_raw_init);
#endif

#endif /* CONFIG_OF_EARLY_FLATTREE */
