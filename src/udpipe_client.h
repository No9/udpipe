/*****************************************************************************
Copyright 2013 Laboratory for Advanced Computing at the University of Chicago

This file is part of udr/udt

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
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <getopt.h>


class udpipeClient
{
private:
    
    char *ip;
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


public:
    
    /* Constructors */
    udpipeClient(char *ip, char *port);
    udpipeClient(char *ip, char *port, int verbose);
    udpipeClient(char *ip, char *port, int verbose, int blast_rate, int blast, int udt_buff, int udp_buff, int mss);
         
    int startUDT();
    int setup();
    int connect();
    int start();
    int start_send_thread();
    int start_receive_thread();
    int set_timeout(int _timeout);
    
};

int run_client(thread_args *args);
