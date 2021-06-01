/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2019-2021 Philippe Proulx <pproulx@efficios.com>
 * Copyright (c) 2020-2021 Simon Marchi <simon.marchi@efficios.com>
 */

#ifndef ARGPAR_ARGPAR_H
#define ARGPAR_ARGPAR_H

#include <stdbool.h>

/*
 * argpar is a library which provides facilities for command-line
 * argument parsing.
 *
 * Create a parsing iterator with argpar_iter_create(), then
 * repeatedly call argpar_iter_next() to access the parsing results,
 * until one of:
 *
 * * There are no more arguments.
 *
 * * The argument parser encounters an error (for example, an unknown
 *   option).
 *
 * * You need to stop.
 *
 * The argpar parser parses the original arguments `argv` of which the
 * count is `argc` using the sentinel-terminated (use
 * `ARGPAR_OPT_DESCR_SENTINEL`) option descriptor array `descrs`.
 *
 * argpar considers ALL the elements of `argv`, including the first one,
 * so that you would typically pass `argc - 1` and `&argv[1]` from what
 * main() receives.
 *
 * The argpar parser supports:
 *
 * * Short options without an argument, possibly tied together:
 *
 *       -f -auf -n
 *
 * * Short options with argument:
 *
 *       -b 45 -f/mein/file -xyzhello
 *
 * * Long options without an argument:
 *
 *       --five-guys --burger-king --pizza-hut --subway
 *
 * * Long options with arguments:
 *
 *       --security enable --time=18.56
 *
 * * Non-option arguments (anything else).
 *
 * The argpar parser parses `-` and `--` as non-option arguments. A
 * non-option argument cannot have the form of an option, for example if
 * you need to pass the exact relative path `--component`. In that case,
 * you would need to pass `./--component`. There's no generic way to
 * escape `-` as of this version.
 *
 * argpar_iter_create() accepts duplicate options in `descrs` (it
 * produces one item for each instance).
 *
 * A returned parsing item has the type `const struct argpar_item *`.
 * Get the type (option or non-option) of an item with
 * argpar_item_type(). Each item type has its set of dedicated methods
 * (`argpar_item_opt_` and `argpar_item_non_opt_` prefixes).
 *
 * argpar_iter_next() produces the items in the same order that the
 * original arguments were parsed, including non-option arguments. This
 * means, for example, that for:
 *
 *     --hello --count=23 /path/to/file -ab --type file magie
 *
 * argpar_iter_next() produces the following items, in this order:
 *
 * 1. Option item (`--hello`).
 * 2. Option item (`--count` with argument `23`).
 * 3. Non-option item (`/path/to/file`).
 * 4. Option item (`-a`).
 * 5. Option item (`-b`).
 * 6. Option item (`--type` with argument `file`).
 * 7. Non-option item (`magie`).
 */

/* Sentinel for an option descriptor array */
#define ARGPAR_OPT_DESCR_SENTINEL	{ -1, '\0', NULL, false }

/*
 * If argpar is used in some shared library, we don't want said library
 * to export its symbols, so mark them as "hidden".
 *
 * On Windows, symbols are local unless explicitly exported; see
 * <https://gcc.gnu.org/wiki/Visibility>.
 */
#if defined(_WIN32) || defined(__CYGWIN__)
# define ARGPAR_HIDDEN
#else
# define ARGPAR_HIDDEN __attribute__((visibility("hidden")))
#endif

/* Forward-declaration for the opaque argpar iterator type */
struct argpar_iter;

/* Option descriptor */
struct argpar_opt_descr {
	/* Numeric ID for this option */
	const int id;

	/* Short option character, or `\0` */
	const char short_name;

	/* Long option name (without the `--` prefix), or `NULL` */
	const char * const long_name;

	/* True if this option has an argument */
	const bool with_arg;
};

/* Item type */
enum argpar_item_type {
	/* Option */
	ARGPAR_ITEM_TYPE_OPT,

	/* Non-option */
	ARGPAR_ITEM_TYPE_NON_OPT,
};

/* Forward-declaration for the opaque argpar parsing item type */
struct argpar_item;

/*
 * Returns the type of the parsing item `item`.
 */
ARGPAR_HIDDEN
enum argpar_item_type argpar_item_type(const struct argpar_item *item);

/*
 * Returns the option descriptor of the option parsing item `item`.
 */
ARGPAR_HIDDEN
const struct argpar_opt_descr *argpar_item_opt_descr(
		const struct argpar_item *item);

/*
 * Returns the argument of the option parsing item `item`, or `NULL` if
 * none.
 */
ARGPAR_HIDDEN
const char *argpar_item_opt_arg(const struct argpar_item *item);

/*
 * Returns the complete argument, pointing to one of the entries of the
 * original arguments (`argv`), of the non-option parsing item `item`.
 */
ARGPAR_HIDDEN
const char *argpar_item_non_opt_arg(const struct argpar_item *item);

/*
 * Returns the original index, within ALL the original arguments
 * (`argv`), of the non-option parsing item `item`.
 */
ARGPAR_HIDDEN
unsigned int argpar_item_non_opt_orig_index(const struct argpar_item *item);

/*
 * Returns the index, within the non-option arguments, of the non-option
 * parsing item `item`.
 */
ARGPAR_HIDDEN
unsigned int argpar_item_non_opt_non_opt_index(const struct argpar_item *item);

/*
 * Destroys `item`, as created by argpar_iter_next().
 */
ARGPAR_HIDDEN
void argpar_item_destroy(const struct argpar_item *item);

/*
 * Creates an argument parsing iterator.
 *
 * This function initializes the returned structure, but doesn't
 * actually start parsing the arguments.
 *
 * `*argv` and `*descrs` must NOT change for the lifetime of the
 * returned iterator (until you call argpar_iter_destroy()) and for the
 * lifetime of any parsing item (until you call argpar_item_destroy())
 * argpar_iter_next() creates for the returned iterator.
 *
 * Call argpar_iter_next() with the returned iterator to obtain the next
 * parsing result (item).
 */
ARGPAR_HIDDEN
struct argpar_iter *argpar_iter_create(unsigned int argc,
		const char * const *argv,
		const struct argpar_opt_descr *descrs);

/*
 * Destroys `iter`, as returned by argpar_iter_create().
 */
ARGPAR_HIDDEN
void argpar_iter_destroy(struct argpar_iter *iter);

/*
 * Return type of argpar_iter_next().
 */
enum argpar_iter_next_status {
	ARGPAR_ITER_NEXT_STATUS_OK,
	ARGPAR_ITER_NEXT_STATUS_END,
	ARGPAR_ITER_NEXT_STATUS_ERROR_UNKNOWN_OPT,
	ARGPAR_ITER_NEXT_STATUS_ERROR_MISSING_OPT_ARG,
	ARGPAR_ITER_NEXT_STATUS_ERROR_UNEXPECTED_OPT_ARG,
	ARGPAR_ITER_NEXT_STATUS_ERROR_MEMORY,
};

/*
 * Parses and returns the next item from `iter`.
 *
 * On success, this function:
 *
 * * Sets `*item` to a parsing item which describes the next option
 *   or non-option argument.
 *
 *   Destroy `*item` with argpar_item_destroy().
 *
 * * Returns `ARGPAR_ITER_NEXT_STATUS_OK`.
 *
 * If there are no more items to return, this function returns
 * `ARGPAR_ITER_NEXT_STATUS_END`.
 *
 * On failure, this function:
 *
 * * Returns one of:
 *
 *   `ARGPAR_ITER_NEXT_STATUS_ERROR_UNKNOWN_OPT`:
 *       Unknown option (not found in `descrs` as passed to
 *       argpar_iter_create() to create `iter`).
 *
 *   `ARGPAR_ITER_NEXT_STATUS_ERROR_MISSING_OPT_ARG`:
 *       Missing option argument.
 *
 *   `ARGPAR_ITER_NEXT_STATUS_ERROR_UNEXPECTED_OPT_ARG`:
 *       Unexpected option argument.
 *
 *   `ARGPAR_ITER_NEXT_STATUS_ERROR_MEMORY`:
 *       Memory error.
 *
 * * Except for the `ARGPAR_ITER_NEXT_STATUS_ERROR_MEMORY` status,
 *   sets `*error`, if not `NULL`, to a descriptive error string.
 *   Free `*error` with free().
 */
enum argpar_iter_next_status argpar_iter_next(
		struct argpar_iter *iter, const struct argpar_item **item,
		char **error);

/*
 * Returns the number of ingested elements from `argv`, as passed to
 * argpar_iter_create() to create `*iter`, that were required to produce
 * the previously returned items.
 */
ARGPAR_HIDDEN
unsigned int argpar_iter_ingested_orig_args(const struct argpar_iter *iter);

/*
 * Destroys `_item` (`const struct argpar_item *`) and sets it to
 * `NULL`.
 */
#define ARGPAR_ITEM_DESTROY_AND_RESET(_item)				\
	{								\
		argpar_item_destroy(_item);				\
		_item = NULL;						\
	}

#endif /* ARGPAR_ARGPAR_H */
