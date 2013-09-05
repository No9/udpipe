/*****************************************************************************
Copyright 2013 Laboratory for Advanced Computing at the University of Chicago

This file is part of udtcat.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions
and limitations under the License.
*****************************************************************************/
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <iostream>
#include <udt.h>

#include "udtcat.h"
#include "udtcat_threads.h"

#define prii(x) fprintf(stderr,"debug:%d\n",x)
#define pris(x) fprintf(stderr,"debug: %s\n",x)
#define prisi(x,y) fprintf(stderr,"%s: %d\n",x,y)
#define uc_err(x) {fprintf(stderr,"error:%s\n",x);exit(1);}


const int ECONNLOST = 2001;

using std::cerr;
using std::endl;

int n_recv_threads = 0;
int last_printed = -1;
pthread_mutex_t lock;

void* recvdata(void * _args)
{
    recv_args * args = (recv_args*)_args;

    UDTSOCKET recver = *args->usocket;
    // delete (UDTSOCKET*) args->usocket;

    int size = BUFF_SIZE;
    int read_len;
    char* data = new char[size];

    e_thread_args e_args[N_CRYPTO_THREADS];
    int decrypt_buf_len = BUFF_SIZE / N_CRYPTO_THREADS;

    prisi("decrypt_buf_len", decrypt_buf_len);
		
    pthread_t decryption_threads[N_CRYPTO_THREADS];

    int len;
    int decrypt_cursor = 0;
    int buffer_cursor = 0;
    int curr_crypto_thread = 0;
    int overflow = 0;


    if (USE_CRYPTO){

	char* decrypt_buffer = (char*) malloc(2*BUFF_SIZE*sizeof(char));
	if (!decrypt_buffer)
	    uc_err("Error allocating decryption buffer");

	while (1) {

	    // read in from UDT
	    if (UDT::ERROR == (len = UDT::recv(recver, decrypt_buffer+buffer_cursor, size, 0))) {
		if (UDT::getlasterror().getErrorCode() != ECONNLOST)
		    cerr << "recv:" << UDT::getlasterror().getErrorMessage() << endl;

		if (buffer_cursor > 0){

		    for (int i = 0; i < curr_crypto_thread; i++){
			if (pthread_join(decryption_threads[i], NULL))
			    uc_err("Unable to join decryption thread[d]");
		    }
		    curr_crypto_thread = 0;

		    crypto_update(decrypt_buffer+decrypt_cursor, 
				  buffer_cursor-decrypt_cursor,
				  args->dec);

		    write(fileno(stdout), decrypt_buffer, buffer_cursor);
    
		}

		exit(0);
	    }

	    buffer_cursor += len;

	    if (buffer_cursor >= 2*BUFF_SIZE)
		uc_err("Decryption buffer overflow");

	    while (decrypt_cursor+decrypt_buf_len <= buffer_cursor){

		if (curr_crypto_thread >= N_CRYPTO_THREADS){
		    for (int i = 0; i < N_CRYPTO_THREADS; i++){
			if (pthread_join(decryption_threads[i], NULL))
			    uc_err("Unable to join decryption thread[e]");
		    }
		    curr_crypto_thread = 0;
		}

		e_args[curr_crypto_thread].in = (uchar*) decrypt_buffer+decrypt_cursor;
		e_args[curr_crypto_thread].len = decrypt_buf_len;
		e_args[curr_crypto_thread].c = args->dec;
		e_args[curr_crypto_thread].ctx = &args->dec->ctx[curr_crypto_thread];
		
		pthread_create(&decryption_threads[curr_crypto_thread],
			       NULL, crypto_update_thread, &e_args);

		curr_crypto_thread++;
		decrypt_cursor += decrypt_buf_len;

	    }

	    if (buffer_cursor >= BUFF_SIZE){

		for (int i = 0; i < curr_crypto_thread; i++){
		    if (pthread_join(decryption_threads[i], NULL))
			uc_err("Unable to join decryption thread");
		}

		write(fileno(stdout), decrypt_buffer, BUFF_SIZE);

		if (buffer_cursor > BUFF_SIZE){

		    fprintf(stderr, "Fixing overflow\n");

		    buffer_cursor -= BUFF_SIZE;
		    
		    memmove(decrypt_buffer, 
			    decrypt_buffer+BUFF_SIZE, 
			    buffer_cursor);

		    decrypt_cursor = 0;
		    
		} else {
		
		    buffer_cursor = 0;
		    decrypt_cursor = 0;
		}

		curr_crypto_thread = 0;


	    }

	}

	prisi("Last buffer state", buffer_cursor);


    } else { 

	while (1){

	    // read in from UDT
	    if (UDT::ERROR == (read_len = UDT::recv(recver, data, size, 0))) {
		if (UDT::getlasterror().getErrorCode() != ECONNLOST)
		    cerr << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
		exit(0);
	    }

	    write(fileno(stdout), data, read_len);
	}

    }


    // free(data);
    // delete [] data;

    UDT::close(recver);

    return NULL;
}

clock_t start, end;

int send_buf(UDTSOCKET client, char* buf, int size, int flags)
{


    // end = clock();
    // double time_elapsed_in_seconds = (end - start)/(double)CLOCKS_PER_SEC;
    // fprintf(stderr, "Time since last send: %f\n", time_elapsed_in_seconds);


    int ssize = 0;
    int ss;
    while (ssize < size) {
	if (UDT::ERROR == (ss = UDT::send(client, buf + ssize, size - ssize, 0))) {
	    cerr << "send:" << UDT::getlasterror().getErrorMessage() << endl;
	    break;
	}

	ssize += ss;
    }

    if (ssize < size)
	pris("Did not send complete buffer");

    if (UDT::ERROR == ss) {
	cerr << "send:" << UDT::getlasterror().getErrorMessage() << endl;
	exit(1);
    }

    // start = clock();

    return ss;

}


void* senddata(void* _args)
{
    
    start = clock();
    
    snd_args * args = (snd_args*) _args;

    UDTSOCKET client = *(UDTSOCKET*)args->usocket;
    // delete (UDTSOCKET*)usocket;


    e_thread_args e_args[N_CRYPTO_THREADS];
    int encrypt_buf_len = BUFF_SIZE / N_CRYPTO_THREADS;
    char* encrypt_buffer;
		
    pthread_t encryption_threads[N_CRYPTO_THREADS];

    char *data;
    int flags = 0;

    if (!(data = (char*)malloc(BUFF_SIZE*sizeof(char))))
	uc_err("Unable to allocate thread buffer data");

    int len;
    int encrypt_cursor = 0;
    int buffer_cursor = 0;
    int curr_crypto_thread = 0;

    if (USE_CRYPTO){
	
	if (!(encrypt_buffer = (char*) malloc(2*BUFF_SIZE*sizeof(char))))
	    uc_err("Unable to allocate encryption buffer");

	curr_crypto_thread = 0;

	while (1) {

	    len = read(STDIN_FILENO, encrypt_buffer+buffer_cursor, BUFF_SIZE);

	    if (len < 0) {
		uc_err(strerror(errno));
	    } else if (!len){
		break;
	    }

	    buffer_cursor += len;

	    if (buffer_cursor >= 2*BUFF_SIZE)
		uc_err("Preventing encryption buffer overflow");

	    while (encrypt_cursor+encrypt_buf_len <= buffer_cursor){

		if (curr_crypto_thread >= N_CRYPTO_THREADS){
		    for (int i = 0; i < N_CRYPTO_THREADS; i++){
			if (pthread_join(encryption_threads[i], NULL))
			    uc_err("Unable to join encryption thread. [a]");
		    }
		    curr_crypto_thread = 0;
		}

		e_args[curr_crypto_thread].in = (uchar*) encrypt_buffer+encrypt_cursor;
		e_args[curr_crypto_thread].len = encrypt_buf_len;
		e_args[curr_crypto_thread].c = args->enc;
		e_args[curr_crypto_thread].ctx = &args->enc->ctx[curr_crypto_thread];
		
		pthread_create(&encryption_threads[curr_crypto_thread],
			       NULL, crypto_update_thread, &e_args);

		curr_crypto_thread++;
		encrypt_cursor += encrypt_buf_len;

	    }

	    if (buffer_cursor >= BUFF_SIZE){

		for (int i = 0; i < curr_crypto_thread; i++){
		    if (pthread_join(encryption_threads[i], NULL))
			uc_err("Unable to join encryption thread. [b]");
		}

		send_buf(client, encrypt_buffer, buffer_cursor, flags);

		if (buffer_cursor > BUFF_SIZE){

		    buffer_cursor -= BUFF_SIZE;
		    
		    memmove(encrypt_buffer, 
			    encrypt_buffer+BUFF_SIZE, 
			    buffer_cursor);

		    encrypt_cursor = 0;
		    
		} else {
		
		    buffer_cursor = 0;
		    encrypt_cursor = 0;
		}

		curr_crypto_thread = 0;

	    }


	}

	prisi("Last cursor state", buffer_cursor);
	if (buffer_cursor > 0){
	    for (int i = 0; i < curr_crypto_thread; i++){
		if (pthread_join(encryption_threads[i], NULL))
		    uc_err("Unable to join encryption thread[c]");
	    }


	    crypto_update(encrypt_buffer+encrypt_cursor, 
			  buffer_cursor-encrypt_cursor,
			  args->enc);

	    send_buf(client, encrypt_buffer, buffer_cursor, flags);
	    
	}


    } else { // Ignore crypto
	    
	while (1) {
	    len = read(STDIN_FILENO, data, BUFF_SIZE);

	    if (len < 0){
		uc_err(strerror(errno));

	    } else if (!len) {
		break;
    
	    } else {
		send_buf(client, data, len, flags);
	    }
	    
	}
    }


    free(data);

    UDT::close(client);

    return NULL;
}

void* send_buf_threaded(void*_args)
{
  
    send_buf_args *args = (send_buf_args*) _args;
    args->idle = 0;

    UDTSOCKET client = args->client;

    char* buf = args->buf;
    int size = args->size;
    // int flags = args->flags;
  
    int ssize = 0;
    int ss;

    while (ssize < size) {
	if (UDT::ERROR == (ss = UDT::send(client, buf + ssize, size - ssize, 0))) {
	    cerr << "send:" << UDT::getlasterror().getErrorMessage() << endl;
	    break;
	}

	ssize += ss;
    }

    if (ssize < size)
	uc_err("Did not send complete buffer");

    if (UDT::ERROR == ss) {
	cerr << "send:" << UDT::getlasterror().getErrorMessage() << endl;
	exit(1);
    }

    args->idle = 1;
    args->size = ss;
    pthread_exit(NULL);

}
