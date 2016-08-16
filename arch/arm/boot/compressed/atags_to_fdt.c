#include <asm/setup.h>
#include <libfdt.h>

#if defined(CONFIG_ARM_ATAG_DTB_COMPAT_CMDLINE_EXTEND)
#define do_extend_cmdline 1
#else
#define do_extend_cmdline 0
#endif

#define NR_BANKS 16

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-12-05
 * ------------------------------------------------------------------
 *
 * 만약, node_path가 존재하지 않으면, add_subnode를 통해 새로 노드를 만든다.
 *
 * ==================================================================
 */
static int node_offset(void *fdt, const char *node_path)
{
	/* node를 찾지못하면 FDT_ERR_NOTFOUND가 나옴 */
	int offset = fdt_path_offset(fdt, node_path);
	if (offset == -FDT_ERR_NOTFOUND)
		/* parent offset이 0인이유는, 루트하위의 노드로 넣겠다는 
		   의미이다.*/
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

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-11-28
 * ------------------------------------------------------------------
 * 
 * fdt 에서 지정된 offset 위치의 property를 string 값으로 세팅
 */
static int setprop_string(void *fdt, const char *node_path,
			  const char *property, const char *string)
{
	int offset = node_offset(fdt, node_path);
	if (offset < 0)
		return offset;
	return fdt_setprop_string(fdt, offset, property, string);
}

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-12-05
 * ------------------------------------------------------------------
 * node_path - property 에 val을 세팅한다.
 * ==================================================================
 */
static int setprop_cell(void *fdt, const char *node_path,
			const char *property, uint32_t val)
{
	/*
	  node_path를 구해온다.
	  만약, node_path가 없다면 추가 이후 오프셋을 구해온다.
	*/
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

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-12-05
 * ------------------------------------------------------------------
 * fdt에서 cell size를 읽어옴
 *
 * 32/64bit를 셀사이즈로 판단한다.
 *
 * 1: 32bit(*default)
 * 2: 64bit
 * ==================================================================
 */
static uint32_t get_cell_size(const void *fdt)
{
	int len;
	uint32_t cell_size = 1;
	const uint32_t *size_len =  getprop(fdt, "/", "#size-cells", &len);

	/* 64bit면 size_len이 2로 올것이다. */
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
	/* fdt_bootargs 로 리턴된 문자열과 
	 * 인자로 넘어온 fdt_cmdline 문자열을 합치는 구문
	 * ex) cmdline = fdt_bootargs + ' ' + fdt_cmdline + '\0'
	 */
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
 * REturn values:
 *    = 0 -> pretend success
 *    = 1 -> bad ATAG (may retry with another possible ATAG pointer)
 *    < 0 -> error from libfdt
 *
 * atags의 내용을 fdt에 변환하여 넣는다.
 *
 * head.S 소스 restart 라벨 근처에서 호출됨
 * in	atags_list	atags 주소 0x00000100
 * out	fdt		dtb의 주소 .bss 주소
 * in	total_space	32KB <= (dtb_totalsize * 1.5) <= 1MB
 */
 /* fdt의 첫 4byte 값은 MAGIC_NUMBER이고 두번째 4byte 값은 fdt의 size */
int atags_to_fdt(void *atag_list, void *fdt, int total_space)
{
	struct tag *atag = atag_list;
	/* In the case of 64 bits memory size, need to reserve 2 cells for
	 * address and size for each bank */
	/* TODO Bank? NR_BANKS:16 */
	/*
	  +-------------------------------------------+
	  |  |  |  |  |  | ...  		   |  |
	  +-------------------------------------------+

	  64비트 머신에서는 uint64_t로 형변환해서 2칸씩 뛰게된다.

	  Bank의 수는 16개, 16개까지 채울수 있다.
	   + Address start 과, 사이즈 값을 담을 배열이 필요하여 곱하기 2를 함
	   + 64비트 머신에서는 Address값이 2칸이 필요함으로 곱하기 2를 함
	     (reserve)
	*/
	uint32_t mem_reg_property[2 * 2 * NR_BANKS];
	int memcount = 0;
	int ret, memsize;

	/* make sure we've got an aligned pointer */
	// 하위 두비트 값 확인  http://www.iamroot.org/xe/Kernel_10_ARM/178300
	// atag_list = 0x00000100 
	if ((u32)atag_list & 0x3)
		return 1;

	/* IAMROOT-12D (2016-01-30):
	 * --------------------------
	 * 만약, 이미 우리가 DTB를 가지고 있다면 리턴한다.
	 * 이는 에러가 아니라, 두 번째 restart를 돌고 있다는 의미이다.
	 */
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
			 */
			// 2015-11-21 시작할 위치
			if (do_extend_cmdline)
				merge_fdt_bootargs(fdt,
						   atag->u.cmdline.cmdline);
			else
				setprop_string(fdt, "/chosen", "bootargs",
					       atag->u.cmdline.cmdline);

		/* if (atag->hdr.tag == ATAG_CMDLINE) */
		} else if (atag->hdr.tag == ATAG_MEM) {
			/* 
			  uint32_t[64]배열
			  sizeof(mem_reg_property) : 64 * 4 = 256
			  배열의 Entry 개수만 구한듯
			  4 -> sizeof(uint32_t)
			  4는 버그같..?
			  64비트일때의 고려가 되어있지 않음

			  continue: for_each_tag(atag, atag_list)
			*/
			if (memcount >= sizeof(mem_reg_property)/4)
				/* continue: 다음 태그를 읽음 */
				continue;
			if (!atag->u.mem.size)
				continue;
			memsize = get_cell_size(fdt);

			/* 2cell이라면 64비트 */
			/* 
			  mem_reg_property 에다가 Address start, Address Size를
			  차곡차곡 저장한다.
			*/
			if (memsize == 2) {
				/* if memsize is 2, that means that
				 * each data needs 2 cells of 32 bits,
				 * so the data are 64 bits */
				uint64_t *mem_reg_prop64 =
					(uint64_t *)mem_reg_property;

				/* 실제로는 아무 문제 없겠지만,
				   memcount는 코드상으로 63까지 가능하다.
				   memcount가 32이상이되면 아래의 메모리참조에서
				   에러가 날 가능성이 있다.
				*/
				mem_reg_prop64[memcount++] =
					cpu_to_fdt64(atag->u.mem.start);
				mem_reg_prop64[memcount++] =
					cpu_to_fdt64(atag->u.mem.size);
			/* 32비트 */
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

			/*
			[example]

			chosen {
				bootargs = "console=ttyS0 ip=on root=/dev/ram";
				linux,stdout-path = "/plb@0/serial@83e00000";

				현재는 아래 두 값이 없지만, 아래 함수에 의해
				설정될 것이다.
				linux,initrd-start = <FD36000>;
				linux,initrd-end = <FEA5F20>;
			} ;
			*/
			setprop_cell(fdt, "/chosen", "linux,initrd-start",
					initrd_start);
			setprop_cell(fdt, "/chosen", "linux,initrd-end",
					initrd_start + initrd_size);
		}
	} /* for_each_tag 종료 */

	/*
	  memsize: 1(32bit) / 2(64bit)
	  mem_reg_property 실제 사이즈: (4 * memcount * memsize)

	  i.e. reg = <0x0 0x8000000 0x8000000 0x1000000>;
	*/
	if (memcount) {
		/* IAMROOT-12CD (2016-07-23):
		 * --------------------------
		 * ATAGS [ATAG_MEM] size: 8000000, start: 0
		 *  memory {
		 *    	reg = <0x0 0x80000000>;
		 *  };
		 */
		setprop(fdt, "/memory", "reg", mem_reg_property,
			4 * memcount * memsize);
	}

	return fdt_pack(fdt);
}
