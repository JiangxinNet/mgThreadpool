#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([mgThreadpool], [1.0], [magelive.jx@foxmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

# Checks for programs.
AC_PROG_CC

AM_INIT_AUTOMAKE
AC_PROG_LIBTOOL

# Checks for libraries.
AC_PROG_RANLIB

# Checks for header files.
AC_CHECK_HEADERS([stdio.h stdlib.h strings.h string.h stddef.h stdint.h sys/time.h time.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_FUNCS([])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
