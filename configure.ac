#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([alfalfa], [0.1], [alfalfa@mit.edu])
AM_INIT_AUTOMAKE([foreign tar-ustar])
AC_CONFIG_SRCDIR([src/decoder/prediction.cc])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

# Checks for programs.
AC_PROG_CXX
dnl AC_PROG_RANLIB
LT_INIT
AC_PATH_PROG([PERL], [perl], [])
AS_IF([test x"$PERL" = x],
  [AC_MSG_ERROR([cannot find perl])])

# Add picky CXXFLAGS
CXX11_FLAGS="-std=c++11 -pthread"
PICKY_CXXFLAGS="-pedantic -Wall -Wextra -Weffc++ -Werror"
AC_SUBST([CXX11_FLAGS])
AC_SUBST([PICKY_CXXFLAGS])
AC_LANG_PUSH(C++)

AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug@<:@=no/asserts/sanitize@:>@],
     [Turn on asserts or santizers])],
  [case "$enableval" in
     no)
       NODEBUG_CXXFLAGS="-DNDEBUG"
       ;;
     '' | yes | sanitize)
       NODEBUG_CXXFLAGS="-fsanitize=address -fsanitize=undefined"
       ;;
     asserts)
       NODEBUG_CXXFLAGS=""
       ;;
     *)
       AC_MSG_ERROR([Unknown argument '$enableval' to --enable-debug])
       ;;
   esac],
  [NODEBUG_CXXFLAGS="-DNDEBUG"])

AC_SUBST(NODEBUG_CXXFLAGS)

AC_ARG_ENABLE([all-static],
  [AS_HELP_STRING([--enable-all-static], [Build statically linked binaries])],
  [case "$enableval" in
    '' | yes)
        STATIC_BUILD_FLAG="-all-static"
        ;;
    *)
        AC_MSG_ERROR([Unknown argument '$enableval' to --enable-all-static])
        ;;
   esac],
  [STATIC_BUILD_FLAG=""])

AC_SUBST(STATIC_BUILD_FLAG)

# Checks for assemblers
# TODO only check this when arch is x86
AC_CHECK_PROGS([AS], [yasm nasm], [none])
AS_IF([test x$AS == xnone], [
  AC_MSG_ERROR(yasm or nasm required for assembly; ASM functions are disabled. Install one of those to use SSE2 in the decoder.)
], [
  ASFLAGS="-f elf64"

  AC_DEFINE([HAVE_SSE2], [1], [CPU supports SSE2])
  AC_DEFINE([ARCH_X86_64], [1], [64bit CPU])
])

AC_SUBST([AS])
AC_SUBST([ASFLAGS])

# Checks for libraries.
PKG_CHECK_MODULES([GL], [gl])
PKG_CHECK_MODULES([GLU], [glu])
PKG_CHECK_MODULES([X264], [x264])
PKG_CHECK_MODULES([GLFW3], [glfw3])
PKG_CHECK_MODULES([GLEW], [glew])
PKG_CHECK_MODULES([ZLIB], [zlib])

# Checks for header files.
AC_PATH_X
AC_CHECK_HEADERS([fcntl.h unistd.h], [], [AC_MSG_ERROR([Missing required header file])])

AC_CHECK_HEADERS([boost/functional/hash.hpp], [], [AC_MSG_ERROR[Missing boost hash]])

AC_CHECK_HEADERS([boost/graph/adjacency_list.hpp boost/graph/graph_traits.hpp], [], [AC_MSG_ERROR[Missing boost graph]])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MMAP

# Checks for Apache and Apache modules

AC_LANG_POP(C++)

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/util/Makefile
                 src/decoder/Makefile
                 src/display/Makefile
                 src/encoder/Makefile
                 src/frontend/Makefile
                 src/tests/Makefile
                 man/Makefile])

AC_OUTPUT
