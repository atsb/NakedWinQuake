dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
AC_INIT([NakedWinQuake],[1.0.9],[])
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([README])
AC_CONFIG_HEADERS([config.h])

dnl Detect the canonical target build environment
dnl AC_CANONICAL_TARGET
AC_CANONICAL_HOST

dnl Setup for automake
AM_INIT_AUTOMAKE([tar-pax subdir-objects])

dnl Check for programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PROG_AS
PKG_PROG_PKG_CONFIG

# Checks for libraries.

# Checks for header files.
AC_PATH_X
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stddef.h stdlib.h string.h sys/file.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([atexit floor gethostbyaddr gethostbyname gethostname gethrtime getpagesize gettimeofday inet_ntoa memset mkdir munmap pow select socket sqrt strcasecmp strchr strerror strstr])

#AS_IF([test "$GCC" = "yes"], [CFLAGS="-g -O6"], [])

dnl The alpha architecture needs special flags for binary portability
AS_CASE(
	[$host],
	[alpha*-*-linux*],[CFLAGS="$CFLAGS -mcpu=ev4 -Wa,-mall"],
	[x86_64-*-linux*],[CFLAGS="$CFLAGS -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations"],
	[x86_64-*-mingw32*],[CFLAGS="$CFLAGS -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations"],
	[*-apple-*],[CFLAGS="$CFLAGS -ffast-math -funroll-loops -fomit-frame-pointer"],
	[*-linux*],[CFLAGS="$CFLAGS -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations"],
	AC_MSG_ERROR(["Unrecognized platform $host."]))

dnl Figure out which math and networking libraries to use
AS_CASE(
	[$host],
    	[*-*-mingw32*],[MATHLIB=''; INETLIB='-lwsock32'],
	[*-*-beos*],[MATHLIB=''; INETLIB=''],
	[MATHLIB='-lm'; INETLIB=''])
AC_SUBST(MATHLIB)
AC_SUBST(INETLIB)

dnl Check for SDL2
PKG_CHECK_MODULES([SDL2],
	[sdl2 >= 2.0.0],
	[CFLAGS="$CFLAGS $SDL2_CFLAGS"; CPPFLAGS="$CPPFLAGS -DSDL"; LIBS="$LIBS $SDL2_LIBS"],
	AC_MSG_ERROR([*** SDL2 version 2.0.0 not found!]))

dnl Set up the icon object file, for Mingw32
AS_CASE(
	[$host],
	[*-*-mingw32*],
	[cp ico_o ico.o; ICONOBJ="ico.o"],
	[ICONOBJ=""])
AC_SUBST(ICONOBJ)

CPPFLAGS="$CPPFLAGS"

# Finally create all the generated files
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
