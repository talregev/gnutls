/*
 * Copyright (C) 2017 Red Hat, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

/* This tests whether a server will reject a client advertising
 * MD5 signature algorithms only */

#if defined(_WIN32) || !defined(ENABLE_SSL2)

/* socketpair isn't supported on Win32. */
int main(int argc, char **argv)
{
	exit(77);
}

#else

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <signal.h>
#include <limits.h>
#include <poll.h>
#include <errno.h>
#include <assert.h>

#include "utils.h"
#include "cert-common.h"

pid_t child;

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "%s |<%d>| %s", child ? "server" : "client", level,
		str);
}


static unsigned char tls1_hello[] = {
0x16, 0x03, 0x01, 0x01, 0x5E, 0x01, 0x00, 0x01, 0x5A, 0x03, 0x03, 0x59, 0x52, 0x41, 0x54, 0xD5,
0x52, 0x62, 0x63, 0x69, 0x1B, 0x46, 0xBE, 0x33, 0xCC, 0xC4, 0xC3, 0xB3, 0x6C, 0xCD, 0xEC, 0x96,
0xF7, 0x7A, 0xCA, 0xE9, 0xFB, 0x85, 0x95, 0x83, 0x51, 0xE4, 0x69, 0x00, 0x00, 0xD4, 0xC0, 0x30,
0xCC, 0xA8, 0xC0, 0x8B, 0xC0, 0x14, 0xC0, 0x28, 0xC0, 0x77, 0xC0, 0x2F, 0xC0, 0x8A, 0xC0, 0x13,
0xC0, 0x27, 0xC0, 0x76, 0xC0, 0x12, 0xC0, 0x2C, 0xC0, 0xAD, 0xCC, 0xA9, 0xC0, 0x87, 0xC0, 0x0A,
0xC0, 0x24, 0xC0, 0x73, 0xC0, 0x2B, 0xC0, 0xAC, 0xC0, 0x86, 0xC0, 0x09, 0xC0, 0x23, 0xC0, 0x72,
0xC0, 0x08, 0x00, 0x9D, 0xC0, 0x9D, 0xC0, 0x7B, 0x00, 0x35, 0x00, 0x3D, 0x00, 0x84, 0x00, 0xC0,
0x00, 0x9C, 0xC0, 0x9C, 0xC0, 0x7A, 0x00, 0x2F, 0x00, 0x3C, 0x00, 0x41, 0x00, 0xBA, 0x00, 0x0A,
0x00, 0x9F, 0xC0, 0x9F, 0xCC, 0xAA, 0xC0, 0x7D, 0x00, 0x39, 0x00, 0x6B, 0x00, 0x88, 0x00, 0xC4,
0x00, 0x9E, 0xC0, 0x9E, 0xC0, 0x7C, 0x00, 0x33, 0x00, 0x67, 0x00, 0x45, 0x00, 0xBE, 0x00, 0x16,
0x00, 0xA3, 0xC0, 0x81, 0x00, 0x38, 0x00, 0x6A, 0x00, 0x87, 0x00, 0xC3, 0x00, 0xA2, 0xC0, 0x80,
0x00, 0x32, 0x00, 0x40, 0x00, 0x44, 0x00, 0xBD, 0x00, 0x13, 0x00, 0xA9, 0xC0, 0xA5, 0xCC, 0xAB,
0xC0, 0x8F, 0x00, 0x8D, 0x00, 0xAF, 0xC0, 0x95, 0x00, 0xA8, 0xC0, 0xA4, 0xC0, 0x8E, 0x00, 0x8C,
0x00, 0xAE, 0xC0, 0x94, 0x00, 0x8B, 0x00, 0xAB, 0xC0, 0xA7, 0xCC, 0xAD, 0xC0, 0x91, 0x00, 0x91,
0x00, 0xB3, 0xC0, 0x97, 0x00, 0xAA, 0xC0, 0xA6, 0xC0, 0x90, 0x00, 0x90, 0x00, 0xB2, 0xC0, 0x96,
0x00, 0x8F, 0xCC, 0xAC, 0xC0, 0x36, 0xC0, 0x38, 0xC0, 0x9B, 0xC0, 0x35, 0xC0, 0x37, 0xC0, 0x9A,
0xC0, 0x34, 0x01, 0x00, 0x00, 0x5D, 0x00, 0x17, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x05,
0x00, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x11, 0x00, 0x00, 0x0E,
0x77, 0x77, 0x77, 0x2E, 0x67, 0x6F, 0x6F, 0x67, 0x6C, 0x65, 0x2E, 0x63, 0x6F, 0x6D, 0xFF, 0x01,
0x00, 0x01, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x08, 0x00, 0x06, 0x00, 0x17, 0x00,
0x18, 0x00, 0x19, 0x00, 0x0B, 0x00, 0x02, 0x01, 0x00, 0x00, 0x0D, 0x00, 0x16, 0x00, 0x14, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01};

static void client(int sd)
{
	char buf[1024];
	int ret;
	struct pollfd pfd;
	unsigned int timeout;

	/* send a TLS 1.x hello advertising RSA-MD5 */

	ret = send(sd, tls1_hello, sizeof(tls1_hello), 0);
	if (ret < 0)
		fail("error sending hello\n");

	pfd.fd = sd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	timeout = get_timeout();
	if (timeout > INT_MAX)
		fail("invalid timeout value\n");

	do {
		ret = poll(&pfd, 1, (int)timeout);
	} while (ret == -1 && errno == EINTR);

	if (ret == -1 || ret == 0) {
		fail("timeout waiting for reply\n");
	}

	success("sent hello\n");
	ret = recv(sd, buf, sizeof(buf), 0);
	if (ret < 0)
		fail("error receiving alert\n");

	success("received reply\n");

	if (ret < 7)
		fail("error in size of received alert\n");

	if (buf[0] != 0x15 || buf[1] != 0x03)
		fail("error in received alert data\n");

	success("all ok\n");

	close(sd);
}

static void server(int sd)
{
	gnutls_certificate_credentials_t x509_cred;
	gnutls_session_t session;
	int ret;

	/* this must be called once in the program
	 */
	global_init();

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(6);

	gnutls_certificate_allocate_credentials(&x509_cred);
	gnutls_certificate_set_x509_trust_mem(x509_cred, &ca3_cert,
					      GNUTLS_X509_FMT_PEM);

	gnutls_certificate_set_x509_key_mem(x509_cred, &server_ca3_localhost_cert,
					    &server_ca3_key,
					    GNUTLS_X509_FMT_PEM);

	gnutls_init(&session, GNUTLS_SERVER);

	/* avoid calling all the priority functions, since the defaults
	 * are adequate.
	 */
	assert(gnutls_priority_set_direct(session, "NORMAL:-VERS-ALL:+VERS-TLS1.2:-RSA", NULL)>=0);
	gnutls_handshake_set_timeout(session, get_timeout());

	gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, x509_cred);

	gnutls_transport_set_int(session, sd);
	do {
		ret = gnutls_handshake(session);
	} while(ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_AGAIN);

	if (ret != GNUTLS_E_NO_CIPHER_SUITES) {
		fail("server: Handshake succeeded unexpectedly: %s\n", gnutls_strerror(ret));
	}

	gnutls_alert_send_appropriate(session, ret);

	close(sd);
	gnutls_deinit(session);

	gnutls_certificate_free_credentials(x509_cred);

	gnutls_global_deinit();

	if (debug)
		success("server: finished\n");
}


void doit(void)
{
	int sockets[2];
	int err;

	signal(SIGPIPE, SIG_IGN);

	err = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
	if (err == -1) {
		perror("socketpair");
		fail("socketpair failed\n");
		return;
	}

	child = fork();
	if (child < 0) {
		perror("fork");
		fail("fork");
		return;
	}

	if (child) {
		int status;

		client(sockets[1]);
		wait(&status);
		check_wait_status(status);
	} else {
		server(sockets[0]);
		_exit(0);
	}
}

#endif				/* _WIN32 */
