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

/*
 * fdt 블럭의 순서들이 올바른지 체크한다.
 *
 * return 0 : 올바르다
 * 	0이 아님 : 옳지 않다
 */
static int _fdt_blocks_misordered(const void *fdt,
			      int mem_rsv_size, int struct_size)
{
	/**
	 * FDT 순서는 아래와 같다.
	 *  아래의 영역들은 인접한 영역이다.
	 * +--------------------+ <-- fdt
	 * | fdt_header(40byte) |
	 * +--------------------+
	 * | mem_rsvmap         |
	 * | ...                |
	 * +--------------------+
	 * | dt_struct          |
	 * | ...                |
	 * +--------------------+
	 * | dt_strings         |
	 * | ...                |
	 * +--------------------+ <-- total_size
	 *
	 * 1. off_mem_rsvmap 은 fdt_header size보다 커야 된다.
	 * 2. dt_struct 는 mem_rsvmap 뒤에 있어야 한다.
	 * 3. dt_strings 는 dt_struct 보다 뒤에 있어야 한다.
	 * 4. totalsize는 dt_strings 보다 뒤에 있어야한다.
	 */
	return (fdt_off_mem_rsvmap(fdt) < FDT_ALIGN(sizeof(struct fdt_header), 8))
		|| (fdt_off_dt_struct(fdt) <
		    (fdt_off_mem_rsvmap(fdt) + mem_rsv_size))
		|| (fdt_off_dt_strings(fdt) <
		    (fdt_off_dt_struct(fdt) + struct_size))
		|| (fdt_totalsize(fdt) <
		    (fdt_off_dt_strings(fdt) + fdt_size_dt_strings(fdt)));
}
/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-11-28
 * ------------------------------------------------------------------
 * 
 * Description :1.fdt 의 version, magic 값 검사
 *		2.fdt->version 이 17보다작으면 에러, 17보다 크면 17로 세팅
 *		3.fdt struct 구조체에 맞는지 체크
 */		
static int _fdt_rw_check_header(void *fdt)
{
	// fdt 의 magic 과 version 을 check
	FDT_CHECK_HEADER(fdt);

	// fdt->version return. default fdt 버전은 17
	if (fdt_version(fdt) < 17)
		return -FDT_ERR_BADVERSION;
	// fdt 가 fdt 구조체 순서에 맞게 되어 있는지 check
	if (_fdt_blocks_misordered(fdt, sizeof(struct fdt_reserve_entry),
				   fdt_size_dt_struct(fdt)))
		return -FDT_ERR_BADLAYOUT;

	// fdt->version 이 17보다 큰 값이면 17로 세팅
	if (fdt_version(fdt) > 17)
		fdt_set_version(fdt, 17);

	return 0;
}

#define FDT_RW_CHECK_HEADER(fdt) \
	{ \
		int err; \
		if ((err = _fdt_rw_check_header(fdt)) != 0) \
			return err; \
	}

static inline int _fdt_data_size(void *fdt)
{
	/*
	 * fdt_off_dt_strings : fdt 구조체의 off_dt_strings 필드 주소 리턴함
	 * fdt_size_dt_strings : fdt 구조체의 size_dt_strings 필드 주소 리턴함 
	 * dtstring 필드의 끝 위치를 리턴
	 */
	return fdt_off_dt_strings(fdt) + fdt_size_dt_strings(fdt);
}
/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-11-28
 * ------------------------------------------------------------------
 *  1. splicepoint 유효성 check
 *  2. splicepoint 의 길이가 변경되므로, 
 *  splicepoint 뒤의 필드들의 위치를 재조정
 */
static int _fdt_splice(void *fdt, void *splicepoint, int oldlen, int newlen)
{
	char *p = splicepoint;
	char *end = (char *)fdt + _fdt_data_size(fdt);

	/* property *p 의 유효성 check */
	if (((p + oldlen) < p) || ((p + oldlen) > end))
		return -FDT_ERR_BADOFFSET;
	/* 새로 갱신되는 길이가 전체 fdt size보다 크면 에러  */
	if ((end - oldlen + newlen) > ((char *)fdt + fdt_totalsize(fdt)))
		return -FDT_ERR_NOSPACE;
	/*
	 * before memmove =>
	 * slicepoint   next tag 
	 * +------------+---------------------+
	 * | abc        | cpus{cpu0, 1...}    |
	 * +------------+---------------------+
	 *		p+oldlen              end
	 * after memmove =>
	 * +----------------+---------------------+
	 * | abc def        | cpus{cpu0, 1...}    |
	 * +----------------+-----------------^---+
	 *		    p+newlen          end(end 위치는 동일)
	 *		    delta 만큼 shift
	 */
	// memmove(p + newlen, p + oldlen, end - (p + oldlen) ); 
	memmove(p + newlen, p + oldlen, end - p - oldlen);
	return 0;
}

static int _fdt_splice_mem_rsv(void *fdt, struct fdt_reserve_entry *p,
			       int oldn, int newn)
{
	int delta = (newn - oldn) * sizeof(*p);
	int err;
	err = _fdt_splice(fdt, p, oldn * sizeof(*p), newn * sizeof(*p));
	if (err)
		return err;
	fdt_set_off_dt_struct(fdt, fdt_off_dt_struct(fdt) + delta);
	fdt_set_off_dt_strings(fdt, fdt_off_dt_strings(fdt) + delta);
	return 0;
}
/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-11-28
 * ------------------------------------------------------------------
 *  1. _fdt_splice : p 뒤의 필드 재조정 
 *  2. fdt의 size_dt_struct 와 off_dt_strings 값 갱신
 */
static int _fdt_splice_struct(void *fdt, void *p,
			      int oldlen, int newlen)
{
	int delta = newlen - oldlen;
	int err;

	if ((err = _fdt_splice(fdt, p, oldlen, newlen)))
		return err;

	/*
	 * fdt_set_size_dt_struct : fdt의  size_dt_struct 값 갱신 
	 * fdt_set_off_dt_strings : fdt의 off_dt_strings 값 갱신 
	 */
	fdt_set_size_dt_struct(fdt, fdt_size_dt_struct(fdt) + delta);
	fdt_set_off_dt_strings(fdt, fdt_off_dt_strings(fdt) + delta);
	return 0;
}
/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-11-28
 * ------------------------------------------------------------------
 * fdt 현재 구조 =>
 * +--------------------------------------------------------+
 * | FDT header | reserved | dt struct | dtstrings | buffer |
 * +--------------------------------------------------------+
 *
 * fdt_splice_string 수행 후 =>
 * +--------------------------------------------------------+
 * | FDT header | reserved | dt struct | dtstrings | buffer |
 * +------------------------------------------------^-------+
 *						    |
 *					   새로운 dt string 필드 추가 됨	
 */
static int _fdt_splice_string(void *fdt, int newlen)
{
	void *p = (char *)fdt
		+ fdt_off_dt_strings(fdt) + fdt_size_dt_strings(fdt);
	int err;

	if ((err = _fdt_splice(fdt, p, 0, newlen)))
		return err;

	fdt_set_size_dt_strings(fdt, fdt_size_dt_strings(fdt) + newlen);
	return 0;
}
/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-11-28
 * ------------------------------------------------------------------
 * 1. fdt에서 dtsting 에서 S 문자열의 시작위치 offset을 리턴함 
 * 2. fdt에서 s 문자열이 존재하지 않을 경우
 *    a. dtstrings 맨 뒤에 s 문자열 추가를 위해 공간 확보 
 *    b. s 문자열을 추가된 공간에 복사한다
 *    c. dtsrings 에서 s 문자열의 시작위치 offset을 리턴함
 */
static int _fdt_find_add_string(void *fdt, const char *s)
{
	char *strtab = (char *)fdt + fdt_off_dt_strings(fdt);
	const char *p;
	char *new;
	int len = strlen(s) + 1;
	int err;

	/* strtab ~ fdt_sisze_dt_strings(fdt) 범위 안에서 
	* s 문자열을 찾아서 문자열의 시작 주소를 리턴함
	*/
	p = _fdt_find_string(strtab, fdt_size_dt_strings(fdt), s);
	if (p)
		/* found it */
		return (p - strtab);

	new = strtab + fdt_size_dt_strings(fdt);
	err = _fdt_splice_string(fdt, len);
	if (err)
		return err;

	memcpy(new, s, len);
	return (new - strtab);
}

int fdt_add_mem_rsv(void *fdt, uint64_t address, uint64_t size)
{
	struct fdt_reserve_entry *re;
	int err;

	FDT_RW_CHECK_HEADER(fdt);

	re = _fdt_mem_rsv_w(fdt, fdt_num_mem_rsv(fdt));
	err = _fdt_splice_mem_rsv(fdt, re, 0, 1);
	if (err)
		return err;

	re->address = cpu_to_fdt64(address);
	re->size = cpu_to_fdt64(size);
	return 0;
}

int fdt_del_mem_rsv(void *fdt, int n)
{
	struct fdt_reserve_entry *re = _fdt_mem_rsv_w(fdt, n);
	int err;

	FDT_RW_CHECK_HEADER(fdt);

	if (n >= fdt_num_mem_rsv(fdt))
		return -FDT_ERR_NOTFOUND;

	err = _fdt_splice_mem_rsv(fdt, re, 1, 0);
	if (err)
		return err;
	return 0;
}

/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-11-28
 * ------------------------------------------------------------------
 * setprop_string(fdt, "/chosen", "bootargs", cmdline); 함수에서 
 *      호출했다는 전제로  분석
 * fdt : device tree pointer, 
 * nodeoffset : "/chosen" node offset, 
 * name : "bootargs" property, 
 * len : 새로 갱신될 bootargs 속성의 길이,
 * prop : fdt_property pointer 변수 
 * 1. *prop = bootargs 속성 값을 얻어 온다. 
 *    oldlen = bootargs 속성 값의 길이
 * 2. prop->data 길이가 변경 되므로, prop 뒤의 필드들이
 *    변경된 prop->data 뒤에 오도록 재조정
 */
static int _fdt_resize_property(void *fdt, int nodeoffset, const char *name,
				int len, struct fdt_property **prop)
{
	int oldlen;
	int err;

	/* 
	 * prop = bootargs, oldlen = bootargs의 value 길이
	 */
	*prop = fdt_get_property_w(fdt, nodeoffset, name, &oldlen);
	if (! (*prop))
		// bootargs를 못 찾으면 error를 return
		return oldlen;

	// 
	// (*prop)->data : property의 value 
	// FDT_TAGALIGN(oldlen) : 현재 property의 길이
	// FDT_TAGALIGN(len) : update 할 property의 길이 
	if ((err = _fdt_splice_struct(fdt, (*prop)->data, FDT_TAGALIGN(oldlen),
				      FDT_TAGALIGN(len))))
		return err;

	(*prop)->len = cpu_to_fdt32(len);
	return 0;
}
/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-11-28
 * ------------------------------------------------------------------
 * 1. offset 유효성 check 하고 nextoffset에 다음 tag의 offset값 할당
 * 2. name 필드의 offset값을 찾아서 namestroff값 할당 
 */
static int _fdt_add_property(void *fdt, int nodeoffset, const char *name,
			     int len, struct fdt_property **prop)
{
	int proplen;
	int nextoffset;
	int namestroff;
	int err;

	if ((nextoffset = _fdt_check_node_offset(fdt, nodeoffset)) < 0)
		return nextoffset;

	/* 
	 * name 문자열을 dtstrings 에서 찾고 없으면 추가한다. 
	 * 반환값은 dtstrings 시작 위치에서 문자열이 시작되는 위치의 offset 
	 */ 
	namestroff = _fdt_find_add_string(fdt, name);
	if (namestroff < 0)
		return namestroff;

	/*
	 * dt_struct 에서 해당 property 주소를 찾는다.
	 */
	*prop = _fdt_offset_ptr_w(fdt, nextoffset);
	proplen = sizeof(**prop) + FDT_TAGALIGN(len);

	/*
	 * dt_struct 에서 prop를 추가할 공간을 확보한다.
	 */
	err = _fdt_splice_struct(fdt, *prop, 0, proplen);
	if (err)
		return err;

	/*
	 * prop member 갱신함 
	 */
	(*prop)->tag = cpu_to_fdt32(FDT_PROP);
	(*prop)->nameoff = cpu_to_fdt32(namestroff);
	(*prop)->len = cpu_to_fdt32(len);
	return 0;
}

int fdt_set_name(void *fdt, int nodeoffset, const char *name)
{
	char *namep;
	int oldlen, newlen;
	int err;

	FDT_RW_CHECK_HEADER(fdt);

	namep = (char *)(uintptr_t)fdt_get_name(fdt, nodeoffset, &oldlen);
	if (!namep)
		return oldlen;

	newlen = strlen(name);

	err = _fdt_splice_struct(fdt, namep, FDT_TAGALIGN(oldlen+1),
				 FDT_TAGALIGN(newlen+1));
	if (err)
		return err;

	memcpy(namep, name, newlen+1);
	return 0;
}
/* ==================================================================
 * 팀:   Iamroot ARM Kernel 분석 12차 D조 (http://www.iamroot.org)
 * 날짜: 2015-11-28
 * ------------------------------------------------------------------
 * nodeoffset : "/chosen" node 의 offset 
 * name : "bootargs" property name
 * val : bootargs property 값과 인자로 넘어온 atags cmdline이 합쳐진 string 
 * len : strlen(val)
 * 1. FDT_RW_CHECK_HEADER : fdt 의 유효성 check
 * 2. _fdt_resize_property : prop 를 갱신해야 되기에 dt_strings size 재조정 및 
 *			     갱신하려는 prop 뒤의 property를 shift 
 * 3. memcpy : val를 지정된 prop->data에 저장
 */
int fdt_setprop(void *fdt, int nodeoffset, const char *name,
		const void *val, int len)
{
	struct fdt_property *prop;
	int err;

	FDT_RW_CHECK_HEADER(fdt);

	err = _fdt_resize_property(fdt, nodeoffset, name, len, &prop);
	if (err == -FDT_ERR_NOTFOUND)
		err = _fdt_add_property(fdt, nodeoffset, name, len, &prop);
	if (err)
		return err;

	/*
	 * property 에 새로운 val 값으로 memcpy 
	 */
	memcpy(prop->data, val, len);
	return 0;
}

int fdt_appendprop(void *fdt, int nodeoffset, const char *name,
		   const void *val, int len)
{
	struct fdt_property *prop;
	int err, oldlen, newlen;

	FDT_RW_CHECK_HEADER(fdt);

	prop = fdt_get_property_w(fdt, nodeoffset, name, &oldlen);
	if (prop) {
		newlen = len + oldlen;
		err = _fdt_splice_struct(fdt, prop->data,
					 FDT_TAGALIGN(oldlen),
					 FDT_TAGALIGN(newlen));
		if (err)
			return err;
		prop->len = cpu_to_fdt32(newlen);
		memcpy(prop->data + oldlen, val, len);
	} else {
		err = _fdt_add_property(fdt, nodeoffset, name, len, &prop);
		if (err)
			return err;
		memcpy(prop->data, val, len);
	}
	return 0;
}

int fdt_delprop(void *fdt, int nodeoffset, const char *name)
{
	struct fdt_property *prop;
	int len, proplen;

	FDT_RW_CHECK_HEADER(fdt);

	prop = fdt_get_property_w(fdt, nodeoffset, name, &len);
	if (! prop)
		return len;

	proplen = sizeof(*prop) + FDT_TAGALIGN(len);
	return _fdt_splice_struct(fdt, prop, proplen, 0);
}

int fdt_add_subnode_namelen(void *fdt, int parentoffset,
			    const char *name, int namelen)
{
	struct fdt_node_header *nh;
	int offset, nextoffset;
	int nodelen;
	int err;
	uint32_t tag;
	uint32_t *endtag;

	FDT_RW_CHECK_HEADER(fdt);

	/* 여기서는 추가하는 중이기때문에 서브노드에 추가할 같은 이름이 있어서는
	   안된다. 따라서 아래 함수의 결과는 에러값(NOT_FOUND)가 나올것이다. */
	offset = fdt_subnode_offset_namelen(fdt, parentoffset, name, namelen);
	if (offset >= 0)
		return -FDT_ERR_EXISTS;
	else if (offset != -FDT_ERR_NOTFOUND)
		return offset;

	/* 
	 * +------+-------------------------+
	 * | Node | FDT_BEGIN_NODE          |
	 * |      +-------------------------+
	 * |      | Node Name               |
	 * |      +----------+--------------+
	 * |      | Property | FDT_PROP     |
	 * |      |          +--------------+
	 * |      |          | Value Length |
	 * |      |          +--------------+
	 * |      |          | Name Offset  |
	 * |      |          +--------------+
	 * |      |          | Value        |
	 * +------+----------+--------------+
	 * | Sub  | FDT_BEGIN_NODE          | << 이렇게 추가(서브노드)
	 * | Node |                         | 
	 * |      +-------------------------+
	 * |      |        ...              |
	 *
	 * 다음 노드를 가르키게 한다.
	 * 즉, 첫번째 서브노드로 add 하겠다는 의미가 된다.
	 */
	/* Try to place the new node after the parent's properties */
	fdt_next_tag(fdt, parentoffset, &nextoffset); /* skip the BEGIN_NODE */
	do {
		offset = nextoffset;
		tag = fdt_next_tag(fdt, offset, &nextoffset);
	} while ((tag == FDT_PROP) || (tag == FDT_NOP));

	/* fdt_node_header *nh
	   nodelen을 계산한다.
	*/
	nh = _fdt_offset_ptr_w(fdt, offset);
	nodelen = sizeof(*nh) + FDT_TAGALIGN(namelen+1) + FDT_TAGSIZE;

	err = _fdt_splice_struct(fdt, nh, 0, nodelen);
	if (err)
		return err;

	nh->tag = cpu_to_fdt32(FDT_BEGIN_NODE);
	memset(nh->name, 0, FDT_TAGALIGN(namelen+1));
	memcpy(nh->name, name, namelen);
	endtag = (uint32_t *)((char *)nh + nodelen - FDT_TAGSIZE);
	*endtag = cpu_to_fdt32(FDT_END_NODE);

	return offset;
}

int fdt_add_subnode(void *fdt, int parentoffset, const char *name)
{
	return fdt_add_subnode_namelen(fdt, parentoffset, name, strlen(name));
}

int fdt_del_node(void *fdt, int nodeoffset)
{
	int endoffset;

	FDT_RW_CHECK_HEADER(fdt);

	endoffset = _fdt_node_end_offset(fdt, nodeoffset);
	if (endoffset < 0)
		return endoffset;

	return _fdt_splice_struct(fdt, _fdt_offset_ptr_w(fdt, nodeoffset),
				  endoffset - nodeoffset, 0);
}

/*
 * in	old	순서가 맞지 않은 fdt
 * out	new	순서를 맞춘 fdt
 */
static void _fdt_packblocks(const char *old, char *new,
			    int mem_rsv_size, int struct_size)
{
	/**
	 * FDT 순서는 아래와 같다.
	 * +--------------------+ <-- fdt
	 * | fdt_header(40byte) |
	 * +--------------------+ <-- mem_rsv_off
	 * | mem_rsvmap         |
	 * | ...                |
	 * +--------------------+ <-- struct_off
	 * | dt_struct          |
	 * | ...                |
	 * +--------------------+ <-- strings_off
	 * | dt_strings         |
	 * | ...                |
	 * +--------------------+ <-- total_size
	 */
	int mem_rsv_off, struct_off, strings_off;

	mem_rsv_off = FDT_ALIGN(sizeof(struct fdt_header), 8);
	struct_off = mem_rsv_off + mem_rsv_size;
	strings_off = struct_off + struct_size;

	// 순서에 맞게 fdt의 각각의 블럭들의 정보를 설정 및 복사한다.
	memmove(new + mem_rsv_off, old + fdt_off_mem_rsvmap(old), mem_rsv_size);
	fdt_set_off_mem_rsvmap(new, mem_rsv_off);

	memmove(new + struct_off, old + fdt_off_dt_struct(old), struct_size);
	fdt_set_off_dt_struct(new, struct_off);
	fdt_set_size_dt_struct(new, struct_size);

	memmove(new + strings_off, old + fdt_off_dt_strings(old),
		fdt_size_dt_strings(old));
	fdt_set_off_dt_strings(new, strings_off);
	fdt_set_size_dt_strings(new, fdt_size_dt_strings(old));
}

/*
 * fdt struct의 공간 확보를 하는것이 아닐까?
 *
 * in	fdt	fdt
 * out	buf	fdt
 * in	bufsize	fdt의 총 크기
 * return	0 성공,
 * 		0미만 실패
 *
 * val = 0x 30, *val=10
 * const int * ptr1=&val;   10을 못바꿈 (data를 못바꿈)
 * int * const ptr2 = &val2 ; 30을 못바꿈 (주소 번지를 못바꿈)
 * 
 * fdt = DTB의 시작 주소
*/
int fdt_open_into(const void *fdt, void *buf, int bufsize)
{
	int err;
	int mem_rsv_size, struct_size;
	int newsize;
	const char *fdtstart = fdt;
	/*bufsize는 32K 1MB제한을 건것이어서 아래 macro에서 struct에서 가져 온
	것과 다를 수 있다.

	fdt_totalsize(fdt) 설명:
		struct fdt_header * pf = fdt;
		return pf->totalsize
	 */
	const char *fdtend = fdtstart + fdt_totalsize(fdt);
	char *tmp;

	// 아래 함수에서 fdt의 MAGIC, VERSION 을 체크해서 문제시 에러로 리턴함.
	FDT_CHECK_HEADER(fdt);

	/* memory reserve map의 총 size를 계산한다. */
	mem_rsv_size = (fdt_num_mem_rsv(fdt)+1)
		* sizeof(struct fdt_reserve_entry);

	/* DEFAULT_FDT_VERSION이 17이다. */
	if (fdt_version(fdt) >= 17) {
		struct_size = fdt_size_dt_struct(fdt);
	} else {
		/* 버전 17이전에는 dt_struct  struct_size 멤버가 없었다
		 * struct_size 를 모두 구할때까지 다음 tag를 계속 구한다.
		 */
		struct_size = 0;
		while (fdt_next_tag(fdt, struct_size, &struct_size) != FDT_END)
			;

		// 만약 size가 음수가 된다면, 에러이다.
		if (struct_size < 0)
			return struct_size;
	}

	if (!_fdt_blocks_misordered(fdt, mem_rsv_size, struct_size)) {
		// 순서가 올바른 경우
		/* no further work necessary */
		err = fdt_move(fdt, buf, bufsize);
		if (err)
			return err;

		/* scripts/dtc/libfdt/libfdt.h 파일에 __fdt_set_hdr(version); 가
		 * 선언 되어 있으며 이것은 아래와 같은 inline 함수로 대처 된다.
		 *  static inline void fdt_set_version(void *fdt, uint32_t val)
		 *  {
		 *          struct fdt_header *fdth = (struct fdt_header*)fdt;
		 *          fdth->version = cpu_to_fdt32(val);
		 *  }
		 */
		fdt_set_version(buf, 17);
		fdt_set_size_dt_struct(buf, struct_size);
		fdt_set_totalsize(buf, bufsize);
		return 0;
	}

	// 순서가 올바르지 않는 경우
	/* Need to reorder */
	newsize = FDT_ALIGN(sizeof(struct fdt_header), 8) + mem_rsv_size
		+ struct_size + fdt_size_dt_strings(fdt);

	if (bufsize < newsize)
		return -FDT_ERR_NOSPACE;

	/* First attempt to build converted tree at beginning of buffer */
	tmp = buf;
	/* But if that overlaps with the old tree... */
	/*
	 * fdtend  --> +-------------+
	 *             |             |
	 *             |             |
	 *             |     +-------+-----+ <-- tmp + newsize
	 *             |     |       |     |
	 * fdtstart -->+-----+-------+     |
	 *                   |             |
	 *                   |             |
	 *                   +-------------+ <-- tmp
	 */
	if (((tmp + newsize) > fdtstart) && (tmp < fdtend)) {
		/* Try right after the old tree instead */
		/* 겹쳤을 경우 tmp를 fdtend로 */
		tmp = (char *)(uintptr_t)fdtend;
		// bufsize 보다 크면 에러
		if ((tmp + newsize) > ((char *)buf + bufsize))
			return -FDT_ERR_NOSPACE;
	}

	// tmp에 fdt 정렬에 맞쳐 패키징한다.
	_fdt_packblocks(fdt, tmp, mem_rsv_size, struct_size);
	memmove(buf, tmp, newsize);

	fdt_set_magic(buf, FDT_MAGIC);
	fdt_set_totalsize(buf, bufsize);
	fdt_set_version(buf, 17);
	fdt_set_last_comp_version(buf, 16);
	fdt_set_boot_cpuid_phys(buf, fdt_boot_cpuid_phys(fdt));

	return 0;
}

int fdt_pack(void *fdt)
{
	int mem_rsv_size;

	FDT_RW_CHECK_HEADER(fdt);

	mem_rsv_size = (fdt_num_mem_rsv(fdt)+1)
		* sizeof(struct fdt_reserve_entry);
	_fdt_packblocks(fdt, fdt, mem_rsv_size, fdt_size_dt_struct(fdt));
	fdt_set_totalsize(fdt, _fdt_data_size(fdt));

	return 0;
}
