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

#include <err.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/avl.h>
#include <sys/debug.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <umem.h>
#include <unistd.h>

#include "input.h"
#include "custr.h"
#include "util.h"

#define	INPUT_BLOCK_SIZE	2048U
#define	LINEPTR_CHUNK		128U
#define	ITER_DEFAULT_DEPTH	8U

input_t *
input_alloc(void)
{
	return (zalloc(sizeof (input_t)));
}

void
input_free(input_t *in)
{
	if (in == NULL)
		return;
	input_release(in);
	umem_free(in, sizeof (*in));
}

input_t *
input_init(input_t *in, FILE *f, const char *name)
{
	custr_t *line = NULL;
	size_t size = 0;
	size_t len = INPUT_BLOCK_SIZE;
	size_t pos = 0;
	int fd = fileno(f);
	int rc;
	struct stat sb = { 0 };

	if (in == NULL)
		return (NULL);

	(void) memset(in, 0, sizeof (*in));

	in->in_filename = xstrdup(name);

	VERIFY0(custr_alloc(&line, cu_memops));

	/*
	 * As a hint, start with a rounded up size of the file for the
	 * input buffer.
	 */
	if (fstat(fd, &sb) == -1)
		err(EXIT_FAILURE, "%s", name);

	if (S_ISREG(sb.st_mode))
		size = sb.st_size;

	while (len < size)
		len += INPUT_BLOCK_SIZE;

	if (in->in_bufalloc < len) {
		in->in_buf = xrealloc(in->in_buf, in->in_bufalloc, len);
		in->in_bufalloc = len;
	}

	for (;;) {
		custr_reset(line);
		VERIFY0(custr_append_line(line, f));

		if (feof(f))
			break;

		if (in->in_numlines + 1 >= in->in_linealloc) {
			size_t oldlen = in->in_linealloc * sizeof (size_t);
			size_t newamt = LINEPTR_CHUNK * sizeof (size_t);

			in->in_lines = xrealloc(in->in_lines, oldlen, oldlen +
			    newamt);
			in->in_linealloc += LINEPTR_CHUNK;
		}

		len = custr_len(line);
		if (pos + len + 1 > in->in_bufalloc) {
			size_t newamt = in->in_bufalloc + INPUT_BLOCK_SIZE;

			in->in_buf = xrealloc(in->in_buf, in->in_bufalloc,
			    newamt);
			in->in_bufalloc = newamt;
		}

		in->in_lines[in->in_numlines++] = pos;
		(void) memcpy(in->in_buf + pos, custr_cstr(line), len);
		pos += len + 1;
	}

	in->in_buflen = pos;
	custr_free(line);

	return (in);
}

void
input_release(input_t *in)
{
	if (in == NULL)
		return;

	strfree(in->in_filename);
	in->in_filename = NULL;

	umem_free(in->in_buf, in->in_bufalloc);
	in->in_buf = NULL;
	in->in_bufalloc = in->in_buflen = 0;

	umem_free(in->in_lines, in->in_linealloc);
	in->in_lines = NULL;
	in->in_numlines = in->in_linealloc = 0;
}

input_iter_t *
input_iter(input_t *in, iter_cb_t cb, void *arg)
{
	input_iter_t *iter = zalloc(sizeof (*iter));

	iter->ii_evtcb = cb;
	iter->ii_arg = arg;
	iter_push(iter, in);
	return (iter);
}

boolean_t
iter_line(input_iter_t *iter, custr_t *str)
{
	iter_item_t *item;
	input_t *in;

	while (iter->ii_n > 0) {
		item = &iter->ii_items[iter->ii_n - 1];
		in = item->item_in;

		if (item->item_line < in->in_numlines) {
			char *p = in->in_buf + in->in_lines[item->item_line++];

			custr_append(str, p);
			return (B_TRUE);
		}

		iter_pop(iter);
	}

	iter_pop(iter);
	return (B_FALSE);
}

void
iter_push(input_iter_t *iter, input_t *in)
{
	iter_item_t *item = NULL;

	if (iter->ii_n + 1 >= iter->ii_alloc) {
		size_t oldlen = iter->ii_alloc * sizeof (iter_item_t);
		size_t newlen = oldlen +
		    (ITER_DEFAULT_DEPTH * sizeof (iter_item_t));

		iter->ii_items = xrealloc(iter->ii_items, oldlen, newlen);
		iter->ii_alloc += ITER_DEFAULT_DEPTH;
	}

	item = &iter->ii_items[iter->ii_n++];
	item->item_in = in;
	item->item_line = 0;

	if (iter->ii_evtcb != NULL)
		iter->ii_evtcb(IEVT_PUSH, in, iter->ii_arg);
}

void
iter_pop(input_iter_t *iter)
{
	input_t *in = NULL;

	if (iter->ii_n > 0)
		in = iter->ii_items[--iter->ii_n].item_in;

	if (iter->ii_evtcb != NULL)
		iter->ii_evtcb(IEVT_POP, in, iter->ii_arg);
}

