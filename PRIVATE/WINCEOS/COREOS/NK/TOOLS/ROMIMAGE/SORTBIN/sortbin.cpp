//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "compcat.h"

int main(int argc, char *argv[]){
  if(argc != 2){
USAGE:
    fprintf(stderr, "Usage: %s filename\n", argv[0]);
    return 1;
  }

  if(compact_bin(argv[1]))
    printf("Done\n");
  else
    goto USAGE;
  
  return 0;
}
