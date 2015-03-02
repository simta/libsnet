ACLOCAL_AMFLAGS = -I m4
AM_LDFLAGS = @LDFLAGS@ @ZLIB_LDFLAGS@ @LIBSSL_LDFLAGS@ @LIBSASL_LDFLAGS@
AM_CPPFLAGS = @CPPFLAGS@ @ZLIB_CPPFLAGS@ @LIBSSL_CPPFLAGS@ @LIBSASL_CPPFLAGS@ -I.

lib_LTLIBRARIES = libsnet.la
libsnet_la_SOURCES = snet.c snet.h
libsnet_la_LDFLAGS = -version-info 1:0:1

include_HEADERS = snet.h

EXTRA_DIST = packaging/rpm/libsnet.spec
