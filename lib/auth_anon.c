/*
 *      Copyright (C) 2000,2001,2002 Nikos Mavroyanopoulos
 *
 * This file is part of GNUTLS.
 *
 *  The GNUTLS library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public   
 *  License as published by the Free Software Foundation; either 
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 * 
 */

/* This file contains the Anonymous Diffie Hellman key exchange part of
 * the anonymous authentication. The functions here are used in the
 * handshake.
 */

#include <gnutls_int.h>

#ifdef ENABLE_ANON

#include "gnutls_auth_int.h"
#include "gnutls_errors.h"
#include "gnutls_dh.h"
#include "auth_anon.h"
#include "gnutls_num.h"
#include "gnutls_mpi.h"
#include <gnutls_state.h>
#include <auth_dh_common.h>

static int gen_anon_server_kx( gnutls_session, opaque**);
static int proc_anon_client_kx( gnutls_session, opaque*, size_t);
static int proc_anon_server_kx( gnutls_session, opaque*, size_t);

const MOD_AUTH_STRUCT anon_auth_struct = {
	"ANON",
	NULL,
	NULL,
	gen_anon_server_kx,
	NULL,
	NULL,
	_gnutls_gen_dh_common_client_kx, /* this can be shared */
	NULL,
	NULL,

	NULL,
	NULL, /* certificate */
	proc_anon_server_kx,
	NULL,
	NULL,
	proc_anon_client_kx,
	NULL,
	NULL
};

static int gen_anon_server_kx( gnutls_session session, opaque** data) {
	GNUTLS_MPI g, p;
	int bits, ret;
	ANON_SERVER_AUTH_INFO info;
	const gnutls_anon_server_credentials cred;
	
	cred = _gnutls_get_cred(session->key, GNUTLS_CRD_ANON, NULL);
	if (cred == NULL) {
		gnutls_assert();
		return GNUTLS_E_INSUFICIENT_CREDENTIALS;
	}

	bits = _gnutls_dh_get_prime_bits( session);

	g = gnutls_get_dh_params( cred->dh_params, &p, bits);
	if (g==NULL || p==NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	if ( (ret=_gnutls_auth_info_set( session, GNUTLS_CRD_ANON, sizeof( ANON_SERVER_AUTH_INFO_INT), 1)) < 0) {
		gnutls_assert();
		return ret;
	}

	info = _gnutls_get_auth_info( session);	
	if ((ret=_gnutls_dh_set_prime_bits( session, _gnutls_mpi_get_nbits(p))) < 0) {
		gnutls_assert();
		return ret;
	}
	
	ret = _gnutls_dh_common_print_server_kx( session, g, p, data);
	_gnutls_mpi_release(&g);
	_gnutls_mpi_release(&p);

	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	return ret;
}


static int proc_anon_client_kx( gnutls_session session, opaque* data, size_t _data_size) 
{
const gnutls_anon_server_credentials cred;
int bits;
int ret;
GNUTLS_MPI p, g;
  	
	bits = _gnutls_dh_get_prime_bits( session);

	cred = _gnutls_get_cred(session->key, GNUTLS_CRD_ANON, NULL);
	if (cred == NULL) {
	        gnutls_assert();
	        return GNUTLS_E_INSUFICIENT_CREDENTIALS;
	}

	g = gnutls_get_dh_params( cred->dh_params, &p, bits);
	if (g == NULL || p == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	ret = _gnutls_proc_dh_common_client_kx( session, data, _data_size, g, p);

	_gnutls_mpi_release(&g);
	_gnutls_mpi_release(&p);
	
	return ret;

}

int proc_anon_server_kx( gnutls_session session, opaque* data, size_t _data_size) 
{

int ret;

	/* set auth_info */
	if ( (ret=_gnutls_auth_info_set( session, GNUTLS_CRD_ANON, sizeof( ANON_CLIENT_AUTH_INFO_INT), 1)) < 0) {
		gnutls_assert();
		return ret;
	}

	ret = _gnutls_proc_dh_common_server_kx( session, data, _data_size);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	return 0;
}

#endif /* ENABLE_ANON */
