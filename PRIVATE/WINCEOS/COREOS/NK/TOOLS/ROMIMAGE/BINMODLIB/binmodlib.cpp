//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/* 
  Wrapper for binmod\binmod.cpp to allow similar code base to 
  work with both files and memory buffers containing images   
 */

#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#include "cbinmod.h"

#include "rtutils.h"

#define FILES 2

#define TRACE_CALLER_NAME_DEFAULT "binmodlib"

class CBinModMemory:public CBinMod
{
private:
  DWORD base_ptr[FILES];
  DWORD current_ptr[FILES];
  DWORD size[FILES];
  DWORD max_size;
  DWORD hcount;

public:
  CBinModMemory(bool log2file = false):CBinMod(log2file)
  {
    memset(base_ptr, 0, sizeof(base_ptr));
    memset(current_ptr, 0, sizeof(current_ptr));
    memset(size, 0, sizeof(size));
    max_size = 0;
    hcount = 0;

  };
  ~CBinModMemory(){};

  bool Init(string _image, BYTE *bin_data, DWORD *bin_size, DWORD _max_size)
  {
    base_ptr[0] = (DWORD)bin_data;
    size[0] = *bin_size;
    current_ptr[0] = 0;
    max_size = _max_size;

    return CBinMod::Init(_image);
  }

  bool ReplaceFile(BYTE *bin_data, DWORD *bin_size, DWORD _max_size, BYTE *file_data, DWORD *file_size, const char *file_name)
  {
    base_ptr[1] = (DWORD)file_data;
    size[1] = *file_size;
    current_ptr[1] = 0;

    if(!Replace(file_name))
      return false;

//  *bin_data = (BYTE*)base_ptr[0];
    *bin_size = size[0];

    return true;
  }

  DWORD my_SetFilePointer(HANDLE handle, LONG dist, PLONG dist_high, DWORD method){
    switch(method){
      case FILE_BEGIN:
        current_ptr[(DWORD)handle] = dist;
  
        if(current_ptr[(DWORD)handle] > size[(DWORD)handle]){
          return INVALID_SET_FILE_POINTER;
        }
        
        return current_ptr[(DWORD)handle];
        break;
        
      case FILE_CURRENT:
        current_ptr[(DWORD)handle] += dist;
  
        if(current_ptr[(DWORD)handle] > size[(DWORD)handle]){
          return INVALID_SET_FILE_POINTER;
        }
        
        return current_ptr[(DWORD)handle];
        break;
  
      case FILE_END:
        current_ptr[(DWORD)handle] = size[(DWORD)handle] + dist;
  
        if(current_ptr[(DWORD)handle] > size[(DWORD)handle]){
          return INVALID_SET_FILE_POINTER;
        }
  
        return current_ptr[(DWORD)handle];
        break;
        
      default:
        break;
    }
  
    return INVALID_SET_FILE_POINTER;
  }
  
  BOOL my_ReadFile(HANDLE handle, LPVOID buffer, DWORD bytes, LPDWORD cb, LPOVERLAPPED){
    if(bytes > size[(DWORD)handle])
      return false;
    
    if(bytes + current_ptr[(DWORD)handle] <= size[(DWORD)handle]){
      memcpy(buffer, (void*)(base_ptr[(DWORD)handle] + current_ptr[(DWORD)handle]), bytes);
      current_ptr[(DWORD)handle] += bytes;
  
      *cb = bytes;
      return true;
    }
    
    return false;
  }
  
  BOOL my_WriteFile(HANDLE handle, LPVOID buffer, DWORD bytes, LPDWORD cb, LPOVERLAPPED){
    if(bytes + current_ptr[(DWORD)handle] > size[(DWORD)handle])
    {
      if((DWORD)handle == 0 && bytes + size[(DWORD)handle] <= max_size)
      {
        size[(DWORD)handle] = bytes + size[(DWORD)handle];
      }
      else
      {
        dprintf("Error: not enough space in buffer for insertion\n");
        return false;
      }
    }
    
    memmove((void*)(base_ptr[(DWORD)handle] + current_ptr[(DWORD)handle]), buffer, bytes);
    current_ptr[(DWORD)handle] += bytes;
  
    *cb = bytes;
    return true;
  }
  
  BOOL my_FlushFileBuffers(HANDLE hFile){
    return true;
  }
  
  HANDLE my_CreateFile(
    LPCTSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
  ){
    if(hcount >= FILES)
      hcount = 0;
    
    return HANDLE(hcount++);
  }
  
  BOOL my_CloseHandle(HANDLE hObject)
  {
    return false;
  }

  DWORD my_GetFileSize(HANDLE handle, LPDWORD lpFileSizeHigh)
  {
    return size[(DWORD)handle];
  }

  DWORD my_GetFileAttributes(LPCTSTR lpFileName)
  {
    return 1;
  };
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

bool ReplaceMemoryFile(BYTE *bin_data, DWORD *bin_size, DWORD _max_size, BYTE *file_data, DWORD *file_size, const char *file_name, bool log2file){
  bool ret = false;
  CBinModMemory *binmodmemory = new CBinModMemory(log2file);

  if(!binmodmemory)
  {
    goto exit;
  }

  if(!binmodmemory->Init("some bin file.bin", bin_data, bin_size, _max_size))
  {
    goto exit;
  }
  if(!binmodmemory->ReplaceFile(bin_data, bin_size, _max_size, file_data, file_size, file_name))
  {
    goto exit;
  }

  ret = true;

exit:
  delete binmodmemory;

  return ret;
}


