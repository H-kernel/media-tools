#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "as_http_digest.h"


/*
 * The basic MD5 functions.
 *
 * F and G are optimized compared to their RFC 1321 definitions for
 * architectures that lack an AND-NOT instruction, just like in Colin Plumb's
 * implementation.
 */
#define F(x, y, z)      ((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z)      ((y) ^ ((z) & ((x) ^ (y))))
#define H(x, y, z)      (((x) ^ (y)) ^ (z))
#define H2(x, y, z)     ((x) ^ ((y) ^ (z)))
#define I(x, y, z)      ((y) ^ ((x) | ~(z)))

/*
 * The MD5 transformation for all four rounds.
 */
#define STEP(f, a, b, c, d, x, t, s) \
  (a) += f((b), (c), (d)) + (x) + (t); \
  (a) = (((a) << (s)) | (((a) & 0xffffffff) >> (32 - (s)))); \
  (a) += (b);

/*
 * SET reads 4 input bytes in little-endian byte order and stores them
 * in a properly aligned word in host byte order.
 *
 * The check for little-endian architectures that tolerate unaligned
 * memory accesses is just an optimization.  Nothing will break if it
 * doesn't work.
 */
#if defined(__i386__) || defined(__x86_64__) || defined(__vax__)
#define SET(n) \
  (*(MD5_u32plus *)&ptr[(n) * 4])
#define GET(n) \
  SET(n)
#else
#define SET(n) \
  (ctx->block[(n)] = \
  (MD5_u32plus)ptr[(n) * 4] | \
  ((MD5_u32plus)ptr[(n) * 4 + 1] << 8) | \
  ((MD5_u32plus)ptr[(n) * 4 + 2] << 16) | \
  ((MD5_u32plus)ptr[(n) * 4 + 3] << 24))
#define GET(n) \
  (ctx->block[(n)])
#endif

/*
 * This processes one or more 64-byte data blocks, but does NOT update
 * the bit counters.  There are no alignment requirements.
 */
static const void *
as_md5_body(as_md5_ctx *ctx, const void *data, unsigned long size)
{
  const unsigned char *ptr;
  MD5_u32plus a, b, c, d;
  MD5_u32plus saved_a, saved_b, saved_c, saved_d;

  ptr = (const unsigned char *)data;

  a = ctx->a;
  b = ctx->b;
  c = ctx->c;
  d = ctx->d;

  do {
    saved_a = a;
    saved_b = b;
    saved_c = c;
    saved_d = d;

/* Round 1 */
    STEP(F, a, b, c, d, SET(0), 0xd76aa478, 7)
    STEP(F, d, a, b, c, SET(1), 0xe8c7b756, 12)
    STEP(F, c, d, a, b, SET(2), 0x242070db, 17)
    STEP(F, b, c, d, a, SET(3), 0xc1bdceee, 22)
    STEP(F, a, b, c, d, SET(4), 0xf57c0faf, 7)
    STEP(F, d, a, b, c, SET(5), 0x4787c62a, 12)
    STEP(F, c, d, a, b, SET(6), 0xa8304613, 17)
    STEP(F, b, c, d, a, SET(7), 0xfd469501, 22)
    STEP(F, a, b, c, d, SET(8), 0x698098d8, 7)
    STEP(F, d, a, b, c, SET(9), 0x8b44f7af, 12)
    STEP(F, c, d, a, b, SET(10), 0xffff5bb1, 17)
    STEP(F, b, c, d, a, SET(11), 0x895cd7be, 22)
    STEP(F, a, b, c, d, SET(12), 0x6b901122, 7)
    STEP(F, d, a, b, c, SET(13), 0xfd987193, 12)
    STEP(F, c, d, a, b, SET(14), 0xa679438e, 17)
    STEP(F, b, c, d, a, SET(15), 0x49b40821, 22)

/* Round 2 */
    STEP(G, a, b, c, d, GET(1), 0xf61e2562, 5)
    STEP(G, d, a, b, c, GET(6), 0xc040b340, 9)
    STEP(G, c, d, a, b, GET(11), 0x265e5a51, 14)
    STEP(G, b, c, d, a, GET(0), 0xe9b6c7aa, 20)
    STEP(G, a, b, c, d, GET(5), 0xd62f105d, 5)
    STEP(G, d, a, b, c, GET(10), 0x02441453, 9)
    STEP(G, c, d, a, b, GET(15), 0xd8a1e681, 14)
    STEP(G, b, c, d, a, GET(4), 0xe7d3fbc8, 20)
    STEP(G, a, b, c, d, GET(9), 0x21e1cde6, 5)
    STEP(G, d, a, b, c, GET(14), 0xc33707d6, 9)
    STEP(G, c, d, a, b, GET(3), 0xf4d50d87, 14)
    STEP(G, b, c, d, a, GET(8), 0x455a14ed, 20)
    STEP(G, a, b, c, d, GET(13), 0xa9e3e905, 5)
    STEP(G, d, a, b, c, GET(2), 0xfcefa3f8, 9)
    STEP(G, c, d, a, b, GET(7), 0x676f02d9, 14)
    STEP(G, b, c, d, a, GET(12), 0x8d2a4c8a, 20)

/* Round 3 */
    STEP(H, a, b, c, d, GET(5), 0xfffa3942, 4)
    STEP(H2, d, a, b, c, GET(8), 0x8771f681, 11)
    STEP(H, c, d, a, b, GET(11), 0x6d9d6122, 16)
    STEP(H2, b, c, d, a, GET(14), 0xfde5380c, 23)
    STEP(H, a, b, c, d, GET(1), 0xa4beea44, 4)
    STEP(H2, d, a, b, c, GET(4), 0x4bdecfa9, 11)
    STEP(H, c, d, a, b, GET(7), 0xf6bb4b60, 16)
    STEP(H2, b, c, d, a, GET(10), 0xbebfbc70, 23)
    STEP(H, a, b, c, d, GET(13), 0x289b7ec6, 4)
    STEP(H2, d, a, b, c, GET(0), 0xeaa127fa, 11)
    STEP(H, c, d, a, b, GET(3), 0xd4ef3085, 16)
    STEP(H2, b, c, d, a, GET(6), 0x04881d05, 23)
    STEP(H, a, b, c, d, GET(9), 0xd9d4d039, 4)
    STEP(H2, d, a, b, c, GET(12), 0xe6db99e5, 11)
    STEP(H, c, d, a, b, GET(15), 0x1fa27cf8, 16)
    STEP(H2, b, c, d, a, GET(2), 0xc4ac5665, 23)

/* Round 4 */
    STEP(I, a, b, c, d, GET(0), 0xf4292244, 6)
    STEP(I, d, a, b, c, GET(7), 0x432aff97, 10)
    STEP(I, c, d, a, b, GET(14), 0xab9423a7, 15)
    STEP(I, b, c, d, a, GET(5), 0xfc93a039, 21)
    STEP(I, a, b, c, d, GET(12), 0x655b59c3, 6)
    STEP(I, d, a, b, c, GET(3), 0x8f0ccc92, 10)
    STEP(I, c, d, a, b, GET(10), 0xffeff47d, 15)
    STEP(I, b, c, d, a, GET(1), 0x85845dd1, 21)
    STEP(I, a, b, c, d, GET(8), 0x6fa87e4f, 6)
    STEP(I, d, a, b, c, GET(15), 0xfe2ce6e0, 10)
    STEP(I, c, d, a, b, GET(6), 0xa3014314, 15)
    STEP(I, b, c, d, a, GET(13), 0x4e0811a1, 21)
    STEP(I, a, b, c, d, GET(4), 0xf7537e82, 6)
    STEP(I, d, a, b, c, GET(11), 0xbd3af235, 10)
    STEP(I, c, d, a, b, GET(2), 0x2ad7d2bb, 15)
    STEP(I, b, c, d, a, GET(9), 0xeb86d391, 21)

    a += saved_a;
    b += saved_b;
    c += saved_c;
    d += saved_d;

    ptr += 64;
  } while (size -= 64);

  ctx->a = a;
  ctx->b = b;
  ctx->c = c;
  ctx->d = d;

  return ptr;
}
static void
as_md5_init(as_md5_ctx *ctx)
{
  ctx->a = 0x67452301;
  ctx->b = 0xefcdab89;
  ctx->c = 0x98badcfe;
  ctx->d = 0x10325476;

  ctx->lo = 0;
  ctx->hi = 0;
}

static void
as_md5_update(as_md5_ctx *ctx, const void *data, unsigned long size)
{
  MD5_u32plus saved_lo;
  unsigned long used, available;

  saved_lo = ctx->lo;
  if ((ctx->lo = (saved_lo + size) & 0x1fffffff) < saved_lo)
    ctx->hi++;
  ctx->hi += size >> 29;

  used = saved_lo & 0x3f;

  if (used) {
    available = 64 - used;

    if (size < available) {
      memcpy(&ctx->buffer[used], data, size);
      return;
    }

    memcpy(&ctx->buffer[used], data, available);
    data = (const unsigned char *)data + available;
    size -= available;
    as_md5_body(ctx, ctx->buffer, 64);
  }

  if (size >= 64) {
    data = as_md5_body(ctx, data, size & ~(unsigned long)0x3f);
    size &= 0x3f;
  }

  memcpy(ctx->buffer, data, size);
}
static void
as_md5_final(unsigned char *result, as_md5_ctx *ctx)
{
  unsigned long used, available;

  used = ctx->lo & 0x3f;

  ctx->buffer[used++] = 0x80;

  available = 64 - used;

  if (available < 8) {
    memset(&ctx->buffer[used], 0, available);
    as_md5_body(ctx, ctx->buffer, 64);
    used = 0;
    available = 64;
  }

  memset(&ctx->buffer[used], 0, available - 8);

  ctx->lo <<= 3;
  ctx->buffer[56] = ctx->lo;
  ctx->buffer[57] = ctx->lo >> 8;
  ctx->buffer[58] = ctx->lo >> 16;
  ctx->buffer[59] = ctx->lo >> 24;
  ctx->buffer[60] = ctx->hi;
  ctx->buffer[61] = ctx->hi >> 8;
  ctx->buffer[62] = ctx->hi >> 16;
  ctx->buffer[63] = ctx->hi >> 24;

  as_md5_body(ctx, ctx->buffer, 64);

  result[0] = ctx->a;
  result[1] = ctx->a >> 8;
  result[2] = ctx->a >> 16;
  result[3] = ctx->a >> 24;
  result[4] = ctx->b;
  result[5] = ctx->b >> 8;
  result[6] = ctx->b >> 16;
  result[7] = ctx->b >> 24;
  result[8] = ctx->c;
  result[9] = ctx->c >> 8;
  result[10] = ctx->c >> 16;
  result[11] = ctx->c >> 24;
  result[12] = ctx->d;
  result[13] = ctx->d >> 8;
  result[14] = ctx->d >> 16;
  result[15] = ctx->d >> 24;

  memset(ctx, 0, sizeof(*ctx));
}

/**
 * Generates an MD5 hash from a string.
 *
 * string needs to be null terminated.
 * result is the buffer where to store the md5 hash. The length will always be
 * 32 characters long.
 */
static void
as_get_md5(const char *string, char *result)
{
    int i = 0;
    unsigned char digest[16];

    as_md5_ctx context;
    as_md5_init(&context);
    as_md5_update(&context, string, strlen(string));
    as_md5_final(digest, &context);

    for (i = 0; i < 16; ++i) {
        sprintf(&result[i * 2], "%02x", (unsigned int) digest[i]);
    }
}

/**
 * Extracts the value part from a attribute-value pair.
 *
 * parameter is the string to parse the value from, ex: "key=value".
 *           The string needs to be null-terminated. Can be both
 *           key=value and key="value".
 *
 * Returns a pointer to the start of the value on success, otherwise NULL.
 */
static char *
as_dgst_get_val(char *parameter)
{
    char *cursor, *q;

    /* Find start of value */
    if (NULL == (cursor = strchr(parameter, '='))) {
        return (char *) NULL;
    }

    if (*(++cursor) != '"') {
        return cursor;
    }

    cursor++;
    if (NULL == (q = strchr(cursor, '"'))) {
        return (char *) NULL;
    }
    *q = '\0';

    return cursor;
}

/**
 * Checks if a string pointer is NULL or if the length is more than 255 chars.
 *
 * string is the string to check.
 *
 * Returns 0 if not NULL and length is below 256 characters, otherwise -1.
 */
int
as_check_string(const char *string)
{
    if (NULL == string || 255 < strlen(string)) {
        return -1;
    }

    return 0;
}

/**
 * Removes the authentication scheme identification token from the
 * WWW-Authenticate header field value.
 *
 * header_value is the WWW-Authenticate header field value.
 *
 * Returns a pointer to a new string containing only
 * the authentication parameters. Must be free'd manually.
 */
static char *
as_crop_sentence(const char *header_value)
{
     /* Skip Digest word, and duplicate string */
    return strdup(header_value + 7);
}

/**
 * Splits a string by comma.
 *
 * sentence is the string to split, null terminated.
 * values is a char pointer array that will be filled with pointers to the
 * splitted values in the sentence string.
 * max_values is the length of the **values array. The function will not parse
 * more than max_values entries.
 *
 * Returns the number of values found in string.
 */
static inline int
as_split_string_by_comma(char *sentence, char **values, int max_values)
{
    int i = 0;

    while (i < max_values && '\0' != *sentence) {
        /* Rewind to after spaces */
        while (' ' == *sentence || ',' == *sentence) {
            sentence++;
        }

        /* Check for end of string */
        if ('\0' == *sentence) {
            break;
        }

        values[i++] = sentence;

        /* Find comma */
        if (NULL == (sentence = strchr(sentence, ','))) {
            /* End of string */
            break;
        }

        *(sentence++) = '\0';
    }

    return i;
}

/**
 * Tokenizes a string containing comma-seperated attribute-key parameters.
 *
 * sentence is the string to split, null terminated.
 * values is a char pointer array that will be filled with pointers to the
 * splitted values in the sentence string.
 * max_values is the length of the **values array. The function will not parse
 * more than max_values entries.
 *
 * Returns the number of values found in sentence.
 */
static inline unsigned int
as_tokenize_sentence(char *sentence, char **values, unsigned int max_values)
{
    unsigned int i = 0;
    char *cursor = sentence;

    while (i < max_values && *cursor != '\0') {
        /* Rewind to after spaces */
        while (' ' == *cursor || ',' == *cursor) {
            cursor++;
        }

        /* Check for end of string */
        if ('\0' == *cursor) {
            break;
        }

        values[i++] = cursor;

        /* Find equal sign (=) */
        if (NULL == (cursor = strchr(cursor, '='))) {
            /* End of string */
            break;
        }

        /* Check if a quotation mark follows the = */
        if ('\"' == *(++cursor)) {
            /* Find next quotation mark */
            if (NULL == (cursor = strchr(++cursor, '\"'))) {
                /* End of string */
                break;
            }
            /* Comma should be after */
            cursor++;
        } else {
            /* Find comma */
            if (NULL == (cursor = strchr(cursor, ','))) {
                /* End of string */
                break;
            }
        }

        *(cursor++) = '\0';
    }

    return i;
}

/**
 * Parses a WWW-Authenticate header value to a struct.
 *
 * dig is a pointer to the digest struct to fill the parsed values with.
 * digest_string should be the value from the WWW-Authentication header,
 * null terminated.
 *
 * Returns the hash as a null terminated string. Should be free'd manually.
 */
static int
as_parse_digest(as_digest_s *dig, const char *digest_string)
{
    int n, i = 0;
    char *val, *parameters;
    char *values[12];

    parameters = as_crop_sentence(digest_string);
    n = as_tokenize_sentence(parameters, values, ARRAY_LENGTH(values));

    while (i < n) {
        if (NULL == (val = values[i++])) {
            continue;
        }

        if (0 == strncmp("nonce=", val, strlen("nonce="))) {
            dig->nonce = as_dgst_get_val(val);
        } else if (0 == strncmp("realm=", val, strlen("realm="))) {
            dig->realm = as_dgst_get_val(val);
        } else if (0 == strncmp("qop=", val, strlen("qop="))) {
            char *qop_options = as_dgst_get_val(val);
            char *qop_values[2];
            int n_qops = as_split_string_by_comma(qop_options, qop_values, ARRAY_LENGTH(qop_values));
            while (n_qops-- > 0) {
                if (0 == strncmp(qop_values[n_qops], "auth", strlen("auth"))) {
                    dig->qop |= DIGEST_QOP_AUTH;
                    continue;
                }
                if (0 == strncmp(qop_values[n_qops], "auth-int", strlen("auth-int"))) {
                    dig->qop |= DIGEST_QOP_AUTH_INT;
                }
            }
        } else if (0 == strncmp("opaque=", val, strlen("opaque="))) {
            dig->opaque = as_dgst_get_val(val);
        } else if (0 == strncmp("algorithm=", val, strlen("algorithm="))) {
            char *algorithm = as_dgst_get_val(val);
            if (0 == strncmp(algorithm, "MD5", strlen("MD5"))) {
                dig->algorithm = DIGEST_ALGORITHM_MD5;
            }
        }
    }

    return i;
}

/**
 * Validates the string values in a digest struct.
 *
 * The function goes through the string values and check if they are valid.
 * They are considered valid if they aren't NULL and the character length is
 * below 256.
 *
 * dig is a pointer to the struct where to check the string values.
 *
 * Returns 0 if valid, otherwise -1.
 */
static int
as_parse_validate_attributes(as_digest_s *dig)
{
    if (-1 == as_check_string(dig->username)) {
        return -1;
    }
    if (-1 == as_check_string(dig->password)) {
        return -1;
    }
    if (-1 == as_check_string(dig->uri)) {
        return -1;
    }
    if (-1 == as_check_string(dig->realm)) {
        return -1;
    }
    if (NULL != dig->opaque && 255 < strlen(dig->opaque)) {
        return -1;
    }

    /* nonce */
    if (DIGEST_QOP_NOT_SET != dig->qop && -1 == as_check_string(dig->nonce)) {
        return -1;
    }

    return 0;
}




/**
 * Hashes method and URI (ex: GET:/api/users).
 *
 * result is the buffer where to store the generated md5 hash.
 * Both method and uri should be null terminated strings.
 */
static void
as_hash_generate_a2(char *result, const char *method, const char *uri)
{
    char raw[512];
    sprintf(raw, "%s:%s", method, uri);
    as_get_md5(raw, result);
}

/**
 * Hashes username, realm and password (ex: jack:GET:password).
 *
 * result is the buffer where to store the generated md5 hash.
 * All other arguments should be null terminated strings.
 */
static void
as_hash_generate_a1(char *result, const char *username, const char *realm, const char *password)
{
    char raw[768];
    sprintf(raw, "%s:%s:%s", username, realm, password);
    as_get_md5(raw, result);
}

/**
 * Generates the response parameter according to rfc.
 *
 * Hashes a1, nonce, nc, cnonce, qop and a2. This should be used when the
 * qop parameter is supplied.
 *
 * result is the buffer where to store the generated md5 hash.
 * All other arguments should be null terminated strings.
 */
static void
as_hash_generate_response_auth(char *result, const char *ha1, const char *nonce, unsigned int nc, unsigned int cnonce, const char *qop, const char *ha2)
{
    char raw[512];
    sprintf(raw, "%s:%s:%08x:%08x:%s:%s", ha1, nonce, nc, cnonce, qop, ha2);
    as_get_md5(raw, result);
}

/**
 * Generates the response parameter according to rfc.
 *
 * Hashes a1, nonce and a2. This is the version used when the qop parameter is
 * not supplied.
 *
 * result is the buffer where to store the generated md5 hash.
 * All other arguments should be null terminated strings.
 */
void
as_hash_generate_response(char *result, const char *ha1, const char *nonce, const char *ha2)
{
    char raw[512];
    sprintf(raw, "%s:%s:%s", ha1, nonce, ha2);
    as_get_md5(raw, result);
}

int
as_digest_init(as_digest_t *digest,unsigned int quotes)
{
    as_digest_s *dig = (as_digest_s *) digest;

    /* Clear */
    memset(dig, 0, sizeof (as_digest_s));

    /* Set default values */
    dig->algorithm = DIGEST_ALGORITHM_MD5;
    dig->quotes    = quotes;

    return 0;
}

int
as_digest_is_digest(const char *header_value)
{
    if (NULL == header_value) {
        return -1;
    }

    if (0 != strncmp(header_value, "Digest", 6)) {
        return -1;
    }

    return 0;
}

void *
as_digest_get_attr(as_digest_t *digest, as_digest_attr_t attr)
{
    as_digest_s *dig = (as_digest_s *) digest;

    switch (attr) {
    case D_ATTR_USERNAME:
        return dig->username;
    case D_ATTR_PASSWORD:
        return dig->password;
    case D_ATTR_REALM:
        return dig->realm;
    case D_ATTR_NONCE:
        return dig->nonce;
    case D_ATTR_CNONCE:
        return &(dig->cnonce);
    case D_ATTR_OPAQUE:
        return dig->opaque;
    case D_ATTR_URI:
        return dig->uri;
    case D_ATTR_METHOD:
        return &(dig->method);
    case D_ATTR_ALGORITHM:
        return &(dig->algorithm);
    case D_ATTR_QOP:
        return &(dig->qop);
    case D_ATTR_NONCE_COUNT:
        return &(dig->nc);
    default:
        return NULL;
    }
}

int
as_digest_set_attr(as_digest_t *digest, as_digest_attr_t attr, const as_digest_attr_value_t value)
{
    as_digest_s *dig = (as_digest_s *) digest;

    switch (attr) {
    case D_ATTR_USERNAME:
        dig->username = value.string;
        break;
    case D_ATTR_PASSWORD:
        dig->password = value.string;
        break;
    case D_ATTR_REALM:
        dig->realm = value.string;
        break;
    case D_ATTR_NONCE:
        dig->nonce = value.string;
        break;
    case D_ATTR_CNONCE:
        dig->cnonce = value.number;
        break;
    case D_ATTR_OPAQUE:
        dig->opaque = value.string;
        break;
    case D_ATTR_URI:
        dig->uri = value.string;
        break;
    case D_ATTR_METHOD:
        dig->method = value.number;
        break;
    case D_ATTR_ALGORITHM:
        dig->algorithm = value.number;
        break;
    case D_ATTR_QOP:
        dig->qop = value.number;
        break;
    case D_ATTR_NONCE_COUNT:
        dig->nc = value.number;
        break;
    default:
        return -1;
    }

    return 0;
}

int
as_digest_client_parse(as_digest_t *digest, const char *digest_string)
{
    as_digest_s *dig = (as_digest_s *) digest;

    /* Set default values */
    dig->nc = 1;
    dig->cnonce = time(NULL);

    return as_parse_digest(dig, digest_string);
}

/**
 * Generates the Authorization header string.
 *
 * Attributes that must be set manually before calling this function:
 *
 *  - Username
 *  - Password
 *  - URI
 *  - Method
 *
 * If not set, NULL will be returned.
 *
 * Returns the number of bytes in the result string.
 */
size_t
as_digest_client_generate_header(as_digest_t *digest, char *result, size_t max_length)
{
    as_digest_s *dig = (as_digest_s *) digest;
    char hash_a1[52], hash_a2[52], hash_res[52];
    char *qop_value, *algorithm_value, *method_value;
    size_t result_size; /* The size of the result string */
    int sz;

    /* Check length of char attributes to prevent buffer overflow */
    if (-1 == as_parse_validate_attributes(dig)) {
        return -1;
    }

    /* Quality of Protection - qop */
    if (DIGEST_QOP_AUTH == (DIGEST_QOP_AUTH & dig->qop)) {
        qop_value = "auth";
    } else if (DIGEST_QOP_AUTH_INT == (DIGEST_QOP_AUTH_INT & dig->qop)) {
        /* auth-int, which is not supported */
        return -1;
    }

    /* Set algorithm */
    algorithm_value = NULL;
    if (DIGEST_ALGORITHM_MD5 == dig->algorithm) {
        algorithm_value = "MD5";
    }

    /* Set method */
    switch (dig->method) {
    case DIGEST_METHOD_OPTIONS:
        method_value = "OPTIONS";
        break;
    case DIGEST_METHOD_GET:
        method_value = "GET";
        break;
    case DIGEST_METHOD_HEAD:
        method_value = "HEAD";
        break;
    case DIGEST_METHOD_POST:
        method_value = "POST";
        break;
    case DIGEST_METHOD_PUT:
        method_value = "PUT";
        break;
    case DIGEST_METHOD_DELETE:
        method_value = "DELETE";
        break;
    case DIGEST_METHOD_TRACE:
        method_value = "TRACE";
        break;
    default:
        return -1;
    }

    /* Generate the hashes */
    as_hash_generate_a1(hash_a1, dig->username, dig->realm, dig->password);
    as_hash_generate_a2(hash_a2, method_value, dig->uri);

    if (DIGEST_QOP_NOT_SET != dig->qop) {
        as_hash_generate_response_auth(hash_res, hash_a1, dig->nonce, dig->nc, dig->cnonce, qop_value, hash_a2);
    } else {
        as_hash_generate_response(hash_res, hash_a1, dig->nonce, hash_a2);
    }

    /* Generate the minimum digest header string */
    if(digest->quotes) {
        result_size = snprintf(result, max_length, "Digest username=\"%s\", realm=\"%s\", uri=\"%s\", response=\"%s\"",\
            dig->username,\
            dig->realm,\
            dig->uri,\
            hash_res);
    }
    else {
        result_size = snprintf(result, max_length, "Digest username=%s, realm=%s, uri=%s, response=%s",\
            dig->username,\
            dig->realm,\
            dig->uri,\
            hash_res);
    }
    if (result_size == -1 || result_size == max_length) {
        return -1;
    }

    /* Add opaque */
    if (NULL != dig->opaque) {
        sz = snprintf(result + result_size, max_length - result_size, ", opaque=\"%s\"", dig->opaque);
        result_size += sz;
        if (sz == -1 || result_size >= max_length) {
            return -1;
        }
    }

    /* Add algorithm */
    if (DIGEST_ALGORITHM_NOT_SET != dig->algorithm) {
        sz = snprintf(result + result_size, max_length - result_size, ", algorithm=\"%s\"",\
                algorithm_value);
        if (sz == -1 || result_size >= max_length) {
            return -1;
        }
    }

    /* If qop is supplied, add nonce, cnonce, nc and qop */
    if (DIGEST_QOP_NOT_SET != dig->qop) {
        sz = snprintf(result + result_size, max_length - result_size, ", qop=%s, nonce=\"%s\", cnonce=\"%08x\", nc=%08x",\
            qop_value,\
            dig->nonce,\
            dig->cnonce,\
            dig->nc);
        if (sz == -1 || result_size >= max_length) {
            return -1;
        }
    }

    return result_size;
}

int
as_digest_server_parse(as_digest_t *digest, const char *digest_string)
{
    as_digest_s *dig = (as_digest_s *) digest;

    return as_parse_digest(dig, digest_string);
}

int
as_digest_server_generate_nonce(as_digest_t *digest)
{
    //as_digest_s *dig = (as_digest_s *) digest;

    /* Use srand and base64 or md5.
       Do the same with cnonce and opaque.
       How should the strings be allocated and free'd? */

    return 0;
}

/**
 * Generates the WWW-Authenticate header string.
 *
 * Attributes that must be set manually before calling this function:
 *
 *  - Realm
 *  - Algorithm
 *  - Nonce
 *
 * If not set, NULL will be returned.
 *
 * Returns the number of bytes in the result string.
 */
size_t
as_digest_server_generate_header(as_digest_t *digest, char *result, size_t max_length)
{
    as_digest_s *dig = (as_digest_s *) digest;
    char *qop_value, *algorithm_value;
    size_t result_size; /* The size of the result string */
    int sz;

    /* Check length of char attributes to prevent buffer overflow */
    if (-1 == as_parse_validate_attributes(dig)) {
        return -1;
    }

    /* Quality of Protection - qop */
    if (DIGEST_QOP_AUTH == (DIGEST_QOP_AUTH & dig->qop)) {
        qop_value = "auth";
    } else if (DIGEST_QOP_AUTH_INT == (DIGEST_QOP_AUTH_INT & dig->qop)) {
        /* auth-int, which is not supported */
        return -1;
    }

    /* Set algorithm */
    algorithm_value = NULL;
    if (DIGEST_ALGORITHM_MD5 == dig->algorithm) {
        algorithm_value = "MD5";
    }

    /* Generate the minimum digest header string */
    result_size = snprintf(result, max_length, "Digest realm=\"%s\"", dig->realm);
    if (result_size == -1 || result_size == max_length) {
        return -1;
    }

    /* Add opaque */
    if (NULL != dig->opaque) {
        sz = snprintf(result + result_size, max_length - result_size, ", opaque=\"%s\"", dig->opaque);
        result_size += sz;
        if (sz == -1 || result_size >= max_length) {
            return -1;
        }
    }

    /* Add algorithm */
    if (DIGEST_ALGORITHM_NOT_SET != dig->algorithm) {
        sz = snprintf(result + result_size, max_length - result_size, ", algorithm=\"%s\"",\
                algorithm_value);
        if (sz == -1 || result_size >= max_length) {
            return -1;
        }
    }

    /* If qop is supplied, add nonce, cnonce, nc and qop */
    if (DIGEST_QOP_NOT_SET != dig->qop) {
        sz = snprintf(result + result_size, max_length - result_size, ", qop=%s, nonce=\"%s\", cnonce=\"%08x\", nc=%08x",\
            qop_value,\
            dig->nonce,\
            dig->cnonce,\
            dig->nc);
        if (sz == -1 || result_size >= max_length) {
            return -1;
        }
    }

    return result_size;
}
