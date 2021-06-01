/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2019-2021 Philippe Proulx <pproulx@efficios.com>
 * Copyright (c) 2020-2021 Simon Marchi <simon.marchi@efficios.com>
 */

#ifndef ARGPAR_ARGPAR_H
#define ARGPAR_ARGPAR_H

#include <stdbool.h>

/*!
@mainpage

See the \ref api module.

@addtogroup api argpar API
@{

argpar is a library which provides an iterator-based API to parse
command-line arguments.

The argpar parser supports:

<ul>
  <li>
    Short options without an argument, possibly tied together:

    @code{.unparsed}
    -f -auf -n
    @endcode

  <li>
    Short options with arguments:

    @code{.unparsed}
    -b 45 -f/mein/file -xyzhello
    @endcode

  <li>
    Long options without an argument:

    @code{.unparsed}
    --five-guys --burger-king --pizza-hut --subway
    @endcode

  <li>
    Long options with arguments (two original arguments or a single
    one with a <code>=</code> character):

    @code{.unparsed}
    --security enable --time=18.56
    @endcode

  <li>
    Non-option arguments (anything else, including
    <code>-</code> and <code>\--</code>).

    A non-option argument cannot have the form of an option, for example
    if you need to pass the exact relative path
    <code>\--component</code>. In that case, you would need to pass
    <code>./\--component</code>. There's no generic way to escape
    <code>-</code> as of this version.
</ul>

Create a parsing iterator with argpar_iter_create(), then repeatedly
call argpar_iter_next() to access the parsing results (items), until one
of:

- There are no more arguments.

- The argument parser encounters an error (for example, an unknown
  option).

- You need to stop.

argpar_iter_create() accepts duplicate option descriptors in
\p descrs (argpar_iter_next() produces one item for each
instance).

A parsing item (the result of argpar_iter_next()) has the type
#argpar_item.

Get the type (option or non-option) of an item with
argpar_item_type(). Each item type has its set of dedicated functions
(\c argpar_item_opt_ and \c argpar_item_non_opt_ prefixes).

argpar_iter_next() produces the items in the same order that it parses
original arguments, including non-option arguments. This means, for
example, that for:

@code{.unparsed}
--hello --count=23 /path/to/file -ab --type file -- magie
@endcode

argpar_iter_next() produces the following items, in this order:

-# Option item (<code>\--hello</code>).
-# Option item (<code>\--count</code> with argument <code>23</code>).
-# Non-option item (<code>/path/to/file</code>).
-# Option item (<code>-a</code>).
-# Option item (<code>-b</code>).
-# Option item (<code>\--type</code> with argument <code>file</code>).
-# Non-option item (<code>\--</code>).
-# Non-option item (<code>magie</code>).
*/

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

struct argpar_opt_descr;

/*!
@name Item API
@{
*/

/*!
@brief
    Type of a parsing item, as returned by argpar_item_type().
*/
enum argpar_item_type {
	/// Option
	ARGPAR_ITEM_TYPE_OPT,

	/// Non-option
	ARGPAR_ITEM_TYPE_NON_OPT,
};

/*!
@struct argpar_item

@brief
    Opaque parsing item type

argpar_iter_next() sets a pointer to such a type.
*/
struct argpar_item;

/*!
@brief
    Returns the type of the parsing item \p item.

@param[in] item
    Parsing item of which to get the type.

@returns
    Type of \p item.

@pre
    \p item is not \c NULL.
*/
/// @cond hidden_macro
ARGPAR_HIDDEN
/// @endcond
enum argpar_item_type argpar_item_type(const struct argpar_item *item);

/*!
@brief
    Returns the option descriptor of the option parsing item \p item.

@param[in] item
    Option parsing item of which to get the option descriptor.

@returns
    Option descriptor of \p item.

@pre
    \p item is not \c NULL.
@pre
    \p item has the type #ARGPAR_ITEM_TYPE_OPT.
*/
/// @cond hidden_macro
ARGPAR_HIDDEN
/// @endcond
const struct argpar_opt_descr *argpar_item_opt_descr(
		const struct argpar_item *item);

/*!
@brief
    Returns the argument of the option parsing item \p item, or
    \c NULL if none.

@param[in] item
    Option parsing item of which to get the argument.

@returns
    Argument of \p item, or \c NULL if none.

@pre
    \p item is not \c NULL.
@pre
    \p item has the type #ARGPAR_ITEM_TYPE_OPT.
*/
/// @cond hidden_macro
ARGPAR_HIDDEN
/// @endcond
const char *argpar_item_opt_arg(const struct argpar_item *item);

/*!
@brief
    Returns the complete original argument, pointing to one of the
    entries of the original arguments (\p argv as passed to
    argpar_iter_create()), of the non-option parsing item \p item.

@param[in] item
    Non-option parsing item of which to get the complete original
    argument.

@returns
    Complete original argument of \p item.

@pre
    \p item is not \c NULL.
@pre
    \p item has the type #ARGPAR_ITEM_TYPE_NON_OPT.
*/
/// @cond hidden_macro
ARGPAR_HIDDEN
/// @endcond
const char *argpar_item_non_opt_arg(const struct argpar_item *item);

/*!
@brief
    Returns the index, within \em all the original arguments (\p argv
    as passed to argpar_iter_create()), of the non-option parsing item
    \p item.

For example, with the following command line (all options have no
argument):

@code{.unparsed}
-f -m meow --jus mix --kilo
@endcode

The original argument index of \c meow is&nbsp;2 while the original
argument index of \c mix is&nbsp;4.

@param[in] item
    Non-option parsing item of which to get the original argument index.

@returns
    Original argument index of \p item.

@pre
    \p item is not \c NULL.
@pre
    \p item has the type #ARGPAR_ITEM_TYPE_NON_OPT.

@sa
    argpar_item_non_opt_non_opt_index() -- Returns the non-option index
    of a non-option parsing item.
*/
/// @cond hidden_macro
ARGPAR_HIDDEN
/// @endcond
unsigned int argpar_item_non_opt_orig_index(const struct argpar_item *item);

/*!
@brief
    Returns the index, within the parsed non-option parsing items, of
    the non-option parsing item \p item.

For example, with the following command line (all options have no
argument):

@code{.unparsed}
-f -m meow --jus mix --kilo
@endcode

The non-option index of \c meow is&nbsp;0 while the original
argument index of \c mix is&nbsp;1.

@param[in] item
    Non-option parsing item of which to get the non-option index.

@returns
    Non-option index of \p item.

@pre
    \p item is not \c NULL.
@pre
    \p item has the type #ARGPAR_ITEM_TYPE_NON_OPT.

@sa
    argpar_item_non_opt_orig_index() -- Returns the original argument
    index of a non-option parsing item.
*/
/// @cond hidden_macro
ARGPAR_HIDDEN
/// @endcond
unsigned int argpar_item_non_opt_non_opt_index(const struct argpar_item *item);

/*!
@brief
    Destroys the parsing item \p item.

@param[in] item
    Parsing item to destroy (may be \c NULL).
*/
/// @cond hidden_macro
ARGPAR_HIDDEN
/// @endcond
void argpar_item_destroy(const struct argpar_item *item);

/*!
@def ARGPAR_ITEM_DESTROY_AND_RESET(_item)

@brief
    Calls argpar_item_destroy() with \p _item, and then sets \p _item
    to \c NULL.

@param[in] _item
    Item to destroy and variable to reset
    (<code>const struct argpar_item *</code> type).
*/
#define ARGPAR_ITEM_DESTROY_AND_RESET(_item)				\
	{								\
		argpar_item_destroy(_item);				\
		_item = NULL;						\
	}

/// @}

/*!
@name Iterator API
@{
*/

/*!
@brief
    Option descriptor

argpar_iter_create() accepts an array of instances of such a type,
terminated with #ARGPAR_OPT_DESCR_SENTINEL, as its \p descrs parameter.

The typical usage is, for example:

@code
const struct argpar_opt_descr descrs[] = {
    { 0, 'd', NULL, false },
    { 1, '\0', "squeeze", true },
    { 2, 'm', "meow", true },
    ARGPAR_OPT_DESCR_SENTINEL,
};
@endcode
*/
struct argpar_opt_descr {
	/// Numeric ID, to uniquely identify this descriptor
	const int id;

	/// Short option character, or <code>'\0'</code>
	const char short_name;

	/// Long option name (without the <code>\--</code> prefix), or \c NULL
	const char * const long_name;

	/// \c true if this option has an argument
	const bool with_arg;
};

/*!
@brief
    Sentinel for an option descriptor array

The typical usage is, for example:

@code
const struct argpar_opt_descr descrs[] = {
    { 0, 'd', NULL, false },
    { 1, '\0', "squeeze", true },
    { 2, 'm', "meow", true },
    ARGPAR_OPT_DESCR_SENTINEL,
};
@endcode
*/
#define ARGPAR_OPT_DESCR_SENTINEL	{ -1, '\0', NULL, false }

/*!
@struct argpar_iter

@brief
    Opaque argpar iterator type

argpar_iter_create() returns a pointer to such a type.
*/
struct argpar_iter;

/*!
@brief
    Creates and returns an argument parsing iterator to parse the
    original arguments \p argv of which the count is \p argc using the
    option descriptors \p descrs.

This function initializes the returned structure, but doesn't actually
start parsing the arguments.

argpar considers \em all the elements of \p argv, including the first
one, so that you would typically pass <code>(argc - 1)</code> as \p argc
and <code>\&argv[1]</code> as \p argv from what <code>main()</code>
receives, or ignore the parsing item of the first call to
argpar_iter_next().

\p *argv and \p *descrs must \em not change for the lifetime of the
returned iterator (until you call argpar_iter_destroy()) and for the
lifetime of any parsing item (until you call argpar_item_destroy())
which argpar_iter_next() creates from the returned iterator.

@param[in] argc
    Number of original arguments to parse in \p argv.
@param[in] argv
    Original arguments to parse, of which the count is \p argc.
@param[in] descrs
    @parblock
    Option descriptor array, terminated with #ARGPAR_OPT_DESCR_SENTINEL.

    May contain duplicate entries.
    @endparblock

@returns
    New argument parsing iterator, or \c NULL on memory error.

@pre
    \p argc is greater than 0.
@pre
    \p argv is not \c NULL.
@pre
    The first \p argc elements of \p argv are not \c NULL.
@pre
    \p descrs is not \c NULL.

@sa
    argpar_iter_destroy() -- Destroys an argument parsing iterator.
*/
/// @cond hidden_macro
ARGPAR_HIDDEN
/// @endcond
struct argpar_iter *argpar_iter_create(unsigned int argc,
		const char * const *argv,
		const struct argpar_opt_descr *descrs);

/*!
@brief
    Destroys the argument parsing iterator \p iter.

@param[in] iter
    Argument parsing iterator to destroy (may be \c NULL).

@sa
    argpar_iter_create() -- Creates an argument parsing iterator.
*/
/// @cond hidden_macro
ARGPAR_HIDDEN
/// @endcond
void argpar_iter_destroy(struct argpar_iter *iter);

/*!
@brief
    Return type of argpar_iter_next().
*/
enum argpar_iter_next_status {
	/// Success
	ARGPAR_ITER_NEXT_STATUS_OK,

	/// End of iteration (no more original arguments to parse)
	ARGPAR_ITER_NEXT_STATUS_END,

	/// Unknown option error
	ARGPAR_ITER_NEXT_STATUS_ERROR_UNKNOWN_OPT,

	/// Missing option argument error
	ARGPAR_ITER_NEXT_STATUS_ERROR_MISSING_OPT_ARG,

	/// Unexpected option argument error
	ARGPAR_ITER_NEXT_STATUS_ERROR_UNEXPECTED_OPT_ARG,

	/// Memory error
	ARGPAR_ITER_NEXT_STATUS_ERROR_MEMORY,
};

/*!
@brief
    Sets \p *item to the next item of the argument parsing iterator
    \p iter and advances \p iter.

If there are no more original arguments to parse, this function returns
#ARGPAR_ITER_NEXT_STATUS_END.

@param[in] iter
    Argument parsing iterator from which to get the next parsing item.
@param[out] item
    @parblock
    On success, \p *item is the next parsing item of \p iter.

    Destroy \p *item with argpar_item_destroy().
    @endparblock
@param[out] error
    @parblock
    When this function returns
    #ARGPAR_ITER_NEXT_STATUS_ERROR_UNKNOWN_OPT,
    #ARGPAR_ITER_NEXT_STATUS_ERROR_MISSING_OPT_ARG, or
    #ARGPAR_ITER_NEXT_STATUS_ERROR_UNEXPECTED_OPG_ARG, if this parameter
    is not \c NULL,
    \p *error is a string which explains the parsing error in English.

    Free \p *error with <code>free()</code>.
    @endparblock

@returns
    Status code.

@pre
    \p iter is not \c NULL.
@pre
    \p item is not \c NULL.
*/
/// @cond hidden_macro
ARGPAR_HIDDEN
/// @endcond
enum argpar_iter_next_status argpar_iter_next(
		struct argpar_iter *iter, const struct argpar_item **item,
		char **error);

/*
 * Returns the number of ingested elements from `argv`, as passed to
 * argpar_iter_create() to create `*iter`, that were required to produce
 * the previously returned items.
 */

/*!
@brief
    Returns the number of ingested original arguments (in
    \p argv as passed to argpar_iter_create() to create \p iter) that
    the parser ingested to produce the \em previous parsing items.

@param[in] iter
    Argument parsing iterator of which to get the number of ingested
    original arguments.

@returns
    Number of original arguments which \p iter ingested.

@pre
    \p iter is not \c NULL.
*/
/// @cond hidden_macro
ARGPAR_HIDDEN
/// @endcond
unsigned int argpar_iter_ingested_orig_args(const struct argpar_iter *iter);

/// @}

/// @}

#endif /* ARGPAR_ARGPAR_H */
