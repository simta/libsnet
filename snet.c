/*
 * Copyright (c) 1995,1997 Regents of The University of Michigan.
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

#ifdef __STDC__
#include <stdarg.h>
#else __STDC__
#include <varargs.h>
#endif __STDC__

#include "snet.h"

#define SNET_BUFLEN	1024
#define SNET_IOVCNT	128

#define SNET_BOL	0
#define SNET_FUZZY	1
#define SNET_IN		2

/*
 * Need SASL entry points.
 * Need snet_write().
 * Read and write must use SASL buffering for encryption.
 * All routines must use snet_read() and snet_write() to access network.
 */

/*
 * This routine is necessary, since snet_getline() doesn't differentiate
 * between NULL => EOF and NULL => connection dropped (or some other error).
 */
    int
snet_eof( sn )
    SNET		*sn;
{
    return ( sn->sn_eof );
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
    if (( sn->sn_buf = (char *)malloc( SNET_BUFLEN )) == NULL ) {
	free( sn );
	return( NULL );
    }
    sn->sn_cur = sn->sn_end = sn->sn_buf;
    sn->sn_buflen = SNET_BUFLEN;
    sn->sn_maxlen = max;
    sn->sn_state = SNET_BOL;
    sn->sn_eof = 0;
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
    free( sn->sn_buf );
    if ( close( sn->sn_fd ) < 0 ) {
	return( -1 );
    }
    free( sn );
    return( 0 );
}

/*
 * Just like fprintf, only use the SNET header to get the fd, and use
 * writev() to move the data.
 *
 * Todo: %f, *, . and, -
 * should handle aribtrarily large output
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
    static struct iovec	*iov = NULL;
    static int		iovcnt = 0;
    va_list		vl;
    char		dbuf[ 1024 ], *p, *dbufoff = dbuf + sizeof( dbuf );
    int			i, state, d;

#ifdef __STDC__
    va_start( vl, format );
#else __STDC__
    va_start( vl );
#endif __STDC__

    for ( state = SNET_BOL, i = 0; *format; format++ ) {
	/*
	 * Make sure there's room for stuff
	 */
	if ( i == iovcnt ) {
	    if ( iov == NULL ) {
		if (( iov = (struct iovec *)
			malloc( sizeof( struct iovec ) * SNET_IOVCNT ))
			== NULL ) {
		    return( -1 );
		}
		iovcnt = SNET_IOVCNT;
	    } else {
		abort();	/* realloc */
	    }
	}

	if ( *format == '%' ) {
	    if ( state == SNET_IN ) {
		iov[ i ].iov_len = format - (char *)iov[ i ].iov_base;
		state = SNET_BOL;
		i++;
	    }

	    switch ( *++format ) {
	    case 's' :
		iov[ i ].iov_base = va_arg( vl, char * );
		iov[ i ].iov_len = strlen( iov[ i ].iov_base );
		i++;
		break;

	    case 'c' :
		if ( --dbufoff < dbuf ) {
		    abort();
		}
		*dbufoff = va_arg( vl, char );
		iov[ i ].iov_base = dbufoff;
		iov[ i ].iov_len = 1;
		i++;
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
		iov[ i ].iov_base = dbufoff;
		iov[ i ].iov_len = p - dbufoff;
		i++;
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
		iov[ i ].iov_base = dbufoff;
		iov[ i ].iov_len = p - dbufoff;
		i++;
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
		iov[ i ].iov_base = dbufoff;
		iov[ i ].iov_len = p - dbufoff;
		i++;
		break;

	    default :
		iov[ i ].iov_base = format;
		state = SNET_IN;
		break;

	    }
	} else {
	    if ( state == SNET_BOL ) {
		iov[ i ].iov_base = format;
		state = SNET_IN;
	    }
	}
    }
    if ( state == SNET_IN ) {
	iov[ i ].iov_len = format - (char *)iov[ i ].iov_base;
	i++;
    }

    va_end( vl );

    return ( writev( snet_fd( sn ), iov, i ));
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

    for ( eol = sn->sn_cur; ; eol++) {
	if ( eol >= sn->sn_end ) {				/* fill */
	    /* pullup */
	    if ( sn->sn_cur > sn->sn_buf ) {
		if ( sn->sn_cur < sn->sn_end ) {
		    memcpy( sn->sn_buf, sn->sn_cur,
			    (unsigned)( sn->sn_end - sn->sn_cur ));
		}
		eol = sn->sn_end = sn->sn_buf + ( sn->sn_end - sn->sn_cur );
		sn->sn_cur = sn->sn_buf;
	    }

	    /* expand */
	    if ( sn->sn_end == sn->sn_buf + sn->sn_buflen ) {
		if ( sn->sn_maxlen != 0 && sn->sn_buflen >= sn->sn_maxlen ) {
		    errno = ENOMEM;
		    return( NULL );
		}
		if (( sn->sn_buf = (char *)realloc( sn->sn_buf,
			sn->sn_buflen + SNET_BUFLEN )) == NULL ) {
		    exit( 1 );
		}
		sn->sn_buflen += SNET_BUFLEN;
		eol = sn->sn_end = sn->sn_buf + ( sn->sn_end - sn->sn_cur );
		sn->sn_cur = sn->sn_buf;
	    }

	    if (( rc = snet_read( sn, sn->sn_end,
		    sn->sn_buflen - ( sn->sn_end - sn->sn_buf ), tv )) < 0 ) {
		return( NULL );
	    }
	    if ( rc == 0 ) {	/* EOF */
		return( NULL );
	    }
	    sn->sn_end += rc;
	}

	if ( *eol == '\r' || *eol == '\0' ) {
	    sn->sn_state = SNET_FUZZY;
	    break;
	}
	if ( *eol == '\n' ) {
	    if ( sn->sn_state == SNET_FUZZY ) {
		sn->sn_state = SNET_BOL;
		sn->sn_cur = eol + 1;
		continue;
	    }
	    sn->sn_state = SNET_BOL;
	    break;
	}
	sn->sn_state = SNET_IN;
    }

    *eol = '\0';
    line = sn->sn_cur;
    sn->sn_cur = eol + 1;
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

    int
snet_read( sn, buf, len, tv )
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
	sn->sn_eof = 1;
    }
    return( rc );
}
