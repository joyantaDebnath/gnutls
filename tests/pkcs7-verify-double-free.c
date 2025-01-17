/*
 * Copyright (C) 2022 Red Hat, Inc.
 *
 * Author: Zoltan Fridrich
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GnuTLS. If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <gnutls/pkcs7.h>
#include <gnutls/x509.h>

#include "utils.h"

static char rca_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDCjCCAfKgAwIBAgIBATANBgkqhkiG9w0BAQsFADAVMRMwEQYDVQQKDApFeGFt\n"
    "cGxlIENBMCAXDTE3MDcyMTE0NDMzNloYDzIyMjIwNzIxMTQ0MzM2WjAVMRMwEQYD\n"
    "VQQKDApFeGFtcGxlIENBMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA\n"
    "v8hnKPJ/IA0SQB/A/a0Uh+npZ67vsgIMrtTQo0r0kJkmkBz5323xO3DVuJfB3QmX\n"
    "v9zvoeCQLuDvWar5Aixfxgm6s5Q+yPvJj9t3NebDrU+Y4+qyewBIJUF8EF/5iBPC\n"
    "ZHONmzbfIRWvQWGGgb2CRcOHp2J7AY/QLB6LsWPaLjs/DHva28Q13JaTTHIpdu8v\n"
    "t6vHr0nXf66DN4MvtoF3N+o+v3snJCMsfXOqASi4tbWR7gtOfCfiz9uBjh0W2Dut\n"
    "/jclBQkJkLe6esNSM+f4YiOpctVDjmfj8yoHCp394vt0wFqhG38wsTFAyVP6qIcf\n"
    "5zoSu9ovEt2cTkhnZHjiiwIDAQABo2MwYTAPBgNVHRMBAf8EBTADAQH/MA4GA1Ud\n"
    "DwEB/wQEAwIBBjAdBgNVHQ4EFgQUhjeO6Uc5imbjOl2I2ltVA27Hu9YwHwYDVR0j\n"
    "BBgwFoAUhjeO6Uc5imbjOl2I2ltVA27Hu9YwDQYJKoZIhvcNAQELBQADggEBAD+r\n"
    "i/7FsbG0OFKGF2+JOnth6NjJQcMfM8LiglqAuBUijrv7vltoZ0Z3FJH1Vi4OeMXn\n"
    "l7X/9tWUve0uFl75MfjDrf0+lCEdYRY1LCba2BrUgpbbkLywVUdnbsvndehegCgS\n"
    "jss2/zys3Hlo3ZaHlTMQ/NQ4nrxcxkjOvkZSEOqgxJTLpzm6pr7YUts4k6c6lNiB\n"
    "FSiJiDzsJCmWR9C3fBbUlfDfTJYGN3JwqX270KchXDElo8gNoDnF7jBMpLFFSEKm\n"
    "MyfbNLX/srh+CEfZaN/OZV4A3MQ0L8vQEp6M4CJhvRLIuMVabZ2coJ0AzystrOMU\n"
    "LirBWjg89RoAjFQ7bTE=\n" "-----END CERTIFICATE-----\n";

static char ca_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDFzCCAf+gAwIBAgIBAjANBgkqhkiG9w0BAQsFADAVMRMwEQYDVQQKDApFeGFt\n"
    "cGxlIENBMCAXDTE3MDcyMTE0NDQzNFoYDzIyMjIwNzIxMTQ0NDM0WjAiMSAwHgYD\n"
    "VQQKDBdFeGFtcGxlIGludGVybWVkaWF0ZSBDQTCCASIwDQYJKoZIhvcNAQEBBQAD\n"
    "ggEPADCCAQoCggEBAKb9ACB8u//sP6MfNU1OsVw68xz3eTPLgKxS0vpqexm6iGVg\n"
    "ug/o9uYRLzqiEukv/eyz9WzHmY7sqlOJjOFdv92+SaNg79Jc51WHPFXgea4/qyfr\n"
    "4y14PGs0SNxm6T44sXurUs7cXydQVUgnq2VCaWFOTUdxXoAWkV8r8GaUoPD/klVz\n"
    "RqxSZVETmX1XBKhsMnnov41kRwVph2C+VfUspsbaUZaz/o/S1/nokhXRACzKsMBr\n"
    "obqiGxbY35uVzsmbAW5ErhQz98AWJL3Bub1fsEMXg6OEMmPH4AtX888dTIYZNw0E\n"
    "bUIESspz1kjJQTtVQDHTprhwz16YiSVeUonlLgMCAwEAAaNjMGEwDwYDVR0TAQH/\n"
    "BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYEFPBjxDWjMhjXERirKF9O\n"
    "o/5Cllc5MB8GA1UdIwQYMBaAFIY3julHOYpm4zpdiNpbVQNux7vWMA0GCSqGSIb3\n"
    "DQEBCwUAA4IBAQCTm+vv3hBa6lL5IT+Fw8aTxQ2Ne7mZ5oyazhvXYwwfKNMX3SML\n"
    "W2JdPaL64ZwbxxxYvW401o5Z0CEgru3YFrsqB/hEdl0Uf8UWWJmE1rRa+miTmbjt\n"
    "lrLNCWdrs6CiwvsPITTHg7jevB4KyZYsTSxQFcyr3N3xF+6EmOTC4IkhPPnXYXcp\n"
    "248ih+WOavSYoRvzgB/Dip1WnPYU2mfIV3O8JReRryngA0TzWCLPLUoWR3R4jwtC\n"
    "+1uSLoqaenz3qv3F1WEbke37az9YJuXx/5D8CqFQiZ62TUUtI6fYd8mkMBM4Qfh6\n"
    "NW9XrCkI9wlpL5K9HllhuW0BhKeJkuPpyQ2p\n" "-----END CERTIFICATE-----\n";

static char ee_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDIjCCAgqgAwIBAgIBATANBgkqhkiG9w0BAQsFADAiMSAwHgYDVQQKDBdFeGFt\n"
    "cGxlIGludGVybWVkaWF0ZSBDQTAgFw0yMjA3MjExNDQ1MzdaGA8yMjIyMDcyMTE0\n"
    "NDUzN1owFTETMBEGA1UEAwwKSm9obiBTbWl0aDCCASIwDQYJKoZIhvcNAQEBBQAD\n"
    "ggEPADCCAQoCggEBAMb1uuxppBFY+WVD45iyHUq7DkIJNNOI/JRaybVJfPktWq2E\n"
    "eNe7XhV05KKnqZTbDO2iYqNHqGhZ8pz/IstDRTZP3z/q1vXTG0P9Gx28rEy5TaUY\n"
    "QjtD+ZoFUQm0ORMDBjd8jikqtJ87hKeuOPMH4rzdydotMaPQSm7KLzHBGBr6gg7z\n"
    "g1IxPWkhMyHapoMqqrhjwjzoTY97UIXpZTEoIA+KpEC8f9CciBtL0i1MPBjWozB6\n"
    "Jma9q5iEwZXuRr3cnPYeIPlK2drgDZCMuSFcYiT8ApLw5OhKqY1m2EvfZ2ox2s9R\n"
    "68/HzYdPi3kZwiNEtlBvMlpt5yKBJAflp76d7DkCAwEAAaNuMGwwCwYDVR0PBAQD\n"
    "AgWgMB0GA1UdJQQWMBQGCCsGAQUFBwMCBggrBgEFBQcDBDAdBgNVHQ4EFgQUc+Mi\n"
    "kr8WMCk00SQo+P2iggp/oQkwHwYDVR0jBBgwFoAU8GPENaMyGNcRGKsoX06j/kKW\n"
    "VzkwDQYJKoZIhvcNAQELBQADggEBAKU9+CUR0Jcfybd1+8Aqgh1RH96yQygnVuyt\n"
    "Na9rFz4fM3ij9tGXDHXrkZw8bW1dWLU9quu8zeTxKxc3aiDIw739Alz0tukttDo7\n"
    "dW7YqIb77zsIsWB9p7G9dlxT6ieUy+5IKk69BbeK8KR0vAciAG4KVQxPhuPy/LGX\n"
    "PzqlJIJ4h61s3UOroReHPB1keLZgpORqrvtpClOmABH9TLFRJA/WFg8Q2XYB/p0x\n"
    "l/pWiaoBC+8wK9cDoMUK5yOwXeuCLffCb+UlAD0+z/qxJ2pisE8E9X8rRKRrWI+i\n"
    "G7LtJCEn86EQK8KuRlJxKgj8lClZhoULB0oL4jbblBuNow9WRmM=\n"
    "-----END CERTIFICATE-----\n";

static char msg_pem[] =
    "-----BEGIN PKCS7-----\n"
    "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\n"
    "-----END PKCS7-----\n";

const gnutls_datum_t rca_datum = { (void *)rca_pem, sizeof(rca_pem) - 1 };
const gnutls_datum_t ca_datum = { (void *)ca_pem, sizeof(ca_pem) - 1 };
const gnutls_datum_t ee_datum = { (void *)ee_pem, sizeof(ee_pem) - 1 };
const gnutls_datum_t msg_datum = { (void *)msg_pem, sizeof(msg_pem) - 1 };

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "%s |<%d>| %s", "err", level, str);
}

#define CHECK(X)\
{\
	r = X;\
	if (r < 0)\
		fail("error in %d: %s\n", __LINE__, gnutls_strerror(r));\
}\

void doit(void)
{
	int r;
	gnutls_x509_crt_t rca_cert = NULL;
	gnutls_x509_crt_t ca_cert = NULL;
	gnutls_x509_crt_t ee_cert = NULL;
	gnutls_x509_trust_list_t tlist = NULL;
	gnutls_pkcs7_t pkcs7 = NULL;
	gnutls_datum_t data = { (unsigned char *)"xxx", 3 };

	if (debug) {
		gnutls_global_set_log_function(tls_log_func);
		gnutls_global_set_log_level(4711);
	}
	// Import certificates
	CHECK(gnutls_x509_crt_init(&rca_cert));
	CHECK(gnutls_x509_crt_import
	      (rca_cert, &rca_datum, GNUTLS_X509_FMT_PEM));
	CHECK(gnutls_x509_crt_init(&ca_cert));
	CHECK(gnutls_x509_crt_import(ca_cert, &ca_datum, GNUTLS_X509_FMT_PEM));
	CHECK(gnutls_x509_crt_init(&ee_cert));
	CHECK(gnutls_x509_crt_import(ee_cert, &ee_datum, GNUTLS_X509_FMT_PEM));

	// Setup trust store
	CHECK(gnutls_x509_trust_list_init(&tlist, 0));
	CHECK(gnutls_x509_trust_list_add_named_crt
	      (tlist, rca_cert, "rca", 3, 0));
	CHECK(gnutls_x509_trust_list_add_named_crt(tlist, ca_cert, "ca", 2, 0));
	CHECK(gnutls_x509_trust_list_add_named_crt(tlist, ee_cert, "ee", 2, 0));

	// Setup pkcs7 structure
	CHECK(gnutls_pkcs7_init(&pkcs7));
	CHECK(gnutls_pkcs7_import(pkcs7, &msg_datum, GNUTLS_X509_FMT_PEM));

	// Signature verification
	gnutls_pkcs7_verify(pkcs7, tlist, NULL, 0, 0, &data, 0);

	gnutls_x509_crt_deinit(rca_cert);
	gnutls_x509_crt_deinit(ca_cert);
	gnutls_x509_crt_deinit(ee_cert);
	gnutls_x509_trust_list_deinit(tlist, 0);
	gnutls_pkcs7_deinit(pkcs7);
}
