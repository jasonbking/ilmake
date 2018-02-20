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
#include <stdio.h>
#include <string.h>
#include <sys/debug.h>
#include <sys/types.h>
#include <umem.h>

#include "custr.h"
#include "input.h"
#include "make.h"
#include "token.h"
#include "util.h"

#define	ARRAY_SIZE(x) (sizeof (x) / sizeof (x[0]))

enum tok_flags {
	TF_NONE		= 0,
	TF_ILLUMOS	= (1U << 0),
	TF_SYSV		= (1U << 1),
	TF_POSIX	= (1U << 2),
	TF_BSD		= (1U << 3),
	TF_GNU		= (1U << 4),
	TF_ALL		= (TF_SYSV|TF_POSIX|TF_BSD|TF_GNU),
};

static struct tok_word {
	const char	*tw_str;
	token_type_t	tw_type;
	enum tok_flags	tw_flags;
} twtbl[] = {
	{ ".for",	TOK_FOR,		TF_BSD },
	{ ".endfor",	TOK_ENDFOR,		TF_BSD },
	{ ".error",	TOK_ERROR,		TF_BSD },
	{ ".export",	TOK_EXPORT,		TF_BSD },
	{ ".export-env", TOK_EXPORT_ENV,	TF_BSD },
	{ ".export-literal", TOK_EXPORT_LIT,	TF_BSD },
	{ ".info",	TOK_INFO,		TF_BSD },
	{ ".undef",	TOK_UNDEF,		TF_BSD },
	{ ".unexport",	TOK_UNEXPORT,		TF_BSD },
	{ ".unexport-env", TOK_UNEXPORT_ENV,	TF_BSD },
	{ ".warning",	TOK_WARNING,		TF_BSD },
	{ ".if",	TOK_IF,			TF_BSD },
	{ ".ifdef",	TOK_IFDEF,		TF_BSD },
	{ ".ifndef",	TOK_IFNDEF,		TF_BSD },
	{ ".ifmake",	TOK_IFMAKE,		TF_BSD },
	{ ".ifnmake",	TOK_IFNMAKE,		TF_BSD },
	{ ".else",	TOK_ELSE,		TF_BSD },
	{ ".elif",	TOK_ELIF,		TF_BSD },
	{ ".elifdef",	TOK_ELIFDEF,		TF_BSD },
	{ ".elifndef",	TOK_ELIFNDEF,		TF_BSD },
	{ ".elifnmake",	TOK_ELIFNMAKE,		TF_BSD },
	{ ".endif",	TOK_ENDIF,		TF_BSD },
	{ ".include",	TOK_INCLUDE,		TF_BSD },
	{ ".-include",	TOK_INCLUDE,		TF_BSD },
	{ "include",	TOK_INCLUDE,		TF_SYSV|TF_GNU },
	{ "-include",	TOK_INCLUDE,		TF_GNU },
	{ "define",	TOK_DEFINE,		TF_GNU },
	{ "endef",	TOK_ENDDEF,		TF_GNU },
	{ "ifeq",	TOK_IFEQ,		TF_GNU },
	{ "ifneq",	TOK_IFNEQ,		TF_GNU },
	{ "else",	TOK_ELSE,		TF_GNU },
	{ "endif",	TOK_ENDIF,		TF_GNU },
};

static const char sep_nl[] = "\n";
static const char tgt_sep_sysv[] = " \t\n+=:";

#define	TOKEN_CHUNK	1024U
typedef struct tok_array {
	token_t	*ta_tokens;
	size_t	ta_n;
	size_t	ta_alloc;
} tok_array_t;

static void
tok_reserve(tok_array_t *ta, size_t n)
{
	if (ta->ta_n + n < ta->ta_alloc)
		return;

	size_t oldlen = ta->ta_alloc * sizeof (token_t);
	size_t newlen = (ta->ta_alloc + TOKEN_CHUNK) * sizeof (token_t);

	ta->ta_tokens = xrealloc(ta->ta_tokens, oldlen, newlen);
	ta->ta_alloc += TOKEN_CHUNK;
}

static token_t *
tok_next(tok_array_t *ta)
{
	tok_reserve(ta, 1);
	return (&ta->ta_tokens[ta->ta_n++]);
}

static boolean_t
is_separator(const make_t *mk, int c)
{
	switch (c) {
	case '\n':
	case '\t':
	case ' ':
	case ':':
	case '+':
	case '=':
	case '$':
	case ';':
		return (B_TRUE);
	case '!':
		if (mk->mk_style & MS_BSD)
			return (B_TRUE);
		return (B_FALSE);
	case '?':
		if (mk->mk_style & (MS_BSD|MS_GNU))
			return (B_TRUE);
		return (B_FALSE);
	case '|':
		if (mk->mk_style & (MS_GNU))
			return (B_TRUE);
		return (B_FALSE);
	}
	return (B_FALSE);
}

static boolean_t
parse_start_of_line(const char **p, const char *end, token_t *t)
{
	size_t plen = (size_t)(end - *p);

	for (size_t i = 0; i < ARRAY_SIZE(twtbl); i++) {
		struct tok_word *tw = &twtbl[i];
		size_t len = strlen(tw->tw_str);

		if (len > plen)
			continue;

		if (strncmp(*p, tw->tw_str, len) != 0)
			continue;

		/*
		 * Is keyword (e.g. '.if') followed by space or tab?
		 * Used to distinguish '.if' vs. '.ifdef', etc.
		 */
		if ((*p + len > end) || ((*p)[len] != ' ' && (*p)[len] != '\t'))
			continue;

		t->tok_val = *p;
		t->tok_len = len;
		t->tok_type = tw->tw_type;
		*p += len;
		return (B_TRUE);
	}

	return (B_FALSE);
}

static boolean_t
parse_variable(const char **pp, const char *end, token_t *t, size_t depth)
{
	const char *start = *pp;
	const char *p = *pp;
	char close;

	if (*p != '$')
		return (B_FALSE);

	if (depth == VAR_NEST_MAX) {
		/* XXX: error */
		return (B_FALSE);
	}

	if (p + 1 >= end) {
		/* XXX: error */
		return (B_FALSE);
	}

	switch (p[1]) {
	case '(':
		close = ')';
		break;
	case '{':
		close = '}';
		break;
	case '\n':
		/* XXX: error */
		return (B_FALSE);
	default:
		p += 2;
		goto done;
	}

	/* skip $ */
	t->tok_len += 2;
	p += 2;

	while (p < end) {
		if (*p == '$') {
			if (!parse_variable(&p, end, t, depth + 1))
				return (B_FALSE);
			continue;
		}

		if (*p++ == close)
			goto done;
	}

done:
	if (p == end) {
		/* XXX Error */
		return (B_FALSE);
	}

	t->tok_type = TOK_VARIABLE;
	t->tok_val = start;
	t->tok_len = (size_t)(p - start);
	*pp = p;
	return (B_TRUE);
}

static void
parse_span(const char **pp, const char *end, const char *sep, token_t *t)
{
	const char *p = NULL;
	boolean_t litnext = B_FALSE;

	for (t->tok_val = p = *pp; p < end; p++) {
		if (litnext) {
			litnext = B_FALSE;
			continue;
		}
		if (*p == '\\') {
			litnext = B_TRUE;
			continue;
		}
		if (strchr(sep, *p) != NULL)
			break;
	}
	t->tok_len = (size_t)(p - t->tok_val);
	*pp = p;
}

static boolean_t
parse_comment(const char **pp, const char *end, token_t *t)
{
	const char *p = *pp;

	if (*p != '#')
		return (B_FALSE);

	t->tok_type = TOK_COMMENT;
	parse_span(pp, end, sep_nl, t);
	return (B_TRUE);
}

static void
parse_recipe(const char **pp, const char *end, token_t *t)
{
	t->tok_type = TOK_RECIPE;
	parse_span(pp, end, sep_nl, t);
}

static void
absorb_whitespace(const char **pp, const char *end, token_t *t)
{
	const char *p;

	for (t->tok_ws = p = *pp; p < end; p++) {
		if (*p == '\\' && p + 1 < end && p[1] == '\n') {
			p++;
			continue;
		}
		if (*p != ' ' && *p != '\t')
			break;
	}
	t->tok_wslen = (size_t)(p - t->tok_ws);

	*pp = p;
}

static void tok_print(token_t *);

#define	START_OF_LINE(tp) ((tp) == NULL || (tp)->tok_type == TOK_NL)
boolean_t
tokenize(make_t *mk, input_t *in)
{
	const char *p = input_line(in, 0);
	const char *end = input_line(in, input_numlines(in));
	token_t *t = NULL, *tprev = NULL;
	tok_array_t ta = { 0 };

	while (p < end) {
		tprev = t;

		t = tok_next(&ta);
		t->tok_src = in;

		if (tprev != NULL)
			tok_print(tprev);

		if (START_OF_LINE(tprev)) {
			if (*p == '\t') {
				t->tok_ws = p++;
				t->tok_wslen = 1;
				t->tok_val = p;
				parse_recipe(&p, end, t);
				continue;
			}

			absorb_whitespace(&p, end, t);
			if (p == end)
				break;

			if (parse_start_of_line(&p, end, t))
				continue;
			if (parse_variable(&p, end, t, 0))
				continue;
			if (parse_comment(&p, end, t))
				continue;

			t->tok_type = TOK_STRING;
			parse_span(&p, end, tgt_sep_sysv, t);
			continue;
		}

		absorb_whitespace(&p, end, t);
		t->tok_val = p;

		switch (*p) {
		case '#':
			VERIFY(parse_comment(&p, end, t));
			continue;
		case '$':
			if (!parse_variable(&p, end, t, 0))
				return (B_FALSE);
			continue;
		case ':':
			if (p + 1 < end) {
				if (p[1] == ':') {
					t->tok_type = TOK_COLONCOLON;
					t->tok_len = 2;
					p += 2;
					continue;
				} else if (p[1] == '=') {
					t->tok_type = TOK_COLONEQ;
					t->tok_len = 2;
					p += 2;
					continue;
				}
			}
			t->tok_type = TOK_COLON;
			t->tok_len = 1;
			p += 1;
			continue;
		case '+':
			if ((p + 1 < end) && p[1] == '=') {
				t->tok_type = TOK_PLUSEQ;
				t->tok_len = 2;
				p += 2;
				continue;
			}
			t->tok_type = TOK_PLUS;
			t->tok_len = 1;
			p++;
			continue;
		case ';':
			t->tok_type = TOK_SEMICOLON;
			t->tok_len = 1;
			p++;
			continue;
		case '|':
			if ((p + 1 < end) && p[1] == '|') {
				t->tok_type = TOK_OR;
				t->tok_len = 2;
				p += 2;
				continue;
			}
			t->tok_type = TOK_PIPE;
			t->tok_len = 1;
			p++;
			continue;
		case '=':
			if ((p + 1 < end) && p[1] == '=') {
				t->tok_type = TOK_EQUALSEQUALS;
				t->tok_len = 2;
				p += 2;
				continue;
			}
			t->tok_type = TOK_EQUALS;
			t->tok_len = 1;
			p++;
			continue;
		case '?':
			if (p + 1 == end || p[1] != '=')
				break;
			t->tok_type = TOK_QUESEQ;
			t->tok_len = 2;
			p += 2;
			continue;
		case '&':
			if (p + 1 == end || p[1] != '&')
				break;
			t->tok_type = TOK_AND;
			t->tok_len = 2;
			p += 2;
			continue;
		case '\n':
			p++;
			while (p < end) {
				if (*p != '\n')
					break;
				p++;
			}

			t->tok_type = TOK_NL;
			if (t->tok_ws != t->tok_val)
				t->tok_wslen = (size_t)(t->tok_val - t->tok_ws);
			t->tok_len = (size_t)(p - t->tok_val);
			continue;
		}

		t->tok_type = TOK_STRING;
		parse_span(&p, end, " \t\n", t);
	}

	tok_print(&ta.ta_tokens[ta.ta_n - 1]);
	return (B_TRUE);
}

#define	STR(x) case x: return (#x)
static const char *
tok_type_name(token_type_t type)
{
	switch (type) {
	STR(TOK_STRING);
	STR(TOK_RECIPE);
	STR(TOK_VARIABLE);
	STR(TOK_COLON);
	STR(TOK_COLONCOLON);
	STR(TOK_PLUS);
	STR(TOK_EQUALS);
	STR(TOK_EQUALSEQUALS);
	STR(TOK_COLONEQ);
	STR(TOK_PLUSEQ);
	STR(TOK_QUESEQ);
	STR(TOK_PIPE);
	STR(TOK_SEMICOLON);
	STR(TOK_BANGEQ);
	STR(TOK_BANG);
	STR(TOK_COMMENT);
	STR(TOK_FOR);
	STR(TOK_IN);
	STR(TOK_ENDFOR);
	STR(TOK_NL);
	STR(TOK_ERROR);
	STR(TOK_EXPORT);
	STR(TOK_EXPORT_ENV);
	STR(TOK_EXPORT_LIT);
	STR(TOK_INFO);
	STR(TOK_UNDEF);
	STR(TOK_UNEXPORT);
	STR(TOK_UNEXPORT_ENV);
	STR(TOK_WARNING);
	STR(TOK_IF);
	STR(TOK_IFDEF);
	STR(TOK_IFNDEF);
	STR(TOK_IFMAKE);
	STR(TOK_IFNMAKE);
	STR(TOK_ELSE);
	STR(TOK_ELIF);
	STR(TOK_ELIFDEF);
	STR(TOK_ELIFNDEF);
	STR(TOK_ELIFNMAKE);
	STR(TOK_ENDIF);
	STR(TOK_OR);
	STR(TOK_AND);
	STR(TOK_INCLUDE);
	STR(TOK_DEFINE);
	STR(TOK_ENDDEF);
	STR(TOK_IFEQ);
	STR(TOK_IFNEQ);
	STR(TOK_LPAREN);
	STR(TOK_RPAREN);
	}
	return ("");
}
#undef STR

void
tok_print_val(const token_t *t, FILE *f, boolean_t ws)
{
	const char *wsp = ws ? t->tok_ws : "";
	const char *vp = t->tok_val;
	int wslen = ws ? (int)t->tok_wslen : 0;
	int tlen = (int)t->tok_len;

	(void) fprintf(f, "%.*s%.*s", wslen, wsp, tlen, t->tok_val);
}

static void
tok_print(token_t *t)
{
	static size_t count = 0;

	printf("%s", tok_type_name(t->tok_type));
	if (t->tok_type != TOK_NL)
		printf("('%.*s')", (int)t->tok_len, t->tok_val);
	fputc('\n', stdout);

	if (++count > 100)
		exit(1);
}
