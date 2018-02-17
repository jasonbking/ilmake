#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/debug.h>
#include <umem.h>

#include "custr.h"
#include "input.h"
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

static void
iter_cb(iter_event_t evt, input_t *in, void *arg)
{
	switch (evt) {
	case IEVT_PUSH:
		printf("# New file: %s\n", in->in_filename);
		break;
	case IEVT_POP:
		if (in != NULL)
			printf("# File done: %s\n", in->in_filename);
		else
			printf("## Finished\n");
		break;
	}
}

int
main(int argc, char **argv)
{
	input_t **files = NULL;
	input_iter_t *it = NULL;
	custr_t *line = NULL;

	umem_nofail_callback(nofail_cb);

	VERIFY0(custr_alloc(&line, cu_memops));
	files = xcalloc(argc - 1, sizeof (input_t *));

	for (size_t i = 1; i < argc; i++) {
		FILE *f = fopen(argv[i], "r");

		if (f == NULL)
			err(EXIT_FAILURE, "%s", argv[i]);

		files[i - 1] = input_init(input_alloc(), f, argv[i]);
		fclose(f);
	}

	it = input_iter(files[0], iter_cb, NULL);
	for (size_t i = 2; i < argc; i++)
		iter_push(it, files[i - 1]);

	while (iter_line(it, line)) {
		const char *s = custr_cstr(line);
		int len = custr_len(line);

		if (len > 0 && s[len - 1] == '\n')
			len--;

		printf("Line: (%d) '%.*s'\n", len, len, s);
		custr_reset(line);
	}

	return (0);
}
