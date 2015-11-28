/*
 * libfdt - Flat Device Tree manipulation
 * Copyright (C) 2006 David Gibson, IBM Corporation.
 *
 * libfdt is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 *
 *  a) This library is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of the
 *     License, or (at your option) any later version.
 *
 *     This library is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public
 *     License along with this library; if not, write to the Free
 *     Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 *     MA 02110-1301 USA
 *
 * Alternatively,
 *
 *  b) Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *     1. Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *     2. Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *     CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *     INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *     MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *     CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *     SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *     NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *     OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *     EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "libfdt_env.h"

#include <fdt.h>
#include <libfdt.h>

#include "libfdt_internal.h"

//MAGIC값과 VERSION을 체크해서 문제가 있을 시 음수값을 리턴함.
int fdt_check_header(const void *fdt)
{
	/* FDT_MAGIC	0xd00dfeed
	fdt_magic(fdt) 설명:
		struct fdt_header * pf = fdt;
		return pf->magic;
	*/
	if (fdt_magic(fdt) == FDT_MAGIC) {
		/* Complete tree */
		if (fdt_version(fdt) < FDT_FIRST_SUPPORTED_VERSION)
			return -FDT_ERR_BADVERSION;
		if (fdt_last_comp_version(fdt) > FDT_LAST_SUPPORTED_VERSION)
			return -FDT_ERR_BADVERSION;
	} else if (fdt_magic(fdt) == FDT_SW_MAGIC) {
		/* Unfinished sequential-write blob */
		if (fdt_size_dt_struct(fdt) == 0)
			return -FDT_ERR_BADSTATE;
	} else {
		return -FDT_ERR_BADMAGIC;
	}

	return 0;
}

const void *fdt_offset_ptr(const void *fdt, int offset, unsigned int len)
{
	const char *p;

	/* 0x11 = 17, Default version */
	if (fdt_version(fdt) >= 0x11)
		/* 1. offset의 overflow체크? len은 unsigned int임.
		 * 2. offset+len은 size_dt_struct보다 작거나 같아야만 한다.
		 *
		 *  ex)    dt_struct
		 *	+-------------+  			+
		 * 	|             |  			|
		 * 	|  something  |				| dt_struct_size
		 * 	|             |  			|
		 * 	+-------------+  <-- dt_struct_offset	+
		 *
		 *  _fdt_offset_ptr 내부에서 dt_struct_offset + offset을 한다.
		 *  따라서, offset + len 이 dt_struct_size 보다 크면 안된다.
		 *
		 * 아래 if를 거꾸로 쓰면,
		 *  offset < offset + len < fdt_size_dt_struct(fdt)
		 *
		 * len이 unsigned!!! 절대 음수가 들어올 수 없음.
		 * 따라서 오버플로우 방어코드라고 추정..
		 */
		if (((offset + len) < offset)
		    || ((offset + len) > fdt_size_dt_struct(fdt)))
			return NULL;

	/* offset은 Valid 검증된 이후 */
	p = _fdt_offset_ptr(fdt, offset);

	if (p + len < p)
		return NULL;
	return p;
}

/*
 * 다음 tag의 Offset를 찾는다.(nextoffset갱신) 
 * in	startoffset	현재 시작 offset
 * out	nextoffset	다음 시작 offset
 * return FDT_END	에러 혹은 현재의 tag값
 * 	tag 구분값
 *		define FDT_BEGIN_NODE	0x1		
 *		#define FDT_END_NODE	0x2
 *		#define FDT_PROP	0x3
 */
uint32_t fdt_next_tag(const void *fdt, int startoffset, int *nextoffset)
{
	const uint32_t *tagp, *lenp;
	uint32_t tag;
	int offset = startoffset;
	const char *p;

	*nextoffset = -FDT_ERR_TRUNCATED;
	/* FDT_TAGSIZE: sizeof(uint32_t) word사이즈 */
	tagp = fdt_offset_ptr(fdt, offset, FDT_TAGSIZE);
	if (!tagp)
		return FDT_END; /* premature end */
	tag = fdt32_to_cpu(*tagp);
	offset += FDT_TAGSIZE;

	*nextoffset = -FDT_ERR_BADSTRUCTURE;
	switch (tag) {
	    /*
		#define FDT_BEGIN_NODE	0x1		
		#define FDT_END_NODE	0x2
		#define FDT_PROP	0x3

		[2015-11-07 여기까지 함]
	    */
	case FDT_BEGIN_NODE:
		/* skip name */
		/*
		   struct fdt_node_header {
			   uint32_t tag;
			   char name[0];
		   };
		 */
		do {
			p = fdt_offset_ptr(fdt, offset++, 1);
		} while (p && (*p != '\0'));
		if (!p)
			return FDT_END; /* premature end */
		break;

	case FDT_PROP:
		/*
		   struct fdt_property {
			   uint32_t tag;
			   uint32_t len;
			   uint32_t nameoff;
			   char data[0];
		   };
		 */
		lenp = fdt_offset_ptr(fdt, offset, sizeof(*lenp));
		if (!lenp)
			return FDT_END; /* premature end */
		/* skip-name offset, length and value */
		/* offset += 12 - 4 + (*lenp)
		sizeof(struct fdt_property) 는 12이다.
		*/
		offset += sizeof(struct fdt_property) - FDT_TAGSIZE
			+ fdt32_to_cpu(*lenp);
		break;

	case FDT_END:
	case FDT_END_NODE:
	case FDT_NOP:
		break;

	default:
		return FDT_END;
	}

	// offset값 범위체크
	if (!fdt_offset_ptr(fdt, startoffset, offset - startoffset))
		return FDT_END; /* premature end */
	/*
	   4 byte align을 맞추기위해
	#define FDT_ALIGN(x, a)		(((x) + (a) - 1) & ~((a) - 1))
	#define FDT_TAGALIGN(x)		(FDT_ALIGN((x), FDT_TAGSIZE))

	ex) FDT_TAGALIGN(10)
		FDT_ALIGN(10, 4)
		((10 + 4 - 1) & ~(4 - 1)) = 12
		(13 & ~3) = 0b1101 & ~0b0011 = 0b1100 = 12
	*/
	*nextoffset = FDT_TAGALIGN(offset);
	return tag;
}
/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-11-28
 * ------------------------------------------------------------------
 * 1. offset 이 valid 한 range에 있는지 check
 */
int _fdt_check_node_offset(const void *fdt, int offset)
{
	if ((offset < 0) || (offset % FDT_TAGSIZE)
	    || (fdt_next_tag(fdt, offset, &offset) != FDT_BEGIN_NODE))
		return -FDT_ERR_BADOFFSET;

	return offset;
}

int _fdt_check_prop_offset(const void *fdt, int offset)
{
	if ((offset < 0) || (offset % FDT_TAGSIZE)
	    || (fdt_next_tag(fdt, offset, &offset) != FDT_PROP))
		return -FDT_ERR_BADOFFSET;

	return offset;
}

/*
 * offset을 기준으로 다음 노드를 가져옴
 * 현재 탐색중인 node의 depth를 저장
*/
int fdt_next_node(const void *fdt, int offset, int *depth)
{
	int nextoffset = 0;
	uint32_t tag;

	// "chosen" 맨 처음에는 offset는 0
	if (offset >= 0)
		/*
		 * offset의 유효성 체크
		 * 참고:http://iamroot.org/wiki/lib/exe/fetch.php?media=%EC%8A%A4%ED%84%B0%EB%94%94:dtb_structure.png
		*/
		if ((nextoffset = _fdt_check_node_offset(fdt, offset)) < 0)
			return nextoffset;

	/*
	 * FDT_BEGIN_NODE
	 * - 현재 노드의 tag값을 찾는데 만약에 새로운 노드가 시작되면 depth++
	 * FDT_END_NODE
	 * - 탐색중인 노드가 끝난 경우 depth를 --
	 * depth를 감소했을때 음수가 아닌경우
	 * - childNode가 더이상 없는 경우 nextoffset(0) 리턴

	 * childNode안에 또 childNode가 있을수 있으니 depth로 판별?
	*/
	do {
		offset = nextoffset;
		tag = fdt_next_tag(fdt, offset, &nextoffset);

		switch (tag) {
		case FDT_PROP:
		case FDT_NOP:
			break;

		case FDT_BEGIN_NODE:
			if (depth)
				(*depth)++;
			break;

		case FDT_END_NODE:
			if (depth && ((--(*depth)) < 0))
				return nextoffset;
			break;

		case FDT_END:
			if ((nextoffset >= 0)
			    || ((nextoffset == -FDT_ERR_TRUNCATED) && !depth))
				return -FDT_ERR_NOTFOUND;
			else
				return nextoffset;
		}
	} while (tag != FDT_BEGIN_NODE);

	return offset;
}
/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-11-28
 * ------------------------------------------------------------------
 * strtab ~ tabsize 구간에서 s 문자열의 위치를 찾아서 return
 */
const char *_fdt_find_string(const char *strtab, int tabsize, const char *s)
{
	int len = strlen(s) + 1;
	const char *last = strtab + tabsize - len;
	const char *p;

	for (p = strtab; p <= last; p++)
		if (memcmp(p, s, len) == 0)
			return p;
	return NULL;
}

/*
 * buf에 fdt를 복사.
 * in	fdt	fdt
 * out	buf	fdt
 * return	0 : 성공
 * 		-FDT_ERR_NOSPACE : 실패
 */
int fdt_move(const void *fdt, void *buf, int bufsize)
{
	FDT_CHECK_HEADER(fdt);

	if (fdt_totalsize(fdt) > bufsize)
		return -FDT_ERR_NOSPACE;

	memmove(buf, fdt, fdt_totalsize(fdt));
	return 0;
}
