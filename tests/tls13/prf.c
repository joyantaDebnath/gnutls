/*
 * Copyright (C) 2015-2018 Red Hat, Inc.
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#if !defined(__linux__) || !defined(__GNUC__)

int main(int argc, char **argv)
{
	exit(77);
}

#else

# include <string.h>
# include <sys/types.h>
# include <netinet/in.h>
# include <sys/socket.h>
# include <sys/wait.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <gnutls/gnutls.h>
# include <gnutls/crypto.h>

# include "cert-common.h"
# include "utils.h"

static void terminate(void);

/* This program tests whether the gnutls_prf() works as
 * expected.
 */

static void server_log_func(int level, const char *str)
{
	fprintf(stderr, "server|<%d>| %s", level, str);
}

static void client_log_func(int level, const char *str)
{
	fprintf(stderr, "client|<%d>| %s", level, str);
}

/* These are global */
static pid_t child;

static const
gnutls_datum_t hrnd = { (void *)
	    "\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
	32
};

static const
gnutls_datum_t hsrnd = { (void *)
	    "\x00\x05\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
	32
};

static int gnutls_rnd_works;

int __attribute__((visibility("protected")))
    gnutls_rnd(gnutls_rnd_level_t level, void *data, size_t len)
{
	gnutls_rnd_works = 1;

	memset(data, 0xff, len);

	/* Flip the first byte to avoid infinite loop in the RSA
	 * blinding code of Nettle */
	if (len > 0)
		memset(data, 0x0, 1);
	return 0;
}

static void dump(const char *name, const uint8_t * data, unsigned data_size)
{
	unsigned i;

	fprintf(stderr, "%s", name);
	for (i = 0; i < data_size; i++)
		fprintf(stderr, "\\x%.2x", (unsigned)data[i]);
	fprintf(stderr, "\n");
}

# define TRY(label_size, label, extra_size, extra, size, exp) \
	{ \
	ret = gnutls_prf_rfc5705(session, label_size, label, extra_size, extra, size, \
			 (void*)key_material); \
	if (ret < 0) { \
		fprintf(stderr, "gnutls_prf_rfc5705: error in %d\n", __LINE__); \
		gnutls_perror(ret); \
		exit(1); \
	} \
	if (memcmp(key_material, exp, size) != 0) { \
		fprintf(stderr, "gnutls_prf_rfc5705: output doesn't match for '%s'\n", label); \
		dump("got ", key_material, size); \
		dump("expected ", exp, size); \
		exit(1); \
	} \
	}

# define TRY_OLD(label_size, label, size, exp) \
	{ \
	ret = gnutls_prf(session, label_size, label, 0, 0, NULL, size, \
			 (void*)key_material); \
	if (ret < 0) { \
		fprintf(stderr, "gnutls_prf: error in %d\n", __LINE__); \
		gnutls_perror(ret); \
		exit(1); \
	} \
	if (memcmp(key_material, exp, size) != 0) { \
		fprintf(stderr, "gnutls_prf: output doesn't match for '%s'\n", label); \
		dump("got ", key_material, size); \
		dump("expected ", exp, size); \
		exit(1); \
	} \
	}

# define KEY_EXP_VALUE "\x28\x70\xa8\x34\xd4\x43\x85\xfd\x55\xe0\x13\x78\x75\xa3\x25\xa7\xfd\x0b\x6b\x68\x5d\x62\x72\x02\xdf\x3d\x79\xca\x55\xab\xea\x24\xf3\x4d"
# define HELLO_VALUE "\xd8\xcb\x72\x1e\x24\x2d\x79\x11\x41\x38\x05\x2b\x1b\x5d\x60\x12\x30\x0a\xf7\x1e\x23\x90\x4d\x64\xf8\xf5\x23\xea\xbf\xa3\x24"
# define CONTEXT_VALUE "\xe6\xc0\x57\xbe\xda\x28\x9c\xc7\xf6\x4f\xb6\x18\x92\xce\x10\xf6\xe1\x5e\xab\x10\xc8\xd1\x94\xf8\xac\xc7\x3e\x93\xde\x57\x12"
# define NULL_CONTEXT_VALUE "\xaf\xea\xd2\x64\xc9\x42\xbd\xe7\xdb\xf0\xd3\x16\x84\x39\xf3\xdb\x5d\x4f\x0e\x5e\x71\x1e\xc0\xd7\x23\xde\x8b\x1e\x80\xa1\xca"
static void check_prfs(gnutls_session_t session)
{
	unsigned char key_material[512];
	int ret;

	if (!gnutls_rnd_works) {
		fprintf(stderr,
			"gnutls_rnd() could not be overridden, see #584\n");
		exit(77);
	}

	TRY_OLD(13, "key expansion", 34, (uint8_t *) KEY_EXP_VALUE);
	TRY_OLD(6, "hello", 31, (uint8_t *) HELLO_VALUE);

	TRY(13, "key expansion", 0, NULL, 34, (uint8_t *) KEY_EXP_VALUE);
	TRY(6, "hello", 0, NULL, 31, (uint8_t *) HELLO_VALUE);
	TRY(7, "context", 5, "abcd\xfa", 31, (uint8_t *) CONTEXT_VALUE);
	TRY(12, "null-context", 0, "", 31, (uint8_t *) NULL_CONTEXT_VALUE);

	/* Try whether calling gnutls_prf() with non-null context or server-first
	 * param, will fail */
	ret =
	    gnutls_prf(session, 3, (void *)"xxx", 0, 3, (void *)"yyy", 16,
		       (void *)key_material);
	if (ret != GNUTLS_E_INVALID_REQUEST)
		fail("gnutls_prf: succeeded under TLS1.3!\n");

	ret =
	    gnutls_prf(session, 3, (void *)"xxx", 1, 0, NULL, 16,
		       (void *)key_material);
	if (ret != GNUTLS_E_INVALID_REQUEST)
		fail("gnutls_prf: succeeded under TLS1.3!\n");
}

static void client(int fd)
{
	gnutls_session_t session;
	int ret;
	gnutls_certificate_credentials_t clientx509cred;
	const char *err;
	/* Need to enable anonymous KX specifically. */

	global_init();

	if (debug) {
		gnutls_global_set_log_function(client_log_func);
		gnutls_global_set_log_level(4711);
	}

	gnutls_certificate_allocate_credentials(&clientx509cred);

	/* Initialize TLS session
	 */
	gnutls_init(&session, GNUTLS_CLIENT);

	/* Use default priorities */
	ret = gnutls_priority_set_direct(session,
					 "NONE:+VERS-TLS1.3:+AES-256-GCM:+AEAD:+SIGN-RSA-PSS-RSAE-SHA384:+GROUP-SECP256R1",
					 &err);
	if (ret < 0) {
		fail("client: priority set failed (%s): %s\n",
		     gnutls_strerror(ret), err);
		exit(1);
	}

	ret = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE,
				     clientx509cred);
	if (ret < 0)
		exit(1);

	gnutls_handshake_set_random(session, &hrnd);
	gnutls_transport_set_int(session, fd);

	/* Perform the TLS handshake
	 */
	do {
		ret = gnutls_handshake(session);
	}
	while (ret < 0 && gnutls_error_is_fatal(ret) == 0);

	if (ret < 0) {
		fail("client: Handshake failed: %s\n", strerror(ret));
		exit(1);
	} else {
		if (debug)
			success("client: Handshake was completed\n");
	}

	if (debug)
		success("client: TLS version is: %s\n",
			gnutls_protocol_get_name
			(gnutls_protocol_get_version(session)));

	ret = gnutls_cipher_get(session);
	if (ret != GNUTLS_CIPHER_AES_256_GCM) {
		fprintf(stderr, "negotiated unexpected cipher: %s\n",
			gnutls_cipher_get_name(ret));
		exit(1);
	}

	ret = gnutls_mac_get(session);
	if (ret != GNUTLS_MAC_AEAD) {
		fprintf(stderr, "negotiated unexpected mac: %s\n",
			gnutls_mac_get_name(ret));
		exit(1);
	}

	ret = gnutls_prf_hash_get(session);
	if (ret != GNUTLS_DIG_SHA384) {
		fprintf(stderr, "negotiated unexpected hash: %s\n",
			gnutls_digest_get_name(ret));
		exit(1);
	}

	check_prfs(session);

	gnutls_bye(session, GNUTLS_SHUT_WR);

	close(fd);

	gnutls_deinit(session);

	gnutls_certificate_free_credentials(clientx509cred);

	gnutls_global_deinit();
}

static void terminate(void)
{
	int status = 0;

	if (child) {
		kill(child, SIGTERM);
		wait(&status);
	}
	exit(1);
}

static void server(int fd)
{
	int ret;
	gnutls_session_t session;
	gnutls_certificate_credentials_t serverx509cred;

	/* this must be called once in the program
	 */
	global_init();

	if (debug) {
		gnutls_global_set_log_function(server_log_func);
		gnutls_global_set_log_level(4711);
	}

	gnutls_certificate_allocate_credentials(&serverx509cred);

	gnutls_init(&session, GNUTLS_SERVER);

	/* avoid calling all the priority functions, since the defaults
	 * are adequate.
	 */
	ret = gnutls_priority_set_direct(session,
					 "NORMAL:-VERS-ALL:+VERS-TLS1.3:-KX-ALL:-SIGN-ALL:+SIGN-RSA-PSS-RSAE-SHA384:-GROUP-ALL:+GROUP-SECP256R1",
					 NULL);
	if (ret < 0) {
		fail("server: priority set failed (%s)\n\n",
		     gnutls_strerror(ret));
		terminate();
	}

	gnutls_certificate_set_x509_key_mem(serverx509cred,
					    &server_cert, &server_key,
					    GNUTLS_X509_FMT_PEM);
	gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, serverx509cred);

	gnutls_handshake_set_random(session, &hsrnd);
	gnutls_transport_set_int(session, fd);

	do {
		ret = gnutls_handshake(session);
	}
	while (ret < 0 && gnutls_error_is_fatal(ret) == 0);
	if (ret < 0) {
		close(fd);
		gnutls_deinit(session);
		fail("server: Handshake has failed (%s)\n\n",
		     gnutls_strerror(ret));
		terminate();
	}
	if (debug)
		success("server: Handshake was completed\n");

	if (debug)
		success("server: TLS version is: %s\n",
			gnutls_protocol_get_name
			(gnutls_protocol_get_version(session)));

	check_prfs(session);

	/* do not wait for the peer to close the connection.
	 */
	gnutls_bye(session, GNUTLS_SHUT_WR);

	close(fd);
	gnutls_deinit(session);

	gnutls_certificate_free_credentials(serverx509cred);

	gnutls_global_deinit();

	if (debug)
		success("server: finished\n");
}

void doit(void)
{
	int fd[2];
	int ret;

	signal(SIGPIPE, SIG_IGN);

	ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
	if (ret < 0) {
		perror("socketpair");
		exit(1);
	}

	child = fork();
	if (child < 0) {
		perror("fork");
		fail("fork");
		exit(1);
	}

	if (child) {
		int status = 0;
		/* parent */

		server(fd[0]);
		wait(&status);
		check_wait_status(status);
	} else {
		close(fd[0]);
		client(fd[1]);
		exit(0);
	}
}

#endif				/* _WIN32 */
