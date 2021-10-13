//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "memory.h"
#include "file.h"
#include "token.h"

DWORD align_dword(DWORD addr) { return (addr + sizeof(DWORD) - 1) & ~(sizeof(DWORD) - 1); }
DWORD align_16k(DWORD addr) { return (addr + 0x04000 - 1) & ~(0x04000 - 1); }
DWORD align_64k(DWORD addr) { return (addr + 0x10000 - 1) & ~(0x10000 - 1); }

/*****************************************************************************
  use()

  in:
    DWORD size - size of memory to remove from range
    
  out:
    none

  desc:
    remove memory from range

    Note: this all assumes the the initial address and dest were dword aligned
 *****************************************************************************/
void Address::use(DWORD size){
  assert(size <= length());
    
  set(address() + align_dword(size), length() - align_dword(size));
}

/*****************************************************************************
  dump()

  in:
    address_list to dump
    
  out:
    none

  desc:
  
 *****************************************************************************/
void Address::dump(AddressList &address_list, bool holes_only){
  static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "hole") : NULL;
  if(suppress) return;
  
  DWORD sum = 0;
  int count = 0;

  AddressList temp = address_list;

  size_sort(temp);


  LAST_PASS_PRINT printf("\nUnfilled ROM holes (address, length):\n");
  
  static char *debug_show = (debug_show = getenv("ri_debug_info")) ? strstr(debug_show, "holes") : NULL;
  if(debug_show)
    holes_only = false;
  
  // starts at 1 to skip great big hole at the end
  for(AddressList::reverse_iterator titr = temp.rbegin(); titr != temp.rend(); titr++){
    if(!holes_only || titr->hole()){
      if(titr->length()){
        if(debug_show){
          printf("%08x %8d   ", titr->address(), titr->length());
          printf("%-3s", titr->hole() ? "h" : "");
          if(!(++count & 3)) printf("\n");
        }
        else{        
          LAST_PASS_PRINT printf("%08x %8d   ", titr->address(), titr->length());
          if(!(++count & 3)) LAST_PASS_PRINT printf("\n");
        }

        sum += titr->length();
      }
    }
  }
  
  LAST_PASS_PRINT printf("\ntotal space %d in %d ranges\n", sum, count);
}

void Address::size_sort(AddressList &address_list){
  vector<Address> temp;

  for(AddressList::iterator aitr = address_list.begin(); aitr != address_list.end(); aitr++)
    temp.push_back(*aitr);

  sort(temp.begin(), temp.end(), comp_size);

  address_list.clear();

  for(vector<Address>::iterator titr = temp.begin(); titr != temp.end(); titr++)
    address_list.push_back(*titr);
}

/*****************************************************************************
  find_range()

  in:
    memory_list - list to search
    len         - size of memory needed
    
  out:
    DWORD - address of first range that is of len size in memory list

  desc:
    finds the first range in the memory list that will fit the size required
******************************************************************************/
DWORD Memory::allocate_range(AddressList &address_list, DWORD len, bool *filler){
  for(AddressList::iterator addr_itr = address_list.begin(); addr_itr != address_list.end(); addr_itr++)
    if(len <= addr_itr->length()){
      DWORD temp = addr_itr->address();
      
      addr_itr->use(len);

      if(filler) *filler = addr_itr->hole();

     static char *debug_show = (debug_show = getenv("ri_debug_info")) ? strstr(debug_show, "holes") : NULL;
     if(debug_show)
        printf("allocating: addr %08x size %d\n", temp, len);
      
      return temp;
    }

  fprintf(stderr, "Failed to find a range for data of size %d\n", len);

  Memory::dump(address_list, false);

  return -1;
}

/*****************************************************************************
  find_next_gap()

  in:
    memory_list - list to search
    addr        - starting address
    len         - size of memory needed
    
  out:
    DWORD - address of first gap in memory list

  desc:
    finds the first gap in the memory list after the address specified that 
    will fit the size required

    !ASSUMES that the memory list is sorted by ADDRESS!
******************************************************************************/
DWORD Memory::find_next_gap(const MemoryList &reserve_list, DWORD addr, DWORD len, AddressList *address_list){
  addr = align_dword(addr);
  
  for(MemoryList::const_iterator ritr = reserve_list.begin(); ritr != reserve_list.end(); ritr++){
    if(ritr->intersects(addr, len) || (!len && ritr->address() == addr)){
      if(len){
        if(address_list){
          Address temp(File::align_page(addr), ritr->address() - File::align_page(addr));
          temp.set_hole();

          if(temp.length())
            address_list->push_back(temp);
        }

        fprintf(stderr, "Reserve area conflict, moving start %08x len %xh to ", addr, len);
      }

      addr = ritr->address_end(); // move past this section

      if(len)
        fprintf(stderr, "%08xh\n", addr);

      MemoryList::const_iterator next = ritr;
      next++;

      // if this doesn't intersect the next reserved section we're done
      if(next != reserve_list.end() && !next->intersects(addr, len))
        break;
    }
  }

  return align_dword(addr);
}

const DWORD Memory::m_DLL_DATA_TOP_ADDR     = 0x02000000;
const DWORD Memory::m_DLL_DATA_BOTTOM_ADDR  = 0x00600000;
const DWORD Memory::m_DLL_CODE_TOP_ADDR     = 0x04000000;
const DWORD Memory::m_DLL_CODE_BOTTOM_ADDR  = 0x02100000;

/*****************************************************************************
  Memory()

  in:
    varies
    
  out:
    none

  desc:
    initializes the class variables
 *****************************************************************************/
Memory::Memory(const string &s){
  m_name = s;
  m_type = "";
  
  m_kernel = false;

  memset(&m_romhdr, 0, sizeof(m_romhdr));

  dll_data_start  = m_DLL_DATA_TOP_ADDR;
  dll_data_orig   = m_DLL_DATA_TOP_ADDR;
  dll_data_bottom = m_DLL_DATA_BOTTOM_ADDR;

  dll_code_start  = m_DLL_CODE_TOP_ADDR;
  dll_code_orig   = m_DLL_CODE_TOP_ADDR;
  dll_code_bottom = m_DLL_CODE_BOTTOM_ADDR;

  dll_data_split = 0;

  code_space_full = false;

  dll_code_gap = 0;
  dll_data_gap = 0;
  rom_gap = 0x3000;  // will get set to zero in sanity check if not autosizing, or overriden if one is specified.
  dll_gap = 0;

  fixupvar_section = 0;
}

/*****************************************************************************
  valid_memory_type()

  in:
    string token - token to check
    
  out:
    boolean value indicating match

  desc:
    determines if the token is a valid (predefined) memory type
******************************************************************************/
bool Memory::valid_memory_type(const string &token){
  return token == ROM8_TYPE     || token == ROM16_TYPE    || 
         token == ROMx8_TYPE    || token == RAM_TYPE      || 
         token == RAMIMAGE_TYPE || token == NANDIMAGE_TYPE ||  
         token == RESERVED_TYPE ||
         token == FIXUPVAR_TYPE || 
         token == EXTENSION_TYPE;
}

/*****************************************************************************
  set()

  in:
    StringList token_list  - vector of string tokens from parsed line
    
  out:
    boolean value indicating success

  desc:
    initializes the class variables and sets the file attributes
******************************************************************************/
bool Memory::set(const StringList &token_list){
  if(token_list.size() < 3 || token_list.size() > 5){
    cerr << "Error: Incorrect number of tokens found parsing memory section\n";
    cerr << "  found: ";
    for(int i = 0; i < token_list.size(); i++)
      cerr << "'" << token_list[i] << "'  ";
    cerr << endl;
    return false;
  }    

  m_name    = token_list[0];
  m_address = Token::get_hex_value(token_list[1]);
  m_length  = Token::get_hex_value(token_list[2]);

  if(token_list.size() >= 4){
    m_type  = token_list[3];

    if(!valid_memory_type(m_type)){
      cerr << "Error: Invalid memory type '" << m_type << "'\n";
      return false;
    }
  }  
  else if(m_name == "reserve"){
    m_type = RESERVED_TYPE;
  }    
  else{
    cerr << "Error: Missing memory type field for memory range '" << m_name << "'\n";
    return false;
  }

  if(m_type == EXTENSION_TYPE && token_list.size() >= 5)
    extension_location = token_list[4];

  return true;
}

/*****************************************************************************
  reserve_extensions()

  in:

  out:

  desc:
 *****************************************************************************/
bool Memory::reserve_extension(AddressList &address_list){
  static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "move") : NULL;

  bool filler = false;
  extension_offset = Memory::allocate_range(address_list, align_dword(length() + sizeof(EXTENSION)), &filler);

  if(extension_offset == -1){
    fprintf(stderr, "Error: Ran out of space in ROM for %s\nsize %d\n", m_name.c_str(), align_dword(length() + sizeof(EXTENSION)));
    return false;
  }


  if(!suppress){
    static char *delim = getenv("ri_comma_delimit") ? "," : " ";

    LAST_PASS_PRINT
      printf("%-22s%s%-8s%s%08x%s%7u%s              %s\n",
           m_name.c_str(), delim,
           m_type.c_str(), delim,
           extension_offset, delim,
           length() + sizeof(EXTENSION), delim,
           filler ? "FILLER" : "");
  }
      
  return true;
}

/*****************************************************************************
  write_extensions()

  in:

  out:

  desc:
 *****************************************************************************/
bool Memory::write_extensions(ostream &out_file, const MemoryList &memory_list, const MemoryList &reserve_list, DWORD chain_addr, const Memory &xip_mem){
  DWORD last = 0;

  for(MemoryList::const_iterator mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
    if(mem_itr->type() != EXTENSION_TYPE || mem_itr->extension_location != xip_mem.name())
      continue;

    EXTENSION *pext = (EXTENSION*) new BYTE[sizeof(EXTENSION) + mem_itr->length()];
  
    if(!pext)
      return false;
  
    strncpy(pext->name, mem_itr->name().c_str(), sizeof(pext->name));

    pext->type = mem_itr->address();
    pext->pdata = (void*)(mem_itr->extension_offset + sizeof(EXTENSION));
    pext->length = 0;
    pext->reserved = mem_itr->length();

    memset(pext + 1, 0, pext->reserved);
    
    if(mem_itr->name() == CHAIN_INFORMATION){
      pext->type = 0;
      pext->length = mem_itr->length();

      XIPCHAIN_SUMMARY *summary = (XIPCHAIN_SUMMARY*)(pext + 1);
      
      int count = 0;
      for(MemoryList::const_iterator mem_itr2 = memory_list.begin(); mem_itr2 != memory_list.end(); mem_itr2++){
        if(mem_itr2->type() == RAMIMAGE_TYPE || mem_itr2->type() == NANDIMAGE_TYPE || mem_itr2->type() == ROM8_TYPE  || mem_itr2->type() == ROMx8_TYPE || mem_itr2->type() == ROM16_TYPE){
          // preincrement because the first one is for the chain file
          assert((count + 2) * sizeof(XIPCHAIN_SUMMARY) <= mem_itr->length());

          summary++;
          
          summary->pvAddr = (void*)mem_itr2->address();
          summary->dwMaxLength = align_64k(mem_itr2->length());
          summary->usFlags = ROMXIP_OK_TO_LOAD;
          summary->usOrder = count++;
        }
      }
      
      for(mem_itr2 = reserve_list.begin(); mem_itr2 != reserve_list.end(); mem_itr2++){
        if(mem_itr2->address() == chain_addr){
          XIPCHAIN_SUMMARY *summary2 = (XIPCHAIN_SUMMARY*)(pext + 1);
          
          summary2->pvAddr = (void*)mem_itr2->address();
          summary2->dwMaxLength = mem_itr2->length();
        }
      }
    }

    pext->pNextExt = (EXTENSION*)last;
    last = mem_itr->extension_offset;

    File::write_bin(out_file, mem_itr->extension_offset, pext, pext->reserved + sizeof(EXTENSION));
  
    delete[] pext;
    pext = NULL;
  }    

  return true;
}

/*****************************************************************************
  output streams for class Memory
******************************************************************************/
ostream &operator<<(ostream &out_file, const Memory &memory){
  char buffer[64];

  static char *delim = getenv("ri_comma_delimit") ? "," : " ";
  
  sprintf(buffer, "%-15s%s%08x%s%08x",
          memory.name().c_str(), delim,
          memory.address(), delim,
          memory.length());
  
  out_file << buffer;

  return out_file;
}

ostream &operator<<(ostream &out_file, const MemoryList &memory_list){
  static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "memory") : NULL;
  if(suppress) return out_file;
  
  out_file << "Memory Sect     Start    Size\n"
              "--------------- -------- --------\n";

  for(MemoryList::const_iterator mitr = memory_list.begin(); mitr != memory_list.end(); mitr++)
    cout << *mitr << endl;

  return out_file;
}

