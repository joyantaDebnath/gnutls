/*
 * Copyright (C) 2005-2012 Free Software Foundation, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */

/* This file contains the PSK Diffie-Hellman key exchange part of the
 * PSK authentication.  The functions here are used in the handshake.
 */

#include <gnutls_int.h>

#ifdef ENABLE_PSK

/* Contains PSK code for DHE and ECDHE
 */

#include "gnutls_auth.h"
#include "gnutls_errors.h"
#include "gnutls_dh.h"
#include <auth/psk.h>
#include "gnutls_num.h"
#include "gnutls_mpi.h"
#include <gnutls_state.h>
#include <auth/dh_common.h>
#include <auth/ecdhe.h>
#include <gnutls_datum.h>
#include <auth/psk_passwd.h>

static int
proc_ecdhe_psk_server_kx(gnutls_session_t session, uint8_t * data,
			 size_t _data_size);
static int gen_dhe_psk_server_kx(gnutls_session_t, gnutls_buffer_st *);
static int gen_dhe_psk_client_kx(gnutls_session_t, gnutls_buffer_st *);
static int gen_ecdhe_psk_client_kx(gnutls_session_t, gnutls_buffer_st *);
static int proc_ecdhe_psk_client_kx(gnutls_session_t, uint8_t *, size_t);
static int proc_dhe_psk_server_kx(gnutls_session_t, uint8_t *, size_t);
static int gen_ecdhe_psk_server_kx(gnutls_session_t session,
				   gnutls_buffer_st * data);
static int proc_dhe_psk_client_kx(gnutls_session_t session, uint8_t * data,
				  size_t _data_size);
#ifdef ENABLE_DHE
const mod_auth_st dhe_psk_auth_struct = {
	"DHE PSK",
	NULL,
	NULL,
	gen_dhe_psk_server_kx,
	gen_dhe_psk_client_kx,
	NULL,
	NULL,

	NULL,
	NULL,			/* certificate */
	proc_dhe_psk_server_kx,
	proc_dhe_psk_client_kx,
	NULL,
	NULL
};
#endif

#ifdef ENABLE_ECDHE
const mod_auth_st ecdhe_psk_auth_struct = {
	"ECDHE PSK",
	NULL,
	NULL,
	gen_ecdhe_psk_server_kx,
	gen_ecdhe_psk_client_kx,
	NULL,
	NULL,

	NULL,
	NULL,			/* certificate */
	proc_ecdhe_psk_server_kx,
	proc_ecdhe_psk_client_kx,
	NULL,
	NULL
};
#endif

static int
gen_ecdhe_psk_client_kx(gnutls_session_t session, gnutls_buffer_st * data)
{
	int ret, free;
	gnutls_psk_client_credentials_t cred;
	gnutls_datum_t username, key;

	cred = (gnutls_psk_client_credentials_t)
	    _gnutls_get_cred(session, GNUTLS_CRD_PSK, NULL);

	if (cred == NULL)
		return
		    gnutls_assert_val(GNUTLS_E_INSUFFICIENT_CREDENTIALS);

	ret = _gnutls_find_psk_key(session, cred, &username, &key, &free);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret =
	    _gnutls_buffer_append_data_prefix(data, 16, username.data,
					      username.size);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	/* The PSK key is set in there */
	ret = _gnutls_gen_ecdh_common_client_kx_int(session, data, &key);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = data->length;

      cleanup:
	if (free) {
		_gnutls_free_datum(&username);
		_gnutls_free_temp_key_datum(&key);
	}

	return ret;
}

static int
gen_dhe_psk_client_kx(gnutls_session_t session, gnutls_buffer_st * data)
{
	int ret, free;
	gnutls_psk_client_credentials_t cred;
	gnutls_datum_t username, key;

	cred = (gnutls_psk_client_credentials_t)
	    _gnutls_get_cred(session, GNUTLS_CRD_PSK, NULL);

	if (cred == NULL)
		return
		    gnutls_assert_val(GNUTLS_E_INSUFFICIENT_CREDENTIALS);

	ret = _gnutls_find_psk_key(session, cred, &username, &key, &free);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret =
	    _gnutls_buffer_append_data_prefix(data, 16, username.data,
					      username.size);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	/* The PSK key is set in there */
	ret = _gnutls_gen_dh_common_client_kx_int(session, data, &key);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = data->length;

      cleanup:
	if (free) {
		_gnutls_free_datum(&username);
		_gnutls_free_temp_key_datum(&key);
	}

	return ret;
}

static int
gen_dhe_psk_server_kx(gnutls_session_t session, gnutls_buffer_st * data)
{
	bigint_t g, p;
	const bigint_t *mpis;
	int ret;
	gnutls_dh_params_t dh_params;
	gnutls_psk_server_credentials_t cred;

	cred = (gnutls_psk_server_credentials_t)
	    _gnutls_get_cred(session, GNUTLS_CRD_PSK, NULL);
	if (cred == NULL) {
		gnutls_assert();
		return GNUTLS_E_INSUFFICIENT_CREDENTIALS;
	}

	dh_params =
	    _gnutls_get_dh_params(cred->dh_params, cred->params_func,
				  session);
	mpis = _gnutls_dh_params_to_mpi(dh_params);
	if (mpis == NULL) {
		gnutls_assert();
		return GNUTLS_E_NO_TEMPORARY_DH_PARAMS;
	}

	p = mpis[0];
	g = mpis[1];

	if ((ret =
	     _gnutls_auth_info_set(session, GNUTLS_CRD_PSK,
				   sizeof(psk_auth_info_st), 1)) < 0) {
		gnutls_assert();
		return ret;
	}

	_gnutls_dh_set_group(session, g, p);

	ret = _gnutls_buffer_append_prefix(data, 16, 0);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret = _gnutls_set_dh_pk_params(session, g, p, dh_params->q_bits);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret =
	    _gnutls_dh_common_print_server_kx(session, data);
	if (ret < 0)
		gnutls_assert();

	return ret;
}

static int
gen_ecdhe_psk_server_kx(gnutls_session_t session, gnutls_buffer_st * data)
{
	int ret;

	if ((ret =
	     _gnutls_auth_info_set(session, GNUTLS_CRD_PSK,
				   sizeof(psk_auth_info_st), 1)) < 0) {
		gnutls_assert();
		return ret;
	}

	ret = _gnutls_buffer_append_prefix(data, 16, 0);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret = _gnutls_ecdh_common_print_server_kx(session, data,
						  _gnutls_session_ecc_curve_get
						  (session));
	if (ret < 0)
		gnutls_assert();

	return ret;
}


static int
proc_dhe_psk_client_kx(gnutls_session_t session, uint8_t * data,
		       size_t _data_size)
{
	int ret;
	bigint_t p, g;
	gnutls_dh_params_t dh_params;
	const bigint_t *mpis;
	gnutls_datum_t psk_key;
	gnutls_psk_server_credentials_t cred;
	psk_auth_info_t info;
	gnutls_datum_t username;
	ssize_t data_size = _data_size;

	cred = (gnutls_psk_server_credentials_t)
	    _gnutls_get_cred(session, GNUTLS_CRD_PSK, NULL);

	if (cred == NULL) {
		gnutls_assert();
		return GNUTLS_E_INSUFFICIENT_CREDENTIALS;
	}

	if ((ret =
	     _gnutls_auth_info_set(session, GNUTLS_CRD_PSK,
				   sizeof(psk_auth_info_st), 1)) < 0) {
		gnutls_assert();
		return ret;
	}

	dh_params =
	    _gnutls_get_dh_params(cred->dh_params, cred->params_func,
				  session);
	mpis = _gnutls_dh_params_to_mpi(dh_params);
	if (mpis == NULL) {
		gnutls_assert();
		return GNUTLS_E_NO_TEMPORARY_DH_PARAMS;
	}

	p = mpis[0];
	g = mpis[1];

	DECR_LEN(data_size, 2);
	username.size = _gnutls_read_uint16(&data[0]);

	DECR_LEN(data_size, username.size);

	username.data = &data[2];

	/* copy the username to the auth info structures
	 */
	info = _gnutls_get_auth_info(session, GNUTLS_CRD_PSK);
	if (info == NULL) {
		gnutls_assert();
		return GNUTLS_E_INTERNAL_ERROR;
	}

	if (username.size > MAX_USERNAME_SIZE) {
		gnutls_assert();
		return GNUTLS_E_ILLEGAL_SRP_USERNAME;
	}

	memcpy(info->username, username.data, username.size);
	info->username[username.size] = 0;

	/* Adjust the data */
	data += username.size + 2;

	ret =
	    _gnutls_psk_pwd_find_entry(session, info->username, &psk_key);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret = _gnutls_proc_dh_common_client_kx(session, data, data_size,
					       g, p, &psk_key);

	_gnutls_free_key_datum(&psk_key);

	return ret;

}

static int
proc_ecdhe_psk_client_kx(gnutls_session_t session, uint8_t * data,
			 size_t _data_size)
{
	int ret;
	gnutls_psk_server_credentials_t cred;
	gnutls_datum_t psk_key;
	psk_auth_info_t info;
	gnutls_datum_t username;
	ssize_t data_size = _data_size;

	cred = (gnutls_psk_server_credentials_t)
	    _gnutls_get_cred(session, GNUTLS_CRD_PSK, NULL);

	if (cred == NULL) {
		gnutls_assert();
		return GNUTLS_E_INSUFFICIENT_CREDENTIALS;
	}

	if ((ret =
	     _gnutls_auth_info_set(session, GNUTLS_CRD_PSK,
				   sizeof(psk_auth_info_st), 1)) < 0) {
		gnutls_assert();
		return ret;
	}

	DECR_LEN(data_size, 2);
	username.size = _gnutls_read_uint16(&data[0]);

	DECR_LEN(data_size, username.size);

	username.data = &data[2];

	/* copy the username to the auth info structures
	 */
	info = _gnutls_get_auth_info(session, GNUTLS_CRD_PSK);
	if (info == NULL) {
		gnutls_assert();
		return GNUTLS_E_INTERNAL_ERROR;
	}


	if (username.size > MAX_USERNAME_SIZE) {
		gnutls_assert();
		return GNUTLS_E_ILLEGAL_SRP_USERNAME;
	}

	memcpy(info->username, username.data, username.size);
	info->username[username.size] = 0;

	/* Adjust the data */
	data += username.size + 2;

	/* should never fail. It will always return a key even if it is
	 * a random one */
	ret =
	    _gnutls_psk_pwd_find_entry(session, info->username, &psk_key);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret = _gnutls_proc_ecdh_common_client_kx(session, data, data_size,
						 _gnutls_session_ecc_curve_get
						 (session), &psk_key);

	_gnutls_free_key_datum(&psk_key);

	return ret;
}

static int
proc_dhe_psk_server_kx(gnutls_session_t session, uint8_t * data,
		       size_t _data_size)
{

	int ret, psk_size;
	ssize_t data_size = _data_size;

	/* set auth_info */
	if ((ret =
	     _gnutls_auth_info_set(session, GNUTLS_CRD_PSK,
				   sizeof(psk_auth_info_st), 1)) < 0) {
		gnutls_assert();
		return ret;
	}

	DECR_LEN(data_size, 2);
	psk_size = _gnutls_read_uint16(data);
	DECR_LEN(data_size, psk_size);
	data += 2 + psk_size;

	ret = _gnutls_proc_dh_common_server_kx(session, data, data_size);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	return 0;
}

static int
proc_ecdhe_psk_server_kx(gnutls_session_t session, uint8_t * data,
			 size_t _data_size)
{

	int ret, psk_size;
	ssize_t data_size = _data_size;

	/* set auth_info */
	if ((ret =
	     _gnutls_auth_info_set(session, GNUTLS_CRD_PSK,
				   sizeof(psk_auth_info_st), 1)) < 0) {
		gnutls_assert();
		return ret;
	}

	DECR_LEN(data_size, 2);
	psk_size = _gnutls_read_uint16(data);
	DECR_LEN(data_size, psk_size);
	data += 2 + psk_size;

	ret = _gnutls_proc_ecdh_common_server_kx(session, data, data_size);
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	return 0;
}

#endif				/* ENABLE_PSK */
