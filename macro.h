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

#ifndef _MACRO_H
#define	_MACRO_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cond_macro {
	struct cond_macro *next;
	char *target;
	char *val;
	bookmark_t where;
	boolean_t assign;
} cond_macro_t;

typedef struct macro {
	char *name;
	char *val;
	cond_macro_t *cond;
	bookmark_t where;	
} macro_t;

void macro_assign(const char *, const char *, bookmark_t);
void macro_append(const char *, const char *, bookmark_t);
void macro_cond_assign(const char *, const char *, const char *, bookmark_t);
void macro_cond_append(const char *, const char *, const char *, bookmark_t);
char *macro_value(const char *);
void macro_iter(boolean_t (*)(macro_t *, void *), void *);


#ifdef __cplusplus
}
#endif

#endif /* _MACRO_H */
