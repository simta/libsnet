/*
 * Copyright (c) 1995,1997 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

typedef struct {
    int		sn_fd;
    char	*sn_buf;
    char	*sn_end;
    char	*sn_cur;
    int		sn_buflen;
    int		sn_maxlen;
    int		sn_state;
    int		sn_eof;
} SNET;

#define snet_fd( sn )	((sn)->sn_fd)

#ifdef __STDC__
#define ___P(x)		x
#else __STDC__
#define ___P(x)		()
#endif __STDC__

int	snet_eof ___P(( SNET * ));
SNET	*snet_attach ___P(( int, int ));
SNET	*snet_open ___P(( char *, int, int, int ));
int	snet_close ___P(( SNET * ));
int	snet_writef ___P(( SNET *, char *, ... ));
char	*snet_getline ___P(( SNET *, struct timeval * ));
char	*snet_getline_multi ___P(( SNET *, void (*)(char *),
		struct timeval * ));
int	snet_read ___P(( SNET *, char *, int, struct timeval * ));
