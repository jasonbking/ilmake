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

#include <sys/types.h>
#include <umem.h>

#include "custr.h"
#include "input.h"
#include "token.h"
#include "util.h"

token_t *
tok_new(token_type_t type, input_t *src, size_t line, size_t col, custr_t *str)
{
	token_t *t = zalloc(sizeof (*t));

	t->tok_type = type;
	t->tok_src = src;
	t->tok_line = line;
	t->tok_col = col;
	t->tok_val = xstrdup(custr_cstr(str));

	return (t);
}

void
tok_free(token_t *t)
{
	if (t == NULL)
		return;

	strfree(t->tok_val);
	umem_free(t, sizeof (*t));
}
