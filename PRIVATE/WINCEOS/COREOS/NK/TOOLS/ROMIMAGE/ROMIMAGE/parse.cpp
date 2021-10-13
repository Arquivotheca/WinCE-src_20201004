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
#include "parse.h"

// do this because passing five params to a stl for_each callback is a PITA!
static MemoryList *pmemory_list;
static MemoryList *preserve_list;
static ModuleList *pmodule_list;
static FileList   *pfile_list;
static Config     *pconfig;

/*****************************************************************************
  IsDuplicate()

  in:
    class container - any container class that supports begin(), and end() 
                      members
    class object    - object type stored in the container
    
  out:
    boolean indicating duplicate object
    
  desc:
    determins if the object already exists in the list.  Used before insertion
    to prevent duping data objects.
******************************************************************************/
template<class container, class object>
bool IsDuplicate(const container &cont, const object &item){
  if(find(cont.begin(), cont.end(), item) == cont.end())
    return false;

  cerr << "Warning: Found duplicate entry '" << item << "' ... skipping\n";
  return true;
}

/*****************************************************************************
  ProcessBIBLine()

  in:
    wStringList token_list - tokenized line
    
  out:
    none

  desc:
    checks the line for section heading, and then parses all line into the 
    apropriate sections.
******************************************************************************/
void ProcessError(const wStringList &slist){
  for(unsigned i = 0; i < slist.size(); i++)
    wcerr << slist[i] << L" ";
  cerr << endl;

  cerr << "\nError: failed setting line\n";
  
  exit(1);
}

void ProcessBIBLine(const wStringList &token_list){
  static wstring section = L"";

  if(token_list.empty())
    return;

  if(IsSection(token_list[0]))
    section = token_list[0];
  else if(section.empty() && !token_list[0].empty())
  {
    cerr << "Warning: found '";
    wcerr << token_list[0];
    cerr << "' before a section header\n";
  }

  else if(section == WCONFIG){
    if(!pconfig->set(token_list, *pmemory_list)) 
      ProcessError(token_list);
  }
  else if(section == WFILES){
    File file;
    Module module;

    if(!file.set(token_list, *pmemory_list, pconfig->compressor_list)) 
      ProcessError(token_list);

    if(file.is_file()){
      if(!IsDuplicate(*pmodule_list, file) && !IsDuplicate(*pfile_list, file))
        pfile_list->push_back(file);
    }
    else{
      if(!module.set(token_list, *pmemory_list, pconfig->compressor_list)) 
        ProcessError(token_list);

      if(!IsDuplicate(*pmodule_list, module) && !IsDuplicate(*pfile_list, module))
        pmodule_list->push_back(module);

      if(module.is_dual_mode())
      {
        module.set_kernel_dll();
        module.name("k." + module.name());

        cout << "Q flag encountered, adding module " << module.name() << "\n";

        if(!IsDuplicate(*pmodule_list, module) && !IsDuplicate(*pfile_list, module))
          pmodule_list->push_back(module);
      }
    }
  }
  else if(section == WMODULES){
    File file;
    Module module;

    if(!module.set(token_list, *pmemory_list, pconfig->compressor_list))
      ProcessError(token_list);

    if(module.is_module()){
      if(!IsDuplicate(*pmodule_list, module) && !IsDuplicate(*pfile_list, module))
        pmodule_list->push_back(module);

      if(module.is_dual_mode())
      {
        module.set_kernel_dll();
        module.name("k." + module.name());

        cout << "Q flag encountered, adding module " << module.name() << "\n";

        if(!IsDuplicate(*pmodule_list, module) && !IsDuplicate(*pfile_list, module))
          pmodule_list->push_back(module);
      }
    }
    else{
      if(!file.set(token_list, *pmemory_list, pconfig->compressor_list)) 
        ProcessError(token_list);
      
      if(!IsDuplicate(*pmodule_list, file) && !IsDuplicate(*pfile_list, file))
        pfile_list->push_back(file);
    }
  }
  else if(section == WMEMORY){
    Memory memory;

    if(!memory.set(token_list))
      ProcessError(token_list);

    if(memory.type() == RESERVED_TYPE)
      preserve_list->push_back(memory);
    else if(!IsDuplicate(*pmemory_list, memory))
      pmemory_list->push_back(memory);
  }
  else
  {
    cerr << "Internal Error: ";
    wcerr << section;
    cerr << " not processed yet!\n";
  }
}

bool PreprocessBIBFile(string &file_name, wstring &open_mode)
{
  if(GetFileAttributes(file_name.c_str()) == -1){
    cerr << "Error: Could not find file '" << file_name << "'\n";
    return false;
  }

  FILE *in_file = fopen(file_name.c_str(), "rb");
  if(!in_file){
    cerr << "Error: Could not open file '" << file_name << "'\n";
    return false;
  }

  // check for unicode
  unsigned char chr[2];
  fread(chr, 1, 2, in_file);
  fclose(in_file);

  if((chr[0] == 0xff && chr[1] == 0xfe) ||
     (chr[0] != 0x00 && chr[1] == 0x00)){
     // already unicode, do nothing
     open_mode = L"rb";
  }
  else{
    open_mode = L"rt";
  }
    
  return true;
}

/*****************************************************************************
  ParseBIBFile()

  in:
    string file_name - name of .bib file to parse
    ...
    
  out:
    boolean indicating success

  desc:
    retrieves and entire file as a vector of lines and passes it off to other
    routines for processing
******************************************************************************/
bool ParseBIBFile(string &file_name, 
                MemoryList &memory_list, 
                MemoryList &reserve_list, 
                ModuleList &module_list, 
                FileList   &file_list,
                Config     &config){

  // do this because passing five params to a stl for_each callback is a PITA!
  pmemory_list = &memory_list;
  preserve_list = &reserve_list;
  pmodule_list = &module_list;
  pfile_list = &file_list;
  pconfig = &config;
  
  wStringList input_listing;
  vector<wStringList> token_listing;
  wstring line;

  wstring open_mode = L"";
  
  if(!PreprocessBIBFile(file_name, open_mode)){
    cerr << "Error: Could not preprocess file '" << file_name << "'\n";
    return false;
  }
  
  FILE *file = _wfopen((tounicode(file_name.c_str())).c_str(), open_mode.c_str());

  if(!file){
    cerr << "Error: Could not open '" << file_name << "' for reading\n";
    return false;
  }
  
  while(GetNextLine(file, line)){
    input_listing.push_back(line);
  }

  if(Token::CheckQuotes(L"", true)){
    cerr << "Error: Missmatched quotes in '" << file_name << "'\n";
    return false;
  }
  
  fclose(file);

  transform(input_listing.begin(), input_listing.end(), input_listing.begin(), FudgeIfs);
  transform(input_listing.begin(), input_listing.end(), input_listing.begin(), ExpandEnvironmentVariables);
  transform(input_listing.begin(), input_listing.end(), back_inserter(token_listing), Token::tokenize);

  // Note: remove_if() is querky, look it up and make SURE you know how it works before touching this!
  token_listing.erase(remove_if(token_listing.begin(), token_listing.end(), SkipLine), token_listing.end());
  
  // if we are still ignoring anything then there was a missmatch
  if(SkipLine(wStringList())){
    cerr << "Error: Unmatched IF/ELSE/ENDIF block... somewhere :(\n";  // bugbug: find the line number of opening
    return false;
  }
  
  for_each(token_listing.begin(), token_listing.end(), ProcessBIBLine);

  return config.sanity_check(memory_list);
}


