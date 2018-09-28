#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([libdft64],[0.1],[])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_TESTDIR([tests])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign -Wall -Werror -Wno-portability])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Checks for programs.
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_LN_S
AC_PROG_MKDIR_P
AC_PROG_SED

LT_INIT([disable-static pic-only])
AC_PROG_LIBTOOL

# Checks for OS.

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_SIZEOF([void *])
AC_CHECK_SIZEOF([int])
AC_CHECK_SIZEOF([long])

# Checks for library functions.

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	tools/Makefile
    tests/Makefile
    tests/atlocal
    tests/error/Makefile
])
AC_OUTPUT
