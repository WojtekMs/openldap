/* slap.h - stand alone ldap server include file */
/* $OpenLDAP$ */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 1998-2015 The OpenLDAP Foundation.
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
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

#ifndef _SLAP_H_
#define _SLAP_H_

#include "ldap_defaults.h"

#include <stdio.h>
#include <ac/stdlib.h>

#include <sys/types.h>
#include <ac/syslog.h>
#include <ac/regex.h>
#include <ac/signal.h>
#include <ac/socket.h>
#include <ac/time.h>
#include <ac/param.h>

#include "avl.h"

#ifndef ldap_debug
#define ldap_debug slap_debug
#endif

#include "ldap_log.h"

#include <ldap.h>
#include <ldap_schema.h>

#include "lber_pvt.h"
#include "ldap_pvt.h"
#include "ldap_pvt_thread.h"
#include "ldap_queue.h"

#include <event2/event.h>

LDAP_BEGIN_DECL

/*
 * SLAPD Memory allocation macros
 *
 * Unlike ch_*() routines, these routines do not assert() upon
 * allocation error.  They are intended to be used instead of
 * ch_*() routines where the caller has implemented proper
 * checking for and handling of allocation errors.
 *
 * Patches to convert ch_*() calls to SLAP_*() calls welcomed.
 */
#define SLAP_MALLOC(s) ber_memalloc( ( s ) )
#define SLAP_CALLOC(n, s) ber_memcalloc( ( n ), ( s ) )
#define SLAP_REALLOC(p, s) ber_memrealloc( ( p ), ( s ) )
#define SLAP_FREE(p) ber_memfree( ( p ) )
#define SLAP_VFREE(v) ber_memvfree( (void **)( v ) )
#define SLAP_STRDUP(s) ber_strdup( ( s ) )
#define SLAP_STRNDUP(s, l) ber_strndup( ( s ), ( l ) )

#define SERVICE_NAME OPENLDAP_PACKAGE "-lloadd"
#define SLAPD_ANONYMOUS ""
#define SLAP_STRING_UNKNOWN "unknown"

#define SLAP_MAX_WORKER_THREADS ( 16 )

#define LLOAD_SB_MAX_INCOMING_CLIENT ( ( 1 << 18 ) - 1 )
#define LLOAD_SB_MAX_INCOMING_UPSTREAM ( ( 1 << 24 ) - 1 )

#define LLOAD_CONN_MAX_PDUS_PER_CYCLE_DEFAULT 10

#define SLAP_TEXT_BUFLEN ( 256 )

/* unknown config file directive */
#define SLAP_CONF_UNKNOWN ( -1026 )

#define BER_BV_OPTIONAL( bv ) ( BER_BVISNULL( bv ) ? NULL : ( bv ) )

LDAP_SLAPD_V (int) slap_debug;

typedef unsigned long slap_mask_t;

typedef struct Backend Backend;
typedef struct PendingConnection PendingConnection;
typedef struct Connection Connection;
typedef struct Operation Operation;
/* end of forward declarations */

typedef union Sockaddr {
    struct sockaddr sa_addr;
    struct sockaddr_in sa_in_addr;
#ifdef LDAP_PF_INET6
    struct sockaddr_storage sa_storage;
    struct sockaddr_in6 sa_in6_addr;
#endif
#ifdef LDAP_PF_LOCAL
    struct sockaddr_un sa_un_addr;
#endif
} Sockaddr;

#ifdef LDAP_PF_INET6
extern int slap_inet4or6;
#endif

typedef LDAP_CIRCLEQ_HEAD(BeSt, Backend) slap_b_head;
typedef LDAP_CIRCLEQ_HEAD(ClientSt, Connection) slap_c_head;

LDAP_SLAPD_V (slap_b_head) backend;
LDAP_SLAPD_V (slap_c_head) clients;
LDAP_SLAPD_V (ldap_pvt_thread_mutex_t) backend_mutex;
LDAP_SLAPD_V (Backend *) current_backend;
LDAP_SLAPD_V (struct slap_bindconf) bindconf;
LDAP_SLAPD_V (struct berval) lloadd_identity;

LDAP_SLAPD_V (int) slapMode;
#define SLAP_UNDEFINED_MODE 0x0000
#define SLAP_SERVER_MODE 0x0001
#define SLAP_TOOL_MODE 0x0002
#define SLAP_MODE 0x0003

#define SLAP_SERVER_RUNNING 0x8000

#define SB_TLS_DEFAULT ( -1 )
#define SB_TLS_OFF 0
#define SB_TLS_ON 1
#define SB_TLS_CRITICAL 2

typedef struct slap_keepalive {
    int sk_idle;
    int sk_probes;
    int sk_interval;
} slap_keepalive;

typedef struct slap_bindconf {
    struct berval sb_uri;
    int sb_version;
    int sb_tls;
    int sb_method;
    int sb_timeout_api;
    int sb_timeout_net;
    struct berval sb_binddn;
    struct berval sb_cred;
    struct berval sb_saslmech;
    char *sb_secprops;
    struct berval sb_realm;
    struct berval sb_authcId;
    struct berval sb_authzId;
    slap_keepalive sb_keepalive;
#ifdef HAVE_TLS
    void *sb_tls_ctx;
    char *sb_tls_cert;
    char *sb_tls_key;
    char *sb_tls_cacert;
    char *sb_tls_cacertdir;
    char *sb_tls_reqcert;
    char *sb_tls_reqsan;
    char *sb_tls_cipher_suite;
    char *sb_tls_protocol_min;
    char *sb_tls_ecname;
#ifdef HAVE_OPENSSL
    char *sb_tls_crlcheck;
#endif
    int sb_tls_int_reqcert;
    int sb_tls_int_reqsan;
    int sb_tls_do_init;
#endif
} slap_bindconf;

typedef struct slap_verbmasks {
    struct berval word;
    const slap_mask_t mask;
} slap_verbmasks;

typedef struct slap_cf_aux_table {
    struct berval key;
    int off;
    char type;
    char quote;
    void *aux;
} slap_cf_aux_table;

typedef int slap_cf_aux_table_parse_x( struct berval *val,
        void *bc,
        slap_cf_aux_table *tab0,
        const char *tabmsg,
        int unparse );

#define SLAP_RESTRICT_OP_ADD 0x0001U
#define SLAP_RESTRICT_OP_BIND 0x0002U
#define SLAP_RESTRICT_OP_COMPARE 0x0004U
#define SLAP_RESTRICT_OP_DELETE 0x0008U
#define SLAP_RESTRICT_OP_EXTENDED 0x0010U
#define SLAP_RESTRICT_OP_MODIFY 0x0020U
#define SLAP_RESTRICT_OP_RENAME 0x0040U
#define SLAP_RESTRICT_OP_SEARCH 0x0080U
#define SLAP_RESTRICT_OP_MASK 0x00FFU

#define SLAP_RESTRICT_READONLY 0x80000000U

#define SLAP_RESTRICT_EXOP_START_TLS 0x0100U
#define SLAP_RESTRICT_EXOP_MODIFY_PASSWD 0x0200U
#define SLAP_RESTRICT_EXOP_WHOAMI 0x0400U
#define SLAP_RESTRICT_EXOP_CANCEL 0x0800U
#define SLAP_RESTRICT_EXOP_MASK 0xFF00U

#define SLAP_RESTRICT_OP_READS \
    ( SLAP_RESTRICT_OP_COMPARE | SLAP_RESTRICT_OP_SEARCH )
#define SLAP_RESTRICT_OP_WRITES \
    ( SLAP_RESTRICT_OP_ADD | SLAP_RESTRICT_OP_DELETE | SLAP_RESTRICT_OP_MODIFY | SLAP_RESTRICT_OP_RENAME )
#define SLAP_RESTRICT_OP_ALL \
    ( SLAP_RESTRICT_OP_READS | SLAP_RESTRICT_OP_WRITES | SLAP_RESTRICT_OP_BIND | SLAP_RESTRICT_OP_EXTENDED )

typedef struct config_reply_s ConfigReply; /* config.h */

typedef struct Listener Listener;

typedef enum {
#ifdef LDAP_API_FEATURE_VERIFY_CREDENTIALS
    LLOAD_FEATURE_VC = 1 << 0,
#endif /* LDAP_API_FEATURE_VERIFY_CREDENTIALS */
    LLOAD_FEATURE_PROXYAUTHZ = 1 << 1,
} lload_features_t;

enum lload_tls_type {
    LLOAD_CLEARTEXT = 0,
    LLOAD_LDAPS,
    LLOAD_STARTTLS_OPTIONAL,
    LLOAD_STARTTLS,
    LLOAD_TLS_ESTABLISHED,
};

struct PendingConnection {
    Backend *backend;

    struct event *event;
    ber_socket_t fd;

    LDAP_LIST_ENTRY(PendingConnection) next;
};

/* Can hold mutex when locking a linked connection */
struct Backend {
    ldap_pvt_thread_mutex_t b_mutex;

    struct berval b_uri;
    int b_proto, b_port;
    enum lload_tls_type b_tls;
    char *b_host;

    int b_retry_timeout, b_failed;
    struct event *b_retry_event;
    struct timeval b_retry_tv;

    int b_numconns, b_numbindconns;
    int b_bindavail, b_active, b_opening;
    LDAP_CIRCLEQ_HEAD(ConnSt, Connection) b_conns, b_bindconns, b_preparing;
    LDAP_LIST_HEAD(ConnectingSt, PendingConnection) b_connecting;

    long b_max_pending, b_max_conn_pending;
    long b_n_ops_executing;

    LDAP_CIRCLEQ_ENTRY(Backend) b_next;
};

typedef int (*OperationHandler)( Operation *op, BerElement *ber );
typedef int (*RequestHandler)( Connection *c, Operation *op );
typedef struct lload_exop_handlers_t {
    struct berval oid;
    RequestHandler func;
} ExopHandler;

typedef int (*CONNECTION_PDU_CB)( Connection *c );
typedef void (*CONNECTION_DESTROY_CB)( Connection *c );

/* connection state (protected by c_mutex) */
enum sc_state {
    LLOAD_C_INVALID = 0, /* MUST BE ZERO (0) */
    LLOAD_C_READY,       /* ready */
    LLOAD_C_CLOSING,     /* closing */
    LLOAD_C_ACTIVE,      /* exclusive operation (tls setup, ...) in progress */
    LLOAD_C_BINDING,     /* binding */
};
enum sc_type {
    LLOAD_C_OPEN = 0,  /* regular connection */
    LLOAD_C_PREPARING, /* upstream connection not assigned yet */
    LLOAD_C_BIND, /* connection used to handle bind client requests if VC not enabled */
    LLOAD_C_PRIVILEGED, /* connection can override proxyauthz control */
};
/*
 * represents a connection from an ldap client/to ldap server
 */
struct Connection {
    enum sc_state c_state; /* connection state */
    enum sc_type c_type;
    ber_socket_t c_fd;

/*
 * Connection reference counting:
 * - connection has a reference counter in c_refcnt
 * - also a liveness/validity token is added to c_refcnt during
 *   connection_init, its existence is tracked in c_live and is usually the
 *   only one that prevents it from being destroyed
 * - anyone who needs to be able to lock the connection after unlocking it has
 *   to use CONNECTION_UNLOCK_INCREF, they are then responsible that
 *   CONNECTION_LOCK_DECREF+CONNECTION_UNLOCK_OR_DESTROY is used when they are
 *   done with it
 * - when a connection is considered dead, use CONNECTION_DESTROY on a locked
 *   connection, it might get disposed of or if anyone still holds a token, it
 *   just gets unlocked and it's the last token holder's responsibility to run
 *   CONNECTION_UNLOCK_OR_DESTROY
 * - CONNECTION_LOCK_DESTROY is a shorthand for locking, decreasing refcount
 *   and CONNECTION_DESTROY
 */
    ldap_pvt_thread_mutex_t c_mutex; /* protect the connection */
    int c_refcnt, c_live;
    CONNECTION_DESTROY_CB c_destroy;
    CONNECTION_PDU_CB c_pdu_cb;
#define CONNECTION_LOCK(c) ldap_pvt_thread_mutex_lock( &(c)->c_mutex )
#define CONNECTION_UNLOCK(c) ldap_pvt_thread_mutex_unlock( &(c)->c_mutex )
#define CONNECTION_LOCK_DECREF(c) \
    do { \
        CONNECTION_LOCK(c); \
        (c)->c_refcnt--; \
    } while (0)
#define CONNECTION_UNLOCK_INCREF(c) \
    do { \
        (c)->c_refcnt++; \
        CONNECTION_UNLOCK(c); \
    } while (0)
#define CONNECTION_UNLOCK_OR_DESTROY(c) \
    do { \
        assert( (c)->c_refcnt >= 0 ); \
        if ( !( c )->c_refcnt ) { \
            Debug( LDAP_DEBUG_TRACE, "%s: destroying connection connid=%lu\n", \
                    __func__, (c)->c_connid ); \
            (c)->c_destroy( (c) ); \
            (c) = NULL; \
        } else { \
            CONNECTION_UNLOCK(c); \
        } \
    } while (0)
#define CONNECTION_DESTROY(c) \
    do { \
        (c)->c_refcnt -= (c)->c_live; \
        (c)->c_live = 0; \
        CONNECTION_UNLOCK_OR_DESTROY(c); \
    } while (0)
#define CONNECTION_LOCK_DESTROY(c) \
    do { \
        CONNECTION_LOCK_DECREF(c); \
        CONNECTION_DESTROY(c); \
    } while (0);

    Sockbuf *c_sb; /* ber connection stuff */

    /* set by connection_init */
    unsigned long c_connid;    /* unique id of this connection */
    struct berval c_peer_name; /* peer name (trans=addr:port) */
    time_t c_starttime;        /* when the connection was opened */

    time_t c_activitytime;  /* when the connection was last used */
    ber_int_t c_next_msgid; /* msgid of the next message */

    /* must not be used while holding either mutex */
    struct event *c_read_event, *c_write_event;

    /* can only be changed by binding thread */
    struct berval c_sasl_bind_mech; /* mech in progress */
    struct berval c_auth;           /* authcDN (possibly in progress) */

#ifdef LDAP_API_FEATURE_VERIFY_CREDENTIALS
    struct berval c_vc_cookie;
#endif /* LDAP_API_FEATURE_VERIFY_CREDENTIALS */

    /* Can be held while acquiring c_mutex to inject things into c_ops or
     * destroy the connection */
    ldap_pvt_thread_mutex_t c_io_mutex; /* only one pdu written at a time */

    BerElement *c_currentber; /* ber we're attempting to read */
    BerElement *c_pendingber; /* ber we're attempting to write */

    TAvlnode *c_ops; /* Operations pending on the connection */

#define CONN_IS_TLS 1
#define CONN_IS_BIND 4
#define CONN_IS_IPC 8

#ifdef HAVE_TLS
    enum lload_tls_type c_is_tls; /* true if this LDAP over raw TLS */
#endif

    long c_n_ops_executing; /* num of ops currently executing */
    long c_n_ops_completed; /* num of ops completed */

    /*
     * Protected by the CIRCLEQ mutex:
     * - Client: clients_mutex
     * - Upstream: b->b_mutex
     */
    LDAP_CIRCLEQ_ENTRY(Connection) c_next;

    void *c_private;
};

enum op_state {
    LLOAD_OP_NOT_FREEING = 0,
    LLOAD_OP_FREEING_UPSTREAM = 1 << 0,
    LLOAD_OP_FREEING_CLIENT = 1 << 1,
    LLOAD_OP_DETACHING_UPSTREAM = 1 << 2,
    LLOAD_OP_DETACHING_CLIENT = 1 << 3,
};
#define LLOAD_OP_FREEING_MASK \
    ( LLOAD_OP_FREEING_UPSTREAM | LLOAD_OP_FREEING_CLIENT )
#define LLOAD_OP_DETACHING_MASK \
    ( LLOAD_OP_DETACHING_UPSTREAM | LLOAD_OP_DETACHING_CLIENT )

struct Operation {
    Connection *o_client;
    unsigned long o_client_connid;
    int o_client_live, o_client_refcnt;
    ber_int_t o_client_msgid;

    Connection *o_upstream;
    unsigned long o_upstream_connid;
    int o_upstream_live, o_upstream_refcnt;
    ber_int_t o_upstream_msgid;

    /* Protects o_client, o_upstream pointers before we lock their c_mutex if
     * we don't know they are still alive */
    ldap_pvt_thread_mutex_t o_link_mutex;
    /* Protects o_freeing, can be locked while holding c_mutex */
    ldap_pvt_thread_mutex_t o_mutex;
    /* Consistent w.r.t. o_mutex, only written to while holding
     * op->o_{client,upstream}->c_mutex */
    enum op_state o_freeing;
    ber_tag_t o_tag;

    BerElement *o_ber;
    BerValue o_request, o_ctrls;
};

#ifdef LDAP_DEBUG
#ifdef LDAP_SYSLOG
#ifdef LOG_LOCAL4
#define SLAP_DEFAULT_SYSLOG_USER LOG_LOCAL4
#endif /* LOG_LOCAL4 */

#define Statslog( level, fmt, connid, opid, arg1, arg2, arg3 ) \
    Log( (level), ldap_syslog_level, (fmt), (connid), (opid), \
            ( arg1 ), ( arg2 ), ( arg3 ) )
#define StatslogTest( level ) ( ( ldap_debug | ldap_syslog ) & ( level ) )
#else /* !LDAP_SYSLOG */
#define Statslog( level, fmt, connid, opid, arg1, arg2, arg3 ) \
    do { \
        if ( ldap_debug & (level) ) \
            lutil_debug( ldap_debug, (level), (fmt), (connid), (opid), \
                    ( arg1 ), ( arg2 ), ( arg3 ) ); \
    } while (0)
#define StatslogTest( level ) ( ldap_debug & ( level ) )
#endif /* !LDAP_SYSLOG */
#else /* !LDAP_DEBUG */
#define Statslog( level, fmt, connid, opid, arg1, arg2, arg3 ) ( (void)0 )
#define StatslogTest( level ) ( 0 )
#endif /* !LDAP_DEBUG */

/*
 * listener; need to access it from monitor backend
 */
struct Listener {
    struct berval sl_url;
    struct berval sl_name;
    mode_t sl_perms;
#ifdef HAVE_TLS
    int sl_is_tls;
#endif
    struct event_base *base;
    struct evconnlistener *listener;
    int sl_mute; /* Listener is temporarily disabled due to emfile */
    int sl_busy; /* Listener is busy (accept thread activated) */
    ber_socket_t sl_sd;
    Sockaddr sl_sa;
#define sl_addr sl_sa.sa_in_addr
#define LDAP_TCP_BUFFER
#ifdef LDAP_TCP_BUFFER
    int sl_tcp_rmem; /* custom TCP read buffer size */
    int sl_tcp_wmem; /* custom TCP write buffer size */
#endif
};

LDAP_END_DECL

#include "proto-slap.h"

#endif /* _SLAP_H_ */
