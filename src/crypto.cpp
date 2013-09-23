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
#define DEBUG 0

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

    e_thread_args* args = (e_thread_args*)_args;
    int evp_outlen = 0;
    
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

    	// [EN][DE]CRYPT
    	if(!EVP_CipherUpdate(&c->ctx[0], (uchar*)in, &evp_outlen, (uchar*)in, len)){
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

int joined[N_CRYPTO_THREADS];

int pthread_join_disregard_ESRCH(pthread_t thread, int thread_id){

    if (joined[thread_id])
	return 0;

    int ret = pthread_join(thread, NULL);

    pthread_mutex_lock(&c_lock);
    joined[thread_id] = 1;
    pthread_mutex_unlock(&c_lock);

    if (ret){
	if (ret != ESRCH){
	    fprintf(stderr, "Unable to join encryption thread: %d\n", ret);
	    exit(1);
	}
    }

    return 0;

}

int join_all_encryption_threads(pthread_t threads[N_CRYPTO_THREADS]){
    for (int i = 0; i < N_CRYPTO_THREADS; i++)
	pthread_join_disregard_ESRCH(threads[i], i);
    return 0;
}

int pass_to_enc_thread(pthread_t crypto_threads[N_CRYPTO_THREADS], 
		       e_thread_args * e_args,
		       int * curr_crypto_thread,
		       char* in, int len, 
		       crypto*c){

    pthread_join_disregard_ESRCH(crypto_threads[*curr_crypto_thread], *curr_crypto_thread);

    // ----------- [ Initialize and set thread detached attribute
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // ----------- [ Setup thread
    e_args[*curr_crypto_thread].in = (uchar*) in;
    e_args[*curr_crypto_thread].len = len;
    e_args[*curr_crypto_thread].ctx = &c->ctx[*curr_crypto_thread];

    // ----------- [ Spawn thread
    int ret = pthread_create(&crypto_threads[*curr_crypto_thread],
			     &attr, crypto_update_thread, &e_args[*curr_crypto_thread]);

    pthread_mutex_lock(&c_lock);
    joined[*curr_crypto_thread] = 0;
    pthread_mutex_unlock(&c_lock);
    
    if (ret){
	fprintf(stderr, "Unable to create thread: %d\n", ret);
	exit(1);
    }

    *curr_crypto_thread = *curr_crypto_thread+1;

    if (*curr_crypto_thread>=N_CRYPTO_THREADS)
	*curr_crypto_thread = 0;

    return 0;
}

