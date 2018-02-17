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

struct custr;

typedef struct input {
	char		*in_filename;
	char		*in_buf;
	size_t		in_buflen;	/* Size of valid content */
	size_t		in_bufalloc;	/* Amount allocated */
	size_t		*in_lines;
	size_t		in_numlines;
	size_t		in_linealloc;
} input_t;

typedef enum iter_event {
	IEVT_PUSH,
	IEVT_POP,
} iter_event_t;

typedef struct iter_item {
	struct input	*item_in;
	size_t		item_line;
} iter_item_t;

typedef void (*iter_cb_t)(iter_event_t, input_t *, void *);
typedef struct input_iter {
	iter_item_t	*ii_items;
	size_t		ii_n;
	size_t		ii_alloc;
	iter_cb_t	ii_evtcb;
	void		*ii_arg;
} input_iter_t;

input_t *input_alloc(void);
input_t *input_init(input_t *, FILE *, const char *);
void input_release(input_t *);
void input_free(input_t *);

input_iter_t	*input_iter(input_t *, iter_cb_t, void *);
boolean_t	iter_line(input_iter_t *, struct custr *);
void		iter_push(input_iter_t *, input_t *);
void		iter_pop(input_iter_t *);

#ifdef __cplusplus
}
#endif

#endif /* _INPUT_H */
