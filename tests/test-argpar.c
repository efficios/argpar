/*
 * Copyright (c) 2019-2021 Philippe Proulx <pproulx@efficios.com>
 * Copyright (c) 2020-2021 Simon Marchi <simon.marchi@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <glib.h>

#include "tap/tap.h"
#include "argpar/argpar.h"

/*
 * Formats `item` and appends the resulting string to `res_str` to
 * incrementally build an expected command line string.
 *
 * This function:
 *
 * ‣ Prefers the `--long-opt=arg` style over the `-s arg` style.
 *
 * ‣ Uses the `arg<A,B>` form for non-option arguments, where `A` is the
 *   original argument index and `B` is the non-option argument index.
 */
static
void append_to_res_str(GString * const res_str,
		const argpar_item_t * const item)
{
	if (res_str->len > 0) {
		g_string_append_c(res_str, ' ');
	}

	switch (argpar_item_type(item)) {
	case ARGPAR_ITEM_TYPE_OPT:
	{
		const argpar_opt_descr_t * const descr =
			argpar_item_opt_descr(item);
		const char * const arg = argpar_item_opt_arg(item);

		if (descr->long_name) {
			g_string_append_printf(res_str, "--%s",
				descr->long_name);

			if (arg) {
				g_string_append_printf(res_str, "=%s", arg);
			}
		} else if (descr->short_name) {
			g_string_append_printf(res_str, "-%c",
				descr->short_name);

			if (arg) {
				g_string_append_printf(res_str, " %s", arg);
			}
		}

		break;
	}
	case ARGPAR_ITEM_TYPE_NON_OPT:
	{
		const char * const arg = argpar_item_non_opt_arg(item);
		const unsigned int orig_index =
			argpar_item_non_opt_orig_index(item);
		const unsigned int non_opt_index =
			argpar_item_non_opt_non_opt_index(item);

		g_string_append_printf(res_str, "%s<%u,%u>", arg, orig_index,
			non_opt_index);
		break;
	}
	default:
		abort();
	}
}

/*
 * Parses `cmdline` with the argpar API using the option descriptors
 * `descrs`, and ensures that the resulting effective command line is
 * `expected_cmd_line` and that the number of ingested original
 * arguments is `expected_ingested_orig_args`.
 *
 * This function splits `cmdline` on spaces to create an original
 * argument array.
 *
 * This function builds the resulting command line from parsing items
 * by space-separating each formatted item (see append_to_res_str()).
 */
static
void test_succeed(const char * const cmdline,
		const char * const expected_cmd_line,
		const argpar_opt_descr_t * const descrs,
		const unsigned int expected_ingested_orig_args)
{
	argpar_iter_t *iter = NULL;
	const argpar_item_t *item = NULL;
	const argpar_error_t *error = NULL;
	GString * const res_str = g_string_new(NULL);
	gchar ** const argv = g_strsplit(cmdline, " ", 0);
	unsigned int i, actual_ingested_orig_args;

	assert(argv);
	assert(res_str);
	iter = argpar_iter_create(g_strv_length(argv),
		(const char * const *) argv, descrs);
	assert(iter);

	for (i = 0; ; i++) {
		argpar_iter_next_status_t status;

		ARGPAR_ITEM_DESTROY_AND_RESET(item);
		status = argpar_iter_next(iter, &item, &error);

		ok(status == ARGPAR_ITER_NEXT_STATUS_OK ||
			status == ARGPAR_ITER_NEXT_STATUS_END,
			"argpar_iter_next() returns the expected status "
			"(%d) for command line `%s` (call %u)",
			status, cmdline, i + 1);
		ok(!error,
			"argpar_iter_next() doesn't set an error for "
			"command line `%s` (call %u)",
			cmdline, i + 1);

		if (status == ARGPAR_ITER_NEXT_STATUS_END) {
			ok(!item,
				"argpar_iter_next() doesn't set an item "
				"for status `ARGPAR_ITER_NEXT_STATUS_END` "
				"and command line `%s` (call %u)",
				cmdline, i + 1);
			break;
		}

		append_to_res_str(res_str, item);
	}

	actual_ingested_orig_args = argpar_iter_ingested_orig_args(iter);
	ok(actual_ingested_orig_args == expected_ingested_orig_args,
		"argpar_iter_ingested_orig_args() returns the expected "
		"number of ingested original arguments for command line `%s`",
		cmdline);

	if (actual_ingested_orig_args != expected_ingested_orig_args) {
		diag("Expected: %u    Got: %u", expected_ingested_orig_args,
			actual_ingested_orig_args);
	}

	ok(strcmp(expected_cmd_line, res_str->str) == 0,
		"argpar_iter_next() returns the expected parsing items "
		"for command line `%s`", cmdline);

	if (strcmp(expected_cmd_line, res_str->str) != 0) {
		diag("Expected: `%s`", expected_cmd_line);
		diag("Got:      `%s`", res_str->str);
	}

	argpar_item_destroy(item);
	argpar_iter_destroy(iter);
	assert(!error);
	g_string_free(res_str, TRUE);
	g_strfreev(argv);
}

static
void succeed_tests(void)
{
	/* No arguments */
	{
		const argpar_opt_descr_t descrs[] = {
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"",
			"",
			descrs, 0);
	}

	/* Single long option */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, '\0', "salut", false },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--salut",
			"--salut",
			descrs, 1);
	}

	/* Single short option */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'f', NULL, false },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-f",
			"-f",
			descrs, 1);
	}

	/* Short and long option (aliases) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'f', "flaw", false },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-f --flaw",
			"--flaw --flaw",
			descrs, 2);
	}

	/* Long option with argument (space form) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, '\0', "tooth", true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--tooth 67",
			"--tooth=67",
			descrs, 2);
	}

	/* Long option with argument (equal form) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, '\0', "polish", true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--polish=brick",
			"--polish=brick",
			descrs, 1);
	}

	/* Short option with argument (space form) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'c', NULL, true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-c chilly",
			"-c chilly",
			descrs, 2);
	}

	/* Short option with argument (glued form) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'c', NULL, true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-cchilly",
			"-c chilly",
			descrs, 1);
	}

	/* Short and long option (aliases) with argument (all forms) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'd', "dry", true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--dry=rate -dthing --dry street --dry=shape",
			"--dry=rate --dry=thing --dry=street --dry=shape",
			descrs, 5);
	}

	/* Many short options, last one with argument (glued form) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'd', NULL, false },
			{ 0, 'e', NULL, false },
			{ 0, 'f', NULL, true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-defmeow",
			"-d -e -f meow",
			descrs, 1);
	}

	/* Many options */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'd', NULL, false },
			{ 0, 'e', "east", true },
			{ 0, '\0', "mind", false },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-d --mind -destart --mind --east cough -d --east=itch",
			"-d --mind -d --east=start --mind --east=cough -d --east=itch",
			descrs, 8);
	}

	/* Single non-option argument */
	{
		const argpar_opt_descr_t descrs[] = {
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"kilojoule",
			"kilojoule<0,0>",
			descrs, 1);
	}

	/* Two non-option arguments */
	{
		const argpar_opt_descr_t descrs[] = {
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"kilojoule mitaine",
			"kilojoule<0,0> mitaine<1,1>",
			descrs, 2);
	}

	/* Single non-option argument mixed with options */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'd', NULL, false },
			{ 0, '\0', "squeeze", true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-d sprout yes --squeeze little bag -d",
			"-d sprout<1,0> yes<2,1> --squeeze=little bag<5,2> -d",
			descrs, 7);
	}

	/* Valid `---opt` */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, '\0', "-fuel", true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"---fuel=three",
			"---fuel=three",
			descrs, 1);
	}

	/* Long option containing `=` in argument (equal form) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, '\0', "zebra", true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--zebra=three=yes",
			"--zebra=three=yes",
			descrs, 1);
	}

	/* Short option's argument starting with `-` (glued form) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'z', NULL, true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-z-will",
			"-z -will",
			descrs, 1);
	}

	/* Short option's argument starting with `-` (space form) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'z', NULL, true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-z -will",
			"-z -will",
			descrs, 2);
	}

	/* Long option's argument starting with `-` (space form) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, '\0', "janine", true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--janine -sutto",
			"--janine=-sutto",
			descrs, 2);
	}

	/* Long option's argument starting with `-` (equal form) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, '\0', "janine", true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"--janine=-sutto",
			"--janine=-sutto",
			descrs, 1);
	}

	/* Long option's empty argument (equal form) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'f', NULL, false },
			{ 0, '\0', "yeah", true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-f --yeah= -f",
			"-f --yeah= -f",
			descrs, 3);
	}

	/* `-` non-option argument */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'f', NULL, false },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-f - -f",
			"-f -<1,0> -f",
			descrs, 3);
	}

	/* `--` non-option argument */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'f', NULL, false },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_succeed(
			"-f -- -f",
			"-f --<1,0> -f",
			descrs, 3);
	}

	/* Very long name of long option */
	{
		const char opt_name[] =
			"kale-chips-waistcoat-yr-bicycle-rights-gochujang-"
			"woke-tumeric-flexitarian-biodiesel-chillwave-cliche-"
			"ethical-cardigan-listicle-pok-pok-sustainable-live-"
			"edge-jianbing-gochujang-butcher-disrupt-tattooed-"
			"tumeric-prism-photo-booth-vape-kogi-jean-shorts-"
			"blog-williamsburg-fingerstache-palo-santo-artisan-"
			"affogato-occupy-skateboard-adaptogen-neutra-celiac-"
			"put-a-bird-on-it-kombucha-everyday-carry-hot-chicken-"
			"craft-beer-subway-tile-tote-bag-disrupt-selvage-"
			"raclette-art-party-readymade-paleo-heirloom-trust-"
			"fund-small-batch-kinfolk-woke-cardigan-prism-"
			"chambray-la-croix-hashtag-unicorn-edison-bulb-tbh-"
			"cornhole-cliche-tattooed-green-juice-adaptogen-"
			"kitsch-lo-fi-vexillologist-migas-gentrify-"
			"viral-raw-denim";
		const argpar_opt_descr_t descrs[] = {
			{ 0, '\0', opt_name, true },
			ARGPAR_OPT_DESCR_SENTINEL
		};
		char cmdline[1024];

		sprintf(cmdline, "--%s=23", opt_name);
		test_succeed(cmdline, cmdline, descrs, 1);
	}
}

/*
 * Parses `cmdline` with the argpar API using the option descriptors
 * `descrs`, and ensures that argpar_iter_next() fails with status
 * `expected_status` and that it sets an error having:
 *
 * ‣ The original argument index `expected_orig_index`.
 *
 * ‣ If applicable:
 *
 *   • The unknown option name `expected_unknown_opt_name`.
 *
 *   • The option descriptor at index `expected_opt_descr_index` of
 *     `descrs`.
 *
 *   • The option type `expected_is_short`.
 *
 * This function splits `cmdline` on spaces to create an original
 * argument array.
 */
static
void test_fail(const char * const cmdline,
		const argpar_error_type_t expected_error_type,
		const unsigned int expected_orig_index,
		const char * const expected_unknown_opt_name,
		const unsigned int expected_opt_descr_index,
		const bool expected_is_short,
		const argpar_opt_descr_t * const descrs)
{
	argpar_iter_t *iter = NULL;
	const argpar_item_t *item = NULL;
	gchar ** const argv = g_strsplit(cmdline, " ", 0);
	unsigned int i;
	const argpar_error_t *error = NULL;

	iter = argpar_iter_create(g_strv_length(argv),
		(const char * const *) argv, descrs);
	assert(iter);

	for (i = 0; ; i++) {
		argpar_iter_next_status_t status;

		ARGPAR_ITEM_DESTROY_AND_RESET(item);
		status = argpar_iter_next(iter, &item, &error);
		ok(status == ARGPAR_ITER_NEXT_STATUS_OK ||
			(status == ARGPAR_ITER_NEXT_STATUS_ERROR &&
				argpar_error_type(error) == expected_error_type),
			"argpar_iter_next() returns the expected status "
			"and error type (%d) for command line `%s` (call %u)",
			expected_error_type, cmdline, i + 1);

		if (status != ARGPAR_ITER_NEXT_STATUS_OK) {
			ok(!item,
				"argpar_iter_next() doesn't set an item "
				"for other status than "
				"`ARGPAR_ITER_NEXT_STATUS_OK` "
				"and command line `%s` (call %u)",
				cmdline, i + 1);
			ok(error,
				"argpar_iter_next() sets an error for "
				"other status than "
				" `ARGPAR_ITER_NEXT_STATUS_OK` "
				"and command line `%s` (call %u)",
				cmdline, i + 1);
			ok(argpar_error_orig_index(error) ==
				expected_orig_index,
				"argpar_iter_next() sets an error with "
				"the expected original argument index "
				"for command line `%s` (call %u)",
				cmdline, i + 1);

			if (argpar_error_type(error) == ARGPAR_ERROR_TYPE_UNKNOWN_OPT) {
				ok(strcmp(argpar_error_unknown_opt_name(error),
					expected_unknown_opt_name) == 0,
					"argpar_iter_next() sets an error with "
					"the expected unknown option name "
					"for command line `%s` (call %u)",
					cmdline, i + 1);
			} else {
				bool is_short;

				ok(argpar_error_opt_descr(error, &is_short) ==
					&descrs[expected_opt_descr_index],
					"argpar_iter_next() sets an error with "
					"the expected option descriptor "
					"for command line `%s` (call %u)",
					cmdline, i + 1);
				ok(is_short == expected_is_short,
					"argpar_iter_next() sets an error with "
					"the expected option type "
					"for command line `%s` (call %u)",
					cmdline, i + 1);
			}
			break;
		}

		ok(item,
			"argpar_iter_next() sets an item for status "
			"`ARGPAR_ITER_NEXT_STATUS_OK` "
			"and command line `%s` (call %u)",
			cmdline, i + 1);
		ok(!error,
			"argpar_iter_next() doesn't set an error for status "
			"`ARGPAR_ITER_NEXT_STATUS_OK` "
			"and command line `%s` (call %u)",
			cmdline, i + 1);
	}

	/*
	ok(strcmp(expected_error, error) == 0,
		"argpar_iter_next() sets the expected error string "
		"for command line `%s`", cmdline);

	if (strcmp(expected_error, error) != 0) {
		diag("Expected: `%s`", expected_error);
		diag("Got:      `%s`", error);
	}
	*/

	argpar_item_destroy(item);
	argpar_iter_destroy(iter);
	argpar_error_destroy(error);
	g_strfreev(argv);
}

static
void fail_tests(void)
{

	/* Unknown short option (space form) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'd', NULL, true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"-d salut -e -d meow",
			ARGPAR_ERROR_TYPE_UNKNOWN_OPT,
			2, "-e", 0, false,
			descrs);
	}

	/* Unknown short option (glued form) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'd', 0, true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"-dsalut -e -d meow",
			ARGPAR_ERROR_TYPE_UNKNOWN_OPT,
			1, "-e", 0, false,
			descrs);
	}

	/* Unknown long option (space form) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, '\0', "sink", true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"--sink party --food --sink impulse",
			ARGPAR_ERROR_TYPE_UNKNOWN_OPT,
			2, "--food", 0, false,
			descrs);
	}

	/* Unknown long option (equal form) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, '\0', "sink", true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"--sink=party --food --sink=impulse",
			ARGPAR_ERROR_TYPE_UNKNOWN_OPT,
			1, "--food", 0, false,
			descrs);
	}

	/* Unknown option before non-option argument */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, '\0', "thumb", true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"--thumb=party --food=18 bateau --thumb waves",
			ARGPAR_ERROR_TYPE_UNKNOWN_OPT,
			1, "--food", 0, false,
			descrs);
	}

	/* Unknown option after non-option argument */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, '\0', "thumb", true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"--thumb=party wound --food --thumb waves",
			ARGPAR_ERROR_TYPE_UNKNOWN_OPT,
			2, "--food", 0, false,
			descrs);
	}

	/* Missing long option argument */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, '\0', "thumb", true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"allo --thumb",
			ARGPAR_ERROR_TYPE_MISSING_OPT_ARG,
			1, NULL, 0, false,
			descrs);
	}

	/* Missing short option argument */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'k', NULL, true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"zoom heille -k",
			ARGPAR_ERROR_TYPE_MISSING_OPT_ARG,
			2, NULL, 0, true,
			descrs);
	}

	/* Missing short option argument (multiple glued) */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'a', NULL, false },
			{ 0, 'b', NULL, false },
			{ 0, 'c', NULL, true },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"-abc",
			ARGPAR_ERROR_TYPE_MISSING_OPT_ARG,
			0, NULL, 2, true,
			descrs);
	}

	/* Unexpected long option argument */
	{
		const argpar_opt_descr_t descrs[] = {
			{ 0, 'c', "chevre", false },
			ARGPAR_OPT_DESCR_SENTINEL
		};

		test_fail(
			"ambulance --chevre=fromage tar -cjv",
			ARGPAR_ERROR_TYPE_UNEXPECTED_OPT_ARG,
			1, NULL, 0, false,
			descrs);
	}
}

int main(void)
{
	plan_tests(309);
	succeed_tests();
	fail_tests();
	return exit_status();
}
