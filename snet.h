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

NET	*net_open();
NET	*net_attach();
char	*net_getline();
