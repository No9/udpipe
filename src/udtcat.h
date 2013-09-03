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
#include <stdlib.h>
#include <stdio.h>

#include "cc.h"
#include "udtcat_threads.h"
#include "mt_aes.h"

#define N_THREADS 2
/* #define BUFF_SIZE 67108864 */
#define BUFF_SIZE 67108864
/* #define BUFF_SIZE 10 */

typedef struct send_buf_args{
  UDTSOCKET client; 
  char* buf;
  int size;
  int flags;
  int idle;
} send_buf_args;


typedef struct thread_args{
  char *ip;
  char *port;
  int blast;
  int blast_rate;
  size_t udt_buff;
  size_t udp_buff;
  int mss;
  
} thread_args;


void* send_buf_threaded(void*_args);
