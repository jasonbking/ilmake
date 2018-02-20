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

#ifndef _VAR_H
#define	_VAR_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct custr;
struct make;

typedef enum var_assign {
	VAS_RECURSIVE,		/* var = value			*/
	VAS_SIMPLE,		/* var := value (GNU/BSD)	*/
	VAS_CONDITIONAL,	/* target := var = value (SysV) */
} var_assign_t;

void		var_set(struct make *, const char *, const char *);
boolean_t	var_get(struct make *, const char *, struct custr *);

#ifdef __cplusplus
}
#endif

#endif /* _VAR_H */
