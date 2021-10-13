//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "bin.h"

/*****************************************************************************/
bool write_bin(AddressList &hole_list, CopyList &copy_list,
                ModuleList &module_list, FileList &file_list, 
                MemoryList &memory_list, MemoryList &reserve_list,
                ModuleList::iterator &kernel, Config &config, MemoryList::iterator xip_mem){
  // open bin file
  if(config.output_file.empty())
    config.output_file = kernel->full_path();

  if(config.output_file.rfind("\\") != string::npos)
    config.output_file = config.output_file.substr(0, config.output_file.rfind("\\"));

  config.output_file += "\\" + xip_mem->name() + ".bin";

  ofstream image_file(config.output_file.c_str(), ios::trunc | ios::binary);
  if(image_file.bad()){
    cerr << "Error: Could not open '" << config.output_file << "' for writing\n";
    return false;
  }
  
  // write out bin file header
  DWORD image_length = 0;
  DWORD image_start = config.rom_info.offset + xip_mem->address();

  File::set_rom_offset(config.rom_info.offset);
  Module::write_bin(image_file, 0, 0, 0, false); // reset this function
  
  LAST_PASS_PRINT cout << "\nWriting " << config.output_file << endl;

  image_file.write("B000FF\n", 7);
  image_file.write((char *)&image_start, sizeof(DWORD));
  image_file.write((char *)&image_length, sizeof(DWORD));

  // clear up Module::s_romhdr
  Module::s_romhdr.nummods = 0;
  Module::s_romhdr.numfiles = 0;
  
  // dump the files and modules!
  DWORD romhdr_offset;
  Data temp;

  // get modules entries
  for(ModuleList::iterator mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++){
    if(mod_itr->memory_iterator() == xip_mem){
      Module::s_romhdr.nummods++;        
      TOCentry module_toc = mod_itr->get_TOC(); 
      temp.append(&module_toc, sizeof(module_toc));
    }
  }
  
  // get files entries
  for(FileList::iterator file_itr = file_list.begin(); file_itr != file_list.end(); file_itr++){
    if(file_itr->memory_iterator() == xip_mem){
      Module::s_romhdr.numfiles++;
      FILESentry file_toc = file_itr->get_TOC(); 
      temp.append(&file_toc, sizeof(file_toc));
    }
  }

  // set romhrd_offset for later.  
  // Romhdr needs to be written physically before the toc's and stuff but needs data from them so should
  // be written chronologically after them in the build process
  romhdr_offset = Memory::allocate_range(hole_list, sizeof(Module::s_romhdr) + temp.size());
  if(romhdr_offset == -1){
    fprintf(stderr, "Error: Ran out of space in ROM for %s\nsize %d", "ROMHDR/TOC", sizeof(Module::s_romhdr));
    return false;
  }
  
  // write module/file TOC data
  if(temp.size()){
    LAST_PASS_PRINT printf("Table of contents  %08x  %08x  (%10u)\n", romhdr_offset + sizeof(ROMHDR), temp.size(), temp.size());
    Module::write_bin(image_file, romhdr_offset + sizeof(ROMHDR), temp.ptr(), temp.size());
  }
  
  // write toc into kernel
  if(xip_mem->is_kernel() && kernel->is_kernel()) 
    kernel->write_TOC_ptr(romhdr_offset);
  
  // write modules data
  for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++)
    if(mod_itr->memory_iterator() == xip_mem)
      if(!mod_itr->write(image_file)) {
          fprintf(stderr, "Error: module write failed\n");
          return false;
      }

  // write files data
  for(file_itr = file_list.begin(); file_itr != file_list.end(); file_itr++)
    if(file_itr->memory_iterator() == xip_mem)
      file_itr->write(image_file); 
  
  // write out kernel signature
  DWORD signature[2];
  signature[0] = ROM_SIGNATURE;
  signature[1] = romhdr_offset;
  
  LAST_PASS_PRINT printf("Writing ROM signature and TOC pointer at %08x\n", xip_mem->address() + ROM_SIGNATURE_OFFSET);
  
  File::write_bin(image_file, xip_mem->address() + ROM_SIGNATURE_OFFSET, (char *)signature, sizeof(signature));

  // write profile information if applicable
  if(config.profile && xip_mem->is_kernel()){
    Data address_hitcount;
    Data symbol_name;
  
    DWORD next_avail = 0;
    DWORD index = 0;

    // funky two pass thing just to get next_avail and correctly use that to compute the Symbol address in the profile entry
    for(int i = 0; i <= 1; i++){
      temp.clear();
      address_hitcount.clear();
      symbol_name.clear();
      
      ProfileList profile_list;    
      DWORD current_offset = 0;

      MemoryList::iterator mem_itr;
      for(mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
        if(mem_itr->type() == RAMIMAGE_TYPE || mem_itr->type() == NANDIMAGE_TYPE){
          for(ModuleList::iterator mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++){
            if(mod_itr->memory_iterator()->name() == mem_itr->name()){
              current_offset += sizeof(PROFentry);
            }
          }
        }
      }

      index = 0;
      for(mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
        if(mem_itr->type() == RAMIMAGE_TYPE || mem_itr->type() == NANDIMAGE_TYPE){
          for(ModuleList::iterator mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++){
            if(mod_itr->memory_iterator()->name() == mem_itr->name()){
              profile_list.push_back(mod_itr->get_profile(i));
        
              profile_list[index].ulModNum = index;
              profile_list[index].ulHitAddress = Module::s_romhdr.ulRAMFree + current_offset;
                
              current_offset += profile_list[index].m_function_list.size() * 8;
        
              index++;
            }
          }
        }
      }

#if 0      
      for(ModuleList::iterator mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++){
        if(mod_itr->memory_iterator() == xip_mem)
          current_offset += sizeof(PROFentry);
      }
  
      index = 0;
      for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++){
        if(mod_itr->memory_iterator() == xip_mem){
          profile_list.push_back(mod_itr->get_profile(i));
    
          profile_list[index].ulModNum = index;
          profile_list[index].ulHitAddress = Module::s_romhdr.ulRAMFree + current_offset;
            
          current_offset += profile_list[index].m_function_list.size() * 8;
    
          index++;
        }
      }
#endif

      static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "symbol") : NULL;
      if(!suppress && i){
        LAST_PASS_PRINT printf("   Module   Section    Start     End       Hits     Symbols   HitAddr  SymAddr\n");
        LAST_PASS_PRINT printf("---------- ---------- -------- -------- ---------- ---------- -------- --------\n");
      }
  
      for(ProfileList::iterator prof_itr = profile_list.begin(); prof_itr != profile_list.end(); prof_itr++){
        if(!prof_itr->m_function_list.empty())
          prof_itr->ulSymAddress = next_avail + current_offset;
  
        for(FunctionList::iterator fun_itr = prof_itr->m_function_list.begin(); fun_itr != prof_itr->m_function_list.end(); fun_itr++){
          DWORD name_length = fun_itr->name.size() + 1;  // + 1 for the null
  
          current_offset += name_length;
  
          address_hitcount.append(fun_itr, 8);
          symbol_name.append(fun_itr->name.c_str(), name_length);
        }
  
        DWORD zero = 0;
        current_offset = align_dword(current_offset);
        symbol_name.append(&zero, align_dword(symbol_name.size()) - symbol_name.size()); // dword pad      
  
        if(!suppress && i){
          static char *delim = getenv("ri_comma_delimit") ? "," : " ";
    
          LAST_PASS_PRINT 
            printf("%10d%s%10d%s%08x%s%08x%s%10d%s%10d%s%08x%s%08x\n",
                 prof_itr->ulModNum, delim,
                 prof_itr->ulSectNum, delim,
                 prof_itr->ulStartAddr, delim,
                 prof_itr->ulEndAddr, delim,
                 prof_itr->ulHits, delim,
                 prof_itr->ulNumSym, delim,
                 prof_itr->ulHitAddress, delim,
                 prof_itr->ulSymAddress);
        }
  
        temp.append(prof_itr, sizeof(PROFentry));
      }
  
      temp.append(address_hitcount);
      temp.append(symbol_name);

      if(!i){
        next_avail = Memory::allocate_range(hole_list, temp.size());

        if(next_avail == -1){
          fprintf(stderr, "Error: Ran out of space in ROM for %s\nsize %d", "Profile section", temp.size());
          return false;
        }
      }
    }

    CopyEntry ce;

    Module::s_romhdr.ulProfileOffset = Module::s_romhdr.ulRAMFree;
    
    ce.ulSource  = next_avail;
    ce.ulDest    = Module::s_romhdr.ulRAMFree;
    ce.ulCopyLen = index * sizeof(PROFentry) + address_hitcount.size();
    ce.ulDestLen = index * sizeof(PROFentry) + address_hitcount.size();
    ce.m_memory_iterator = xip_mem;

    copy_list.push_back(ce);

    Module::s_romhdr.ulProfileLen = temp.size();
    Module::s_romhdr.ulRAMFree += temp.size();

    LAST_PASS_PRINT printf("Profile Header:           %08x  %08x  (%10u)\n", next_avail, index * sizeof(PROFentry), index * sizeof(PROFentry));
      
    Module::write_bin(image_file, next_avail, temp.ptr(), temp.size());
  }

  // write copylist information if applicable
  temp.clear();

  // clear this first
  Module::s_romhdr.ulCopyEntries = 0;
  Module::s_romhdr.ulCopyOffset  = 0;
  for(int i = 0; i < copy_list.size(); i++){
    if(copy_list[i].memory_iterator() == xip_mem){
      temp.append(&copy_list[i], sizeof(COPYentry)); // I really mean COPYentry and *NOT* the CopyEntry wrapper class
      Module::s_romhdr.ulCopyEntries++;
    }
  }

  if(Module::s_romhdr.ulCopyEntries){
    DWORD addr = Memory::allocate_range(hole_list, temp.size());
    if(addr == -1){
      fprintf(stderr, "Error: Ran out of space in ROM for %s\nsize %d", "CopyEntries", temp.size());
      return false;
    }

    Module::s_romhdr.ulCopyOffset  = addr;
  
    LAST_PASS_PRINT printf("Kernel data copy section  %08x  %08x  (%10u)\n", addr, temp.size(), temp.size());
    Module::write_bin(image_file, addr, temp.ptr(), temp.size());
  }
  
  // set glob stuff
  for(MemoryList::iterator ritr = reserve_list.begin(); ritr != reserve_list.end(); ritr++){
    if(ritr->name() == "drivglob"){
      Module::s_romhdr.ulDrivglobStart = ritr->address();
      Module::s_romhdr.ulDrivglobLen   = ritr->length();
    }

    if(ritr->name() == "tracking"){
      Module::s_romhdr.ulTrackingStart = ritr->address();
      Module::s_romhdr.ulTrackingLen   = ritr->length();
    }
  }

  if(xip_mem->dll_data_start < xip_mem->dll_data_bottom || xip_mem->dll_data_start > xip_mem->dll_data_orig){
    fprintf(stderr, "Error: Too much data space used by DLL's in MODULES section\n");
    fprintf(stderr, "  Current usage = %4dk, Maximum usage = %4dk.\n", 
            (xip_mem->dll_data_orig - xip_mem->dll_data_start) >> 10, 
            (xip_mem->dll_data_orig - xip_mem->dll_data_bottom) >> 10);
    fprintf(stderr, "  Reduce DLL usage or move some DLL's into the FILES section.\n");
    
    return false;
  }

  if(xip_mem->dll_code_start < xip_mem->dll_code_bottom || xip_mem->dll_code_start > xip_mem->dll_code_orig){
    fprintf(stderr, "Error: Too much code space used by DLL's in MODULES section\n");
    fprintf(stderr, "  Current usage = %4dk, Maximum usage = %4dk.\n", 
            (xip_mem->dll_code_orig - xip_mem->dll_code_start) >> 10, 
            (xip_mem->dll_code_orig - xip_mem->dll_code_bottom) >> 10);
    fprintf(stderr, "  Reduce DLL usage or move some DLL's into the FILES section.\n");

    return false;
  }
  
  // setup rest of romhdr
  if(!xip_mem->dll_data_split)
    xip_mem->dll_data_split = xip_mem->dll_data_start;

  if(!config.autosize && config.xipchain){
    static bool first = true;
    string config_list = config.output_file;
    
    if(config_list.rfind("\\") != string::npos)
      config_list = config_list.substr(0, config_list.rfind("\\"));
  
    config_list += "\\config.lst";

    ofstream list_file(config_list.c_str(), first ? ios::trunc : ios::app);
      
    if(list_file.bad()){
      cerr << "Error: Could not open '" << config_list << "' for writing\n";
      return false;
    }

    if(first){
      list_file << "CONFIG\n";
      first = false;
    }

    list_file << "DLLHIGHCODEADDR " << xip_mem->name() << "\t " << hex << (xip_mem->dll_code_orig  & 0xffff0000) << endl;
    list_file << "DLLLOWCODEADDR  " << xip_mem->name() << "\t " << hex << (xip_mem->dll_code_start & 0xffff0000) << endl;
    list_file << "DLLHIGHADDR     " << xip_mem->name() << "\t " << hex << (xip_mem->dll_data_orig  & 0xffff0000) << endl;
    list_file << "DLLLOWADDR      " << xip_mem->name() << "\t " << hex << (xip_mem->dll_data_start & 0xffff0000) << endl;
  }

  Module::s_romhdr.usMiscFlags = kernel->is_kernel_debugger_enabled() ? 1 : 0;
  
  if(xip_mem->is_kernel())
    Module::s_romhdr.pExtensions = (void *)kernel->rom_extensions();
  
  Memory::write_extensions(image_file, memory_list, reserve_list, config.xipchain, *xip_mem);
 
  Module::s_romhdr.dllfirst  = (HIWORD(xip_mem->dll_data_start) << 16) | HIWORD(xip_mem->dll_data_split);
  Module::s_romhdr.dlllast   = xip_mem->dll_data_orig;

  if(HIWORD(Module::s_romhdr.dllfirst) == LOWORD(Module::s_romhdr.dllfirst) && 
     HIWORD(Module::s_romhdr.dllfirst) == HIWORD(Module::s_romhdr.dlllast))
    Module::s_romhdr.dllfirst  &= 0xffff0000;

  Module::s_romhdr.physfirst = xip_mem->address();
  Module::s_romhdr.physlast  = max(Module::physlast(), romhdr_offset + sizeof(ROMHDR));

  /***** only the ROMHDR record can be written after here *****/
  /*****                   NO OTHER RECORDS               *****/

  Module::s_romhdr.ulKernelFlags  = config.rom_info.flags;
  Module::s_romhdr.ulFSRamPercent = config.fsram_percent;
  Module::s_romhdr.usCPUType      = kernel->cpu_id();

  Module::s_romhdr.ulRAMFree = kernel->align_page(Module::s_romhdr.ulRAMFree);

  LAST_PASS_PRINT printf("ROM Header                %08x  %08x  (%10u)\n", romhdr_offset, sizeof(Module::s_romhdr), sizeof(Module::s_romhdr));
  Module::dump_toc(xip_mem);
  memcpy(&xip_mem->m_romhdr, &Module::s_romhdr, sizeof(Module::s_romhdr));

  // check for overlapped image/ram condition
  if(!config.autosize && xip_mem->type() != NANDIMAGE_TYPE){
    Address temp(Module::s_romhdr.ulRAMFree, Module::s_romhdr.ulRAMEnd - Module::s_romhdr.ulRAMFree);

    if(temp.intersects(Module::s_romhdr.physfirst, Module::s_romhdr.physlast - Module::s_romhdr.physfirst)){
      fprintf(stderr, "Error: Ram start overlaps rom binary\n");
      fprintf(stderr, "Rom end  : 0x%08x\n", Module::s_romhdr.physlast);
      fprintf(stderr, "Ram start: 0x%08x\n", Module::s_romhdr.ulRAMFree);

      for(MemoryList::iterator mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
        if(mem_itr->m_romhdr.physfirst && mem_itr->m_romhdr.physlast && mem_itr->m_romhdr.ulRAMFree){
          fprintf(stderr, "%s\n", mem_itr->name().c_str());
          fprintf(stderr, "physfirst %08x\n", mem_itr->m_romhdr.physfirst);
          fprintf(stderr, "physlast  %08x\n", mem_itr->m_romhdr.physlast);
          fprintf(stderr, "ulRAMFree %08x\n", mem_itr->m_romhdr.ulRAMFree);
        }
      }

      return false;      
    }
  }

  // write romhdr
  Module::write_bin(image_file, romhdr_offset, &Module::s_romhdr, sizeof(Module::s_romhdr));

  // write some other misc stuff
  DWORD start_ip = xip_mem->address() + kernel->entry_rva();
    
  if(xip_mem->is_kernel() && kernel->is_kernel()){
    if(config.bootjump)
      kernel->write_prolog(image_file, start_ip, config.bootjump_addr, config.user_reset_vector);

    if(config.x86boot && start_ip){
      unsigned short jump_offset;
  
      if(config.x86boot_addr)
        jump_offset = LOWORD(config.x86boot_addr) - LOWORD(0xfff3);
      else{
        jump_offset = LOWORD(start_ip) - LOWORD(0xfff3);
    
        if(HIWORD(start_ip) != 0xffff)
          cerr << "Error: Address of startup not in last 64k of x86 image." << endl;
      }
      
      unsigned char x86_boot_record[] = { 0xe9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xaa };

      memcpy(x86_boot_record + 1, &jump_offset, sizeof(jump_offset));

#if 0
      if(kernel->memory_iterator()->address() + kernel->memory_iterator()->length() > Module::s_romhdr.physlast){        
        fprintf(stderr, 
                "Error: bootjump (%08x) record was outside of rom region (%08x-%08x)",
                kernel->memory_iterator()->address() + kernel->memory_iterator()->length() - 16,
                Module::s_romhdr.physfirst,
                Module::s_romhdr.physlast);
        return false;
      }
#endif

      Module::write_bin(image_file, 
                        kernel->memory_iterator()->address() + kernel->memory_iterator()->length() - 16,
                        x86_boot_record, 
                        sizeof(x86_boot_record), false);
    }
  }
  
  // write out image start info
  DWORD zero = 0;

  LAST_PASS_PRINT printf("Starting ip:             %08x\n", start_ip);

  start_ip += config.rom_info.offset;

  DWORD size = 0;
  for(FileList::iterator fitr = file_list.begin(); fitr != file_list.end(); fitr++) size += fitr->size();
  LAST_PASS_PRINT printf("Raw files size:          %08x\n", size);

  size = 0;
  for(fitr = file_list.begin(); fitr != file_list.end(); fitr++) size += fitr->compressed_size();
  LAST_PASS_PRINT printf("Compressed files size:   %08x\n", size);

  if(!config.autosize){
    if(0 && config.rom_autosize && config.rom_info.size){
      if(Module::s_romhdr.physlast > config.rom_info.offset + config.rom_info.size + config.rom_info.start){
        fprintf(stderr, "Error: Image exceeds specified memory size by %u bytes and may not run.\n",
                Module::s_romhdr.physlast - (config.rom_info.offset + config.rom_info.size));
        return false;
      }  
    }
    else{
      for(MemoryList::iterator mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
        if(Module::s_romhdr.physfirst < mem_itr->address() && mem_itr->address() < Module::s_romhdr.physlast ||
           Module::s_romhdr.physfirst < mem_itr->address_end() && mem_itr->address_end() < Module::s_romhdr.physlast){
          if(mem_itr->type() == RAM_TYPE && config.error_too_big){
            fprintf(stderr, "Error: Image exceeds specified memory size by %u bytes and may not run.\n",
                    Module::s_romhdr.physlast - mem_itr->address_end());
            return false;
          }
          else if(mem_itr->type() == RAMIMAGE_TYPE){
            fprintf(stderr, "Warning: Image exceeds specified memory size by %u bytes and may not run.\n",
                    Module::s_romhdr.physlast - mem_itr->address_end());
          }
        }
      }
    }
  }

  if(!config.autosize){
    if(xip_mem->is_kernel()){
      image_file.write((char *)&zero, sizeof(DWORD));
      image_file.write((char *)&start_ip, sizeof(DWORD));
      image_file.write((char *)&zero, sizeof(DWORD));
    }
    else{
      image_file.write((char *)&zero, sizeof(DWORD));
      image_file.write((char *)&zero, sizeof(DWORD));
      image_file.write((char *)&zero, sizeof(DWORD));
    }
  }

  // fixup length
  image_length = Module::s_romhdr.physlast - Module::s_romhdr.physfirst;
  if(!config.autosize){
    image_file.seekp(11);
    image_file.write((char *)&image_length, sizeof(DWORD));
  }

  return true;
}

/*****************************************************************************/
bool write_chain(const Config &config, const MemoryList &memory_list){
  string chain_file_name = config.output_file;
  
  ofstream image_file(chain_file_name.c_str(), ios::trunc | ios::binary);
  if(image_file.bad()){
    cerr << "Error: Could not open '" << chain_file_name << "' for writing\n";
    return false;
  }
  
  if(chain_file_name.rfind("\\") != string::npos)
    chain_file_name = chain_file_name.substr(0, chain_file_name.rfind("\\"));

  chain_file_name += "\\chain.lst";

  ofstream list_file(chain_file_name.c_str(), ios::trunc);
  if(list_file.bad()){
    cerr << "Error: Could not open '" << chain_file_name << "' for writing\n";
    return false;
  }
  
  const DWORD zero = 0;
  
  image_file.write("B000FF\n", 7);

  DWORD chain_start = config.xipchain + config.rom_info.offset;
  image_file.write((char *)&chain_start, sizeof(DWORD));
  image_file.write((char *)&zero, sizeof(DWORD));  // temporary size holder

  DWORD crc = 0;
  DWORD order = 0;

  Data xip_data(&zero, sizeof(DWORD));  // reserve room for count at the head

  for(MemoryList::const_iterator mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
    if(mem_itr->type() != RAMIMAGE_TYPE && mem_itr->type() != NANDIMAGE_TYPE)
      continue;

    XIPCHAIN_ENTRY xip_entry = {0};
    xip_entry.pvAddr      = (LPVOID)mem_itr->address();
    xip_entry.dwLength    = mem_itr->m_romhdr.physlast - mem_itr->m_romhdr.physfirst;
    xip_entry.dwMaxLength = mem_itr->length();
    xip_entry.usFlags     = ROMXIP_OK_TO_LOAD;
    xip_entry.usOrder     = ++order;

    strncpy(xip_entry.szName, mem_itr->name().c_str(), mem_itr->name().size() + 1);

    xip_data.append(&xip_entry, sizeof(xip_entry));
    (*(DWORD*)xip_data.ptr())++; // increment region count at start of buffer

    if(mem_itr->is_kernel())
      list_file << "+";
      
    list_file << mem_itr->name() << ".bin" << endl;
  }

  // zero to end the chain
  xip_data.append(&zero, sizeof(DWORD));

  list_file << "chain.bin\n";

  File::write_bin(image_file, config.xipchain + ROM_CHAIN_OFFSET, xip_data.ptr(), xip_data.size());

  // three more dwords to close out the image
  image_file.write((char *)&zero, sizeof(DWORD));
  image_file.write((char *)&zero, sizeof(DWORD));
  image_file.write((char *)&zero, sizeof(DWORD));

  // and one more for good luck
  image_file.write((char *)&zero, sizeof(DWORD));

  // fixup length
  DWORD image_length = xip_data.size();
  image_file.seekp(11);
  image_file.write((char *)&image_length, sizeof(DWORD));

  string privkey = config.output_file;
  
  if(privkey.rfind("\\") != string::npos)
    privkey = privkey.substr(0, privkey.rfind("\\"));

  privkey += "\\privkey.dat";
  
  char cmd_line[MAX_PATH];

  sprintf(cmd_line, "bin2xip.exe %s %s", config.output_file.c_str(), privkey.c_str());

  STARTUPINFO suDummy;
  memset(&suDummy, 0, sizeof(suDummy));
  PROCESS_INFORMATION piDummy;

  if(!CreateProcess(0, cmd_line, NULL, NULL, TRUE, 0, NULL, "\\", &suDummy, &piDummy)){
    fprintf(stderr, "Failed calling %s\n", cmd_line);
    // return false;
  }
  else{
    WaitForSingleObject(piDummy.hProcess, INFINITE);
  }
  
  return true;
}

