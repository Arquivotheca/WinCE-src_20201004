//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "file.h"

#include "compress.h"
/*****************************************************************************
  File()

  in:
    none
    
  out:
    none

  desc:
    initializes the class variables
******************************************************************************/
File::File(const string &file){
  m_name = file;
  m_build_path = "";
  m_release_path = "";
  
  m_load_offset = 0;
  m_name_offset = 0;

  // m_memory_section = ?;

  m_file_attributes = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_COMPRESSED;
  m_flags = 0;
  m_file_size = 0;

  set_file(); 
  
  memset(&m_file_time, 0, sizeof(m_file_time));
}

/*****************************************************************************
  convert_file_attributes_to_flags()

  in:
    string token - token to set flags for

  out:
    none
    
  desc:
    this function is needed because CE decided to reuse several bits and 
    therefor not all the flags match up with the NT defined defaults.

    NOTE: this is overridden in the module class
******************************************************************************/
void File::convert_file_attributes_to_flags(string token, CompressorList &compressor_list){
  token = lowercase(token);

  if(is_module()){
    // for files that will be modules
    m_file_attributes &= ~FILE_ATTRIBUTE_COMPRESSED;
    m_flags |= FLAG_SPLIT_CODE;
    
    if(token.find('p') != string::npos) m_flags |= FLAG_IGNORE_CPU;
    if(token.find('x') != string::npos) m_flags |= FLAG_NEEDS_SIGNING;
    if(token.find('k') != string::npos) m_flags |= FLAG_FIXUP_LIKE_KERNEL;
    if(token.find('r') != string::npos) m_flags |= FLAG_COMPRESS_RESOURCES;
    if(token.find('m') != string::npos) m_flags |= IMAGE_SCN_MEM_NOT_PAGED;
    if(token.find('l') != string::npos) m_flags &= ~FLAG_SPLIT_CODE;
  }
  
  if(token.find('h') != string::npos) m_file_attributes |= FILE_ATTRIBUTE_HIDDEN;
  if(token.find('s') != string::npos) m_file_attributes |= FILE_ATTRIBUTE_SYSTEM;
  if(token.find('n') != string::npos) m_file_attributes |= MODULE_ATTR_NOT_TRUSTED;
  if(token.find('d') != string::npos) m_file_attributes |= MODULE_ATTR_NODEBUG;
  if(token.find('u') != string::npos) m_file_attributes &= ~FILE_ATTRIBUTE_COMPRESSED;
  if(token.find('c') != string::npos) m_file_attributes |= FILE_ATTRIBUTE_COMPRESSED;

  CompressorList::iterator citr = compressor_list.begin();
  for(char i = '0'; i <= '9'; i++, citr++){
    if(token.find(i) != string::npos){
      m_compressor = citr;
      break;
    }
  }
}

/*****************************************************************************
  set()

  in:
    StringList token_list  - vector of string tokens from parsed line
    MemoryList memory_list - the already complete memory list so the we
                                 know what section this file belongs in
    bool compressed            - is this file meant to be compressed
    
  out:
    boolean value indicating success

  desc:
    initializes the class variables and sets the file attributes
******************************************************************************/
bool 
File::set(
  const StringList &token_list, 
  MemoryList &memory_list, 
  CompressorList &compressor_list)
{
  // hack for paths with ('s and )'s in them!
  StringList fudged_list;
  for(int i = 0; i < token_list.size(); i++){
    if(i + 4 < token_list.size() && token_list[i + 1] == "("){
      string token = token_list[i] + token_list[i + 1] + token_list[i + 2] + token_list[i + 3] + token_list[i + 4];
      fudged_list.push_back(token);
      i += 4;
    }
    else
      fudged_list.push_back(token_list[i]);
  }
  
  if(fudged_list.size() < 3 || fudged_list.size() > 5){
    cerr << "Error: Incorrect number of tokens found parsing file\n";
    cerr << "  found: ";
    for(int i = 0; i < fudged_list.size(); i++)
      cerr << "'" << fudged_list[i] << "'  ";
    cerr << endl;
    return false;
  }

  // takes a line like:
  //   nk.exe  c:\wince400\public\projects\nkexe\SH3_rel\nk.exe  NK  SH
  // or:
  //   public\projects\nkexe\SH3_rel\nk.exe  c:\wince400\public\projects\nkexe\SH3_rel\nk.exe  NK  SH
  m_name = fudged_list[0];
  if(m_name.rfind("\\")){
    m_build_path = m_name.substr(0, m_name.rfind("\\") + 1);
    m_name.erase(0, m_build_path.size());
  }
  
  m_release_path = fudged_list[1];

  // find an iterator to the memory section specified from the memory_list
  m_memory_section = find(memory_list.begin(), memory_list.end(), Memory(fudged_list[2]));
  if(m_memory_section == memory_list.end()){
    cerr << "Error: Unknown memory type found '" << fudged_list[2] << "'\n";
    return false;
  }

  // init default compressor
  m_compressor = compressor_list.begin();

  if(fudged_list.size() >= 4){
    if(fudged_list[3] == MODULE) set_module();
    if(fudged_list[3] == FILE)   set_file();

    if(fudged_list[3] == MODULE || fudged_list[3] == FILE){
      if(fudged_list.size() >= 5)
        convert_file_attributes_to_flags(fudged_list[4], compressor_list);
    }
    else
      convert_file_attributes_to_flags(fudged_list[3], compressor_list);
  }

  return get_file_info();
}

bool 
File::set(
  const string &file, 
  const string &release_path, 
  DWORD attributes, 
  const MemoryList::iterator memory_section, 
  const CompressorList::iterator compressor)
{
  m_name = file;
  m_release_path = release_path;
  m_memory_section = memory_section;
  m_file_attributes = attributes;

  m_compressor = compressor;
  
  return get_file_info();
}

/*****************************************************************************
  load()

  in:
    none

  out:
    boolean value indicating success

  desc:
******************************************************************************/
bool 
File::load()
{
  if(GetFileAttributes(m_release_path.c_str()) == -1){
    cerr << "Error: Could not find file '" << m_release_path << "'\n";
    return false;
  }

  ifstream file(m_release_path.c_str(), ios::in|ios::binary);

  if(file.bad()){
    cerr << "Error: Could not open '" << m_release_path << "' for reading\n";
    return false;
  }

  get_file_info();
  file.read((char *)m_data.user_fill(m_file_size), m_file_size);
  if(file.fail()){
    cerr << "Error: Failed reading file " << m_release_path << endl;
    return false;
  }
  
  return true;
}

/*****************************************************************************
  compress()

  in:
    none

  out:
    boolean value indicating success

  desc:
******************************************************************************/
bool File::compress(bool comp){
  // no compression wanted or zero length
  if(!comp || !m_file_size)
    m_file_attributes &= ~FILE_ATTRIBUTE_COMPRESSED;

  // no compressed attribute
  if(!(m_file_attributes & FILE_ATTRIBUTE_COMPRESSED))
    return true;

  // compress the section
  Data compressed_data;
  DWORD compressed_size;

  assert(s_page_size);

  compressed_size = m_compressor->cecompress(m_data.ptr(), 
                                             m_file_size, 
                                             compressed_data.user_fill(m_file_size), 
                                             m_file_size - 1, 
                                             1, 
                                             s_page_size);

  if(compressed_size == -1) // failed
    m_file_attributes &= ~FILE_ATTRIBUTE_COMPRESSED;    // we are no longer considered compressed
  else                      // succeeded
    m_data.set(compressed_data.ptr(), compressed_size); // chose not to use = operator to allow for explicit resize
  
  return true;
}

/*****************************************************************************
 *****************************************************************************/
void File::print_header(){
  static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "move") : NULL;
  if(suppress) return;
  
  LAST_PASS_PRINT printf("\nFILES Section\n");
  LAST_PASS_PRINT printf("    Raw   Compr Location Filler File\n");
}

/*****************************************************************************
 *****************************************************************************/
bool File::move_locataion(AddressList &address_list){
  bool filler = false;
  m_load_offset = Memory::allocate_range(address_list, m_data.size(), &filler);

  if(m_load_offset == -1){
    fprintf(stderr, "Error: Ran out of space in ROM for %s\nsize %d\n", m_name.c_str(), m_data.size());
    return false;
  }

  static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "move") : NULL;
  if(!suppress){
    static char *delim = getenv("ri_comma_delimit") ? "," : " ";

    LAST_PASS_PRINT
      printf("%7d%s%7d%s%08x%s%s%s%s\n",
           m_file_size, delim,
           m_data.size(), delim,
           m_load_offset, delim,
           filler ? "FILLER" : "", delim,
           m_name.c_str());
  }

  return true;
}

/*****************************************************************************
 *****************************************************************************/
bool File::move_name(AddressList &address_list){
  DWORD size = m_name.size() + 1;
  if(!m_build_path.empty())
    size += m_build_path.size() + 1;

  bool filler = false;
  m_name_offset = Memory::allocate_range(address_list, size, &filler);

  if(m_name_offset == -1){
    fprintf(stderr, "Error: Ran out of space in ROM for %s\nsize %d\n", m_name.c_str(), size);    
    return false;
  }

  static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "move") : NULL;
  if(!suppress){
    static char *delim = getenv("ri_comma_delimit") ? "," : " ";

    LAST_PASS_PRINT
      printf("%-22s%s%-8s%s%08x%s %7u%s%s%s              %s\n",
           m_name.c_str(), delim,
           "FileName", delim,
           m_name_offset, delim,
           size, delim,
           delim, delim,
           filler ? "FILLER" : "");
  }

  return true;
}

/*****************************************************************************
 *****************************************************************************/
bool File::write(ostream &out_file){
  // write data
  write_bin(out_file, m_load_offset, m_data.ptr(), m_data.size());

  // write out file name
  write_bin(out_file, m_name_offset, m_name.c_str(), m_name.size() + 1);

  if(!m_build_path.empty())
    write_bin(out_file, m_name_offset + m_name.size() + 1, m_build_path.c_str(), m_build_path.size() + 1);

  return true;
}

/*****************************************************************************
 *****************************************************************************/
FILESentry File::get_TOC(){
  FILESentry toc = {0};

  toc.dwFileAttributes = m_file_attributes;
  toc.ftTime           = m_file_time;
  toc.lpszFileName     = (char *)m_name_offset;
  toc.nCompFileSize    = m_data.size();
  toc.nRealFileSize    = m_file_size;
  toc.ulLoadOffset     = m_load_offset;

  return toc;
}

/*****************************************************************************
  get_file_info()

  in:
    none

  out:
    boolean value indicating success

  desc:
    reads this file status from disk, including the last write time, the file 
    size, and 8.3 filename.
******************************************************************************/
bool File::get_file_info(){
  HANDLE  hf;
  WIN32_FIND_DATA fd;

  hf = FindFirstFile(m_release_path.c_str(), &fd);
  if(hf == INVALID_HANDLE_VALUE){
    cerr << "Error: Could not find file '" << m_release_path << "' on disk\n";
    return false;
  }

  m_file_time  = fd.ftLastWriteTime;
  m_file_size  = fd.nFileSizeLow;

  FindClose(hf);

  return true;
}

DWORD File::s_rom_offset = 0;
DWORD File::s_page_size = 0x1000;
DWORD File::s_physlast = 0;

/*****************************************************************************
  chksum()

  in:
    ptr  - pointer to data to checksum
    size - length of data
    
  out:
    DWORD - check sum

  desc:
    does a simple sum check sum on a data element
******************************************************************************/
DWORD File::chksum(const void *ptr, DWORD size){
  DWORD chksum = 0;

  for(int i = 0; i < size; i++)
    chksum += *((unsigned char *)ptr + i);

  return chksum;
}

/*****************************************************************************
  write_bin()

  in:
    out_file - an open ostream for output
    offset   - record offset in image
    data     - pointer to data
    size     - size of data
    
  out:
    boolean indicating success

  desc:
    writes a record to the bin file with image offset, length, and checksum
******************************************************************************/
bool File::write_bin(ostream &out_file, DWORD offset, const void *data, DWORD size, bool check_overlap){
  static AddressList overlap_list;
  
  if(!offset && !data && !size){
    overlap_list.clear();
    s_physlast = 0;
  }
  
  if(!size)
    return true;

  offset += s_rom_offset;

  // check for overlaping records

  DWORD aligned_size = align_dword(size);

  if(check_overlap){
    int i = 0;
    for(AddressList::iterator addr_itr = overlap_list.begin(); addr_itr != overlap_list.end(); addr_itr++, i++){
      if(addr_itr->intersects(offset, aligned_size)){
        printf("writing... offset = %08x, dataptr = %08x, size = %8x\n", offset, (DWORD)data, aligned_size);
        printf("hit recort %d offset = %08x, size = %8x\n", i, addr_itr->address(), addr_itr->length());
        fflush(stdout);

        assert(!"Error: Bin record overlap in record");
      }
    }

    overlap_list.push_back(Address(offset, aligned_size));
  }

  DWORD sum = chksum(data, size);

  if(last_pass){
    out_file.write((char *)&offset,       sizeof(offset));
    out_file.write((char *)&aligned_size, sizeof(aligned_size));
    out_file.write((char *)&sum,          sizeof(sum));
    
    out_file.write((char *)data, size);
  
    DWORD zero = 0;
    out_file.write((char *)&zero, aligned_size - size);
  }
  
  s_physlast = max(s_physlast, offset - s_rom_offset + aligned_size);

  return true;
}

/*****************************************************************************
  output streams for class File
******************************************************************************/
inline ostream &operator<<(ostream &out_file, const File &file){
  char  buffer[1024];
  
  static char *delim = getenv("ri_comma_delimit") ? "," : " ";
  
  sprintf(buffer, "%-15s%s%-40s%s%-12s%s%08x", 
          file.name().c_str(), delim,
          file.full_path().c_str(), delim,
          file.memory_iterator()->name().c_str(), delim,
          file.file_attributes());

  out_file << buffer;

  return out_file;
}

ostream &operator<<(ostream &out_file, const FileList &file_list){
  static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "file") : NULL;
  if(suppress) return out_file;
  
  out_file << "File Name       Path     Section\n"
              "--------------- -------- -------\n";

  for(FileList::const_iterator fitr = file_list.begin(); fitr != file_list.end(); fitr++)
    cout << *fitr << endl;
  
  return out_file;
}

