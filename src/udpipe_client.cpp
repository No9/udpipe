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
#include <fcntl.h>
#include "udpipe.h"
#include "udpipe_client.h"

using std::cerr;
using std::endl;

class udpipeClient
{
public:

    int pipe_in[2], pipe_out[2];
    int *pipe_in_read, *pipe_in_write, *pipe_out_read, *pipe_out_write;
    char *host, *port;
    int blast, blast_rate, udt_buff, udp_buff, mss, verbose, timeout;
    UDTSOCKET socket, *snd_socket, *rcv_socket;
    struct addrinfo *local, *peer;
    pthread_t rcvthread, sndthread;
    rs_args send_args, rcvargs;
    char *buffer_in;
    
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
    int start_receive_thread();
    int start_send_thread();

    // Class modifier methods
    int set_timeout(int _timeout);
    int set_host(char *_host);
    int set_port(char *_port);
    int send_from_fd(int fd);
    int send_file(char *path);
    int write_buffer(char *path, int len);
    int create_pipes();

};

extern "C" {

    udpipeClient* udpipeClient_new_host_port(char *_host, char *_port){ 
        return new udpipeClient(_host, _port); 
    }

    udpipeClient* udpipeClient_new(){ 
        return new udpipeClient(); 
    }

    int udpipeClient_start(udpipeClient* client){ 
        return client->start(); 
    }

    void udpipeClient_join(udpipeClient* client){ 
        client->join(); 
    }

    void udpipeClient_set_host(udpipeClient* client, char *_host){ 
        client->set_host(_host);
    }

    void udpipeClient_set_port(udpipeClient* client, char *_port){ 
        client->set_port(_port);
    }

    void udpipeClient_send_from_fd(udpipeClient* client, int _fd){ 
        client->send_from_fd(_fd);
    }

    void udpipeClient_send_file(udpipeClient* client, char *path){ 
        client->send_file(path);
    }

    int udpipeClient_write_buffer(udpipeClient* client, char *buffer, int len){ 
        return client->write_buffer(buffer, len);
    }

    int udpipeClient_get_pipe_in(udpipeClient* client){ 
        return *client->pipe_in_write;
    }

    int udpipeClient_get_pipe_out(udpipeClient* client){ 
        return *client->pipe_out_read;
    }


}

int udpipeClient::start()
{

    if (!host){
        cerr << "No host set before starting client." << endl;
        return -1;
    }
    
    if (!port){
        cerr << "No portset before starting client." << endl;
        return -1;
    }

    create_pipes();
    startUDT();
    setup();
    connect();
    start_receive_thread();
    start_send_thread();

    return fileno(stdin);
}

int udpipeClient::create_pipes()
{
    pipe(pipe_in);
    pipe(pipe_out);

    pipe_in_read   = pipe_in;
    pipe_in_write  = pipe_in + 1;
    pipe_out_read  = pipe_out;
    pipe_out_write = pipe_out + 1;

    return 0;
}

int udpipeClient::join()
{
    void * retval;

    if (sndthread){
        if (verbose) cout << "Joining send thread " << endl;        
        pthread_join(sndthread, &retval);
    }

    if (rcvthread){
        if (verbose) cout << "Joining receive thread " << endl;        
        pthread_join(rcvthread, &retval);
    }

    return 0;
}

int udpipeClient::set_timeout(int _timeout)
{
    timeout = _timeout;
    return 0;
}


int udpipeClient::set_host(char *_host)
{
    if (host){
        if (verbose) cout << "Freeing old host" << endl;
        free(host);
    }

    if (verbose) cout << "Setting host to: " << _host << endl;
    host = strdup(_host);

    return 0;
}


int udpipeClient::set_port(char *_port)
{
    if (port){
        if (verbose) cout << "Freeing old port" << endl;
        free(port);
    }

    if (verbose) cout << "Setting port to: " << _port << endl;

    port = strdup(_port);
    return 0;
}


udpipeClient::udpipeClient() 
{
    host = NULL;
    port = NULL;
    buffer_in = NULL;

    // defaults
    blast = 0;
    blast_rate = 0;
    udt_buff = 67108864;
    udp_buff = 67108864;
    mss = 8000;
    verbose = 1;
}


udpipeClient::udpipeClient(char *_ip, 
                           char *_port) 
{
    host = strdup(_ip);
    port = strdup(_port);
    buffer_in = NULL;

    // defaults
    blast = 0;
    blast_rate = 0;
    udt_buff = 67108864;
    udp_buff = 67108864;
    mss = 8000;
    verbose = 1;
}

udpipeClient::udpipeClient(char *_ip, 
                           char *_port, 
                           int _verbose) 
{
    host    = strdup(_ip);
    port    = strdup(_port);
    verbose = _verbose;
    buffer_in = NULL;

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
    buffer_in = NULL;
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

    send_args.pipe_in = pipe_in[0];
    send_args.pipe_out = pipe_out[1];

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

    rcvargs.pipe_out = pipe_out[1];

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

int udpipeClient::write_buffer(char *buff, int len)
{

    if (verbose) cout << "write_buffer to pipe: " << *pipe_in_write << endl;
    int written = write(*pipe_in_write, buff, len);
    if (verbose) cout << "Wrote: " << written << endl;

    return 0;
}



int udpipeClient::send_file(char *path)
{
    int fd;

    if (verbose) cout << "Opening file: " << path << endl;
    if ((fd = open(path, O_RDONLY)) < 0){
        cerr << "Unable to open file: " << errno << endl;
    }
    if (verbose) cout << "Opened: " << path << endl;

    send_from_fd(fd);
    close(fd);
    return 0;
}


int udpipeClient::send_from_fd(int fd)
{
        
    if (!buffer_in) {
        cout << "Creating buffer of size " << udt_buff*sizeof(char) << endl;
        buffer_in = (char*) malloc(udt_buff*sizeof(char));
    }
    
    while (1) {
        
        int ssize = 0;
        int bytes_read = read(fd, buffer_in, udt_buff);

        if (bytes_read < 0){
            cerr << "unable to read from file: " << errno << endl;
            return -1;
        }

        if (bytes_read == 0) {
            return 0;
        }

        while (ssize < bytes_read) {        
            ssize += write(*pipe_in_write, buffer_in + ssize, bytes_read - ssize);
        }

    }	

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
