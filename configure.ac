#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([alfalfa], [0.1], [alfalfa@mit.edu])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([dx2/modemv.hh])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_RANLIB

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

# Add picky CXXFLAGS
PICKY_CXXFLAGS="-std=c++11 -pedantic -Wall -Wextra -Weffc++ -fno-default-inline -pipe -pthread -Wno-unused-function -Werror"
AC_SUBST([PICKY_CXXFLAGS])

AC_CONFIG_FILES([Makefile src/Makefile dx2/Makefile dx3/Makefile])
AC_OUTPUT
