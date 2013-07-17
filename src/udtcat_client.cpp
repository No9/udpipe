/*****************************************************************************
Copyright 2013 Laboratory for Advanced Computing at the University of Chicago

This file is part of .

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
#include "udtcat_threads.h"
#include "udtcat.h"


using std::cerr;
using std::endl;

int run_client(thread_args *args)
{

  char *ip = args->ip; 
  char *port = args->port;
  int blast = args->blast;
  int blast_rate = args->blast_rate;
  int udt_buff = args->udt_buff;
  int udp_buff = args->udp_buff; // 67108864;
  int mss = args->mss;
    
  //  argv[0]    argv[1]   argv[2]       argv[3]        argv[4]       argv[5]     argv[6]
  // appclient server_ip server_port use_blast(0 or 1) udt_sendbuff udp_sendbuff   mss

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

  size_t buf_size = udt_buff;
  int size;
  char* data = NULL;

  while ((size = getline(&data, &buf_size, stdin)) > 0) {
    int ssize = 0;
    int ss;
    while (ssize < size) {
      if (UDT::ERROR == (ss = UDT::send(client, data + ssize, size - ssize, 0))) {
	cerr << "send:" << UDT::getlasterror().getErrorMessage() << endl;
	break;
      }

      ssize += ss;
    }

    if (ssize < size)
      break;
  }

  UDT::close(client);

  free(data);

  UDT::cleanup();

  return 0;
}

