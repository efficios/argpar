#ifndef BABELTRACE_ARGPAR_H
#define BABELTRACE_ARGPAR_H

/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <glib.h>
#include <stdbool.h>

#include "common/macros.h"

/* Sentinel for an option descriptor array */
#define BT_ARGPAR_OPT_DESCR_SENTINEL	{ -1, '\0', NULL, false }

/* Option descriptor */
struct bt_argpar_opt_descr {
	/* Numeric ID for this option */
	const int id;

	/* Short option character, or `\0` */
	const char short_name;

	/* Long option name (without `--`), or `NULL` */
	const char * const long_name;

	/* True if this option has an argument */
	const bool with_arg;
};

/* Item type */
enum bt_argpar_item_type {
	/* Option */
	BT_ARGPAR_ITEM_TYPE_OPT,

	/* Non-option */
	BT_ARGPAR_ITEM_TYPE_NON_OPT,
};

/* Base item */
struct bt_argpar_item {
	enum bt_argpar_item_type type;
};

/* Option item */
struct bt_argpar_item_opt {
	struct bt_argpar_item base;

	/* Corresponding descriptor */
	const struct bt_argpar_opt_descr *descr;

	/* Argument, or `NULL` if none */
	const char *arg;
};

/* Non-option item */
struct bt_argpar_item_non_opt {
	struct bt_argpar_item base;

	/*
	 * Complete argument, pointing to one of the entries of the
	 * original arguments (`argv`).
	 */
	const char *arg;

	/* Index of this argument amongst all original arguments (`argv`) */
	unsigned int orig_index;

	/* Index of this argument amongst other non-option arguments */
	unsigned int non_opt_index;
};

/* What is returned by bt_argpar_parse() */
struct bt_argpar_parse_ret {
	/* Array of `struct bt_argpar_item *`, or `NULL` on error */
	GPtrArray *items;

	/* Error string, or `NULL` if none */
	GString *error;

	/* Number of original arguments (`argv`) ingested */
	unsigned int ingested_orig_args;
};

/*
 * Parses the arguments `argv` of which the count is `argc` using the
 * sentinel-terminated (use `BT_ARGPAR_OPT_DESCR_SENTINEL`) option
 * descriptor array `descrs`.
 *
 * This function considers ALL the elements of `argv`, including the
 * first one, so that you would typically pass `argc - 1` and
 * `&argv[1]` from what main() receives.
 *
 * This argument parser supports:
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
 * This function does not accept `-` or `--` as arguments. The latter
 * means "end of options" for many command-line tools, but this function
 * is all about keeping the order of the arguments, so it does not mean
 * much to put them at the end. This has the side effect that a
 * non-option argument cannot have the form of an option, for example if
 * you need to pass the exact relative path `--component`. In that case,
 * you would need to pass `./--component`. There's no generic way to
 * escape `-` for the moment.
 *
 * This function accepts duplicate options (the resulting array of items
 * contains one entry for each instance).
 *
 * On success, this function returns an array of items
 * (`struct bt_argpar_item *`). Each item is to be casted to the
 * appropriate type (`struct bt_argpar_item_opt *` or
 * `struct bt_argpar_item_non_opt *`) depending on its type.
 *
 * The returned array contains the items in the same order that the
 * arguments were parsed, including non-option arguments. This means,
 * for example, that for
 *
 *     --hello --meow=23 /path/to/file -b
 *
 * the function returns an array of four items: two options, one
 * non-option, and one option.
 *
 * In the returned structure, `ingested_orig_args` is the number of
 * ingested arguments within `argv` to produce the resulting array of
 * items. If `fail_on_unknown_opt` is true, then on success
 * `ingested_orig_args` is equal to `argc`. Otherwise,
 * `ingested_orig_args` contains the number of original arguments until
 * an unknown _option_ occurs. For example, with
 *
 *     --great --white contact nuance --shark nuclear
 *
 * if `--shark` is not described within `descrs` and
 * `fail_on_unknown_opt` is false, then `ingested_orig_args` is 4 (two
 * options, two non-options), whereas `argc` is 6.
 *
 * This makes it possible to know where a command name is, for example.
 * With those arguments:
 *
 *     --verbose --stuff=23 do-something --specific-opt -f -b
 *
 * and the descriptors for `--verbose` and `--stuff` only, the function
 * returns the `--verbose` and `--stuff` option items, the
 * `do-something` non-option item, and that three original arguments
 * were ingested. This means you can start the next argument parsing
 * stage, with option descriptors depending on the command name, at
 * `&argv[3]`.
 *
 * Note that `ingested_orig_args` is not always equal to the number of
 * returned items, as
 *
 *     --hello -fdw
 *
 * for example contains two ingested original arguments, but four
 * resulting items.
 *
 * On failure, the returned structure's `items` member is `NULL`, and
 * the `error` string member contains details about the error.
 *
 * You can finalize the returned structure with
 * bt_argpar_parse_ret_fini().
 */
BT_HIDDEN
struct bt_argpar_parse_ret bt_argpar_parse(unsigned int argc,
		const char * const *argv,
		const struct bt_argpar_opt_descr *descrs,
		bool fail_on_unknown_opt);

/*
 * Finalizes what is returned by bt_argpar_parse().
 *
 * It is safe to call bt_argpar_parse() multiple times with the same
 * structure.
 */
BT_HIDDEN
void bt_argpar_parse_ret_fini(struct bt_argpar_parse_ret *ret);

#endif /* BABELTRACE_ARGPAR_H */
