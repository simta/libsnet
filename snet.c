/*
 * Copyright (c) 1995,2001 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <netinet/in.h>


#ifdef __STDC__
#include <stdarg.h>
#else __STDC__
#include <varargs.h>
#endif __STDC__

#include "snet.h"

#define SNET_BUFLEN	1024

#define SNET_BOL	0
#define SNET_FUZZY	1
#define SNET_IN		2

#define SNET_EOF	(1<<0)
#define SNET_ENCRYPT	(1<<1)

static int snet_saslread ___P(( SNET *, char *, int, struct timeval * ));
static int snet_readread ___P(( SNET *, char *, int, struct timeval * ));

/*
 * This routine is necessary, since snet_getline() doesn't differentiate
 * between NULL => EOF and NULL => connection dropped (or some other error).
 */
    int
snet_eof( sn )
    SNET		*sn;
{
    return ( sn->sn_flag & SNET_EOF );
}

    SNET *
snet_attach( fd, max )
    int		fd;
    int		max;
{
    SNET		*sn;

    if (( sn = (SNET *)malloc( sizeof( SNET ))) == NULL ) {
	return( NULL );
    }
    sn->sn_fd = fd;
    if (( sn->sn_rbuf = (char *)malloc( SNET_BUFLEN )) == NULL ) {
	free( sn );
	return( NULL );
    }
    sn->sn_rbuflen = SNET_BUFLEN;
    sn->sn_rstate = SNET_BOL;
    sn->sn_rcur = sn->sn_rend = sn->sn_rbuf;
    sn->sn_maxlen = max;

    if (( sn->sn_wbuf = (char *)malloc( SNET_BUFLEN )) == NULL ) {
	free( sn->sn_rbuf );
	free( sn );
	return( NULL );
    }
    sn->sn_wbuflen = SNET_BUFLEN;

    sn->sn_flag = 0;

    sn->sn_encrypt = NULL;
    sn->sn_decrypt = NULL;
    sn->sn_crypto = NULL;
    sn->sn_ebuf = NULL;
    sn->sn_ebuflen = 0;
    sn->sn_dbuf = NULL;
    sn->sn_dcur = sn->sn_dend = NULL;
    sn->sn_dbuflen = 0;

    return( sn );
}

    SNET *
snet_open( path, flags, mode, max )
    char	*path;
    int		flags;
    int		mode;
{
    int		fd;

    if (( fd = open( path, flags, mode )) < 0 ) {
	return( NULL );
    }
    return( snet_attach( fd, max ));
}

    int
snet_close( sn )
    SNET		*sn;
{
    free( sn->sn_wbuf );
    free( sn->sn_rbuf );
    if ( sn->sn_crypto ) {
	free( sn->sn_ebuf );
	free( sn->sn_dbuf );
    }
    if ( close( sn->sn_fd ) < 0 ) {
	return( -1 );
    }
    free( sn );
    return( 0 );
}

/*
 * Just like fprintf, only use the SNET header to get the fd, and use
 * snet_write() to move the data.
 *
 * Todo: %f, *, . and, -
 */
    int
#ifdef __STDC__
snet_writef( SNET *sn, char *format, ... )
#else __STDC__
snet_writef( sn, format, va_alist )
    SNET			*sn;
    char		*format;
    va_dcl
#endif __STDC__
{
    va_list		vl;
    char		dbuf[ 128 ], *p, *dbufoff;
    int			d, len;
    char		*cur, *end;

#ifdef __STDC__
    va_start( vl, format );
#else __STDC__
    va_start( vl );
#endif __STDC__

#define SNET_WRITEFGROW(x)						\
	    while ( cur + (x) > end ) {					\
		if (( sn->sn_wbuf = (char *)realloc( sn->sn_wbuf,	\
			sn->sn_wbuflen + SNET_BUFLEN )) == NULL ) {	\
		    abort();						\
		}							\
		cur = sn->sn_wbuf + sn->sn_wbuflen - ( end - cur );	\
		sn->sn_wbuflen += SNET_BUFLEN;				\
		end = sn->sn_wbuf + sn->sn_wbuflen;			\
	    }		

    cur = sn->sn_wbuf;
    end = sn->sn_wbuf + sn->sn_wbuflen;

    for ( ; *format; format++ ) {
	dbufoff = dbuf + sizeof( dbuf );

	if ( *format != '%' ) {
	    SNET_WRITEFGROW( 1 );
	    *cur++ = *format;
	} else {
	    switch ( *++format ) {
	    case 's' :
		p = va_arg( vl, char * );
		len = strlen( p );
		SNET_WRITEFGROW( len );
		strcpy( cur, p );
		cur += strlen( p );
		break;

	    case 'c' :
		SNET_WRITEFGROW( 1 );
		*cur++ = va_arg( vl, char );
		break;

	    case 'd' :
		d = va_arg( vl, int );
		p = dbufoff;
		do {
		    if ( --dbufoff < dbuf ) {
			abort();
		    }
		    *dbufoff = '0' + ( d % 10 );
		    d /= 10;
		} while ( d );
		len = p - dbufoff;
		SNET_WRITEFGROW( len );
		strncpy( cur, dbufoff, len );
		cur += len;
		break;

	    case 'o' :
		d = va_arg( vl, int );
		p = dbufoff;
		do {
		    if ( --dbufoff < dbuf ) {
			abort();
		    }
		    *dbufoff = '0' + ( d & 0007 );
		    d = d >> 3;
		} while ( d );
		len = p - dbufoff;
		SNET_WRITEFGROW( len );
		strncpy( cur, dbufoff, len );
		cur += len;
		break;

	    case 'x' :
		d = va_arg( vl, int );
		p = dbufoff;
		do {
		    char	hexalpha[] = "0123456789abcdef";

		    if ( --dbufoff < dbuf ) {
			abort();
		    }
		    *dbufoff = hexalpha[ d & 0x0f ];
		    d = d >> 4;
		} while ( d );
		SNET_WRITEFGROW( len );
		strncpy( cur, dbufoff, len );
		cur += len;
		break;

	    default :
		SNET_WRITEFGROW( 2 );
		*cur++ = '%';
		*cur++ = *format;
		break;
	    }
	}
    }

    va_end( vl );

    return( snet_write( sn, sn->sn_wbuf, cur - sn->sn_wbuf, 0 ));
}

/*
 * Enable SASL.  Should we check that esize and dsize are > 0?
 */
    int
snet_sasl( sn, crypto, encrypt, decrypt, esize, dsize )
    SNET		*sn;
    void		*crypto;
    int			(*encrypt)( void *, char *, int );
    int			(*decrypt)( void *, char *, int );
    unsigned int	esize, dsize;
{
    sn->sn_crypto = crypto;
    sn->sn_encrypt = encrypt;
    sn->sn_decrypt = decrypt;

    if (( sn->sn_ebuf = (char *)malloc( esize + 4 )) == NULL ) {
	return( -1 );
    }
    sn->sn_ebuflen = esize;

    if (( sn->sn_dbuf = (char *)malloc( dsize + 4 )) == NULL ) {
	free( sn->sn_ebuf );
	return( -1 );
    }
    sn->sn_dbuflen = dsize;
    sn->sn_dend = sn->sn_dbuf;
    sn->sn_dcur = NULL;

    return( 0 );
}

/*
 * Should we set non-blocking IO?  Do we need to bother?
 * We'll leave tv in here now, so that we don't have to change the call
 * later.  It's currently ignored.
 */
    int
snet_write( sn, buf, len, tv )
    SNET		*sn;
    char		*buf;
    int			len;
    struct timeval	*tv;
{

    if ( sn->sn_crypto == NULL ) {
	return( write( snet_fd( sn ), buf, len ));
    }

    abort();
}

    static int
snet_saslread( sn, buf, len, tv )
    SNET		*sn;
    char		*buf;
    int			len;
    struct timeval	*tv;
{
    uint32_t		maxrbuf;
    int			rc;
    extern int		errno;

    if ( sn->sn_crypto == NULL ) {
	return( snet_readread( sn, buf, len, tv ));
    }

    /*
     * we already have decrypted data
     */
    if ( sn->sn_dcur != NULL ) {
	maxrbuf = ntohl( *(uint32_t *)sn->sn_dbuf );
	goto gotsum;
    }

    /* if we already have encrypted data (or no data), read some more */
    for (;;) {
	if ( sn->sn_dend - sn->sn_dbuf < 4 ) {
	    maxrbuf = sn->sn_dbuflen + 4;
	    maxrbuf -= ( sn->sn_dend - sn->sn_dbuf );
	} else {
	    maxrbuf = ntohl( *(uint32_t *)sn->sn_dbuf );
	    if ( maxrbuf > sn->sn_dbuflen ) {
		errno = EPROTO;				/* losers... */
		return( -1 );
	    }

	    /* check for full buffer */
	    if (( maxrbuf -= ( sn->sn_dend - ( sn->sn_dbuf + 4 ))) <= 0 ) {
		break;
	    }
	}

	if (( rc = snet_readread( sn, sn->sn_dend, maxrbuf, tv )) <= 0 ) {
	    return( rc );
	}
	sn->sn_dend += rc;
    }

    /*
     * we have a full encrypted buffer: decrypt it, save state,
     * and return some
     */
    sn->sn_dcur = sn->sn_dbuf + 4;
    maxrbuf = ntohl( *(uint32_t *)sn->sn_dbuf );
    (*sn->sn_decrypt)( sn->sn_crypto, sn->sn_dcur, maxrbuf );

gotsum:
#define min(x,y)	(((x)<(y))?(x):(y))
    maxrbuf -= ( sn->sn_dcur - ( sn->sn_dbuf + 4 ));
    rc = min( len, maxrbuf );
    memcpy( buf, sn->sn_dcur, rc );
    sn->sn_dcur += rc;

    /* did we exhaust the decrypted data? */
    if ( rc == maxrbuf ) {
	if ( sn->sn_dcur < sn->sn_dend ) { /* anything past decrypted data? */
	    memcpy( sn->sn_dbuf, sn->sn_dcur, sn->sn_dend - sn->sn_dcur );
	    sn->sn_dend = sn->sn_dbuf + ( sn->sn_dend - sn->sn_dcur );
	} else {
	    sn->sn_dend = sn->sn_dbuf;
	}
	sn->sn_dcur = NULL;
    }
    return( rc );
}

    static int
snet_readread( sn, buf, len, tv )
    SNET		*sn;
    char		*buf;
    int			len;
    struct timeval	*tv;
{
#ifndef linux
    struct timeval	tv_begin, tv_end;
#endif linux
    fd_set		fds;
    int			rc;
    extern int		errno;

    if ( tv ) {
	FD_ZERO( &fds );
	FD_SET( snet_fd( sn ), &fds );
#ifndef linux
	if ( gettimeofday( &tv_begin, NULL ) < 0 ) {
	    return( -1 );
	}
#endif linux
	/* time out case? */
	if ( select( snet_fd( sn ) + 1, &fds, NULL, NULL, tv ) < 0 ) {
	    return( -1 );
	}
	if ( FD_ISSET( snet_fd( sn ), &fds ) == 0 ) {
	    errno = ETIMEDOUT;
	    return( -1 );
	}
#ifndef linux
	if ( gettimeofday( &tv_end, NULL ) < 0 ) {
	    return( -1 );
	}

	if ( tv_begin.tv_usec > tv_end.tv_usec ) {
	    tv_end.tv_usec += 1000000;
	    tv_end.tv_sec -= 1;
	}
	if (( tv->tv_usec -= ( tv_end.tv_usec - tv_begin.tv_usec )) < 0 ) {
	    tv->tv_usec += 1000000;
	    tv->tv_sec -= 1;
	}
	if (( tv->tv_sec -= ( tv_end.tv_sec - tv_begin.tv_sec )) < 0 ) {
	    errno = ETIMEDOUT;
	    return( -1 );
	}
#endif linux
    }

    if (( rc = read( snet_fd( sn ), buf, len )) == 0 ) {
	sn->sn_flag = SNET_EOF;
    }
    return( rc );
}

/*
 * External entry point for reading with the snet library.  Compatible
 * with snet_getline()'s buffering.
 */
    int
snet_read( sn, buf, len, tv )
    SNET		*sn;
    char		*buf;
    int			len;
    struct timeval	*tv;
{
    int			rc;

    /*
     * If there's data already buffered, make sure it's not left over
     * from snet_getline(), and then return whatever's left.
     * XXX Note that snet_getline() calls snet_saslread().
     */
    if ( sn->sn_rcur < sn->sn_rend ) {
	if (( *sn->sn_rcur == '\n' ) && ( sn->sn_rstate == SNET_FUZZY )) {
	    sn->sn_rstate = SNET_BOL;
	    sn->sn_rcur++;
	}
	if ( sn->sn_rcur < sn->sn_rend ) {
	    rc = min( sn->sn_rend - sn->sn_rcur, len );
	    memcpy( buf, sn->sn_rcur, rc );
	    sn->sn_rcur += rc;
	    return( rc );
	}
    }

    return( snet_saslread( sn, buf, len, tv ));
}

/*
 * Get a null-terminated line of input, handle CR/LF issues.
 * Note that snet_getline() returns information from a common area which
 * may be overwritten by subsequent calls.
 */
    char *
snet_getline( sn, tv )
    SNET		*sn;
    struct timeval	*tv;
{
    char		*eol, *line;
    int			rc;
    extern int		errno;

    for ( eol = sn->sn_rcur; ; eol++) {
	if ( eol >= sn->sn_rend ) {				/* fill */
	    /* pullup */
	    if ( sn->sn_rcur > sn->sn_rbuf ) {
		if ( sn->sn_rcur < sn->sn_rend ) {
		    memcpy( sn->sn_rbuf, sn->sn_rcur,
			    (unsigned)( sn->sn_rend - sn->sn_rcur ));
		}
		eol = sn->sn_rend = sn->sn_rbuf + ( sn->sn_rend - sn->sn_rcur );
		sn->sn_rcur = sn->sn_rbuf;
	    }

	    /* expand */
	    if ( sn->sn_rend == sn->sn_rbuf + sn->sn_rbuflen ) {
		if ( sn->sn_maxlen != 0 && sn->sn_rbuflen >= sn->sn_maxlen ) {
		    errno = ENOMEM;
		    return( NULL );
		}
		if (( sn->sn_rbuf = (char *)realloc( sn->sn_rbuf,
			sn->sn_rbuflen + SNET_BUFLEN )) == NULL ) {
		    exit( 1 );
		}
		sn->sn_rbuflen += SNET_BUFLEN;
		eol = sn->sn_rend = sn->sn_rbuf + ( sn->sn_rend - sn->sn_rcur );
		sn->sn_rcur = sn->sn_rbuf;
	    }

	    if (( rc = snet_saslread( sn, sn->sn_rend,
		    sn->sn_rbuflen - ( sn->sn_rend - sn->sn_rbuf ),
		    tv )) < 0 ) {
		return( NULL );
	    }
	    if ( rc == 0 ) {	/* EOF */
		return( NULL );
	    }
	    sn->sn_rend += rc;
	}

	if ( *eol == '\r' || *eol == '\0' ) {
	    sn->sn_rstate = SNET_FUZZY;
	    break;
	}
	if ( *eol == '\n' ) {
	    if ( sn->sn_rstate == SNET_FUZZY ) {
		sn->sn_rstate = SNET_BOL;
		sn->sn_rcur = eol + 1;
		continue;
	    }
	    sn->sn_rstate = SNET_BOL;
	    break;
	}
	sn->sn_rstate = SNET_IN;
    }

    *eol = '\0';
    line = sn->sn_rcur;
    sn->sn_rcur = eol + 1;
    return( line );
}

    char * 
snet_getline_multi( sn, logger, tv )
    SNET		*sn;
    void		(*logger)( char * );
    struct timeval	*tv;
{
    char		*line; 

    do {
	if (( line = snet_getline( sn, tv )) == NULL ) {
	    return ( NULL );
	}

	if ( logger != NULL ) {
	    (*logger)( line );
	}

	if ( strlen( line ) < 3 ) {
	    return( NULL );
	}

	if ( !isdigit( (int)line[ 0 ] ) ||
		!isdigit( (int)line[ 1 ] ) ||
		!isdigit( (int)line[ 2 ] )) {
	    return( NULL );
	}

	if ( line[ 3 ] != '\0' &&
		line[ 3 ] != ' ' &&
		line [ 3 ] != '-' ) {
	    return ( NULL );
	}

    } while ( line[ 3 ] == '-' );

    return( line );
}
