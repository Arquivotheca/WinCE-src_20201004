//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "module.h"
#include "..\parser\token.h"

#include "compress.h"
#include "romsig.h"

/*****************************************************************************
  get_map_symbols()

  in:
    bool       return mangled names if profile_all is set
    MemoryList &memory_list
    
  out:
    boolean value indicating success

  desc:
    retrieves all the info for the symbols from the .map file, if it exists
 *****************************************************************************/
bool Module::get_map_symbols(bool profile_all, MemoryList &memory_list){
  string map_file = m_release_path;

  map_file.replace(map_file.rfind('.'), 4, ".map");

  // used to make sure the file exists
  if(GetFileAttributes(map_file.c_str()) == -1)
    return true; // don't care if there is a map file or not

  // open
  ifstream file(map_file.c_str(), ios::in);
  if(file.bad()){
    cerr << "Error: Could not open '" << map_file << "' for reading\n";
    return false;
  }

  string line;

  // skip to start of symbols
  while(!file.eof() && line.find("Publics by Value") == string::npos)
    getline(file, line, '\n');

  // start processing and seaching tokens
  string token;
  while(!file.eof()){
    getline(file, line, '\n');

    // skip lines with no symbol information
    if(line.substr(0, 3) != " 00" || line[5] != ':')
      continue;

    // envvar for backwards compatability
    static string delim = getenv("romimagenotrunc") || profile_all ? " " : " @";
    token = Token::get_token(line, 21, delim);

    
    DWORD sec_number = Token::get_hex_value(line.substr(1, 4)) - 1;
    DWORD sec_offset = Token::get_hex_value(line.substr(6, 8)) + page_size();
    
    if(line.find(" f ") != string::npos){
      Function f;
      f.name = token;
      f.address = Token::get_hex_value(line.substr(6, 8));
      m_profile.m_function_list.push_back(f);
    }
    else if(sec_number >= 0 && sec_number < m_o32_list.size() && !m_o32_list[sec_number].is_writable() && token[0] != '?'){
      Function f;
      f.name = token;
      f.address = Token::get_hex_value(line.substr(6, 8));
      m_profile.m_function_list.push_back(f);
    }
   
    check_special_symbol(token, sec_number, sec_offset, memory_list);
  }

  return true;  
}

/*****************************************************************************
  check_special_symbol()

  in:
    MemoryList &memory_list
    
  out:
    none

  desc:
    looks for certain special symbol names to set state variables
 *****************************************************************************/
void Module::check_special_symbol(string token, DWORD o32_section, DWORD offset, MemoryList &memory_list){
  if(token[0] == '_')
    token.erase(0, 1);

  if(token == "RomExt")
    m_ROM_extensions = memory_iterator()->address() + offset + m_load_offset;

  if(token == "ResetVector")
    m_reset_vector = (DWORD *)(m_o32_list[0].data.ptr() + offset - page_size());

  if(token == "ResetVectorEnd")
    m_reset_vector_end = (DWORD *)(m_o32_list[0].data.ptr() + offset - page_size());
  
  if(token == "ResetVectorAddr")
    m_reset_vector_addr = (DWORD *)(m_o32_list[0].data.ptr() + offset - page_size());
  
  if(token == "KdDebuggerEnabled")
    m_flags |= FLAG_KERNEL_DEBUGGER;

  if(is_kernel()){
    if(token == "pTOC"){
      m_TOC_offset = offset + m_load_offset;  // doesn't get load offset added, because only compared with rva later
      LAST_PASS_PRINT printf("Found pTOC at %08x\n", m_TOC_offset);
    }

    if(needs_signing()){
      if(token == "OEMIoControl")
        s_oem_io_control = offset + m_load_offset - page_size();
    }

    if(o32_section != -1)
      for(MemoryList::iterator mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++)
        if(mem_itr->type() == FIXUPVAR_TYPE)
          if(mem_itr->name() == token){
            static char *debug_show = (debug_show = getenv("ri_debug_info")) ? strstr(debug_show, "fixupvar") : NULL;

            mem_itr->fixupvar_section = o32_section;
            mem_itr->Address::set(offset + m_load_offset - page_size(), mem_itr->length());

            if(debug_show) printf("FixupVar %s is at %08x\n", mem_itr->name().c_str(), mem_itr->address());
          }
  }
}

/*****************************************************************************
  rva2ptr()

  in:
    DWORD relative vertual address to resolve
    
  out:
    DWORD address of data - NULL indicates failure

  desc:
    takes the rva and searches to see which objects range it falls into and 
    then returns a pointer the the apropriate location in the data
 *****************************************************************************/
DWORD Module::rva2ptr(DWORD rva) const{
  for(O32List::const_iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++)
    if((!o32_itr->o32_psize && o32_itr->o32_rva == rva) || 
       (o32_itr->o32_rva <= rva && rva < o32_itr->o32_rva + o32_itr->o32_psize))
      return (DWORD)o32_itr->data.ptr() + rva - o32_itr->o32_rva;

  fprintf(stderr, "Error: Could not find rva %08x in %s\n", rva, m_name.c_str());
  for(DWORD i = 0; i < m_o32_list.size(); i++)
    fprintf(stderr, "o32[%d].o32_rva = %08x (len = %08x)\n", i, m_o32_list[i].o32_rva, m_o32_list[i].o32_psize);

  assert(!"rva2ptr() error");

  return 0;
}

/*****************************************************************************
  nk_fixup_rva()

  in:
    DWORD fixup_addr - address to be fixed up
    DWORD base       - base memory offset
    
  out:
    DWORD - fixed up address or origional if no fixup found

  desc:
    fixes up kernel rva's
 *****************************************************************************/
DWORD Module::fixup_rva_nk(DWORD fixup_addr){
  DWORD orig_fixup_addr = fixup_addr;

  // Actual section starting rva after readonly sections have been moved to start of image
  DWORD next_avail = m_o32_list[0].o32_rva;

  

  fixup_addr = (fixup_addr & 0x00FFFFFF) - (m_vreloc & 0x00FFFFFF);

  for(O32List::iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end() && fixup_addr >= o32_itr->o32_rva + o32_itr->max_size(); o32_itr++){
    // address found
    if((o32_itr + 1) != m_o32_list.end() && fixup_addr < (o32_itr + 1)->o32_rva)
      break;

    if(!o32_itr->is_writable())
      next_avail += align_page(o32_itr->min_size());
  }

  if(o32_itr == m_o32_list.end()){
#if 0   // debug check for sections   
    static bool once = true;

    if(once){
      fprintf(stderr, "Error: Could not find rva %08x in %s\n", fixup_addr, m_name.c_str());
      for(int i = 0; i < m_o32_list.size(); i++)
        fprintf(stderr, "o32[%d].o32_rva = %08x (len = %08x)\n", i, m_o32_list[i].o32_rva, m_o32_list[i].o32_psize);

      once = false;
    }
#endif
    
    fprintf(stderr, "Section not found for %8x fixing up %s\n", fixup_addr, name().c_str());
    return orig_fixup_addr;
  }

  if(!o32_itr->is_writable())
    return orig_fixup_addr - (o32_itr->o32_rva - next_avail);

  if(fixup_addr == m_TOC_offset){
    fprintf(stderr, "NKFixupRVA: target %8x is in section %s offset=%8x - is TOC not changed!\n",
            fixup_addr, m_name.c_str(), o32_itr->o32_rva + o32_itr->o32_dataptr);

    return orig_fixup_addr;
  }

  fixup_addr = fixup_addr - o32_itr->o32_rva + o32_itr->o32_dataptr;

  // Reset top bit to be same as original fixup address
  // Note: this scheme assumes a 1:1 physical to virtual address correspondence, where 0x80000000 <-> 0x00000000
  if(!(orig_fixup_addr & 0x80000000))
    fixup_addr &= 0x7fffffff;

  return fixup_addr;
}

/*****************************************************************************
 *****************************************************************************/
DWORD Module::get_rva_delta(const FixupInfo &kernel_fixup, DWORD rva_delta){
  if(m_o32_list[0].is_writable())
    assert(!"Error: First section of NK must be READONLY!");

  DWORD next_avail = m_o32_list[0].o32_rva; // actual section starting rva after relocate_image() is called
  for(O32List::iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end() && (kernel_fixup.RvaTarget < o32_itr->o32_rva || kernel_fixup.RvaTarget >= o32_itr->o32_rva + o32_itr->o32_vsize); o32_itr++){
    if((o32_itr + 1) != m_o32_list.end() && kernel_fixup.RvaTarget >= o32_itr->o32_rva && kernel_fixup.RvaTarget < (o32_itr + 1)->o32_rva)
      break;

    if(!o32_itr->is_writable())
      next_avail += align_page(o32_itr->min_size());
  }

  if(o32_itr == m_o32_list.end()){
    fprintf(stderr, "Section not found for %8x\n", kernel_fixup.RvaTarget);
    // assert(0); // I don't think a return is apropriate here! but I'm going to let it go for now.
  }

  if(!o32_itr->is_writable()){
    if(o32_itr->o32_rva != next_avail)
      return rva_delta - (o32_itr->o32_rva - next_avail);

    return rva_delta;
  }

  return (DWORD)o32_itr->data.ptr() - m_e32.e32_vbase - o32_itr->o32_rva;
}

/*****************************************************************************
  addr_from_eat()

  in:
    module_list - modules that may be needed for recursive searching
    DWORD   eat - eat address to find
    
  out:
    DWORD - result or zero if not found

  desc:
    get the address from the export address table
 *****************************************************************************/
DWORD Module::addr_from_eat(const ModuleList &module_list, DWORD eat) const{
  const DWORD SECTION_MASK = 0x03F;
  const DWORD VA_SECTION   = 25;
  
  if(eat < m_e32.e32_unit[EXP].rva || eat >= m_e32.e32_unit[EXP].rva + m_e32.e32_unit[EXP].size){
    DWORD ptr = m_vreloc;

    if(ptr & 0x80000000) 
      ptr &= ~(SECTION_MASK << VA_SECTION);

    return eat + ptr;
  }

  string filename = (char *)rva2ptr(eat);
  string str = filename;

  if(filename.find('.')){
    filename.erase(filename.find('.'));
    str.erase(0, str.find('.') + 1);
  }

  for(ModuleList::const_iterator mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++){
    string modname = mod_itr->name();

    modname = modname.substr(0, modname.find('.'));

    if(modname == filename)
      if(str[0] == '@')
        return mod_itr->resolve_imp_ord(module_list, atoi(str.c_str() + 1));
      else
        return mod_itr->resolve_imp_str(module_list, str);
  }

  cerr << "Error: Unresolved external: " << filename << endl;

  return 0;
}

/*****************************************************************************
  resolve_imp_ord()

  in:
    module_list - modules that may be needed for recursive searching
    DWORD   ord - ordinal to find the address for
    
  out:
    DWORD - address of function for ordinal or NULL for failure

  desc:
    find the address of a function specified by ordinal number
 *****************************************************************************/
DWORD Module::resolve_imp_ord(const ModuleList &module_list, DWORD ord) const{
  // no rva nothing to resolve
  if(!m_e32.e32_unit[EXP].rva)
    return 0;

  ExpHdr *expptr = (ExpHdr *)rva2ptr(m_e32.e32_unit[EXP].rva);
  DWORD *eatptr = (DWORD *)rva2ptr(expptr->exp_eat);

  DWORD hint = ord - expptr->exp_ordbase;
  DWORD ret = 0;
  
  if(hint <= expptr->exp_eatcnt)
    ret = addr_from_eat(module_list, eatptr[hint]);

  if(!ret){
    cerr << "Error: Can't find import " << ord << " in " << m_name << endl;
  }

  return ret;
}

/*****************************************************************************
  resolve_imp_str()

  in:
    module_list - modules that may be needed for recursive searching
    string str  - name of function to resolve
    
  out:
    DWORD - address of function named, NULL for failure

  desc:
    resolve a funciton by name
 *****************************************************************************/
DWORD Module::resolve_imp_str(const ModuleList &module_list, const string &str) const{
  // no rva nothing to resolve
  if(!m_e32.e32_unit[EXP].rva)
    return 0;

  ExpHdr *expptr = (ExpHdr *)rva2ptr(m_e32.e32_unit[EXP].rva);
  char **nameptr = (char **)rva2ptr(expptr->exp_name);
  DWORD *eatptr = (DWORD *)rva2ptr(expptr->exp_eat);
  WORD *ordptr = (WORD *)rva2ptr(expptr->exp_ordinal);

  for(unsigned i = 0; i < expptr->exp_namecnt; i++)
    if(str == (char *)rva2ptr((DWORD)nameptr[i]))
      break;
  
  // found it
  if(i != expptr->exp_namecnt)
    return addr_from_eat(module_list, eatptr[ordptr[i]]);
  
  cerr << "Error: Can't find import " << str << " in " << m_name << endl;

  return 0;
}

/*****************************************************************************
  resolve_imp_hint_ord()

  in:
    module_list - modules that may be needed for recursive searching
    DWORD hint  - type of string provided for the hint
    string str  - using in cojunction with a hint to determin function resolution
    
  out:
    DWORD - address of function, NULL indicates failure

  desc:
    resolve a function address from a hint string
 *****************************************************************************/
DWORD Module::resolve_imp_hint_str(const ModuleList &module_list, DWORD hint, const string &str) const{
  // no rva nothing to resolve
  if(!m_e32.e32_unit[EXP].rva)
    return 0;

  ExpHdr *expptr = (ExpHdr *)rva2ptr(m_e32.e32_unit[EXP].rva);
  DWORD *eatptr = (DWORD *)rva2ptr(expptr->exp_eat);
  WORD *ordptr = (WORD *)rva2ptr(expptr->exp_ordinal);
  char **nameptr = (char **)rva2ptr(expptr->exp_name);
  DWORD ret = 0;

  if(hint >= expptr->exp_namecnt || str != (char *)rva2ptr((DWORD)nameptr[hint]))
    ret = resolve_imp_str(module_list, str);
  else
    ret = addr_from_eat(module_list, eatptr[ordptr[hint]]);

  if(!ret)
    cerr << "Error: Can't find import " << str << " hint " << hint << " in " << m_name << endl;
  
  return ret;
}

/*****************************************************************************
  truncate_o32()

  in:
    none
    
  out:
    boolean value indicating success

  desc:
    truncate a module's o32 data section
 *****************************************************************************/
DWORD Module::truncate_o32(O32 &o32){
  DWORD length = o32.min_size();

  // truncate the data
  BYTE *ptr = (BYTE *)rva2ptr(o32.o32_rva) + length;
  while(length && !*--ptr)
    length--;

  o32.o32_psize = length;

  return o32.o32_psize;
}

/*****************************************************************************
  compress_o32()

  in:
    none
    
  out:
    boolean value indicating success

  desc:
    compress a module's o32 data section
 *****************************************************************************/
DWORD Module::compress_o32(O32 &o32){
  if(o32.is_writable())
    truncate_o32(o32);

  if(!o32.min_size())
    return 0; // skip furthur processing since there is nothing to compress

  // compress the section
  Data compressed_data;
  DWORD compressed_size;

  compressed_size = m_compressor->cecompress((BYTE *)rva2ptr(o32.o32_rva), 
                                             o32.min_size(), 
                                             compressed_data.user_fill(o32.min_size()), 
                                             o32.min_size() - 1, 
                                             1, 
                                             page_size());

  if(compressed_size == -1) // failure, just make sure it's marked as aligned
    o32.o32_flags |= IMAGE_SCN_ALIGN_4BYTES;  // flag is ignored in rest of romimage
  else if(!compressed_size) // compressed to all zeros
    o32.o32_psize = 0;
  else{ // compression worked
    o32.data.set(compressed_data.ptr(), compressed_size); // chose not to use = operator to allow for explicit resize
    o32.o32_flags |= IMAGE_SCN_COMPRESSED;
    o32.o32_psize = compressed_size;
  }

  return o32.o32_psize;
}

/*****************************************************************************
  Module()

  in:
    varies
    
  out:
    none

  desc:
    initializes the class variables
 *****************************************************************************/
Module::Module(const string &file):File(file){
  memset(&m_e32, 0, sizeof(m_e32));
  m_o32_list.clear();

  m_profile.m_function_list.clear();
  memset(&m_profile, 0, sizeof(m_profile));
  
  m_kernel_fixup_list.clear();

  m_file_attributes = FILE_ATTRIBUTE_READONLY;

  set_code_split(true);
  set_module();

  // information for relocation
  m_e32_offset = 0;
  m_o32_offset = 0;
  m_vreloc  = 0;

  // map symbol information
  m_TOC_offset        = 0;
  m_ROM_extensions    = 0;
  m_reset_vector      = 0;
  m_reset_vector_end  = 0;
  m_reset_vector_addr = 0;
}

/*****************************************************************************
  init_kernel()

  in:
    fixup - wether or not we want to fix up the kernel

  out:
    none
    
  desc:
    sets some flags dealing with witch module is the kernel
 *****************************************************************************/
bool Module::init_kernel(bool fixup, const Memory &ram_section){
  if(name() == "nk.exe"){
    m_flags |= FLAG_KERNEL;
    if(fixup) 
      m_flags |= FLAG_KERNEL_FIXUP;

    memory_iterator()->init_kernel();
  }
  else{
    cerr << "Warning: No kernel module found" << endl;
  }
 
  assert(ram_section.type() == RAM_TYPE);

  memset(&s_romhdr, 0, sizeof(s_romhdr));
  s_romhdr.ulRAMStart = ram_section.address();
  s_romhdr.ulRAMFree  = ram_section.address();
  s_romhdr.ulRAMEnd   = ram_section.address() + ram_section.length();
  
  return s_romhdr.ulRAMStart != 0 || s_romhdr.ulRAMEnd != 0;
}

/*****************************************************************************
  sync_names()

  in:
    none

  out:
    boolean value indicating success
    
  desc:
    check if path name and file name are different: 
    
      m_name = nk.bin, m_release_path = d:\nk32.bin
      optional m_build_path = public\projects\nkexe\SH3_rel\
      
    if so, copy nk32.bin to nk.bin and copy the pdb and map file also.
 *****************************************************************************/
bool Module::sync_names(bool profile, bool copy_files){
  // check to see if the names differ
  if(is_kernel() && profile){
    m_release_path = lowercase(m_release_path);
    
    if(m_release_path.find("nk.exe") != string::npos ||
       m_release_path.find("nknodbg.exe") != string::npos)
      m_release_path = m_release_path.substr(0, m_release_path.rfind("\\") + 1) + "nkprof.exe";
  }
  
  if(!copy_files)
    return true;
  
  if(m_name != m_release_path.substr(m_release_path.rfind("\\") + 1)){
    string t_to = m_release_path.substr(0, m_release_path.rfind("\\") + 1) + m_name;  // replace with short name
    string t_from = m_release_path;

    cout << "Copying " << t_from << " to " << t_to << " for debugger\n";
    
    m_release_path = t_to;  // update the qualified path with the new name
    if(!CopyFile(t_from.c_str(), t_to.c_str(), false)){
      cerr << "Error: Failed copying " << t_from << " to " << t_to << endl;
      return false;
    }

    t_to.replace(t_to.rfind('.'), 4, ".pdb");
    t_from.replace(t_from.rfind('.'), 4, ".pdb");
    if(t_from != t_to && !CopyFile(t_from.c_str(), t_to.c_str(), false)){
//      cerr << "Warning: Failed copying " << t_from << " to " << t_to << endl;
    }

    t_to.replace(t_to.rfind('.'), 4, ".map");
    t_from.replace(t_from.rfind('.'), 4, ".map");
    if(t_from != t_to && !CopyFile(t_from.c_str(), t_to.c_str(), false)){
//      cerr << "Warning: Failed copying " << t_from << " to " << t_to << endl;
    }

    t_to.replace(t_to.rfind('.'), 4, ".rel");
    t_from.replace(t_from.rfind('.'), 4, ".rel");
    if(t_from != t_to && !CopyFile(t_from.c_str(), t_to.c_str(), false)){
//      cerr << "Warning: Failed copying " << t_from << " to " << t_to << endl;
    }
  }

  if(fixup_like_kernel()){
    string reloc_file = m_release_path;  // replace with short name
    reloc_file.replace(reloc_file.rfind('.'), 4, ".rel");
    
    if(GetFileAttributes(reloc_file.c_str()) == -1){
      cerr << "Error: Module " << m_name << " reqested kernel fixup and couldn't find required .rel file\n";
      return false;
    }
  }    

  return true;
}

/*****************************************************************************
  add_sig_files()

  in:

  out:

  desc:
 *****************************************************************************/

bool Module::add_sig_files(FileList &file_list){
  if(!needs_signing())
    return true;

  // add a signature file for this module, named in the form
  // "module-suffix.sig"
  string sig_name = name();
  string release_path;
  int pos = sig_name.rfind('.');

  if(pos != string::npos)
    sig_name[pos] = '-';
  
  sig_name += ".sig";
  release_path = m_release_path.substr(0, m_release_path.rfind("\\") + 1) + sig_name;

  if(GetFileAttributes(release_path.c_str()) == -1){
    ofstream sig_file(release_path.c_str(), ios::trunc);
    if(sig_file.bad()){
      cerr << "Error: Could not open '" << release_path << "' for writing\n";
      return false;
    }
  
    sig_file << "This is a place holder for sig info";
    sig_file.flush();
    sig_file.close();
  }

  File file;

  if(!file.set(sig_name, release_path, FILE_ATTRIBUTE_READONLY, memory_iterator(), m_compressor))
    return false;

  file_list.push_back(file);

  m_signature_file = file_list.end();
  m_signature_file--;

  return true;
}

/*****************************************************************************
  load()

  in:
    none

  out:
    boolean value indicating success

  desc:
    reads in the e32 structure, the o32 structures, the o32 data areas (if 
    any), and possible debuf fixup tables if we're the kernel

    does a couple basic checks on the data also
 *****************************************************************************/
bool Module::load(){
  DWORD e32_ptr;
  DWORD debug_offset;
  
  ifstream file(m_release_path.c_str(), ios::in | ios::binary);
  if(file.bad()){
    cerr << "Error: Could not open '" << m_release_path << "' for reading\n";
    return false;
  }

  file.seekg(0x3c); // e32 pointer offset
  file.read((char *)&e32_ptr, sizeof(e32_ptr));
  if(file.fail()){
    cerr << "Error: Failed reading e32 pointer in module " << m_name << endl;
    return false;
  }

  file.seekg(e32_ptr);
  file.read((char *)&m_e32, sizeof(m_e32));
  if(file.fail()){
    cerr << "Error: Failed reading e32 structure in module " << m_name << endl;
    return false;
  }

  if(!m_e32.e32_objalign){
    cerr << "Warning: Page Size is zero, forcing to " << s_page_size << " in module " << m_name << endl;
    m_e32.e32_objalign = s_page_size;
  }

  if(m_e32.e32_objalign != s_page_size){
    cerr << "Error: only 4k page alignments supported at this time, found in module " << m_name << endl;
    return false;
  }

  if(!m_e32.e32_vsize){
    cerr << "Error: Invalid size specification (e32_vsize == 0) in module " << m_name << endl;
    return false;
  }

  // might as well be thorough
  if(m_e32.e32_magic[0] != 'P'  ||
     m_e32.e32_magic[1] != 'E'  ||
     m_e32.e32_magic[2] != '\0' ||
     m_e32.e32_magic[3] != '\0' ){
    cerr << "Error: Image signature invalid: found '" << m_e32.e32_magic 
         << "' where '" << "PE" << "' expected in module " << m_name << endl;
    return false;
  }

  bool resource_only = true;

  // read all objects
  for(int i = 0; i < m_e32.e32_objcnt; i++){
    O32 o32;
    
    file.seekg(e32_ptr + sizeof(m_e32) + i * sizeof(o32_obj));
    file.read((char *)&o32, sizeof(o32_obj));
    if(file.fail()){
      cerr << "Error: Failed reading o32 structure in module " << m_name << endl;
      return false;
    }

    if(o32.o32_rva <= m_e32.e32_unit[DEB].rva && m_e32.e32_unit[DEB].rva < o32.o32_rva + o32.o32_vsize)
      debug_offset = o32.o32_dataptr - o32.o32_rva + m_e32.e32_unit[DEB].rva;

    if(o32.o32_dataptr && o32.o32_psize){
      file.seekg(o32.o32_dataptr);
      file.read((char *)o32.data.user_fill(o32.o32_psize), o32.o32_psize);
      if(file.fail()){
        cerr << "Error: Failed reading o32 data in module " << m_name << endl;
        return false;
      }
    }

    if(o32.name() == ".rsrc")
      o32.o32_flags &= ~IMAGE_SCN_MEM_WRITE;
    else
      resource_only = false;

    if(m_flags & IMAGE_SCN_MEM_NOT_PAGED)
      o32.o32_flags |= IMAGE_SCN_MEM_NOT_PAGED;

    m_o32_list.push_back(o32);
  }

  if(resource_only){
    fprintf(stderr, "Found a resource only dll %s\n", m_name.c_str());
    m_flags |= FLAG_RESOURCE_ONLY;
  }

  // read in debugging info for kernel fixups, if appropriate
  if(!s_cpu_id){
    s_cpu_id = m_e32.e32_cpu;
    s_page_size = page_size();
  }

  // check for pb_private_build in comment area
  {
    Data temp;

    // skip past all o32 sections
    DWORD offset = e32_ptr + sizeof(m_e32) + m_o32_list.size() * sizeof(o32_obj); 
    file.seekg(offset);

    // read whole block
    DWORD size = m_o32_list[0].o32_dataptr - offset;

    if(size){
      file.read((char*)temp.user_fill(size), size);
      if(file.fail()){
        cerr << "Error: Failed reading comment structure" << m_name << endl;
        return false;
      }
  
      if(strstr((const char *)temp.ptr(), "pb_private_source"))
        m_flags |= FLAG_WARN_PRIVATE_BITS;
    }
  }

  if(is_kernel()){
    // read in any kernel debug info if doing fixups
    if(is_kernel_fixup_enabled() && m_e32.e32_unit[DEB].size){
      IMAGE_DEBUG_DIRECTORY debug_dir = {0};
      int debug_count = m_e32.e32_unit[DEB].size / sizeof(debug_dir);

      // search for a fixup type debug dir
      file.seekg(debug_offset);
      for(int i = 0; i < debug_count && debug_dir.Type != IMAGE_DEBUG_TYPE_FIXUP; i++){
        file.read((char *)&debug_dir, sizeof(debug_dir));
        if(file.fail()){
          cerr << "Error: Failed reading debug data in module " << m_name << endl;
          return false;
        }
      }
      
      if(debug_dir.Type != IMAGE_DEBUG_TYPE_FIXUP)
        // cerr << "Warning: NK image debug type fixups not found, using standard fixups\n";
        ;
      else{
        // read in fixup data
        FixupInfo fixup;

        file.seekg(debug_dir.PointerToRawData);
        for(unsigned i = 0; i < debug_dir.SizeOfData / sizeof(fixup); i++){
          file.read((char *)&fixup, sizeof(fixup));
          if(file.fail()){
            cerr << "Error: Failed reading fixup data in module " << m_name << endl;
            return false;
          }

          m_kernel_fixup_list.push_back(fixup);
        }
      }
    }
  }

  return true;
}

/*****************************************************************************
  set_load_address()

  in:
    DWORD addr - address to load module in memory

  out:
    none

  desc:
    sets the load address and also the e32 and o32 table load offsets
 *****************************************************************************/
void Module::set_load_address(DWORD addr){
  m_load_offset = addr;

  DWORD size = page_size();

  // Compute size of sections
  for(O32List::const_iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++)
    size += align_page(o32_itr->max_size());

  // add in size of e32
  m_e32_offset = m_load_offset + size;
  size += sizeof(m_e32);

  // add in size of o32's
  m_o32_offset = m_load_offset + size;
}

/*****************************************************************************
  get_load_size()

  in:
    none

  out:
    DWORD - size of module

  desc:
    returns module size as read from disk (ie. uncompressed)
 *****************************************************************************/
DWORD Module::get_load_size(){
  DWORD size = page_size();

  // Compute size of sections
  for(O32List::const_iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++)
    size += align_page(o32_itr->max_size());  

  // add in size of e32
  size += sizeof(m_e32);
  
  // add in size of o32's
  size += sizeof(o32_obj) * m_e32.e32_objcnt;

  // round up to next page
  return align_page(size); 
}

/*****************************************************************************
  set_base()

  in:
    MemoryList - list of reserved regions for kernel fixed up modules to look out for

  out:
    bool - invalid address

  desc:
    sets the relocation location for the module with a small check for 
    stripped kernels

    dll    - to next 64k aligned address DOWN!
    kernel - to specified address
    exe... if stripped  - doesn't move
           not stripped - moves to 0x10000
 *****************************************************************************/
bool Module::set_base(const MemoryList &reserve_list){
  static DWORD kernel_readonly_offset = 0;

  // reinitialize some pass specific variables
  if(is_kernel()){
    kernel_readonly_offset = 0;
  }    

//  if(is_resource_only())
//    return true;

  if(memory_iterator()->code_space_full)
    set_code_split(false);
  
  // give me some shorter names for all the references below
  DWORD &code_addr = memory_iterator()->dll_code_start;
  DWORD &data_addr = memory_iterator()->dll_data_start;
      
  if(is_dll() && !fixup_like_kernel()){
    if(is_code_split()){
      // if we're going, old we can't go back
      assert(!memory_iterator()->dll_data_split);
      
      DWORD code_orig = code_addr;
      DWORD data_orig = data_addr;
  
      data_addr -= page_size();
  
      for(O32List::const_iterator c_o32_itr = m_o32_list.begin(); c_o32_itr != m_o32_list.end(); c_o32_itr++){
        if(c_o32_itr->is_writable() && !c_o32_itr->is_shared())
          data_addr -= align_page(c_o32_itr->max_size());
  
        code_addr -= align_page(c_o32_itr->max_size());
      }
  
      code_addr -= align_page(sizeof(e32_exe) + sizeof(o32_obj) * m_e32.e32_objcnt);
      
      data_addr &= 0xfffff000; // round DOWN to next PAGE boundry
      code_addr &= 0xffff0000; // round DOWN to next 64k boundry

      if(code_addr < memory_iterator()->dll_code_bottom){
        code_addr = code_orig;
        data_addr = data_orig;

        set_code_split(false);
        memory_iterator()->code_space_full = true;

        goto OLD_FIXUP;
      }
  
      DWORD data = data_addr + page_size();
      DWORD code = code_addr + page_size();
  
      for(O32List::iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++)
        if(o32_itr->is_writable() && !o32_itr->is_shared()){
          o32_itr->o32_realaddr = data;
          data += align_page(o32_itr->max_size());
          code += align_page(o32_itr->max_size());
        }
        else{
          o32_itr->o32_realaddr = code;
          code += align_page(o32_itr->max_size());
        }
    }
    else{
OLD_FIXUP:
      if(memory_iterator()->code_space_full){
        cout << "Code space full, fixing up " << m_name << " to ram space\n";
      }

      // add in gaps for first low module and set split address
      if(!memory_iterator()->dll_data_split){
        data_addr -= memory_iterator()->dll_data_gap;

        data_addr &= 0xffff0000;      // round DOWN to next 64k boundry 
        memory_iterator()->dll_data_split = data_addr;

        data_addr -= memory_iterator()->dll_gap;  
      }
        
      data_addr -= get_load_size() - page_size();
      data_addr &= 0xffff0000;      // round DOWN to next 64k boundry

      m_vreloc = data_addr;
    }
  }
  else if(is_kernel()){
    m_vreloc = memory_iterator()->address();

    kernel_readonly_offset = page_size();
    for(unsigned i = 0; i < m_o32_list.size(); i++)
      if(!m_o32_list[i].is_writable() && !m_o32_list[i].is_discardable())
        kernel_readonly_offset += align_page(m_o32_list[i].min_size());
  }
  else if(fixup_like_kernel()){
    m_vreloc = memory_iterator()->address() + kernel_readonly_offset;

    kernel_readonly_offset += page_size();
    for(O32List::const_iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++)
      if(!o32_itr->is_writable() && !o32_itr->is_discardable())
        kernel_readonly_offset += align_page(o32_itr->min_size());

    DWORD size = kernel_readonly_offset - (m_vreloc - memory_iterator()->address());
    DWORD temp = align_page(Memory::find_next_gap(reserve_list, m_vreloc, size));

    if(m_vreloc != temp){
      m_vreloc = temp;
      kernel_readonly_offset = temp + size;
    }
  }
  else if(m_e32.e32_imageflags & IMAGE_FILE_RELOCS_STRIPPED)
    m_vreloc =  m_e32.e32_vbase;
  else
    m_vreloc = 0x10000;

  if(m_vreloc)
    for(O32List::iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++)
      o32_itr->o32_realaddr = m_vreloc + o32_itr->o32_rva;

  // checking to make sure a stripped kernel is in some arbitrary range
  if(is_kernel() && m_e32.e32_imageflags & IMAGE_FILE_RELOCS_STRIPPED && 
     (m_vreloc < 0x10000 || m_vreloc > 0x400000)){
    cerr << "Error: Module " << m_name << " relocations stripped, remove -fixed from link command\n";
    return false;
  }

  return true;
}

/*****************************************************************************
  verify_cpu()

  in:
    none

  out:
    bool - indicating good status

  desc:
    checks to make sure the the cpu the module was compiled for is compatible
    with that which the kernel was compiled with
 *****************************************************************************/
bool Module::verify_cpu() const{
  if(ignore_cpu())
    return true;

  // Allow ARM & Strongarm to mix, even though on SA1100 it won't work.  Even though we can't
  // detect this, the kernel will detect it and fail to load something that he can't handle.
  if(m_e32.e32_cpu != s_cpu_id &&
     !m_e32.e32_unit[RS4].rva &&
     !(m_e32.e32_cpu == IMAGE_FILE_MACHINE_SH3   && s_cpu_id == IMAGE_FILE_MACHINE_SH3DSP) &&
     !(m_e32.e32_cpu == IMAGE_FILE_MACHINE_SH3   && s_cpu_id == IMAGE_FILE_MACHINE_SH4   ) &&
     !(m_e32.e32_cpu == IMAGE_FILE_MACHINE_R4000 && s_cpu_id == IMAGE_FILE_MACHINE_MIPS16) &&
     !(m_e32.e32_cpu == IMAGE_FILE_MACHINE_ARM   && s_cpu_id == IMAGE_FILE_MACHINE_THUMB )
     ){

    map<DWORD, string> cpu_id_list;

    cpu_id_list[IMAGE_FILE_MACHINE_I386]    = "I386";
    cpu_id_list[IMAGE_FILE_MACHINE_R3000]   = "R3000";
    cpu_id_list[IMAGE_FILE_MACHINE_R4000]   = "R4000";
    cpu_id_list[IMAGE_FILE_MACHINE_MIPS16]  = "mips";
    cpu_id_list[IMAGE_FILE_MACHINE_ALPHA]   = "ALPHA";
    cpu_id_list[IMAGE_FILE_MACHINE_SH3]     = "SH3";
    cpu_id_list[IMAGE_FILE_MACHINE_SH4]     = "SH4";
    cpu_id_list[IMAGE_FILE_MACHINE_POWERPC] = "PowerPC";
    cpu_id_list[IMAGE_FILE_MACHINE_ARM]     = "ARM";
    cpu_id_list[IMAGE_FILE_MACHINE_THUMB]   = "Thumb";
    cpu_id_list[IMAGE_FILE_MACHINE_UNKNOWN] = "Unknown";
     
    cerr << "Error: Module " << m_name << " built for " << cpu_id_list[m_e32.e32_cpu]
         << ", kernel built for " << cpu_id_list[s_cpu_id] << endl;
    
    return false;
  }

  return true;
}

/*****************************************************************************
  get_symbols()
  
  in:
    none
    
  out:
    bool - value indicating success

  desc:
    Load the symbol information
 *****************************************************************************/
bool Module::get_symbols(bool profile_all, MemoryList &memory_list){
  if(!m_e32.e32_symcount) // mo symbols in module, use map file
    return get_map_symbols(profile_all, memory_list);

  // open the file
  ifstream file(m_release_path.c_str(), ios::in | ios::binary);
  if(file.bad()){
    cerr << "Error: Could not open '" << m_release_path << "' for reading\n";
    return false;
  }

  // seek to the start of the symbol table
  file.seekg(m_e32.e32_symtaboff);

  // read symbol table
  SymbolList symbol_list;
  for(unsigned i = 0; i < m_e32.e32_symcount; i++){
    COFF_Symbol coff_sym;

    file.read((char *)&coff_sym, sizeof(coff_sym));
    if(file.fail()){
      cerr << "Error: Failed reading COFF symbol structure in module " << m_name << endl;
      return false;
    }

    symbol_list.push_back(coff_sym);
  }

  // read the length of the string table (imediatly follows the symbol table)
  DWORD string_table_len = 0;
  Data string_table;
  file.read((char *)&string_table_len, sizeof(string_table_len));
  if(file.fail()){
    cerr << "Error: Failed reading string table length in module " << m_name << endl;
    return false;
  }

  // read the table itself (imediatly follows the length)
  if(string_table_len){
    file.read((char *)string_table.user_fill(string_table_len), string_table_len);
    if(file.fail()){
      cerr << "Error: Failed reading string table in module " << m_name << endl;
      return false;
    }
  }

  file.close();

  // check each symbol against the string table names
  for(i = 0; i < symbol_list.size(); i++){
    if(symbol_list[i].usType == 0x20 ||
       (profile_all && symbol_list[i].ulValue >= m_e32.e32_codebase && symbol_list[i].ulValue < m_e32.e32_database)){
      string buffer;

      // get the name either from the Coff symbol or string table lookup
      if(symbol_list[i].dwNULL != NULL)
        buffer.assign((char *)symbol_list[i].szName, 8);
      else{
        assert(!string_table.empty());
        buffer = (char *)string_table.ptr() + symbol_list[i].dwPosLow - 4;
      }

      // skip strings with .'s in them
      if(buffer.find('.') != string::npos)
        continue;

      // if NOT profiling all then skip lines with a $ and lines beginning with L#
      if(!profile_all)
        if(buffer.find('$') != string::npos || (buffer[0] == 'L' && isdigit((unsigned char)buffer[1])))
          continue;

      // do this to whatever gets through the previous checks
      Function f;
      f.name = buffer;
      f.address = symbol_list[i].ulValue;
      m_profile.m_function_list.push_back(f);      
    }
  }       

  // sorts them in address order (as if they had been read from a map file)
  sort(m_profile.m_function_list.begin(), m_profile.m_function_list.end());

  
  for(i = 0; i < m_profile.m_function_list.size(); i++){
    
  }

  // search the symbol list against special symbols
  for(i = 0; i < symbol_list.size(); i++){
    
    if(symbol_list[i].usType != 0x20 && symbol_list[i].cClass == 2){
      string buffer;
      
      // get the name either from the Coff symbol or string table lookup
      if(symbol_list[i].dwNULL != NULL)
        buffer.assign((char *)symbol_list[i].szName, 8);
      else{
        assert(!string_table.empty());
        buffer = (char *)string_table.ptr() + symbol_list[i].dwPosLow - 4;
      }

      check_special_symbol(buffer, -1, symbol_list[i].ulValue, memory_list);
    }
  }  

  return true;
}

/*****************************************************************************
  relocate_image()

  in:
    none
    
  out:
    boolean value indicating success

  desc:
    relocates module load and bin file locations
 *****************************************************************************/
bool Module::relocate_image(){
  DWORD offset = page_size();

  for(O32List::iterator itr = m_o32_list.begin(); itr != m_o32_list.end(); itr++){
    // set section header data pointer
    itr->o32_dataptr = memory_iterator()->address() + m_load_offset + offset;
    
    offset += align_page(itr->max_size());

    if((is_kernel() || fixup_like_kernel()) && is_kernel_fixup_enabled() && itr->is_writable()){
      if(itr->name() == ".KDATA"){
        DWORD aligned_free;
        
        if(is_arm())
          aligned_free = align_64k(Module::s_romhdr.ulRAMFree);
        else
          aligned_free = align_16k(Module::s_romhdr.ulRAMFree);

        if(aligned_free != Module::s_romhdr.ulRAMFree)
          Module::s_romhdr.ulRAMFree = aligned_free;
      }

      itr->o32_dataptr = Module::s_romhdr.ulRAMFree;

      Module::s_romhdr.ulRAMFree += align_page(itr->max_size());
    }
  }

  return true;
}

/*****************************************************************************
  fixup()

  in:
    none
    
  out:
    boolean value indicating success

  desc:
    fix up tables for relocated module
 *****************************************************************************/
bool Module::fixup(){
  // kernel is special
  if(is_kernel() && is_kernel_fixup_enabled() && m_kernel_fixup_list.size())
    return fixup_nk();
  
  // no relocations
  if(!m_e32.e32_unit[FIX].size){
    if(is_kernel() && is_kernel_fixup_enabled())
      cerr << "Warning: No fixup information for NK.  NK linkr command /FIXED:NO may be required\n";

    return true;
  }

  if(fixup_like_kernel() && !is_code_split()){
    cerr << "Error: Module " << m_name << " reqested kernel fixup but has an invalid .rel file or"
         << " some other condition that prevents the code section from being split\n";
    
    exit(1);
  }

  static char *debug_show = (debug_show = getenv("ri_debug_info")) ? strstr(debug_show, "fixups") : NULL;
  
  if(debug_show) printf("Fixups for %s\n", m_name.c_str());

  if(!is_code_split())
    return fixup_no_split();

  // setup relocation filename
  string reloc_file = m_release_path;
  reloc_file.replace(reloc_file.rfind('.'), 4, ".rel");
  if(GetFileAttributes(reloc_file.c_str()) == -1)
    return false;

  ifstream file(reloc_file.c_str(), ios::in);
  if(file.bad()){
    cerr << "Error: Could not open '" << reloc_file << "' for reading\n";
    return false;
  }

  string line;
  DWORD time_stamp;

  getline(file, line, '\n');
  if(sscanf(line.c_str(), "Timestamp: %08X", &time_stamp) != 1 || time_stamp != m_e32.e32_timestamp){
    if(is_kernel() || fixup_like_kernel()){
      cerr << "Error: Mismatched time stamp on .rel file for module " << m_name << " requesting kernel fixup.  Valid .rel file is required for kernel fixups.\n";
      exit(1);
    }
    
    cerr << "Warning: Mismatched time stamp on .rel file for module " << m_name << endl;
    return false;
  }

  while(!file.eof()){
    WORD  reloc_type;
    WORD  reloc_section;
    DWORD reloc_offset;
    DWORD reloc_lowaddr;

    getline(file, line, '\n');
    if(sscanf(line.c_str(), "%04X %04X %08X %08X", &reloc_type, &reloc_section, &reloc_offset, &reloc_lowaddr) != 4)
      continue;

    if(debug_show) printf("%s ", line.c_str());
  
    assert(reloc_section >= 1 && reloc_section <= m_o32_list.size());

    reloc_section--; // I'm zero based

    O32List::iterator o32_itr = m_o32_list.end();
    
    if(o32_itr == m_o32_list.end() || 
       o32_itr->o32_rva > reloc_offset ||
       o32_itr->o32_rva + o32_itr->o32_vsize <= reloc_offset){

      for(o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++)
        if(o32_itr->o32_rva <= reloc_offset &&
           o32_itr->o32_rva + o32_itr->o32_vsize > reloc_offset)
          break;

      assert(o32_itr != m_o32_list.end());
    }

    DWORD offset = m_o32_list[reloc_section].o32_realaddr - m_e32.e32_vbase - m_o32_list[reloc_section].o32_rva;
    DWORD *fixup_addr = (DWORD*)(reloc_offset - o32_itr->o32_rva + (DWORD)o32_itr->data.ptr());

    apply_fixup(reloc_type, fixup_addr, offset, (WORD)reloc_lowaddr);
  }

  return true;
}

/*****************************************************************************
  do_fixups()

  in:
    none
    
  out:
    boolean value indicating success

  desc:
    fix up tables for relocated module
 *****************************************************************************/
bool Module::fixup_no_split(){
  assert(!fixup_like_kernel());
  
  DWORD offset = m_vreloc - m_e32.e32_vbase;

  // don't really have any clue about what is going on here.  Just following the source code.
  info *block_ptr = (info*)rva2ptr(m_e32.e32_unit[FIX].rva);
  for(info *block_start = block_ptr; 
      (DWORD)block_ptr < (DWORD)block_start + m_e32.e32_unit[FIX].size && block_ptr->size;
      block_ptr = (info*)((DWORD)block_ptr + block_ptr->size)){

    O32List::iterator o32_itr = m_o32_list.end();
    
    for(WORD *current_ptr = (WORD*)((DWORD)block_ptr + sizeof(info)); 
        (DWORD)current_ptr < (DWORD)block_ptr + block_ptr->size; 
        current_ptr++){

      WORD current_offset = *current_ptr & 0xfff;

      // nothing here to fix
      if(!current_offset && !block_ptr->rva){
        current_ptr++;
        continue;
      }

      if(o32_itr == m_o32_list.end() || 
         o32_itr->o32_rva > block_ptr->rva + current_offset ||
         o32_itr->o32_rva + o32_itr->o32_vsize <= block_ptr->rva + current_offset){

        for(o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++)
          if(o32_itr->o32_rva <= block_ptr->rva + current_offset &&
             o32_itr->o32_rva + o32_itr->o32_vsize > block_ptr->rva + current_offset)
            break;

        assert(o32_itr != m_o32_list.end());
      }

      DWORD *fixup_addr = (DWORD*)(block_ptr->rva - o32_itr->o32_rva + current_offset + (DWORD)o32_itr->data.ptr());
      WORD   fixup_type = (*current_ptr >> 12) & 0xf;

      if(fixup_type == IMAGE_REL_BASED_HIGHADJ)
        ++current_ptr;

      static char *debug_show = (debug_show = getenv("ri_debug_info")) ? strstr(debug_show, "fixups") : NULL;
      
      if(debug_show) printf("%04x %04x %08x %08x ", fixup_type, 1, block_ptr->rva + current_offset, fixup_type == IMAGE_REL_BASED_HIGHADJ ? *current_ptr : 0);

      apply_fixup(fixup_type, fixup_addr, offset, LOWORD(*current_ptr));
    }
  }

  return true;
}
  
/*****************************************************************************/
bool Module::apply_fixup(WORD type, DWORD *addr, DWORD offset, WORD low_addr){
  static WORD *fixup_addr_hi = NULL;
  static bool matched_ref_lo = false;

  struct Type4Fixup{ 
    DWORD addr;
    WORD  addr_lo;
  };

  const DWORD MAX_TYPE4_NEST = 16;
  static DWORD num_type4 = 0;
  static Type4Fixup type4_fixup[MAX_TYPE4_NEST];

  DWORD value;
  static char *debug_show = (debug_show = getenv("ri_debug_info")) ? strstr(debug_show, "fixups") : NULL;
  
  switch(type){
    // MIPS relocation types:

    // No location is necessary, reference is to an absolute value
    case IMAGE_REL_BASED_ABSOLUTE:
      break;

    // Reference to the upper 16 bits of a 32-bit address.
    // Save the address and go to get REF_LO paired with this one
    case IMAGE_REL_BASED_HIGH:
      fixup_addr_hi = (WORD*)addr;
      matched_ref_lo = true;
      break;

    // Direct reference to a 32-bit address. With RISC
    // architecture, such an address cannot fit into a single
    // instruction, so this reference is likely pointer data
    // Low - (16-bit) relocate high part too.
    case IMAGE_REL_BASED_LOW:
      if(debug_show) printf("fixup %08x -> ", *addr);

      if((is_kernel() || fixup_like_kernel()) && num_type4){
        for(unsigned i = 0; i < num_type4; i++)
          if(*(WORD *)addr == type4_fixup[i].addr_lo)
            break;

        if(i >= num_type4)
          assert(!"Error: BASED_LOW fixup nested too deap!");
        
        value = type4_fixup[i].addr;
        memcpy(&type4_fixup[i], &type4_fixup[i + 1], (num_type4 - i) * sizeof(Type4Fixup));
        num_type4--;
      }
      else if(matched_ref_lo){
        value = (DWORD)(*fixup_addr_hi << 16) + *(WORD*)addr + offset;
        
        *fixup_addr_hi = HIWORD(value + 0x8000);
        matched_ref_lo = false;
      }          
      else{
        value = *(short *)addr + offset;

        if(is_kernel() || fixup_like_kernel())
          assert(!"Error: kernel must have matched BASED_HIGH/BASED_LOW fixup pair");
      }

      *(WORD*)addr = (WORD)(value & 0xffff);

      if(debug_show) printf("%08x", *addr);

      break;

    // Word - (32-bits) relocate the entire address.
    case IMAGE_REL_BASED_HIGHLOW:
      if(debug_show) printf("fixup %08x -> ", *addr);
        
      if(is_kernel() || fixup_like_kernel()){
        *addr = fixup_rva_nk(*addr + offset);
      }
      else
        *addr += offset;

      if(debug_show) printf("%08x", *addr);

      break;

    // 32 bit relocation of 16 bit high word, sign extended
    case IMAGE_REL_BASED_HIGHADJ:
      if(debug_show) printf("fixup %08x -> ", *addr);
      
      if(is_kernel() || fixup_like_kernel()){
        value = fixup_rva_nk((*(short UNALIGNED *)addr << 16) + (long)(short)low_addr + offset);

        if(num_type4 < MAX_TYPE4_NEST){
          type4_fixup[num_type4].addr = value;
          type4_fixup[num_type4].addr_lo = low_addr;
          num_type4++;
        }
        else{
          cerr << "Error: exceeded type 4 fixup nesting level in module " << m_name << endl;
          printf("addr %08x low_addr %04x\n", (DWORD)addr, low_addr);
          return false;
        }

        *(short UNALIGNED *)addr = HIWORD(value + 0x8000);
      }
      else
        *(short UNALIGNED *)addr += HIWORD((long)(short)low_addr + offset + 0x8000);

      if(debug_show) printf("%08x", *addr);

      break;

    // Reference to the low portion of a 32-bit address.
    // jump to 26 bit target (shifted left 2)

    // weird dll in mips16 land uses absolute address for jumps while the rest of the world
    // uses relative.  Therefore we had to come up with way to add the base in and not mess up
    // when the base was already in.
    case IMAGE_REL_BASED_MIPS_JMPADDR:
      if(debug_show) printf("fixup %08x -> ", *addr);

      value = (*addr & 0x03ffffff) + (offset >> 2);
      *addr = (*addr & 0xfc000000) | (value & 0x03ffffff);

      if(debug_show) printf("%08x", *addr);
      break;

    // MIPS16 jal/jalx to 26 bit target (shifted left 2)
    case IMAGE_REL_BASED_MIPS_JMPADDR16:
      if(debug_show) printf("fixup %08x -> ", *addr);
      
      value = *(WORD *)addr & 0x3ff;
      value = (value >> 5) | ((value & 0x1f) << 5);
      value = (value << 16) | *((WORD *)addr + 1);
      value += offset >> 2;      
      *((WORD *)addr + 1) = LOWORD(value);
      
      value = HIWORD(value) & 0x3ff;
      value = (value >> 5) | ((value & 0x1f) << 5);
      *(WORD *)addr = LOWORD((*(WORD *)addr & 0x1c00) | value);

      if(debug_show) printf("%08x", *addr);
      break;
    
    default:
      if(!is_kernel() && !fixup_like_kernel())
        assert(!"Error: Unknown fixup type encountered");
      break;
  }

  if(debug_show) printf("\n");

  return true;
}

/*****************************************************************************/
// MIPS relocation Types:
//
//  Relocation types are different in DEBUG fixups
//
//  Example                 COFF Fixup      DEBUG Fixup
//
//  lui     ax,0x8c05       4               4
//  jal     0x85049231      5               3
//  addiu   a0,a0,0xC610    2               5
//  0x8c049201              3               2
//
//#define IMAGE_REL_MIPS_ABSOLUTE           0
//#define IMAGE_REL_MIPS_REFHALF            01
//#define IMAGE_REL_MIPS_REFWORD            02
//#define IMAGE_REL_MIPS_JMPADDR            03
//#define IMAGE_REL_MIPS_REFHI              04
//#define IMAGE_REL_MIPS_REFLO              05
//
//
//#define IMAGE_REL_BASED_ABSOLUTE          0
//#define IMAGE_REL_BASED_HIGH              1
//#define IMAGE_REL_BASED_LOW               2
//#define IMAGE_REL_BASED_HIGHLOW           3
//#define IMAGE_REL_BASED_HIGHADJ           4
//#define IMAGE_REL_BASED_MIPS_JMPADDR      5
//
// PPC relocation Types:
// type 8  00568 81620000   lwz      r11,dpCurSettings(r2) ; dpCurSettings
// type 6  00590 4bfffa71   bl        ..NKDbgPrintfW ; NKDbgPrintfW     (subroutine call)
//
// type 16 13EFC 3C608002 lis        r3,[hia]HandleException
// type 17 13F00 386387E0 addi      r3,r3,[lo]HandleException
//
// type 18 (PAIR) always immediately follows type 16
//
// #define IMAGE_REL_PPC_REFHI  0x0010
// #define IMAGE_REL_PPC_REFLO  0x0011
// #define IMAGE_REL_PPC_PAIR    0x0012
//
/*****************************************************************************/

/*****************************************************************************
  fixup_nk()

  in:
    none
    
  out:
    boolean value indicating success

  desc:
    do special kernal fixups
 *****************************************************************************/
bool Module::fixup_nk(){
  assert(is_kernel());

#define DEBUG_IMAGE_REL_BASED_ABSOLUTE       0
#define DEBUG_IMAGE_REL_BASED_HIGH           1
#define DEBUG_IMAGE_REL_BASED_LOW            5
#define DEBUG_IMAGE_REL_BASED_HIGHLOW        2
#define DEBUG_IMAGE_REL_BASED_HIGHADJ        4
#define DEBUG_IMAGE_REL_BASED_MIPS_JMPADDR   3
#define DEBUG_IMAGE_REL_BASED_I386_HIGHLOW  20
#define DEBUG_IMAGE_REL_BASED_I386_HIGHLOW2  6
#define DEBUG_IMAGE_REL_BASED_PPC_REL24      6
#define DEBUG_IMAGE_REL_BASED_PPC_REFHI     16
#define DEBUG_IMAGE_REL_BASED_PPC_REFLO     17
#define DEBUG_IMAGE_REL_BASED_PPC_PAIR      18

  DWORD rva_delta = m_vreloc - m_e32.e32_vbase;

  DWORD *fixup_addr;
  WORD  *fixup_addr_hi;
  DWORD fixup_value;
  bool matched_ref_lo = false;

  for(unsigned i = 0; i < m_kernel_fixup_list.size(); i++){
    fixup_addr = (DWORD*)1;

    if(m_e32.e32_cpu == IMAGE_FILE_MACHINE_ARM || m_e32.e32_cpu == IMAGE_FILE_MACHINE_THUMB){
      switch(m_kernel_fixup_list[i].Type){
        case DEBUG_IMAGE_REL_BASED_MIPS_JMPADDR:
          break;
          
        case DEBUG_IMAGE_REL_BASED_HIGH:
          *(long UNALIGNED *)fixup_addr += get_rva_delta(m_kernel_fixup_list[i], rva_delta);
          break;
          
        default:
          fprintf(stderr, "Error: Not doing fixup type %d\n", m_kernel_fixup_list[i].Type);
          fprintf(stderr, "type %u, location %08x target %08x fixupop %08x\n", 
                  m_kernel_fixup_list[i].Type,
                  m_kernel_fixup_list[i].RvaLocation,
                  m_kernel_fixup_list[i].RvaTarget,
                  *fixup_addr);
          return false;
          break;
      }
      continue;
    }

    switch(m_kernel_fixup_list[i].Type){
      case DEBUG_IMAGE_REL_BASED_ABSOLUTE: 
        break;
      
      case DEBUG_IMAGE_REL_BASED_HIGH:
        fixup_addr_hi = (WORD *)fixup_addr;
        matched_ref_lo = true;
        break;

      case DEBUG_IMAGE_REL_BASED_LOW:
      case DEBUG_IMAGE_REL_BASED_PPC_REFLO:
        if(matched_ref_lo){
          fixup_value = (*fixup_addr_hi << 16) + *(WORD *)fixup_addr + get_rva_delta(m_kernel_fixup_list[i], rva_delta);
          *fixup_addr_hi = HIWORD(fixup_value + 0x8000);
          matched_ref_lo = false;
        }
        else
          fixup_value = *(WORD *)fixup_addr + get_rva_delta(m_kernel_fixup_list[i], rva_delta);

        
        *(WORD *)fixup_addr = LOWORD(fixup_value);
        break;
        
      case DEBUG_IMAGE_REL_BASED_I386_HIGHLOW:
        break;

      case DEBUG_IMAGE_REL_BASED_I386_HIGHLOW2:
        if(m_e32.e32_cpu & IMAGE_FILE_MACHINE_POWERPC){
          if(*fixup_addr & 0x2){
            fixup_value = *fixup_addr & 0x03fffffc;
            fixup_value += rva_delta;
            if(fixup_value & 0xfc000000){
              fprintf(stderr, "Error: fixup overflow at location %08x\n", m_kernel_fixup_list[i].RvaLocation);
              assert(!"Fatal error");
            }
            *fixup_addr = (*fixup_addr & 0xfc000003) | (fixup_value & 0x03fffffc);
          }
        }
        else{
          fixup_value = *fixup_addr & 0xff000000;
          *fixup_addr += get_rva_delta(m_kernel_fixup_list[i], rva_delta);
        }          
        break;
      case DEBUG_IMAGE_REL_BASED_HIGHLOW:
        *(long UNALIGNED *)fixup_addr += get_rva_delta(m_kernel_fixup_list[i], rva_delta);
        break;

      case DEBUG_IMAGE_REL_BASED_PPC_REFHI:
        if(m_e32.e32_cpu != IMAGE_FILE_MACHINE_POWERPC){
          fixup_value = *(WORD *)fixup_addr & 0x03ff;
          fixup_value = (fixup_value >> 5) | ((fixup_value & 0x1f) << 5);
          fixup_value = (fixup_value << 16) | *((WORD *)fixup_addr + 1);
          fixup_value += get_rva_delta(m_kernel_fixup_list[i], rva_delta) >> 2;
          *((WORD *)fixup_addr + 1) = LOWORD(fixup_value);
  
          fixup_value = HIWORD(fixup_value) & 0x3ff;
          fixup_value = (fixup_value >> 5) | ((fixup_value & 0x1f) << 5);
          *(WORD *)fixup_addr = LOWORD((*(WORD *)fixup_addr & 0x1c00) | fixup_value);
          break;
        }
        /* fall through for PowerPC's */

      case DEBUG_IMAGE_REL_BASED_HIGHADJ:
        assert(i + 1 < m_kernel_fixup_list.size());
        
        fixup_value = *(short UNALIGNED *)fixup_addr << 16;
        fixup_value += (long)(short)LOWORD(m_kernel_fixup_list[i].RvaTarget + m_kernel_fixup_list[i + 1].RvaTarget);
        fixup_value += get_rva_delta(m_kernel_fixup_list[i], rva_delta); // a REFHI has to be followed by a PAIR
        i++; // increment fixup list counter
        *(short UNALIGNED *)fixup_addr = HIWORD(fixup_value + 0x8000);
        break;

      case DEBUG_IMAGE_REL_BASED_MIPS_JMPADDR:
        fixup_value = (*fixup_addr & 0x03ffffff) + (rva_delta >> 2);
        *fixup_addr = (*fixup_addr & 0xfc000000) | (fixup_value & 0x3ffffff);
        break;

      default:
        fprintf(stderr, "Error: Not doing fixup type %d\n", m_kernel_fixup_list[i].Type);
        fprintf(stderr, "type %u, location %08x target %08x fixupop %08x\n", 
                m_kernel_fixup_list[i].Type,
                m_kernel_fixup_list[i].RvaLocation,
                m_kernel_fixup_list[i].RvaTarget,
                *fixup_addr);
        return false;
        break;
    }
  }
  
  return true;
}

bool Module::fixupvar(DWORD section, DWORD addr, DWORD value){
  if(!is_kernel()){
    fprintf(stderr, "Warning: All fixupvar elements must point to kernel addresses...ignored\n");
    return false;
  }

  O32List::iterator o32_itr = &m_o32_list[section];
  *(DWORD *)(o32_itr->data.ptr() + addr) = value;

  return true;
}

/*****************************************************************************
  remove_discardable_sections()

  in:
    none
    
  out:
    boolean value indicating success

  desc:
    Delete the discardable sections from the object list
 *****************************************************************************/
bool Module::remove_discardable_sections(){
  // remove discardable flag from resource section, we need to keep it
  for(O32List::iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++){
    if(o32_itr->o32_rva == m_e32.e32_unit[RES].rva){
      o32_itr->o32_flags &= ~IMAGE_SCN_MEM_DISCARDABLE;
      break;
    }
  }

  // remove all sections still marked as discardable
  o32_itr = m_o32_list.begin(); 
  while(o32_itr != m_o32_list.end())
    if(o32_itr->is_discardable())
      o32_itr = m_o32_list.erase(o32_itr);
    else
      o32_itr++;

  // adjust the object count to reflect the current count after removal
  m_e32.e32_objcnt = m_o32_list.size();
  
  return true;
}

/*****************************************************************************
  do_imports()

  in:
    module_list - may be needed for recursive searching
    
  out:
    boolean value indicating success

  desc:
    resolve import tables
 *****************************************************************************/
void Module::resolve_imports(ModuleList &module_list, bool error_late_bind){
  if(!m_e32.e32_unit[IMP].size){
    LAST_PASS_PRINT cout << "No imports for " << m_name << endl;
    return;
  }

  // Make imports section readonly since we're fixing it up
  for(O32List::iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++){
    if(o32_itr->o32_rva == m_e32.e32_unit[IMP].rva || o32_itr->o32_rva == m_e32.e32_unit[EXP].rva){
      o32_itr->o32_flags &= ~IMAGE_SCN_MEM_WRITE;
      break;
    }
  }

  ImpHdr *block_ptr, *block_start;

  block_ptr = block_start = (ImpHdr *)rva2ptr(m_e32.e32_unit[IMP].rva);
  while(block_ptr->imp_lookup){
    ModuleList::iterator mod_itr;

    // find dll name in module list
    mod_itr = find(module_list.begin(), module_list.end(), Module((char *)rva2ptr(block_ptr->imp_dllname)));
    if(mod_itr != module_list.end())
      mod_itr->set_static_ref_flag();
    else
      if (error_late_bind){
        cerr << "Error: Unable to do imports from " 
             << m_name << " to " << (char *)rva2ptr(block_ptr->imp_dllname) 
             << endl;
      }
      else{
        cerr << "Warning: Unable to do imports from " 
             << m_name << " to " << (char *)rva2ptr(block_ptr->imp_dllname) 
             << " - will late bind" << endl;
      }

    DWORD *ltptr = (DWORD *)rva2ptr(block_ptr->imp_lookup);
    DWORD *atptr = (DWORD *)rva2ptr(block_ptr->imp_address);

    while(*ltptr){
      if(mod_itr != module_list.end()){ // found it above
        if(*ltptr & 0x80000000)
          *atptr = mod_itr->resolve_imp_ord(module_list, *ltptr & ~0x80000000);
        else{ // see if the hint string is of any help
          ImpProc *impptr = (ImpProc *)rva2ptr(*ltptr);
          *atptr = mod_itr->resolve_imp_hint_str(module_list, impptr->ip_hint, string((char *)impptr->ip_name));
        }

        if(!*atptr){
          cerr << "Error: Fatal import error in " << m_name << endl;
          s_import_success_status = false;
        }
      } else // it's now statically loaded, forget about it
        *atptr = 0;
      
      ltptr++;
      atptr++;
    }
    
    block_ptr++;
  }
}

/*****************************************************************************
  compress()

  in:
    bool comp - global compression flag
    
  out:
    boolean value indicating success

  desc:
    (pretty self explanatory)
******************************************************************************/
bool Module::compress(bool comp){
  /* WARNING TO READERS
   *   you don't want to mess with any of the logic 
   *   in here if you don't absolutely have to :)
   */

  // no compression wanted or zero length
  if(!comp || !m_file_size){
    m_file_attributes &= ~FILE_ATTRIBUTE_COMPRESSED;

    //  truncate the kernel writable sections even if compression is off
    if(is_kernel() || fixup_like_kernel())
      for(O32List::iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++)
        if(o32_itr->is_writable())
          truncate_o32(*o32_itr);
    
    return true;
  }

  for(O32List::iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++){
    //  I am REALLY sorry for this conditional!
    if(m_e32.e32_unit[IMP].rva == o32_itr->o32_rva || m_e32.e32_unit[EXP].rva == o32_itr->o32_rva){
      if(m_file_attributes & FILE_ATTRIBUTE_COMPRESSED)
        compress_o32(*o32_itr);
    }
    else if(is_kernel() || fixup_like_kernel()){
      if(o32_itr->is_writable())
        truncate_o32(*o32_itr); // don't compress, only truncate the kernel writable sections
    }
    else if(m_file_attributes & FILE_ATTRIBUTE_COMPRESSED ||
            o32_itr->is_writable()                   ||
            o32_itr->name() == ".pdata"              ||
            (o32_itr->name() == ".rsrc" && compress_resources())
           )
      compress_o32(*o32_itr);
  }

  return true;
}


/*****************************************************************************
 *****************************************************************************/
void Module::print_header(){
  static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "move") : NULL;
  if(suppress) return;
  
  LAST_PASS_PRINT printf("\nMODULES Section\n");
  LAST_PASS_PRINT printf("Module                 Section  Start     Length  psize   vsize   Filler\n");
  LAST_PASS_PRINT printf("---------------------- -------- --------- ------- ------- ------- ------\n");
}

/*****************************************************************************
  move_readonly_sections()

  in:
    reserve_list     - reserved memory list
    DWORD next_avail - next available storage address

  out:
    boolean indicating success

  desc:
    moves readonly sections to the start of the memory space and image
 *****************************************************************************/
bool Module::move_readonly_sections(const MemoryList &reserve_list, DWORD &next_avail, AddressList &address_list){
  for(O32List::iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++){
    if(!o32_itr->is_writable() &&
       !o32_itr->is_compressed()){

      DWORD kernel_offset = 0;
      DWORD backup_next_avail = 0;
      
      if(next_avail == memory_iterator()->address() ||
         ((is_kernel() || fixup_like_kernel()) && m_o32_list.begin() == o32_itr))
        kernel_offset = o32_itr->o32_rva;

      bool used_reserve_hole = false;
      
      if(!address_list.empty()){
        // see if we can fit in a space caused by a reserve skip first, if not go to the regular way.
        DWORD size = align_page(o32_itr->min_size() + kernel_offset);

        for(AddressList::iterator aitr = address_list.begin(); aitr != address_list.end(); aitr++){
          if(align_page(aitr->address()) < aitr->address_end() && 
             size < aitr->length() - (align_page(aitr->address()) - aitr->address())){
            used_reserve_hole = true;

            // if either of these don't exist (size of zero) do this anyway because it's ok
            // Address of space in front of that we're going to use
            Address temp1(aitr->address(), align_page(aitr->address()) - aitr->address());
            temp1.set_hole();

            // Address of space behind that we're going to use
            Address temp2(temp1.address_end() + size, aitr->address_end() - (temp1.address_end() + size));
            temp2.set_hole();
            
            // add the remaining bits
            if(temp1.length()) address_list.push_back(temp1);
            if(temp2.length()) address_list.push_back(temp2);

            // cant do this as we wipe out our good value;
            backup_next_avail = next_avail;
            next_avail = align_page(aitr->address());

            // erase the used one
            address_list.erase(aitr);

            break;
          }
        }
      }
      
      if(!used_reserve_hole){
        // regular way of allocating a module
        next_avail = kernel_offset + align_page(Memory::find_next_gap(reserve_list, next_avail, align_page(o32_itr->min_size() + kernel_offset), &address_list, memory_iterator()->address()));
      }

      static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "move") : NULL;
      if(!suppress){
        static char *delim = getenv("ri_comma_delimit") ? "," : " ";

        LAST_PASS_PRINT
          printf("%-22s%s%-8.8s%s%08x%s%7u%s%7u%s%7u%so32_rva=%08x\n", 
               m_name.c_str(), delim,
               o32_itr->o32_name, delim,
               next_avail, delim,
               align_page(o32_itr->min_size()), delim,
               o32_itr->o32_psize, delim,
               o32_itr->o32_vsize, delim,
               o32_itr->o32_rva);
      }

      o32_itr->o32_dataptr = next_avail;

      if(fixup_like_kernel() && 
         m_o32_list.begin() == o32_itr &&
         o32_itr->o32_dataptr - o32_itr->o32_rva != m_vreloc){
        fprintf(stderr, "Error: Kernel fixup module %s dataptr [%x] != m_vreloc [%x]\n",
                name().c_str(),
                o32_itr->o32_dataptr,
                m_vreloc);
        exit(1);
      }
      
      if(is_kernel()){
        o32_itr->o32_rva = next_avail - memory_iterator()->address();

        if(o32_itr->name() == ".pdata"){
          m_e32.e32_unit[EXC].rva = o32_itr->o32_rva;
        }
      }

      if(!is_kernel() && fixup_like_kernel()){
        o32_itr->o32_rva = o32_itr->o32_dataptr - m_o32_list[0].o32_dataptr + m_o32_list[0].o32_rva;

        if(o32_itr->name() == ".pdata"){
          m_e32.e32_unit[EXC].rva = o32_itr->o32_rva;
        }
      }

      // add any hole space after the module.
      {
        DWORD start = align_dword(o32_itr->o32_dataptr + o32_itr->min_size());
        DWORD size = align_page(start) - start;

        for(MemoryList::const_iterator ritr = reserve_list.begin(); ritr != reserve_list.end() && ritr->address() < start + size; ritr++){
          if(ritr->intersects(start, size)){
            assert(ritr->address() > start);
            size = ritr->address() - start;
            break;
          }
        }
  
        address_list.push_back(Address(start, size));
        address_list.rbegin()->set_hole();
      }      

      // restore this for the next pass
      if(used_reserve_hole)
        next_avail = backup_next_avail;
      else
        next_avail += o32_itr->min_size();
    }
  }

  return true;
}

/*****************************************************************************
  move_readwrite_sections()

  in:
    address_list - ranges that can be used
    copy_list    - list of kernel areas that need to be copied to a new
                   memory location before the kernel is run

  out:
    boolean indicating success

  desc:
    moves readwrite sections into the gaps between read only sections or tacking
    on the end if no range could be found
 *****************************************************************************/
bool Module::move_readwrite_sections(AddressList &address_list, CopyList &copy_list){
  static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "move") : NULL;
  
  for(O32List::iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++){
    if(o32_itr->is_writable()   || 
       o32_itr->is_compressed()){
      // swaps dataptr and realaddr not that fixup are done
      DWORD dest = o32_itr->o32_dataptr;

      bool filler = false;
      o32_itr->o32_dataptr = Memory::allocate_range(address_list, o32_itr->min_size(), &filler);

      if(o32_itr->o32_dataptr == -1){
        fprintf(stderr, "Error: Ran out of space in ROM for %s\nsize %d\n", m_name.c_str(), o32_itr->min_size());
        return false;
      }

      if(!suppress){
        static char *delim = getenv("ri_comma_delimit") ? "," : " ";

        LAST_PASS_PRINT
          printf("%-22s%s%-8.8s%s%08x%s%7u%s%7u%s%7u%s%s",
               m_name.c_str(), delim,
               o32_itr->o32_name, delim,
               o32_itr->o32_dataptr, delim,
               o32_itr->min_size(), delim,
               o32_itr->o32_psize, delim,
               o32_itr->o32_vsize, delim,
               filler ? "FILLER" : "");
      }

      if(is_kernel() || fixup_like_kernel()){
        o32_itr->o32_realaddr = dest;

        if(!suppress) LAST_PASS_PRINT printf("->%08x", o32_itr->o32_realaddr);
        
        if(o32_itr->name() != ".KDATA"){
          CopyEntry ce;

          ce.ulSource  = o32_itr->o32_dataptr;
          ce.ulDest    = o32_itr->o32_realaddr;
          ce.ulCopyLen = o32_itr->min_size();
          ce.ulDestLen = o32_itr->o32_vsize;
          ce.m_memory_iterator = memory_iterator();

          copy_list.push_back(ce);
        }
      }

      if(!suppress) LAST_PASS_PRINT printf("\n");
    }
  }

  return true;
}

/*****************************************************************************
  move_eo32_sections()

  in:
    addrses_list  - ranges that can be used

  out:
    boolean indicating success

  desc:
    moves e32 and o32 tables into the gaps between read only sections
 *****************************************************************************/
bool Module::move_eo32_structures(AddressList &address_list){
  static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "move") : NULL;

  bool filler = false;
  m_e32_offset = Memory::allocate_range(address_list, align_dword(sizeof(e32_rom)), &filler);

  if(m_e32_offset == -1){
    fprintf(stderr, "Error: Ran out of space in ROM for %s\nsize %d", m_name.c_str(), align_dword(sizeof(e32_rom)));
    return false;
  }
  
  if(!suppress){
    static char *delim = getenv("ri_comma_delimit") ? "," : " ";

    LAST_PASS_PRINT
      printf("%-22s%s%-8s%s%08x%s%7u%s%s%s              %s\n",
           m_name.c_str(), delim,
           "E32", delim,
           m_e32_offset, delim,
           sizeof(e32_rom), delim,
           delim, delim,
           filler ? "FILLER" : "");
  }
      
  m_o32_offset = Memory::allocate_range(address_list, m_o32_list.size() * sizeof(o32_rom), &filler);

  if(m_o32_offset == -1){
    fprintf(stderr, "Error: Ran out of space in ROM for %s\nsize %d", m_name.c_str(), m_o32_list.size() * sizeof(o32_rom));
    return false;
  }
  
  if(!suppress){
    static char *delim = getenv("ri_comma_delimit") ? "," : " ";

    LAST_PASS_PRINT
      printf("%-22s%s%-8s%s%08x%s%7u%s%s%s              %s\n",
           m_name.c_str(), delim,
           "O32", delim,
           m_o32_offset, delim,
           m_o32_list.size() * sizeof(o32_rom), delim,
           delim, delim,
           filler ? "FILLER" : "");
  }

  return true;
}

/*****************************************************************************

  in:

  out:

  desc:
 *****************************************************************************/
void Module::write_prolog(ostream &out_file, DWORD &start_ip, DWORD boot_jump, DWORD user_reset_vector){
  assert(is_kernel());

  switch(m_e32.e32_cpu){
    case IMAGE_FILE_MACHINE_POWERPC:
    {
      if(m_reset_vector && m_reset_vector_end && m_reset_vector_addr)
        write_bin(out_file, *m_reset_vector_addr, m_reset_vector, (m_reset_vector_end - m_reset_vector) * 4);
      else{
        DWORD ppc_start_ip = (start_ip - memory_iterator()->address()) & 0x7fffffff;
        BYTE ppc_prolog[4];
  
        ppc_start_ip -= 0x100;
        ppc_start_ip |= 0x48000000;
  
        ppc_prolog[0] = LOBYTE(ppc_start_ip >> 24);
        ppc_prolog[1] = LOBYTE(ppc_start_ip >> 16);
        ppc_prolog[2] = LOBYTE(ppc_start_ip >>  8);
        ppc_prolog[3] = LOBYTE(ppc_start_ip >>  0);
  
        write_bin(out_file, memory_iterator()->address() + 0x100, ppc_prolog, sizeof(ppc_prolog));
      }
      break;
    }

    case IMAGE_FILE_MACHINE_R3000:
    case IMAGE_FILE_MACHINE_R4000:
    case IMAGE_FILE_MACHINE_MIPS16:
    case IMAGE_FILE_MACHINE_MIPSFPU:
    case IMAGE_FILE_MACHINE_MIPSFPU16:
    {
      BYTE mips_prolog[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x3c, 
                             0x00, 0x00, 0x5a, 0x37, 0x08, 0x00, 0x40, 0x03, 
                             0x00, 0x00, 0x00, 0x00 };
      
      if(0x8d000000 <= start_ip && start_ip < 0x9fffffff)
        start_ip |= 0xa0000000;
  
      if(boot_jump){
        start_ip -= memory_iterator()->address();
        start_ip |= boot_jump;
      }
  
      mips_prolog[4] = LOBYTE(start_ip >> 16);
      mips_prolog[5] = LOBYTE(start_ip >> 24);
      mips_prolog[8] = LOBYTE(start_ip >>  0);
      mips_prolog[9] = LOBYTE(start_ip >>  8);
  
      
      if(user_reset_vector)
        write_bin(out_file, user_reset_vector, mips_prolog, sizeof(mips_prolog));
  
      write_bin(out_file, memory_iterator()->address(), mips_prolog, sizeof(mips_prolog));
  
      start_ip = memory_iterator()->address() + 4;

      break;
    }

    case IMAGE_FILE_MACHINE_I386:
    {
      if(m_reset_vector && m_reset_vector_end && m_reset_vector_addr)
        write_bin(out_file, *m_reset_vector_addr, m_reset_vector, (m_reset_vector_end - m_reset_vector) * 4);
      else{
        DWORD reladdress = start_ip - memory_iterator()->address() - 9;
        
        BYTE x86_prolog[]  = { 0x90, 0x90, 0x90, 0x90, 0xe9, 0x90, 0x90, 0x90, 
                               0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

        memcpy(&x86_prolog[5], &reladdress, sizeof(reladdress));
        write_bin(out_file, memory_iterator()->address(), x86_prolog, sizeof(x86_prolog));
      }

      break;
    }

    case IMAGE_FILE_MACHINE_ARM:
    case IMAGE_FILE_MACHINE_THUMB:
    {
      DWORD arm_jump = 0xea000000 - 2 + (start_ip - memory_iterator()->address()) / 4;
  
      write_bin(out_file, memory_iterator()->address(), &arm_jump, sizeof(arm_jump));

      break;
    }

    case IMAGE_FILE_MACHINE_SH3:
    case IMAGE_FILE_MACHINE_SH4:
    {
      BYTE sh3_prolog[]  = { 0x09, 0x00, 0x09, 0x00, 0x01, 0xd0, 0x09, 0x00, 
                             0x2b, 0x40, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00 };
  
      if(boot_jump){
        start_ip -= memory_iterator()->address();
        start_ip |= boot_jump;
      }
    
      memcpy(&sh3_prolog[12], &start_ip, sizeof(start_ip));
    
      write_bin(out_file, memory_iterator()->address(), sh3_prolog, sizeof(sh3_prolog));
    
      start_ip = memory_iterator()->address() + 4;
  
      break;
    }

    default:
      cerr << "Error: no prolog for prossor type (" << m_e32.e32_cpu << ")" << endl;
      assert(!"error in write_prolog");
  }
}

/*****************************************************************************

  in:

  out:

  desc:
 *****************************************************************************/
void Module::write_TOC_ptr(DWORD addr){
  assert(is_kernel());

  if(!m_TOC_offset){
    fprintf(stderr, "Error: Found NULL or missing TOC pointer for %s\n", m_name.c_str());
    exit(1);
  }
  
//  *(DWORD *)(m_o32_list[0].data.ptr() + m_TOC_offset - page_size()) = addr;
  *(DWORD *)rva2ptr(m_TOC_offset) = addr;
}

/*****************************************************************************
  write()

  in:
    ostream out_file - previously opened stream to write the module to

  out:
    boolean indicating success

  desc:
    writes all parts of the module to the proper offset in the output stream
 *****************************************************************************/
bool Module::write(ostream &out_file){
  int i;
  Data o32hdrs;
  for(O32List::iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++){
    o32_rom orom = {0};

    if(!o32_itr->data.empty()){
      if(o32_itr->is_writable() && o32_itr->is_compressed())
        write_bin(out_file, o32_itr->o32_dataptr, o32_itr->data.ptr(), o32_itr->o32_psize);
      else
        write_bin(out_file, o32_itr->o32_dataptr, o32_itr->data.ptr(), o32_itr->min_size());
    }

    orom.o32_vsize   = o32_itr->o32_vsize;
    orom.o32_psize   = o32_itr->o32_psize;
    orom.o32_dataptr = o32_itr->o32_dataptr;
    orom.o32_flags   = o32_itr->o32_flags;
    orom.o32_rva     = o32_itr->o32_rva;

    if((is_dll() && is_code_split()) || is_kernel())
      orom.o32_realaddr = o32_itr->o32_realaddr;
    else
      orom.o32_realaddr = m_vreloc + o32_itr->o32_rva;

    if(needs_signing())
      if(is_kernel() && o32_itr->name() == ".text"){
        s_oem_io_control += o32_itr->o32_dataptr;
      }

    o32hdrs.append(&orom, sizeof(orom));
  }

  write_bin(out_file, m_o32_offset, o32hdrs.ptr(), o32hdrs.size());
  
  // shrink e32 structure for ROM
  e32_rom erom = {0};

  erom.e32_objcnt      = m_e32.e32_objcnt;       // Number of memory objects
  erom.e32_imageflags  = m_e32.e32_imageflags;   // Image flags
  erom.e32_entryrva    = m_e32.e32_entryrva;     // Relative virt. addr. of entry point
  erom.e32_vbase       = m_o32_list[0].o32_realaddr - m_o32_list[0].o32_rva; // Virtual base address of module
  erom.e32_subsysmajor = m_e32.e32_subsysmajor;  // The subsystem major version number
  erom.e32_subsysminor = m_e32.e32_subsysminor;  // The subsystem minor version number
  erom.e32_stackmax    = m_e32.e32_stackmax;     // Maximum stack size
  erom.e32_timestamp   = m_e32.e32_timestamp;     // Maximum stack size

#if 0 // optimization to save virtual space when there are large data sections
  for(i = 0; i < m_o32_list.size(); i++)
    if(!m_o32_list[i].is_writable() || m_o32_list[i].is_shared())
      erom.e32_vsize += m_o32_list[i].o32_vsize;
#else
  erom.e32_vsize       = m_e32.e32_vsize;        // Virtual size of the entire image
#endif

  erom.e32_subsys      = m_e32.e32_subsys;       // The subsystem type
  for(i = 0; i < ROM_EXTRA; i++)
    erom.e32_unit[i] = m_e32.e32_unit[i];

  write_bin(out_file, m_e32_offset, &erom, sizeof(erom));

  if(needs_signing())
    if(!sign(erom, o32hdrs))
      return false;

  write_bin(out_file, m_name_offset, m_name.c_str(), m_name.size() + 1);

  if(!m_build_path.empty())
    write_bin(out_file, m_name_offset + m_name.size() + 1, m_build_path.c_str(), m_build_path.size() + 1);

  return true;
}

/*****************************************************************************
*****************************************************************************/
static bool GetBBPrivKey(FileList::iterator sig_file, CKeyBlob *pKey, DWORD *pdwBufSize)
{
  #define ROMIMAGE_SIG_KEYFILE        "BBPrivKey.dat"
  string key_name = sig_file->full_path();
  if(key_name.rfind("\\"))
    key_name = key_name.substr(0, key_name.rfind("\\") + 1);

  key_name += ROMIMAGE_SIG_KEYFILE;
  if(GetFileAttributes(key_name.c_str()) == -1){
    cerr << "Error: Could not find file '" << key_name << "'\n";
    return false;
  }

  Data key_buffer;

  // had to use create file because istream.read won't tell how
  // many bytes it read if it doesn't read the requested amount
  HANDLE hFile = CreateFile(key_name.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if(hFile == INVALID_HANDLE_VALUE){
    cerr << "Error: Could not open key file '" << key_name << "' gle=" << GetLastError() << endl;
    return false;
  }

  if(!ReadFile(hFile, key_buffer.user_fill(nMAX_KEY_SIZE*2), nMAX_KEY_SIZE*2, pdwBufSize, NULL)){
    cerr << "Error: Could not read key file '" << key_name << "' gle=" << GetLastError() << endl;
    return false;
  }

  CloseHandle(hFile);

  if(FAILED(UDInitKeyFromBinary(*pdwBufSize, key_buffer.ptr(), pKey))){
    cerr << "Error: Could not init key from bin gle=" << GetLastError() << endl;
    return false;
  }

  return true;
}

/*****************************************************************************
  sig_adjust()

  in:
    none

  out:
    boolean value indicating success

  desc:
******************************************************************************/
bool Module::sig_adjust(){
  if(!needs_signing())
    return true;

  //
  // Figure out the final size of the sig file
  //
  DWORD size = 0;
  
  // Need to figure out the size of the signed hash
  
  CKeyBlob ckey_blob;
  DWORD    cb;  // ignored
  if (!GetBBPrivKey(m_signature_file, &ckey_blob, &cb)) {
      return false;
  }

  // Use UDSign to query the size of a signature
  if(FAILED(UDSign(&ckey_blob,                // need to pass a valid key even though it won't be used
                   sizeof(DWORD), (BYTE*)&cb, // garbage params
                   (UINT*)&size,              // will receive the required size
                   NULL))){                   // ignored when querying
    cerr << "Error: Could not get size of signature gle=" << GetLastError() << endl;
    return false;
  }
  printf("SIG: Size of signature = %u\n", size);

  
  // Add in the sizes of the other parts of the sig file
  size += sizeof(ROMSIG_MEMREGION) * m_o32_list.size(); // one for each o32 section
  size += sizeof(ROMSIG_MEMREGION) * 2; // one for e32rom and one for o32rom
  size += sizeof(ROMSIG_INFO);

  
  //
  // Fill the sig file with dots to placehold the amount of space it will need.
  //

  ofstream sig_file(m_signature_file->full_path().c_str(), ios::binary | ios::trunc);
  if(sig_file.bad()){
    cerr << "Error: Could not open '" << m_signature_file->full_path() << "' for writing\n";
    return false;
  }

  while(size--)
    sig_file.write(".", 1);

  sig_file.flush();
  sig_file.close();

  return m_signature_file->load();
}

bool Module::sign(e32_rom erom, Data o32hdrs){
  ROMSIG_MEMREGION rsm;
  CUDSha1 csha1;
  Data sig_file_buffer;

  clog << "SIG: signing " << m_name << endl;

  ROMSIG_INFO rsi = {0};
  printf("SIG: OEMIoControl at %08x\n", s_oem_io_control);
  rsi.dwOEMIoControl = s_oem_io_control;
  rsi.dwHWIDType     = ROMSIG_HWID_TYPE_V1;
  rsi.dwMagic        = ROMSIG_MAGIC_ROM_MODULE;
  // Note: it is too early to write rsi into the file, but it helps place-hold
  // the space so that we can append other data to the file.  It will be 
  // overwritten with the most recent data later.
  sig_file_buffer.append(&rsi, sizeof(ROMSIG_INFO));

  WORD rgwSigVersion[] = ROMSIG_VERSION_V1;

  wcscpy(rsi.wszKeyId, ROMSIG_KEY_ID_V1);
  memcpy(rsi.rgwVersion, rgwSigVersion, sizeof(rsi.rgwVersion));

  //
  // fill in our siginfo struct
  // the invariant here is that you make sure that whatever data is
  // passed to WriteSRERecord for this module, you use those params
  // for the signature process.  That way, you make sure you're signing
  // exactly what will appear in ROM.
  //

  if(!csha1.Init()){
    fprintf(stderr, "Error: Couldn't init SHA gle=%ld\n", GetLastError());
    return false;
  }

  // first the PE header
  printf("SIG: hashing PE hdr at %08x, len=%ld, dest=%08x\n", (DWORD)&erom, sizeof(e32_rom), m_e32_offset);

  rsm.lpvAddress = (LPVOID)m_e32_offset;
  rsm.dwLength   = sizeof(e32_rom);
  sig_file_buffer.append(&rsm, sizeof(ROMSIG_MEMREGION));
  rsi.dwNumMemRegions++;

  if(!csha1.Update((BYTE*)&erom, sizeof(e32_rom))){
    fprintf(stderr, "Error:  Couldn't update SHA gle=%ld\n", GetLastError());
    return false;
  }

  // now the section headers
  printf("SIG: hashing sect hdrs at %08x, len=%ld, dest=%08x\n", (DWORD)o32hdrs.ptr(), sizeof(o32_rom) * m_o32_list.size(), m_o32_offset);

  rsm.lpvAddress = (LPVOID)m_o32_offset;
  rsm.dwLength   = sizeof(o32_rom) * m_o32_list.size();
  sig_file_buffer.append(&rsm, sizeof(ROMSIG_MEMREGION));
  rsi.dwNumMemRegions++;

  if(!csha1.Update((BYTE*)o32hdrs.ptr(), sizeof(o32_rom) * m_o32_list.size())){
    fprintf(stderr, "Error:  Couldn't update SHA gle=%ld\n", GetLastError());
    return false;
  }

  // now the sections themselves
  for(O32List::iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++){
    DWORD size;
    
    if(o32_itr->is_writable() && o32_itr->is_compressed())
      size = o32_itr->o32_psize;
    else
      size = o32_itr->min_size();
    
    printf("SIG: hashing section at %08x, len=%ld, dest=%08x\n", (DWORD)o32_itr->data.ptr(), size, o32_itr->o32_dataptr);

    rsm.lpvAddress = (LPVOID)o32_itr->o32_dataptr;
    rsm.dwLength   = size;
    sig_file_buffer.append(&rsm, sizeof(ROMSIG_MEMREGION));
    rsi.dwNumMemRegions++;

    if(!csha1.Update((BYTE*)o32_itr->data.ptr(), size)){
      fprintf(stderr, "Error:  Couldn't update SHA gle=%ld\n", GetLastError());
      return false;
    }
  }

  //
  // don't forget the mod entry in the TOC
  //

  // printf("SIG: hashing TOC entry at %08x, len=%ld, dest=%08x\n", get_TOC());
  
  printf("SIG: hashing signature header at len=%ld, dest=%08x\n", sig_file_buffer.size(), (DWORD)sig_file_buffer.ptr());

  sig_file_buffer.set(0, &rsi, sizeof(ROMSIG_INFO));
  if(!csha1.Update((BYTE*)sig_file_buffer.ptr() + sizeof(rsi.byDigest), sig_file_buffer.size() - sizeof(rsi.byDigest))){
    fprintf(stderr, "Error:  Couldn't update SHA gle=%ld\n", GetLastError());
    return false;
  }

  if(!csha1.Final(rsi.byDigest)){
    fprintf(stderr, "Error:  Couldn't get hash from SHA gle=%ld\n", GetLastError());
    return false;
  }

  //
  // Calculate the signature
  //

  CKeyBlob ckey_blob;
  DWORD cb;  // receives the size required for sig_buffer
  if (!GetBBPrivKey(m_signature_file, &ckey_blob, &cb)) {
      return false;
  }

  Data sig_buffer;
  sig_buffer.user_fill(cb);  // allocate space
  if(FAILED(UDSign(&ckey_blob, sizeof(rsi.byDigest), rsi.byDigest, (UINT*)&cb, sig_buffer.ptr()))){
    cerr << "Error: Could not init key from bin gle=" << GetLastError() << endl;
    return false;
  }

  printf("SIG: UDSign succeeded, sig size is %u\r\n", cb);

  sig_file_buffer.set(0, &rsi, sizeof(ROMSIG_INFO));
  sig_file_buffer.append(sig_buffer.ptr(), cb);

  //
  // now need to write the info to the sig file
  //

  // first, do a sanity check to make sure that what we
  // are about to stuff into the file will fit into the file
  WIN32_FIND_DATA fd;
  HANDLE          hf;
  hf = FindFirstFile(m_signature_file->full_path().c_str(), &fd);
  if(hf == INVALID_HANDLE_VALUE){
    cerr << "Error: Couldn't find '" << m_signature_file->full_path() << "'\n";
    return false;
  } 
  FindClose(hf);

  if(fd.nFileSizeLow != sig_file_buffer.size()){
    cerr << "Error: File " << m_signature_file->full_path() << " is not the size it should be "
         << sig_file_buffer.size() << "(actual=" << fd.nFileSizeLow << ")\n\r";
    return false;
  }

  // write the info to the file
  ofstream sig_file(m_signature_file->full_path().c_str(), ios::binary | ios::trunc);
  if(sig_file.bad()){
    cerr << "Error: Could not open '" << m_signature_file->full_path() << "' for writing\n";
    return false;
  }

  sig_file.write((char*)sig_file_buffer.ptr(), sig_file_buffer.size());
  sig_file.flush();
  sig_file.close();

  return m_signature_file->load();

  //
  // end bin signature process
  // that means that you shouldn't modify the PE header, section headers,
  // TOC entry after this point in the code
  //
}    

/*****************************************************************************
 *****************************************************************************/
TOCentry Module::get_TOC(){
  TOCentry toc = {0};

  toc.dwFileAttributes  = m_file_attributes & 0x00ffffff;
  toc.ftTime            = m_file_time;
  toc.lpszFileName      = (char *)m_name_offset;
  toc.nFileSize         = m_file_size;
  toc.ulE32Offset       = m_e32_offset;
//  toc.ulLoadOffset      = m_o32_list[0].o32_realaddr;
  toc.ulLoadOffset      = m_load_offset + memory_iterator()->address();
  
  toc.ulO32Offset       = m_o32_offset;

  return toc;
}

/*****************************************************************************
 *****************************************************************************/
Profile Module::get_profile(int pass){
  int index = 0;

  for(O32List::iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++){
    if(o32_itr->o32_flags & IMAGE_SCN_CNT_CODE){
      m_profile.ulSectNum = index;
    
      if(is_kernel() || fixup_like_kernel())
        m_profile.ulStartAddr = o32_itr->o32_dataptr;
      else
        m_profile.ulStartAddr = o32_itr->o32_realaddr;

      m_profile.ulEndAddr   = m_profile.ulStartAddr + o32_itr->o32_vsize;

      // add starting physical address to symbol offset
      if(pass && (m_profile.ulNumSym = m_profile.m_function_list.size()))
        for(FunctionList::iterator fun_itr = m_profile.m_function_list.begin(); fun_itr != m_profile.m_function_list.end(); fun_itr++)
          fun_itr->address += m_profile.ulStartAddr;

      break;
    }

    index++;
  }

  return m_profile;
}

/*****************************************************************************
  static class variables and constants

  desc:
    initializes the class variables and constants (duh!)
 *****************************************************************************/
DWORD Module::s_cpu_id  = IMAGE_FILE_MACHINE_UNKNOWN;
ROMHDR Module::s_romhdr = {0};
DWORD Module::s_oem_io_control = 0;
bool Module::s_import_success_status = true;
  
/*****************************************************************************
  dump_e32_header()

  in:
    none

  out:
    none

  desc:
    um... just what the name says
 *****************************************************************************/
void Module::dump_e32_header(){
  static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "e32") : NULL;
  if(suppress) return;
  
  printf("  e32_magic      : %8.8lx\n", *(LPDWORD)&m_e32.e32_magic);
  printf("  e32_cpu        : %8.8lx\n", m_e32.e32_cpu);
  printf("  e32_objcnt     : %8.8lx\n", m_e32.e32_objcnt);
  printf("  e32_symtaboff  : %8.8lx\n", m_e32.e32_symtaboff);
  printf("  e32_symcount   : %8.8lx\n", m_e32.e32_symcount);
  printf("  e32_timestamp  : %8.8lx\n", m_e32.e32_timestamp);
  printf("  e32_opthdrsize : %8.8lx\n", m_e32.e32_opthdrsize);
  printf("  e32_imageflags : %8.8lx\n", m_e32.e32_imageflags);
  printf("  e32_coffmagic  : %8.8lx\n", m_e32.e32_coffmagic);
  printf("  e32_linkmajor  : %8.8lx\n", m_e32.e32_linkmajor);
  printf("  e32_linkminor  : %8.8lx\n", m_e32.e32_linkminor);
  printf("  e32_codesize   : %8.8lx\n", m_e32.e32_codesize);
  printf("  e32_initdsize  : %8.8lx\n", m_e32.e32_initdsize);
  printf("  e32_uninitdsize: %8.8lx\n", m_e32.e32_uninitdsize);
  printf("  e32_entryrva   : %8.8lx\n", m_e32.e32_entryrva);
  printf("  e32_codebase   : %8.8lx\n", m_e32.e32_codebase);
  printf("  e32_database   : %8.8lx\n", m_e32.e32_database);
  printf("  e32_vbase      : %8.8lx\n", m_e32.e32_vbase);
  printf("  e32_objalign   : %8.8lx\n", m_e32.e32_objalign);
  printf("  e32_filealign  : %8.8lx\n", m_e32.e32_filealign);
  printf("  e32_osmajor    : %8.8lx\n", m_e32.e32_osmajor);
  printf("  e32_osminor    : %8.8lx\n", m_e32.e32_osminor);
  printf("  e32_usermajor  : %8.8lx\n", m_e32.e32_usermajor);
  printf("  e32_userminor  : %8.8lx\n", m_e32.e32_userminor);
  printf("  e32_subsysmajor: %8.8lx\n", m_e32.e32_subsysmajor);
  printf("  e32_subsysminor: %8.8lx\n", m_e32.e32_subsysminor);
  printf("  e32_vsize      : %8.8lx\n", m_e32.e32_vsize);
  printf("  e32_hdrsize    : %8.8lx\n", m_e32.e32_hdrsize);
  printf("  e32_filechksum : %8.8lx\n", m_e32.e32_filechksum);
  printf("  e32_subsys     : %8.8lx\n", m_e32.e32_subsys);
  printf("  e32_dllflags   : %8.8lx\n", m_e32.e32_dllflags);
  printf("  e32_stackmax   : %8.8lx\n", m_e32.e32_stackmax);
  printf("  e32_stackinit  : %8.8lx\n", m_e32.e32_stackinit);
  printf("  e32_heapmax    : %8.8lx\n", m_e32.e32_heapmax);
  printf("  e32_heapinit   : %8.8lx\n", m_e32.e32_heapinit);
  printf("  e32_hdrextra   : %8.8lx\n", m_e32.e32_hdrextra);

  char *e32str[] = { // e32 image header data directory types
    "EXP",           // export table
    "IMP",           // import table
    "RES",           // resource table
    "EXC",           // exception table
    "SEC",           // security table
    "FIX",           // base relocation table
    "DEB",           // debug data
    "IMD",           // copyright
    "MSP",           // global ptr
    "TLS",           // tls table
    "CBK"            // load configuration table
  };

  for(int i = 0; i < 11; i++)
    printf("  e32_unit:%3.3s   : %8.8lx %8.8lx\n", e32str[i], m_e32.e32_unit[i].rva, m_e32.e32_unit[i].size);
}

/*****************************************************************************
  dump_o32_sections()

  in:
    base - rom offset for printing purposes

  out:
    none

  desc:
    dumps the o32s
 *****************************************************************************/
void Module::dump_o32_sections(DWORD base){
  static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "o32") : NULL;
  if(suppress) return;

  for(O32List::iterator o32_itr = m_o32_list.begin(); o32_itr != m_o32_list.end(); o32_itr++){
    printf("Section %8.8s:\n", o32_itr->o32_name);
    printf("  o32_vsize      : %8.8lx\n", o32_itr->o32_vsize);
    printf("  o32_rva        : %8.8lx\n", o32_itr->o32_rva);
    printf("  o32_psize      : %8.8lx\n", o32_itr->o32_psize);
    printf("  o32_dataptr    : %8.8lx\n", o32_itr->o32_dataptr);
    printf("  o32_realaddr   : %8.8lx\n", o32_itr->o32_realaddr);
    printf("  o32_flags      : %8.8lx\n", o32_itr->o32_flags);
  
    for(unsigned long j = 0; j + 16 < o32_itr->min_size(); j += 16)
      printf("%8.8lx (%8.8lx): %8.8lx %8.8lx %8.8lx %8.8lx\n",
               o32_itr->o32_rva + base + j,
               j,
               *(LPDWORD)(o32_itr->data.ptr() + j),
               *(LPDWORD)(o32_itr->data.ptr() + j + 4),
               *(LPDWORD)(o32_itr->data.ptr() + j + 8),
               *(LPDWORD)(o32_itr->data.ptr() + j + 12));
  }
}

void Module::dump_toc(MemoryList::iterator mem_itr){
  if(!last_pass) return;
  
  printf("\nFirst DLL code Address:  %08x\n", mem_itr->dll_code_start);
  printf("Last DLL code Address:   %08x\n", mem_itr->dll_code_orig);
  printf("First DLL Address:       %08x\n", s_romhdr.dllfirst);
  printf("Last DLL Address:        %08x\n", s_romhdr.dlllast);
  printf("Physical Start Address:  %08x\n", s_romhdr.physfirst);
  printf("Physical End Address:    %08x\n", s_romhdr.physlast);
  printf("Start RAM:               %08x\n", s_romhdr.ulRAMStart);
  printf("Start of free RAM:       %08x\n", s_romhdr.ulRAMFree);
  printf("End of RAM:              %08x\n", s_romhdr.ulRAMEnd);
  printf("Number of Modules:       %lu\n",  s_romhdr.nummods);
  printf("Number of Copy Sections: %lu\n",  s_romhdr.ulCopyEntries);
  printf("Copy Section Offset:     %08x\n", s_romhdr.ulCopyOffset);
  
  if(s_romhdr.ulKernelFlags)
    printf("Kernel Flags:            %08x\n", 
           s_romhdr.ulKernelFlags);
  
  printf("FileSys 4K Chunks/Mbyte: %d <2Mbyte  %d 2-4Mbyte  %d 4-6Mbyte  %d >6Mbyte\n",
         LOBYTE(s_romhdr.ulFSRamPercent), 
         HIBYTE(s_romhdr.ulFSRamPercent),
         HIWORD(LOBYTE(s_romhdr.ulFSRamPercent)),
         HIWORD(HIBYTE(s_romhdr.ulFSRamPercent))
         );
  
  if(s_romhdr.ulDrivglobLen)       
    printf("Driver globals:          %08x, %08x\n", 
           s_romhdr.ulDrivglobStart, 
           s_romhdr.ulDrivglobLen);
  
  printf("CPU Type:                %8.4xh\n", 
         s_romhdr.usCPUType);
  
  if(s_romhdr.usMiscFlags)
    printf("Miscellaneous Flags:     %8.4xh\n", 
           s_romhdr.usMiscFlags);
  
  if(s_romhdr.pExtensions)     
    printf("Extensions Pointer:      %08x\n", 
           (DWORD)s_romhdr.pExtensions);
  
  if(s_romhdr.ulTrackingLen)       
    printf("Memory Tracking          %08x, %08x\n", 
           s_romhdr.ulTrackingStart, 
           s_romhdr.ulTrackingLen);
  
  printf("Total ROM size:          %08x (%10u)\n", 
         s_romhdr.physlast - s_romhdr.physfirst, 
         s_romhdr.physlast - s_romhdr.physfirst);
}

/*****************************************************************************
 * output stream routines
 *****************************************************************************/
ostream &operator<<(ostream &out_file, const Module &module){
  char  buffer[1024];
  
  static char *delim = getenv("ri_comma_delimit") ? "," : " ";
  
  sprintf(buffer, "%-14s%s%-16s%s%-40s%s%08x", 
          module.name().c_str(), delim,
          module.memory_iterator()->name().c_str(), delim,
          module.full_path().c_str(), delim,
          module.file_attributes());

  out_file << buffer;

  return out_file;
}

ostream &operator<<(ostream &out_file, const ModuleList &module_list){
  static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "module") : NULL;
  if(suppress) return out_file;
  
  out_file << "Module Name    Memory Type     Module Pathname\n"
              "-------------  --------------- ---------------------------------------\n";

  for(unsigned i = 0; i < module_list.size(); i++)
    cout << module_list[i] << endl;
  
  return out_file;
}

