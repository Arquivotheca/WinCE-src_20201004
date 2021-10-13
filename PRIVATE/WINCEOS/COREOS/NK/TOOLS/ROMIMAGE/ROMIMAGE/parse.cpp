//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
    StringList token_list - tokenized line
    
  out:
    none

  desc:
    checks the line for section heading, and then parses all line into the 
    apropriate sections.
******************************************************************************/
void ProcessError(const StringList &slist){
  for(unsigned i = 0; i < slist.size(); i++)
    cerr << slist[i] << " ";
  cerr << endl;

  cerr << "\nError: failed setting line\n";
  
  exit(1);
}

void ProcessBIBLine(const StringList &token_list){
  static string section = "";

  if(token_list.empty())
    return;

  if(IsSection(token_list[0]))
    section = token_list[0];
  else if(section.empty())
    cerr << "Warning: found '" << token_list[0] << "' before a section header\n";

  else if(section == CONFIG){
    if(!pconfig->set(token_list, *pmemory_list)) 
      ProcessError(token_list);
  }
  else if(section == FILES){
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
    }
  }
  else if(section == MODULES){
    File file;
    Module module;

    if(!module.set(token_list, *pmemory_list, pconfig->compressor_list))
      ProcessError(token_list);

    if(module.is_module()){
      if(!IsDuplicate(*pmodule_list, module) && !IsDuplicate(*pfile_list, module))
        pmodule_list->push_back(module);
    }
    else{
      if(!file.set(token_list, *pmemory_list, pconfig->compressor_list)) 
        ProcessError(token_list);
      
      if(!IsDuplicate(*pmodule_list, file) && !IsDuplicate(*pfile_list, file))
        pfile_list->push_back(file);
    }
  }
  else if(section == MEMORY){
    Memory memory;

    if(!memory.set(token_list))
      ProcessError(token_list);

    if(memory.type() == RESERVED_TYPE)
      preserve_list->push_back(memory);
    else if(!IsDuplicate(*pmemory_list, memory))
      pmemory_list->push_back(memory);
  }
  else
    cerr << "Internal Error: " << section << " not processed yet!\n";
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
bool ParseBIBFile(const string &file_name, 
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
  
  StringList input_listing;
  vector<StringList> token_listing;
  string line;

  if(GetFileAttributes(file_name.c_str()) == -1){
    cerr << "Error: Could not find file '" << file_name << "'\n";
    return false;
  }

  ifstream file(file_name.c_str(), ios::in);

  if(file.bad()){
    cerr << "Error: Could not open '" << file_name << "' for reading\n";
    return false;
  }
  
  while(GetNextLine(file, line)){
    input_listing.push_back(line);
  }

  if(Token::CheckQuotes("", true)){
    cerr << "Error: Missmatched quotes in '" << file_name << "'\n";
    return false;
  }
  
  file.close();

  transform(input_listing.begin(), input_listing.end(), input_listing.begin(), FudgeIfs);
  transform(input_listing.begin(), input_listing.end(), input_listing.begin(), ExpandEnvironmentVariables);
  transform(input_listing.begin(), input_listing.end(), back_inserter(token_listing), Token::tokenize);

  // Note: remove_if() is querky, look it up and make SURE you know how it works before touching this!
  token_listing.erase(remove_if(token_listing.begin(), token_listing.end(), SkipLine), token_listing.end());
  
  // if we are still ignoring anything then there was a missmatch
  if(SkipLine(StringList())){
    cerr << "Error: Unmatched IF/ELSE/ENDIF block... somewhere :(\n";  
    return false;
  }
  
  for_each(token_listing.begin(), token_listing.end(), ProcessBIBLine);

  return config.sanity_check(memory_list);
}


