//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef INCLUDED_File_H
#define INCLUDED_File_H

#include "headers.h"

#include "config.h"
#include "memory.h"
#include "data.h"

#include "pehdr.h"
#include "romldr.h"

#define FILES   "files"
#define MODULES "modules"
#define FILE    "file"
#define MODULE  "module"

/* redefined because they don't all match up with the NT definitions */
// for m_file_attributes
#define FILE_ATTRIBUTE_READONLY       0x00000001
#define FILE_ATTRIBUTE_HIDDEN         0x00000002
#define FILE_ATTRIBUTE_SYSTEM         0x00000004

#define FILE_ATTRIBUTE_DIRECTORY      0x00000010
#define FILE_ATTRIBUTE_ARCHIVE        0x00000020
#define FILE_ATTRIBUTE_NORMAL         0x00000080

#define FILE_ATTRIBUTE_TEMPORARY      0x00000100
#define MODULE_ATTR_NOT_TRUSTED       0x00000200
#define MODULE_ATTR_NODEBUG           0x00000400
#define FILE_ATTRIBUTE_COMPRESSED     0x00000800

#define FILE_ATTRIBUTE_ROMSTATICREF   0x00001000

// for m_flags
#define FLAG_KERNEL               0x00000001
#define FLAG_FIXUP_LIKE_KERNEL    0x00000002
#define FLAG_KERNEL_FIXUP         0x00000004
#define FLAG_KERNEL_DEBUGGER      0x00000008

#define FLAG_NEEDS_SIGNING        0x00010000
#define FLAG_IGNORE_CPU           0x00020000
#define FLAG_COMPRESS_RESOURCES   0x00040000
#define FLAG_SPLIT_CODE           0x00080000

#define FLAG_MODULE               0x00100000
#define FLAG_FILE                 0x00200000

#define FLAG_RESOURCE_ONLY        0x01000000
#define FLAG_WARN_PRIVATE_BITS    0x02000000

/*****************************************************************************/
class File;
typedef list<File> FileList;

class File{
protected:
  string m_name;
  string m_build_path;
  string m_release_path;
  MemoryList::iterator m_memory_section;
  CompressorList::iterator m_compressor;

  Data m_data;

  DWORD m_load_offset;
  DWORD m_name_offset;

  DWORD m_flags;
  DWORD m_file_attributes;
  DWORD m_file_size;
  FILETIME m_file_time;

  bool get_file_info();
  virtual void convert_file_attributes_to_flags(string token, CompressorList &compressor_list);
  
public:
  File(const string &file = "");
  
  virtual bool set(const StringList &token_list, 
                   MemoryList &memory_list, 
                   CompressorList &compressor_list);
  virtual bool set(const string &file, 
                   const string &release_path, 
                   DWORD attributes, 
                   const MemoryList::iterator memory_section, 
                   const CompressorList::iterator compressor);
  virtual bool load();
  virtual bool compress(bool comp);

  string name()             const{ return m_name; }
  string full_path()        const{ return m_release_path; }
  DWORD  size()             const{ return m_file_size; }
  DWORD  compressed_size()  const{ return m_data.size(); }
  DWORD  file_attributes()  const{ return m_file_attributes; }
  bool   is_module()        const{ return m_flags & FLAG_MODULE; }
  bool   is_file()          const{ return m_flags & FLAG_FILE; }
  bool   is_resource_only() const{ return m_flags * FLAG_RESOURCE_ONLY; }
  
  void set_module(){ m_flags &= ~FLAG_FILE; m_flags |= FLAG_MODULE; }
  void set_file()  { m_flags &= ~FLAG_MODULE; m_flags |= FLAG_FILE; }

  MemoryList::iterator       memory_iterator()      { return m_memory_section; }
  MemoryList::const_iterator memory_iterator() const{ return m_memory_section; }

  bool move_locataion(AddressList &address_list);
  bool move_name(AddressList &address_list);
  virtual bool write(ostream &out_file);
  FILESentry get_TOC();

//statics
protected:
  static DWORD s_rom_offset;
  static DWORD s_page_size;
  static DWORD s_physlast;
  
public:
  static DWORD physlast() { return s_physlast; }
    
  static void print_header();
  static void set_rom_offset(DWORD addr) { s_rom_offset = addr; }
  
  static DWORD chksum(const void *ptr, DWORD size);
  static bool write_bin(ostream &out_file, DWORD offset, const void *data, DWORD size, bool check_overlap = true);
  static DWORD page_size() { return s_page_size; }
  static DWORD align_page(DWORD addr) { return (addr + page_size() - 1) & ~(page_size() - 1); }
};

/*****************************************************************************/
inline bool operator==(const File &b1, const File &b2){ return b1.name() == b2.name() && b1.memory_iterator() == b2.memory_iterator(); }

ostream &operator<<(ostream &out_file, const File &file);
ostream &operator<<(ostream &out_file, const FileList &file_list);

#endif
