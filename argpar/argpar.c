/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2019-2021 Philippe Proulx <pproulx@efficios.com>
 * Copyright (c) 2020-2021 Simon Marchi <simon.marchi@efficios.com>
 */

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argpar.h"

#define ARGPAR_REALLOC(_ptr, _type, _nmemb)				\
	((_type *) realloc(_ptr, (_nmemb) * sizeof(_type)))

#define ARGPAR_CALLOC(_type, _nmemb)					\
	((_type *) calloc((_nmemb), sizeof(_type)))

#define ARGPAR_ZALLOC(_type) ARGPAR_CALLOC(_type, 1)

#define ARGPAR_ASSERT(_cond) assert(_cond)

#ifdef __MINGW_PRINTF_FORMAT
# define ARGPAR_PRINTF_FORMAT __MINGW_PRINTF_FORMAT
#else
# define ARGPAR_PRINTF_FORMAT printf
#endif

/*
 * An argpar iterator.
 *
 * Such a structure contains the state of an iterator between
 * calls to argpar_iter_parse_next().
 */
struct argpar_iter {
	/*
	 * Data provided by the user to argpar_iter_create(); immutable
	 * afterwards.
	 */
	unsigned int argc;
	const char * const *argv;
	const struct argpar_opt_descr *descrs;

	/*
	 * Index of the argument to process in the next
	 * argpar_iter_parse_next() call.
	 */
	unsigned int i;

	/* Counter of non-option arguments */
	int non_opt_index;

	/*
	 * Current character of the current short option group: if it's
	 * not `NULL`, the parser is in within a short option group,
	 * therefore it must resume there in the next
	 * argpar_iter_parse_next() call.
	 */
	const char *short_opt_ch;
};

/* Base parsing item */
struct argpar_item {
	enum argpar_item_type type;
};

/* Option parsing item */
struct argpar_item_opt {
	struct argpar_item base;

	/* Corresponding descriptor */
	const struct argpar_opt_descr *descr;

	/* Argument, or `NULL` if none; owned by this */
	char *arg;
};

/* Non-option parsing item */
struct argpar_item_non_opt {
	struct argpar_item base;

	/*
	 * Complete argument, pointing to one of the entries of the
	 * original arguments (`argv`).
	 */
	const char *arg;

	/*
	 * Index of this argument amongst all original arguments
	 * (`argv`).
	 */
	unsigned int orig_index;

	/* Index of this argument amongst other non-option arguments */
	unsigned int non_opt_index;
};

static __attribute__((format(ARGPAR_PRINTF_FORMAT, 1, 0)))
char *argpar_vasprintf(const char * const fmt, va_list args)
{
	int len1, len2;
	char *str;
	va_list args2;

	va_copy(args2, args);
	len1 = vsnprintf(NULL, 0, fmt, args);
	if (len1 < 0) {
		str = NULL;
		goto end;
	}

	str = malloc(len1 + 1);
	if (!str) {
		goto end;
	}

	len2 = vsnprintf(str, len1 + 1, fmt, args2);
	ARGPAR_ASSERT(len1 == len2);

end:
	va_end(args2);
	return str;
}


static __attribute__((format(ARGPAR_PRINTF_FORMAT, 1, 2)))
char *argpar_asprintf(const char * const fmt, ...)
{
	va_list args;
	char *str;

	va_start(args, fmt);
	str = argpar_vasprintf(fmt, args);
	va_end(args);
	return str;
}

static __attribute__((format(ARGPAR_PRINTF_FORMAT, 2, 3)))
bool try_append_string_printf(char ** const str, const char *fmt, ...)
{
	char *new_str = NULL;
	char *addendum = NULL;
	bool success;
	va_list args;

	if (!str) {
		success = true;
		goto end;
	}

	ARGPAR_ASSERT(str);
	va_start(args, fmt);
	addendum = argpar_vasprintf(fmt, args);
	va_end(args);

	if (!addendum) {
		success = false;
		goto end;
	}

	new_str = argpar_asprintf("%s%s", *str ? *str : "", addendum);
	if (!new_str) {
		success = false;
		goto end;
	}

	free(*str);
	*str = new_str;
	success = true;

end:
	free(addendum);
	return success;
}

ARGPAR_HIDDEN
enum argpar_item_type argpar_item_type(const struct argpar_item * const item)
{
	ARGPAR_ASSERT(item);
	return item->type;
}

ARGPAR_HIDDEN
const struct argpar_opt_descr *argpar_item_opt_descr(
		const struct argpar_item * const item)
{
	ARGPAR_ASSERT(item);
	ARGPAR_ASSERT(item->type == ARGPAR_ITEM_TYPE_OPT);
	return ((const struct argpar_item_opt *) item)->descr;
}

ARGPAR_HIDDEN
const char *argpar_item_opt_arg(const struct argpar_item * const item)
{
	ARGPAR_ASSERT(item);
	ARGPAR_ASSERT(item->type == ARGPAR_ITEM_TYPE_OPT);
	return ((const struct argpar_item_opt *) item)->arg;
}

ARGPAR_HIDDEN
const char *argpar_item_non_opt_arg(const struct argpar_item * const item)
{
	ARGPAR_ASSERT(item);
	ARGPAR_ASSERT(item->type == ARGPAR_ITEM_TYPE_NON_OPT);
	return ((const struct argpar_item_non_opt *) item)->arg;
}

ARGPAR_HIDDEN
unsigned int argpar_item_non_opt_orig_index(
		const struct argpar_item * const item)
{
	ARGPAR_ASSERT(item);
	ARGPAR_ASSERT(item->type == ARGPAR_ITEM_TYPE_NON_OPT);
	return ((const struct argpar_item_non_opt *) item)->orig_index;
}

ARGPAR_HIDDEN
unsigned int argpar_item_non_opt_non_opt_index(
		const struct argpar_item * const item)
{
	ARGPAR_ASSERT(item);
	ARGPAR_ASSERT(item->type == ARGPAR_ITEM_TYPE_NON_OPT);
	return ((const struct argpar_item_non_opt *) item)->non_opt_index;
}

ARGPAR_HIDDEN
void argpar_item_destroy(const struct argpar_item * const item)
{
	if (!item) {
		goto end;
	}

	if (item->type == ARGPAR_ITEM_TYPE_OPT) {
		struct argpar_item_opt * const opt_item =
			(struct argpar_item_opt *) item;

		free(opt_item->arg);
	}

	free((void *) item);

end:
	return;
}

static
bool push_item(struct argpar_item_array * const array,
		const struct argpar_item * const item)
{
	bool success;

	ARGPAR_ASSERT(array);
	ARGPAR_ASSERT(item);

	if (array->n_items == array->n_alloc) {
		const unsigned int new_n_alloc = array->n_alloc * 2;
		const struct argpar_item ** const new_items =
			ARGPAR_REALLOC(array->items, const struct argpar_item *,
				new_n_alloc);
		if (!new_items) {
			success = false;
			goto end;
		}

		array->n_alloc = new_n_alloc;
		array->items = new_items;
	}

	array->items[array->n_items] = item;
	array->n_items++;
	success = true;

end:
	return success;
}

static
void destroy_item_array(struct argpar_item_array * const array)
{
	if (array) {
		unsigned int i;

		for (i = 0; i < array->n_items; i++) {
			argpar_item_destroy(array->items[i]);
		}

		free(array->items);
		free(array);
	}
}

static
struct argpar_item_array *create_item_array(void)
{
	struct argpar_item_array *ret;
	const int initial_size = 10;

	ret = ARGPAR_ZALLOC(struct argpar_item_array);
	if (!ret) {
		goto end;
	}

	ret->items = ARGPAR_CALLOC(const struct argpar_item *, initial_size);
	if (!ret->items) {
		goto error;
	}

	ret->n_alloc = initial_size;
	goto end;

error:
	destroy_item_array(ret);
	ret = NULL;

end:
	return ret;
}

static
struct argpar_item_opt *create_opt_item(
		const struct argpar_opt_descr * const descr,
		const char * const arg)
{
	struct argpar_item_opt *opt_item =
		ARGPAR_ZALLOC(struct argpar_item_opt);

	if (!opt_item) {
		goto end;
	}

	opt_item->base.type = ARGPAR_ITEM_TYPE_OPT;
	opt_item->descr = descr;

	if (arg) {
		opt_item->arg = strdup(arg);
		if (!opt_item->arg) {
			goto error;
		}
	}

	goto end;

error:
	argpar_item_destroy(&opt_item->base);
	opt_item = NULL;

end:
	return opt_item;
}

static
struct argpar_item_non_opt *create_non_opt_item(const char * const arg,
		const unsigned int orig_index,
		const unsigned int non_opt_index)
{
	struct argpar_item_non_opt * const non_opt_item =
		ARGPAR_ZALLOC(struct argpar_item_non_opt);

	if (!non_opt_item) {
		goto end;
	}

	non_opt_item->base.type = ARGPAR_ITEM_TYPE_NON_OPT;
	non_opt_item->arg = arg;
	non_opt_item->orig_index = orig_index;
	non_opt_item->non_opt_index = non_opt_index;

end:
	return non_opt_item;
}

static
const struct argpar_opt_descr *find_descr(
		const struct argpar_opt_descr * const descrs,
		const char short_name, const char * const long_name)
{
	const struct argpar_opt_descr *descr;

	for (descr = descrs; descr->short_name || descr->long_name; descr++) {
		if (short_name && descr->short_name &&
				short_name == descr->short_name) {
			goto end;
		}

		if (long_name && descr->long_name &&
				strcmp(long_name, descr->long_name) == 0) {
			goto end;
		}
	}

end:
	return !descr->short_name && !descr->long_name ? NULL : descr;
}

enum parse_orig_arg_opt_ret {
	PARSE_ORIG_ARG_OPT_RET_OK,
	PARSE_ORIG_ARG_OPT_RET_ERROR_UNKNOWN_OPT = -2,
	PARSE_ORIG_ARG_OPT_RET_ERROR = -1,
};

static
enum parse_orig_arg_opt_ret parse_short_opts(const char * const short_opts,
		const char * const next_orig_arg,
		const struct argpar_opt_descr * const descrs,
		struct argpar_iter * const iter,
		char ** const error, struct argpar_item ** const item)
{
	enum parse_orig_arg_opt_ret ret = PARSE_ORIG_ARG_OPT_RET_OK;
	bool used_next_orig_arg = false;
	const char *opt_arg = NULL;
	const struct argpar_opt_descr *descr;
	struct argpar_item_opt *opt_item;

	if (strlen(short_opts) == 0) {
		try_append_string_printf(error, "Invalid argument");
		goto error;
	}

	if (!iter->short_opt_ch) {
		iter->short_opt_ch = short_opts;
	}

	/* Find corresponding option descriptor */
	descr = find_descr(descrs, *iter->short_opt_ch, NULL);
	if (!descr) {
		ret = PARSE_ORIG_ARG_OPT_RET_ERROR_UNKNOWN_OPT;
		try_append_string_printf(error, "Unknown option `-%c`",
			*iter->short_opt_ch);
		goto error;
	}

	if (descr->with_arg) {
		if (iter->short_opt_ch[1]) {
			/* `-oarg` form */
			opt_arg = &iter->short_opt_ch[1];
		} else {
			/* `-o arg` form */
			opt_arg = next_orig_arg;
			used_next_orig_arg = true;
		}

		/*
		 * We accept `-o ''` (empty option argument), but not
		 * `-o` alone if an option argument is expected.
		 */
		if (!opt_arg || (iter->short_opt_ch[1] &&
				strlen(opt_arg) == 0)) {
			try_append_string_printf(error,
				"Missing required argument for option `-%c`",
				*iter->short_opt_ch);
			used_next_orig_arg = false;
			goto error;
		}
	}

	/* Create and append option argument */
	opt_item = create_opt_item(descr, opt_arg);
	if (!opt_item) {
		goto error;
	}

	*item = &opt_item->base;
	iter->short_opt_ch++;

	if (descr->with_arg || !*iter->short_opt_ch) {
		/* Option has an argument: no more options */
		iter->short_opt_ch = NULL;

		if (used_next_orig_arg) {
			iter->i += 2;
		} else {
			iter->i++;
		}
	}

	goto end;

error:
	if (ret == PARSE_ORIG_ARG_OPT_RET_OK) {
		ret = PARSE_ORIG_ARG_OPT_RET_ERROR;
	}

end:
	return ret;
}

static
enum parse_orig_arg_opt_ret parse_long_opt(const char * const long_opt_arg,
		const char * const next_orig_arg,
		const struct argpar_opt_descr * const descrs,
		struct argpar_iter * const iter,
		char ** const error, struct argpar_item ** const item)
{
	const size_t max_len = 127;
	enum parse_orig_arg_opt_ret ret = PARSE_ORIG_ARG_OPT_RET_OK;
	const struct argpar_opt_descr *descr;
	struct argpar_item_opt *opt_item;
	bool used_next_orig_arg = false;

	/* Option's argument, if any */
	const char *opt_arg = NULL;

	/* Position of first `=`, if any */
	const char *eq_pos;

	/* Buffer holding option name when `long_opt_arg` contains `=` */
	char buf[max_len + 1];

	/* Option name */
	const char *long_opt_name = long_opt_arg;

	if (strlen(long_opt_arg) == 0) {
		try_append_string_printf(error, "Invalid argument");
		goto error;
	}

	/* Find the first `=` in original argument */
	eq_pos = strchr(long_opt_arg, '=');
	if (eq_pos) {
		const size_t long_opt_name_size = eq_pos - long_opt_arg;

		/* Isolate the option name */
		if (long_opt_name_size > max_len) {
			try_append_string_printf(error,
				"Invalid argument `--%s`", long_opt_arg);
			goto error;
		}

		memcpy(buf, long_opt_arg, long_opt_name_size);
		buf[long_opt_name_size] = '\0';
		long_opt_name = buf;
	}

	/* Find corresponding option descriptor */
	descr = find_descr(descrs, '\0', long_opt_name);
	if (!descr) {
		try_append_string_printf(error, "Unknown option `--%s`",
			long_opt_name);
		ret = PARSE_ORIG_ARG_OPT_RET_ERROR_UNKNOWN_OPT;
		goto error;
	}

	/* Find option's argument if any */
	if (descr->with_arg) {
		if (eq_pos) {
			/* `--long-opt=arg` style */
			opt_arg = eq_pos + 1;
		} else {
			/* `--long-opt arg` style */
			if (!next_orig_arg) {
				try_append_string_printf(error,
					"Missing required argument for option `--%s`",
					long_opt_name);
				goto error;
			}

			opt_arg = next_orig_arg;
			used_next_orig_arg = true;
		}
	} else if (eq_pos) {
		/*
		 * Unexpected `--opt=arg` style for a long option which
		 * doesn't accept an argument.
		 */
		try_append_string_printf(error,
			"Unexpected argument for option `--%s`", long_opt_name);
		goto error;
	}

	/* Create and append option argument */
	opt_item = create_opt_item(descr, opt_arg);
	if (!opt_item) {
		goto error;
	}

	if (used_next_orig_arg) {
		iter->i += 2;
	} else {
		iter->i++;
	}

	*item = &opt_item->base;
	goto end;

error:
	if (ret == PARSE_ORIG_ARG_OPT_RET_OK) {
		ret = PARSE_ORIG_ARG_OPT_RET_ERROR;
	}

end:
	return ret;
}

static
enum parse_orig_arg_opt_ret parse_orig_arg_opt(const char * const orig_arg,
		const char * const next_orig_arg,
		const struct argpar_opt_descr * const descrs,
		struct argpar_iter * const iter, char ** const error,
		struct argpar_item ** const item)
{
	enum parse_orig_arg_opt_ret ret = PARSE_ORIG_ARG_OPT_RET_OK;

	ARGPAR_ASSERT(orig_arg[0] == '-');

	if (orig_arg[1] == '-') {
		/* Long option */
		ret = parse_long_opt(&orig_arg[2],
			next_orig_arg, descrs, iter, error, item);
	} else {
		/* Short option */
		ret = parse_short_opts(&orig_arg[1],
			next_orig_arg, descrs, iter, error, item);
	}

	return ret;
}

static
bool try_prepend_while_parsing_arg_to_error(char ** const error,
		const unsigned int i, const char * const arg)
{
	char *new_error;
	bool success;

	if (!error) {
		success = true;
		goto end;
	}

	ARGPAR_ASSERT(*error);
	new_error = argpar_asprintf("While parsing argument #%u (`%s`): %s",
		i + 1, arg, *error);
	if (!new_error) {
		success = false;
		goto end;
	}

	free(*error);
	*error = new_error;
	success = true;

end:
	return success;
}

ARGPAR_HIDDEN
struct argpar_iter *argpar_iter_create(const unsigned int argc,
		const char * const * const argv,
		const struct argpar_opt_descr * const descrs)
{
	struct argpar_iter * const iter = ARGPAR_ZALLOC(struct argpar_iter);

	if (!iter) {
		goto end;
	}

	iter->argc = argc;
	iter->argv = argv;
	iter->descrs = descrs;

end:
	return iter;
}

ARGPAR_HIDDEN
void argpar_iter_destroy(struct argpar_iter * const iter)
{
	free(iter);
}

ARGPAR_HIDDEN
enum argpar_iter_parse_next_status argpar_iter_parse_next(
		struct argpar_iter * const iter,
		const struct argpar_item ** const item, char ** const error)
{
	enum argpar_iter_parse_next_status status;
	enum parse_orig_arg_opt_ret parse_orig_arg_opt_ret;
	const char *orig_arg;
	const char *next_orig_arg;

	ARGPAR_ASSERT(iter->i <= iter->argc);

	if (error) {
		*error = NULL;
	}

	if (iter->i == iter->argc) {
		status = ARGPAR_ITER_PARSE_NEXT_STATUS_END;
		goto end;
	}

	orig_arg = iter->argv[iter->i];
	next_orig_arg =
		iter->i < (iter->argc - 1) ? iter->argv[iter->i + 1] : NULL;

	if (orig_arg[0] != '-') {
		/* Non-option argument */
		struct argpar_item_non_opt * const non_opt_item =
			create_non_opt_item(orig_arg, iter->i,
				iter->non_opt_index);

		if (!non_opt_item) {
			status = ARGPAR_ITER_PARSE_NEXT_STATUS_ERROR;
			goto end;
		}

		iter->non_opt_index++;
		iter->i++;
		*item = &non_opt_item->base;
		status = ARGPAR_ITER_PARSE_NEXT_STATUS_OK;
		goto end;
	}

	/* Option argument */
	parse_orig_arg_opt_ret = parse_orig_arg_opt(orig_arg,
		next_orig_arg, iter->descrs, iter, error,
		(struct argpar_item **) item);
	switch (parse_orig_arg_opt_ret) {
	case PARSE_ORIG_ARG_OPT_RET_OK:
		status = ARGPAR_ITER_PARSE_NEXT_STATUS_OK;
		break;
	case PARSE_ORIG_ARG_OPT_RET_ERROR_UNKNOWN_OPT:
		try_prepend_while_parsing_arg_to_error(error, iter->i,
			orig_arg);
		status = ARGPAR_ITER_PARSE_NEXT_STATUS_ERROR_UNKNOWN_OPT;
		break;
	case PARSE_ORIG_ARG_OPT_RET_ERROR:
		try_prepend_while_parsing_arg_to_error(error, iter->i,
			orig_arg);
		status = ARGPAR_ITER_PARSE_NEXT_STATUS_ERROR;
		break;
	default:
		abort();
	}

end:
	return status;
}

ARGPAR_HIDDEN
unsigned int argpar_iter_get_ingested_orig_args(
		const struct argpar_iter * const iter)
{
	return iter->i;
}

ARGPAR_HIDDEN
struct argpar_parse_ret argpar_parse(const unsigned int argc,
		const char * const * const argv,
		const struct argpar_opt_descr * const descrs,
		const bool fail_on_unknown_opt)
{
	struct argpar_parse_ret parse_ret = { 0 };
	const struct argpar_item *item = NULL;
	struct argpar_iter *iter = NULL;

	parse_ret.items = create_item_array();
	if (!parse_ret.items) {
		parse_ret.error = strdup("Failed to create items array.");
		ARGPAR_ASSERT(parse_ret.error);
		goto error;
	}

	iter = argpar_iter_create(argc, argv, descrs);
	if (!iter) {
		parse_ret.error = strdup("Failed to create argpar iter.");
		ARGPAR_ASSERT(parse_ret.error);
		goto error;
	}

	while (true) {
		const enum argpar_iter_parse_next_status status =
			argpar_iter_parse_next(iter, &item, &parse_ret.error);

		if (status == ARGPAR_ITER_PARSE_NEXT_STATUS_ERROR) {
			goto error;
		} else if (status == ARGPAR_ITER_PARSE_NEXT_STATUS_END) {
			break;
		} else if (status == ARGPAR_ITER_PARSE_NEXT_STATUS_ERROR_UNKNOWN_OPT) {
			if (fail_on_unknown_opt) {
				parse_ret.ingested_orig_args =
					argpar_iter_get_ingested_orig_args(iter);
				goto error;
			}

			free(parse_ret.error);
			parse_ret.error = NULL;
			break;
		}

		ARGPAR_ASSERT(status == ARGPAR_ITER_PARSE_NEXT_STATUS_OK);

		if (!push_item(parse_ret.items, item)) {
			goto error;
		}

		item = NULL;
	}

	ARGPAR_ASSERT(!parse_ret.error);
	parse_ret.ingested_orig_args = argpar_iter_get_ingested_orig_args(iter);
	goto end;

error:
	ARGPAR_ASSERT(parse_ret.error);

	/* That's how we indicate that an error occurred */
	destroy_item_array(parse_ret.items);
	parse_ret.items = NULL;

end:
	argpar_iter_destroy(iter);
	argpar_item_destroy(item);
	return parse_ret;
}

ARGPAR_HIDDEN
void argpar_parse_ret_fini(struct argpar_parse_ret * const ret)
{
	ARGPAR_ASSERT(ret);
	destroy_item_array(ret->items);
	ret->items = NULL;
	free(ret->error);
	ret->error = NULL;
}
