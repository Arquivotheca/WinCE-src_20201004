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
//------------------------------------------------------------------------------
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//  
//------------------------------------------------------------------------------

#include <stdio.h>
#include <tchar.h>

#include "cbinmod.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#define FUNCTION_REPLACE 1
#define FUNCTION_EXTRACT 2

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
  bool ret = 1;

  printf("BinMod V1.0 built " __DATE__ " " __TIME__ "\n\n");
    
	if(getenv("d_break"))
	  DebugBreak();

  TCHAR *tool_name = argv[0];
	TCHAR *image_name = "nk.bin";
  TCHAR *file_name = NULL;
  DWORD function = 0;

  CBinMod binmod;
#if 0
{
  int i;
  
  for(i = 0; i < argc; i++)
    printf("%d) %s\n", i, argv[i]);
}

#endif

  if(argc < 2){
    goto usage;
  }

  if(argc >=3 && _tcsicmp(argv[1], "-i") == 0){
    image_name = argv[2];
    argc -= 2;
    argv = &argv[2];
  }

  if(argc >= 3 && _tcsicmp(argv[1], "-r") == 0){
    file_name = argv[2];
    argc -= 2;
    argv = &argv[2];

    function = FUNCTION_REPLACE;
  }
  else if(argc >= 3 && _tcsicmp(argv[1], "-e") == 0){
    file_name = argv[2];
    argc -= 2;
    argv = &argv[2];

    function = FUNCTION_EXTRACT;
  }

  if(!file_name || !image_name || !function){
    goto usage;
  }

  binmod.Init(image_name);

  if(function == FUNCTION_REPLACE){
    if(!binmod.Replace(file_name)){
      printf("Error: Replacement failed!\n");
      goto exit;
    }
    printf("Done!\n");
  }
  else if(function == FUNCTION_EXTRACT){
    if(!binmod.Extract(file_name)){
      printf( "Error: Extraction failed!\n");
      goto exit;
    }
    printf("Done!\n");
  }
  else{
    printf("Well that wasn't very productive, was I supposed to be doing something?\n");
    goto exit;
  }

  ret = 0;
  goto exit;

usage:
	printf("\nUsage: %s [-i imagename] -r \"replacement filename.ext\"\n"
         "       %s [-i imagename] -e \"extraction filename.ext\"\n"
         "    imagename       - name of .bin file to stamp (defaults to nk.bin)\n"
         "    filename        - file for replacement/extraction\n",
         tool_name,
         tool_name);
  goto exit;

exit:
  return ret;
}
