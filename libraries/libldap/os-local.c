/* os-local.c -- platform-specific domain socket code */
/* $OpenLDAP$ */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 1998-2004 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
/* Portions Copyright (c) 1995 Regents of the University of Michigan.
 * All rights reserved. 
 */
/* Portions (C) Copyright PADL Software Pty Ltd. 1999
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that this notice is preserved
 * and that due credit is given to PADL Software Pty Ltd. This software
 * is provided ``as is'' without express or implied warranty.  
 */

#include "portable.h"

#ifdef LDAP_PF_LOCAL

#include <stdio.h>

#include <ac/stdlib.h>

#include <ac/errno.h>
#include <ac/socket.h>
#include <ac/string.h>
#include <ac/time.h>
#include <ac/unistd.h>

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_IO_H
#include <io.h>
#endif /* HAVE_IO_H */

#include "ldap-int.h"
#include "ldap_defaults.h"

#ifdef LDAP_DEBUG

#define oslocal_debug(ld,fmt,arg1,arg2,arg3) \
do { \
	ldap_log_printf(ld, LDAP_DEBUG_TRACE, fmt, arg1, arg2, arg3); \
} while(0)

#else

#define oslocal_debug(ld,fmt,arg1,arg2,arg3) ((void)0)

#endif /* LDAP_DEBUG */

static void
ldap_pvt_set_errno(int err)
{
	errno = err;
}

static int
ldap_pvt_ndelay_on(LDAP *ld, int fd)
{
	oslocal_debug(ld, "ldap_ndelay_on: %d\n",fd,0,0);
	return ber_pvt_socket_set_nonblock( fd, 1 );
}
   
static int
ldap_pvt_ndelay_off(LDAP *ld, int fd)
{
	oslocal_debug(ld, "ldap_ndelay_off: %d\n",fd,0,0);
	return ber_pvt_socket_set_nonblock( fd, 0 );
}

static ber_socket_t
ldap_pvt_socket(LDAP *ld)
{
	ber_socket_t s = socket(PF_LOCAL, SOCK_STREAM, 0);
	oslocal_debug(ld, "ldap_new_socket: %d\n",s,0,0);
	return ( s );
}

static int
ldap_pvt_close_socket(LDAP *ld, int s)
{
	oslocal_debug(ld, "ldap_close_socket: %d\n",s,0,0);
	return tcp_close(s);
}

#undef TRACE
#define TRACE do { \
	char ebuf[128]; \
	oslocal_debug(ld, \
		"ldap_is_socket_ready: errror on socket %d: errno: %d (%s)\n", \
		s, \
		errno, \
		AC_STRERROR_R(errno, ebuf, sizeof ebuf)); \
} while( 0 )

/*
 * check the socket for errors after select returned.
 */
static int
ldap_pvt_is_socket_ready(LDAP *ld, int s)
{
	oslocal_debug(ld, "ldap_is_sock_ready: %d\n",s,0,0);

#if defined( notyet ) /* && defined( SO_ERROR ) */
{
	int so_errno;
	socklen_t dummy = sizeof(so_errno);
	if ( getsockopt( s, SOL_SOCKET, SO_ERROR, &so_errno, &dummy )
		== AC_SOCKET_ERROR )
	{
		return -1;
	}
	if ( so_errno ) {
		ldap_pvt_set_errno(so_errno);
		TRACE;
		return -1;
	}
	return 0;
}
#else
{
	/* error slippery */
	struct sockaddr_un sa;
	char ch;
	socklen_t dummy = sizeof(sa);
	if ( getpeername( s, (struct sockaddr *) &sa, &dummy )
		== AC_SOCKET_ERROR )
	{
		/* XXX: needs to be replace with ber_stream_read() */
		read(s, &ch, 1);
		TRACE;
		return -1;
	}
	return 0;
}
#endif
	return -1;
}
#undef TRACE

#if !defined(HAVE_GETPEEREID) && \
	!defined(SO_PEERCRED) && !defined(LOCAL_PEERCRED) && \
	defined(HAVE_SENDMSG) && defined(HAVE_MSGHDR_MSG_ACCRIGHTS)
#define DO_SENDMSG
static const char abandonPDU[] = {LDAP_TAG_MESSAGE, 6,
	LDAP_TAG_MSGID, 1, 0, LDAP_REQ_ABANDON, 1, 0};
#endif

static int
ldap_pvt_connect(LDAP *ld, ber_socket_t s, struct sockaddr_un *sa, int async)
{
	int rc;
	struct timeval	tv, *opt_tv=NULL;

	if ( (opt_tv = ld->ld_options.ldo_tm_net) != NULL ) {
		tv.tv_usec = opt_tv->tv_usec;
		tv.tv_sec = opt_tv->tv_sec;
	}

	oslocal_debug(ld, "ldap_connect_timeout: fd: %d tm: %ld async: %d\n",
		s, opt_tv ? tv.tv_sec : -1L, async);

	if ( ldap_pvt_ndelay_on(ld, s) == -1 ) return -1;

	if ( connect(s, (struct sockaddr *) sa, sizeof(struct sockaddr_un))
		!= AC_SOCKET_ERROR )
	{
		if ( ldap_pvt_ndelay_off(ld, s) == -1 ) return -1;

#ifdef DO_SENDMSG
	/* Send a dummy message with access rights. Remote side will
	 * obtain our uid/gid by fstat'ing this descriptor.
	 */
sendcred:
		{
			int fds[2];
			if (pipe(fds) == 0) {
				/* Abandon, noop, has no reply */
				struct iovec iov;
				struct msghdr msg = {0};
				iov.iov_base = (char *) abandonPDU;
				iov.iov_len = sizeof abandonPDU;
				msg.msg_iov = &iov;
				msg.msg_iovlen = 1;
				msg.msg_accrights = (char *)fds;
				msg.msg_accrightslen = sizeof(int);
				sendmsg( s, &msg, 0 );
				close(fds[0]);
				close(fds[1]);
			}
		}
#endif
		return 0;
	}

	if ( errno != EINPROGRESS && errno != EWOULDBLOCK ) return -1;
	
#ifdef notyet
	if ( async ) return -2;
#endif

#ifdef HAVE_POLL
	{
		struct pollfd fd;
		int timeout = INFTIM;

		if( opt_tv != NULL ) timeout = TV2MILLISEC( &tv );

		fd.fd = s;
		fd.events = POLLOUT;

		do {
			fd.revents = 0;
			rc = poll( &fd, 1, timeout );
		} while( rc == AC_SOCKET_ERROR && errno == EINTR &&
			LDAP_BOOL_GET(&ld->ld_options, LDAP_BOOL_RESTART ));

		if( rc == AC_SOCKET_ERROR ) return rc;

		if( fd.revents & POLLOUT ) {
			if ( ldap_pvt_is_socket_ready(ld, s) == -1 ) return -1;
			if ( ldap_pvt_ndelay_off(ld, s) == -1 ) return -1;
#ifdef DO_SENDMSG
			goto sendcred;
#else
			return ( 0 );
#endif
		}
	}
#else
	{
		fd_set wfds, *z=NULL;

		do { 
			FD_ZERO(&wfds);
			FD_SET(s, &wfds );
			rc = select( ldap_int_tblsize, z, &wfds, z, opt_tv ? &tv : NULL );
		} while( rc == AC_SOCKET_ERROR && errno == EINTR &&
			LDAP_BOOL_GET(&ld->ld_options, LDAP_BOOL_RESTART ));

		if( rc == AC_SOCKET_ERROR ) return rc;

		if ( FD_ISSET(s, &wfds) ) {
			if ( ldap_pvt_is_socket_ready(ld, s) == -1 ) return -1;
			if ( ldap_pvt_ndelay_off(ld, s) == -1 ) return -1;
#ifdef DO_SENDMSG
			goto sendcred;
#else
			return ( 0 );
#endif
		}
	}
#endif

	oslocal_debug(ld, "ldap_connect_timeout: timed out\n",0,0,0);
	ldap_pvt_set_errno( ETIMEDOUT );
	return ( -1 );
}

int
ldap_connect_to_path(LDAP *ld, Sockbuf *sb, const char *path, int async)
{
	struct sockaddr_un	server;
	ber_socket_t		s;
	int			rc;

	oslocal_debug(ld, "ldap_connect_to_path\n",0,0,0);

	s = ldap_pvt_socket( ld );
	if ( s == AC_SOCKET_INVALID ) {
		return -1;
	}

	if ( path == NULL || path[0] == '\0' ) {
		path = LDAPI_SOCK;
	} else {
		if ( strlen(path) > (sizeof( server.sun_path ) - 1) ) {
			ldap_pvt_set_errno( ENAMETOOLONG );
			return -1;
		}
	}

	oslocal_debug(ld, "ldap_connect_to_path: Trying %s\n", path, 0, 0);

	memset( &server, '\0', sizeof(server) );
	server.sun_family = AF_LOCAL;
	strcpy( server.sun_path, path );

	rc = ldap_pvt_connect(ld, s, &server, async);

	if (rc == 0) {
		ber_sockbuf_ctrl( sb, LBER_SB_OPT_SET_FD, (void *)&s );
	} else {
		ldap_pvt_close_socket(ld, s);
	}
	return rc;
}
#else
static int dummy;
#endif /* LDAP_PF_LOCAL */
