/* $OpenLDAP$ */
/*
 * Copyright 1999-2003 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */
/*
 *	 Copyright 1999, John C. Quillan, All rights reserved.
 *	 Portions Copyright 2002, myinternet Limited. All rights reserved.
 *
 *	 Redistribution and use in source and binary forms are permitted only
 *	 as authorized by the OpenLDAP Public License.	A copy of this
 *	 license is available at http://www.OpenLDAP.org/license.html or
 *	 in file LICENSE in the top-level directory of the distribution.
 */

#include "portable.h"

#include <stdio.h>

#include "slap.h"
#ifdef HAVE_WIN32_ASPERL
#include "asperl_undefs.h"
#endif

#include <EXTERN.h>
#include <perl.h>

#include "perl_back.h"

/**********************************************************
 *
 * Search
 *
 **********************************************************/
int
perl_back_search(
	Operation *op,
	SlapReply *rs )
{
	PerlBackend *perl_back = (PerlBackend *)op->o_bd->be_private;
	int count ;
	AttributeName *an;
	Entry	*e;
	char *buf;
	int i;

	ldap_pvt_thread_mutex_lock( &perl_interpreter_mutex );	

	{
		dSP; ENTER; SAVETMPS;

		PUSHMARK(sp) ;
		XPUSHs( perl_back->pb_obj_ref );
		XPUSHs(sv_2mortal(newSVpv( op->o_req_ndn.bv_val , 0)));
		XPUSHs(sv_2mortal(newSViv( op->ors_scope )));
		XPUSHs(sv_2mortal(newSViv( op->ors_deref )));
		XPUSHs(sv_2mortal(newSViv( op->ors_slimit )));
		XPUSHs(sv_2mortal(newSViv( op->ors_tlimit )));
		XPUSHs(sv_2mortal(newSVpv( op->ors_filterstr.bv_val , 0)));
		XPUSHs(sv_2mortal(newSViv( op->ors_attrsonly )));

		for ( an = op->ors_attrs; an && an->an_name.bv_val; an++ ) {
			XPUSHs(sv_2mortal(newSVpv( an->an_name.bv_val , 0)));
		}
		PUTBACK;

#ifdef PERL_IS_5_6
		count = call_method("search", G_ARRAY );
#else
		count = perl_call_method("search", G_ARRAY );
#endif

		SPAGAIN;

		if (count < 1) {
			croak("Big trouble in back_search\n") ;
		}

		if ( count > 1 ) {
							 
			for ( i = 1; i < count; i++ ) {

				buf = POPp;

				if ( (e = str2entry( buf )) == NULL ) {
					Debug( LDAP_DEBUG_ANY, "str2entry(%s) failed\n", buf, 0, 0 );

				} else {
					int send_entry;

					if (perl_back->pb_filter_search_results)
						send_entry = (test_filter( op, e, op->ors_filter ) == LDAP_COMPARE_TRUE);
					else
						send_entry = 1;

					if (send_entry) {
						rs->sr_entry = e;
						rs->sr_attrs = op->ors_attrs;
						send_search_entry( op, rs );
					}

					entry_free( e );
				}
			}
		}

		/*
		 * We grab the return code last because the stack comes
		 * from perl in reverse order. 
		 *
		 * ex perl: return ( 0, $res_1, $res_2 );
		 *
		 * ex stack: <$res_2> <$res_1> <0>
		 */

		rs->sr_err = POPi;



		PUTBACK; FREETMPS; LEAVE;
	}

	ldap_pvt_thread_mutex_unlock( &perl_interpreter_mutex );	

	send_ldap_result( op, rs );

	return 0;
}

