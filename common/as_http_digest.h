#ifndef __AS_HTTP_DIGEST_H__
#define __AS_HTTP_DIGEST_H__

#define ARRAY_LENGTH(a) (sizeof a / sizeof (a[0]))



typedef unsigned int MD5_u32plus;

typedef struct {
  MD5_u32plus lo, hi;
  MD5_u32plus a, b, c, d;
  unsigned char buffer[64];
  MD5_u32plus block[16];
} as_md5_ctx;


typedef struct {
	char *username;
	char *password;
	char *realm;
	char *nonce;
	unsigned int cnonce;
	char *opaque;
	char *uri;
	unsigned int method;
	char algorithm;
	unsigned int qop;
	unsigned int nc;
    unsigned int quotes;
} as_digest_s;

/* Digest context type (digest struct) */
typedef as_digest_s as_digest_t;

/* The attributes found in a digest string, both WWW-Authenticate and
    Authorization headers.
 */
typedef enum {
	D_ATTR_USERNAME,	/* char * */
	D_ATTR_PASSWORD,	/* char * */
	D_ATTR_REALM,		/* char * */
	D_ATTR_NONCE,		/* char * */
	D_ATTR_CNONCE,		/* int */
	D_ATTR_OPAQUE,		/* char * */
	D_ATTR_URI,		/* char * */
	D_ATTR_METHOD,		/* int */
	D_ATTR_ALGORITHM,	/* int */
	D_ATTR_QOP,		/* int */
	D_ATTR_NONCE_COUNT	/* int */
} as_digest_attr_t;

/* Union type for attribute get/set function  */
typedef union {
	int number;
	char *string;
	const char *const_str; // for supress compiler warnings
} as_digest_attr_value_t;

/* Supported hashing algorithms */
#define DIGEST_ALGORITHM_NOT_SET	0
#define DIGEST_ALGORITHM_MD5		1

/* Quality of Protection (qop) values */
#define DIGEST_QOP_NOT_SET 	0
#define DIGEST_QOP_AUTH 	1
#define DIGEST_QOP_AUTH_INT	2 /* Not supported yet */

/* Method values */
#define DIGEST_METHOD_OPTIONS	1
#define DIGEST_METHOD_GET   	2
#define DIGEST_METHOD_HEAD  	3
#define DIGEST_METHOD_POST  	4
#define DIGEST_METHOD_PUT   	5
#define DIGEST_METHOD_DELETE	6
#define DIGEST_METHOD_TRACE 	7

/*********************************common************************************/
/**
 * Initiate the digest context.
 *
 */
int as_digest_init(as_digest_t *digest,unsigned int quotes);

/**
 * Check if WWW-Authenticate string is digest authentication scheme.
 *
 * @param const char *header_value The value of the WWW-Authentication header.
 *
 * @returns int 0 if digest scheme, otherwise -1.
 */
int as_digest_is_digest(const char *header_value);

/**
 * Get an attribute from a digest context.
 *
 * @param as_digest_t *digest The digest context to get attribute from.
 * @param as_digest_attr_t attr Which attribute to get.
 *
 * @returns void * The attribute value, a C string or a pointer to an int.
 */
void * as_digest_get_attr(as_digest_t *digest, as_digest_attr_t attr);

/**
 * Set an attribute on a digest object.
 *
 * @param as_digest_t *digest The digest context to set attribute to.
 * @param as_digest_attr_t attr Which attribute to set.
 * @param const void *value Value to set the attribute to. If the value
 *        is a string, *value should be a C string (char *). If it is
 *        an integer, *value should be a an integer (unsigned int).
 *
 * @returns int 0 on success, otherwise -1.
 */
int as_digest_set_attr(as_digest_t *digest, as_digest_attr_t attr, const as_digest_attr_value_t value);

/*********************************client************************************/
/**
 * Parse a digest string.
 *
 * @param as_digest_t *digest The digest context.
 * @param char *digest_string The header value of the WWW-Authenticate header.
 *
 * @returns int 0 on success, otherwise -1.
 */
int as_digest_client_parse(as_digest_t *digest, const char *digest_string);

/**
 * Generate the Authorization header value.
 *
 * Attributes that must be set manually before calling this function:
 *
 *  - Username
 *  - Password
 *  - URI
 *  - Method
 *
 * @param as_digest_t *digest The digest context to generate the header value from.
 * @param char *result The buffer to store the generated header value in.
 *
 * Returns the number of bytes in the result string. -1 on failure.
 */
size_t as_digest_client_generate_header(as_digest_t *digest, char *result, size_t max_length);

/******************************Server****************************************/
/**
 * Parse a digest string.
 *
 * @param as_digest_t *digest The digest context.
 * @param char *digest_string The header value of the WWW-Authenticate header.
 *
 * @returns int 0 on success, otherwise -1.
 */
int as_digest_server_parse(as_digest_t *digest, const char *digest_string);

/**
 * Generate a nonce for a digest context.
 *
 * @param as_digest_t *digest The digest context.
 *
 * @returns int 0 on success, otherwise -1.
 */
int as_digest_server_generate_nonce(as_digest_t *digest);

/**
 * Generate the WWW-Authenticate header value.
 *
 * Attributes that must be set manually before calling this function:
 *
 *  - Realm
 *  - Algorithm
 *  - Nonce
 *
 * @param as_digest_t *digest The digest context to generate the header value from.
 * @param char *result The buffer to store the generated header value in.
 *
 * Returns the number of bytes in the result string. -1 on failure.
 */
size_t as_digest_server_generate_header(as_digest_t *digest, char *result, size_t max_length);

#endif  /* __AS_HTTP_DIGEST_H__ */
