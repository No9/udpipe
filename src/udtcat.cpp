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

#include "udtcat.h"
#include "udtcat_client.h"
#include "udtcat_server.h"


void usage(){
  fprintf(stderr, "usage: udtcat [udtcat options] [server ip] port\n");
  exit(1);
}

void initialize_thread_args(thread_args *args){
  
  args->ip = "127.0.0.1";
  args->port = "9000";
  args->blast = 0;
  args->blast_rate = 0;
  args->udt_buff = 67108864;
  args->udp_buff = 67108864;
  args->mss = 1300;
  
}

void err_msg(char* err){
  fprintf(stderr, "error: %s\n", err);
  usage();
}


int main(int argc, char *argv[]){

  
  int opt;
  int operation = CLIENT;
  char* port = NULL;
  char* ip = NULL;

  while ((opt = getopt (argc, argv, "l")) != -1){
    switch (opt){
    case 'l':
      operation = SERVER;
      break;
    default:
      fprintf(stderr, "Unknown command line arg. -h for help.\n");
      usage();
      exit(1);
    }
  }
  
  thread_args args;
  initialize_thread_args(&args);

  if (operation == CLIENT){
    if (optind < argc){
      if (strcmp(argv[optind], "localhost"))
	args.ip = strdup(argv[optind++]);

    } else {
      err_msg("Please specify server ip.");
      exit(1);
    }
  }

  if (optind < argc){
    args.port = strdup(argv[optind++]);
  } else {
    err_msg("Please specify port num.");
    exit(1);
  }

  // if (argc-optind){
  //   fprintf(stderr, "Unknown argument: %s\n", argv[optind]);
  //   usage();
  // }


  if (operation == SERVER){
    run_server(&args);

  } else if (operation == CLIENT){
    run_client(&args);


  } else {
    err_msg("Operation type not known");
    
  }

  
}
