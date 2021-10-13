#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <assert.h>

#include "binutils.h"

struct RECORD_DATA{
  DWORD start_address;
  DWORD length;
  DWORD chksum;
  DWORD file_ptr;
};

#define MAX_BUFFER 1024

static RECORD_DATA records[MAX_BUFFER];
static DWORD record_count = 0;

static ROMHDR *pTOC;
static HANDLE hfile;
static DWORD rom_offset;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool read_image(void *pDestBuffer, DWORD dwAddress, DWORD dwLength, bool use_rom_offset){
  assert(hfile != INVALID_HANDLE_VALUE);
  
  DWORD dwDiff;
  DWORD dwRead;

  if(use_rom_offset)
    dwAddress += rom_offset;

  for (DWORD i = 0; i < record_count; i++){
    // is the data contained within the bounds of this record?
    if(records[i].start_address <= dwAddress &&
       records[i].start_address + records[i].length >= (dwAddress + dwLength)){
            
      // Offset into the record.
      dwDiff = dwAddress - records[i].start_address;
      SetFilePointer(hfile, records[i].file_ptr + dwDiff, NULL, FILE_BEGIN);

      if(!ReadFile(hfile, pDestBuffer, dwLength, &dwRead, NULL)){
        _tprintf(_T("ReadFile failed: %d"), GetLastError());
        return false;
      }

      return true;
    }
  }

  _ftprintf(stderr, _T("failed to read data of size %d at %08x\n"), dwLength, dwAddress);
  
  return false;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD get_rom_offset(DWORD image_start, DWORD image_length, ROMHDR *ptoc){
  bool ret = true;
  DWORD temp_offset;
  ROMHDR romhdr;

  for(DWORD i = 0; i < record_count; i++){
    // skip records of the wrong size
    if (records[i].length != sizeof(ROMHDR))
      continue;
    
    // If this _IS_ the TOC record, compute the ROM Offset.
    temp_offset = (DWORD) records[i].start_address - (DWORD)ptoc;

    _tprintf(_T("Checking record #%d for potential TOC (ROMOFFSET = 0x%08X)\n"), i, temp_offset);

    // Read out the record to verify. (unadjusted)
    if(read_image((PBYTE)&romhdr, records[i].start_address, sizeof(ROMHDR), false))
      if((romhdr.physfirst == image_start - temp_offset) &&
         (romhdr.physlast  == image_start - temp_offset + image_length) &&
         (DWORD)(HIWORD(romhdr.dllfirst) << 16) <= romhdr.dlllast && 
         (DWORD)(LOWORD(romhdr.dllfirst) << 16) <= romhdr.dlllast){
        rom_offset = temp_offset;
        goto EXIT;
      }
  }

  // reverse of normal logic, we only get here if we DIDN'T find it
  ret = false;

EXIT:
  _tprintf(_T("ROM offset = 0x%08X\n"), rom_offset);

  return ret;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool init(const TCHAR *file_name){
  bool ret = false;
  
  hfile = CreateFile(file_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if(hfile == INVALID_HANDLE_VALUE){
    _tprintf(_T("failed opening %s: %d\n"), file_name, GetLastError());
    return false;
  }

  char bin_header[] = {'B', '0', '0', '0', 'F', 'F', '\x0a'};
  char buffer[sizeof(bin_header)];
  DWORD image_start;
  DWORD image_length;
  DWORD image_jump;
  DWORD cb;

  // read headers

  if(!ReadFile(hfile, buffer, sizeof(bin_header), &cb, NULL)){
    _tprintf(_T("failed reading bin header: %d\n"), GetLastError());
    goto EXIT;
  }

  if(memcmp(buffer, bin_header, sizeof(bin_header)) != 0){
    _tprintf(_T("invalid bin header\n"));
    goto EXIT;
  }

  if(!ReadFile(hfile, &image_start, sizeof(image_start), &cb, NULL)){
    _tprintf(_T("failed reading image start: %d\n"), GetLastError());
    goto EXIT;
  }

  if(!ReadFile(hfile, &image_length, sizeof(image_length), &cb, NULL)){
    _tprintf(_T("failed reading image length: %d\n"), GetLastError());
    goto EXIT;
  }

  // read records
  for(;;){
    if(!ReadFile(hfile, &records[record_count], 3*sizeof(DWORD), &cb, NULL))
      break;

    records[record_count].file_ptr = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);

//    _tprintf(_T("record [%3d] : start = 0x%08X, length = 0x%08X, chksum = 0x%08X\n"), record_count, records[record_count].start_address, records[record_count].length, records[record_count].chksum);

    if(records[record_count].start_address == 0 && records[record_count].chksum== 0){
      image_jump = records[record_count].length;
      _tprintf(_T("start address = 0x%08X\n"), image_jump);
      break;
    }

    SetFilePointer(hfile, records[record_count].length, NULL, FILE_CURRENT);

    record_count++;
  }

  // find toc
  if(!read_image((PBYTE)buffer, image_start + 0x40, 8, false)) {
    _tprintf(_T("Couldn't find pTOC @ image start (0x%08X) + 0x40\n"), image_start);
    goto EXIT;
  }
  
  pTOC = *(ROMHDR**)(buffer + 4);
  _tprintf(_T("found pTOC  = 0x%08X\n"), pTOC);

  if(!get_rom_offset(image_start, image_length, pTOC)){
    _tprintf(_T("Failed to find the rom offset\n"), image_start);
    goto EXIT;
  }

  ret = true;

EXIT:
  if(!ret && hfile != INVALID_HANDLE_VALUE)
    CloseHandle(hfile);
  
  return ret;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool process_image(const TCHAR *file_name, SIGNING_CALLBACK call_back){
  bool ret = false;

  if(!init(file_name)){
    _ftprintf(stderr, _T("init failed\n"));
    goto EXIT;
  }

  ret = process_image(pTOC, rom_offset, call_back);

EXIT:
  if(hfile != INVALID_HANDLE_VALUE)
    CloseHandle(hfile);

  return ret;
}

