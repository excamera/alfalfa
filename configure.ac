#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([alfalfa], [0.1], [alfalfa@mit.edu])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([src/decoder/prediction.cc])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_RANLIB

# Add picky CXXFLAGS
CPPFLAGS="-std=c++11"
PICKY_CXXFLAGS="-pedantic -Wall -Wextra -Weffc++ -Werror -Wno-unused-function"
AC_SUBST([PICKY_CXXFLAGS])
AC_LANG_PUSH(C++)

# Checks for libraries.
PKG_CHECK_MODULES([X11_XCB], [x11-xcb])
PKG_CHECK_MODULES([GLU], [glu])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile src/Makefile src/decoder/Makefile src/frontend/Makefile])
AC_OUTPUT
