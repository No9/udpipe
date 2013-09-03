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

    pthread_mutex_lock(&lock);
    int thread_num = n_recv_threads++;
    pthread_mutex_unlock(&lock);
    fprintf(stderr, "New recv thread %d\n", thread_num);
    
    UDTSOCKET recver = *args->usocket;
    // delete (UDTSOCKET*) args->usocket;

    // UDTSOCKET recver = *(UDTSOCKET*)usocket;
    // delete (UDTSOCKET*) usocket;

    char* data;
    int size = BUFF_SIZE;
    data = new char[size];

    while (true) {
	int rs;

	if (UDT::ERROR == (rs = UDT::recv(recver, data, size, 0))) {
	    if (UDT::getlasterror().getErrorCode() == ECONNLOST){
		exit(0);
	    } else {
		cerr << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
	    }
	    exit(0);
	    break;
	}

	// fprintf(stdout, "waiting on %d\n", last_printed);
	// while (last_printed != thread_num-1);

	// pthread_mutex_lock(&lock);
	// if (thread_num == n_recv_threads-1)
	//     last_printed = -1;
	// else 
	//     last_printed = thread_num;
	// pthread_mutex_unlock(&lock);

#ifdef CRYPTO
	char* plaintext = (char*) malloc(rs*sizeof(char));
	encrypt(data, plaintext, rs, args->dec);
	write(fileno(stdout), plaintext, rs);
#else	
	write(fileno(stdout), data, rs);
#endif

    }

    delete [] data;

    UDT::close(recver);

    return NULL;
}


void* senddata(void* usocket)
{

    fprintf(stderr, "New send thread\n");
    UDTSOCKET client = *(UDTSOCKET*)usocket;
    delete (UDTSOCKET*)usocket;

    char* data = NULL;
    size_t buf_size = BUFF_SIZE;
    int size;

    while ((size = getline(&data, &buf_size, stdin)) > 0) {
    	int ssize = 0;
    	int ss;

    	while (ssize < size) {
    	    if (UDT::ERROR == (ss = UDT::send(client, data + ssize, size - ssize, 0)))
    		{
    		    cerr << "send:" << UDT::getlasterror().getErrorMessage() << endl;
    		    break;
    		}

    	    ssize += ss;
    	}

    	if (ssize < size)
    	    break;
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
    int flags = args->flags;
  
    int ssize = 0;
    int ss;
    int* ret = (int*)malloc(sizeof(int));

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
    *ret = ss;
    args->idle = 1;

    return ret;

}
