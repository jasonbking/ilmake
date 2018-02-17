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
 * Copyright 2018 Jason King
 */

#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <sys/debug.h>

#include "custr.h"
#include "input.h"
#include "make.h"
#include "parse.h"
#include "util.h"
#include "var.h"

static void
pdbg(make_t *mk, const char *fmt, ...)
{
	va_list ap;

	if ((mk->mk_debug_flags & MDF_PARSE) == 0)
		return;

	va_start(ap, fmt);
	(void) vfprintf(mk->mk_debug, fmt, ap);
	va_end(ap);
}

static void
iter_cb(input_iter_t *iter, iter_event_t evt, void *arg)
{
	make_t *mk = arg;
	input_t *in = iter_input(iter);
	char *hstr = NULL;
	size_t depth = iter_depth(iter);

	/*
	 * Construct a string of '#'s as a prefix onto the debug message.
	 * The number of #s represents the include depth + 1 (as we always
	 * start with one # as a prefix).
	 */
	hstr = zalloc(depth + 2);
	for (size_t i = 0; i < depth; i++)
		hstr[i] = '#';
	hstr[depth] = '#';

	switch (evt) {
	case IEVT_START:
		pdbg(mk, _("%s Starting input processing\n"), hstr);
		break;
	case IEVT_PUSH:
		pdbg(mk, _("%s >>>>> Reading %s\n"), hstr, input_name(in));
		break;
	case IEVT_POP:
		pdbg(mk, _("%s <<<<< Finished with %s\n"), hstr,
		    input_name(in));
		break;
	case IEVT_END:
		pdbg(mk, _("%s End of input\n"), hstr);
		break;
	}

	strfree(hstr);
}

/*
 * Gets a 'logical' line consisting of one or more physical lines from
 * the input.  For example:
 *	line1 \
 *	line2
 * Would return as one logical line "line 1 \\\nline2"
 */

static boolean_t
get_logical_line(make_t *mk, input_iter_t *iter, custr_t *line)
{
	const char *s = NULL;
	size_t slen = 0;
	boolean_t litnext = B_FALSE;
	char c;

	custr_reset(line);

again:
	s = iter_line(iter);
	if (s == NULL)
		return (B_FALSE);

	slen = strlen(s);

	for (; *s != '\0'; s++) {
		c = *s;

		if (litnext) {
			VERIFY0(custr_appendc(line, c));
			litnext = B_FALSE;

			if (c == '\n')
				goto again;
			continue;
		}

		switch (c) {
		case '\n':
			goto done;
		case '\\':
			litnext = B_TRUE;
			break;
		}

		VERIFY0(custr_appendc(line, c));
	}

done:
	if (litnext) {
		/* XXX: Make this nicer */
		(void) fprintf(stderr,
		    "File cannot end with continuation character\n");
		return (B_FALSE);
	}

	return (B_TRUE);
}

boolean_t
parse_input(make_t *mk, input_t *in_start)
{
	input_t *in = NULL;
	input_iter_t *iter = NULL;
	custr_t *line = NULL;
	const char *s = NULL;
	size_t len = 0;
	size_t linenum = 0;
	boolean_t recipe = B_FALSE;

	VERIFY0(custr_alloc(&line, cu_memops));

	iter = iter_new(in_start, iter_cb, mk);
	while (get_logical_line(mk, iter, line)) {
		s = custr_cstr(line);
		len = custr_len(line);

		pdbg(mk, "'%s'\n", s);
	}

	return (B_TRUE);
}
