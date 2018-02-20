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

#include <stdio.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct input;
struct make;

typedef enum token_type {
	TOK_STRING,
	TOK_RECIPE,
	TOK_VARIABLE,
	TOK_COLON,		/* : */
	TOK_COLONCOLON,		/* :: */
	TOK_PLUS,		/* + */
	TOK_EQUALS,		/* = */
	TOK_EQUALSEQUALS,	/* == */
	TOK_COLONEQ,		/* := */
	TOK_PLUSEQ,		/* += */
	TOK_QUESEQ,		/* ?= mmm... queseq */
	TOK_PIPE,		/* | */
	TOK_SEMICOLON,		/* ; */
	TOK_BANGEQ,		/* != */
	TOK_BANG,		/* ! */
	TOK_COMMENT,
	TOK_FOR,		/* .for */
	TOK_IN,			/* in */
	TOK_ENDFOR,		/* .endfor */
	TOK_NL,			/* \n */
	TOK_ERROR,		/* .error */
	TOK_EXPORT,		/* .export */
	TOK_EXPORT_ENV,		/* .export-env */
	TOK_EXPORT_LIT,		/* .export-literal */
	TOK_INFO,		/* .info */
	TOK_UNDEF,		/* .undef */
	TOK_UNEXPORT,		/* .unexport */
	TOK_UNEXPORT_ENV,	/* .unexport-env */
	TOK_WARNING,		/* .warning */
	TOK_IF,			/* .if */
	TOK_IFDEF,		/* .ifdef */
	TOK_IFNDEF,		/* .ifndef */
	TOK_IFMAKE,		/* .ifmake */
	TOK_IFNMAKE,		/* .ifnmake */
	TOK_ELSE,		/* .else */
	TOK_ELIF,		/* .elif */
	TOK_ELIFDEF,		/* .elifdef */
	TOK_ELIFNDEF,		/* .elifndef */
	TOK_ELIFNMAKE,		/* .elifnmake */
	TOK_ENDIF,		/* .endif */
	TOK_OR,			/* || */
	TOK_AND,		/* && */
	TOK_INCLUDE,		/* include, .include */
	TOK_DEFINE,		/* define */
	TOK_ENDDEF,		/* enddef */
	TOK_IFEQ,		/* ifeq */
	TOK_IFNEQ,		/* ifneq */
	TOK_LPAREN,		/* ( */
	TOK_RPAREN,		/* ) */
} token_type_t;

typedef struct token {
	const char	*tok_val;	/* value */
	const char	*tok_ws;	/* leading white space */
	struct input	*tok_src;	/* source of token */
	size_t		tok_len;	/* value length */
	size_t		tok_wslen;	/* length of whitespace */
	token_type_t	tok_type;	/* type */
} token_t;

boolean_t tokenize(struct make *, struct input *);
void tok_print_val(const token_t *, FILE *, boolean_t);

#ifdef __cplusplus
}
#endif

#endif /* _TOKEN_H */
