//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
bool write_sre_line(ostream out_file_list[], DWORD rom_count, DWORD rom_offset, const DWORD *buffer, unsigned char size){
  char data[4][500] = {0};  

  assert(size < sizeof(data[0]) / 8);
  
  for(int i = 0; i < rom_count; i++)
    sprintf(data[i], "S3%02X%08X", size * 4 / rom_count + 5, rom_offset); // +5 for address and checksum

  switch(rom_count){
    case 4:{
      char *ptr[4];
      int i;

      rom_offset >>= rom_count & 0xfe; 

      for(i = 0; i < rom_count; i++)
        ptr[i] = data[i] + strlen(data[i]);
  
      for(i = 0; i < size; i++){ 
        for(int j = 0; j < rom_count; j++){
          char2ascii(ptr[j], buffer[i] >> (j * sizeof(DWORD) / rom_count));
          ptr[i] += 4;
        }
      }

      for(i = 0; i < rom_count; i++){
        sprintf(ptr[i], "%02X", chksum(data[i]));

        out_file_list[i] << data[i] << endl;
      }
    }
    break;
    
    case 2:{
      char *ptr[2];
      int i;

      rom_offset >>= rom_count & 0xfe; 
      
      for(i = 0; i < rom_count; i++)
        ptr[i] = data[i] + strlen(data[i]);
  
      for(i = 0; i < size; i++){ 
        for(int j = 0; j < rom_count; j++){
          word2ascii(ptr[j], buffer[i] >> (j * sizeof(DWORD) / rom_count));
          ptr[i] += 4;
        }
      }

      for(i = 0; i < rom_count; i++){
        sprintf(ptr[i], "%02X", chksum(data[i]));

        out_file_list[i] << data[i] << endl;
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

      out_file_list[0] << data[0] << endl;
    }
    break;
  }

  return true;
}

//------------------------------------------------------------------------------
bool write_sre(string input_file, string memory_type, DWORD rom_offset, DWORD virtual_base, bool launch){
  cout << "Writing sre file...\n";
  
  DWORD start;
  DWORD length;
  Data buffer;

  ifstream in_file(input_file.c_str(), ios::in | ios::binary);
  if(in_file.bad()){
    cerr << "Error: Could not open '" << input_file << "' for reading\n";
    return false;
  }

  in_file.read((char *)buffer.user_fill(7), 7);
  if(in_file.fail()){
    cerr << "Error: Failed reading file " << input_file << endl;
    return false;
  }

  if(memcmp("B000FF\n", buffer.ptr(), 7) != 0){
    cerr << "Error: Missing initial signature (BOOOFF\x0A). Not a BIN file\n";
    return false;
  }

  in_file.read((char *)&start, sizeof(start));
  if(in_file.fail()){
    cerr << "Error: Failed reading file " << input_file << endl;
    return false;
  }

  in_file.read((char *)&length, sizeof(length));
  if(in_file.fail()){
    cerr << "Error: Failed reading file " << input_file << endl;
    return false;
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
    return false;
  }

  ofstream *out_file_list = new ofstream[rom_count];
  if(!out_file_list) {
    cerr << "Error: Could not allocate ofstream stuctures'\n";
    return false;
  }

  for(int i = 0; i < rom_count; i++){
    char temp[MAX_PATH];

    if(memory_type.empty())
      sprintf(temp, "%s.sre", input_file.substr(0, input_file.rfind(".")).c_str());
    else
      sprintf(temp, "%s%d.sre", input_file.substr(0, input_file.rfind(".")).c_str(), i);

    out_file_list[i].open(temp, ios::trunc | ios::binary);
    if(out_file_list[i].bad()){
      cerr << "Error: failed opening '" << temp << "'\n";
      delete[] out_file_list;
      return false;
    }
  }

  // sre header
  DWORD op_offset = 0;

  switch(rom_count){
    case 4: op_offset = 0x1c000; break;
    case 2: out_file_list[1] << "S0030000FC\n"; /* fall through */
    case 1: out_file_list[0] << "S0030000FC\n"; break;
  }

  if(!memory_type.empty()){
    DWORD op_code[2] = {0};
    
    op_code[0] = ((virtual_base & 0x0fffffff) >> 2) | 0x08000000;
    
    write_sre_line(out_file_list, rom_count, op_offset, op_code, 2);
  }

  RecHdr rechdr = {0};
  DWORD readreq = 0;

  for(;;){
    memset(&rechdr, 0, sizeof(rechdr));
    
    in_file.read((char *)&rechdr, 3 * sizeof(DWORD));
    if(in_file.fail()){
      cerr << "Error: Failed reading file " << input_file << endl;
      return false;
    }

    if(!rechdr.start && !rechdr.chksum)
      break;

    for(int i = rechdr.length; i > 0; ){
      readreq = (i > SRE_DATA_SIZE ? SRE_DATA_SIZE : i);
      i -= readreq;

      in_file.read((char *)buffer.user_fill(readreq), readreq);
      if(in_file.fail()){
        cerr << "Error: Failed reading file " << input_file << endl;
        return false;
      }

      write_sre_line(out_file_list, rom_count, rechdr.start - virtual_base, (DWORD *)buffer.ptr(), buffer.size() / 4);

      rechdr.start += readreq;
    }
  }

  // sre trailers
  switch(rom_count){
    case 4:
    case 2:{
      for(int i = 0; i < rom_count; i++)
        out_file_list[i] << "S9030000FC\n"; 
      break;
    }
      
    case 1:{
      if(launch) out_file_list[0] << "SG050000000000\n"; 

      DWORD start_of_execution = rom_offset + rechdr.length; // the length of the last record is really the starting ip addr
    
      char szAscii[100];
      sprintf(szAscii, "S705%08X%", start_of_execution);
      sprintf(szAscii + strlen(szAscii), "%02X\n", chksum(szAscii));
      out_file_list[0] << szAscii;
    }
    break;
  }

  delete[] out_file_list;
  
  return true;
}

