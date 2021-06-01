AC_INIT([argpar], [0.1.0])
AM_INIT_AUTOMAKE([foreign])
LT_INIT

AC_CONFIG_MACRO_DIRS([m4])

AC_USE_SYSTEM_EXTENSIONS

# Depend on glib just for the tests.
PKG_CHECK_MODULES([GLIB], [glib-2.0])

# When given, add -Werror to WARN_CFLAGS and WARN_CXXFLAGS.
# Disabled by default
AE_FEATURE_DEFAULT_DISABLE
AE_FEATURE([Werror],[Treat compiler warnings as errors.])

# Detect warning flags supported by the C compiler and append them to
# WARN_CFLAGS.
#
m4_define([WARN_FLAGS_LIST], [ dnl
  -Wall dnl
  -Wextra dnl
  -Wmissing-prototypes dnl
  -Wmissing-declarations dnl
  -Wnull-dereference dnl
  -Wundef dnl
  -Wredundant-decls dnl
  -Wshadow dnl
  -Wjump-misses-init dnl
  -Wsuggest-attribute=format dnl
  -Wtautological-constant-out-of-range-compare dnl
  -Wnested-externs dnl
  -Wwrite-strings dnl
  -Wformat=2 dnl
  -Wstrict-aliasing dnl
  -Wmissing-noreturn dnl
  -Winit-self dnl
  -Wduplicated-cond dnl
  -Wduplicated-branches dnl
  -Wlogical-op dnl
])

AX_APPEND_COMPILE_FLAGS([WARN_FLAGS_LIST], [WARN_CFLAGS], [-Werror])
AE_IF_FEATURE_ENABLED([Werror], [WARN_CFLAGS="${WARN_CFLAGS} -Werror"])
AM_CFLAGS="${AM_CFLAGS} ${WARN_CFLAGS}"

AC_SUBST(AM_CFLAGS)

AC_CONFIG_FILES([
	Doxyfile
	Makefile
	argpar/Makefile
	tests/Makefile
	tests/tap/Makefile
])

AC_OUTPUT
