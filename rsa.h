#ifdef _WIN32
#include "mpir/gmp.h"
#else
#include <gmp.h>
#endif

typedef struct {
    mpz_t n; /* Modulus */
    mpz_t e; /* Public Exponent */
} public_key;

typedef struct {
    mpz_t n; /* Modulus */
    mpz_t e; /* Public Exponent */
    mpz_t d; /* Private Exponent */
    mpz_t p; /* Starting prime p */
    mpz_t q; /* Starting prime q */
} private_key;

extern int generate_keys(private_key* ku, public_key* kp);
extern void block_encrypt(mpz_t C, mpz_t M, public_key kp);
extern int rsa_encrypt(char* cipher, const char *message, int length, public_key kp);
extern void block_decrypt(mpz_t M, mpz_t C, private_key ku);
extern int rsa_decrypt(char* message, const char* cipher, int length, private_key ku);
/* vim: set sw=4 ts=8 expandtab: */
