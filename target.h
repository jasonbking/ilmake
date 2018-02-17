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

#ifndef _TARGET_H
#define	_TARGET_H

#include <sys/types.h>
#include "input.h"

#ifdef __cplusplus
extern "C" {
#endif

struct target;
struct dependency;
struct cmd;

typedef struct dependency {
	struct target *target;
	bookmark_t src;
} dependency_t;

typedef struct target {
	bookmark_t src;
	char *name;
	struct dependency **deps;
	struct cmd **cmds;
	boolean_t phony;
} target_t;

typedef struct command {
	char *cmd;
	bookmark_t src;
} cmd_t;

#ifdef __cplusplus
}
#endif

#endif /* _TARGET_H */
