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

#ifndef _TOKEN_H
#define	_TOKEN_H

#ifdef __cplusplus
extern "C" {
#endif

struct custr;
struct input;

typedef enum token_type {
	TOK_TARGET,
	TOK_RECIPE,
	TOK_VARIABLE,
	TOK_COLON,		/* : */
	TOK_COLONCOLON,		/* :: */
	TOK_PLUS,		/* + */
	TOK_EQUALS,		/* = */
	TOK_COLONEQ,		/* := */
	TOK_PLUSEQ,		/* += */
	TOK_QUESEQ,		/* ?= */
	TOK_PIPE,		/* | */
	TOK_SEMICOLON,		/* ; */
	TOK_BANGEQ,		/* != */
	TOK_BANG,		/* ! */
	TOK_EOL,
} token_type_t;

typedef struct token {
	token_type_t	tok_type;
	struct input	*tok_src;
	size_t		tok_line;	/* 0 based */
	size_t		tok_col;	/* 0 based */
	char		*tok_val;
} token_t;

token_t	*tok_new(token_type_t, input_t *, size_t, size_t, struct custr *);
void	tok_free(token_t *);

#ifdef __cplusplus
}
#endif

#endif /* _TOKEN_H */
