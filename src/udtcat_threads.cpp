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

#define DEBUG 1
#define EXIT_FAILURE 1

#define prii(x) fprintf(stderr,"debug:%d\n",x)
#define pris(x) fprintf(stderr,"debug: %s\n",x)
#define prisi(x,y) fprintf(stderr,"%s: %d\n",x,y)
#define uc_err(x) {fprintf(stderr,"error:%s\n",x);exit(EXIT_FAILURE);}

const int ECONNLOST = 2001;

using std::cerr;
using std::endl;


void print_bytes(const void *object, size_t size) 
{
    size_t i;
    
    fprintf(stderr, "[ ");
    for(i = 0; i < size; i++){
	fprintf(stderr, "%02x ", ((const unsigned char *) object)[i] & 0xff);
    }
    fprintf(stderr, "]\n");
}


void* recvdata(void * _args)
{



    recv_args * args = (recv_args*)_args;
    UDTSOCKET recver = *args->usocket;


    int crypto_buff_len = BUFF_SIZE / N_CRYPTO_THREADS;
    int buffer_cursor;
    char* indata;

    if (USE_CRYPTO){

	long remote_ssl_version = 0;
	
	UDT::recv(recver, (char*)&remote_ssl_version, sizeof(long), 0);

	if (remote_ssl_version != OPENSSL_VERSION_NUMBER){
	    fprintf(stderr, "warning: OpenSSL versions do not match. local [%li] -> remote [%li].\n",
		    OPENSSL_VERSION_NUMBER, remote_ssl_version);
	}

	indata = (char*) malloc(BUFF_SIZE*sizeof(char));
	if (!indata){
	    fprintf(stderr, "Unable to allocate decryption buffer");
	    exit(EXIT_FAILURE);
	}

    }

    int new_block = 1;
    int block_size = 0;
    int offset = sizeof(int)/sizeof(char);
    int crypto_cursor;

    while(true) {
	int rs;

	if (new_block){

	    block_size = 0;
	    rs = UDT::recv(recver, (char*)&block_size, offset, 0);

	    if (UDT::ERROR == rs) {
		if (UDT::getlasterror().getErrorCode() != ECONNLOST)
		    cerr << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
		exit(1);
	    }

	    new_block = 0;
	    buffer_cursor = 0;
	    crypto_cursor = 0;

	}	
	
	rs = UDT::recv(recver, indata+buffer_cursor, 
		       block_size-buffer_cursor, 0);


	if (UDT::ERROR == rs) {
	    UDT::close(recver);
	    return NULL;
	}


	int written_bytes;
	if(USE_CRYPTO) {
	    buffer_cursor += rs;

	    // Decrypt any full encryption buffer sectors
	    while (crypto_cursor + crypto_buff_len < buffer_cursor){

		pass_to_enc_thread(indata+crypto_cursor, indata+crypto_cursor, 
				   crypto_buff_len, args->dec);
		crypto_cursor += crypto_buff_len;

	    }

	    // If we received the whole block
	    if (buffer_cursor == block_size){
		
		int size = buffer_cursor - crypto_cursor;
		pass_to_enc_thread(indata+crypto_cursor, indata+crypto_cursor, 
				   size, args->dec);
		crypto_cursor += size;

		join_all_encryption_threads(args->dec);

		written_bytes = write(fileno(stdout), indata, block_size);

		buffer_cursor = 0;
		crypto_cursor = 0;
		new_block = 1;

	    } 
	    
	    
	} else {
	    written_bytes = write(fileno(stdout), indata, rs);
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

    char* outdata = (char*)malloc(BUFF_SIZE*sizeof(char));
    int crypto_buff_len = BUFF_SIZE / N_CRYPTO_THREADS;

    
    int	offset = sizeof(int)/sizeof(char);
    int bytes_read;

    if (USE_CRYPTO){

	long local_openssl_version = OPENSSL_VERSION_NUMBER;
	UDT::send(client, (char*)&local_openssl_version, sizeof(long), 0);

    }


    while(true) {
	int ss;

	bytes_read = read(fileno(stdin), outdata+offset, BUFF_SIZE);
	
	if(bytes_read < 0){
	    UDT::close(client);
	    return NULL;
	}

	if(bytes_read == 0) {
	    UDT::close(client);
	    return NULL;
	}
	

	if(USE_CRYPTO){

	    *((int*)outdata) = bytes_read;
	    int crypto_cursor = 0;

	    while (crypto_cursor < bytes_read){
		int size = min(crypto_buff_len, bytes_read-crypto_cursor);
		pass_to_enc_thread(outdata+crypto_cursor+offset, 
				   outdata+crypto_cursor+offset, 
				   size, args->enc);
		
		crypto_cursor += size;
		
	    }
	    
	    join_all_encryption_threads(args->enc);

	    bytes_read += offset;

	}

	int ssize = 0;
	while(ssize < bytes_read) {
	    
	    if (UDT::ERROR == (ss = UDT::send(client, outdata + ssize, 
					      bytes_read - ssize, 0))) {
		
		return NULL;
	    }

	    ssize += ss;

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
