SUBDIRS = tap

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/tests/tap \
	$(GLIB_CFLAGS)

noinst_PROGRAMS = test_argpar
test_argpar_SOURCES = test_argpar.c
test_argpar_LDADD = \
	$(top_builddir)/tests/tap/libtap.la \
	$(top_builddir)/argpar/libargpar.la \
	$(GLIB_LIBS)

TESTS = test_argpar
