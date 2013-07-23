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


using std::cerr;
using std::endl;

void prii(int i){fprintf(stderr, "debug: %d\n", i);}
void pris(char*s){fprintf(stderr, "debug: %s\n", s);}


void uc_err(char*s){
  fprintf(stderr, "error: %s\n", s);
  exit(1);
}


int send_buf(UDTSOCKET client, char* buf, int size, int flags){

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

  return ss;

}

int run_client(thread_args *args)
{

  char *ip = args->ip; 
  char *port = args->port;
  int blast = args->blast;
  int blast_rate = args->blast_rate;
  int udt_buff = args->udt_buff;
  int udp_buff = args->udp_buff; // 67108864;
  int mss = args->mss;
    
  UDT::startup();

  struct addrinfo hints, *local, *peer;

  memset(&hints, 0, sizeof(struct addrinfo));

  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (0 != getaddrinfo(NULL, "9000", &hints, &local))
    {
      cerr << "incorrect network address.\n" << endl;
      return 1;
    }


  UDTSOCKET client = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);



  // UDT Options
  if (blast)
    UDT::setsockopt(client, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));

  UDT::setsockopt(client, 0, UDT_MSS, &mss, sizeof(int));
  UDT::setsockopt(client, 0, UDT_SNDBUF, &udt_buff, sizeof(int));
  UDT::setsockopt(client, 0, UDP_SNDBUF, &udp_buff, sizeof(int));


  freeaddrinfo(local);

  if (0 != getaddrinfo(ip, port, &hints, &peer))
    {
      cerr << "incorrect server/peer address. " << ip << ":" << port << endl;
      return 1;
    }

  // connect to the server, implict bind
  if (UDT::ERROR == UDT::connect(client, peer->ai_addr, peer->ai_addrlen))
    {
      cerr << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
      return 1;
    }

  pthread_t rcvthread;
  pthread_create(&rcvthread, NULL, recvdata, new UDTSOCKET(client));
  pthread_detach(rcvthread);

  freeaddrinfo(peer);

  if (blast) {
    CUDPBlast* cchandle = NULL;
    int temp;
    UDT::getsockopt(client, 0, UDT_CC, &cchandle, &temp);
    if (NULL != cchandle)
      cchandle->setRate(blast_rate);
  }

  size_t size;
  char *data;
  if (!(data=(char*)malloc(udt_buff*sizeof(char))))
    uc_err("Unable to allocate buffer");

  long total_sent = 0;
  // clock_t start, stop;

  while (1){

    // start = clock();
    size = read(STDIN_FILENO, data, udt_buff);

    total_sent += size;
    if (size < 0)
      uc_err(strerror(errno));

    if (size == 0){
      pris("No more to read");
      break;
    }
    
    if (size > 0){
      fprintf(stderr, "Sending buffer, sent: %.3f GB\n", total_sent*9.31323e-10);
      send_buf(client, data, size, 0);
    }
    
    // stop = clock();
    // double diff = ((double)stop - (double)start) * 1E-4;
    // if (diff)
    //   fprintf(stderr, "rate: %e\n",size/diff);   

  }

  UDT::close(client);
  
  free(data);

  UDT::cleanup();

  return 0;
}

