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
#include "sre.h"

#include "data.h"
#include "memory.h"

struct RecHdr{
  DWORD start;
  DWORD length;
  DWORD chksum;
};

#define SRE_DATA_SIZE 0x28

//------------------------------------------------------------------------------
void char2ascii(char *ptr, char value){
  sprintf(ptr, "%02X", LOBYTE(value));
}

//------------------------------------------------------------------------------
void word2ascii(char *ptr, WORD value){
  sprintf(ptr, "%02X%02X", LOBYTE(value), HIBYTE(value));
}

//------------------------------------------------------------------------------
void dword2ascii(char *ptr, DWORD value){
  sprintf(ptr, 
          "%02X%02X%02X%02X", 
          LOBYTE(value), HIBYTE(value), LOBYTE(HIWORD(value)), HIBYTE(HIWORD(value)));
}

//------------------------------------------------------------------------------
unsigned char chksum(const char *line){
  unsigned char sum = 0;
  
  // pre increment to skip the S# which isn't added in the chksum
  line += 2;

  // sum all the ascii representations of the bytes
  while(*line){
    unsigned char ch = *line++ - '0';
    if(ch > 9) ch -= 7;
    sum += ch << 4;
    
    ch = *line++ - '0';
    if(ch > 9) ch -= 7;
    sum += ch;
  }

  return ~sum;
}

//------------------------------------------------------------------------------
bool write_sre_line(FILE *out_file_list[], DWORD rom_count, DWORD rom_offset, const DWORD *buffer, unsigned char size){
  char data[4][500] = {0};  

  assert(size < sizeof(data[0]) / 8);
  
  for(unsigned i = 0; i < rom_count; i++)
    sprintf(data[i], "S3%02X%08X", size * 4 / rom_count + 5, rom_offset); // +5 for address and checksum

  switch(rom_count){
    case 4:{
      char *ptr[4];
      unsigned i;

      rom_offset >>= rom_count & 0xfe; 

      for(i = 0; i < rom_count; i++)
        ptr[i] = data[i] + strlen(data[i]);
  
      for(i = 0; i < size; i++){ 
        for(unsigned j = 0; j < rom_count; j++){
          char2ascii(ptr[j], (CHAR)(buffer[i] >> (j * sizeof(DWORD) / rom_count)));
          ptr[i] += 4;
        }
      }

      for(i = 0; i < rom_count; i++){
        sprintf(ptr[i], "%02X", chksum(data[i]));

        fprintf(out_file_list[i], "%s\n", data[i]);
      }
    }
    break;
    
    case 2:{
      char *ptr[2];
      unsigned i;

      rom_offset >>= rom_count & 0xfe; 
      
      for(i = 0; i < rom_count; i++)
        ptr[i] = data[i] + strlen(data[i]);
  
      for(i = 0; i < size; i++){ 
        for(unsigned j = 0; j < rom_count; j++){
          word2ascii(ptr[j], (CHAR)(buffer[i] >> (j * sizeof(DWORD) / rom_count)));
          ptr[i] += 4;
        }
      }

      for(i = 0; i < rom_count; i++){
        sprintf(ptr[i], "%02X", chksum(data[i]));

        fprintf(out_file_list[i], "%s\n", data[i]);
      }
    }
    break;
      
    case 1:{
      char *ptr = data[0] + strlen(data[0]);
      
      rom_offset >>= rom_count & 0xfe; 
      
      for(int i = 0; i < size; i++){ 
        dword2ascii(ptr, buffer[i]);
        ptr += 8;  // in ascii a dword is 8 chars long
      }
  
      sprintf(ptr, "%02X", chksum(data[0]));

      fprintf(out_file_list[0], "%s\n", data[0]);
    }
    break;
  }

  return true;
}

//------------------------------------------------------------------------------
bool write_sre(wstring input_file, string memory_type, DWORD rom_offset, DWORD virtual_base, bool launch){
  bool ret = false;
  
  cout << "Writing sre file...\n";
  
  DWORD start;
  DWORD length;
  Data buffer;

  RecHdr rechdr = {0};
  DWORD readreq = 0;

  FILE *in_file = _wfopen(input_file.c_str(), L"rb");
  if(!in_file){
    wcerr << L"Error: Could not open '" << input_file << L"' for reading\n";
    return false;
  }

  fread((char *)buffer.user_fill(7), 1, 7, in_file);
  if(ferror(in_file) || feof(in_file)){
    wcerr << L"Error: Failed reading file " << input_file << endl;
    goto exit;
  }

  if(memcmp("B000FF\n", buffer.ptr(), 7) != 0){
    cerr << "Error: Missing initial signature (BOOOFF\x0A). Not a BIN file\n";
    goto exit;
  }

  fread((char *)&start, 1, sizeof(start), in_file);
  if(ferror(in_file) || feof(in_file)){
    wcerr << L"Error: Failed reading file " << input_file << endl;
    goto exit;
  }

  fread((char *)&length, 1, sizeof(length), in_file);
  if(ferror(in_file) || feof(in_file)){
    wcerr << L"Error: Failed reading file " << input_file << endl;
    goto exit;
  }

  DWORD rom_count = 0;
  if(memory_type == RAMIMAGE_TYPE || memory_type == NANDIMAGE_TYPE){
    memory_type.erase();
    virtual_base = 0;
    rom_count = 1;
  }
  if(memory_type == ROMx8_TYPE) rom_count = 1;
  if(memory_type == ROM16_TYPE) rom_count = 2;
  if(memory_type == ROM8_TYPE) rom_count = 4;
  if(!rom_count){
    fprintf(stderr, "\nError: Invalid rom type '%s'\n", memory_type.c_str());
    goto exit;
  }

  FILE **out_file_list = new FILE*[rom_count];
  if(!out_file_list) {
    cerr << "Error: Could not allocate FILE  stuctures'\n";
    goto exit;
  }

  for(unsigned i = 0; i < rom_count; i++){
    wchar_t temp[MAX_PATH];

    if(memory_type.empty())
      wsprintfW(temp, L"%s.sre", input_file.substr(0, input_file.rfind(L".")).c_str());
    else
      wsprintfW(temp, L"%s%d.sre", input_file.substr(0, input_file.rfind(L".")).c_str(), i);

    out_file_list[i] = _wfopen(temp, L"wt");
    if(!out_file_list[i]){
      wcerr << L"Error: failed opening '" << temp << L"'\n";
      for(unsigned j = 0; j < i; j++)
        fclose(out_file_list[j]);
      goto exit;
    }
  }

  // sre header
  DWORD op_offset = 0;

  switch(rom_count){
    case 4: op_offset = 0x1c000; break;
    case 2: fprintf(out_file_list[1], "S0030000FC\n"); /* fall through */
    case 1: fprintf(out_file_list[0], "S0030000FC\n"); break;
  }

  if(!memory_type.empty()){
    DWORD op_code[2] = {0};
    
    op_code[0] = ((virtual_base & 0x0fffffff) >> 2) | 0x08000000;
    
    write_sre_line(out_file_list, rom_count, op_offset, op_code, 2);
  }

  for(;;){
    memset(&rechdr, 0, sizeof(rechdr));
    
    fread((char *)&rechdr, 1, 3 * sizeof(DWORD), in_file);
    if(ferror(in_file) || feof(in_file)){
      wcerr << L"Error: Failed reading file " << input_file << endl;
      goto exit;
    }

    if(!rechdr.start && !rechdr.chksum)
      break;

    for(int i = rechdr.length; i > 0; ){
      readreq = (i > SRE_DATA_SIZE ? SRE_DATA_SIZE : i);
      i -= readreq;

      fread((char *)buffer.user_fill(readreq), 1, readreq, in_file);
      if(ferror(in_file) || feof(in_file)){
        wcerr << L"Error: Failed reading file " << input_file << endl;
        goto exit;
      }

      write_sre_line(out_file_list, rom_count, rechdr.start - virtual_base, (DWORD *)buffer.ptr(), (UCHAR)(buffer.size() / 4));

      rechdr.start += readreq;
    }
  }

  // sre trailers
  switch(rom_count){
    case 4:
    case 2:{
      for(unsigned i = 0; i < rom_count; i++)
        fprintf(out_file_list[i], "S9030000FC\n"); 
      break;
    }
      
    case 1:{
      if(launch) fprintf(out_file_list[0], "SG050000000000\n"); 

      DWORD start_of_execution = rom_offset + rechdr.length; // the length of the last record is really the starting ip addr
    
      char szAscii[100];
      sprintf(szAscii, "S705%08X%", start_of_execution);
      sprintf(szAscii + strlen(szAscii), "%02X\n", chksum(szAscii));
      fprintf(out_file_list[0], szAscii);
    }
    break;
  }

  for(unsigned j = 0; j < i; j++)
    fclose(out_file_list[j]);
  
  ret = true;
  
exit:
  if(in_file) fclose(in_file);
  
  return ret;
}

