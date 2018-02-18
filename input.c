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
#include <sys/queue.h>
#include <umem.h>
#include <unistd.h>

#include "input.h"
#include "custr.h"
#include "util.h"

/*
 * An input file.  in_buf points to the contents of the file, and in_bufend
 * points to just after the end of the contents.  To iterate through
 * the contents, one could do:
 *	for (ptr = in->in_buf; ptr < in->in_bufend; ptr++) { ... }
 *
 * In addition, we keep pointers to the start of each line (in_line[linenum])
 * to facilitate either iteration by line, or for determining the position
 * of an offset into the file in terms of lines and columns.
 */

LIST_HEAD(input_list, input);
struct input {
	LIST_ENTRY(input) in_link;
	char		*in_filename;
	const char	*in_buf;
	const char	*in_bufend;	/* address just past end of buf */
	size_t		in_bufalloc;	/* Amount allocated */
	const char	**in_line;
	size_t		in_numlines;
};

typedef struct iter_item {
	input_t		*item_in;
	size_t		item_line;
} iter_item_t;

struct input_iter {
	iter_item_t	*ii_items;
	size_t		ii_n;
	size_t		ii_alloc;
	iter_cb_t	ii_evtcb;
	void		*ii_arg;
	custr_t		*ii_line;
};

struct line_index {
	size_t	*li_offsets;
	size_t	li_alloc;
};

#define	INPUT_BLOCK_SIZE	2048U
#define	LINEPTR_CHUNK		128U
#define	ITER_DEFAULT_DEPTH	8U

static struct input_list inputs = LIST_HEAD_INITIALIZER(input);
static boolean_t input_read(input_t *, FILE *);

input_t *
input_new(const char *filename)
{
	FILE *f = NULL;
	input_t *in = zalloc(sizeof (input_t));

	if ((f = fopen(filename, "rF")) == NULL) {
		warn(_("Unable to open %s"), filename);
		goto fail;
	}

	in->in_filename = xstrdup(filename);
	if (!input_read(in, f))
		goto fail;

	(void) fclose(f);

	LIST_INSERT_HEAD(&inputs, in, in_link);
	return (in);

fail:
	if (f != NULL)
		(void) fclose(f);

	input_free(in);
	return (NULL);
}

input_t *
input_fnew(const char *filename, FILE *f)
{
	input_t *in = zalloc(sizeof (input_t));

	in->in_filename = xstrdup(filename);
	if (!input_read(in, f)) {
		input_free(in);
		return (NULL);
	}

	LIST_INSERT_HEAD(&inputs, in, in_link);
	return (in);
}

void
input_free(input_t *in)
{
	if (in == NULL)
		return;

	strfree(in->in_filename);
	umem_free((void *)in->in_buf, in->in_bufalloc);
	umem_free((void *)in->in_line, (in->in_numlines + 1) * sizeof (char *));
	umem_free(in, sizeof (*in));
}

/*
 * If f is a regular file, use the rounded up file size as a hint for
 * the initial size of the buffer, otherwise use INPUT_BLOCK_SIZE.
 */
static char *
guess_size(const char *name, FILE *f, size_t *lenp)
{
	size_t len = INPUT_BLOCK_SIZE;
	int fd = fileno(f);
	struct stat sb = { 0 };

	if (fstat(fd, &sb) == -1) {
		warn("%s", name);
		return (NULL);
	}

	if (!S_ISREG(sb.st_mode))
		goto done;

	/* Round up to multiple of INPUT_BLOCK_SIZE */
	len = (sb.st_size + INPUT_BLOCK_SIZE - 1) / INPUT_BLOCK_SIZE;
	len *= INPUT_BLOCK_SIZE;

done:
	*lenp = len;
	return (zalloc(len));
}

static void
index_input(input_t *in)
{
	const char *p = NULL;
	size_t lines = 0;

	if (in->in_buf == in->in_bufend)
		return;

	p = in->in_buf;
	while (p != NULL && p < in->in_bufend) {
		lines++;
		if ((p = strchr(p, '\n')) != NULL)
			p++;
	}

	in->in_line = xcalloc(lines + 1, sizeof (char *));
	in->in_numlines = lines;

	p = in->in_buf;
	lines = 0;
	while (p != NULL && p < in->in_bufend) {
		in->in_line[lines++] = p;
		if ((p = strchr(p, '\n')) != NULL)
			p++;
	}

	in->in_line[lines] = in->in_bufend;
}

static boolean_t
input_read(input_t *in, FILE *f)
{
	char *buf = NULL;
	size_t buflen = 0;
	size_t total = 0, amt = 0;

	if ((buf = guess_size(in->in_filename, f, &buflen)) == NULL)
		return (B_FALSE);

	while (!feof(f) && !ferror(f)) {
		if (total + 1 > buflen) {
			amt = buflen + INPUT_BLOCK_SIZE;
			buf = xrealloc(buf, buflen, amt);
			buflen = amt;
		}
		ASSERT3U(buflen, >, total);

		amt = buflen - total;
		total += fread(buf + total, 1, amt, f);
	}

	if (ferror(f)) {
		warn("%s", in->in_filename);
		umem_free(buf, buflen);
		return (B_FALSE);
	}

	in->in_buf = buf;
	in->in_bufalloc = buflen;
	in->in_bufend = in->in_buf + total;
	index_input(in);
}

size_t
input_numlines(const input_t *in)
{
	return (in->in_numlines);
}

const char *
input_line(const input_t *in, size_t linenum)
{
	if (linenum < in->in_numlines)
		return (in->in_line[linenum]);
	return (NULL);
}

const char *
input_name(const input_t *in)
{
	return (in->in_filename);
}

boolean_t
input_pos(const input_t *in, const char *p, size_t *lp, size_t *cp)
{
	size_t i, line = 0, col = 0;

	if (p == NULL || p < in->in_buf || p >= in->in_bufend)
		return (B_FALSE);

	for (i = 0; i < in->in_numlines; i++) {
		if (in->in_line[i + 1] < p)
			continue;

		line = i;
		break;
	}

	col = (size_t)(p - in->in_line[line]);

	if (lp != NULL)
		*lp = line;
	if (cp != NULL)
		*cp = col;

	return (B_TRUE);
}

input_iter_t *
iter_new(input_t *in, iter_cb_t cb, void *arg)
{
	input_iter_t *iter = zalloc(sizeof (*iter));

	iter->ii_evtcb = cb;
	iter->ii_arg = arg;

	VERIFY0(custr_alloc(&iter->ii_line, cu_memops));

	if (iter->ii_evtcb != NULL)
		iter->ii_evtcb(iter, IEVT_START, iter->ii_arg);

	iter_push(iter, in);
	return (iter);
}

void
iter_free(input_iter_t *iter)
{
	if (iter == NULL)
		return;

	umem_free(iter->ii_items, iter->ii_alloc * sizeof (iter_item_t));
	umem_free(iter, sizeof (*iter));
}

const char *
iter_line(input_iter_t *iter)
{
	iter_item_t *item = NULL;
	input_t *in = NULL;
	const char *p = NULL;

	custr_reset(iter->ii_line);

	while (iter->ii_n > 0) {
		item = &iter->ii_items[iter->ii_n - 1];
		in = item->item_in;

		/* End of file, pop and try again */
		if ((p = input_line(in, item->item_line++)) == NULL) {
			iter_pop(iter);
			continue;
		}

		/* Copy line into iter->ii_line and return */
		const char *end = input_line(in, item->item_line);
		if (end == NULL)
			end = in->in_bufend;

		while (p < end) {
			VERIFY0(custr_appendc(iter->ii_line, *p));
			p++;
		}

		return (custr_cstr(iter->ii_line));
	}

	ASSERT3U(iter->ii_n, ==, 0);

	if (iter->ii_evtcb != NULL)
		iter->ii_evtcb(iter, IEVT_END, iter->ii_arg);

	return (NULL);
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
		iter->ii_evtcb(iter, IEVT_PUSH, iter->ii_arg);
}

void
iter_pop(input_iter_t *iter)
{
	input_t *in = NULL;

	if (iter->ii_evtcb != NULL)
		iter->ii_evtcb(iter, IEVT_POP, iter->ii_arg);

	if (iter->ii_n > 0)
		in = iter->ii_items[--iter->ii_n].item_in;
}

input_t *
iter_input(const input_iter_t *iter)
{
	if (iter->ii_n == 0)
		return (NULL);
	return (iter->ii_items[iter->ii_n - 1].item_in);
}

size_t
iter_lineno(const input_iter_t *iter)
{
	if (iter->ii_n == 0)
		return (0);
	return (iter->ii_items[iter->ii_n - 1].item_line);
}

size_t
iter_depth(const input_iter_t *iter)
{
	return (iter->ii_n);
}
