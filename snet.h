/*
 * Copyright (c) 1995,1997 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

typedef struct {
    int		nh_fd;
    char	*nh_buf;
    char	*nh_end;
    char	*nh_cur;
    int		nh_buflen;
    int		nh_maxlen;
    int		nh_state;
    int		nh_error;
} NET;

#define net_fd( n )	((n)->nh_fd)

#ifdef __STDC__
#define ___P(x)		x
#else __STDC__
#define ___P(x)		()
#endif __STDC__

char	*net_error ___P(( NET * ));
NET	*net_attach ___P(( int, int ));
NET	*net_open ___P(( char *, int, int, int ));
int	net_close ___P(( NET * ));
int	net_writef ___P(( NET *, char *, ... ));
char	*net_getline ___P(( NET *, struct timeval * ));
int	net_read ___P(( NET *, char *, int, struct timeval * ));
