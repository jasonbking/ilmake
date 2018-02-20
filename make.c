#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/debug.h>
#include <umem.h>

#include "custr.h"
#include "input.h"
#include "make.h"
#include "parse.h"
#include "token.h"
#include "util.h"

#ifdef DEBUG
const char *
_umem_debug_init(void)
{
	return ("default,verbose");
}

const char *
_umem_logging_init(void)
{
	return ("fail,contents");
}

static int
nofail_cb(void)
{
	assfail("Out of memory", __FILE__, __LINE__);
}
#else
const char *
_umem_debug_init(void)
{
	return ("guards");
}

static int
nofail_cb(void)
{
	(void) fprintf(stderr, _("Out of memory\n"));
	return (UMEM_CALLBACK_EXIT(1));
}
#endif

void tok_parse(make_t *mk, input_t *);

int
main(int argc, char **argv)
{
	input_t *in = NULL;
	make_t mk = {
		.mk_debug = stderr,
		.mk_debug_flags = MDF_PARSE,
		.mk_style = MS_SYSV,
	};

	umem_nofail_callback(nofail_cb);

	if (argc < 2) {
		in = input_fnew("(stdin)", stdin);
		parse_input(&mk, in);
		input_free(in);
	}

	for (size_t i = 1; i < argc; i++) {
		in = input_new(argv[i]);

		parse_input(&mk, in);
		printf("-------\n");
		tokenize(&mk, in);
		input_free(in);
	}

	return (0);
}
