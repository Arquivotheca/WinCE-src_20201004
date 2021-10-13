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
#ifndef INCLUDED_CONFIG_H
#define INCLUDED_CONFIG_H

#include "headers.h"

#include "compress.h"
#include "memory.h"

#define CONFIG  "config"

struct RomInfo{
  DWORD offset;
  DWORD base;
  
  DWORD start;
  DWORD size;
  DWORD width;

  DWORD flags;
};

class Compressor;
typedef vector<Compressor> CompressorList;

class Compressor{
private:
  string m_name;
  HMODULE m_hcomp;
  
public:
  Compressor(string name){ m_name = name; m_hcomp = NULL; }
  ~Compressor();

  bool init();
  
  CECOMPRESS cecompress;
  CEDECOMPRESS cedecompress;  
};  

class Config{
private:
  
public:
  int  autosize;     // flag to autosize ram for ram only images

  bool compress;      // wether or not to compress some things
  bool bincompress;   // wether or not to compress whole bin file
  bool copy_files;    // wether or not to copy files to their short names
  bool bootjump;     // include jump startup code
  bool x86boot;  // include special x86 boot jump
  bool kernel_fixups; // fixup kernel writeable data segments
  bool profile;       // add profile data
  bool profile_all;   // use all public symbols flag
  bool bSRE;          // generate SRE file
  bool error_too_big; // force warning on too big to an error
  bool error_late_bind; // force warning on late bind to error
  
  bool rom_autosize; // autosize rom
  bool ram_autosize; // ram autosize (used with multixip)
  
  bool dll_addr_autosize; // autosize the dll reagions for XIP

  DWORD xipchain;     // location of the xipchain
  string chainvar;    // fixup variable for the chain location
    
  static const DWORD m_FSRAM_PERCENTAGE; // default ram split
    
  DWORD bootjump_addr;
  DWORD x86boot_addr;      // add jump to start for x86 bootup

  DWORD user_reset_vector;

  RomInfo rom_info;
    
  DWORD fsram_percent;
  wstring output_file;

  CompressorList compressor_list;

  Config();

  bool set(wStringList token_list, MemoryList &memory_list);
  bool sanity_check(MemoryList &memory_list);
};

#endif
