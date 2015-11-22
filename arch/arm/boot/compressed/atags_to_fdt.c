#include <asm/setup.h>
#include <libfdt.h>

#if defined(CONFIG_ARM_ATAG_DTB_COMPAT_CMDLINE_EXTEND)
#define do_extend_cmdline 1
#else
#define do_extend_cmdline 0
#endif

#define NR_BANKS 16

static int node_offset(void *fdt, const char *node_path)
{
	int offset = fdt_path_offset(fdt, node_path);
	if (offset == -FDT_ERR_NOTFOUND)
		offset = fdt_add_subnode(fdt, 0, node_path);
	return offset;
}

static int setprop(void *fdt, const char *node_path, const char *property,
		   uint32_t *val_array, int size)
{
	int offset = node_offset(fdt, node_path);
	if (offset < 0)
		return offset;
	return fdt_setprop(fdt, offset, property, val_array, size);
}

static int setprop_string(void *fdt, const char *node_path,
			  const char *property, const char *string)
{
	int offset = node_offset(fdt, node_path);
	if (offset < 0)
		return offset;
	return fdt_setprop_string(fdt, offset, property, string);
}

static int setprop_cell(void *fdt, const char *node_path,
			const char *property, uint32_t val)
{
	int offset = node_offset(fdt, node_path);
	if (offset < 0)
		return offset;
	return fdt_setprop_cell(fdt, offset, property, val);
}

/*
 * chosen {
 * bootargs="console=ttyS0,115200 ubi.mtd=4 root=ubi0:rootfs rootfstype=ubifs";
 * };

 * 호출: fdt_bootargs = getprop(fdt, "/chosen", "bootargs", &len);
 */
static const void *getprop(const void *fdt, const char *node_path,
			   const char *property, int *len)
{
	// node "/chosen"의 offset을 가져옴
	int offset = fdt_path_offset(fdt, node_path);

	if (offset == -FDT_ERR_NOTFOUND)
		return NULL;

	// 이젠 node "/chosen" 안에 있는 property "bootargs"를 가져옴
	return fdt_getprop(fdt, offset, property, len);
}

static uint32_t get_cell_size(const void *fdt)
{
	int len;
	uint32_t cell_size = 1;
	const uint32_t *size_len =  getprop(fdt, "/", "#size-cells", &len);

	if (size_len)
		cell_size = fdt32_to_cpu(*size_len);
	return cell_size;
}

static void merge_fdt_bootargs(void *fdt, const char *fdt_cmdline)
{
	char cmdline[COMMAND_LINE_SIZE];
	const char *fdt_bootargs;
	char *ptr = cmdline;
	int len = 0;

	/*
	 * fdt_node 예
	 * /{
	 * ...
	 * chosen {
	 * bootargs="root=/dev/nfs rw nfsroot=192.168.1.1 console=ttyS0,115200";
	 * };
	 * ...
	 * };

	 * 위 예제에서
	 * 노드명: "/chosen"
	 * property: "bootargs"
	 * 더 많은 참고: http://xillybus.com/tutorials/device-tree-zynq-2

	 * fdt_bootargs에 받아오는 내용은 아래와 같음
	 * "root=/dev/nfs rw nfsroot=192.168.1.1 console=ttyS0,115200";
	 * len에 길이가 저장됨
	*/

	// node "/chosen"의 property "bootargs"의 value과 길이를 받아옴
	/* copy the fdt command line into the buffer */
	fdt_bootargs = getprop(fdt, "/chosen", "bootargs", &len);
	// [2015.11.28 여기서부터 시작]
	if (fdt_bootargs)
		if (len < COMMAND_LINE_SIZE) {
			memcpy(ptr, fdt_bootargs, len);
			/* len is the length of the string
			 * including the NULL terminator */
			ptr += len - 1;
		}

	/* and append the ATAG_CMDLINE */
	if (fdt_cmdline) {
		len = strlen(fdt_cmdline);
		if (ptr - cmdline + len + 2 < COMMAND_LINE_SIZE) {
			*ptr++ = ' ';
			memcpy(ptr, fdt_cmdline, len);
			ptr += len;
		}
	}
	*ptr = '\0';

	setprop_string(fdt, "/chosen", "bootargs", cmdline);
}

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-11-07
 * ------------------------------------------------------------------
 *  
 * 참석자: 곽희범 (andrew@norux.me)
 *	   임채훈 (im.fehead@gmail.com)
 *	   박종성 (@minidump)
 *	   안종찬 (ahnchan2@gmail.com)
 *	   김건용 (gykim0914@gmail.com)
 *	   권세홍 (sehongkwon2.24@gmail.com)
 *	   조훈근 (hoonycho12@gmail.com)
 *	   김민호 (8891m@naver.com)
 *	   정종채 (fynia@naver.com)
 *	   김문영 (m03y29@gmail.com)
 *
 *	   참석인원: 10 명
 *
 * http://www.iamroot.org/xe/Kernel_10_ARM/178300
 *
 * ==================================================================
 */
/*
 * Convert and fold provided ATAGs into the provided FDT.
 *
 * atags의 내용을 fdt에 변환하여 넣는다.
 *
 * REturn values:
 *    = 0 -> pretend success
 *    = 1 -> bad ATAG (may retry with another possible ATAG pointer)
 *    < 0 -> error from libfdt
 *
 * head.S 소스 restart 라벨 근처에서 호출됨
 * in	atags_list	atags 주소 0x00000100
 * in	fdt		dtb의 주소 .bss 주소
 * in	total_space	32KB <= (dtb_totalsize * 1.5) <= 1MB
 */
 /* fdt의 첫 4byte 값은 MAGIC_NUMBER이고 두번째 4byte 값은 fdt의 size */
int atags_to_fdt(void *atag_list, void *fdt, int total_space)
{
	struct tag *atag = atag_list;
	/* In the case of 64 bits memory size, need to reserve 2 cells for
	 * address and size for each bank */
	uint32_t mem_reg_property[2 * 2 * NR_BANKS];
	int memcount = 0;
	int ret, memsize;

	/* make sure we've got an aligned pointer */
	// 하위 두비트 값 확인  http://www.iamroot.org/xe/Kernel_10_ARM/178300
	// atag_list = 0x00000100 
	if ((u32)atag_list & 0x3)
		return 1;

	/* if we get a DTB here we're done already */
	if (*(u32 *)atag_list == fdt32_to_cpu(FDT_MAGIC))
	       return 0;

	/* validate the ATAG */
	if (atag->hdr.tag != ATAG_CORE ||
	    (atag->hdr.size != tag_size(tag_core) &&
	     atag->hdr.size != 2))
		return 1;

	/* let's give it all the room it could need */
	ret = fdt_open_into(fdt, fdt, total_space);
	if (ret < 0)
		return ret;

	for_each_tag(atag, atag_list) {
		if (atag->hdr.tag == ATAG_CMDLINE) {
			/* Append the ATAGS command line to the device tree
			 * command line.
			 * NB: This means that if the same parameter is set in
			 * the device tree and in the tags, the one from the
			 * tags will be chosen.
			 */
			/*
			 * do_extend_cmdline는 아래 config 가 들어가 있을때 1
			 *   ARM_ATAG_DTB_COMPAT_CMDLINE_EXTEND
			 *     "Extend with bootloader kernel arguments"
			 *     help
			 *     The command-line arguments provided by the boot
			 *     loader will be appended to the the device tree
			 *     bootargs property.
			 *
			 * /arch/arm/boot/dts/ *.dts 파일 중 아래 옵션
			 *   chosen {
			 *    bootargs = "console=ttyS0,115200 ubi.mtd=4 \
			 *       root=ubi0:rootfs rootfstype=ubifs";
			 *   };
			 */
			// 2015-11-21 시작할 위치
			if (do_extend_cmdline)
				merge_fdt_bootargs(fdt,
						   atag->u.cmdline.cmdline);
			else
				setprop_string(fdt, "/chosen", "bootargs",
					       atag->u.cmdline.cmdline);
		} else if (atag->hdr.tag == ATAG_MEM) {
			if (memcount >= sizeof(mem_reg_property)/4)
				continue;
			if (!atag->u.mem.size)
				continue;
			memsize = get_cell_size(fdt);

			if (memsize == 2) {
				/* if memsize is 2, that means that
				 * each data needs 2 cells of 32 bits,
				 * so the data are 64 bits */
				uint64_t *mem_reg_prop64 =
					(uint64_t *)mem_reg_property;
				mem_reg_prop64[memcount++] =
					cpu_to_fdt64(atag->u.mem.start);
				mem_reg_prop64[memcount++] =
					cpu_to_fdt64(atag->u.mem.size);
			} else {
				mem_reg_property[memcount++] =
					cpu_to_fdt32(atag->u.mem.start);
				mem_reg_property[memcount++] =
					cpu_to_fdt32(atag->u.mem.size);
			}

		} else if (atag->hdr.tag == ATAG_INITRD2) {
			uint32_t initrd_start, initrd_size;
			initrd_start = atag->u.initrd.start;
			initrd_size = atag->u.initrd.size;
			setprop_cell(fdt, "/chosen", "linux,initrd-start",
					initrd_start);
			setprop_cell(fdt, "/chosen", "linux,initrd-end",
					initrd_start + initrd_size);
		}
	}

	if (memcount) {
		setprop(fdt, "/memory", "reg", mem_reg_property,
			4 * memcount * memsize);
	}

	return fdt_pack(fdt);
}
