ACLOCAL_AMFLAGS = -I m4
AM_LDFLAGS = @ZLIB_LIBS@ @LIBSSL_LIBS@ @LIBSASL_LIBS@
AM_CPPFLAGS = @ZLIB_CFLAGS@ @LIBSSL_CFLAGS@ @LIBSASL_CFLAGS@ -I.
pkgconfigdir = $(libdir)/pkgconfig

lib_LTLIBRARIES = libsnet.la
libsnet_la_SOURCES = snet.c snet.h
libsnet_la_LDFLAGS = -version-info 2:0:0

include_HEADERS = snet.h

nodist_pkgconfig_DATA = packaging/pkgconfig/libsnet.pc

EXTRA_DIST = packaging/rpm/libsnet.spec

rpm: dist-xz
	rpmbuild -ta $(distdir).tar.xz
