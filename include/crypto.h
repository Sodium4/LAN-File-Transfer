#ifndef CRYPTO_H
#define CRYPTO_H

#include "common.h"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>


// AES-256-GCM parameters
#define AES_KEY_SIZE 32 // 256 bits
#define AES_IV_SIZE 12  // 96 bits for GCM
#define AES_TAG_SIZE 16 // 128 bits

// Crypto context
typedef struct {
  unsigned char key[AES_KEY_SIZE];
  unsigned char iv[AES_IV_SIZE];
  EVP_PKEY *ec_key;
} CryptoContext;


/**
 * Initialize the OpenSSL crypto library.
 * 
 * @return 0 on success, non-zero on failure.
 */
int crypto_init(void);


// Cleanup the OpenSSL crypto library resources.
void crypto_cleanup(void);


// Generate a new ECDH keypair and an optional IV if is_server is true.
int crypto_generate_key(CryptoContext *ctx, int is_server);


// Export the local public key to a buffer.
int crypto_export_public_key(CryptoContext *ctx, unsigned char *pubkey, uint32_t *pubkey_len);


//Derive the AES key using the peer's public key.
int crypto_derive_aes_key(CryptoContext *ctx, const unsigned char *peer_pubkey, uint32_t peer_pubkey_len);


// Cleanup CryptoContext resources (free the internal EC key).
void crypto_free_context(CryptoContext *ctx);

/**
 * Encrypt data using AES-256-GCM.
 * 
 * @param ctx Crypto context containing key and IV.
 * @param plaintext Buffer containing data to encrypt.
 * @param plaintext_len Length of data to encrypt.
 * @param ciphertext Buffer to write the encrypted data.
 * @param tag Buffer to write the GCM authentication tag.
 * @return The length of the ciphertext on success, < 0 on failure.
 */
int crypto_encrypt(CryptoContext *ctx, const unsigned char *plaintext,
                   int plaintext_len, unsigned char *ciphertext,
                   unsigned char *tag);

/**
 * Decrypt data using AES-256-GCM.
 * 
 * @param ctx Crypto context containing key and IV.
 * @param ciphertext Buffer containing encrypted data.
 * @param ciphertext_len Length of encrypted data.
 * @param tag GCM authentication tag for verification.
 * @param plaintext Buffer to write decrypted data.
 * @return The length of the plaintext on success, < 0 on failure.
 */
int crypto_decrypt(CryptoContext *ctx, const unsigned char *ciphertext,
                   int ciphertext_len, const unsigned char *tag,
                   unsigned char *plaintext);

#endif
