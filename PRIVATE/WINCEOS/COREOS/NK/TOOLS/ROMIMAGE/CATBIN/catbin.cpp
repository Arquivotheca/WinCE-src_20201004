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
  if(getenv("d_break"))
    DebugBreak();

  if(argc != 1 && argc != 2){
USAGE:
    fprintf(stderr, "\nUsage: %s path\n", argv[0]);
    fprintf(stderr, "  All bins (input and out) must be in the same directory along with a file called chain.lst\n"
                    "  chain.lst must be in current directory or a fully qualified path to it must be supplied.\n"
                    "  The proper format for a chain.lst file is as follows:\n"
                    "     +nk.bin\n"
                    "     other.bin\n"
                    "     ...\n\n"
                    "  where the bin file who's jump record you wish to use (usually nk.bin) has a '+' preceding the name.\n"
                    "  The combined output file is called xip.bin\n\n"
                    "Example:\n"
                    "  catbin.exe c:\\wince400\\release\\\n");

      
    return 1;
  }

  string path = argc == 2 ? argv[1] : "";

  if(path.empty())
    path = ".";
  
  if(path.rfind("\\") != string::npos)
    path = path.substr(0, path.rfind("\\"));

  path += "\\";

  if(cat_bins(path) && compact_bin(path))
    printf("Done\n");
  else
    goto USAGE;
  
  return 0;
}
