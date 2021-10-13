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
#include "config.h"
#include "..\parser\token.h"

const DWORD Config::m_FSRAM_PERCENTAGE = 0x80808080;

bool Compressor::init(){
  if(m_name.empty())
    return true;

  m_hcomp = LoadLibrary(m_name.c_str());
  if(!m_hcomp){
    fprintf(stderr, "Error: LoadLibrary() failed to load '%s': %d\n", m_name.c_str(), GetLastError());
    return false;
  }

  cecompress = (CECOMPRESS)GetProcAddress(m_hcomp, "CECompress");
  if(!cecompress){
    fprintf(stderr, "Error: GetProcAddress() failed to find 'CECompress' in '%s': %d\n", m_name.c_str(), GetLastError());
    return false;
  }
/*
  cedecompress = (CEDECOMPRESS)GetProcAddress(m_hcomp, "CEDecompress");
  if(!cecompress){
    fprintf(stderr, "Error: GetProcAddress() failed to find 'CEDecompress' in '%s': %d\n", m_name.c_str(), GetLastError());
    return false;
  }
*/

  return true;
}

Compressor::~Compressor(){
  if(m_hcomp) FreeLibrary(m_hcomp);
  
//  if(m_hcomp && !FreeLibrary(m_hcomp))
//    fprintf(stderr, "Error: FreeLibray() failed freeing '%s': %d\n", m_name.c_str(), GetLastError());
}

/*****************************************************************************
  Config()

  in:
    none
    
  out:
    none

  desc:
    initializes the class variables


Options:

  dllhighcodeaddr - upper limit of the code region for dll's
  dlllowcodeaddr  - lower limit of the code region for dll's

  dllhighaddr - upper limit of the data region for dll's
  dlllowaddr  - lower limit of the data region for dll's

  autosize_dllcodegap - minimum* gap for code space dll's
  autosize_dllcodegap - minimum* gap for data space dll's
  autosize_dllgap     - minimum* gap for old style dll's
  autosize_romgap     - minimum* gap for rom sections
     (* some gaps may be larger due to 64k alignment)

Usages:

  dllhighaddr  =   01e90000
  dllhighaddr  NK  01e90000

Notes:

  the named form can be used with or without multixip.  With multixip the named 
  form must be used unless dlladdr_autosize is enabled or there is only one 
  region.
    
******************************************************************************/
Config::Config(){
  compress      = true;
  profile       = true;
  bootjump      = true;
  copy_files    = true;
  
  bincompress   = false;
  autosize      = false;
  kernel_fixups = false;
  profile_all   = false;
  bSRE          = false;
  x86boot       = false;
  error_too_big = false;
  error_late_bind = false;
  rom_autosize  = false;
  ram_autosize  = false;
  dll_addr_autosize = false;
   
  bootjump_addr     = 0;
  x86boot_addr      = 0;
  user_reset_vector = 0;
  xipchain          = 0;
  chainvar = "";

  memset(&rom_info, 0, sizeof(rom_info));  

  fsram_percent  = m_FSRAM_PERCENTAGE;

  compressor_list.push_back(Compressor("compress.dll"));  
  for(int i = 1; i <= 9; i++)
    compressor_list.push_back(Compressor(""));

}

/*****************************************************************************
  set()

  in:
    vector<string> token_list  - vector of string tokens from parsed line
    
  out:
    boolean value indicating success

  desc:
    sets the class variables
******************************************************************************/
bool Config::set(wStringList wtoken_list, MemoryList &memory_list){
  // convert to ansi
  StringList token_list;
  for(unsigned i = 0; i < wtoken_list.size(); i++){
    token_list.push_back(toansi(wtoken_list[i]));
  }
  
  if(token_list.size() != 3){
    cerr << "Error: Incorrect number of tokens found parsing config section\n";
ERROR_EXIT:
    cerr << "  found: ";
    for(unsigned i = 0; i < token_list.size(); i++)
      cerr << "'" << token_list[i] << "'  ";
    cerr << endl;
    return false;
  }   
  
  if     (token_list[0] == "autosize")     autosize     = Token::get_bool_value(token_list[2]);
  else if(token_list[0] == "ram_autosize") ram_autosize = Token::get_bool_value(token_list[2]);  
  else if(token_list[0] == "rom_autosize") rom_autosize = Token::get_bool_value(token_list[2]);

  else if(token_list[0] == "autosize_chain") chainvar = token_list[2];

  else if(token_list[0] == "compression")  compress      = Token::get_bool_value(token_list[2]);
  else if(token_list[0] == "bincompress")  bincompress   = Token::get_bool_value(token_list[2]);

  else if(token_list[0] == "compressor")   compressor_list[0] = Compressor(token_list[2]);
  else if(token_list[0] == "compressor1")  compressor_list[1] = Compressor(token_list[2]);
  else if(token_list[0] == "compressor2")  compressor_list[2] = Compressor(token_list[2]);
  else if(token_list[0] == "compressor3")  compressor_list[3] = Compressor(token_list[2]);
  else if(token_list[0] == "compressor4")  compressor_list[4] = Compressor(token_list[2]);
  else if(token_list[0] == "compressor5")  compressor_list[5] = Compressor(token_list[2]);
  else if(token_list[0] == "compressor6")  compressor_list[6] = Compressor(token_list[2]);
  else if(token_list[0] == "compressor7")  compressor_list[7] = Compressor(token_list[2]);
  else if(token_list[0] == "compressor8")  compressor_list[8] = Compressor(token_list[2]);
  else if(token_list[0] == "compressor9")  compressor_list[9] = Compressor(token_list[2]);

  else if(token_list[0] == "kernelfixups") kernel_fixups = Token::get_bool_value(token_list[2]);
  else if(token_list[0] == "sre")          bSRE          = Token::get_bool_value(token_list[2]);
  else if(token_list[0] == "copyfiles")    copy_files    = Token::get_bool_value(token_list[2]);
  else if(token_list[0] == "errontoobig")  error_too_big = Token::get_bool_value(token_list[2]);
  else if(token_list[0] == "errorlatebind")    error_late_bind   = Token::get_bool_value(token_list[2]); 
  else if(token_list[0] == "dlladdr_autosize") dll_addr_autosize = Token::get_bool_value(token_list[2]);
  
  // I really don't care about this option, if there are no fixupvar mem reagions then so be it
  else if(token_list[0] == "varsfixup")    /* don't do anything */ ;

  else if(token_list[0] == "xipschain")    xipchain          = Token::get_hex_value(token_list[2]);
  else if(token_list[0] == "fsrampercent") fsram_percent     = Token::get_hex_value(token_list[2]);
  else if(token_list[0] == "resetvector")  user_reset_vector = Token::get_hex_value(token_list[2]);

  else if(token_list[0] == "romoffset")    rom_info.offset = Token::get_hex_value(token_list[2]);
  else if(token_list[0] == "romflags")     rom_info.flags  = Token::get_hex_value(token_list[2]);
  else if(token_list[0] == "romsize")      rom_info.size   = Token::get_hex_value(token_list[2]);
  else if(token_list[0] == "romstart")     rom_info.start  = Token::get_hex_value(token_list[2]);
  
  else if(token_list[0] == "romwidth")     rom_info.width  = Token::get_dec_value(token_list[2]);

  else if(token_list[0] == "output")       output_file = wtoken_list[2];
  else if(token_list[0] == "imagestart")   cerr << "Warning: IMAGESTART no longer supported\n";

  else if(token_list[0] == "dllhighcodeaddr" || 
          token_list[0] == "dlllowcodeaddr"){
    cerr << "Warning: DLLHIGHCODEADDR and DLLLOWCODEADDR are no longer supported.  Values ignored.\n";
  }

  else if(token_list[0] == "dllhighaddr"     || 
          token_list[0] == "dlllowaddr"){
    bool failed = true;
    
    if(xipchain && !dll_addr_autosize && token_list[1] == "="){
      cerr << "Error: " << token_list[0] << " must be specified per xip region when dlladdr_autosize isn't enabled\n";
      goto ERROR_EXIT;
    }

    for(MemoryList::iterator mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
      if(token_list[1] == mem_itr->name() || 
         (token_list[1] == "=" && (mem_itr->type() == RAMIMAGE_TYPE || mem_itr->type() == NANDIMAGE_TYPE))
        ){
        if(token_list[0] == "dllhighaddr")     mem_itr->dll_data_top_orig = Token::get_hex_value(token_list[2]);
        else if(token_list[0] == "dlllowaddr") mem_itr->dll_data_top = mem_itr->dll_data_bottom = mem_itr->dll_data_bottom_orig = Token::get_hex_value(token_list[2]);
        failed = false;
      }
    }

    if(failed){
      cerr << "Error: Unknown config entry\n";
      goto ERROR_EXIT;
    }
  }

  else if(token_list[0] == "autosize_dllcodeaddrgap" ||
          token_list[0] == "autosize_dlldataaddrgap"){
    cerr << "Warning: AUTOSIZE_DLLCODEADDRGAP and AUTOSIZE_DLLDATAADDRGAP no longer supported.  Values ignored.\n";
  }

  else if(token_list[0] == "autosize_dlladdrgap"     || 
          token_list[0] == "autosize_romgap"){
    bool failed = true;

    for(MemoryList::iterator mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
      if(token_list[1] == mem_itr->name() || 
         (token_list[1] == "=" && (mem_itr->type() == RAMIMAGE_TYPE || mem_itr->type() == NANDIMAGE_TYPE))
        ){
        if(token_list[0] == "autosize_dlladdrgap")  mem_itr->dll_gap = Token::get_hex_value(token_list[2]);
        else if(token_list[0] == "autosize_romgap") mem_itr->rom_gap = Token::get_hex_value(token_list[2]);
        failed = false;
      }
    }

    if(failed){
      cerr << "Error: Unknown config entry\n";
      goto ERROR_EXIT;
    }
  }
 
  else if(token_list[0] == "x86boot"){
    x86boot = Token::get_bool_value(token_list[2], "off", false);

    if(x86boot)
      x86boot_addr = Token::get_hex_value(token_list[2]);
  }
  else if(token_list[0] == "bootjump"){
    bootjump = Token::get_bool_value(token_list[2], "none", false);

    if(bootjump)
      bootjump_addr = Token::get_hex_value(token_list[2]);
  }
  else if(token_list[0] == "profile"){
    profile = Token::get_bool_value(token_list[2]);
  
    if(Token::get_bool_value(token_list[2], "all"))
      profile_all = profile = true;

    static char *profile_env = getenv("winceprofile");
    if(profile_env) // WINCEPROFILE overrides PROFILE= config statement
      profile = true;
  }
  else{
    cerr << "Error : Unknown config entry\n";
    goto ERROR_EXIT;
  }

  return true;
}

/*****************************************************************************
  sanity_check()

  in:
    none
    
  out:
    boolean value indicating success

  desc:
    run over the settings and see if they make any sense
******************************************************************************/
bool Config::sanity_check(MemoryList &memory_list){
  if(xipchain && !autosize){
    if(rom_autosize || ram_autosize || dll_addr_autosize){
      cerr << "Warning: setting ROM_AUTOSIZE, RAM_AUTOSIZE, or DLLADDR_AUTOSIZE are ignored without AUTOSIZE=ON\n";
      ram_autosize = false;
      rom_autosize = false;
      dll_addr_autosize = false;      
    }
  }

  if(ram_autosize && !rom_autosize){
    cerr << "Warning: setting RAM_AUTOSIZE is ignored without ROM_AUTOSIZE=ON\n";
    ram_autosize = false;
  }    

  for(MemoryList::const_iterator mem_itr1 = memory_list.begin(); mem_itr1 != memory_list.end(); mem_itr1++){
    for(MemoryList::const_iterator mem_itr2 = mem_itr1; ++mem_itr2 != memory_list.end(); /* preincremented in conditional */){
      if((mem_itr1->type() == RAMIMAGE_TYPE || mem_itr1->type() == NANDIMAGE_TYPE) && 
         (mem_itr2->type() == RAMIMAGE_TYPE || mem_itr2->type() == NANDIMAGE_TYPE)){
        if(!dll_addr_autosize || !autosize){
          // check dll ranges
          if(Address(mem_itr1->dll_data_bottom_orig, mem_itr1->dll_data_top_orig - mem_itr1->dll_data_bottom_orig).intersects(mem_itr2->dll_data_bottom_orig, mem_itr2->dll_data_top_orig - mem_itr2->dll_data_bottom_orig)){
            cerr << "Error: intersecting DLL ranges specified without DLLADDR_AUTOSIZE\n";

            printf("%s (%08x->%08x) intersects %s (%08x->%08x)\n", 
                   mem_itr1->name().c_str(),
                   mem_itr1->dll_data_bottom_orig,
                   mem_itr1->dll_data_top_orig,
                   mem_itr2->name().c_str(),
                   mem_itr2->dll_data_bottom_orig,
                   mem_itr2->dll_data_top_orig);
            
            return false;
          }
          if(Address(mem_itr1->kernel_bottom_orig, mem_itr1->kernel_top_orig - mem_itr1->kernel_bottom_orig).intersects(mem_itr2->kernel_bottom_orig, mem_itr2->kernel_top_orig - mem_itr2->kernel_bottom_orig)){
            cerr << "Error: Kernel ranges intesect.\n";

            printf("%s (%08x->%08x) intersects %s (%08x->%08x)\n", 
                   mem_itr1->name().c_str(),
                   mem_itr1->kernel_bottom_orig,
                   mem_itr1->kernel_top_orig,
                   mem_itr2->name().c_str(),
                   mem_itr2->kernel_bottom_orig,
                   mem_itr2->kernel_top_orig);
            
            return false;
          }
          if(Address(mem_itr1->kernel_dll_bottom_orig, mem_itr1->kernel_dll_top_orig - mem_itr1->kernel_dll_bottom_orig).intersects(mem_itr2->kernel_dll_bottom_orig, mem_itr2->kernel_dll_top_orig - mem_itr2->kernel_dll_bottom_orig)){
            cerr << "Error: Kernel dll ranges intesect.\n";

            printf("%s (%08x->%08x) intersects %s (%08x->%08x)\n", 
                   mem_itr1->name().c_str(),
                   mem_itr1->kernel_dll_bottom_orig,
                   mem_itr1->kernel_dll_top_orig,
                   mem_itr2->name().c_str(),
                   mem_itr2->kernel_dll_bottom_orig,
                   mem_itr2->kernel_dll_top_orig);
            
            return false;
          }
        }

        if(!rom_autosize || !autosize){
          // check romimage ranges
          if(mem_itr1->intersects((Address)*mem_itr2)){
            cerr << "Error: intersecting RAMIMAGE regions specified withough ROM_AUTOSIZE\n";
            return false;
          }
        }
      }
    }
  }

  for(MemoryList::iterator mem_itr = memory_list.begin(); mem_itr!= memory_list.end(); mem_itr++){
    if(!rom_autosize)
      mem_itr->rom_gap = 0;

    if(!dll_addr_autosize)
      mem_itr->dll_gap = 0;

    // bugbug: check for last 64 clear for each region?
  }

  for(CompressorList::iterator citr = compressor_list.begin(); citr != compressor_list.end(); citr++)
    if(!citr->init())
      return false;

  return true;
}

