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

/*
 * An input file, indexed by line.  The entire file is stored as a series of
 * NUL-terminated strings inside a single contiguous buffer (in_buf).  We
 * then store pointers to the start of each string in in_line[linenum].
 *
 * The entire contents of the file is retained for the duration of parsing,
 * this hopefully allows for the display of more context for any parsing
 * errors.
 */
struct input {
	char		*in_filename;
	char		*in_buf;
	size_t		in_buflen;	/* Size of valid content */
	size_t		in_bufalloc;	/* Amount allocated */
	char		**in_line;
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
};

struct line_index {
	size_t	*li_offsets;
	size_t	li_alloc;
};

#define	INPUT_BLOCK_SIZE	2048U
#define	LINEPTR_CHUNK		128U
#define	ITER_DEFAULT_DEPTH	8U

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

	return (in);
}

void
input_free(input_t *in)
{
	if (in == NULL)
		return;

	strfree(in->in_filename);
	umem_free(in->in_buf, in->in_bufalloc);
	umem_free(in->in_line, in->in_numlines * sizeof (char *));
	umem_free(in, sizeof (*in));
}

/*
 * If f is a regular file, use the rounded up file size as a hint for
 * the initial size of the buffer, otherwise use INPUT_BLOCK_SIZE.
 */
static boolean_t
guess_size(input_t *in, FILE *f)
{
	size_t len = INPUT_BLOCK_SIZE;
	int fd = fileno(f);
	struct stat sb = { 0 };


	if (fstat(fd, &sb) == -1) {
		warn("%s", in->in_filename);
		return (B_FALSE);
	}

	if (!S_ISREG(sb.st_mode))
		goto done;

	/* Round up to multiple of INPUT_BLOCK_SIZE */
	len = (sb.st_size + INPUT_BLOCK_SIZE - 1) / INPUT_BLOCK_SIZE;
	len *= INPUT_BLOCK_SIZE;

done:
	in->in_buf = zalloc(len);
	in->in_bufalloc = len;
	return (B_TRUE);
}

static void
get_line(custr_t *cus, FILE *f)
{
	int c;

	custr_reset(cus);

	do {
		if ((c = fgetc(f)) == EOF)
			break;
		VERIFY0(custr_appendc(cus, c));
	} while (c != '\n');
}

void
check_space(input_t *in, struct line_index *idx, custr_t *line)
{
	size_t linelen = custr_len(line);
	size_t newamt;

	if (in->in_numlines + 1 >= idx->li_alloc) {
		size_t oldamt = idx->li_alloc * sizeof (size_t);

		/* newamt = idx->li_alloc + LINEPTR_CHUNK */
		if (uadd_overflow(idx->li_alloc, LINEPTR_CHUNK, &newamt)) {
			errx(EXIT_FAILURE, _("%s: line number overflow"),
			    in->in_filename);
		}

		/* newamt *= sizeof (size_t) */
		if (umul_overflow(newamt, sizeof (size_t), &newamt)) {
			errx(EXIT_FAILURE, _("%s: file size is too large"),
			    in->in_filename);
		}

		idx->li_offsets = xrealloc(idx->li_offsets, oldamt, newamt);
		idx->li_alloc += LINEPTR_CHUNK;
	}

	/* Make sure there's space for the line plus a terminating NUL */
	if (in->in_buflen + linelen + 1 >= in->in_bufalloc) {
		/* newamt = in->in_bufalloc + INPUT_BLOCK_SIZE */
		if (umul_overflow(in->in_bufalloc, INPUT_BLOCK_SIZE, &newamt)) {
			errx(EXIT_FAILURE, _("%s: file size is too large"),
			    in->in_filename);
		}

		in->in_buf = xrealloc(in->in_buf, in->in_bufalloc, newamt);
		in->in_bufalloc = newamt;
	}
}

static boolean_t
input_read(input_t *in, FILE *f)
{
	custr_t *line = NULL;
	struct line_index idx = { 0 };
	size_t pos = 0, len = 0;
	boolean_t ret = B_TRUE;

	VERIFY0(custr_alloc(&line, cu_memops));
	guess_size(in, f);

	do {
		get_line(line, f);
		len = custr_len(line);
		if (len == 0)
			break;

		check_space(in, &idx, line);

		idx.li_offsets[in->in_numlines++] = pos;
		(void) memcpy(in->in_buf + pos, custr_cstr(line), len);
		pos += len + 1;
	} while (!feof(f) && !ferror(f));

	if (ferror(f)) {
		ret = B_FALSE;
		warn("%s", in->in_filename);
		goto done;
	}

	in->in_buflen = pos;
	custr_free(line);

	/*
	 * Now that in_buf is populated, populate line pointers from the
	 * saved offsets.
	 */
	in->in_line = xcalloc(in->in_numlines, sizeof (char *));
	for (size_t i = 0; i < in->in_numlines; i++)
		in->in_line[i] = in->in_buf + idx.li_offsets[i];

done:
	umem_free(idx.li_offsets, idx.li_alloc * sizeof (size_t));
	return (ret);
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

input_iter_t *
iter_new(input_t *in, iter_cb_t cb, void *arg)
{
	input_iter_t *iter = zalloc(sizeof (*iter));

	iter->ii_evtcb = cb;
	iter->ii_arg = arg;

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

	while (iter->ii_n > 0) {
		item = &iter->ii_items[iter->ii_n - 1];
		in = item->item_in;

		p = input_line(in, item->item_line++);
		if (p != NULL)
			return (p);

		iter_pop(iter);
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
