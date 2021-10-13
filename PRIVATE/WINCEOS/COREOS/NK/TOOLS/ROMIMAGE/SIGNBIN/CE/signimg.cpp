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

