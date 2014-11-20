/*****************************************************************************
Copyright 2013 Laboratory for Advanced Computing at the University of Chicago

This file is part of udpipe.

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
#include <signal.h>

#include "udpipe.h"
#include "udpipe_client.h"

using std::cerr;
using std::endl;

class udpipeClient
{
public:
    char *host;
    char *port;
    int blast;
    int blast_rate;
    int udt_buff;
    int udp_buff;
    int mss;
    int verbose;
    UDTSOCKET socket, *snd_socket, *rcv_socket;
    struct addrinfo *local, *peer;
    pthread_t rcvthread, sndthread;
    int timeout;
    rs_args send_args;
    rs_args rcvargs;
    
    // Constructors
    udpipeClient();
    udpipeClient(char *host, char *port);
    udpipeClient(char *host, char *port, int verbose);
    udpipeClient(char *host, char *port, int verbose, int blast_rate, int blast, int udt_buff, int udp_buff, int mss);
         
    int start();
    int join();
    int startUDT();
    int setup();
    int connect();
    int set_timeout(int _timeout);
    int start_receive_thread();
    int start_send_thread();

};

extern "C" {
    udpipeClient* udpipeClient_new_host_port(char *_host, char *_port){ 
        return new udpipeClient(_host, _port); 
    }
    udpipeClient* udpipeClient_new(){ 
        return new udpipeClient(); 
    }
    void udpipeClient_start(udpipeClient* client){ 
        client->start(); 
    }
    void udpipeClient_join(udpipeClient* client){ 
        client->join(); 
    }
}

int udpipeClient::start()
{

    if (!host){
        cerr << "No host set before starting client." << endl;
        return 1;
    }
    
    if (!port){
        cerr << "No portset before starting client." << endl;
        return 1;
    }

    startUDT();
    setup();
    connect();
    start_receive_thread();
    start_send_thread();
    join();
    return 0;
}

int udpipeClient::join()
{
    void * retval;

    if (sndthread){
        pthread_join(sndthread, &retval);
    }

    if (rcvthread){
        pthread_join(rcvthread, &retval);
    }

    return 0;
}

int udpipeClient::set_timeout(int _timeout)
{
    timeout = _timeout;
    return 0;
}


udpipeClient::udpipeClient() 
{
    host   = NULL;
    port = NULL;
    
    // defaults
    blast = 0;
    blast_rate = 0;
    udt_buff = 67108864;
    udp_buff = 67108864;
    mss = 8000;
    verbose = 0;
}


udpipeClient::udpipeClient(char *_ip, 
                           char *_port) 
{
    host   = strdup(_ip);
    port = strdup(_port);
    
    // defaults
    blast = 0;
    blast_rate = 0;
    udt_buff = 67108864;
    udp_buff = 67108864;
    mss = 8000;
    verbose = 0;
}

udpipeClient::udpipeClient(char *_ip, 
                           char *_port, 
                           int _verbose) 
{
    host      = strdup(_ip);
    port    = strdup(_port);
    verbose = _verbose;

    // defaults
    blast = 0;
    blast_rate = 0;
    udt_buff = 67108864;
    udp_buff = 67108864;
    mss = 8000;
}

udpipeClient::udpipeClient(char *_ip, 
                           char *_port, 
                           int _verbose,
                           int _blast, 
                           int _blast_rate, 
                           int _udt_buff, 
                           int _udp_buff, 
                           int _mss)
{
    host         = strdup(_ip);
    port       = strdup(_port);
    verbose    = _verbose;
    blast      = _blast;
    blast_rate = _blast_rate;
    udt_buff   = _udt_buff;
    udp_buff   = _udp_buff; 
    mss        = _mss;
}


int udpipeClient::start_send_thread()
{
    if (verbose) fprintf(stderr, "[udpipeClient] Creating send thread...\n");

    send_args.usocket = snd_socket;
    send_args.verbose = verbose;
    send_args.timeout = timeout;

    // encryption
    send_args.use_crypto = 0;
    send_args.n_crypto_threads = 1;

    // send_args.use_crypto = args->use_crypto;
    // send_args.n_crypto_threads = args->n_crypto_threads;
    // send_args.c = args->enc;

    pthread_create(&sndthread, NULL, senddata, &send_args);

    return 0;
}

int udpipeClient::start_receive_thread()
{
    if (verbose) fprintf(stderr, "[udpipeClient] Creating receive thread...\n");

    rcvargs.usocket = rcv_socket;
    rcvargs.timeout = timeout;
    rcvargs.verbose = verbose;

    // encryption parameters
    rcvargs.use_crypto = 0;
    rcvargs.n_crypto_threads = 1;

    // rcvargs.use_crypto = args->use_crypto;
    // rcvargs.n_crypto_threads = args->n_crypto_threads;
    // rcvargs.c = args->dec;

    pthread_create(&rcvthread, NULL, recvdata, &rcvargs);

    return 0;
}

int udpipeClient::startUDT()
{
    if (verbose) fprintf(stderr, "Starting UDT...\n");
    UDT::startup();
    return 0;
}

int udpipeClient::setup()
{
    if (verbose) fprintf(stderr, "[udpipeClient] Creating socket...\n");

    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (0 != getaddrinfo(NULL, port, &hints, &local)){
        cerr << "incorrect network address.\n" << endl;
        return 1;
    }
    
    socket = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);

    // UDT Options
    if (blast){
        UDT::setsockopt(socket, 0, 
                        UDT_CC, 
                        new CCCFactory<CUDPBlast>, 
                        sizeof(CCCFactory<CUDPBlast>));
    }
	
    UDT::setsockopt(socket, 0, UDT_MSS, &mss, sizeof(int));
    UDT::setsockopt(socket, 0, UDT_SNDBUF, &udt_buff, sizeof(int));
    UDT::setsockopt(socket, 0, UDP_SNDBUF, &udp_buff, sizeof(int));

    if (0 != getaddrinfo(host, port, &hints, &peer)) {
        cerr << "incorrect server/peer address. " << host << ":" << port << endl;
        return 1;
    }
    return 1;
}

int udpipeClient::connect()
{
    if (verbose) fprintf(stderr, "[udpipeClient] Connecting to server...\n");
    if (UDT::ERROR == UDT::connect(socket, peer->ai_addr, peer->ai_addrlen)) {
        cerr << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
        return 1;
    }
    rcv_socket = new UDTSOCKET(socket);
    snd_socket = new UDTSOCKET(socket);
    return 0;
}


int run_client(thread_args *args)
{

    if (args->verbose)
	fprintf(stderr, "[udpipeClient] Running client...\n");

    udpipeClient client(args->ip, 
                        args->port, 
                        args->verbose,
                        args->blast, 
                        args->blast_rate, 
                        args->udt_buff, 
                        args->udp_buff, 
                        args->mss);

    client.start();

    // if (blast) {
    //     CUDPBlast* cchandle = NULL;
    //     int temp;
    //     UDT::getsockopt(client, 0, UDT_CC, &cchandle, &temp);
    //     if (NULL != cchandle)
    //         cchandle->setRate(blast_rate);
    // }
    // if (args->print_speed){
    //     pthread_t mon_thread;
    //     pthread_create(&mon_thread, NULL, monitor, &client);
	
    // }
    // Partial cause of segfault issue commented out for now
    // UDT::cleanup();

    return 0;
}
