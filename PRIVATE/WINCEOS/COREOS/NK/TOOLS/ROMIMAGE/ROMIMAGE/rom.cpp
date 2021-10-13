//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "rom.h"

#include "data.h"

struct RecHdr{
  DWORD start;
  DWORD length;
  DWORD chksum;
};

bool write_rom(string input_file, RomInfo rom_info){
  cout << "Writing rom file...\n";
  
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
    cerr << "Error: Failed reading bin signature in " << input_file << endl;
    return false;
  }

  if(memcmp("B000FF\n", buffer.ptr(), 7) != 0){
    cerr << "Error: Missing initial signature (BOOOFF\x0A). Not a BIN file\n";
    return false;
  }

  in_file.read((char *)&start, sizeof(start));
  if(in_file.fail()){
    cerr << "Error: Failed reading image start in " << input_file << endl;
    return false;
  }

  start -= rom_info.offset;

  in_file.read((char *)&length, sizeof(length));
  if(in_file.fail()){
    cerr << "Error: Failed reading image length in " << input_file << endl;
    return false;
  }

  if(rom_info.start != start)
    printf("First %u bytes of rom empty\n", start - rom_info.start);

  DWORD rom_count = 0;
  RecHdr rechdr = {0};

  rom_count = (start + length - rom_info.start) / rom_info.size;
  if((start + length - rom_info.start) % rom_info.size)
    rom_count++;

  Data rom(rom_count * rom_info.size);
  rom.allow_shrink(false);
  for(;;){
    memset(&rechdr, 0, sizeof(rechdr));
    
    in_file.read((char *)&rechdr, 3 * sizeof(DWORD));
    if(in_file.fail()){
      cerr << "Error: Failed reading record header in " << input_file << endl;
      return false;
    }

    if(!rechdr.start && !rechdr.chksum)
      break;

    printf("Start %08x Len %08x\n", rechdr.start, rechdr.length);

    rechdr.start -= rom_info.offset + rom_info.start;

    if(rechdr.start >= rom_info.size * rom_count){
      fprintf(stderr, "Warning: Record starting at %08x is outside of rom range, skipping...\n", rechdr.start);
      in_file.seekg(rechdr.length, ios::cur);
    }
    else{
      Data temp;

      in_file.read((char *)temp.user_fill(rechdr.length), rechdr.length);
      if(in_file.fail()){
        fprintf(stderr, "Error: reading record: start %08x, length %08x, chksum %08x\n", rechdr.start, rechdr.length, rechdr.chksum);
        cerr << "Error: Failed reading file" << input_file << endl;
        return false;
      }

      rom.set(rechdr.start, temp);
    }
  }

  char temp[MAX_PATH];

  switch(rom_info.width){
    case 32:{
      for(int i = 0; i < rom_count; i++){
        sprintf(temp, "%s.nb%d", input_file.substr(0, input_file.rfind(".")).c_str(), i);
        cout << "Creating rom file " << temp << endl;
    
        ofstream out_file(temp, ios::trunc | ios::binary);
        if(out_file.bad()){
          cerr << "Error: failed opening '" << temp << "'\n";
          return false;
        }
    
        out_file.write((char *)rom.ptr(i * rom_info.size), rom_info.size);
      }
    }
    break;

    case 16:{
      for(int i = 0; i < rom_count; i++){
        sprintf(temp, "%s.nb%d", input_file.substr(0, input_file.rfind(".")).c_str(), i * 2);
        cout << "Creating rom file " << temp << endl;
    
        ofstream out_file0(temp, ios::trunc | ios::binary);
        if(out_file0.bad()){
          cerr << "Error: failed opening '" << temp << "'\n";
          return false;
        }

        sprintf(temp, "%s.nb%d", input_file.substr(0, input_file.rfind(".")).c_str(), i * 2 + 1);
        cout << "Creating rom file " << temp << endl;
    
        ofstream out_file1(temp, ios::trunc | ios::binary);
        if(out_file1.bad()){
          cerr << "Error: failed opening '" << temp << "'\n";
          return false;
        }

        for(int j = 0; j < rom_info.size / 2; j++){
          out_file0.write((char *)rom.ptr(i * rom_info.size + j), 2);
          out_file1.write((char *)rom.ptr(i * rom_info.size + j + 2), 2);
        }
      }
    }
    break;
      
    default:
      fprintf(stderr, "\nError: Bad rom_width %x!\n", rom_info.width);
      return false;
      break;
  }
 
  return true;
}

