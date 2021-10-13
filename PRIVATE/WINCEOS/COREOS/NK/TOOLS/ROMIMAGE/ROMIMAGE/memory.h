//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef INCLUDED_MEMORY_H
#define INCLUDED_MEMORY_H

#include "headers.h"

#include "pehdr.h"
#include "romldr.h"

#define ROM8_TYPE     "rom_8"
#define ROM16_TYPE    "rom_16"
#define ROMx8_TYPE    "romx8"
#define RAM_TYPE      "ram"
#define RAMIMAGE_TYPE "ramimage"
#define NANDIMAGE_TYPE "nandimage"
#define RESERVED_TYPE "reserved"
#define FIXUPVAR_TYPE "fixupvar"
#define EXTENSION_TYPE "extension"
#define CHAIN_INFORMATION "chain information"

DWORD align_dword(DWORD a);
DWORD align_16k(DWORD a);
DWORD align_64k(DWORD a);

#define MEMORY  "memory"

/*****************************************************************************/
class Address;
typedef list<Address> AddressList;
  
class Address{
protected:
  bool m_hole;
  DWORD m_address;
  DWORD m_length;

public:
  Address() { m_address = 0; m_length = 0; m_hole = false; }
  Address(DWORD addr, DWORD len){ set(addr, len); m_hole = false; }

  void set(DWORD addr, DWORD len){ m_address = addr; m_length = len; }

  bool hole(){ return m_hole; }
  void set_hole(){ m_hole = true; }

  DWORD address() const{ return m_address; }
  DWORD address_end() const{ return m_address + m_length; }
  DWORD length() const{ return m_length; }

  bool intersects(DWORD addr, DWORD len) const{ return !(addr + len <= address() || address_end() <= addr); }
  bool intersects(const Address &addr) const{ return !(addr.address_end() <= address() || address_end() <= addr.address()); }

  bool operator<(const Address &addr) const{ return m_address < addr.m_address; }

  void use(DWORD size);

  static bool comp_size(const Address &addr1, const Address &addr2) { return addr1.m_length < addr2.m_length; }
  
  static void size_sort(AddressList &address_list);
  static void dump(AddressList &address_list, bool holes_only = true);
};

/*****************************************************************************/
class Memory;
typedef list<Memory> MemoryList;

class Memory:public Address{
private:
  string m_name;
  string m_type;

  bool m_kernel;

  bool valid_memory_type(const string &token);

public:
  static const DWORD m_DLL_DATA_TOP_ADDR;     // dll data top load address
  static const DWORD m_DLL_DATA_BOTTOM_ADDR;  // dll data hard floor

  static const DWORD m_DLL_CODE_TOP_ADDR;     // dll code top load address
  static const DWORD m_DLL_CODE_BOTTOM_ADDR;  // dll code hard floor
    
  DWORD dll_code_gap;
  DWORD dll_data_gap;
  DWORD dll_gap;
  DWORD rom_gap;

  DWORD dll_data_start;
  DWORD dll_data_orig;
  DWORD dll_data_bottom;

  DWORD dll_data_split;

  DWORD dll_code_start;
  DWORD dll_code_orig;
  DWORD dll_code_bottom;

  DWORD fixupvar_section;
  DWORD extension_offset;
  string extension_location;

  bool code_space_full;

  Address origional_address;
  
  Memory(const string &s = "");
  
  bool set(const StringList &token_list);
  void init_kernel(){ m_kernel = true; }
  bool is_kernel() const { return m_kernel; }
  
  void set_extension_location(string loc){ extension_location = loc; }
  
  string name() const{ return m_name; }
  string type() const{ return m_type; }

  ROMHDR m_romhdr;

  static DWORD allocate_range(AddressList &address_list, DWORD len, bool *filler = NULL);
  static DWORD find_next_gap(const MemoryList &reserve_list, DWORD addr, DWORD len, AddressList *address_list = NULL, DWORD region_base = 0);
  static bool write_extensions(ostream &out_file, const MemoryList &memory_list, const MemoryList &reserve_list, DWORD config_addr, const Memory &xip_mem);

  bool reserve_extension(AddressList &address_list);
};

/*****************************************************************************/
inline bool operator==(const Memory &m1, const Memory &m2){ return m1.name() == m2.name(); }
inline bool operator!=(const Memory &m1, const Memory &m2){ return !(m1 == m2); }

ostream &operator<<(ostream &out_file, const Memory &memory);
ostream &operator<<(ostream &out_file, const MemoryList &memory_list);

#endif
