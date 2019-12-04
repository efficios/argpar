AC_INIT([argpar], [1])
AM_INIT_AUTOMAKE([foreign])
LT_INIT

AC_CONFIG_MACRO_DIRS([m4])

# Depend on glib just for the tests.
PKG_CHECK_MODULES([GLIB], [glib-2.0])

AC_CONFIG_FILES([
	Makefile
	argpar/Makefile
	tests/Makefile
	tests/tap/Makefile
])

AC_OUTPUT
