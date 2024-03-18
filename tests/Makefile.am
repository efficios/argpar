# SPDX-License-Identifier: GPL-2.0-only
# SPDX-FileCopyrightText: EfficiOS Inc.

SUBDIRS = tap

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/tests/tap \
	$(GLIB_CFLAGS)

noinst_PROGRAMS = test-argpar
test_argpar_SOURCES = test-argpar.c
test_argpar_LDADD = \
	$(top_builddir)/tests/tap/libtap.la \
	$(top_builddir)/argpar/libargpar.la \
	$(GLIB_LIBS)

TESTS = test-argpar
