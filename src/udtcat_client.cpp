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
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "udtcat.h"
#include "udtcat_client.h"

#define prii(x) fprintf(stderr,"debug:%d\n",x)
#define pris(x) fprintf(stderr,"debug: %s\n",x)
#define uc_err(x) {fprintf(stderr,"error:%s\n",x);exit(1);}

using std::cerr;
using std::endl;

int run_client(thread_args *args)
{

    if (args->verbose)
	fprintf(stderr, "Running client...\n");

    char *ip = args->ip; 
    char *port = args->port;
    int blast = args->blast;
    int blast_rate = args->blast_rate;
    int udt_buff = args->udt_buff;
    int udp_buff = args->udp_buff; // 67108864;
    int mss = args->mss;


    if (args->verbose)
	fprintf(stderr, "Starting UDT...\n");

    UDT::startup();

    struct addrinfo hints, *local, *peer;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (0 != getaddrinfo(NULL, port, &hints, &local)){
	cerr << "incorrect network address.\n" << endl;
	return 1;
    }
    

    if (args->verbose)
	fprintf(stderr, "Creating socket...\n");

    
    UDTSOCKET client;
    client = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);

    // UDT Options
    if (blast)
	UDT::setsockopt(client, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
	
    UDT::setsockopt(client, 0, UDT_MSS, &mss, sizeof(int));
    UDT::setsockopt(client, 0, UDT_SNDBUF, &udt_buff, sizeof(int));
    UDT::setsockopt(client, 0, UDP_SNDBUF, &udp_buff, sizeof(int));

    freeaddrinfo(local);

    if (0 != getaddrinfo(ip, port, &hints, &peer)) {
	cerr << "incorrect server/peer address. " << ip << ":" << port << endl;
	return 1;
    }

    if (args->verbose)
	fprintf(stderr, "Connecting to server...\n");
    
    if (UDT::ERROR == UDT::connect(client, peer->ai_addr, peer->ai_addrlen)) {
	
	// cerr << "connect: " << UDT::getlasterror().getErrorCode() << endl;
	cerr << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
	    
	return 1;
    }

    if (args->verbose)
	fprintf(stderr, "Creating receive thread...\n");

    pthread_t rcvthread, sndthread;
    rs_args rcvargs;
    rcvargs.usocket = new UDTSOCKET(client);
    rcvargs.use_crypto = args->use_crypto;
    rcvargs.verbose = args->verbose;
    rcvargs.c = args->dec;

    pthread_create(&rcvthread, NULL, recvdata, &rcvargs);
    pthread_detach(rcvthread);


    if (args->verbose)
	fprintf(stderr, "Creating send thread...\n");


    rs_args send_args;
    send_args.usocket = new UDTSOCKET(client);
    send_args.use_crypto = args->use_crypto;
    send_args.verbose = args->verbose;
    send_args.c = args->enc;

    freeaddrinfo(peer);

    if (blast) {
	CUDPBlast* cchandle = NULL;
	int temp;
	UDT::getsockopt(client, 0, UDT_CC, &cchandle, &temp);
	if (NULL != cchandle)
	    cchandle->setRate(blast_rate);
    }

    pthread_create(&sndthread, NULL, senddata, &send_args);

    if (args->verbose)
	fprintf(stderr, "Setup complete.\n");

    void * retval;
    pthread_join(sndthread, &retval);

    UDT::close(client);
  
    UDT::cleanup();
    return 0;
}

