//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef INCLUDED_MODULE_H
#define INCLUDED_MODULE_H

#include "headers.h"

#include "file.h"
#include "data.h"

#include "pehdr.h"
#include "romldr.h"

/*****************************************************************************
 * Helper classes/structures for modules
 *****************************************************************************/
class Function;
typedef vector<Function> FunctionList;

class Function{
public:
  Function(){ address = hit_count = 0; }
  
  DWORD address;
  DWORD hit_count;
  string name;
  bool operator<(const Function &f){ return  address < f.address; }
};

/*****************************************************************************/
class Profile;
typedef vector<Profile> ProfileList;

class Profile:public PROFentry{
public:
  Profile() { memset(this, 0, sizeof(PROFentry)); }
  FunctionList m_function_list;
};

/*****************************************************************************/
class FixupInfo;
typedef vector<FixupInfo> FixupInfoList;

class FixupInfo{
public:
 WORD  Type;
 WORD  Null;
 DWORD RvaLocation;
 DWORD RvaTarget;
};

/*****************************************************************************/
class COFF_Symbol;
typedef vector<COFF_Symbol> SymbolList;

class COFF_Symbol{
public:
  union{
    struct{
     unsigned long dwNULL;
     unsigned long dwPosLow;
    };

    unsigned char szName[8];
  };

  unsigned long  ulValue;
  unsigned short usSection;
  unsigned short usType;
  unsigned char  cClass;
  unsigned char  cNumAux;
};

/*****************************************************************************/
class O32;
typedef vector<O32> O32List;

class O32:public o32_obj{
public:
  O32(){ memset(this, 0, sizeof(o32_obj)); }
  
  Data data;

  DWORD max_size() const { return max(o32_vsize, o32_psize); }
  DWORD min_size() const { return min(o32_vsize, o32_psize); }

  string name()        { return string((char*)o32_name, min(strlen((char*)o32_name), 8)); }

  bool is_writable()    const { return o32_flags & IMAGE_SCN_MEM_WRITE; }
  bool is_compressed()  const { return o32_flags & IMAGE_SCN_COMPRESSED; }
  bool is_align_4bytes()const { return o32_flags & IMAGE_SCN_ALIGN_4BYTES; }
  bool is_discardable() const { return o32_flags & IMAGE_SCN_MEM_DISCARDABLE; }
  bool is_shared()      const { return o32_flags & IMAGE_SCN_MEM_SHARED; }
};

/*****************************************************************************/
class CopyEntry:public COPYentry{
public:
  CopyEntry() { memset(this, 0, sizeof(COPYentry)); }
    
  MemoryList::iterator m_memory_iterator;

  MemoryList::iterator memory_iterator() const { return m_memory_iterator; }
};

typedef vector<CopyEntry> CopyList;

/*****************************************************************************
 *
 * Module class - big ugly workhorse of romimage
 *
 *****************************************************************************/
class Module;
typedef vector<Module> ModuleList;

class Module:public File{
private:
  // origional module data from file
  e32_exe m_e32;
  O32List m_o32_list;
  FixupInfoList m_kernel_fixup_list;

  FileList::iterator m_signature_file;
  
  // information for relocation
  DWORD m_e32_offset;
  DWORD m_o32_offset;
  DWORD m_vreloc;

  // map symbol information for kernel
  Profile m_profile;
  DWORD m_TOC_offset;
  DWORD m_ROM_extensions;
  DWORD *m_reset_vector;
  DWORD *m_reset_vector_end;
  DWORD *m_reset_vector_addr;

  // interally used functions
  void initialize();
  bool get_map_symbols(bool profile_all, MemoryList &memory_list);
  void check_special_symbol(string token, DWORD section, DWORD offset, MemoryList &memory_list);

  DWORD rva2ptr(DWORD rva) const;
  DWORD fixup_rva_nk(DWORD fixup_addr);
  DWORD get_rva_delta(const FixupInfo &kernel_fixup, DWORD rva_delta);

  DWORD addr_from_eat(const ModuleList &module_list, DWORD eat) const;
  DWORD resolve_imp_ord(const ModuleList &module_list, DWORD ord) const;
  DWORD resolve_imp_str(const ModuleList &module_list, const string &str) const;
  DWORD resolve_imp_hint_str(const ModuleList & module_list, DWORD hint, const string &str) const;
  
  DWORD truncate_o32(O32 &o32);
  DWORD compress_o32(O32 &o32);
  
public:
  Module(const string &file = "");

  DWORD align_page(DWORD addr) const{ return (addr + page_size() - 1) & ~(page_size() - 1); }
  DWORD page_size()            const{ return m_e32.e32_objalign ? m_e32.e32_objalign : s_page_size; }

  bool init_kernel(bool fixup, const Memory &ram_section);
  bool sync_names(bool profile, bool copy_files);
  bool add_sig_files(FileList &file_list);
  bool load();

  void  set_load_address(DWORD addr);
  DWORD get_load_size();

  bool set_base(const MemoryList &reserve_list);

  bool verify_cpu() const;

  bool get_symbols(bool profile_all, MemoryList &memory_list);

  bool relocate_image();
  bool fixup();
  bool apply_fixup(WORD type, DWORD *addr, DWORD offset, WORD low_addr);
  bool fixup_no_split();
  bool fixup_nk();
  bool fixupvar(DWORD section, DWORD offset, DWORD value);
  bool remove_discardable_sections();
  void resolve_imports(ModuleList &module_list, bool error_late_bind);
  bool compress(bool comp);

  bool move_readonly_sections(const MemoryList &reserved_list, DWORD &next_avail, AddressList &address_list);
  bool move_readwrite_sections(AddressList &address_list, CopyList &copy_list);
  bool move_eo32_structures(AddressList &address_list);

  void write_prolog(ostream &out_file, DWORD &start_ip, DWORD boot_jump, DWORD user_reset_vector);
  void write_TOC_ptr(DWORD addr);
  bool sig_adjust();
  bool sign(e32_rom, Data o32hdrs);
  bool write(ostream &out_file);
  TOCentry get_TOC();
  Profile get_profile(int pass);

  bool is_arm(){ return (m_e32.e32_cpu == IMAGE_FILE_MACHINE_ARM) || 
                        (m_e32.e32_cpu == IMAGE_FILE_MACHINE_THUMB); 
               }

  bool is_dll(){ return m_e32.e32_imageflags & IMAGE_FILE_DLL; }
  bool is_exe(){ return !is_dll(); }
 
  void set_code_split(bool flag){ flag ? m_flags |= FLAG_SPLIT_CODE : m_flags &= ~FLAG_SPLIT_CODE; }

  bool is_code_split()             { return m_flags & FLAG_SPLIT_CODE; }
  bool needs_signing()             { return m_flags & FLAG_NEEDS_SIGNING; }
  bool compress_resources()        { return m_flags & FLAG_COMPRESS_RESOURCES; }
  bool ignore_cpu()           const{ return m_flags & FLAG_IGNORE_CPU; }
  bool is_kernel()                 { return m_flags & FLAG_KERNEL; }
  bool is_kernel_debugger_enabled(){ return m_flags & FLAG_KERNEL_DEBUGGER; }
  bool is_kernel_fixup_enabled()   { return m_flags & FLAG_KERNEL_FIXUP || fixup_like_kernel(); }
  bool fixup_like_kernel()         { return m_flags & FLAG_FIXUP_LIKE_KERNEL; }
  bool warn_private_bits()         { return m_flags & FLAG_WARN_PRIVATE_BITS; }

  DWORD rom_extensions(){ return m_ROM_extensions; }
  void set_rom_extensions(DWORD addr){ m_ROM_extensions = addr; }

  DWORD entry_rva(){ return m_e32.e32_entryrva; }

  void set_static_ref_flag(){ m_file_attributes |= FILE_ATTRIBUTE_ROMSTATICREF; }

// statics...
private:
  static DWORD s_oem_io_control;
  static DWORD s_cpu_id;
  static bool s_import_success_status;

public:
  static ROMHDR s_romhdr;
  static bool import_success() { 
    // reset before returning for next pass
    // but I think if this fails there won't be a next pass so it shouldn't really matter
    bool ret = s_import_success_status;
    s_import_success_status = true;
     
    return ret; 
  }

  static void print_header();
  static void dump_toc(MemoryList::iterator mem_itr);
  
  static DWORD cpu_id(){ return s_cpu_id; }

// debugging...
public:
  void dump_o32_sections(DWORD base);
  void dump_e32_header();
};

/*****************************************************************************/
inline bool operator==(const Module &b1, const Module &b2){ return b1.name() == b2.name(); }

ostream &operator<<(ostream &out_file, const Module &module);
ostream &operator<<(ostream &out_file, const ModuleList &module_list);

#endif


