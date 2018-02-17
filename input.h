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

#ifndef _INPUT_H
#define	_INPUT_H

#include <stdio.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct input;
struct input_iter;
typedef struct input input_t;
typedef struct input_iter input_iter_t;

input_t		*input_new(const char *);
input_t		*input_fnew(const char *, FILE *);
void		input_free(input_t *);
size_t		input_numlines(const input_t *);
const char	*input_line(const input_t *, size_t);
const char	*input_name(const input_t *);

/*
 * Iteration of input_t lines, with optional stacking of inputs (for handling
 * included files.  Since these are most likely short lived, unlike
 * input_t's, these can be created on the stack.
 */

/* The types of events that can happen during iteration */
typedef enum iter_event {
	IEVT_START,		/* Start of iteration */
	IEVT_PUSH,		/* New file is pushed */
	IEVT_POP,		/* File is popped, or finished */
	IEVT_END,		/* Done with iteration */
} iter_event_t;
typedef void (*iter_cb_t)(input_iter_t *, iter_event_t, void *);

input_iter_t	*iter_new(input_t *, iter_cb_t, void *);
void		iter_free(input_iter_t *);
const char	*iter_line(input_iter_t *);
void		iter_push(input_iter_t *, input_t *);
void		iter_pop(input_iter_t *);
input_t		*iter_input(const input_iter_t *);
size_t		iter_lineno(const input_iter_t *);
size_t		iter_depth(const input_iter_t *);

#ifdef __cplusplus
}
#endif

#endif /* _INPUT_H */
