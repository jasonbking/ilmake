/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * String utility functions with dynamic memory management.
 */

/*
 * Copyright 2018, Joyent, Inc.
 */

#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/debug.h>
#include <sys/errno.h>

#include "custr.h"
#include "util.h"

#ifndef __unused
#define	__unused __attribute__((__unused__))
#endif

typedef enum {
	CUSTR_FIXEDBUF	= 0x01,
	CUSTR_EXCISE	= 0x02,
} custr_flags_t;

struct custr {
	size_t cus_strlen;
	size_t cus_datalen;
	char *cus_data;
	custr_memops_t cus_memops;
	custr_flags_t cus_flags;
};

#define	STRING_CHUNK_SIZE	64

static void *cua_def_alloc(size_t);
static void cua_def_free(void *, size_t);

static custr_memops_t custr_default_memops = {
	.cmo_alloc = cua_def_alloc,
	.cmo_free = cua_def_free
};

static void *
cua_def_alloc(size_t len)
{
	return (malloc(len));
}

static void
cua_def_free(void *p, size_t len __unused)
{
	free(p);
}

static void *
custr_int_alloc(const custr_memops_t *ops, size_t len)
{
	return (ops->cmo_alloc(len));
}

static void
custr_int_free(const custr_memops_t *ops, void *p, size_t len)
{
	ops->cmo_free(p, len);
}

void
custr_reset(custr_t *cus)
{
	if (cus->cus_data == NULL)
		return;

	cus->cus_strlen = 0;
	cus->cus_data[0] = '\0';
}

size_t
custr_len(custr_t *cus)
{
	return (cus->cus_strlen);
}

const char *
custr_cstr(custr_t *cus)
{
	if (cus->cus_data == NULL) {
		VERIFY3U(cus->cus_strlen, ==, 0);
		VERIFY3U(cus->cus_datalen, ==, 0);

		/*
		 * This function should never return NULL.  If no buffer has
		 * been allocated, return a pointer to a zero-length string.
		 */
		return ("");
	}
	return (cus->cus_data);
}

/*
 * Ensure cus can hold an additional len bytes (excluding the terminating NUL).
 * Will expand the internal buffer if possible.
 */
static int
custr_reserve(custr_t *cus, size_t len)
{
	char *new_data = NULL;
	size_t new_len = 0;
	size_t chunksz = STRING_CHUNK_SIZE;

	if (len == SIZE_MAX ||
	    uadd_overflow(cus->cus_strlen, len + 1, &new_len)) {
		errno = EOVERFLOW;
		return (-1);
	}

	if (new_len < cus->cus_datalen) {
		return (0);
	}

	if (cus->cus_flags & CUSTR_FIXEDBUF) {
		errno = EOVERFLOW;
		return (-1);
	}

	while (chunksz < len) {
		if (umul_overflow(chunksz, 2, &chunksz)) {
			errno = EOVERFLOW;
			return (-1);
		}
	}

	if (uadd_overflow(cus->cus_datalen, chunksz, &new_len)) {
		errno = EOVERFLOW;
		return (-1);
	}

	/*
	 * Allocate replacement memory:
	 */
	if ((new_data = custr_int_alloc(&cus->cus_memops, new_len)) == NULL) {
		return (-1);
	}

	/*
	 * Copy existing data into replacement memory and free
	 * the old memory.
	 */
	if (cus->cus_data != NULL) {
		(void) memcpy(new_data, cus->cus_data, cus->cus_strlen + 1);

		if (cus->cus_flags & CUSTR_EXCISE)
			explicit_bzero(cus->cus_data, cus->cus_datalen);
		custr_int_free(&cus->cus_memops, cus->cus_data,
		    cus->cus_datalen);
	}

	/*
	 * Swap in the replacement buffer:
	 */
	cus->cus_data = new_data;
	cus->cus_datalen = new_len;

	return (0);
}

int
custr_append_vprintf(custr_t *cus, const char *fmt, va_list ap)
{
	int len = vsnprintf(NULL, 0, fmt, ap);

	if (len == -1)
		return (len);

	if (custr_reserve(cus, len) == -1) {
		return (-1);
	}

	/*
	 * Append new string to existing string:
	 */
	len = vsnprintf(cus->cus_data + cus->cus_strlen,
	    (uintptr_t)cus->cus_data - (uintptr_t)cus->cus_strlen, fmt, ap);
	if (len == -1)
		return (len);
	cus->cus_strlen += len;

	return (0);
}

int
custr_appendc(custr_t *cus, char newc)
{
	if (custr_reserve(cus, 1) == -1)
		return (-1);

	cus->cus_data[cus->cus_strlen++] = newc;
	cus->cus_data[cus->cus_strlen] = '\0';
	return (0);
}

int
custr_append_printf(custr_t *cus, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = custr_append_vprintf(cus, fmt, ap);
	va_end(ap);

	return (ret);
}

int
custr_append(custr_t *cus, const char *name)
{
	size_t len = strlen(name);

	if (custr_reserve(cus, len) == -1)
		return (-1);

	(void) strlcat(cus->cus_data, name, cus->cus_datalen);
	cus->cus_strlen += len;
	return (0);
}

int
custr_insert_vprintf(custr_t *cus, size_t pos, const char *fmt, va_list ap)
{
	char *p;
	int len = vsnprintf(NULL, 0, fmt, ap);
	char save;

	if (len == -1)
		return (len);

	if (pos > cus->cus_strlen) {
		errno = EINVAL;
		return (-1);
	}

	if (custr_reserve(cus, len) == -1) {
		return (-1);
	}

	p = cus->cus_data + pos;
	(void) memmove(p + len, p, len);

	/*
	 * When vsnprintf() writes out it's string, it will clobber
	 * p[len] with a 'terminating' NUL.  Save p[len], then restore
	 * after we've inserted our text.
	 */
	save = p[len];
	VERIFY3S(vsnprintf(p, len + 1, fmt, ap), ==, len);
	p[len] = save;

	cus->cus_strlen += len;
	cus->cus_data[cus->cus_strlen] = '\0';

	return (0);
}

int
custr_insert_printf(custr_t *cus, size_t pos, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = custr_insert_vprintf(cus, pos, fmt, ap);
	va_end(ap);

	return (ret);
}

int
custr_insert(custr_t *cus, size_t pos, const char *str)
{
	return (custr_insert_printf(cus, pos, "%s", str));
}

int
custr_insertc(custr_t *cus, size_t pos, char c)
{
	char *p;

	if (pos > cus->cus_strlen) {
		errno = EINVAL;
		return (-1);
	}

	if (custr_reserve(cus, 1) == -1)
		return (-1);

	p = cus->cus_data + pos;
	(void) memmove(p + 1, p, 1);

	cus->cus_data[pos] = c;
	cus->cus_data[++cus->cus_strlen] = '\0';

	return (0);
}

int
custr_delete(custr_t *cus, ssize_t pos, size_t len)
{
	char *p;

	if (pos < 0)
		pos += cus->cus_strlen;

	if (pos > cus->cus_strlen || pos + len > cus->cus_strlen) {
		errno = EINVAL;
		return (-1);
	}

	/*
	 * If we're clearing to the end of the string, we don't need
	 * to move any data around, just terminate at the new position,
	 * update the length, and return.
	 */
	if (len == 0 || pos + len == cus->cus_strlen) {
		cus->cus_data[pos] = '\0';
		cus->cus_strlen = pos;
		return (0);
	}

	p = cus->cus_data + pos;
	(void) memmove(p, p + len, len);

	cus->cus_strlen -= len;
	cus->cus_data[cus->cus_strlen] = '\0';

	return (0);
}

int
custr_alloc(custr_t **cus, const custr_memops_t *memops)
{
	custr_t *t;

	if (memops == NULL)
		memops = &custr_default_memops;

	if ((t = custr_int_alloc(memops, sizeof (*t))) == NULL) {
		*cus = NULL;
		return (-1);
	}
	(void) memset(t, 0, sizeof (*t));

	*cus = t;
	(*cus)->cus_memops = *memops;
	return (0);
}

int
custr_alloc_buf(custr_t **cus, void *buf, size_t buflen)
{
	int ret;

	if (buflen == 0 || buf == NULL) {
		errno = EINVAL;
		return (-1);
	}

	if ((ret = custr_alloc(cus, NULL)) != 0)
		return (ret);

	(*cus)->cus_data = buf;
	(*cus)->cus_datalen = buflen;
	(*cus)->cus_strlen = 0;
	(*cus)->cus_flags = CUSTR_FIXEDBUF;
	(*cus)->cus_data[0] = '\0';

	return (0);
}

void
custr_free(custr_t *cus)
{
	custr_memops_t ops = cus->cus_memops;

	if (cus == NULL)
		return;

	if ((cus->cus_flags & CUSTR_FIXEDBUF) == 0) {
		if ((cus->cus_flags & CUSTR_EXCISE))
			explicit_bzero(cus->cus_data, cus->cus_datalen);
		custr_int_free(&ops, cus->cus_data, cus->cus_datalen);
	}
	custr_int_free(&ops, cus, sizeof (*cus));
}
