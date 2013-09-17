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

#define EXIT_FAILURE 1

#define prii(x) fprintf(stderr,"debug:%d\n",x)
#define pris(x) fprintf(stderr,"debug: %s\n",x)
#define prisi(x,y) fprintf(stderr,"%s: %d\n",x,y)
#define uc_err(x) {fprintf(stderr,"error:%s\n",x);exit(EXIT_FAILURE);}

const int ECONNLOST = 2001;

using std::cerr;
using std::endl;

int n_recv_threads = 0;
int last_printed = -1;
pthread_mutex_t lock;

/* 
 THINGS NEEDED IN APPLICATION TO RUN THE MT SUPPORTED CRYPTO:
   pthread_t crypto_threads[N_THREADS];
   e_thread_args * e_args;
   crypto *c;
   int* curr_crypto_thread;
*/

void* recvdata(void * _args)
{

    // Handle socket
    recv_args * args = (recv_args*)_args;
    UDTSOCKET recver = *args->usocket;

    // Decryption locals
    int read_len;
    e_thread_args e_args[N_CRYPTO_THREADS];

    pthread_t decryption_threads[N_CRYPTO_THREADS];
    
    int decrypt_buf_len = BUFF_SIZE / N_CRYPTO_THREADS;
    int len, decrypt_cursor, buffer_cursor, curr_crypto_thread;
    decrypt_cursor = buffer_cursor = curr_crypto_thread = 0;

    if (USE_CRYPTO){

	char* decrypt_buffer = (char*) malloc(BUFF_SIZE*sizeof(char));
	if (!decrypt_buffer){
	    fprintf(stderr, "Unable to allocate decryption buffer");
	    exit(EXIT_FAILURE);
	}

	while (1) {

	    // Read in from UDT
	    if (UDT::ERROR == (len = UDT::recv(recver, decrypt_buffer+buffer_cursor, 
					       BUFF_SIZE-buffer_cursor, 0))) {

		if (UDT::getlasterror().getErrorCode() != ECONNLOST)
		    cerr << "recv:" << UDT::getlasterror().getErrorMessage() << endl;

		// Finish any remaining data in the buffer
		if (buffer_cursor > 0){
		    join_all_encryption_threads(decryption_threads);
		    crypto_update(decrypt_buffer+decrypt_cursor, 
				  buffer_cursor-decrypt_cursor, args->dec);
		    write(fileno(stdout), decrypt_buffer, buffer_cursor);
		}

		exit(0);
	    }

	    buffer_cursor += len;

	    // This should never happen
	    if (buffer_cursor > BUFF_SIZE)
		uc_err("Decryption buffer overflow");

	    // Decrypt what we've got
	    while (decrypt_cursor+decrypt_buf_len <= buffer_cursor){
		pass_to_enc_thread(decryption_threads, 
				   e_args,
				   &curr_crypto_thread,
				   decrypt_buffer+decrypt_cursor, 
				   decrypt_buf_len,
				   args->dec);
		decrypt_cursor += decrypt_buf_len;
	    }

	    // Write the decrypted buffer and reset
	    if (decrypt_cursor >= BUFF_SIZE){
		join_all_encryption_threads(decryption_threads);
		write(fileno(stdout), decrypt_buffer, BUFF_SIZE);
		buffer_cursor = decrypt_cursor = curr_crypto_thread = 0;
	    }

	}

	free(decrypt_buffer);

    } else { 
	char* data = new char[BUFF_SIZE];
	while (1){
	    if (UDT::ERROR == (read_len = UDT::recv(recver, data, BUFF_SIZE, 0))) {
		if (UDT::getlasterror().getErrorCode() != ECONNLOST)
		    cerr << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
		break;
	    }
	    write(fileno(stdout), data, read_len);
	}
    }

    UDT::close(recver);
    return NULL;

}

int send_buf(UDTSOCKET client, char* buf, int size, int flags)
{

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
	exit(EXIT_FAILURE);
    }

    return ss;

}


void* senddata(void* _args)
{
        
    snd_args * args = (snd_args*) _args;
    UDTSOCKET client = *(UDTSOCKET*)args->usocket;

    char* encrypt_buffer;

    e_thread_args e_args[N_CRYPTO_THREADS];
    int encrypt_buf_len = BUFF_SIZE / N_CRYPTO_THREADS;

    pthread_t encryption_threads[N_CRYPTO_THREADS];

    int flags = 0;
    int len, encrypt_cursor, buffer_cursor, curr_crypto_thread;
    encrypt_cursor = buffer_cursor = curr_crypto_thread = 0;

    if (USE_CRYPTO){
	
	if (!(encrypt_buffer = (char*) malloc(2*BUFF_SIZE*sizeof(char))))
	    uc_err("Unable to allocate encryption buffer");

	curr_crypto_thread = 0;

	while (1) {

	    len = read(STDIN_FILENO, encrypt_buffer+buffer_cursor, BUFF_SIZE-buffer_cursor);

	    if (len < 0) {
		uc_err(strerror(errno));
	    } else if (!len){
		break;
	    }

	    buffer_cursor += len;

	    // This should never happen
	    if (buffer_cursor > BUFF_SIZE)
		uc_err("Encryption buffer overflow");

	    // Encrypt data
	    while (encrypt_cursor+encrypt_buf_len <= buffer_cursor){
		pass_to_enc_thread(encryption_threads, 
				   e_args,
				   &curr_crypto_thread,
				   encrypt_buffer+encrypt_cursor, 
				   encrypt_buf_len,
				   args->enc);
		encrypt_cursor += encrypt_buf_len;
	    }

	    // If full buffer, then send to UDT
	    if (encrypt_cursor >= BUFF_SIZE){
		join_all_encryption_threads(encryption_threads);
		send_buf(client, encrypt_buffer, buffer_cursor, flags);
		buffer_cursor = encrypt_cursor = curr_crypto_thread = 0;
	    }


	}

	// Finish any remaining buffer data
	if (buffer_cursor > 0){
	    join_all_encryption_threads(encryption_threads);
	    crypto_update(encrypt_buffer+encrypt_cursor, buffer_cursor-encrypt_cursor, args->enc);
	    send_buf(client, encrypt_buffer, buffer_cursor, flags);
	    
	}

	free(encrypt_buffer);

    } else { // Ignore crypto

	char *data;
	if (!(data = (char*)malloc(BUFF_SIZE*sizeof(char))))
	    uc_err("Unable to allocate thread buffer data");
	    
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
	exit(EXIT_FAILURE);
    }

    args->idle = 1;
    args->size = ss;
    pthread_exit(NULL);

}
