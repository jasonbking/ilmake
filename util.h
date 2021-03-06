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
 * Copyright 2017 Jason King
 */

#ifndef _UTIL_H
#define	_UTIL_H

#include <libintl.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	_(x) gettext(x)

struct custr;
struct custr_memops;
extern const struct custr_memops *cu_memops;

boolean_t uadd_overflow(size_t, size_t, size_t *);
boolean_t umul_overflow(size_t, size_t, size_t *);

void append_range(const char *, size_t, struct custr *);

void *zalloc(size_t);
void *xcalloc(size_t, size_t);
void *xrealloc(void *, size_t, size_t);
char *xprintf(const char *, ...);
char *xstrdup(const char *);

void strfree(char *);
void cfree(void *, size_t, size_t);

#ifdef __cplusplus
}
#endif

#endif /* _UTIL_H */
