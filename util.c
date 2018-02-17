#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <err.h>
#include <errno.h>
#include <sys/debug.h>
#include <umem.h>

#include "util.h"
#include "custr.h"

#ifndef MIN
#define	MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

static boolean_t
umul_overflow(size_t a, size_t b, size_t *cp)
{
	*cp = a * b;
	return ((*cp < a || *cp < b) ? B_TRUE : B_FALSE);
}

void *
zalloc(size_t len)
{
	return (umem_zalloc(len, UMEM_NOFAIL));
}

char *
xprintf(const char *fmt, ...)
{
	char *s = NULL;
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	VERIFY3S(n, >=, 0);

	s = zalloc(n + 1);

	va_start(ap, fmt);
	VERIFY3S(vsnprintf(s, n + 1, fmt, ap), ==, n);
	va_end(ap);

	return (s);
}

void
strfree(char *s)
{
	if (s == NULL)
		return;

	umem_free(s, strlen(s) + 1);
}

char *
xstrdup(const char *s)
{
	char *news = NULL;
	size_t len = strlen(s);

	news = zalloc(len + 1);
	(void) strlcpy(news, s, len + 1);

	return (news);
}

void *
xrealloc(void *old, size_t oldlen, size_t newlen)
{
	void *newmem = zalloc(newlen);

	(void) memcpy(newmem, old, MIN(oldlen, newlen));
	umem_free(old, oldlen);

	return (newmem);
}

void *
xcalloc(size_t nelem, size_t elsize)
{
	size_t len = 0;

	if (umul_overflow(nelem, elsize, &len))
		assfail("Overflow", __FILE__, __LINE__);

	return (zalloc(len));
}

void
cfree(void *p, size_t nelem, size_t elsize)
{
	size_t len = nelem * elsize;

	if (p == NULL)
		return;

	umem_free(p, len);
}

static const custr_memops_t i_memops = {
	.cmo_alloc = zalloc,
	.cmo_free = umem_free
};

const custr_memops_t *cu_memops = &i_memops;
