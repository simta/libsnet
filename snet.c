/*
 * Copyright (c) 1995,1997 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <varargs.h>
#include <syslog.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include "net.h"

#define NET_BUFLEN	1024
#define NET_IOVCNT	128

#define NET_BOL		0
#define NET_FUZZY	1
#define NET_IN		2

    char *
net_error( n )
    NET		*n;
{
    extern int	sys_nerr;
    extern char	*sys_errlist[];

    if ( n->nh_error ) {
	if ( n->nh_error > sys_nerr ) {
	    return( "Unknown error" );
	}
	return( sys_errlist[ n->nh_error ] );
    }
    return( NULL );
}

    NET *
net_attach( fd, max )
    int		fd;
    int		max;
{
    NET		*n;

    if (( n = (NET *)malloc( sizeof( NET ))) == NULL ) {
	return( NULL );
    }
    n->nh_fd = fd;
    if (( n->nh_buf = (char *)malloc( NET_BUFLEN )) == NULL ) {
	free( n );
	return( NULL );
    }
    n->nh_cur = n->nh_end = n->nh_buf;
    n->nh_buflen = NET_BUFLEN;
    n->nh_maxlen = max;
    n->nh_state = NET_BOL;
    n->nh_error = 0;
    return( n );
}

    NET *
net_open( path, flags, mode )
    char	*path;
    int		flags;
    int		mode;
{
    int		fd;

    if (( fd = open( path, flags, mode )) < 0 ) {
	return( NULL );
    }
    return( net_attach( fd ));
}

net_close( n )
    NET		*n;
{
    if ( free( n->nh_buf ) != 1 ) {
	return( -1 );
    }
    if ( close( n->nh_fd ) < 0 ) {
	return( -1 );
    }
    if ( free( n ) != 1 ) {
	return( -1 );
    }
    return( 0 );
}

/*
 * Just like fprintf, only use the NET header to get the fd, and use
 * writev() to move the data.
 *
 * Todo: %c, %f, *, . and, -
 * get rid of statics
 * handle aribtrary output
 */
net_writef( va_alist )
    va_dcl
{
    static struct iovec	*iov = NULL;
    static int		iovcnt = 0;
    va_list		vl;
    char		dbuf[ 1024 ], *p, *dbufoff = dbuf + sizeof( dbuf );
    NET			*n;
    char		*format;
    int			i, state, d;

    va_start( vl );

    n = va_arg( vl, NET * );
    format = va_arg( vl, char * );

    for ( state = NET_BOL, i = 0; *format; format++ ) {
	/*
	 * Make sure there's room for stuff
	 */
	if ( i == iovcnt ) {
	    if ( iov == NULL ) {
		if (( iov = (struct iovec *)
			malloc( sizeof( struct iovec ) * NET_IOVCNT ))
			== NULL ) {
		    return( -1 );
		}
		iovcnt = NET_IOVCNT;
	    } else {
		abort();	/* realloc */
	    }
	}

	if ( *format == '%' ) {
	    if ( state == NET_IN ) {
		iov[ i ].iov_len = format - iov[ i ].iov_base;
		state = NET_BOL;
		i++;
	    }

	    switch ( *++format ) {
	    case 's' :
		iov[ i ].iov_base = va_arg( vl, char * );
		iov[ i ].iov_len = strlen( iov[ i ].iov_base );
		i++;
		break;

	    case 'd' :
		d = va_arg( vl, int );
		p = dbufoff;
		while ( d ) {
		    if ( --dbufoff < dbuf ) {
			syslog( LOG_ERR, "net_writef: ran out of %%d room!" );
			abort();
		    }
		    *dbufoff = '0' + ( d % 10 );
		    d /= 10;
		}
		iov[ i ].iov_base = dbufoff;
		iov[ i ].iov_len = p - dbufoff;
		i++;
		break;

	    default :
		iov[ i ].iov_base = format;
		state = NET_IN;
		break;

	    }
	} else {
	    if ( state == NET_BOL ) {
		iov[ i ].iov_base = format;
		state = NET_IN;
	    }
	}
    }
    if ( state == NET_IN ) {
	iov[ i ].iov_len = format - iov[ i ].iov_base;
	i++;
    }

    va_end( vl );

    return ( writev( net_fd( n ), iov, i ));
}

/*
 * Get a null-terminated line of input, handle CR/LF issues.
 * Note that net_getline() returns information from a common area which
 * may be overwritten by subsequent calls.
 */
    char *
net_getline( n, tv )
    NET			*n;
    struct timeval	*tv;
{
    char		*eol, *t;
    int			rc;
    extern int		errno;

    for ( eol = n->nh_cur; ; eol++) {
	if ( eol >= n->nh_end ) {				/* fill */
	    /* pullup */
	    if ( n->nh_cur > n->nh_buf ) {
		if ( n->nh_cur < n->nh_end ) {
		    bcopy( n->nh_cur, n->nh_buf, n->nh_end - n->nh_cur );
		}
		eol = n->nh_end = n->nh_buf + ( n->nh_end - n->nh_cur );
		n->nh_cur = n->nh_buf;
	    }

	    /* expand */
	    if ( n->nh_end == n->nh_buf + n->nh_buflen ) {
		if ( n->nh_buflen >= n->nh_maxlen ) {
		    errno = ENOMEM;
		    return( NULL );
		}
		if (( n->nh_buf = (char *)realloc( n->nh_buf,
			n->nh_buflen + NET_BUFLEN )) == NULL ) {
		    exit( 1 );
		}
		n->nh_buflen += NET_BUFLEN;
		eol = n->nh_end = n->nh_buf + ( n->nh_end - n->nh_cur );
		n->nh_cur = n->nh_buf;
	    }

	    if (( rc = net_read( n, n->nh_end,
		    n->nh_buflen - ( n->nh_end - n->nh_buf ), tv )) < 0 ) {
		return( NULL );
	    }
	    if ( rc == 0 ) {	/* EOF */
		return( NULL );
	    }
	    n->nh_end += rc;
	}

	if ( *eol == '\r' || *eol == '\0' ) {
	    n->nh_state = NET_FUZZY;
	    break;
	}
	if ( *eol == '\n' ) {
	    if ( n->nh_state == NET_FUZZY ) {
		n->nh_state = NET_BOL;
		n->nh_cur = eol + 1;
		continue;
	    }
	    break;
	}
	n->nh_state = NET_IN;
    }

    *eol = '\0';
    t = n->nh_cur;
    n->nh_cur = eol + 1;
    return( t );
}

net_read( n, buf, len, tv )
    NET			*n;
    char		*buf;
    int			len;
    struct timeval	*tv;
{
    struct timeval	tv_begin, tv_end;
    fd_set		fds;
    extern int		errno;

    if ( tv ) {
	FD_ZERO( &fds );
	FD_SET( net_fd( n ), &fds );
	if ( gettimeofday( &tv_begin, NULL ) < 0 ) {
	    return( -1 );
	}
	/* time out case? */
	if ( select( net_fd( n ) + 1, &fds, NULL, NULL, tv ) < 0 ) {
	    return( -1 );
	}
	if ( FD_ISSET( net_fd( n ), &fds ) == 0 ) {
	    errno = ETIME;
	    return( -1 );
	}
	if ( gettimeofday( &tv_end, NULL ) < 0 ) {
	    return( -1 );
	}

	tv->tv_usec -= tv_end.tv_usec - tv_begin.tv_usec;
	if ( tv->tv_usec < 0 ) {
	    tv->tv_usec += 1000000;
	    tv->tv_sec -= 1;
	}
	tv->tv_sec -= tv_end.tv_sec - tv_begin.tv_sec;
	if ( tv->tv_sec < 0 ) {
	    errno = ETIME;
	    return( -1 );
	}
    }

    errno = 0;
    return( read( net_fd( n ), buf, len ));
}
