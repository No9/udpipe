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

#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <iostream>
#include <udt.h>


#include "udtcat.h"
#include "udtcat_threads.h"

using std::cerr;
using std::endl;
using std::string;

void* recvdata(void*);
void* senddata(void*);

int buffer_size;

int run_server(thread_args *args){


  // if (argc != 6) {

                         // 0          1           2                 3
  //   cerr << "usage: appserver server_port use_blast(0 or 1) udt_recvbuff "
  //     "udp_recvbuff mss" << endl;
  //   return 1;
  // }

  char *ip = args->ip; 
  char *port = args->port;
  int blast = args->blast;
  int blast_rate = args->blast_rate;
  int udt_buff = args->udt_buff;
  int udp_buff = args->udp_buff; // 67108864;
  int mss = args->mss;

  UDT::startup();

  addrinfo hints;
  addrinfo* res;

  memset(&hints, 0, sizeof(struct addrinfo));

  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  string service(port);


  if (0 != getaddrinfo(NULL, service.c_str(), &hints, &res)) {
    cerr << "illegal port number or port is busy.\n" << endl;
    return 1;
  }

  UDTSOCKET serv = UDT::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

  buffer_size = udt_buff;

  // UDT Options
  if (blast)
    UDT::setsockopt(serv, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));

  UDT::setsockopt(serv, 0, UDT_MSS, &mss, sizeof(int));
  UDT::setsockopt(serv, 0, UDT_RCVBUF, &udt_buff, sizeof(int));
  UDT::setsockopt(serv, 0, UDP_RCVBUF, &udp_buff, sizeof(int));


  if (UDT::ERROR == UDT::bind(serv, res->ai_addr, res->ai_addrlen)) {
    cerr << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
    return 1;
  }

  freeaddrinfo(res);

  cerr << "server is ready at port: " << service << endl;

  if (UDT::ERROR == UDT::listen(serv, 10))
    {
      cerr << "listen: " << UDT::getlasterror().getErrorMessage() << endl;
      return 1;
    }

  sockaddr_storage clientaddr;
  int addrlen = sizeof(clientaddr);

  UDTSOCKET recver;

  while (true) {
    if (UDT::INVALID_SOCK == (recver = UDT::accept(serv,
						   (sockaddr*)&clientaddr, &addrlen))) {

      cerr << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
      return 1;
    }

    char clienthost[NI_MAXHOST];
    char clientservice[NI_MAXSERV];
    getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost,
		sizeof(clienthost), clientservice, sizeof(clientservice),
		NI_NUMERICHOST|NI_NUMERICSERV);

    cerr << "new connection: " << clienthost << ":" << clientservice << endl;

    pthread_t rcvthread;
    pthread_t sendthread;
    pthread_create(&rcvthread, NULL, recvdata, new UDTSOCKET(recver));
    pthread_create(&sendthread, NULL, senddata, new UDTSOCKET(recver));
    pthread_detach(rcvthread);
    pthread_detach(sendthread);
  }

  UDT::close(serv);

  UDT::cleanup();

  return 0;
}

