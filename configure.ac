#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([alfalfa], [0.1], [excamera@cs.stanford.edu])
AM_INIT_AUTOMAKE([foreign tar-ustar])
AC_CONFIG_SRCDIR([src/decoder/prediction.cc])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CXX
dnl AC_PROG_RANLIB
LT_INIT
AC_PATH_PROG([PERL], [perl], [])
AS_IF([test x"$PERL" = x],
  [AC_MSG_ERROR([cannot find perl])])

# Add picky CXXFLAGS
CXX11_FLAGS="-std=c++14 -pthread"
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
       NODEBUG_CXXFLAGS="-fsanitize=address -fsanitize=undefined -fuse-ld=gold"
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
    no)
        STATIC_BUILD_FLAG=""
        ;;
    *)
        AC_MSG_ERROR([Unknown argument '$enableval' to --enable-all-static])
        ;;
   esac],
  [STATIC_BUILD_FLAG=""])

AC_SUBST(STATIC_BUILD_FLAG)

AC_ARG_ENABLE([vp8play],
  [AS_HELP_STRING([--enable-vp8play], [Build vp8play (requires gl, glu, glfw3, glew)])],
  [case "$enableval" in
    '' | yes)
        buildvp8=true
        ;;
    no)
        buildvp8=false
        ;;
    *)
        AC_MSG_ERROR([Unknown argument '$enableval' to --enable-vp8play])
        ;;
   esac],
  [buildvp8=true])

AM_CONDITIONAL([BUILDVP8PLAY], [test "$buildvp8" = true])

# Checks for assemblers
# TODO only check this when arch is x86
AC_CHECK_PROGS([AS], [yasm nasm], [none])
AS_IF([test x$AS == xnone], [
  AC_MSG_ERROR([yasm or nasm required for assembly; ASM functions are disabled. Install one of those to use SSE2 in the decoder.])
], [
  ASFLAGS="-f elf64"

  AC_DEFINE([HAVE_SSE2], [1], [CPU supports SSE2])
  AC_DEFINE([ARCH_X86_64], [1], [64bit CPU])
])

AC_SUBST([AS])
AC_SUBST([ASFLAGS])

# Checks for libraries.
PKG_CHECK_MODULES([X264], [x264])
PKG_CHECK_MODULES([ZLIB], [zlib])
AC_SEARCH_LIBS([jpeg_CreateDecompress], [jpeg], , [AC_MSG_ERROR([Unable to find libjpeg.])])

if test "$buildvp8" = true; then
    PKG_CHECK_MODULES([GL], [gl])
    PKG_CHECK_MODULES([GLU], [glu])
    PKG_CHECK_MODULES([GLFW3], [glfw3])
    PKG_CHECK_MODULES([GLEW], [glew])
fi

# Checks for header files.
AC_PATH_X
AC_CHECK_HEADERS([fcntl.h unistd.h], [], [AC_MSG_ERROR([Missing required header file])])

AC_CHECK_HEADERS([boost/functional/hash.hpp], [], [AC_MSG_ERROR([Missing boost hash])])

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
                 src/input/Makefile
                 src/encoder/Makefile
                 src/net/Makefile
                 src/frontend/Makefile
                 src/salsify/Makefile
                 src/tests/Makefile
                 man/Makefile])

AC_OUTPUT
