#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#include "binutils.h"
#include "traverse.h"

DWORD hash_value = 0;

HANDLE hfile;

bool hash(DWORD addr, DWORD size){
  for(DWORD i = 0; i < size; i++)
    hash_value += *((BYTE*)addr + i);

  DWORD cb;

  int ret = WriteFile(hfile, (void*)addr, size, &cb, NULL);

  if(!ret)
    _ftprintf(stderr, _T("writefile failed: %d\n"), GetLastError());

  return ret ? true : false;
}

TCHAR *image = _T("nk.bin");
TCHAR *file_name = _T("signature.out");

bool ParseArgs(int argc, TCHAR *argv[]){
  int i = 0;
  while(++i < argc){
    if(_tcsicmp(argv[i], "-o") == 0){
      if(i + 1 == argc)
        return false;

      file_name = argv[++i];
    }
    else if(_tcsicmp(argv[i], "-i") == 0){
      if(i + 1 == argc)
        return false;
 
      image = argv[++i];
    }
    else
      return false;
  }

  return true;
}

int _tmain(int argc, TCHAR *argv[]){
  printf("Signbin V1.1 built " __DATE__ " " __TIME__ "\n\n");
  
  if(!ParseArgs(argc, argv)){
    _tprintf(_T("Usage: %s [options]\n"), argv[0]);
    _tprintf(_T("   -o Outfile\n"));
    _tprintf(_T("   -i input image\n"), argv[0]);
    
    return 0;
  }
  
  hfile = CreateFile(file_name, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  if(hfile == INVALID_HANDLE_VALUE){
    _tprintf(_T("failed opening %s: %d\n"), file_name, GetLastError());
    return false;
  }

  if(!process_image(image, (SIGNING_CALLBACK)hash)){
    _ftprintf(stderr, _T("processing image failed\n"));
    exit(1);
  }

  _tprintf(_T("hash_value = %08x\n"), hash_value);

  CloseHandle(hfile);

  return 0;
}
  
