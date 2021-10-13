//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>
#include <internal.h>

void *realloc (void *OldPtr, size_t NewSize) {
  
  HLOCAL hLocal;

  if (!OldPtr)
    return malloc(NewSize);
  
  if (!NewSize) {
    LocalFree(OldPtr);
    return(0);
  }

  for(;;) {

    hLocal =  LocalReAlloc( OldPtr, NewSize, 0x2 );

    if(!hLocal && _newmode == 1) {

      if(!_callnewh(NewSize))
        return NULL;

    }
    else
      return hLocal;

  }

}

