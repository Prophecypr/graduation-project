#include "mbedtls/aes.h"

#define AES_KEY_SIZE 16  // 128-bit key
static const unsigned char aes_key[AES_KEY_SIZE] = "my_secret_key_16";  // 16字节密钥（需提前定义）
static unsigned char aes_iv[AES_KEY_SIZE] = "my_init_vector_16";  // 16字节IV

static void aes_encrypt(uint8_t *input, uint8_t *output, size_t length) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, aes_key, 128);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, length, aes_iv, input, output);
    mbedtls_aes_free(&aes);
}

static void aes_decrypt(uint8_t *input, uint8_t *output, size_t length) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, aes_key, 128);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, length, aes_iv, input, output);
    mbedtls_aes_free(&aes);
}
