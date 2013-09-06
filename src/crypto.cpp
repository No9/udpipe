#include <openssl/evp.h>
#include <openssl/crypto.h>



#include <time.h>

#include <limits.h>
#include <unistd.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdlib.h>
#define DEBUG 1

#include "crypto.h"

#define pris(x)            if (DEBUG)fprintf(stderr,"[crypto] %s\n",x)   

#define MUTEX_TYPE	   pthread_mutex_t
#define MUTEX_SETUP(x)	   pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x)   pthread_mutex_destroy(&x) 
#define MUTEX_LOCK(x)	   pthread_mutex_lock(&x)
#define MUTEX_UNLOCK(x)	   pthread_mutex_unlock(&x)
#define THREAD_ID	   pthread_self()

pthread_mutex_t c_lock;

int current_cipher = 0;

#define AES_BLOCK_SIZE 8


static MUTEX_TYPE *mutex_buf = NULL;
static void locking_function(int mode, int n, const char*file, int line);

void pric(uchar* s, int len)
{
    int i;
    fprintf(stderr, "data: ");
    for (i = 0; i < len/4; i ++){
	fprintf(stderr, "%x ",  s[i]);
    }
    fprintf(stderr, "\n");
}

void prii(int i)
{
    if (DEBUG)
	fprintf(stderr, "             -> %d\n", i);
}

const int max_block_size = 64*1024;

// Function for OpenSSL to lock mutex
static void locking_function(int mode, int n, const char*file, int line)
{
    
    pris("LOCKING FUNCTION CALLED");
    if (mode & CRYPTO_LOCK)
	MUTEX_LOCK(mutex_buf[n]);
    else
	MUTEX_UNLOCK(mutex_buf[n]);
}

// Returns the thread ID
static void threadid_func(CRYPTO_THREADID * id)
{
    fprintf(stderr, "[debug] %s\n", "Passing thread ID");
    CRYPTO_THREADID_set_numeric(id, THREAD_ID);
}

// Setups up the mutual exclusion for OpenSSL
int THREAD_setup(void)
{
    pris("Setting up threads");
    mutex_buf = (MUTEX_TYPE*)malloc(CRYPTO_num_locks()*sizeof(MUTEX_TYPE));
  
    if (!mutex_buf)
	return 0;

    int i;
    for (i = 0; i < CRYPTO_num_locks(); i++)
	MUTEX_SETUP(mutex_buf[i]);

    // CRYPTO_set_id_callback(threadid_func);
    CRYPTO_THREADID_set_callback(threadid_func);
    CRYPTO_set_locking_callback(locking_function);

    pris("Locking and callback functions set");

    return 0;
}

// Cleans up the mutex buffer for openSSL
int THREAD_cleanup(void)
{
    pris("Cleaning up threads");
    if (!mutex_buf)
	return 0;

    /* CRYPTO_set_id_callback(NULL); */
    CRYPTO_THREADID_set_callback(NULL);
    CRYPTO_set_locking_callback(NULL);

    int i;
    for (i = 0; i < CRYPTO_num_locks(); i ++)
	MUTEX_CLEANUP(mutex_buf[i]);

    return 0;

}

void *crypto_update_thread(void* _args)
{

    // clock_t start = clock();
    
    e_thread_args* args = (e_thread_args*)_args;

    int evp_outlen = 0;

    // for (int i = 0; i < args->len; i ++)
    // 	args->in[i] = args->in[i] ^ 0xCC;
    
    if(!EVP_CipherUpdate(args->ctx, args->in, &evp_outlen, args->in, args->len)){
    	fprintf(stderr, "encryption error\n");
    	exit(EXIT_FAILURE);
    }

    if (evp_outlen-args->len){
    	fprintf(stderr, "Did not encrypt full length of data [%d-%d]", 
    		evp_outlen, args->len);
    	exit(1);
    }

    args->len = evp_outlen;

    // clock_t end = clock();
    // double time_elapsed_in_seconds = (end - start)/(double)CLOCKS_PER_SEC;
    // fprintf(stderr, "Time in crypto: %.3f s\n", time_elapsed_in_seconds);

    pthread_exit(NULL);
  
}

int crypto_update(char* in, int len, crypto *c)
{


    int i = 0, evp_outlen = 0;
    if (len == 0) {
	
	// FINALIZE CIPHER
	if (!EVP_CipherFinal_ex(&c->ctx[i], (uchar*)in, &evp_outlen)) {
	    	fprintf(stderr, "encryption error\n");
	    	exit(EXIT_FAILURE);
	}

    } else {

	// UPDATE CIPHER NUMBER
	i = 0;
    
	// for (int i = 0; i < len; i ++)
	//     in[i] = in[i] ^ 0xCC;

    	// [EN][DE]CRYPT
    	if(!EVP_CipherUpdate(&c->ctx[i], (uchar*)in, &evp_outlen, (uchar*)in, len)){
    	    fprintf(stderr, "encryption error\n");
    	    exit(EXIT_FAILURE);
    	}

    	// DOUBLE CHECK
    	if (evp_outlen-len){
    	    fprintf(stderr, "Did not encrypt full length of data [%d-%d]", 
    		    evp_outlen, len);
    	    exit(EXIT_FAILURE);
    	}

    }

    return evp_outlen;

}



