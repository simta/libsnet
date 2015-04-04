AC_INIT(libsnet, m4_esyscmd([sh version.sh]), simta@umich.edu) 
AM_INIT_AUTOMAKE([foreign])
AC_PREREQ(2.52)
AC_COPYRIGHT([Copyright (c) 1995-2015 Regents of The University of Michigan])
AC_CONFIG_HEADER(config.h)
AC_CONFIG_SRCDIR([snet.c])
AC_CONFIG_MACRO_DIR([m4])

# Canonicalise things
AC_CANONICAL_HOST

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

# Checks for functions
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS_ONCE([gettimeofday select])

# Checks for libraries.
AC_ARG_WITH([sasl], AC_HELP_STRING([--with-sasl], [Build with sasl]))
AS_IF(
    [test "x$with_sasl" = "xyes"], [
        PKG_CHECK_MODULES([LIBSASL], [libsasl2], [
            CONFIG_DEFINES="$CONFIG_DEFINES
#ifndef SNET_HAVE_LIBSASL
#define SNET_HAVE_LIBSASL
#endif"
            PKGCONF_REQUIRES="$PKGCONF_REQUIRES libsasl2"
])])

AC_ARG_WITH([ssl], AC_HELP_STRING([--with-ssl], [Build with TLS support]))
AS_IF(
    [test "x$with_ssl" != "xno" ], [
        PKG_CHECK_MODULES([LIBSSL], [openssl], [
            CONFIG_DEFINES="$CONFIG_DEFINES
#ifndef SNET_HAVE_LIBSSL
#define SNET_HAVE_LIBSSL
#endif"
            PKGCONF_REQUIRES="$PKGCONF_REQUIRES openssl"
])])

AC_ARG_WITH([zlib], AC_HELP_STRING([--with-zlib], [Build with zlib support]))
AS_IF(
    [test "x$with_zlib" = "xyes" ], [
        PKG_CHECK_MODULES([ZLIB], [zlib], [
            CONFIG_DEFINES="$CONFIG_DEFINES
#ifndef SNET_HAVE_ZLIB
#define SNET_HAVE_ZLIB
#endif"
            PKGCONF_REQUIRES="$PKGCONF_REQUIRES zlib"
])])

AC_SUBST([PKGCONF_REQUIRES])
AC_SUBST([CONFIG_DEFINES])
AM_SUBST_NOTMAKE([CONFIG_DEFINES])

# Write output
AC_CONFIG_FILES([Makefile snet.h packaging/rpm/libsnet.spec packaging/pkgconfig/libsnet.pc])
AC_OUTPUT
