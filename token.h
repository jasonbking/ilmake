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

struct input;

enum token_type {
	TOK_BEGIN,		/* .BEGIN */
	TOK_DEFAULT,		/* .DEFAULT */
	TOK_DEL_ON_ERROR,
	TOK_END,
	TOK_ERROR,
	TOK_IGNORE,
	TOK_INCLUDES,
	TOK_INTERRUPT,
	TOK_LIBS,
	TOK_META,
	TOK_MFLAGS,
	TOK_MAIN,
	TOK_NOEXPORT,
	TOK_NOMETA,
	TOK_NOMETACMP,
	TOK_NOPATH,
	TOK_NOT,
	TOK_NOT_PARALLEL,
	TOK_NULL,
	TOK_OBJDIR,
	TOK_ORDER,
	TOK_PARALLEL,
	TOK_PATH,
	TOK_PHONY,
	TOK_POSIX,
	TOK_PRECIOUS,
	TOK_SHELL,
	TOK_SILENT,
	TOK_SINGLESHELL,
	TOK_STALE,
	TOK_SUFFIXES,
	TOK_WAIT,
	TOK_ATTRIBUTE,
};
typedef enum token_type token_type_t;

struct token {
	token_type_t	tok_type;
	char		*tok_source;
	size_t		tok_line;	/* 0 based */
	size_t		tok_col;	/* 0 based */
	union {
		char	*toku_str;
	} toku;
};
#define	tok_str	toku.toku_str

#ifdef __cplusplus
}
#endif

#endif /* _TOKEN_H */
