#include <openssl/evp.h>
#include <openssl/crypto.h>

#include <limits.h>
#include <unistd.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdlib.h>

#define N_CRYPTO_THREADS 20
#define DEBUG 0

#define ENC_MODE 0
#define DEC_MODE 1

#define MUTEX_TYPE		pthread_mutex_t
#define MUTEX_SETUP(x)		pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x)	pthread_mutex_destroy(&x) 
#define MUTEX_LOCK(x)		pthread_mutex_lock(&x)
#define MUTEX_UNLOCK(x)		pthread_mutex_unlock(&x)
#define THREAD_ID		pthread_self()

typedef struct e_thread_args{
    char *in;
    char *out;
    int len;
    int n_threads;
    EVP_CIPHER_CTX* d;
    EVP_CIPHER_CTX* e;
} e_thread_args;


#define AES_BLOCK_SIZE 8

int THREAD_setup(void);

int THREAD_cleanup(void);

unsigned char *aes_encrypt(EVP_CIPHER_CTX *e, unsigned char *plaintext, unsigned char*out, int len);

unsigned char *aes_decrypt(EVP_CIPHER_CTX *e, unsigned char* plaintext, unsigned char *ciphertext, int len);

void *encrypt_threaded(void* _args);

void *decrypt_threaded(void* _args);

int update(int mode, e_thread_args args[N_CRYPTO_THREADS], char* in, char*out, int len);

int aes_init(unsigned char *key_data, int key_data_len, 
	     unsigned char *salt, EVP_CIPHER_CTX *e_ctx, 
	     EVP_CIPHER_CTX *d_ctx);
