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
#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#include <pkfuncs.h>

#include "traverse.h"

DWORD hash_value = 0;

bool hash(DWORD addr, DWORD size){
  for(DWORD i = 0; i < size; i++)
    hash_value += *((BYTE*)addr + i);
  
  return true;
}

int _tmain(int argc, TCHAR *argv[]){
  ROMChain_t *prom_chain;
  BOOL        fPrevKMode;
  DWORD       rom_offset = 0;

  fPrevKMode = SetKMode(TRUE);
  prom_chain = (ROMChain_t*)KLibGetROMChain();
  
  while(prom_chain){
    if(!process_image(prom_chain->pTOC, rom_offset, hash)){
      _ftprintf(stderr, _T("processing image failed\n"));
      exit(1);
    }
    
    _tprintf(_T("hash_value = %08x\n"), hash_value);
    
    prom_chain = prom_chain->pNext;
  }

  SetKMode(fPrevKMode);

  return 0;
}

