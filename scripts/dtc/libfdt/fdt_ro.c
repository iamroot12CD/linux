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

static int _fdt_nodename_eq(const void *fdt, int offset,
			    const char *s, int len)
{
	/*
	 * struct fdt_node_header {
	 * uint32_t tag;
	 * char name[0];
	 * };

	 * fdt_node_header의 name을 가져옴
	 * (len+1)만큼 가져오는 것은 밑에서 '@'을 체크하기 위해?
	*/
	const char *p = fdt_offset_ptr(fdt, offset + FDT_TAGSIZE, len+1);

	if (! p)
		/* short match */
		return 0;

	if (memcmp(p, s, len) != 0)
		return 0;

	if (p[len] == '\0')
		return 1;
	else if (!memchr(s, '@', len) && (p[len] == '@'))
		return 1;
	else
		return 0;
}

const char *fdt_string(const void *fdt, int stroffset)
{
	return (const char *)fdt + fdt_off_dt_strings(fdt) + stroffset;
}

/*
 * stroffset: property name offset
 * s: 찾고자하는 propert의 이름
 * 찾고자 하는 property가 맞는지 propert명 체크
*/
static int _fdt_string_eq(const void *fdt, int stroffset,
			  const char *s, int len)
{
	const char *p = fdt_string(fdt, stroffset);

	return (strlen(p) == len) && (memcmp(p, s, len) == 0);
}

/* IAMROOT-12CD (2016-08-20):
 * --------------------------
 * 라즈베리파이2에서는 _fdt_mem_rsv 영역에 아무런 값이 없다.
 */
int fdt_get_mem_rsv(const void *fdt, int n, uint64_t *address, uint64_t *size)
{
	FDT_CHECK_HEADER(fdt);
	*address = fdt64_to_cpu(_fdt_mem_rsv(fdt, n)->address);
	*size = fdt64_to_cpu(_fdt_mem_rsv(fdt, n)->size);
	return 0;
}

/* memory reserve map의 개수를 구한다.*/ 
int fdt_num_mem_rsv(const void *fdt)
{
	int i = 0;

	/* size가 0이 아니면 reserve map이 있다고 판단한다. */
	while (fdt64_to_cpu(_fdt_mem_rsv(fdt, i)->size) != 0)
		i++;
	return i;
}

/*
 * 다음 property를 찾아서 해당 offset을 리턴
*/
static int _nextprop(const void *fdt, int offset)
{
	uint32_t tag;
	int nextoffset;

	do {
		/*
		 * offset: property의 offset
		 * nextoffset: property offset의 다음 offset
		*/
		tag = fdt_next_tag(fdt, offset, &nextoffset);

		switch (tag) {
		case FDT_END:
			/*
			 * 이미 FDT_END인데 nextoffset이 있다면 에러
			 * (FDT_END인데 다음 offset이 있다가 말이 안됨)
			 * nextoffset<0 라면 에러값이 들어가 있음
			*/
			if (nextoffset >= 0)
				return -FDT_ERR_BADSTRUCTURE;
			else
				return nextoffset;

		// property라면 해당 offset 리턴
		case FDT_PROP:
			return offset;
		}
		// property를 찾을때까지 다음 offset으로 이동
		offset = nextoffset;
	} while (tag == FDT_NOP);

	return -FDT_ERR_NOTFOUND;
}

/*
 * fdt에서 offset를 기준으로 하위 노드를 탐색
 * (참고로 꼭 root가 아닐수 있음 즉, offset는 상대적인 위치임)

 * 참고: http://iamroot.org/wiki/lib/exe/fetch.php?media=%EC%8A%A4%ED%84%B0%EB%94%94:dtb_structure.png
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
*/
int fdt_subnode_offset_namelen(const void *fdt, int offset,
			       const char *name, int namelen)
{
	int depth;

	FDT_CHECK_HEADER(fdt);

	/*
	 * root부터 시작해서(depth=0) node를 탐색
	 * 현재 탐색중인 node의 depth를 받아와서
	*/
	for (depth = 0;
	     (offset >= 0) && (depth >= 0);
	     offset = fdt_next_node(fdt, offset, &depth))
		/*
		 * root 입장으로 봤을때 depth가 1인 node들 중에서
		 * 찾고자 하는 node가 맞다면 node offset을 리턴
		*/
		if ((depth == 1)
		    && _fdt_nodename_eq(fdt, offset, name, namelen))
			return offset;

	// 탐색했더니 찾고자 하는 node가 없는 상황
	if (depth < 0)
		return -FDT_ERR_NOTFOUND;
	return offset; /* error */
}

int fdt_subnode_offset(const void *fdt, int parentoffset,
		       const char *name)
{
	return fdt_subnode_offset_namelen(fdt, parentoffset, name, strlen(name));
}

/*
 * chosen {
 * bootargs = "console=ttyS0,115200 ubi.mtd=4 root=ubi0:rootfs rootfstype=ubifs";
 * };

 * 호출: int offset = fdt_path_offset(fdt, "/chosen");
*/
int fdt_path_offset(const void *fdt, const char *path)
{
	/*
	 * p : "chosen"의 시작 주소
	 * end : "chosen"의 끝 주소
	*/
	const char *end = path + strlen(path);
	const char *p = path;
	int offset = 0;

	FDT_CHECK_HEADER(fdt);

	/* see if we have an alias */
	if (*path != '/') {
		// path가 '/'로 시작하지 않으면 path내에서 '/' 위치 찾음
		const char *q = strchr(path, '/');

		// path내에 '/'가 없으면 끝 주소로 지정
		if (!q)
			q = end;

		/*
		 * '/'로 시작하지 않았다면 alias라고 가정하고 원래 node명 탐색
		 * p: alias명
		 * q-p: alias 길이
		*/ 
		p = fdt_get_alias_namelen(fdt, p, q - p);
		// alias도 아닌경우 에러 리턴
		if (!p)
			return -FDT_ERR_BADPATH;
		offset = fdt_path_offset(fdt, p);

		p = q;
	}

	/*
	 * "chosen"의 alias가 없으면 여기서 탐색
	 * 여기서는 node의 이름이 "chosen"인지 아닌지 탐색
	*/
	while (*p) {
		const char *q;

		// path내에서 '/'가 아닌곳까지 이동
		while (*p == '/')
			p++;
		// path 끝까지 이동한 경우
		if (! *p)
			return offset;
		// p를 기준으로 다음 '/' 위치까지 찾아서
		q = strchr(p, '/');
		// 다음 '/'가 없다면 path의 끝으로 지정
		if (! q)
			q = end;
	
		// path에서 '/'로 split 해보면서
		// 하위 node들중 해당 이름의 node가 있는지 탐색
		offset = fdt_subnode_offset_namelen(fdt, offset, p, q-p);
		if (offset < 0)
			return offset;

		p = q;
	}

	return offset;
}

const char *fdt_get_name(const void *fdt, int nodeoffset, int *len)
{
	const struct fdt_node_header *nh = _fdt_offset_ptr(fdt, nodeoffset);
	int err;

	if (((err = fdt_check_header(fdt)) != 0)
	    || ((err = _fdt_check_node_offset(fdt, nodeoffset)) < 0))
			goto fail;

	if (len)
		*len = strlen(nh->name);

	return nh->name;

 fail:
	if (len)
		*len = err;
	return NULL;
}

/*
 * 첫번째 property의 offset을 가져온다.
*/
int fdt_first_property_offset(const void *fdt, int nodeoffset)
{
	int offset;

	// node 유효성 체크
	if ((offset = _fdt_check_node_offset(fdt, nodeoffset)) < 0)
		return offset;

	return _nextprop(fdt, offset);
}

/*
 * 그 다음 property의 offset을 가져온다.
*/
int fdt_next_property_offset(const void *fdt, int offset)
{
	if ((offset = _fdt_check_prop_offset(fdt, offset)) < 0)
		return offset;

	return _nextprop(fdt, offset);
}

/*
 * offset: propert의 offset
 * lenp: propert내의 value 길이 저장
 * offset을 이용하여 property에 접근 및 property 구조체와, value 길이를 받아옴
*/
const struct fdt_property *fdt_get_property_by_offset(const void *fdt,
						      int offset,
						      int *lenp)
{
	int err;
	const struct fdt_property *prop;

	// property의 유효성을 체크 
	if ((err = _fdt_check_prop_offset(fdt, offset)) < 0) {
		if (lenp)
			*lenp = err;
		return NULL;
	}

	// property 구조체를 가져옴
	prop = _fdt_offset_ptr(fdt, offset);

	// property의 value의 길이 저장
	if (lenp)
		*lenp = fdt32_to_cpu(prop->len);

	return prop;
}

/*
 * chosen {
 * bootargs="console=ttyS0,115200 ubi.mtd=4 root=ubi0:rootfs rootfstype=ubifs";
 * };

 * offset: node "/chosen"의 offset
 * name: property "bootargs"
 * namelen: property "bootargs" 길이
 * lenp: property 내용의 길이
*/
const struct fdt_property *fdt_get_property_namelen(const void *fdt,
						    int offset,
						    const char *name,
						    int namelen, int *lenp)
{
	/*
	 * node안의 property들을 순회하면서 찾고자 하는 name의 property 리턴
	 * 참고로 아래 for문에서 offset을 계속 받아오면서 값을 체크하고 있음
	 * 즉, 순회하면서 찾는 property가 없거나 탐색시 에러가 나면 탐색 중단
	*/
	for (offset = fdt_first_property_offset(fdt, offset);
	     (offset >= 0);
	     (offset = fdt_next_property_offset(fdt, offset))) {
		const struct fdt_property *prop;

		/*
		 * offset만큼 위치한 property에 접근해서
		 * lenp에 property의 value의 길이를 저장
		 * 하지만 에러가 나는 경우엔 for문을 빠져나감
		*/
		if (!(prop = fdt_get_property_by_offset(fdt, offset, lenp))) {
			offset = -FDT_ERR_INTERNAL;
			break;
		}
		/*
		 * 찾고자 하는 property가 맞는지 확인하고
		 * 맞다면 가져온 property 구조체를 리턴
		*/
		if (_fdt_string_eq(fdt, fdt32_to_cpu(prop->nameoff),
				   name, namelen))
			return prop;
	}

	/*
	 * 찾는 property가 없는경우 or property 접근하다 에러가 난 경우
	 * lenp에 에러값 저장
	*/
	if (lenp)
		*lenp = offset;

	return NULL;
}

const struct fdt_property *fdt_get_property(const void *fdt,
					    int nodeoffset,
					    const char *name, int *lenp)
{
	return fdt_get_property_namelen(fdt, nodeoffset, name,
					strlen(name), lenp);
}

/*
 * nodeoffset: 탐색 대상이되는 node의 offset
 * name: 찾을 property 이름
 * namelen: 찾을 property 이름 길이
 * lenp: property value 길이
*/
const void *fdt_getprop_namelen(const void *fdt, int nodeoffset,
				const char *name, int namelen, int *lenp)
{
	const struct fdt_property *prop;

	// 찾고자 하는 property가 있는지 확인하고 property의 구조체를 가져옴
	prop = fdt_get_property_namelen(fdt, nodeoffset, name, namelen, lenp);
	// 가져온 property 구조체가 없다면 NULL 리턴
	if (! prop)
		return NULL;

	// 가져온 property 구조체가 있다면 value을 리턴
	return prop->data;
}

const void *fdt_getprop_by_offset(const void *fdt, int offset,
				  const char **namep, int *lenp)
{
	const struct fdt_property *prop;

	prop = fdt_get_property_by_offset(fdt, offset, lenp);
	if (!prop)
		return NULL;
	if (namep)
		*namep = fdt_string(fdt, fdt32_to_cpu(prop->nameoff));
	return prop->data;
}

/*
 * nodeoffset: 탐색 대상이 되는 node의 offset
 * name: 찾고자 하는 property 이름
 * lenp: 찾고자 하는 property의 value 길이
*/
const void *fdt_getprop(const void *fdt, int nodeoffset,
			const char *name, int *lenp)
{
	return fdt_getprop_namelen(fdt, nodeoffset, name, strlen(name), lenp);
}

uint32_t fdt_get_phandle(const void *fdt, int nodeoffset)
{
	const uint32_t *php;
	int len;

	/* FIXME: This is a bit sub-optimal, since we potentially scan
	 * over all the properties twice. */
	php = fdt_getprop(fdt, nodeoffset, "phandle", &len);
	if (!php || (len != sizeof(*php))) {
		php = fdt_getprop(fdt, nodeoffset, "linux,phandle", &len);
		if (!php || (len != sizeof(*php)))
			return 0;
	}

	return fdt32_to_cpu(*php);
}

/*
 * name: alias
 * namelen: alias 길이
*/
const char *fdt_get_alias_namelen(const void *fdt,
				  const char *name, int namelen)
{
	int aliasoffset;

	// aliases에 정의된 alias가 있는지 찾아보고 alias offset을 가져옴
	aliasoffset = fdt_path_offset(fdt, "/aliases");
	// alias offset이 에러값이라면 NULL 리턴
	if (aliasoffset < 0)
		return NULL;

	/*
	 * alias offset이 유효하다면
	 * alias에 해당하는 property의 value를 가져옴
	*/
	return fdt_getprop_namelen(fdt, aliasoffset, name, namelen, NULL);
}

const char *fdt_get_alias(const void *fdt, const char *name)
{
	return fdt_get_alias_namelen(fdt, name, strlen(name));
}

int fdt_get_path(const void *fdt, int nodeoffset, char *buf, int buflen)
{
	int pdepth = 0, p = 0;
	int offset, depth, namelen;
	const char *name;

	FDT_CHECK_HEADER(fdt);

	if (buflen < 2)
		return -FDT_ERR_NOSPACE;

	for (offset = 0, depth = 0;
	     (offset >= 0) && (offset <= nodeoffset);
	     offset = fdt_next_node(fdt, offset, &depth)) {
		while (pdepth > depth) {
			do {
				p--;
			} while (buf[p-1] != '/');
			pdepth--;
		}

		if (pdepth >= depth) {
			name = fdt_get_name(fdt, offset, &namelen);
			if (!name)
				return namelen;
			if ((p + namelen + 1) <= buflen) {
				memcpy(buf + p, name, namelen);
				p += namelen;
				buf[p++] = '/';
				pdepth++;
			}
		}

		if (offset == nodeoffset) {
			if (pdepth < (depth + 1))
				return -FDT_ERR_NOSPACE;

			if (p > 1) /* special case so that root path is "/", not "" */
				p--;
			buf[p] = '\0';
			return 0;
		}
	}

	if ((offset == -FDT_ERR_NOTFOUND) || (offset >= 0))
		return -FDT_ERR_BADOFFSET;
	else if (offset == -FDT_ERR_BADOFFSET)
		return -FDT_ERR_BADSTRUCTURE;

	return offset; /* error from fdt_next_node() */
}

int fdt_supernode_atdepth_offset(const void *fdt, int nodeoffset,
				 int supernodedepth, int *nodedepth)
{
	int offset, depth;
	int supernodeoffset = -FDT_ERR_INTERNAL;

	FDT_CHECK_HEADER(fdt);

	if (supernodedepth < 0)
		return -FDT_ERR_NOTFOUND;

	for (offset = 0, depth = 0;
	     (offset >= 0) && (offset <= nodeoffset);
	     offset = fdt_next_node(fdt, offset, &depth)) {
		if (depth == supernodedepth)
			supernodeoffset = offset;

		if (offset == nodeoffset) {
			if (nodedepth)
				*nodedepth = depth;

			if (supernodedepth > depth)
				return -FDT_ERR_NOTFOUND;
			else
				return supernodeoffset;
		}
	}

	if ((offset == -FDT_ERR_NOTFOUND) || (offset >= 0))
		return -FDT_ERR_BADOFFSET;
	else if (offset == -FDT_ERR_BADOFFSET)
		return -FDT_ERR_BADSTRUCTURE;

	return offset; /* error from fdt_next_node() */
}

int fdt_node_depth(const void *fdt, int nodeoffset)
{
	int nodedepth;
	int err;

	err = fdt_supernode_atdepth_offset(fdt, nodeoffset, 0, &nodedepth);
	if (err)
		return (err < 0) ? err : -FDT_ERR_INTERNAL;
	return nodedepth;
}

int fdt_parent_offset(const void *fdt, int nodeoffset)
{
	int nodedepth = fdt_node_depth(fdt, nodeoffset);

	if (nodedepth < 0)
		return nodedepth;
	return fdt_supernode_atdepth_offset(fdt, nodeoffset,
					    nodedepth - 1, NULL);
}

int fdt_node_offset_by_prop_value(const void *fdt, int startoffset,
				  const char *propname,
				  const void *propval, int proplen)
{
	int offset;
	const void *val;
	int len;

	FDT_CHECK_HEADER(fdt);

	/* FIXME: The algorithm here is pretty horrible: we scan each
	 * property of a node in fdt_getprop(), then if that didn't
	 * find what we want, we scan over them again making our way
	 * to the next node.  Still it's the easiest to implement
	 * approach; performance can come later. */
	for (offset = fdt_next_node(fdt, startoffset, NULL);
	     offset >= 0;
	     offset = fdt_next_node(fdt, offset, NULL)) {
		val = fdt_getprop(fdt, offset, propname, &len);
		if (val && (len == proplen)
		    && (memcmp(val, propval, len) == 0))
			return offset;
	}

	return offset; /* error from fdt_next_node() */
}

int fdt_node_offset_by_phandle(const void *fdt, uint32_t phandle)
{
	int offset;

	if ((phandle == 0) || (phandle == -1))
		return -FDT_ERR_BADPHANDLE;

	FDT_CHECK_HEADER(fdt);

	/* FIXME: The algorithm here is pretty horrible: we
	 * potentially scan each property of a node in
	 * fdt_get_phandle(), then if that didn't find what
	 * we want, we scan over them again making our way to the next
	 * node.  Still it's the easiest to implement approach;
	 * performance can come later. */
	for (offset = fdt_next_node(fdt, -1, NULL);
	     offset >= 0;
	     offset = fdt_next_node(fdt, offset, NULL)) {
		if (fdt_get_phandle(fdt, offset) == phandle)
			return offset;
	}

	return offset; /* error from fdt_next_node() */
}

static int _fdt_stringlist_contains(const char *strlist, int listlen,
				    const char *str)
{
	int len = strlen(str);
	const char *p;

	while (listlen >= len) {
		if (memcmp(str, strlist, len+1) == 0)
			return 1;
		p = memchr(strlist, '\0', listlen);
		if (!p)
			return 0; /* malformed strlist.. */
		listlen -= (p-strlist) + 1;
		strlist = p + 1;
	}
	return 0;
}

int fdt_node_check_compatible(const void *fdt, int nodeoffset,
			      const char *compatible)
{
	const void *prop;
	int len;

	prop = fdt_getprop(fdt, nodeoffset, "compatible", &len);
	if (!prop)
		return len;
	if (_fdt_stringlist_contains(prop, len, compatible))
		return 0;
	else
		return 1;
}

int fdt_node_offset_by_compatible(const void *fdt, int startoffset,
				  const char *compatible)
{
	int offset, err;

	FDT_CHECK_HEADER(fdt);

	/* FIXME: The algorithm here is pretty horrible: we scan each
	 * property of a node in fdt_node_check_compatible(), then if
	 * that didn't find what we want, we scan over them again
	 * making our way to the next node.  Still it's the easiest to
	 * implement approach; performance can come later. */
	for (offset = fdt_next_node(fdt, startoffset, NULL);
	     offset >= 0;
	     offset = fdt_next_node(fdt, offset, NULL)) {
		err = fdt_node_check_compatible(fdt, offset, compatible);
		if ((err < 0) && (err != -FDT_ERR_NOTFOUND))
			return err;
		else if (err == 0)
			return offset;
	}

	return offset; /* error from fdt_next_node() */
}
