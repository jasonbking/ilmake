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

#ifndef _MAKE_H
#define	_MAKE_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum make_style {
	MS_ILLUMOS,
	MS_SYSV,
	MS_POSIX,
	MS_BSD,
	MS_GNU
} make_style_t;

typedef enum make_debug_flags {
	MDF_NONE	= 0,
	MDF_PARSE	= (1U << 1),
} make_debug_flags_t;

typedef struct make {
	make_style_t		mk_style;
	FILE			*mk_debug;
	make_debug_flags_t	mk_debug_flags;
} make_t;

#ifdef __cplusplus
}
#endif

#endif /* _MAKE_H */
