#ifndef GNUTLS_UI_H
# define GNUTLS_UI_H


/* Extra definitions */

#define GNUTLS_X509_CN_SIZE 256
#define GNUTLS_X509_C_SIZE 3
#define GNUTLS_X509_O_SIZE 256
#define GNUTLS_X509_OU_SIZE 256
#define GNUTLS_X509_L_SIZE 256
#define GNUTLS_X509_S_SIZE 256
#define GNUTLS_X509_EMAIL_SIZE 256

typedef struct {
	char common_name[GNUTLS_X509_CN_SIZE];
	char country[GNUTLS_X509_C_SIZE];
	char organization[GNUTLS_X509_O_SIZE];
	char organizational_unit_name[GNUTLS_X509_OU_SIZE];
	char locality_name[GNUTLS_X509_L_SIZE];
	char state_or_province_name[GNUTLS_X509_S_SIZE];
	char email[GNUTLS_X509_EMAIL_SIZE];
} gnutls_x509_dn;
#define gnutls_DN gnutls_x509_dn

typedef struct {
	char name[GNUTLS_X509_CN_SIZE];
	char email[GNUTLS_X509_CN_SIZE];
} gnutls_openpgp_name;	

/* For key Usage, test as:
 * if (st.keyUsage & X509KEY_DIGITAL_SIGNATURE) ...
 */
#define GNUTLS_X509KEY_DIGITAL_SIGNATURE 	256
#define GNUTLS_X509KEY_NON_REPUDIATION		128
#define GNUTLS_X509KEY_KEY_ENCIPHERMENT		64
#define GNUTLS_X509KEY_DATA_ENCIPHERMENT	32
#define GNUTLS_X509KEY_KEY_AGREEMENT		16
#define GNUTLS_X509KEY_KEY_CERT_SIGN		8
#define GNUTLS_X509KEY_CRL_SIGN			4
#define GNUTLS_X509KEY_ENCIPHER_ONLY		2
#define GNUTLS_X509KEY_DECIPHER_ONLY		1


# ifdef LIBGNUTLS_VERSION /* These are defined only in gnutls.h */

typedef int gnutls_certificate_client_callback_func(GNUTLS_STATE, const gnutls_datum *, int, const gnutls_datum *, int);
typedef int gnutls_certificate_server_callback_func(GNUTLS_STATE, const gnutls_datum *, int);

/* Functions that allow AUTH_INFO structures handling
 */

GNUTLS_CredType gnutls_auth_get_type( GNUTLS_STATE state);

/* SRP */

const char* gnutls_srp_server_get_username( GNUTLS_STATE state);

/* ANON */

void gnutls_dh_set_bits( GNUTLS_STATE state, int bits);
int gnutls_dh_get_bits( GNUTLS_STATE);

/* X509PKI */

void gnutls_certificate_client_set_select_func( GNUTLS_CERTIFICATE_CREDENTIALS, gnutls_certificate_client_callback_func *);
void gnutls_certificate_server_set_select_func( GNUTLS_CERTIFICATE_CREDENTIALS, gnutls_certificate_server_callback_func *);

void gnutls_certificate_server_set_request( GNUTLS_STATE, GNUTLS_CertificateRequest);

/* X.509 certificate handling functions */
int gnutls_x509_extract_dn( const gnutls_datum*, gnutls_x509_dn*);
int gnutls_x509_extract_certificate_dn( const gnutls_datum*, gnutls_x509_dn*);
int gnutls_x509_extract_certificate_issuer_dn(  const gnutls_datum*, gnutls_x509_dn *);
int gnutls_x509_extract_certificate_version( const gnutls_datum*);
int gnutls_x509_extract_certificate_serial(const gnutls_datum * cert, char* result, int* result_size);
time_t gnutls_x509_extract_certificate_activation_time( const gnutls_datum*);
time_t gnutls_x509_extract_certificate_expiration_time( const gnutls_datum*);
int gnutls_x509_extract_subject_dns_name( const gnutls_datum*, char*, int*);

int gnutls_x509_verify_certificate( const gnutls_datum* cert_list, int cert_list_length, const gnutls_datum * CA_list, int CA_list_length, const gnutls_datum* CRL_list, int CRL_list_length);

/* Openpgp certificate stuff */
int gnutls_openpgp_extract_key_name( const gnutls_datum *cert,
                                                  gnutls_openpgp_name *name);
int gnutls_openpgp_extract_key_version( const gnutls_datum *cert );
time_t gnutls_openpgp_extract_key_activation_time( const gnutls_datum *cert );
time_t gnutls_openpgp_extract_key_expiration_time( const gnutls_datum *cert );

int gnutls_openpgp_verify_key( const gnutls_datum* key_list, int key_list_length);

/* get data from the state */
const gnutls_datum* gnutls_certificate_get_peers( GNUTLS_STATE, int* list_size);
const gnutls_datum *gnutls_certificate_get_ours(GNUTLS_STATE state);
int gnutls_certificate_get_ours_index(GNUTLS_STATE state);

int gnutls_certificate_client_get_request_status(  GNUTLS_STATE);
int gnutls_certificate_verify_peers( GNUTLS_STATE);

int gnutls_b64_encode_fmt( const char* msg, const gnutls_datum *data, char* result, int* result_size);
int gnutls_b64_decode_fmt( const gnutls_datum *b64_data, char* result, int* result_size);

# endif /* LIBGNUTLS_VERSION */

#endif /* GNUTLS_UI_H */
