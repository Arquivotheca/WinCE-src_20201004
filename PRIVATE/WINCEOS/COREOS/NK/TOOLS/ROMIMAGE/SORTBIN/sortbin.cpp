//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "compcat.h"
#include "..\mystring\mystring.h"

int main(int argc, char *argv[]){
  if(argc != 2){
USAGE:
    fprintf(stderr, "Usage: %s filename\n", argv[0]);
    return 1;
  }

  if(compact_bin(tounicode(argv[1])))
    printf("Done\n");
  else
    goto USAGE;
  
  return 0;
}
