/*
 * Copyright (C) 2011-2012 Free Software Foundation, Inc.
 * Copyright (C) 2017 Red Hat, Inc.
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 *
 */

#include "gnutls_int.h"
#include <algorithms.h>
#include "errors.h"
#include <dh.h>
#include <state.h>
#include <x509/common.h>
#include <auth/cert.h>
#include <auth/anon.h>
#include <auth/psk.h>
#include <ext/safe_renegotiation.h>

#ifndef ENABLE_SSL3
#define GNUTLS_SSL3 GNUTLS_TLS1
#endif

/* Cipher SUITES */
#define ENTRY(name, canonical_name, block_algorithm, kx_algorithm,          \
	      mac_algorithm, min_version, dtls_version)                     \
	{                                                                   \
#name, name, canonical_name, block_algorithm, kx_algorithm, \
			mac_algorithm, min_version, GNUTLS_TLS1_2,          \
			dtls_version, GNUTLS_DTLS1_2, GNUTLS_MAC_SHA256     \
	}
#define ENTRY_PRF(name, canonical_name, block_algorithm, kx_algorithm,      \
		  mac_algorithm, min_version, dtls_version, prf)            \
	{                                                                   \
#name, name, canonical_name, block_algorithm, kx_algorithm, \
			mac_algorithm, min_version, GNUTLS_TLS1_2,          \
			dtls_version, GNUTLS_DTLS1_2, prf                   \
	}
#define ENTRY_TLS13(name, canonical_name, block_algorithm, min_version, prf) \
	{                                                                    \
#name, name, canonical_name, block_algorithm, 0,             \
			GNUTLS_MAC_AEAD, min_version, GNUTLS_TLS1_3,         \
			GNUTLS_VERSION_UNKNOWN, GNUTLS_VERSION_UNKNOWN, prf  \
	}

/* TLS 1.3 ciphersuites */
#define GNUTLS_AES_128_GCM_SHA256 \
	{                         \
		0x13, 0x01        \
	}
#define GNUTLS_AES_256_GCM_SHA384 \
	{                         \
		0x13, 0x02        \
	}
#define GNUTLS_CHACHA20_POLY1305_SHA256 \
	{                               \
		0x13, 0x03              \
	}
#define GNUTLS_AES_128_CCM_SHA256 \
	{                         \
		0x13, 0x04        \
	}
#define GNUTLS_AES_128_CCM_8_SHA256 \
	{                           \
		0x13, 0x05          \
	}

/* RSA with NULL cipher and MD5 MAC
 * for test purposes.
 */
#define GNUTLS_RSA_NULL_MD5 \
	{                   \
		0x00, 0x01  \
	}
#define GNUTLS_RSA_NULL_SHA1 \
	{                    \
		0x00, 0x02   \
	}
#define GNUTLS_RSA_NULL_SHA256 \
	{                      \
		0x00, 0x3B     \
	}

/* ANONymous cipher suites.
 */

#define GNUTLS_DH_ANON_3DES_EDE_CBC_SHA1 \
	{                                \
		0x00, 0x1B               \
	}
#define GNUTLS_DH_ANON_ARCFOUR_128_MD5 \
	{                              \
		0x00, 0x18             \
	}

/* rfc3268: */
#define GNUTLS_DH_ANON_AES_128_CBC_SHA1 \
	{                               \
		0x00, 0x34              \
	}
#define GNUTLS_DH_ANON_AES_256_CBC_SHA1 \
	{                               \
		0x00, 0x3A              \
	}

/* rfc4132 */
#define GNUTLS_DH_ANON_CAMELLIA_128_CBC_SHA1 \
	{                                    \
		0x00, 0x46                   \
	}
#define GNUTLS_DH_ANON_CAMELLIA_256_CBC_SHA1 \
	{                                    \
		0x00, 0x89                   \
	}

/* rfc5932 */
#define GNUTLS_RSA_CAMELLIA_128_CBC_SHA256 \
	{                                  \
		0x00, 0xBA                 \
	}
#define GNUTLS_DHE_DSS_CAMELLIA_128_CBC_SHA256 \
	{                                      \
		0x00, 0xBD                     \
	}
#define GNUTLS_DHE_RSA_CAMELLIA_128_CBC_SHA256 \
	{                                      \
		0x00, 0xBE                     \
	}
#define GNUTLS_DH_ANON_CAMELLIA_128_CBC_SHA256 \
	{                                      \
		0x00, 0xBF                     \
	}
#define GNUTLS_RSA_CAMELLIA_256_CBC_SHA256 \
	{                                  \
		0x00, 0xC0                 \
	}
#define GNUTLS_DHE_DSS_CAMELLIA_256_CBC_SHA256 \
	{                                      \
		0x00, 0xC3                     \
	}
#define GNUTLS_DHE_RSA_CAMELLIA_256_CBC_SHA256 \
	{                                      \
		0x00, 0xC4                     \
	}
#define GNUTLS_DH_ANON_CAMELLIA_256_CBC_SHA256 \
	{                                      \
		0x00, 0xC5                     \
	}

/* rfc6367 */
#define GNUTLS_ECDHE_ECDSA_CAMELLIA_128_CBC_SHA256 \
	{                                          \
		0xC0, 0x72                         \
	}
#define GNUTLS_ECDHE_ECDSA_CAMELLIA_256_CBC_SHA384 \
	{                                          \
		0xC0, 0x73                         \
	}
#define GNUTLS_ECDHE_RSA_CAMELLIA_128_CBC_SHA256 \
	{                                        \
		0xC0, 0x76                       \
	}
#define GNUTLS_ECDHE_RSA_CAMELLIA_256_CBC_SHA384 \
	{                                        \
		0xC0, 0x77                       \
	}
#define GNUTLS_PSK_CAMELLIA_128_CBC_SHA256 \
	{                                  \
		0xC0, 0x94                 \
	}
#define GNUTLS_PSK_CAMELLIA_256_CBC_SHA384 \
	{                                  \
		0xC0, 0x95                 \
	}
#define GNUTLS_DHE_PSK_CAMELLIA_128_CBC_SHA256 \
	{                                      \
		0xC0, 0x96                     \
	}
#define GNUTLS_DHE_PSK_CAMELLIA_256_CBC_SHA384 \
	{                                      \
		0xC0, 0x97                     \
	}
#define GNUTLS_RSA_PSK_CAMELLIA_128_CBC_SHA256 \
	{                                      \
		0xC0, 0x98                     \
	}
#define GNUTLS_RSA_PSK_CAMELLIA_256_CBC_SHA384 \
	{                                      \
		0xC0, 0x99                     \
	}
#define GNUTLS_ECDHE_PSK_CAMELLIA_128_CBC_SHA256 \
	{                                        \
		0xC0, 0x9A                       \
	}
#define GNUTLS_ECDHE_PSK_CAMELLIA_256_CBC_SHA384 \
	{                                        \
		0xC0, 0x9B                       \
	}

#define GNUTLS_RSA_CAMELLIA_128_GCM_SHA256 \
	{                                  \
		0xC0, 0x7A                 \
	}
#define GNUTLS_RSA_CAMELLIA_256_GCM_SHA384 \
	{                                  \
		0xC0, 0x7B                 \
	}
#define GNUTLS_DHE_RSA_CAMELLIA_128_GCM_SHA256 \
	{                                      \
		0xC0, 0x7C                     \
	}
#define GNUTLS_DHE_RSA_CAMELLIA_256_GCM_SHA384 \
	{                                      \
		0xC0, 0x7D                     \
	}
#define GNUTLS_DHE_DSS_CAMELLIA_128_GCM_SHA256 \
	{                                      \
		0xC0, 0x80                     \
	}
#define GNUTLS_DHE_DSS_CAMELLIA_256_GCM_SHA384 \
	{                                      \
		0xC0, 0x81                     \
	}
#define GNUTLS_DH_ANON_CAMELLIA_128_GCM_SHA256 \
	{                                      \
		0xC0, 0x84                     \
	}
#define GNUTLS_DH_ANON_CAMELLIA_256_GCM_SHA384 \
	{                                      \
		0xC0, 0x85                     \
	}
#define GNUTLS_ECDHE_ECDSA_CAMELLIA_128_GCM_SHA256 \
	{                                          \
		0xC0, 0x86                         \
	}
#define GNUTLS_ECDHE_ECDSA_CAMELLIA_256_GCM_SHA384 \
	{                                          \
		0xC0, 0x87                         \
	}
#define GNUTLS_ECDHE_RSA_CAMELLIA_128_GCM_SHA256 \
	{                                        \
		0xC0, 0x8A                       \
	}
#define GNUTLS_ECDHE_RSA_CAMELLIA_256_GCM_SHA384 \
	{                                        \
		0xC0, 0x8B                       \
	}
#define GNUTLS_PSK_CAMELLIA_128_GCM_SHA256 \
	{                                  \
		0xC0, 0x8E                 \
	}
#define GNUTLS_PSK_CAMELLIA_256_GCM_SHA384 \
	{                                  \
		0xC0, 0x8F                 \
	}
#define GNUTLS_DHE_PSK_CAMELLIA_128_GCM_SHA256 \
	{                                      \
		0xC0, 0x90                     \
	}
#define GNUTLS_DHE_PSK_CAMELLIA_256_GCM_SHA384 \
	{                                      \
		0xC0, 0x91                     \
	}
#define GNUTLS_RSA_PSK_CAMELLIA_128_GCM_SHA256 \
	{                                      \
		0xC0, 0x92                     \
	}
#define GNUTLS_RSA_PSK_CAMELLIA_256_GCM_SHA384 \
	{                                      \
		0xC0, 0x93                     \
	}

#define GNUTLS_DH_ANON_AES_128_CBC_SHA256 \
	{                                 \
		0x00, 0x6C                \
	}
#define GNUTLS_DH_ANON_AES_256_CBC_SHA256 \
	{                                 \
		0x00, 0x6D                \
	}

/* draft-ietf-tls-chacha20-poly1305-02 */
#define GNUTLS_ECDHE_RSA_CHACHA20_POLY1305 \
	{                                  \
		0xCC, 0xA8                 \
	}
#define GNUTLS_ECDHE_ECDSA_CHACHA20_POLY1305 \
	{                                    \
		0xCC, 0xA9                   \
	}
#define GNUTLS_DHE_RSA_CHACHA20_POLY1305 \
	{                                \
		0xCC, 0xAA               \
	}

#define GNUTLS_PSK_CHACHA20_POLY1305 \
	{                            \
		0xCC, 0xAB           \
	}
#define GNUTLS_ECDHE_PSK_CHACHA20_POLY1305 \
	{                                  \
		0xCC, 0xAC                 \
	}
#define GNUTLS_DHE_PSK_CHACHA20_POLY1305 \
	{                                \
		0xCC, 0xAD               \
	}
#define GNUTLS_RSA_PSK_CHACHA20_POLY1305 \
	{                                \
		0xCC, 0xAE               \
	}

/* PSK (not in TLS 1.0)
 * draft-ietf-tls-psk:
 */
#define GNUTLS_PSK_ARCFOUR_128_SHA1 \
	{                           \
		0x00, 0x8A          \
	}
#define GNUTLS_PSK_3DES_EDE_CBC_SHA1 \
	{                            \
		0x00, 0x8B           \
	}
#define GNUTLS_PSK_AES_128_CBC_SHA1 \
	{                           \
		0x00, 0x8C          \
	}
#define GNUTLS_PSK_AES_256_CBC_SHA1 \
	{                           \
		0x00, 0x8D          \
	}

#define GNUTLS_DHE_PSK_ARCFOUR_128_SHA1 \
	{                               \
		0x00, 0x8E              \
	}
#define GNUTLS_DHE_PSK_3DES_EDE_CBC_SHA1 \
	{                                \
		0x00, 0x8F               \
	}
#define GNUTLS_DHE_PSK_AES_128_CBC_SHA1 \
	{                               \
		0x00, 0x90              \
	}
#define GNUTLS_DHE_PSK_AES_256_CBC_SHA1 \
	{                               \
		0x00, 0x91              \
	}

#define GNUTLS_RSA_PSK_ARCFOUR_128_SHA1 \
	{                               \
		0x00, 0x92              \
	}
#define GNUTLS_RSA_PSK_3DES_EDE_CBC_SHA1 \
	{                                \
		0x00, 0x93               \
	}
#define GNUTLS_RSA_PSK_AES_128_CBC_SHA1 \
	{                               \
		0x00, 0x94              \
	}
#define GNUTLS_RSA_PSK_AES_256_CBC_SHA1 \
	{                               \
		0x00, 0x95              \
	}

#ifdef ENABLE_SRP
/* SRP (rfc5054)
 */
#define GNUTLS_SRP_SHA_3DES_EDE_CBC_SHA1 \
	{                                \
		0xC0, 0x1A               \
	}
#define GNUTLS_SRP_SHA_RSA_3DES_EDE_CBC_SHA1 \
	{                                    \
		0xC0, 0x1B                   \
	}
#define GNUTLS_SRP_SHA_DSS_3DES_EDE_CBC_SHA1 \
	{                                    \
		0xC0, 0x1C                   \
	}

#define GNUTLS_SRP_SHA_AES_128_CBC_SHA1 \
	{                               \
		0xC0, 0x1D              \
	}
#define GNUTLS_SRP_SHA_RSA_AES_128_CBC_SHA1 \
	{                                   \
		0xC0, 0x1E                  \
	}
#define GNUTLS_SRP_SHA_DSS_AES_128_CBC_SHA1 \
	{                                   \
		0xC0, 0x1F                  \
	}

#define GNUTLS_SRP_SHA_AES_256_CBC_SHA1 \
	{                               \
		0xC0, 0x20              \
	}
#define GNUTLS_SRP_SHA_RSA_AES_256_CBC_SHA1 \
	{                                   \
		0xC0, 0x21                  \
	}
#define GNUTLS_SRP_SHA_DSS_AES_256_CBC_SHA1 \
	{                                   \
		0xC0, 0x22                  \
	}
#endif

/* RSA
 */
#define GNUTLS_RSA_ARCFOUR_128_SHA1 \
	{                           \
		0x00, 0x05          \
	}
#define GNUTLS_RSA_ARCFOUR_128_MD5 \
	{                          \
		0x00, 0x04         \
	}
#define GNUTLS_RSA_3DES_EDE_CBC_SHA1 \
	{                            \
		0x00, 0x0A           \
	}

/* rfc3268:
 */
#define GNUTLS_RSA_AES_128_CBC_SHA1 \
	{                           \
		0x00, 0x2F          \
	}
#define GNUTLS_RSA_AES_256_CBC_SHA1 \
	{                           \
		0x00, 0x35          \
	}

/* rfc4132 */
#define GNUTLS_RSA_CAMELLIA_128_CBC_SHA1 \
	{                                \
		0x00, 0x41               \
	}
#define GNUTLS_RSA_CAMELLIA_256_CBC_SHA1 \
	{                                \
		0x00, 0x84               \
	}

#define GNUTLS_RSA_AES_128_CBC_SHA256 \
	{                             \
		0x00, 0x3C            \
	}
#define GNUTLS_RSA_AES_256_CBC_SHA256 \
	{                             \
		0x00, 0x3D            \
	}

/* DHE DSS
 */
#define GNUTLS_DHE_DSS_3DES_EDE_CBC_SHA1 \
	{                                \
		0x00, 0x13               \
	}

/* draft-ietf-tls-56-bit-ciphersuites-01:
 */
#define GNUTLS_DHE_DSS_ARCFOUR_128_SHA1 \
	{                               \
		0x00, 0x66              \
	}

/* rfc3268:
 */
#define GNUTLS_DHE_DSS_AES_256_CBC_SHA1 \
	{                               \
		0x00, 0x38              \
	}
#define GNUTLS_DHE_DSS_AES_128_CBC_SHA1 \
	{                               \
		0x00, 0x32              \
	}

/* rfc4132 */
#define GNUTLS_DHE_DSS_CAMELLIA_128_CBC_SHA1 \
	{                                    \
		0x00, 0x44                   \
	}
#define GNUTLS_DHE_DSS_CAMELLIA_256_CBC_SHA1 \
	{                                    \
		0x00, 0x87                   \
	}

#define GNUTLS_DHE_DSS_AES_128_CBC_SHA256 \
	{                                 \
		0x00, 0x40                \
	}
#define GNUTLS_DHE_DSS_AES_256_CBC_SHA256 \
	{                                 \
		0x00, 0x6A                \
	}

/* DHE RSA
 */
#define GNUTLS_DHE_RSA_3DES_EDE_CBC_SHA1 \
	{                                \
		0x00, 0x16               \
	}

/* rfc3268:
 */
#define GNUTLS_DHE_RSA_AES_128_CBC_SHA1 \
	{                               \
		0x00, 0x33              \
	}
#define GNUTLS_DHE_RSA_AES_256_CBC_SHA1 \
	{                               \
		0x00, 0x39              \
	}

/* rfc4132 */
#define GNUTLS_DHE_RSA_CAMELLIA_128_CBC_SHA1 \
	{                                    \
		0x00, 0x45                   \
	}
#define GNUTLS_DHE_RSA_CAMELLIA_256_CBC_SHA1 \
	{                                    \
		0x00, 0x88                   \
	}

#define GNUTLS_DHE_RSA_AES_128_CBC_SHA256 \
	{                                 \
		0x00, 0x67                \
	}
#define GNUTLS_DHE_RSA_AES_256_CBC_SHA256 \
	{                                 \
		0x00, 0x6B                \
	}

/* GCM: RFC5288 */
#define GNUTLS_RSA_AES_128_GCM_SHA256 \
	{                             \
		0x00, 0x9C            \
	}
#define GNUTLS_DHE_RSA_AES_128_GCM_SHA256 \
	{                                 \
		0x00, 0x9E                \
	}
#define GNUTLS_DHE_DSS_AES_128_GCM_SHA256 \
	{                                 \
		0x00, 0xA2                \
	}
#define GNUTLS_DH_ANON_AES_128_GCM_SHA256 \
	{                                 \
		0x00, 0xA6                \
	}
#define GNUTLS_RSA_AES_256_GCM_SHA384 \
	{                             \
		0x00, 0x9D            \
	}
#define GNUTLS_DHE_RSA_AES_256_GCM_SHA384 \
	{                                 \
		0x00, 0x9F                \
	}
#define GNUTLS_DHE_DSS_AES_256_GCM_SHA384 \
	{                                 \
		0x00, 0xA3                \
	}
#define GNUTLS_DH_ANON_AES_256_GCM_SHA384 \
	{                                 \
		0x00, 0xA7                \
	}

/* CCM: RFC6655/7251 */
#define GNUTLS_RSA_AES_128_CCM \
	{                      \
		0xC0, 0x9C     \
	}
#define GNUTLS_RSA_AES_256_CCM \
	{                      \
		0xC0, 0x9D     \
	}
#define GNUTLS_DHE_RSA_AES_128_CCM \
	{                          \
		0xC0, 0x9E         \
	}
#define GNUTLS_DHE_RSA_AES_256_CCM \
	{                          \
		0xC0, 0x9F         \
	}

#define GNUTLS_ECDHE_ECDSA_AES_128_CCM \
	{                              \
		0xC0, 0xAC             \
	}
#define GNUTLS_ECDHE_ECDSA_AES_256_CCM \
	{                              \
		0xC0, 0xAD             \
	}

#define GNUTLS_PSK_AES_128_CCM \
	{                      \
		0xC0, 0xA4     \
	}
#define GNUTLS_PSK_AES_256_CCM \
	{                      \
		0xC0, 0xA5     \
	}
#define GNUTLS_DHE_PSK_AES_128_CCM \
	{                          \
		0xC0, 0xA6         \
	}
#define GNUTLS_DHE_PSK_AES_256_CCM \
	{                          \
		0xC0, 0xA7         \
	}

/* CCM-8: RFC6655/7251 */
#define GNUTLS_RSA_AES_128_CCM_8 \
	{                        \
		0xC0, 0xA0       \
	}
#define GNUTLS_RSA_AES_256_CCM_8 \
	{                        \
		0xC0, 0xA1       \
	}
#define GNUTLS_DHE_RSA_AES_128_CCM_8 \
	{                            \
		0xC0, 0xA2           \
	}
#define GNUTLS_DHE_RSA_AES_256_CCM_8 \
	{                            \
		0xC0, 0xA3           \
	}

#define GNUTLS_ECDHE_ECDSA_AES_128_CCM_8 \
	{                                \
		0xC0, 0xAE               \
	}
#define GNUTLS_ECDHE_ECDSA_AES_256_CCM_8 \
	{                                \
		0xC0, 0xAF               \
	}

#define GNUTLS_PSK_AES_128_CCM_8 \
	{                        \
		0xC0, 0xA8       \
	}
#define GNUTLS_PSK_AES_256_CCM_8 \
	{                        \
		0xC0, 0xA9       \
	}
#define GNUTLS_DHE_PSK_AES_128_CCM_8 \
	{                            \
		0xC0, 0xAA           \
	}
#define GNUTLS_DHE_PSK_AES_256_CCM_8 \
	{                            \
		0xC0, 0xAB           \
	}

/* RFC 5487 */
/* GCM-PSK */
#define GNUTLS_PSK_AES_128_GCM_SHA256 \
	{                             \
		0x00, 0xA8            \
	}
#define GNUTLS_DHE_PSK_AES_128_GCM_SHA256 \
	{                                 \
		0x00, 0xAA                \
	}
#define GNUTLS_PSK_AES_256_GCM_SHA384 \
	{                             \
		0x00, 0xA9            \
	}
#define GNUTLS_DHE_PSK_AES_256_GCM_SHA384 \
	{                                 \
		0x00, 0xAB                \
	}

#define GNUTLS_PSK_AES_256_CBC_SHA384 \
	{                             \
		0x00, 0xAF            \
	}
#define GNUTLS_PSK_NULL_SHA384 \
	{                      \
		0x00, 0xB1     \
	}
#define GNUTLS_DHE_PSK_AES_256_CBC_SHA384 \
	{                                 \
		0x00, 0xB3                \
	}
#define GNUTLS_DHE_PSK_NULL_SHA384 \
	{                          \
		0x00, 0xB5         \
	}

#define GNUTLS_PSK_NULL_SHA1 \
	{                    \
		0x00, 0x2C   \
	}
#define GNUTLS_DHE_PSK_NULL_SHA1 \
	{                        \
		0x00, 0x2D       \
	}
#define GNUTLS_RSA_PSK_NULL_SHA1 \
	{                        \
		0x00, 0x2E       \
	}
#define GNUTLS_ECDHE_PSK_NULL_SHA1 \
	{                          \
		0xC0, 0x39         \
	}

#define GNUTLS_RSA_PSK_AES_128_GCM_SHA256 \
	{                                 \
		0x00, 0xAC                \
	}
#define GNUTLS_RSA_PSK_AES_256_GCM_SHA384 \
	{                                 \
		0x00, 0xAD                \
	}
#define GNUTLS_RSA_PSK_AES_128_CBC_SHA256 \
	{                                 \
		0x00, 0xB6                \
	}
#define GNUTLS_RSA_PSK_AES_256_CBC_SHA384 \
	{                                 \
		0x00, 0xB7                \
	}
#define GNUTLS_RSA_PSK_NULL_SHA256 \
	{                          \
		0x00, 0xB8         \
	}
#define GNUTLS_RSA_PSK_NULL_SHA384 \
	{                          \
		0x00, 0xB9         \
	}

/* PSK - SHA256 HMAC */
#define GNUTLS_PSK_AES_128_CBC_SHA256 \
	{                             \
		0x00, 0xAE            \
	}
#define GNUTLS_DHE_PSK_AES_128_CBC_SHA256 \
	{                                 \
		0x00, 0xB2                \
	}

#define GNUTLS_PSK_NULL_SHA256 \
	{                      \
		0x00, 0xB0     \
	}
#define GNUTLS_DHE_PSK_NULL_SHA256 \
	{                          \
		0x00, 0xB4         \
	}

/* ECC */
#define GNUTLS_ECDH_ANON_NULL_SHA1 \
	{                          \
		0xC0, 0x15         \
	}
#define GNUTLS_ECDH_ANON_3DES_EDE_CBC_SHA1 \
	{                                  \
		0xC0, 0x17                 \
	}
#define GNUTLS_ECDH_ANON_AES_128_CBC_SHA1 \
	{                                 \
		0xC0, 0x18                \
	}
#define GNUTLS_ECDH_ANON_AES_256_CBC_SHA1 \
	{                                 \
		0xC0, 0x19                \
	}
#define GNUTLS_ECDH_ANON_ARCFOUR_128_SHA1 \
	{                                 \
		0xC0, 0x16                \
	}

/* ECC-RSA */
#define GNUTLS_ECDHE_RSA_NULL_SHA1 \
	{                          \
		0xC0, 0x10         \
	}
#define GNUTLS_ECDHE_RSA_3DES_EDE_CBC_SHA1 \
	{                                  \
		0xC0, 0x12                 \
	}
#define GNUTLS_ECDHE_RSA_AES_128_CBC_SHA1 \
	{                                 \
		0xC0, 0x13                \
	}
#define GNUTLS_ECDHE_RSA_AES_256_CBC_SHA1 \
	{                                 \
		0xC0, 0x14                \
	}
#define GNUTLS_ECDHE_RSA_ARCFOUR_128_SHA1 \
	{                                 \
		0xC0, 0x11                \
	}

/* ECC-ECDSA */
#define GNUTLS_ECDHE_ECDSA_NULL_SHA1 \
	{                            \
		0xC0, 0x06           \
	}
#define GNUTLS_ECDHE_ECDSA_3DES_EDE_CBC_SHA1 \
	{                                    \
		0xC0, 0x08                   \
	}
#define GNUTLS_ECDHE_ECDSA_AES_128_CBC_SHA1 \
	{                                   \
		0xC0, 0x09                  \
	}
#define GNUTLS_ECDHE_ECDSA_AES_256_CBC_SHA1 \
	{                                   \
		0xC0, 0x0A                  \
	}
#define GNUTLS_ECDHE_ECDSA_ARCFOUR_128_SHA1 \
	{                                   \
		0xC0, 0x07                  \
	}

/* RFC5289 */
/* ECC with SHA2 */
#define GNUTLS_ECDHE_ECDSA_AES_128_CBC_SHA256 \
	{                                     \
		0xC0, 0x23                    \
	}
#define GNUTLS_ECDHE_RSA_AES_128_CBC_SHA256 \
	{                                   \
		0xC0, 0x27                  \
	}
#define GNUTLS_ECDHE_RSA_AES_256_CBC_SHA384 \
	{                                   \
		0xC0, 0x28                  \
	}

/* ECC with AES-GCM */
#define GNUTLS_ECDHE_ECDSA_AES_128_GCM_SHA256 \
	{                                     \
		0xC0, 0x2B                    \
	}
#define GNUTLS_ECDHE_RSA_AES_128_GCM_SHA256 \
	{                                   \
		0xC0, 0x2F                  \
	}
#define GNUTLS_ECDHE_RSA_AES_256_GCM_SHA384 \
	{                                   \
		0xC0, 0x30                  \
	}

/* SuiteB */
#define GNUTLS_ECDHE_ECDSA_AES_256_GCM_SHA384 \
	{                                     \
		0xC0, 0x2C                    \
	}
#define GNUTLS_ECDHE_ECDSA_AES_256_CBC_SHA384 \
	{                                     \
		0xC0, 0x24                    \
	}

/* ECC with PSK */
#define GNUTLS_ECDHE_PSK_3DES_EDE_CBC_SHA1 \
	{                                  \
		0xC0, 0x34                 \
	}
#define GNUTLS_ECDHE_PSK_AES_128_CBC_SHA1 \
	{                                 \
		0xC0, 0x35                \
	}
#define GNUTLS_ECDHE_PSK_AES_256_CBC_SHA1 \
	{                                 \
		0xC0, 0x36                \
	}
#define GNUTLS_ECDHE_PSK_AES_128_CBC_SHA256 \
	{                                   \
		0xC0, 0x37                  \
	}
#define GNUTLS_ECDHE_PSK_AES_256_CBC_SHA384 \
	{                                   \
		0xC0, 0x38                  \
	}
#define GNUTLS_ECDHE_PSK_ARCFOUR_128_SHA1 \
	{                                 \
		0xC0, 0x33                \
	}
#define GNUTLS_ECDHE_PSK_NULL_SHA256 \
	{                            \
		0xC0, 0x3A           \
	}
#define GNUTLS_ECDHE_PSK_NULL_SHA384 \
	{                            \
		0xC0, 0x3B           \
	}

/* draft-smyshlyaev-tls12-gost-suites */
#ifdef ENABLE_GOST
#define GNUTLS_GOSTR341112_256_28147_CNT_IMIT \
	{                                     \
		0xc1, 0x02                    \
	}
#endif

#define CIPHER_SUITES_COUNT \
	(sizeof(cs_algorithms) / sizeof(gnutls_cipher_suite_entry_st) - 1)

/* The following is a potential list of ciphersuites. For the options to be
 * available, the ciphers and MACs must be available to gnutls as well.
 */
static const gnutls_cipher_suite_entry_st cs_algorithms[] = {
	/* TLS 1.3 */
	ENTRY_TLS13(GNUTLS_AES_128_GCM_SHA256, "TLS_AES_128_GCM_SHA256",
		    GNUTLS_CIPHER_AES_128_GCM, GNUTLS_TLS1_3,
		    GNUTLS_MAC_SHA256),

	ENTRY_TLS13(GNUTLS_AES_256_GCM_SHA384, "TLS_AES_256_GCM_SHA384",
		    GNUTLS_CIPHER_AES_256_GCM, GNUTLS_TLS1_3,
		    GNUTLS_MAC_SHA384),

	ENTRY_TLS13(GNUTLS_CHACHA20_POLY1305_SHA256,
		    "TLS_CHACHA20_POLY1305_SHA256",
		    GNUTLS_CIPHER_CHACHA20_POLY1305, GNUTLS_TLS1_3,
		    GNUTLS_MAC_SHA256),

	ENTRY_TLS13(GNUTLS_AES_128_CCM_SHA256, "TLS_AES_128_CCM_SHA256",
		    GNUTLS_CIPHER_AES_128_CCM, GNUTLS_TLS1_3,
		    GNUTLS_MAC_SHA256),

	ENTRY_TLS13(GNUTLS_AES_128_CCM_8_SHA256, "TLS_AES_128_CCM_8_SHA256",
		    GNUTLS_CIPHER_AES_128_CCM_8, GNUTLS_TLS1_3,
		    GNUTLS_MAC_SHA256),

	/* RSA-NULL */
	ENTRY(GNUTLS_RSA_NULL_MD5, "TLS_RSA_WITH_NULL_MD5", GNUTLS_CIPHER_NULL,
	      GNUTLS_KX_RSA, GNUTLS_MAC_MD5, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_RSA_NULL_SHA1, "TLS_RSA_WITH_NULL_SHA", GNUTLS_CIPHER_NULL,
	      GNUTLS_KX_RSA, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_RSA_NULL_SHA256, "TLS_RSA_WITH_NULL_SHA256",
	      GNUTLS_CIPHER_NULL, GNUTLS_KX_RSA, GNUTLS_MAC_SHA256,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),

	/* RSA */
	ENTRY(GNUTLS_RSA_ARCFOUR_128_SHA1, "TLS_RSA_WITH_RC4_128_SHA",
	      GNUTLS_CIPHER_ARCFOUR_128, GNUTLS_KX_RSA, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_VERSION_UNKNOWN),
	ENTRY(GNUTLS_RSA_ARCFOUR_128_MD5, "TLS_RSA_WITH_RC4_128_MD5",
	      GNUTLS_CIPHER_ARCFOUR_128, GNUTLS_KX_RSA, GNUTLS_MAC_MD5,
	      GNUTLS_SSL3, GNUTLS_VERSION_UNKNOWN),
	ENTRY(GNUTLS_RSA_3DES_EDE_CBC_SHA1, "TLS_RSA_WITH_3DES_EDE_CBC_SHA",
	      GNUTLS_CIPHER_3DES_CBC, GNUTLS_KX_RSA, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_RSA_AES_128_CBC_SHA1, "TLS_RSA_WITH_AES_128_CBC_SHA",
	      GNUTLS_CIPHER_AES_128_CBC, GNUTLS_KX_RSA, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_RSA_AES_256_CBC_SHA1, "TLS_RSA_WITH_AES_256_CBC_SHA",
	      GNUTLS_CIPHER_AES_256_CBC, GNUTLS_KX_RSA, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),

	ENTRY(GNUTLS_RSA_CAMELLIA_128_CBC_SHA256,
	      "TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_CBC, GNUTLS_KX_RSA, GNUTLS_MAC_SHA256,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_RSA_CAMELLIA_256_CBC_SHA256,
	      "TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_256_CBC, GNUTLS_KX_RSA, GNUTLS_MAC_SHA256,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_RSA_CAMELLIA_128_CBC_SHA1,
	      "TLS_RSA_WITH_CAMELLIA_128_CBC_SHA",
	      GNUTLS_CIPHER_CAMELLIA_128_CBC, GNUTLS_KX_RSA, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_RSA_CAMELLIA_256_CBC_SHA1,
	      "TLS_RSA_WITH_CAMELLIA_256_CBC_SHA",
	      GNUTLS_CIPHER_CAMELLIA_256_CBC, GNUTLS_KX_RSA, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_RSA_AES_128_CBC_SHA256, "TLS_RSA_WITH_AES_128_CBC_SHA256",
	      GNUTLS_CIPHER_AES_128_CBC, GNUTLS_KX_RSA, GNUTLS_MAC_SHA256,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_RSA_AES_256_CBC_SHA256, "TLS_RSA_WITH_AES_256_CBC_SHA256",
	      GNUTLS_CIPHER_AES_256_CBC, GNUTLS_KX_RSA, GNUTLS_MAC_SHA256,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),

	/* GCM */
	ENTRY(GNUTLS_RSA_AES_128_GCM_SHA256, "TLS_RSA_WITH_AES_128_GCM_SHA256",
	      GNUTLS_CIPHER_AES_128_GCM, GNUTLS_KX_RSA, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_RSA_AES_256_GCM_SHA384,
		  "TLS_RSA_WITH_AES_256_GCM_SHA384", GNUTLS_CIPHER_AES_256_GCM,
		  GNUTLS_KX_RSA, GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),
	ENTRY(GNUTLS_RSA_CAMELLIA_128_GCM_SHA256,
	      "TLS_RSA_WITH_CAMELLIA_128_GCM_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_GCM, GNUTLS_KX_RSA, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_RSA_CAMELLIA_256_GCM_SHA384,
		  "TLS_RSA_WITH_CAMELLIA_256_GCM_SHA384",
		  GNUTLS_CIPHER_CAMELLIA_256_GCM, GNUTLS_KX_RSA,
		  GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),

	/* CCM */
	ENTRY(GNUTLS_RSA_AES_128_CCM, "TLS_RSA_WITH_AES_128_CCM",
	      GNUTLS_CIPHER_AES_128_CCM, GNUTLS_KX_RSA, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_RSA_AES_256_CCM, "TLS_RSA_WITH_AES_256_CCM",
	      GNUTLS_CIPHER_AES_256_CCM, GNUTLS_KX_RSA, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),

	/* CCM_8 */
	ENTRY(GNUTLS_RSA_AES_128_CCM_8, "TLS_RSA_WITH_AES_128_CCM_8",
	      GNUTLS_CIPHER_AES_128_CCM_8, GNUTLS_KX_RSA, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_RSA_AES_256_CCM_8, "TLS_RSA_WITH_AES_256_CCM_8",
	      GNUTLS_CIPHER_AES_256_CCM_8, GNUTLS_KX_RSA, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),

/* DHE_DSS */
#ifdef ENABLE_DHE
	ENTRY(GNUTLS_DHE_DSS_ARCFOUR_128_SHA1, "TLS_DHE_DSS_RC4_128_SHA",
	      GNUTLS_CIPHER_ARCFOUR_128, GNUTLS_KX_DHE_DSS, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_VERSION_UNKNOWN),
	ENTRY(GNUTLS_DHE_DSS_3DES_EDE_CBC_SHA1,
	      "TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA", GNUTLS_CIPHER_3DES_CBC,
	      GNUTLS_KX_DHE_DSS, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DHE_DSS_AES_128_CBC_SHA1,
	      "TLS_DHE_DSS_WITH_AES_128_CBC_SHA", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_DHE_DSS, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DHE_DSS_AES_256_CBC_SHA1,
	      "TLS_DHE_DSS_WITH_AES_256_CBC_SHA", GNUTLS_CIPHER_AES_256_CBC,
	      GNUTLS_KX_DHE_DSS, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DHE_DSS_CAMELLIA_128_CBC_SHA256,
	      "TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_CBC, GNUTLS_KX_DHE_DSS,
	      GNUTLS_MAC_SHA256, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DHE_DSS_CAMELLIA_256_CBC_SHA256,
	      "TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_256_CBC, GNUTLS_KX_DHE_DSS,
	      GNUTLS_MAC_SHA256, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),

	ENTRY(GNUTLS_DHE_DSS_CAMELLIA_128_CBC_SHA1,
	      "TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA",
	      GNUTLS_CIPHER_CAMELLIA_128_CBC, GNUTLS_KX_DHE_DSS,
	      GNUTLS_MAC_SHA1, GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DHE_DSS_CAMELLIA_256_CBC_SHA1,
	      "TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA",
	      GNUTLS_CIPHER_CAMELLIA_256_CBC, GNUTLS_KX_DHE_DSS,
	      GNUTLS_MAC_SHA1, GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DHE_DSS_AES_128_CBC_SHA256,
	      "TLS_DHE_DSS_WITH_AES_128_CBC_SHA256", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_DHE_DSS, GNUTLS_MAC_SHA256, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DHE_DSS_AES_256_CBC_SHA256,
	      "TLS_DHE_DSS_WITH_AES_256_CBC_SHA256", GNUTLS_CIPHER_AES_256_CBC,
	      GNUTLS_KX_DHE_DSS, GNUTLS_MAC_SHA256, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	/* GCM */
	ENTRY(GNUTLS_DHE_DSS_AES_128_GCM_SHA256,
	      "TLS_DHE_DSS_WITH_AES_128_GCM_SHA256", GNUTLS_CIPHER_AES_128_GCM,
	      GNUTLS_KX_DHE_DSS, GNUTLS_MAC_AEAD, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_DHE_DSS_AES_256_GCM_SHA384,
		  "TLS_DHE_DSS_WITH_AES_256_GCM_SHA384",
		  GNUTLS_CIPHER_AES_256_GCM, GNUTLS_KX_DHE_DSS, GNUTLS_MAC_AEAD,
		  GNUTLS_TLS1_2, GNUTLS_DTLS1_2, GNUTLS_MAC_SHA384),
	ENTRY(GNUTLS_DHE_DSS_CAMELLIA_128_GCM_SHA256,
	      "TLS_DHE_DSS_WITH_CAMELLIA_128_GCM_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_GCM, GNUTLS_KX_DHE_DSS,
	      GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_DHE_DSS_CAMELLIA_256_GCM_SHA384,
		  "TLS_DHE_DSS_WITH_CAMELLIA_256_GCM_SHA384",
		  GNUTLS_CIPHER_CAMELLIA_256_GCM, GNUTLS_KX_DHE_DSS,
		  GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),

	/* DHE_RSA */
	ENTRY(GNUTLS_DHE_RSA_3DES_EDE_CBC_SHA1,
	      "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA", GNUTLS_CIPHER_3DES_CBC,
	      GNUTLS_KX_DHE_RSA, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DHE_RSA_AES_128_CBC_SHA1,
	      "TLS_DHE_RSA_WITH_AES_128_CBC_SHA", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_DHE_RSA, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DHE_RSA_AES_256_CBC_SHA1,
	      "TLS_DHE_RSA_WITH_AES_256_CBC_SHA", GNUTLS_CIPHER_AES_256_CBC,
	      GNUTLS_KX_DHE_RSA, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DHE_RSA_CAMELLIA_128_CBC_SHA256,
	      "TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_CBC, GNUTLS_KX_DHE_RSA,
	      GNUTLS_MAC_SHA256, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DHE_RSA_CAMELLIA_256_CBC_SHA256,
	      "TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_256_CBC, GNUTLS_KX_DHE_RSA,
	      GNUTLS_MAC_SHA256, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DHE_RSA_CAMELLIA_128_CBC_SHA1,
	      "TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA",
	      GNUTLS_CIPHER_CAMELLIA_128_CBC, GNUTLS_KX_DHE_RSA,
	      GNUTLS_MAC_SHA1, GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DHE_RSA_CAMELLIA_256_CBC_SHA1,
	      "TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA",
	      GNUTLS_CIPHER_CAMELLIA_256_CBC, GNUTLS_KX_DHE_RSA,
	      GNUTLS_MAC_SHA1, GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DHE_RSA_AES_128_CBC_SHA256,
	      "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_DHE_RSA, GNUTLS_MAC_SHA256, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DHE_RSA_AES_256_CBC_SHA256,
	      "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256", GNUTLS_CIPHER_AES_256_CBC,
	      GNUTLS_KX_DHE_RSA, GNUTLS_MAC_SHA256, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	/* GCM */
	ENTRY(GNUTLS_DHE_RSA_AES_128_GCM_SHA256,
	      "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256", GNUTLS_CIPHER_AES_128_GCM,
	      GNUTLS_KX_DHE_RSA, GNUTLS_MAC_AEAD, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_DHE_RSA_AES_256_GCM_SHA384,
		  "TLS_DHE_RSA_WITH_AES_256_GCM_SHA384",
		  GNUTLS_CIPHER_AES_256_GCM, GNUTLS_KX_DHE_RSA, GNUTLS_MAC_AEAD,
		  GNUTLS_TLS1_2, GNUTLS_DTLS1_2, GNUTLS_MAC_SHA384),
	ENTRY(GNUTLS_DHE_RSA_CAMELLIA_128_GCM_SHA256,
	      "TLS_DHE_RSA_WITH_CAMELLIA_128_GCM_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_GCM, GNUTLS_KX_DHE_RSA,
	      GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_DHE_RSA_CAMELLIA_256_GCM_SHA384,
		  "TLS_DHE_RSA_WITH_CAMELLIA_256_GCM_SHA384",
		  GNUTLS_CIPHER_CAMELLIA_256_GCM, GNUTLS_KX_DHE_RSA,
		  GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),

	ENTRY(GNUTLS_DHE_RSA_CHACHA20_POLY1305,
	      "TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256",
	      GNUTLS_CIPHER_CHACHA20_POLY1305, GNUTLS_KX_DHE_RSA,
	      GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),

	/* CCM */
	ENTRY(GNUTLS_DHE_RSA_AES_128_CCM, "TLS_DHE_RSA_WITH_AES_128_CCM",
	      GNUTLS_CIPHER_AES_128_CCM, GNUTLS_KX_DHE_RSA, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DHE_RSA_AES_256_CCM, "TLS_DHE_RSA_WITH_AES_256_CCM",
	      GNUTLS_CIPHER_AES_256_CCM, GNUTLS_KX_DHE_RSA, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DHE_RSA_AES_128_CCM_8, "TLS_DHE_RSA_WITH_AES_128_CCM_8",
	      GNUTLS_CIPHER_AES_128_CCM_8, GNUTLS_KX_DHE_RSA, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DHE_RSA_AES_256_CCM_8, "TLS_DHE_RSA_WITH_AES_256_CCM_8",
	      GNUTLS_CIPHER_AES_256_CCM_8, GNUTLS_KX_DHE_RSA, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),

#endif /* DHE */
#ifdef ENABLE_ECDHE
	/* ECC-RSA */
	ENTRY(GNUTLS_ECDHE_RSA_NULL_SHA1, "TLS_ECDHE_RSA_WITH_NULL_SHA",
	      GNUTLS_CIPHER_NULL, GNUTLS_KX_ECDHE_RSA, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_ECDHE_RSA_3DES_EDE_CBC_SHA1,
	      "TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA", GNUTLS_CIPHER_3DES_CBC,
	      GNUTLS_KX_ECDHE_RSA, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_ECDHE_RSA_AES_128_CBC_SHA1,
	      "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_ECDHE_RSA, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_ECDHE_RSA_AES_256_CBC_SHA1,
	      "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA", GNUTLS_CIPHER_AES_256_CBC,
	      GNUTLS_KX_ECDHE_RSA, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY_PRF(GNUTLS_ECDHE_RSA_AES_256_CBC_SHA384,
		  "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384",
		  GNUTLS_CIPHER_AES_256_CBC, GNUTLS_KX_ECDHE_RSA,
		  GNUTLS_MAC_SHA384, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),
	ENTRY(GNUTLS_ECDHE_RSA_ARCFOUR_128_SHA1,
	      "TLS_ECDHE_RSA_WITH_RC4_128_SHA", GNUTLS_CIPHER_ARCFOUR,
	      GNUTLS_KX_ECDHE_RSA, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_VERSION_UNKNOWN),
	ENTRY(GNUTLS_ECDHE_RSA_CAMELLIA_128_CBC_SHA256,
	      "TLS_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_CBC, GNUTLS_KX_ECDHE_RSA,
	      GNUTLS_MAC_SHA256, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_ECDHE_RSA_CAMELLIA_256_CBC_SHA384,
		  "TLS_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384",
		  GNUTLS_CIPHER_CAMELLIA_256_CBC, GNUTLS_KX_ECDHE_RSA,
		  GNUTLS_MAC_SHA384, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),

	/* ECDHE-ECDSA */
	ENTRY(GNUTLS_ECDHE_ECDSA_NULL_SHA1, "TLS_ECDHE_ECDSA_WITH_NULL_SHA",
	      GNUTLS_CIPHER_NULL, GNUTLS_KX_ECDHE_ECDSA, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_ECDHE_ECDSA_3DES_EDE_CBC_SHA1,
	      "TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA", GNUTLS_CIPHER_3DES_CBC,
	      GNUTLS_KX_ECDHE_ECDSA, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_ECDHE_ECDSA_AES_128_CBC_SHA1,
	      "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_ECDHE_ECDSA, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_ECDHE_ECDSA_AES_256_CBC_SHA1,
	      "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA", GNUTLS_CIPHER_AES_256_CBC,
	      GNUTLS_KX_ECDHE_ECDSA, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_ECDHE_ECDSA_ARCFOUR_128_SHA1,
	      "TLS_ECDHE_ECDSA_WITH_RC4_128_SHA", GNUTLS_CIPHER_ARCFOUR,
	      GNUTLS_KX_ECDHE_ECDSA, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_VERSION_UNKNOWN),
	ENTRY(GNUTLS_ECDHE_ECDSA_CAMELLIA_128_CBC_SHA256,
	      "TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_CBC, GNUTLS_KX_ECDHE_ECDSA,
	      GNUTLS_MAC_SHA256, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_ECDHE_ECDSA_CAMELLIA_256_CBC_SHA384,
		  "TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384",
		  GNUTLS_CIPHER_CAMELLIA_256_CBC, GNUTLS_KX_ECDHE_ECDSA,
		  GNUTLS_MAC_SHA384, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),

	/* More ECC */

	ENTRY(GNUTLS_ECDHE_ECDSA_AES_128_CBC_SHA256,
	      "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256",
	      GNUTLS_CIPHER_AES_128_CBC, GNUTLS_KX_ECDHE_ECDSA,
	      GNUTLS_MAC_SHA256, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_ECDHE_RSA_AES_128_CBC_SHA256,
	      "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256",
	      GNUTLS_CIPHER_AES_128_CBC, GNUTLS_KX_ECDHE_RSA, GNUTLS_MAC_SHA256,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_ECDHE_ECDSA_CAMELLIA_128_GCM_SHA256,
	      "TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_GCM, GNUTLS_KX_ECDHE_ECDSA,
	      GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_ECDHE_ECDSA_CAMELLIA_256_GCM_SHA384,
		  "TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384",
		  GNUTLS_CIPHER_CAMELLIA_256_GCM, GNUTLS_KX_ECDHE_ECDSA,
		  GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),
	ENTRY(GNUTLS_ECDHE_ECDSA_AES_128_GCM_SHA256,
	      "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256",
	      GNUTLS_CIPHER_AES_128_GCM, GNUTLS_KX_ECDHE_ECDSA, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_ECDHE_ECDSA_AES_256_GCM_SHA384,
		  "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384",
		  GNUTLS_CIPHER_AES_256_GCM, GNUTLS_KX_ECDHE_ECDSA,
		  GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),
	ENTRY(GNUTLS_ECDHE_RSA_AES_128_GCM_SHA256,
	      "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",
	      GNUTLS_CIPHER_AES_128_GCM, GNUTLS_KX_ECDHE_RSA, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_ECDHE_RSA_AES_256_GCM_SHA384,
		  "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",
		  GNUTLS_CIPHER_AES_256_GCM, GNUTLS_KX_ECDHE_RSA,
		  GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),
	ENTRY_PRF(GNUTLS_ECDHE_ECDSA_AES_256_CBC_SHA384,
		  "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384",
		  GNUTLS_CIPHER_AES_256_CBC, GNUTLS_KX_ECDHE_ECDSA,
		  GNUTLS_MAC_SHA384, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),

	ENTRY(GNUTLS_ECDHE_RSA_CAMELLIA_128_GCM_SHA256,
	      "TLS_ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_GCM, GNUTLS_KX_ECDHE_RSA,
	      GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_ECDHE_RSA_CAMELLIA_256_GCM_SHA384,
		  "TLS_ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384",
		  GNUTLS_CIPHER_CAMELLIA_256_GCM, GNUTLS_KX_ECDHE_RSA,
		  GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),

	ENTRY(GNUTLS_ECDHE_RSA_CHACHA20_POLY1305,
	      "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256",
	      GNUTLS_CIPHER_CHACHA20_POLY1305, GNUTLS_KX_ECDHE_RSA,
	      GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),

	ENTRY(GNUTLS_ECDHE_ECDSA_CHACHA20_POLY1305,
	      "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256",
	      GNUTLS_CIPHER_CHACHA20_POLY1305, GNUTLS_KX_ECDHE_ECDSA,
	      GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),

	ENTRY(GNUTLS_ECDHE_ECDSA_AES_128_CCM,
	      "TLS_ECDHE_ECDSA_WITH_AES_128_CCM", GNUTLS_CIPHER_AES_128_CCM,
	      GNUTLS_KX_ECDHE_ECDSA, GNUTLS_MAC_AEAD, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_ECDHE_ECDSA_AES_256_CCM,
	      "TLS_ECDHE_ECDSA_WITH_AES_256_CCM", GNUTLS_CIPHER_AES_256_CCM,
	      GNUTLS_KX_ECDHE_ECDSA, GNUTLS_MAC_AEAD, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_ECDHE_ECDSA_AES_128_CCM_8,
	      "TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8", GNUTLS_CIPHER_AES_128_CCM_8,
	      GNUTLS_KX_ECDHE_ECDSA, GNUTLS_MAC_AEAD, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_ECDHE_ECDSA_AES_256_CCM_8,
	      "TLS_ECDHE_ECDSA_WITH_AES_256_CCM_8", GNUTLS_CIPHER_AES_256_CCM_8,
	      GNUTLS_KX_ECDHE_ECDSA, GNUTLS_MAC_AEAD, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
#endif
#ifdef ENABLE_PSK
	/* ECC - PSK */
	ENTRY(GNUTLS_ECDHE_PSK_3DES_EDE_CBC_SHA1,
	      "TLS_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA", GNUTLS_CIPHER_3DES_CBC,
	      GNUTLS_KX_ECDHE_PSK, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_ECDHE_PSK_AES_128_CBC_SHA1,
	      "TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_ECDHE_PSK, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_ECDHE_PSK_AES_256_CBC_SHA1,
	      "TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA", GNUTLS_CIPHER_AES_256_CBC,
	      GNUTLS_KX_ECDHE_PSK, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_ECDHE_PSK_AES_128_CBC_SHA256,
	      "TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256",
	      GNUTLS_CIPHER_AES_128_CBC, GNUTLS_KX_ECDHE_PSK, GNUTLS_MAC_SHA256,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_ECDHE_PSK_AES_256_CBC_SHA384,
		  "TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384",
		  GNUTLS_CIPHER_AES_256_CBC, GNUTLS_KX_ECDHE_PSK,
		  GNUTLS_MAC_SHA384, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),
	ENTRY(GNUTLS_ECDHE_PSK_ARCFOUR_128_SHA1,
	      "TLS_ECDHE_PSK_WITH_RC4_128_SHA", GNUTLS_CIPHER_ARCFOUR,
	      GNUTLS_KX_ECDHE_PSK, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_VERSION_UNKNOWN),
	ENTRY(GNUTLS_ECDHE_PSK_NULL_SHA1, "TLS_ECDHE_PSK_WITH_NULL_SHA",
	      GNUTLS_CIPHER_NULL, GNUTLS_KX_ECDHE_PSK, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_ECDHE_PSK_NULL_SHA256, "TLS_ECDHE_PSK_WITH_NULL_SHA256",
	      GNUTLS_CIPHER_NULL, GNUTLS_KX_ECDHE_PSK, GNUTLS_MAC_SHA256,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_ECDHE_PSK_NULL_SHA384,
		  "TLS_ECDHE_PSK_WITH_NULL_SHA384", GNUTLS_CIPHER_NULL,
		  GNUTLS_KX_ECDHE_PSK, GNUTLS_MAC_SHA384, GNUTLS_TLS1,
		  GNUTLS_DTLS1_2, GNUTLS_MAC_SHA384),
	ENTRY(GNUTLS_ECDHE_PSK_CAMELLIA_128_CBC_SHA256,
	      "TLS_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_CBC, GNUTLS_KX_ECDHE_PSK,
	      GNUTLS_MAC_SHA256, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_ECDHE_PSK_CAMELLIA_256_CBC_SHA384,
		  "TLS_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384",
		  GNUTLS_CIPHER_CAMELLIA_256_CBC, GNUTLS_KX_ECDHE_PSK,
		  GNUTLS_MAC_SHA384, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),

	/* PSK */
	ENTRY(GNUTLS_PSK_ARCFOUR_128_SHA1, "TLS_PSK_WITH_RC4_128_SHA",
	      GNUTLS_CIPHER_ARCFOUR, GNUTLS_KX_PSK, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_VERSION_UNKNOWN),
	ENTRY(GNUTLS_PSK_3DES_EDE_CBC_SHA1, "TLS_PSK_WITH_3DES_EDE_CBC_SHA",
	      GNUTLS_CIPHER_3DES_CBC, GNUTLS_KX_PSK, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_PSK_AES_128_CBC_SHA1, "TLS_PSK_WITH_AES_128_CBC_SHA",
	      GNUTLS_CIPHER_AES_128_CBC, GNUTLS_KX_PSK, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_PSK_AES_256_CBC_SHA1, "TLS_PSK_WITH_AES_256_CBC_SHA",
	      GNUTLS_CIPHER_AES_256_CBC, GNUTLS_KX_PSK, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_PSK_AES_128_CBC_SHA256, "TLS_PSK_WITH_AES_128_CBC_SHA256",
	      GNUTLS_CIPHER_AES_128_CBC, GNUTLS_KX_PSK, GNUTLS_MAC_SHA256,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_PSK_AES_256_GCM_SHA384,
		  "TLS_PSK_WITH_AES_256_GCM_SHA384", GNUTLS_CIPHER_AES_256_GCM,
		  GNUTLS_KX_PSK, GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),
	ENTRY(GNUTLS_PSK_CAMELLIA_128_GCM_SHA256,
	      "TLS_PSK_WITH_CAMELLIA_128_GCM_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_GCM, GNUTLS_KX_PSK, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_PSK_CAMELLIA_256_GCM_SHA384,
		  "TLS_PSK_WITH_CAMELLIA_256_GCM_SHA384",
		  GNUTLS_CIPHER_CAMELLIA_256_GCM, GNUTLS_KX_PSK,
		  GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),

	ENTRY(GNUTLS_PSK_AES_128_GCM_SHA256, "TLS_PSK_WITH_AES_128_GCM_SHA256",
	      GNUTLS_CIPHER_AES_128_GCM, GNUTLS_KX_PSK, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_PSK_NULL_SHA1, "TLS_PSK_WITH_NULL_SHA", GNUTLS_CIPHER_NULL,
	      GNUTLS_KX_PSK, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_PSK_NULL_SHA256, "TLS_PSK_WITH_NULL_SHA256",
	      GNUTLS_CIPHER_NULL, GNUTLS_KX_PSK, GNUTLS_MAC_SHA256,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_PSK_CAMELLIA_128_CBC_SHA256,
	      "TLS_PSK_WITH_CAMELLIA_128_CBC_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_CBC, GNUTLS_KX_PSK, GNUTLS_MAC_SHA256,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_PSK_CAMELLIA_256_CBC_SHA384,
		  "TLS_PSK_WITH_CAMELLIA_256_CBC_SHA384",
		  GNUTLS_CIPHER_CAMELLIA_256_CBC, GNUTLS_KX_PSK,
		  GNUTLS_MAC_SHA384, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),

	ENTRY_PRF(GNUTLS_PSK_AES_256_CBC_SHA384,
		  "TLS_PSK_WITH_AES_256_CBC_SHA384", GNUTLS_CIPHER_AES_256_CBC,
		  GNUTLS_KX_PSK, GNUTLS_MAC_SHA384, GNUTLS_TLS1_2,
		  GNUTLS_DTLS1_2, GNUTLS_MAC_SHA384),
	ENTRY_PRF(GNUTLS_PSK_NULL_SHA384, "TLS_PSK_WITH_NULL_SHA384",
		  GNUTLS_CIPHER_NULL, GNUTLS_KX_PSK, GNUTLS_MAC_SHA384,
		  GNUTLS_TLS1_2, GNUTLS_DTLS1_2, GNUTLS_MAC_SHA384),

	/* RSA-PSK */
	ENTRY(GNUTLS_RSA_PSK_ARCFOUR_128_SHA1, "TLS_RSA_PSK_WITH_RC4_128_SHA",
	      GNUTLS_CIPHER_ARCFOUR, GNUTLS_KX_RSA_PSK, GNUTLS_MAC_SHA1,
	      GNUTLS_TLS1, GNUTLS_VERSION_UNKNOWN),
	ENTRY(GNUTLS_RSA_PSK_3DES_EDE_CBC_SHA1,
	      "TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA", GNUTLS_CIPHER_3DES_CBC,
	      GNUTLS_KX_RSA_PSK, GNUTLS_MAC_SHA1, GNUTLS_TLS1,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_RSA_PSK_AES_128_CBC_SHA1,
	      "TLS_RSA_PSK_WITH_AES_128_CBC_SHA", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_RSA_PSK, GNUTLS_MAC_SHA1, GNUTLS_TLS1,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_RSA_PSK_AES_256_CBC_SHA1,
	      "TLS_RSA_PSK_WITH_AES_256_CBC_SHA", GNUTLS_CIPHER_AES_256_CBC,
	      GNUTLS_KX_RSA_PSK, GNUTLS_MAC_SHA1, GNUTLS_TLS1,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_RSA_PSK_CAMELLIA_128_GCM_SHA256,
	      "TLS_RSA_PSK_WITH_CAMELLIA_128_GCM_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_GCM, GNUTLS_KX_RSA_PSK,
	      GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_RSA_PSK_CAMELLIA_256_GCM_SHA384,
		  "TLS_RSA_PSK_WITH_CAMELLIA_256_GCM_SHA384",
		  GNUTLS_CIPHER_CAMELLIA_256_GCM, GNUTLS_KX_RSA_PSK,
		  GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),

	ENTRY(GNUTLS_RSA_PSK_AES_128_GCM_SHA256,
	      "TLS_RSA_PSK_WITH_AES_128_GCM_SHA256", GNUTLS_CIPHER_AES_128_GCM,
	      GNUTLS_KX_RSA_PSK, GNUTLS_MAC_AEAD, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_RSA_PSK_AES_128_CBC_SHA256,
	      "TLS_RSA_PSK_WITH_AES_128_CBC_SHA256", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_RSA_PSK, GNUTLS_MAC_SHA256, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_RSA_PSK_NULL_SHA1, "TLS_RSA_PSK_WITH_NULL_SHA",
	      GNUTLS_CIPHER_NULL, GNUTLS_KX_RSA_PSK, GNUTLS_MAC_SHA1,
	      GNUTLS_TLS1, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_RSA_PSK_NULL_SHA256, "TLS_RSA_PSK_WITH_NULL_SHA256",
	      GNUTLS_CIPHER_NULL, GNUTLS_KX_RSA_PSK, GNUTLS_MAC_SHA256,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_RSA_PSK_AES_256_GCM_SHA384,
		  "TLS_RSA_PSK_WITH_AES_256_GCM_SHA384",
		  GNUTLS_CIPHER_AES_256_GCM, GNUTLS_KX_RSA_PSK, GNUTLS_MAC_AEAD,
		  GNUTLS_TLS1_2, GNUTLS_DTLS1_2, GNUTLS_MAC_SHA384),
	ENTRY_PRF(GNUTLS_RSA_PSK_AES_256_CBC_SHA384,
		  "TLS_RSA_PSK_WITH_AES_256_CBC_SHA384",
		  GNUTLS_CIPHER_AES_256_CBC, GNUTLS_KX_RSA_PSK,
		  GNUTLS_MAC_SHA384, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),
	ENTRY_PRF(GNUTLS_RSA_PSK_NULL_SHA384, "TLS_RSA_PSK_WITH_NULL_SHA384",
		  GNUTLS_CIPHER_NULL, GNUTLS_KX_RSA_PSK, GNUTLS_MAC_SHA384,
		  GNUTLS_TLS1_2, GNUTLS_DTLS1_2, GNUTLS_MAC_SHA384),
	ENTRY(GNUTLS_RSA_PSK_CAMELLIA_128_CBC_SHA256,
	      "TLS_RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_CBC, GNUTLS_KX_RSA_PSK,
	      GNUTLS_MAC_SHA256, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_RSA_PSK_CAMELLIA_256_CBC_SHA384,
		  "TLS_RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384",
		  GNUTLS_CIPHER_CAMELLIA_256_CBC, GNUTLS_KX_RSA_PSK,
		  GNUTLS_MAC_SHA384, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),

	/* DHE-PSK */
	ENTRY(GNUTLS_DHE_PSK_ARCFOUR_128_SHA1, "TLS_DHE_PSK_WITH_RC4_128_SHA",
	      GNUTLS_CIPHER_ARCFOUR, GNUTLS_KX_DHE_PSK, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_VERSION_UNKNOWN),
	ENTRY(GNUTLS_DHE_PSK_3DES_EDE_CBC_SHA1,
	      "TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA", GNUTLS_CIPHER_3DES_CBC,
	      GNUTLS_KX_DHE_PSK, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DHE_PSK_AES_128_CBC_SHA1,
	      "TLS_DHE_PSK_WITH_AES_128_CBC_SHA", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_DHE_PSK, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DHE_PSK_AES_256_CBC_SHA1,
	      "TLS_DHE_PSK_WITH_AES_256_CBC_SHA", GNUTLS_CIPHER_AES_256_CBC,
	      GNUTLS_KX_DHE_PSK, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DHE_PSK_AES_128_CBC_SHA256,
	      "TLS_DHE_PSK_WITH_AES_128_CBC_SHA256", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_DHE_PSK, GNUTLS_MAC_SHA256, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DHE_PSK_AES_128_GCM_SHA256,
	      "TLS_DHE_PSK_WITH_AES_128_GCM_SHA256", GNUTLS_CIPHER_AES_128_GCM,
	      GNUTLS_KX_DHE_PSK, GNUTLS_MAC_AEAD, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DHE_PSK_NULL_SHA1, "TLS_DHE_PSK_WITH_NULL_SHA",
	      GNUTLS_CIPHER_NULL, GNUTLS_KX_DHE_PSK, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DHE_PSK_NULL_SHA256, "TLS_DHE_PSK_WITH_NULL_SHA256",
	      GNUTLS_CIPHER_NULL, GNUTLS_KX_DHE_PSK, GNUTLS_MAC_SHA256,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_DHE_PSK_NULL_SHA384, "TLS_DHE_PSK_WITH_NULL_SHA384",
		  GNUTLS_CIPHER_NULL, GNUTLS_KX_DHE_PSK, GNUTLS_MAC_SHA384,
		  GNUTLS_TLS1_2, GNUTLS_DTLS1_2, GNUTLS_MAC_SHA384),
	ENTRY_PRF(GNUTLS_DHE_PSK_AES_256_CBC_SHA384,
		  "TLS_DHE_PSK_WITH_AES_256_CBC_SHA384",
		  GNUTLS_CIPHER_AES_256_CBC, GNUTLS_KX_DHE_PSK,
		  GNUTLS_MAC_SHA384, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),
	ENTRY_PRF(GNUTLS_DHE_PSK_AES_256_GCM_SHA384,
		  "TLS_DHE_PSK_WITH_AES_256_GCM_SHA384",
		  GNUTLS_CIPHER_AES_256_GCM, GNUTLS_KX_DHE_PSK, GNUTLS_MAC_AEAD,
		  GNUTLS_TLS1_2, GNUTLS_DTLS1_2, GNUTLS_MAC_SHA384),
	ENTRY(GNUTLS_DHE_PSK_CAMELLIA_128_CBC_SHA256,
	      "TLS_DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_CBC, GNUTLS_KX_DHE_PSK,
	      GNUTLS_MAC_SHA256, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_DHE_PSK_CAMELLIA_256_CBC_SHA384,
		  "TLS_DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384",
		  GNUTLS_CIPHER_CAMELLIA_256_CBC, GNUTLS_KX_DHE_PSK,
		  GNUTLS_MAC_SHA384, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),
	ENTRY(GNUTLS_DHE_PSK_CAMELLIA_128_GCM_SHA256,
	      "TLS_DHE_PSK_WITH_CAMELLIA_128_GCM_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_GCM, GNUTLS_KX_DHE_PSK,
	      GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_DHE_PSK_CAMELLIA_256_GCM_SHA384,
		  "TLS_DHE_PSK_WITH_CAMELLIA_256_GCM_SHA384",
		  GNUTLS_CIPHER_CAMELLIA_256_GCM, GNUTLS_KX_DHE_PSK,
		  GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),

	ENTRY(GNUTLS_PSK_AES_128_CCM, "TLS_PSK_WITH_AES_128_CCM",
	      GNUTLS_CIPHER_AES_128_CCM, GNUTLS_KX_PSK, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_PSK_AES_256_CCM, "TLS_PSK_WITH_AES_256_CCM",
	      GNUTLS_CIPHER_AES_256_CCM, GNUTLS_KX_PSK, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DHE_PSK_AES_128_CCM, "TLS_DHE_PSK_WITH_AES_128_CCM",
	      GNUTLS_CIPHER_AES_128_CCM, GNUTLS_KX_DHE_PSK, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DHE_PSK_AES_256_CCM, "TLS_DHE_PSK_WITH_AES_256_CCM",
	      GNUTLS_CIPHER_AES_256_CCM, GNUTLS_KX_DHE_PSK, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_PSK_AES_128_CCM_8, "TLS_PSK_WITH_AES_128_CCM_8",
	      GNUTLS_CIPHER_AES_128_CCM_8, GNUTLS_KX_PSK, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_PSK_AES_256_CCM_8, "TLS_PSK_WITH_AES_256_CCM_8",
	      GNUTLS_CIPHER_AES_256_CCM_8, GNUTLS_KX_PSK, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DHE_PSK_AES_128_CCM_8, "TLS_PSK_DHE_WITH_AES_128_CCM_8",
	      GNUTLS_CIPHER_AES_128_CCM_8, GNUTLS_KX_DHE_PSK, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DHE_PSK_AES_256_CCM_8, "TLS_PSK_DHE_WITH_AES_256_CCM_8",
	      GNUTLS_CIPHER_AES_256_CCM_8, GNUTLS_KX_DHE_PSK, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DHE_PSK_CHACHA20_POLY1305,
	      "TLS_DHE_PSK_WITH_CHACHA20_POLY1305_SHA256",
	      GNUTLS_CIPHER_CHACHA20_POLY1305, GNUTLS_KX_DHE_PSK,
	      GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_ECDHE_PSK_CHACHA20_POLY1305,
	      "TLS_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256",
	      GNUTLS_CIPHER_CHACHA20_POLY1305, GNUTLS_KX_ECDHE_PSK,
	      GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),

	ENTRY(GNUTLS_RSA_PSK_CHACHA20_POLY1305,
	      "TLS_RSA_PSK_WITH_CHACHA20_POLY1305_SHA256",
	      GNUTLS_CIPHER_CHACHA20_POLY1305, GNUTLS_KX_RSA_PSK,
	      GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),

	ENTRY(GNUTLS_PSK_CHACHA20_POLY1305,
	      "TLS_PSK_WITH_CHACHA20_POLY1305_SHA256",
	      GNUTLS_CIPHER_CHACHA20_POLY1305, GNUTLS_KX_PSK, GNUTLS_MAC_AEAD,
	      GNUTLS_TLS1_2, GNUTLS_DTLS1_2),

#endif
#ifdef ENABLE_ANON
	/* DH_ANON */
	ENTRY(GNUTLS_DH_ANON_ARCFOUR_128_MD5, "TLS_DH_anon_WITH_RC4_128_MD5",
	      GNUTLS_CIPHER_ARCFOUR_128, GNUTLS_KX_ANON_DH, GNUTLS_MAC_MD5,
	      GNUTLS_SSL3, GNUTLS_VERSION_UNKNOWN),
	ENTRY(GNUTLS_DH_ANON_3DES_EDE_CBC_SHA1,
	      "TLS_DH_anon_WITH_3DES_EDE_CBC_SHA", GNUTLS_CIPHER_3DES_CBC,
	      GNUTLS_KX_ANON_DH, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DH_ANON_AES_128_CBC_SHA1,
	      "TLS_DH_anon_WITH_AES_128_CBC_SHA", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_ANON_DH, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DH_ANON_AES_256_CBC_SHA1,
	      "TLS_DH_anon_WITH_AES_256_CBC_SHA", GNUTLS_CIPHER_AES_256_CBC,
	      GNUTLS_KX_ANON_DH, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DH_ANON_CAMELLIA_128_CBC_SHA256,
	      "TLS_DH_anon_WITH_CAMELLIA_128_CBC_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_CBC, GNUTLS_KX_ANON_DH,
	      GNUTLS_MAC_SHA256, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DH_ANON_CAMELLIA_256_CBC_SHA256,
	      "TLS_DH_anon_WITH_CAMELLIA_256_CBC_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_256_CBC, GNUTLS_KX_ANON_DH,
	      GNUTLS_MAC_SHA256, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DH_ANON_CAMELLIA_128_CBC_SHA1,
	      "TLS_DH_anon_WITH_CAMELLIA_128_CBC_SHA",
	      GNUTLS_CIPHER_CAMELLIA_128_CBC, GNUTLS_KX_ANON_DH,
	      GNUTLS_MAC_SHA1, GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DH_ANON_CAMELLIA_256_CBC_SHA1,
	      "TLS_DH_anon_WITH_CAMELLIA_256_CBC_SHA",
	      GNUTLS_CIPHER_CAMELLIA_256_CBC, GNUTLS_KX_ANON_DH,
	      GNUTLS_MAC_SHA1, GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_DH_ANON_AES_128_CBC_SHA256,
	      "TLS_DH_anon_WITH_AES_128_CBC_SHA256", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_ANON_DH, GNUTLS_MAC_SHA256, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DH_ANON_AES_256_CBC_SHA256,
	      "TLS_DH_anon_WITH_AES_256_CBC_SHA256", GNUTLS_CIPHER_AES_256_CBC,
	      GNUTLS_KX_ANON_DH, GNUTLS_MAC_SHA256, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	ENTRY(GNUTLS_DH_ANON_AES_128_GCM_SHA256,
	      "TLS_DH_anon_WITH_AES_128_GCM_SHA256", GNUTLS_CIPHER_AES_128_GCM,
	      GNUTLS_KX_ANON_DH, GNUTLS_MAC_AEAD, GNUTLS_TLS1_2,
	      GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_DH_ANON_AES_256_GCM_SHA384,
		  "TLS_DH_anon_WITH_AES_256_GCM_SHA384",
		  GNUTLS_CIPHER_AES_256_GCM, GNUTLS_KX_ANON_DH, GNUTLS_MAC_AEAD,
		  GNUTLS_TLS1_2, GNUTLS_DTLS1_2, GNUTLS_MAC_SHA384),
	ENTRY(GNUTLS_DH_ANON_CAMELLIA_128_GCM_SHA256,
	      "TLS_DH_anon_WITH_CAMELLIA_128_GCM_SHA256",
	      GNUTLS_CIPHER_CAMELLIA_128_GCM, GNUTLS_KX_ANON_DH,
	      GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2),
	ENTRY_PRF(GNUTLS_DH_ANON_CAMELLIA_256_GCM_SHA384,
		  "TLS_DH_anon_WITH_CAMELLIA_256_GCM_SHA384",
		  GNUTLS_CIPHER_CAMELLIA_256_GCM, GNUTLS_KX_ANON_DH,
		  GNUTLS_MAC_AEAD, GNUTLS_TLS1_2, GNUTLS_DTLS1_2,
		  GNUTLS_MAC_SHA384),

	/* ECC-ANON */
	ENTRY(GNUTLS_ECDH_ANON_NULL_SHA1, "TLS_ECDH_anon_WITH_NULL_SHA",
	      GNUTLS_CIPHER_NULL, GNUTLS_KX_ANON_ECDH, GNUTLS_MAC_SHA1,
	      GNUTLS_SSL3, GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_ECDH_ANON_3DES_EDE_CBC_SHA1,
	      "TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA", GNUTLS_CIPHER_3DES_CBC,
	      GNUTLS_KX_ANON_ECDH, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_ECDH_ANON_AES_128_CBC_SHA1,
	      "TLS_ECDH_anon_WITH_AES_128_CBC_SHA", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_ANON_ECDH, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_ECDH_ANON_AES_256_CBC_SHA1,
	      "TLS_ECDH_anon_WITH_AES_256_CBC_SHA", GNUTLS_CIPHER_AES_256_CBC,
	      GNUTLS_KX_ANON_ECDH, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_ECDH_ANON_ARCFOUR_128_SHA1,
	      "TLS_ECDH_anon_WITH_RC4_128_SHA", GNUTLS_CIPHER_ARCFOUR,
	      GNUTLS_KX_ANON_ECDH, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_VERSION_UNKNOWN),
#endif
#ifdef ENABLE_SRP
	/* SRP */
	ENTRY(GNUTLS_SRP_SHA_3DES_EDE_CBC_SHA1,
	      "TLS_SRP_SHA_WITH_3DES_EDE_CBC_SHA", GNUTLS_CIPHER_3DES_CBC,
	      GNUTLS_KX_SRP, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_SRP_SHA_AES_128_CBC_SHA1,
	      "TLS_SRP_SHA_WITH_AES_128_CBC_SHA", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_SRP, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
	ENTRY(GNUTLS_SRP_SHA_AES_256_CBC_SHA1,
	      "TLS_SRP_SHA_WITH_AES_256_CBC_SHA", GNUTLS_CIPHER_AES_256_CBC,
	      GNUTLS_KX_SRP, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),

	ENTRY(GNUTLS_SRP_SHA_DSS_3DES_EDE_CBC_SHA1,
	      "TLS_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA", GNUTLS_CIPHER_3DES_CBC,
	      GNUTLS_KX_SRP_DSS, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),

	ENTRY(GNUTLS_SRP_SHA_RSA_3DES_EDE_CBC_SHA1,
	      "TLS_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA", GNUTLS_CIPHER_3DES_CBC,
	      GNUTLS_KX_SRP_RSA, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),

	ENTRY(GNUTLS_SRP_SHA_DSS_AES_128_CBC_SHA1,
	      "TLS_SRP_SHA_DSS_WITH_AES_128_CBC_SHA", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_SRP_DSS, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),

	ENTRY(GNUTLS_SRP_SHA_RSA_AES_128_CBC_SHA1,
	      "TLS_SRP_SHA_RSA_WITH_AES_128_CBC_SHA", GNUTLS_CIPHER_AES_128_CBC,
	      GNUTLS_KX_SRP_RSA, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),

	ENTRY(GNUTLS_SRP_SHA_DSS_AES_256_CBC_SHA1,
	      "TLS_SRP_SHA_DSS_WITH_AES_256_CBC_SHA", GNUTLS_CIPHER_AES_256_CBC,
	      GNUTLS_KX_SRP_DSS, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),

	ENTRY(GNUTLS_SRP_SHA_RSA_AES_256_CBC_SHA1,
	      "TLS_SRP_SHA_RSA_WITH_AES_256_CBC_SHA", GNUTLS_CIPHER_AES_256_CBC,
	      GNUTLS_KX_SRP_RSA, GNUTLS_MAC_SHA1, GNUTLS_SSL3,
	      GNUTLS_DTLS_VERSION_MIN),
#endif

#ifdef ENABLE_GOST
	ENTRY_PRF(GNUTLS_GOSTR341112_256_28147_CNT_IMIT,
		  "TLS_GOSTR341112_256_WITH_28147_CNT_IMIT",
		  GNUTLS_CIPHER_GOST28147_TC26Z_CNT, GNUTLS_KX_VKO_GOST_12,
		  GNUTLS_MAC_GOST28147_TC26Z_IMIT, GNUTLS_TLS1_2,
		  GNUTLS_VERSION_UNKNOWN, GNUTLS_MAC_STREEBOG_256),
#endif

	{ 0, { 0, 0 }, 0, 0, 0, 0, 0, 0 }
};

#define CIPHER_SUITE_LOOP(b)                                    \
	{                                                       \
		const gnutls_cipher_suite_entry_st *p;          \
		for (p = cs_algorithms; p->name != NULL; p++) { \
			b;                                      \
		}                                               \
	}

#define CIPHER_SUITE_ALG_LOOP(a, suite)                                 \
	CIPHER_SUITE_LOOP(                                              \
		if ((p->id[0] == suite[0]) && (p->id[1] == suite[1])) { \
			a;                                              \
			break;                                          \
		})

/* Cipher Suite's functions */
const gnutls_cipher_suite_entry_st *ciphersuite_to_entry(const uint8_t suite[2])
{
	CIPHER_SUITE_ALG_LOOP(return p, suite);
	return NULL;
}

gnutls_kx_algorithm_t _gnutls_cipher_suite_get_kx_algo(const uint8_t suite[2])
{
	gnutls_kx_algorithm_t ret = GNUTLS_KX_UNKNOWN;

	CIPHER_SUITE_ALG_LOOP(ret = p->kx_algorithm, suite);
	return ret;
}

const char *_gnutls_cipher_suite_get_name(const uint8_t suite[2])
{
	const char *ret = NULL;

	/* avoid prefix */
	CIPHER_SUITE_ALG_LOOP(ret = p->name + sizeof("GNUTLS_") - 1, suite);

	return ret;
}

const gnutls_cipher_suite_entry_st *
cipher_suite_get(gnutls_kx_algorithm_t kx_algorithm,
		 gnutls_cipher_algorithm_t cipher_algorithm,
		 gnutls_mac_algorithm_t mac_algorithm)
{
	const gnutls_cipher_suite_entry_st *ret = NULL;

	CIPHER_SUITE_LOOP(if (kx_algorithm == p->kx_algorithm &&
			      cipher_algorithm == p->block_algorithm &&
			      mac_algorithm == p->mac_algorithm) {
		ret = p;
		break;
	});

	return ret;
}

/* Returns 0 if the given KX has not the corresponding parameters
 * (DH or RSA) set up. Otherwise returns 1.
 */
static unsigned check_server_dh_params(gnutls_session_t session,
				       unsigned cred_type,
				       gnutls_kx_algorithm_t kx)
{
	unsigned have_dh_params = 0;

	if (!_gnutls_kx_needs_dh_params(kx)) {
		return 1;
	}

	if (session->internals.hsk_flags & HSK_HAVE_FFDHE) {
		/* if the client has advertized FFDHE then it doesn't matter
		 * whether we have server DH parameters. They are no good. */
		gnutls_assert();
		return 0;
	}

	/* Read the Diffie-Hellman parameters, if any.
	 */
	if (cred_type == GNUTLS_CRD_CERTIFICATE) {
		gnutls_certificate_credentials_t x509_cred =
			(gnutls_certificate_credentials_t)_gnutls_get_cred(
				session, cred_type);

		if (x509_cred != NULL &&
		    (x509_cred->dh_params || x509_cred->params_func ||
		     x509_cred->dh_sec_param)) {
			have_dh_params = 1;
		}

#ifdef ENABLE_ANON
	} else if (cred_type == GNUTLS_CRD_ANON) {
		gnutls_anon_server_credentials_t anon_cred =
			(gnutls_anon_server_credentials_t)_gnutls_get_cred(
				session, cred_type);

		if (anon_cred != NULL &&
		    (anon_cred->dh_params || anon_cred->params_func ||
		     anon_cred->dh_sec_param)) {
			have_dh_params = 1;
		}
#endif
#ifdef ENABLE_PSK
	} else if (cred_type == GNUTLS_CRD_PSK) {
		gnutls_psk_server_credentials_t psk_cred =
			(gnutls_psk_server_credentials_t)_gnutls_get_cred(
				session, cred_type);

		if (psk_cred != NULL &&
		    (psk_cred->dh_params || psk_cred->params_func ||
		     psk_cred->dh_sec_param)) {
			have_dh_params = 1;
		}
#endif
	} else {
		return 1; /* no need for params */
	}

	return have_dh_params;
}

/**
 * gnutls_cipher_suite_get_name:
 * @kx_algorithm: is a Key exchange algorithm
 * @cipher_algorithm: is a cipher algorithm
 * @mac_algorithm: is a MAC algorithm
 *
 * This function returns the ciphersuite name under TLS1.2 or earlier
 * versions when provided with individual algorithms. The full cipher suite
 * name must be prepended by TLS or SSL depending of the protocol in use.
 *
 * To get a description of the current ciphersuite across versions, it
 * is recommended to use gnutls_session_get_desc().
 *
 * Returns: a string that contains the name of a TLS cipher suite,
 * specified by the given algorithms, or %NULL.
 **/
const char *
gnutls_cipher_suite_get_name(gnutls_kx_algorithm_t kx_algorithm,
			     gnutls_cipher_algorithm_t cipher_algorithm,
			     gnutls_mac_algorithm_t mac_algorithm)
{
	const gnutls_cipher_suite_entry_st *ce;

	ce = cipher_suite_get(kx_algorithm, cipher_algorithm, mac_algorithm);
	if (ce == NULL)
		return NULL;
	else
		return ce->name + sizeof("GNUTLS_") - 1;
}

/*-
 * _gnutls_cipher_suite_get_id:
 * @kx_algorithm: is a Key exchange algorithm
 * @cipher_algorithm: is a cipher algorithm
 * @mac_algorithm: is a MAC algorithm
 * @suite: The id to be returned
 *
 * This function returns the ciphersuite ID in @suite, under TLS1.2 or earlier
 * versions when provided with individual algorithms.
 *
 * Returns: 0 on success or a negative error code otherwise.
 -*/
int _gnutls_cipher_suite_get_id(gnutls_kx_algorithm_t kx_algorithm,
				gnutls_cipher_algorithm_t cipher_algorithm,
				gnutls_mac_algorithm_t mac_algorithm,
				uint8_t suite[2])
{
	const gnutls_cipher_suite_entry_st *ce;

	ce = cipher_suite_get(kx_algorithm, cipher_algorithm, mac_algorithm);
	if (ce == NULL)
		return GNUTLS_E_INVALID_REQUEST;
	else {
		suite[0] = ce->id[0];
		suite[1] = ce->id[1];
	}
	return 0;
}

/**
 * gnutls_cipher_suite_info:
 * @idx: index of cipher suite to get information about, starts on 0.
 * @cs_id: output buffer with room for 2 bytes, indicating cipher suite value
 * @kx: output variable indicating key exchange algorithm, or %NULL.
 * @cipher: output variable indicating cipher, or %NULL.
 * @mac: output variable indicating MAC algorithm, or %NULL.
 * @min_version: output variable indicating TLS protocol version, or %NULL.
 *
 * Get information about supported cipher suites.  Use the function
 * iteratively to get information about all supported cipher suites.
 * Call with idx=0 to get information about first cipher suite, then
 * idx=1 and so on until the function returns NULL.
 *
 * Returns: the name of @idx cipher suite, and set the information
 * about the cipher suite in the output variables.  If @idx is out of
 * bounds, %NULL is returned.
 **/
const char *gnutls_cipher_suite_info(size_t idx, unsigned char *cs_id,
				     gnutls_kx_algorithm_t *kx,
				     gnutls_cipher_algorithm_t *cipher,
				     gnutls_mac_algorithm_t *mac,
				     gnutls_protocol_t *min_version)
{
	if (idx >= CIPHER_SUITES_COUNT)
		return NULL;

	if (cs_id)
		memcpy(cs_id, cs_algorithms[idx].id, 2);
	if (kx)
		*kx = cs_algorithms[idx].kx_algorithm;
	if (cipher)
		*cipher = cs_algorithms[idx].block_algorithm;
	if (mac)
		*mac = cs_algorithms[idx].mac_algorithm;
	if (min_version)
		*min_version = cs_algorithms[idx].min_version;

	return cs_algorithms[idx].name + sizeof("GNU") - 1;
}

#define VERSION_CHECK(entry)                                             \
	if (is_dtls) {                                                   \
		if (entry->min_dtls_version == GNUTLS_VERSION_UNKNOWN || \
		    version->id < entry->min_dtls_version ||             \
		    version->id > entry->max_dtls_version)               \
			continue;                                        \
	} else {                                                         \
		if (entry->min_version == GNUTLS_VERSION_UNKNOWN ||      \
		    version->id < entry->min_version ||                  \
		    version->id > entry->max_version)                    \
			continue;                                        \
	}

#define CIPHER_CHECK(algo)                                           \
	if (session->internals.priorities->force_etm && !have_etm) { \
		const cipher_entry_st *_cipher;                      \
		_cipher = cipher_to_entry(algo);                     \
		if (_cipher == NULL ||                               \
		    _gnutls_cipher_type(_cipher) == CIPHER_BLOCK)    \
			continue;                                    \
	}

#define KX_SRP_CHECKS(kx, action)                                 \
	if (kx == GNUTLS_KX_SRP_RSA || kx == GNUTLS_KX_SRP_DSS) { \
		if (!_gnutls_get_cred(session, GNUTLS_CRD_SRP)) { \
			action;                                   \
		}                                                 \
	}

static unsigned kx_is_ok(gnutls_session_t session, gnutls_kx_algorithm_t kx,
			 unsigned cred_type,
			 const gnutls_group_entry_st **sgroup)
{
	if (_gnutls_kx_is_ecc(kx)) {
		if (session->internals.cand_ec_group == NULL) {
			return 0;
		} else {
			*sgroup = session->internals.cand_ec_group;
		}
	} else if (_gnutls_kx_is_dhe(kx)) {
		if (session->internals.cand_dh_group == NULL) {
			if (!check_server_dh_params(session, cred_type, kx)) {
				return 0;
			}
		} else {
			*sgroup = session->internals.cand_dh_group;
		}
	}
	KX_SRP_CHECKS(kx, return 0);

	return 1;
}

/* Called on server-side only */
int _gnutls_figure_common_ciphersuite(gnutls_session_t session,
				      const ciphersuite_list_st *peer_clist,
				      const gnutls_cipher_suite_entry_st **ce)
{
	unsigned int i, j;
	int ret;
	const version_entry_st *version = get_version(session);
	unsigned int is_dtls = IS_DTLS(session);
	gnutls_kx_algorithm_t kx;
	gnutls_credentials_type_t cred_type =
		GNUTLS_CRD_CERTIFICATE; /* default for TLS1.3 */
	const gnutls_group_entry_st *sgroup = NULL;
	gnutls_ext_priv_data_t epriv;
	unsigned have_etm = 0;

	if (version == NULL) {
		return gnutls_assert_val(GNUTLS_E_NO_CIPHER_SUITES);
	}

	/* we figure whether etm is negotiated by checking the raw extension data
	 * because we only set (security_params) EtM to true only after the ciphersuite is
	 * negotiated. */
	ret = _gnutls_hello_ext_get_priv(session, GNUTLS_EXTENSION_ETM, &epriv);
	if (ret >= 0 && ((intptr_t)epriv) != 0)
		have_etm = 1;

	/* If we didn't receive the supported_groups extension, then
	 * we should assume that SECP256R1 is supported; that is required
	 * by RFC4492, probably to allow SSLv2 hellos negotiate elliptic curve
	 * ciphersuites */
	if (!version->tls13_sem && session->internals.cand_ec_group == NULL &&
	    !_gnutls_hello_ext_is_present(session,
					  GNUTLS_EXTENSION_SUPPORTED_GROUPS)) {
		session->internals.cand_ec_group =
			_gnutls_id_to_group(DEFAULT_EC_GROUP);
	}

	if (session->internals.priorities->server_precedence == 0) {
		for (i = 0; i < peer_clist->size; i++) {
			_gnutls_debug_log(
				"checking %.2x.%.2x (%s) for compatibility\n",
				(unsigned)peer_clist->entry[i]->id[0],
				(unsigned)peer_clist->entry[i]->id[1],
				peer_clist->entry[i]->name);
			VERSION_CHECK(peer_clist->entry[i]);

			kx = peer_clist->entry[i]->kx_algorithm;

			CIPHER_CHECK(peer_clist->entry[i]->block_algorithm);

			if (!version->tls13_sem)
				cred_type = _gnutls_map_kx_get_cred(kx, 1);

			for (j = 0; j < session->internals.priorities->cs.size;
			     j++) {
				if (session->internals.priorities->cs.entry[j] ==
				    peer_clist->entry[i]) {
					sgroup = NULL;
					if (!kx_is_ok(session, kx, cred_type,
						      &sgroup))
						continue;

					/* if we have selected PSK, we need a ciphersuites which matches
					 * the selected binder */
					if (session->internals.hsk_flags &
					    HSK_PSK_SELECTED) {
						if (session->key.binders[0]
							    .prf->id !=
						    session->internals
							    .priorities->cs
							    .entry[j]
							    ->prf)
							continue;
					} else if (cred_type ==
						   GNUTLS_CRD_CERTIFICATE) {
						ret = _gnutls_select_server_cert(
							session,
							peer_clist->entry[i]);
						if (ret < 0) {
							/* couldn't select cert with this ciphersuite */
							gnutls_assert();
							break;
						}
					}

					/* select the group based on the selected ciphersuite */
					if (sgroup)
						_gnutls_session_group_set(
							session, sgroup);
					*ce = peer_clist->entry[i];
					return 0;
				}
			}
		}
	} else {
		for (j = 0; j < session->internals.priorities->cs.size; j++) {
			VERSION_CHECK(
				session->internals.priorities->cs.entry[j]);

			CIPHER_CHECK(session->internals.priorities->cs.entry[j]
					     ->block_algorithm);

			for (i = 0; i < peer_clist->size; i++) {
				_gnutls_debug_log(
					"checking %.2x.%.2x (%s) for compatibility\n",
					(unsigned)peer_clist->entry[i]->id[0],
					(unsigned)peer_clist->entry[i]->id[1],
					peer_clist->entry[i]->name);

				if (session->internals.priorities->cs.entry[j] ==
				    peer_clist->entry[i]) {
					sgroup = NULL;
					kx = peer_clist->entry[i]->kx_algorithm;

					if (!version->tls13_sem)
						cred_type =
							_gnutls_map_kx_get_cred(
								kx, 1);

					if (!kx_is_ok(session, kx, cred_type,
						      &sgroup))
						break;

					/* if we have selected PSK, we need a ciphersuites which matches
					 * the selected binder */
					if (session->internals.hsk_flags &
					    HSK_PSK_SELECTED) {
						if (session->key.binders[0]
							    .prf->id !=
						    session->internals
							    .priorities->cs
							    .entry[j]
							    ->prf)
							break;
					} else if (cred_type ==
						   GNUTLS_CRD_CERTIFICATE) {
						ret = _gnutls_select_server_cert(
							session,
							peer_clist->entry[i]);
						if (ret < 0) {
							/* couldn't select cert with this ciphersuite */
							gnutls_assert();
							break;
						}
					}

					/* select the group based on the selected ciphersuite */
					if (sgroup)
						_gnutls_session_group_set(
							session, sgroup);
					*ce = peer_clist->entry[i];
					return 0;
				}
			}
		}
	}

	/* nothing in common */

	return gnutls_assert_val(GNUTLS_E_NO_CIPHER_SUITES);
}

#define CLIENT_VERSION_CHECK(minver, maxver, e)       \
	if (is_dtls) {                                \
		if (e->min_dtls_version > maxver->id) \
			continue;                     \
	} else {                                      \
		if (e->min_version > maxver->id)      \
			continue;                     \
	}

#define RESERVED_CIPHERSUITES 4
int _gnutls_get_client_ciphersuites(gnutls_session_t session,
				    gnutls_buffer_st *cdata,
				    const version_entry_st *vmin,
				    unsigned add_scsv)
{
	unsigned int j;
	int ret;
	unsigned int is_dtls = IS_DTLS(session);
	gnutls_kx_algorithm_t kx;
	gnutls_credentials_type_t cred_type;
	uint8_t cipher_suites[MAX_CIPHERSUITE_SIZE * 2 + RESERVED_CIPHERSUITES];
	unsigned cipher_suites_size = 0;
	size_t init_length = cdata->length;
	const version_entry_st *vmax;

	vmax = _gnutls_version_max(session);
	if (vmax == NULL)
		return gnutls_assert_val(GNUTLS_E_NO_PRIORITIES_WERE_SET);

	for (j = 0; j < session->internals.priorities->cs.size; j++) {
		CLIENT_VERSION_CHECK(
			vmin, vmax, session->internals.priorities->cs.entry[j]);

		kx = session->internals.priorities->cs.entry[j]->kx_algorithm;
		if (kx !=
		    GNUTLS_KX_UNKNOWN) { /* In TLS 1.3 ciphersuites don't map to credentials */
			cred_type = _gnutls_map_kx_get_cred(kx, 0);

			if (!session->internals.premaster_set &&
			    _gnutls_get_cred(session, cred_type) == NULL)
				continue;

			KX_SRP_CHECKS(kx, continue);
		}

		_gnutls_debug_log(
			"Keeping ciphersuite %.2x.%.2x (%s)\n",
			(unsigned)session->internals.priorities->cs.entry[j]
				->id[0],
			(unsigned)session->internals.priorities->cs.entry[j]
				->id[1],
			session->internals.priorities->cs.entry[j]->name);
		cipher_suites[cipher_suites_size] =
			session->internals.priorities->cs.entry[j]->id[0];
		cipher_suites[cipher_suites_size + 1] =
			session->internals.priorities->cs.entry[j]->id[1];
		cipher_suites_size += 2;

		if (cipher_suites_size >= MAX_CIPHERSUITE_SIZE * 2)
			break;
	}
#ifdef ENABLE_SSL3
	if (add_scsv) {
		cipher_suites[cipher_suites_size] = 0x00;
		cipher_suites[cipher_suites_size + 1] = 0xff;
		cipher_suites_size += 2;

		ret = _gnutls_ext_sr_send_cs(session);
		if (ret < 0)
			return gnutls_assert_val(ret);

		_gnutls_hello_ext_save_sr(session);
	}
#endif

	if (session->internals.priorities->fallback) {
		cipher_suites[cipher_suites_size] = GNUTLS_FALLBACK_SCSV_MAJOR;
		cipher_suites[cipher_suites_size + 1] =
			GNUTLS_FALLBACK_SCSV_MINOR;
		cipher_suites_size += 2;
	}

	ret = _gnutls_buffer_append_data_prefix(cdata, 16, cipher_suites,
						cipher_suites_size);
	if (ret < 0)
		return gnutls_assert_val(ret);

	return cdata->length - init_length;
}

/**
 * gnutls_priority_get_cipher_suite_index:
 * @pcache: is a #gnutls_priority_t type.
 * @idx: is an index number.
 * @sidx: internal index of cipher suite to get information about.
 *
 * Provides the internal ciphersuite index to be used with
 * gnutls_cipher_suite_info(). The index @idx provided is an
 * index kept at the priorities structure. It might be that a valid
 * priorities index does not correspond to a ciphersuite and in
 * that case %GNUTLS_E_UNKNOWN_CIPHER_SUITE will be returned.
 * Once the last available index is crossed then
 * %GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE will be returned.
 *
 * Returns: On success it returns %GNUTLS_E_SUCCESS (0), or a negative error value otherwise.
 *
 * Since: 3.0.9
 **/
int gnutls_priority_get_cipher_suite_index(gnutls_priority_t pcache,
					   unsigned int idx, unsigned int *sidx)
{
	unsigned int i, j;
	unsigned max_tls = 0;
	unsigned max_dtls = 0;

	if (idx >= pcache->cs.size)
		return GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE;

	/* find max_tls and max_dtls */
	for (j = 0; j < pcache->protocol.num_priorities; j++) {
		if (pcache->protocol.priorities[j] <= GNUTLS_TLS_VERSION_MAX &&
		    pcache->protocol.priorities[j] >= max_tls) {
			max_tls = pcache->protocol.priorities[j];
		} else if (pcache->protocol.priorities[j] <=
				   GNUTLS_DTLS_VERSION_MAX &&
			   pcache->protocol.priorities[j] >= max_dtls) {
			max_dtls = pcache->protocol.priorities[j];
		}
	}

	for (i = 0; i < CIPHER_SUITES_COUNT; i++) {
		if (pcache->cs.entry[idx] != &cs_algorithms[i])
			continue;

		*sidx = i;
		if (_gnutls_cipher_exists(cs_algorithms[i].block_algorithm) &&
		    _gnutls_mac_exists(cs_algorithms[i].mac_algorithm)) {
			if (max_tls >= cs_algorithms[i].min_version) {
				return 0;
			} else if (max_dtls >=
				   cs_algorithms[i].min_dtls_version) {
				return 0;
			}
		} else
			break;
	}

	return GNUTLS_E_UNKNOWN_CIPHER_SUITE;
}
