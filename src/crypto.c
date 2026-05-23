#include "crypto.h"
#include <openssl/sha.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/x509.h>

// Initialize the OpenSSL crypto library.
int crypto_init(void) {
  OpenSSL_add_all_algorithms();
  ERR_load_crypto_strings();
  return 0;
}


// Cleanup the OpenSSL crypto library resources.
void crypto_cleanup(void) {
  EVP_cleanup();
  ERR_free_strings();
}

// Cleanup CryptoContext resources (free the internal EC key).
void crypto_free_context(CryptoContext *ctx) {
    if (ctx && ctx->ec_key) {
        EVP_PKEY_free(ctx->ec_key);
        ctx->ec_key = NULL;
    }
}


// Generate a new ECDH keypair and an optional IV if is_server is true.
int crypto_generate_key(CryptoContext *ctx, int is_server) {
  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
  if (!pctx) return -1;
  if (EVP_PKEY_keygen_init(pctx) <= 0) {
      EVP_PKEY_CTX_free(pctx);
      return -1;
  }
  if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, NID_X9_62_prime256v1) <= 0) {
      EVP_PKEY_CTX_free(pctx);
      return -1;
  }
  
  ctx->ec_key = NULL;
  if (EVP_PKEY_keygen(pctx, &ctx->ec_key) <= 0) {
      EVP_PKEY_CTX_free(pctx);
      return -1;
  }
  EVP_PKEY_CTX_free(pctx);

  if (is_server) {
      if (RAND_bytes(ctx->iv, AES_IV_SIZE) != 1) return -1;
  } else {
      memset(ctx->iv, 0, AES_IV_SIZE);
  }
  
  return 0;
}


// Export the local public key to a buffer.
int crypto_export_public_key(CryptoContext *ctx, unsigned char *pubkey, uint32_t *pubkey_len) {
    if (!ctx->ec_key) return -1;
    unsigned char *p = pubkey;
    int len = i2d_PUBKEY(ctx->ec_key, &p);
    if (len < 0) return -1;
    *pubkey_len = len;
    return 0;
}


// Derive the AES key using the peer's public key.
int crypto_derive_aes_key(CryptoContext *ctx, const unsigned char *peer_pubkey, uint32_t peer_pubkey_len) {
    if (!ctx->ec_key) return -1;
    
    // Import peer public key
    const unsigned char *p = peer_pubkey;
    EVP_PKEY *peer_key = d2i_PUBKEY(NULL, &p, peer_pubkey_len);
    if (!peer_key) return -1;
    
    EVP_PKEY_CTX *dctx = EVP_PKEY_CTX_new(ctx->ec_key, NULL);
    if (!dctx) {
        EVP_PKEY_free(peer_key);
        return -1;
    }
    
    if (EVP_PKEY_derive_init(dctx) <= 0 ||
        EVP_PKEY_derive_set_peer(dctx, peer_key) <= 0) {
        EVP_PKEY_CTX_free(dctx);
        EVP_PKEY_free(peer_key);
        return -1;
    }
    
    size_t secret_len;
    if (EVP_PKEY_derive(dctx, NULL, &secret_len) <= 0) {
        EVP_PKEY_CTX_free(dctx);
        EVP_PKEY_free(peer_key);
        return -1;
    }
    
    unsigned char *secret = (unsigned char*)malloc(secret_len);
    if (EVP_PKEY_derive(dctx, secret, &secret_len) <= 0) {
        free(secret);
        EVP_PKEY_CTX_free(dctx);
        EVP_PKEY_free(peer_key);
        return -1;
    }
    
    EVP_PKEY_CTX_free(dctx);
    EVP_PKEY_free(peer_key);
    
    // Hash shared secret to form AES key
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(secret, secret_len, hash);
    free(secret);
    
    memcpy(ctx->key, hash, AES_KEY_SIZE);
    return 0;
}


// Encrypt data using AES-256-GCM.
int crypto_encrypt(CryptoContext *ctx, const unsigned char *plaintext,
                   int plaintext_len, unsigned char *ciphertext,
                   unsigned char *tag) {
  EVP_CIPHER_CTX *evp_ctx = EVP_CIPHER_CTX_new();
  if (!evp_ctx)
    return -1;

  int len;
  int ciphertext_len;

  if (EVP_EncryptInit_ex(evp_ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
    EVP_CIPHER_CTX_free(evp_ctx);
    return -1;
  }

  if (EVP_EncryptInit_ex(evp_ctx, NULL, NULL, ctx->key, ctx->iv) != 1) {
    EVP_CIPHER_CTX_free(evp_ctx);
    return -1;
  }

  if (EVP_EncryptUpdate(evp_ctx, ciphertext, &len, plaintext, plaintext_len) !=
      1) {
    EVP_CIPHER_CTX_free(evp_ctx);
    return -1;
  }
  ciphertext_len = len;

  if (EVP_EncryptFinal_ex(evp_ctx, ciphertext + len, &len) != 1) {
    EVP_CIPHER_CTX_free(evp_ctx);
    return -1;
  }
  ciphertext_len += len;

  if (EVP_CIPHER_CTX_ctrl(evp_ctx, EVP_CTRL_GCM_GET_TAG, AES_TAG_SIZE, tag) !=
      1) {
    EVP_CIPHER_CTX_free(evp_ctx);
    return -1;
  }

  EVP_CIPHER_CTX_free(evp_ctx);

  // Increment IV for next encryption
  for (int i = AES_IV_SIZE - 1; i >= 0; i--) {
    if (++ctx->iv[i] != 0)
      break;
  }

  return ciphertext_len;
}


// Decrypt data using AES-256-GCM.
int crypto_decrypt(CryptoContext *ctx, const unsigned char *ciphertext,
                   int ciphertext_len, const unsigned char *tag,
                   unsigned char *plaintext) {
  EVP_CIPHER_CTX *evp_ctx = EVP_CIPHER_CTX_new();
  if (!evp_ctx)
    return -1;

  int len;
  int plaintext_len;

  if (EVP_DecryptInit_ex(evp_ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
    EVP_CIPHER_CTX_free(evp_ctx);
    return -1;
  }

  if (EVP_DecryptInit_ex(evp_ctx, NULL, NULL, ctx->key, ctx->iv) != 1) {
    EVP_CIPHER_CTX_free(evp_ctx);
    return -1;
  }

  if (EVP_DecryptUpdate(evp_ctx, plaintext, &len, ciphertext, ciphertext_len) !=
      1) {
    EVP_CIPHER_CTX_free(evp_ctx);
    return -1;
  }
  plaintext_len = len;

  if (EVP_CIPHER_CTX_ctrl(evp_ctx, EVP_CTRL_GCM_SET_TAG, AES_TAG_SIZE,
                          (void *)tag) != 1) {
    EVP_CIPHER_CTX_free(evp_ctx);
    return -1;
  }

  if (EVP_DecryptFinal_ex(evp_ctx, plaintext + len, &len) != 1) {
    EVP_CIPHER_CTX_free(evp_ctx);
    return -1;
  }
  plaintext_len += len;

  EVP_CIPHER_CTX_free(evp_ctx);

  // Increment IV for next decryption
  for (int i = AES_IV_SIZE - 1; i >= 0; i--) {
    if (++ctx->iv[i] != 0)
      break;
  }

  return plaintext_len;
}
