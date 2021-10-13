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
#ifndef INCLUDED_File_H
#define INCLUDED_File_H

#include "headers.h"

#include "config.h"
#include "memory.h"
#include "data.h"

#include "pehdr.h"
#include "romldr.h"

#define aFILES   "files"
#define aMODULES "modules"
#define aFILE    "file"
#define aMODULE  "module"

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
#define FLAG_KERNEL                   0x00000001
#define FLAG_LIKE_KERNEL              0x00000002
#define FLAG_KERNEL_FIXUP             0x00000004
#define FLAG_KERNEL_DEBUGGER          0x00000008

#define FLAG_KERNEL_DLL               0x00000010
#define FLAG_DUAL_MODE                0x00000020

#define FLAG_NEEDS_SIGNING            0x00010000
#define FLAG_IGNORE_CPU               0x00020000
#define FLAG_COMPRESS_RESOURCES       0x00040000

#define FLAG_MODULE                   0x00100000
#define FLAG_FILE                     0x00200000

#define FLAG_RESOURCE_ONLY            0x01000000
#define FLAG_WARN_PRIVATE_BITS        0x02000000

/*****************************************************************************/
class File;
typedef list<File> FileList;

class File{
protected:
  string m_name;
  string m_build_path;
  wstring m_release_path;
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
  
  virtual bool set(const wStringList &token_list, 
                   MemoryList &memory_list, 
                   CompressorList &compressor_list);
  virtual bool set(const string &file, 
                   const wstring &release_path, 
                   DWORD attributes, 
                   const MemoryList::iterator memory_section, 
                   const CompressorList::iterator compressor);
  virtual bool load();
  virtual bool compress(bool comp);

  void name(string new_name) { m_name = new_name; }
  string name()             const{ return m_name; }
  wstring full_path()        const{ return m_release_path; }
  DWORD  size()             const{ return m_file_size; }
  DWORD  compressed_size()  const{ return m_data.size(); }
  DWORD  file_attributes()  const{ return m_file_attributes; }
  bool   is_module()        const{ return m_flags & FLAG_MODULE; }
  bool   is_file()          const{ return m_flags & FLAG_FILE; }
  bool   is_resource_only() const{ return m_flags & FLAG_RESOURCE_ONLY; }
  
  void set_module(){ m_flags &= ~FLAG_FILE; m_flags |= FLAG_MODULE; }
  void set_file()  { m_flags &= ~FLAG_MODULE; m_flags |= FLAG_FILE; }

  MemoryList::iterator       memory_iterator()      { return m_memory_section; }
  MemoryList::const_iterator memory_iterator() const{ return m_memory_section; }

  bool move_locataion(AddressList &address_list);
  bool move_name(AddressList &address_list);
  virtual bool write(FILE *out_file);
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
  static bool write_bin(FILE *out_file, DWORD offset, const void *data, DWORD size, bool check_overlap = true);
  static DWORD page_size() { return s_page_size; }
  static DWORD align_page(DWORD addr) { return (addr + page_size() - 1) & ~(page_size() - 1); }
};

/*****************************************************************************/
inline bool operator==(const File &b1, const File &b2){ return b1.name() == b2.name() && b1.memory_iterator() == b2.memory_iterator(); }

ostream &operator<<(ostream &out_file, const File &file);
ostream &operator<<(ostream &out_file, const FileList &file_list);

#endif
