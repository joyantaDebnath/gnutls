#!/bin/sh

# Copyright (C) 2014 Red Hat, Inc.
#
# Author: Nikos Mavrogiannopoulos
#
# This file is part of GnuTLS.
#
# GnuTLS is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 3 of the License, or (at
# your option) any later version.
#
# GnuTLS is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GnuTLS.  If not, see <https://www.gnu.org/licenses/>.

#set -e

: ${srcdir=.}
: ${CERTTOOL=../../src/certtool${EXEEXT}}
: ${DIFF=diff}

if ! test -x "${CERTTOOL}"; then
	exit 77
fi

if ! test -z "${VALGRIND}"; then
	VALGRIND="${LIBTOOL:-libtool} --mode=execute ${VALGRIND} --error-exitcode=15"
fi

TMPFILE=crq.$$.tmp
OUTFILE=out.$$.tmp
OUTFILE2=out2.$$.tmp

. ${srcdir}/../scripts/common.sh

skip_if_no_datefudge

${VALGRIND} "${CERTTOOL}" --inder --crq-info --infile "${srcdir}/data/csr-invalid.der" >"${OUTFILE}" 2>&1
rc=$?

# We're done.
if test "${rc}" != "0"; then
	echo "Invalid crq decoding failed"
	exit ${rc}
fi

grep "error: get_key_id" "${OUTFILE}" >/dev/null 2>&1
if test "$?" != "0"; then
	echo "crq decoding didn't fail as expected"
	exit 1
fi

rm -f "${OUTFILE}"

# check whether the honor_crq_extension option works
datefudge -s "2007-04-22" \
	"${CERTTOOL}" --generate-request \
		--load-privkey "${srcdir}/data/template-test.key" \
		--template "${srcdir}/templates/template-tlsfeature.tmpl" \
		--outfile $OUTFILE 2>/dev/null

${CERTTOOL} --crq-info --no-text --infile ${OUTFILE} --outfile ${TMPFILE}
rc=$?

if test "${rc}" != "0"; then
	echo "--no-text crq info failed 1"
	exit ${rc}
fi

if grep -v '^-----BEGIN [A-Z0-9 ]\+-----$' ${TMPFILE} | grep -v '^[A-Za-z0-9/+=]\+$' | grep -v '^-----END [A-Z0-9 ]\+-----$' ; then
	echo "--no-text crq info failed 2"
	exit 1
fi

datefudge -s "2007-04-22" \
	"${CERTTOOL}" --generate-certificate \
		--load-ca-privkey "${srcdir}/data/template-test.key" \
		--load-ca-certificate "${srcdir}/data/template-tlsfeature.pem" \
		--load-request="$OUTFILE" \
		--template "${srcdir}/templates/template-crq.tmpl" \
		--outfile "${OUTFILE2}" 2>/dev/null

${DIFF} "${srcdir}/data/template-crq.pem" "${OUTFILE2}" >/dev/null 2>&1
rc=$?

# We're done.
if test "${rc}" != "0"; then
	echo "Certificate request generation failed"
	echo $OUTFILE2
	exit ${rc}
fi

rm -f "${OUTFILE}" "${OUTFILE2}"


# Test interactive CRQ creation with very long input
cat >$TMPFILE <<__EOF__





super-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-long.com


super-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-very-long.com








N
Y
N
Y
N
N
N
N
N
N
__EOF__

setsid \
datefudge -s "2007-04-22" \
	"${CERTTOOL}" -q \
		--load-privkey "${srcdir}/data/template-test.key" \
		--outfile "${OUTFILE}" <$TMPFILE 2>/dev/null

${DIFF} --ignore-matching-lines "Algorithm Security Level" "${srcdir}/data/template-long-dns-crq.pem" "${OUTFILE}" >/dev/null 2>&1
rc=$?

# We're done.
if test "${rc}" != "0"; then
	echo "Certificate request generation with long DNS failed"
	echo $OUTFILE
	exit ${rc}
fi

# check whether the generation with extension works
datefudge -s "2007-04-22" \
	"${CERTTOOL}" --generate-request \
		--load-privkey "${srcdir}/data/template-test.key" \
		--template "${srcdir}/templates/arb-extensions.tmpl" \
		--outfile $OUTFILE 2>/dev/null
rc=$?

if test "${rc}" != "0"; then
	echo "add_extension crq failed"
	exit ${rc}
fi

${DIFF} --ignore-matching-lines "Algorithm Security Level" "${srcdir}/data/arb-extensions.csr" "${OUTFILE}" >/dev/null 2>&1
rc=$?

if test "${rc}" != "0"; then
	echo "Certificate request generation with explicit extensions failed"
	exit ${rc}
fi

# Generate certificate from CRQ with no explicit extensions
datefudge -s "2007-04-22" \
	"${CERTTOOL}" --generate-certificate \
		--load-ca-privkey "${srcdir}/../../doc/credentials/x509/ca-key.pem" \
		--load-ca-certificate "${srcdir}/../../doc/credentials/x509/ca.pem" \
		--load-request "${srcdir}/data/arb-extensions.csr" \
		--template "${srcdir}/templates/template-no-ca.tmpl" \
		--outfile "${OUTFILE}" 2>/dev/null
rc=$?

if test "${rc}" != "0"; then
	echo "generate certificate with crq failed"
	exit ${rc}
fi

${DIFF} --ignore-matching-lines "Algorithm Security Level" "${srcdir}/data/crq-cert-no-ca.pem" "${OUTFILE}" >/dev/null 2>&1
rc=$?

if test "${rc}" != "0"; then
	echo "Certificate from request generation failed"
	exit ${rc}
fi

# Generate certificate from CRQ with CRQ extensions
datefudge -s "2007-04-22" \
	"${CERTTOOL}" --generate-certificate \
		--load-ca-privkey "${srcdir}/../../doc/credentials/x509/ca-key.pem" \
		--load-ca-certificate "${srcdir}/../../doc/credentials/x509/ca.pem" \
		--load-request "${srcdir}/data/arb-extensions.csr" \
		--template "${srcdir}/templates/template-no-ca-honor.tmpl" \
		--outfile "${OUTFILE}" 2>/dev/null
rc=$?

if test "${rc}" != "0"; then
	echo "generate certificate with crq failed"
	exit ${rc}
fi

${DIFF} --ignore-matching-lines "Algorithm Security Level" "${srcdir}/data/crq-cert-no-ca-honor.pem" "${OUTFILE}" >/dev/null 2>&1
rc=$?

if test "${rc}" != "0"; then
	echo "Certificate from request generation with honor flag failed"
	exit ${rc}
fi

# Generate certificate from CRQ with explicit extensions
datefudge -s "2007-04-22" \
	"${CERTTOOL}" --generate-certificate \
		--load-ca-privkey "${srcdir}/../../doc/credentials/x509/ca-key.pem" \
		--load-ca-certificate "${srcdir}/../../doc/credentials/x509/ca.pem" \
		--load-request "${srcdir}/data/arb-extensions.csr" \
		--template "${srcdir}/templates/template-no-ca-explicit.tmpl" \
		--outfile "${OUTFILE}" 2>/dev/null
rc=$?

if test "${rc}" != "0"; then
	echo "generate certificate with crq failed"
	exit ${rc}
fi

${DIFF} --ignore-matching-lines "Algorithm Security Level" "${srcdir}/data/crq-cert-no-ca-explicit.pem" "${OUTFILE}" >/dev/null 2>&1
rc=$?

if test "${rc}" != "0"; then
	echo "Certificate from request generation with explicit extensions failed"
	exit ${rc}
fi


rm -f "${OUTFILE}" "${OUTFILE2}" "${TMPFILE}"

exit 0
