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
#include "rom.h"

#include "data.h"

struct RecHdr{
  DWORD start;
  DWORD length;
  DWORD chksum;
};

bool write_rom(wstring input_file, RomInfo rom_info){
  bool ret = false;
    
  cout << "Writing rom file...\n";
  
  DWORD start;
  DWORD length;
  Data buffer;

  DWORD rom_count = 0;
  RecHdr rechdr = {0};

  Data rom;

  FILE *in_file= _wfopen(input_file.c_str(), L"rb");
  if(!in_file){
    wcerr << L"Error: Could not open '" << input_file << L"' for reading\n";
    return false;
  }

  fread((char *)buffer.user_fill(7), 1, 7, in_file);
  if(ferror(in_file) || feof(in_file)){
    wcerr << L"Error: Failed reading bin signature in " << input_file << endl;
    goto exit;
  }

  if(memcmp("B000FF\n", buffer.ptr(), 7) != 0){
    cerr << "Error: Missing initial signature (BOOOFF\x0A). Not a BIN file\n";
    goto exit;
  }

  fread((char *)&start, 1, sizeof(start), in_file);
  if(ferror(in_file) || feof(in_file)){
    wcerr << L"Error: Failed reading image start in " << input_file << endl;
    goto exit;
  }

  start -= rom_info.offset;

  fread((char *)&length, 1, sizeof(length), in_file);
  if(ferror(in_file) || feof(in_file)){
    wcerr << L"Error: Failed reading image length in " << input_file << endl;
    goto exit;
  }

  if(rom_info.start != start)
    printf("First %u bytes of rom empty\n", start - rom_info.start);

  rom_count = (start + length - rom_info.start) / rom_info.size;
  if((start + length - rom_info.start) % rom_info.size)
    rom_count++;

  rom.reserve(rom_count * rom_info.size);
  rom.allow_shrink(false);
  for(;;){
    memset(&rechdr, 0, sizeof(rechdr));
    
    fread((char *)&rechdr, 1, 3 * sizeof(DWORD), in_file);
    if(ferror(in_file) || feof(in_file)){
      wcerr << L"Error: Failed reading record header in " << input_file << endl;
      goto exit;
    }

    if(!rechdr.start && !rechdr.chksum)
      break;

    printf("Start %08x Len %08x\n", rechdr.start, rechdr.length);

    rechdr.start -= rom_info.offset + rom_info.start;

    if(rechdr.start >= rom_info.size * rom_count){
      fprintf(stderr, "Warning: Record starting at %08x is outside of rom range, skipping...\n", rechdr.start);
      fseek(in_file, rechdr.length, SEEK_CUR);
    }
    else{
      Data temp;

      fread((char *)temp.user_fill(rechdr.length), 1, rechdr.length, in_file);
      if(ferror(in_file) || feof(in_file)){
        fprintf(stderr, "Error: reading record: start %08x, length %08x, chksum %08x\n", rechdr.start, rechdr.length, rechdr.chksum);
        wcerr << L"Error: Failed reading file" << input_file << endl;
        goto exit;
      }

      rom.set(rechdr.start, temp);
    }
  }

  wchar_t temp[MAX_PATH];

  switch(rom_info.width){
    case 32:{
      for(unsigned i = 0; i < rom_count; i++){
        wsprintfW(temp, L"%s.nb%d", input_file.substr(0, input_file.rfind(L".")).c_str(), i);
        wcout << L"Creating rom file " << temp << endl;
    
        FILE *out_file = _wfopen(temp, L"wb");
        if(!out_file){
          wcerr << L"Error: failed opening '" << temp << L"'\n";
          goto exit;
        }
    
        fwrite((char *)rom.ptr(i * rom_info.size), 1, rom_info.size, out_file);

        fclose(out_file);
      }
    }
    break;

    case 16:{
      for(unsigned i = 0; i < rom_count; i+=2){
        wsprintfW(temp, L"%s.nb%d", input_file.substr(0, input_file.rfind(L".")).c_str(), i * 2);
        wcout << L"Creating rom file " << temp << endl;
    
        FILE *out_file0 = _wfopen(temp, L"wb");
        if(!out_file0){
          wcerr << L"Error: failed opening '" << temp << L"'\n";
          goto exit;
        }

        wsprintfW(temp, L"%s.nb%d", input_file.substr(0, input_file.rfind(L".")).c_str(), i * 2 + 1);
        cout << L"Creating rom file " << temp << endl;
    
        FILE *out_file1 = _wfopen(temp, L"wb");
        if(!out_file1){
          wcerr << L"Error: failed opening '" << temp << L"'\n";
          fclose(out_file0);
          goto exit;
        }

        for(unsigned j = 0; j < rom_info.size; j+=4){
          fwrite((char *)rom.ptr(i * rom_info.size + j), 1, 2, out_file0);
          fwrite((char *)rom.ptr(i * rom_info.size + j + 2), 1, 2, out_file1);
        }

        fclose(out_file0);
        fclose(out_file1);
      }
    }
    break;
      
    default:
      fprintf(stderr, "\nError: Bad rom_width %x!\n", rom_info.width);
      goto exit;
      break;
  }

  ret = true;
  
exit:
  if(in_file) fclose(in_file);
 
  return ret;
}

