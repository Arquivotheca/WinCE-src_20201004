//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "headers.h"

#include "parse.h"

#include "bin.h"
#include "compcat.h"

#include "sre.h"
#include "rom.h"

#include "pbtimebomb.h"

bool last_pass = true;

bool process_args(char *argv[], int argc, string &bib_file, string &output_file, bool &launch);
bool pass_state(Config &config, ModuleList &module_list, FileList &file_list, MemoryList &memory_list, MemoryList &reserve_list, ModuleList::iterator &kernel);
bool get_kernel(ModuleList::iterator &kernel, const ModuleList &module_list);
bool init_kernel(ModuleList::iterator &kernel, ModuleList &module_list, const MemoryList &memory_list, const Config &config);
bool compute_load_locataions(ModuleList &module_list, const MemoryList &reserve_list, Config &config);

bool compact_image(AddressList &hole_list, 
                   CopyList &copy_list, 
                   ModuleList &module_list, 
                   MemoryList &memory_list,
                   FileList &file_list, 
                   const MemoryList &reserve_list, 
                   const Config &config,
                   const MemoryList::iterator &xip_mem);

#define FATAL(x) do{ if(!(x)){ fprintf(stderr, "Fatal error hit, exiting...\n"); exit(1); } } while(0)

/*****************************************************************************/
int main(int argc, char *argv[]){
  if(getenv("d_break"))
    DebugBreak();

#ifdef PBTIMEBOMB
  cout << "\nWindows CE ROM Image Builder v4.0." << PBTIMEBOMB << " Copyright (c) Microsoft Corporation\n";
  cout << "Built: " __DATE__ " " __TIME__ << "\n\n";
  
  // check PB PID Timebomb
  if(!IsRomImageEnabled()){
    fprintf(stderr, "Error: failed PB timebomb check\n");
    exit(1);
  }
#else
  cout << "\nWindows CE ROM Image Builder v4.0.000 Copyright (c) Microsoft Corporation\n";
  cout << "Built: " __DATE__ " " __TIME__ << "\n\n";
#endif
  
  Config     config;
  // prereserve 100 memory spots because if someone adds a memory section after the modules are set STL
  // may realoc and invalidate all the memory_iterators in the files and modules.
  MemoryList memory_list;
  MemoryList reserve_list;
  ModuleList module_list;
  FileList   file_list;

  MemoryList::iterator mem_itr;
  ModuleList::iterator mod_itr;
  FileList::iterator file_itr;

  string bib_file;
  bool launch = false;
  bool bad_fixups = false;
  bool bad_fixups_fatal = false;

  // do command line arguments
  FATAL(process_args(argv, argc, bib_file, config.output_file, launch));

  // parse the bib file
  FATAL(ParseBIBFile(bib_file, memory_list, reserve_list, module_list, file_list, config));

  // this only needs to be done once
  for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) FATAL(mod_itr->sync_names(config.profile, config.copy_files));
  for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) FATAL(mod_itr->add_sig_files(file_list));

  // determine if we have any .rel files at all
  WIN32_FIND_DATA FindFileData;
  HANDLE hFind;

  string reloc_file = module_list[0].full_path();
  reloc_file.erase(reloc_file.rfind('\\') + 1);
  reloc_file += "*.rel";
  hFind = FindFirstFile(reloc_file.c_str(), &FindFileData);
     
  if(hFind == INVALID_HANDLE_VALUE){
    cerr << "Warning: No .rel files found, using old fixup style for all modules\n";

    for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) mod_itr->set_code_split(false);

    bad_fixups_fatal = true;
  }
  else
    FindClose(hFind);


  // find the kernel and make sure it's first.
  ModuleList::iterator kernel;

  for(kernel = module_list.begin(); kernel != module_list.end(); kernel++)
    if(kernel->name() == "nk.exe"){
      Module temp = *kernel;

      module_list.erase(kernel);

      module_list.insert(module_list.begin(), temp);
      
      break;
    }

  kernel = module_list.begin();

  // sort by exe and by split code sections
//  stable_partition(kernel + 1, module_list.end(), mem_fun_ref(Module::not_is_file_compressed));
  stable_partition(kernel + 1, module_list.end(), mem_fun_ref(Module::is_exe));
  stable_partition(kernel + 1, module_list.end(), mem_fun_ref(Module::is_code_split));
  stable_partition(kernel + 1, module_list.end(), mem_fun_ref(Module::fixup_like_kernel));

  reserve_list.sort();

  Config     config_backup(config);
  ModuleList module_list_backup(module_list);

  if(config.xipchain){
    Memory memory;
    StringList token_list;
    char buffer[10];

    int count = 1; // for the chain file itself
    for(mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++)
      if(mem_itr->type() == RAMIMAGE_TYPE || 
         mem_itr->type() == NANDIMAGE_TYPE || 
         mem_itr->type() == ROM8_TYPE  || 
         mem_itr->type() == ROMx8_TYPE || 
         mem_itr->type() == ROM16_TYPE)
        count++;
    
    token_list.push_back(CHAIN_INFORMATION);
    sprintf(buffer, "%08x", config.xipchain);
    token_list.push_back(buffer);
    sprintf(buffer, "%08x", count * sizeof(XIPCHAIN_SUMMARY));
    token_list.push_back(buffer);
    token_list.push_back(EXTENSION_TYPE);
    token_list.push_back(kernel->memory_iterator()->name());

    memory.set(token_list);

    memory_list.push_back(memory);
  }

  do{
    FATAL(pass_state(config, module_list, file_list, memory_list, reserve_list, kernel));

fixup_start:
    ModuleList::iterator modbak_itr = module_list_backup.begin();
    for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) modbak_itr++->set_code_split(mod_itr->is_code_split());

    stable_partition(module_list_backup.begin() + 1, module_list_backup.end(), mem_fun_ref(Module::is_code_split));

    // if we autosized and are now on the last pass restore the modules and config
    if(!config.autosize && config_backup.autosize){
      module_list = module_list_backup;
      
      DWORD tempxip = config.xipchain;
      config = config_backup;
      config.xipchain = tempxip;
      config.autosize = false;
    }
    else if(bad_fixups){
      module_list = module_list_backup;

      DWORD tempxip = config.xipchain;
      config = config_backup;
      config.xipchain = tempxip;
      
      bad_fixups_fatal = true;
    }
  
    FATAL(init_kernel(kernel, module_list, memory_list, config));

    // process the modules
    for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) FATAL(mod_itr->load());
    for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) FATAL(mod_itr->verify_cpu());

    // reset dll ranges    
    for(mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
      mem_itr->dll_code_start = mem_itr->dll_code_orig;
      mem_itr->dll_data_start = mem_itr->dll_data_orig;
      mem_itr->dll_data_split = 0;
      mem_itr->code_space_full = false;
    }
      
    FATAL(compute_load_locataions(module_list, reserve_list, config));

    // get symbols
    for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) 
      if(config.profile || mod_itr->is_kernel()) 
        FATAL(mod_itr->get_symbols(config.profile_all, memory_list));

    // relocate image      
    for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) FATAL(mod_itr->relocate_image());

    // do fixups
    for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) 
      if(!mod_itr->fixup()){
        if(bad_fixups_fatal){
          cerr << "Error: fatal fixup error, aborting....\n";
          exit(1);
        }

        static bool first = true;
        if(first){
          cerr << "Warning: .rel files are required to load the code section of modules into slot 1\n"
               << "         and therefor freeing up more virtual memory space in slot 0.\n";
          first = false;
        }
        
        cerr << "Warning: No .rel file found for module " << mod_itr->name() << ", using old fixup style." << endl;

        
        mod_itr->set_code_split(false);
        bad_fixups = 1;
      }
   

    if(bad_fixups && !bad_fixups_fatal)
      goto fixup_start;

    // remove discardables    
    for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) FATAL(mod_itr->remove_discardable_sections());

    // imports
    for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) FATAL(mod_itr->resolve_imports(module_list, config.error_late_bind));
    for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) FATAL(mod_itr->compress(config.compress));
    for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) FATAL(mod_itr->sig_adjust());
  
    // process the files
    for(file_itr = file_list.begin(); file_itr != file_list.end(); file_itr++) FATAL(file_itr->load());
    for(file_itr = file_list.begin(); file_itr != file_list.end(); file_itr++) FATAL(file_itr->compress(config.compress));

    CopyList copy_list;
    AddressList hole_list;

    // per region processing
    for(mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
      if(mem_itr->type() == RAMIMAGE_TYPE || mem_itr->type() == NANDIMAGE_TYPE || mem_itr->type() == ROM8_TYPE  || mem_itr->type() == ROMx8_TYPE || mem_itr->type() == ROM16_TYPE){
        LAST_PASS_PRINT cout << "Processing " << mem_itr->name() << endl;
        FATAL(compact_image(hole_list, copy_list, module_list, memory_list, file_list, reserve_list, config, mem_itr));

        for(MemoryList::iterator mem_itr2 = memory_list.begin(); mem_itr2 != memory_list.end(); mem_itr2++){
          // fixup kernel variables
          if(mem_itr->is_kernel()){
            if(mem_itr2->type() == FIXUPVAR_TYPE){
              if(mem_itr2->address())
                kernel->fixupvar(mem_itr2->fixupvar_section, mem_itr2->address(), mem_itr2->length());
              else
                cerr << "Warning: FixupVar " << mem_itr2->name() << " not found in kernel.  Variable not fixed up.\n";
            }
          }

          // set extension address
          if(mem_itr2->type() == EXTENSION_TYPE && mem_itr2->extension_location == mem_itr->name()){
            if(mem_itr->is_kernel() && kernel->is_kernel() && kernel->rom_extensions())
              kernel->fixupvar(0, 
                               kernel->rom_extensions() - kernel->memory_iterator()->address() + sizeof(EXTENSION) - sizeof(DWORD) - kernel->page_size(), 
                               mem_itr2->extension_offset);
            else if(mem_itr->is_kernel() && kernel->is_kernel())
              kernel->set_rom_extensions(mem_itr2->extension_offset);
            else
              Module::s_romhdr.pExtensions = (void*)mem_itr2->extension_offset;
          }
        }
        
        FATAL(write_bin(hole_list, copy_list, module_list, file_list, memory_list, reserve_list, kernel, config, mem_itr));
  
        // final pass stuff
        if(!config.autosize){
          static char *debug_nocomp = (debug_nocomp = getenv("ri_debug_info")) ? strstr(debug_nocomp, "nocomp") : NULL;
          
          // compact bin
          if(!debug_nocomp)
            compact_bin(config.output_file);
        }
      }
    }
  }while(config.autosize--);

  // process the chain file
  if(config.xipchain){
    if(config.output_file.rfind("\\") != string::npos)
      config.output_file = config.output_file.substr(0, config.output_file.rfind("\\"));
  
    config.output_file += "\\chain.bin";
    
    FATAL(write_chain(config, memory_list));

    FATAL(cat_bins(config.output_file));
    FATAL(compact_bin(config.output_file));
  }

  if(config.bSRE ||
     kernel->memory_iterator()->type() == ROM8_TYPE  ||
     kernel->memory_iterator()->type() == ROM16_TYPE ||
     kernel->memory_iterator()->type() == ROMx8_TYPE)
    FATAL(write_sre(config.output_file, kernel->memory_iterator()->type(), config.rom_info.offset, kernel->entry_rva() + kernel->memory_iterator()->address(), launch));

  // write the rom file
  if(config.rom_info.start ||
     config.rom_info.size ||
     config.rom_info.width){
    if(// config.rom_info.start &&     // not sure if start is allowed to be zero sometimes or not.  Could work.
       config.rom_info.size &&
       config.rom_info.width)
      FATAL(write_rom(config.output_file, config.rom_info));
    else
      fprintf(stderr, "Warning: ROMSIZE = %08x and ROMWIDTH = %d, both must be set to generate a ROM\n", config.rom_info.size, config.rom_info.width);
  }

  // post build warnings about private bits in the image
  for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++)
    if(mod_itr->warn_private_bits())
      cerr << "Warning: Module " << mod_itr->name() << " contains updated private code\n";

  cout << "Done!\n";
  
  return 0;
}

/*****************************************************************************/
bool process_args(char *argv[], int argc, string &bib_file, string &output_file, bool &launch){
  bib_file = getenv("_winceroot")  ? getenv("_winceroot") : "\\wince";
  bib_file += "\\release\\ram.bib";

  for(int i = 0; i < argc; i++){
    if(strnicmp(argv[i], "-o", 3) == 0)
      output_file = argv[i] + 3;
    else if(strnicmp(argv[i], "-g", 3) == 0)
      launch = true;
    else if(strnicmp(argv[i], "/?", 3) == 0 || argc == 1){
      printf("Usage:  romimage [options] filename.bib\n\n"
             "Valid options are:\n"
             "  -o output file - defaults to kernelname.bin\n"
             "  -g             - auto launch (only used when generating an sre file)\n"
             "  /?             - this usage statement\n\n"
             "Romimage also checks various environment variables\n"
             "  To suppress various output:\n"
             "    ri_suppress_info = all,move,module,file,memory,hole,e32,o32,symbol\n\n"
             "  To comma delimit some of the output lists for easier parsing:\n"
             "    ri_comma_delimit = 1\n\n");
      return false;
    }
    else if(argv[i][0] != '-')
      bib_file = argv[i];
  }

  char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "all") : NULL;
  if(suppress)
    _putenv("ri_suppress_info=all,move,module,file,memory,hole,e32,o32,symbol");

  return true;
}

/*****************************************************************************/
/*
  theory here is that we go through all the memory sections on the first pass 
  and make the ram start and size cover them all (no gaps allowed here).  

  on the second pass ...
 */
bool pass_state(Config &config, ModuleList &module_list, FileList &file_list, MemoryList &memory_list, MemoryList &reserve_list, ModuleList::iterator &kernel){
  static int pass = 0;

  static char *debug_show = (debug_show = getenv("ri_debug_info")) ? strstr(debug_show, "allpasses") : NULL;
  if(!debug_show)
    last_pass = !config.autosize; // is this the last pass.  Last pass happens when autosize is 0
  
  if(!config.autosize && !pass)
    return true;

  printf("\nPass %d...\n\n", ++pass);

  MemoryList::iterator mem_itr, mem_itr2, ram_section;
  DWORD start = 0xffffffff;
  DWORD size = 0;

  DWORD dll_code_start = 0;
  DWORD dll_code_end   = 0;
  DWORD dll_data_start = 0;
  DWORD dll_data_end   = 0;
  
// autosize stuff

//*********************************************//  
// multixip part
  if(config.xipchain){
    if(config.autosize){
      if(config.rom_autosize){
        // rom autosize
        for(mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
          if(mem_itr->type() == RAM_TYPE)
            ram_section = mem_itr;
          
          if(mem_itr->type() == RAMIMAGE_TYPE || mem_itr->type() == NANDIMAGE_TYPE || (mem_itr->type() == RAM_TYPE && config.ram_autosize)){
            start = min(start, mem_itr->address());
            size += mem_itr->length();
          }
        }

        // automoving the fixupvar for the chain location to the end of everything
        if(config.chainvar.size()){
          DWORD newchain = start + size;
        
          for(mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
            if(mem_itr->type() == FIXUPVAR_TYPE && mem_itr->name() == config.chainvar){
              mem_itr->Address::set(mem_itr->address(), newchain);
              break;
            }
          }
  
          // moving the reserved region
          for(mem_itr = reserve_list.begin(); mem_itr != reserve_list.end(); mem_itr++){
            if(mem_itr->address() == config.xipchain){
              mem_itr->Address::set(newchain, mem_itr->length());
              break;
            }
          }
  
          // move pointer to the end
          config.xipchain = newchain;
        }

        for(mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
          if(mem_itr->type() == RAMIMAGE_TYPE || mem_itr->type() == NANDIMAGE_TYPE || (mem_itr->type() == RAM_TYPE && config.ram_autosize)){
            mem_itr->Address::set(start, size);
          }
        }
      }
    }
    else{
      MemoryList::iterator last = memory_list.end();
      bool setchain = true;       // to prevent this from happening every ramimage section through
      
      for(last = memory_list.end(), mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
        if(mem_itr->type() == RAM_TYPE)
          ram_section = mem_itr;
        
        if(mem_itr->type() == RAMIMAGE_TYPE){
          if(config.rom_autosize){
            if(last != memory_list.end()){
              if(setchain && config.chainvar.size()){
                DWORD newchain = last->address_end();
                
                // automoving the fixupvar for the chain location to the end of everything
                for(mem_itr2 = memory_list.begin(); mem_itr2 != memory_list.end(); mem_itr2++){
                  if(mem_itr2->type() == FIXUPVAR_TYPE && mem_itr2->name() == config.chainvar){
                    mem_itr2->Address::set(mem_itr2->address(), newchain);
                    fprintf(stderr, "Warning: Changed fixupvar %s to point to new chain location %08x\n", mem_itr2->name().c_str(), newchain);
                    break;
                  }
                }

                // moving the reserved region
                for(mem_itr2 = reserve_list.begin(); mem_itr2 != reserve_list.end(); mem_itr2++){
                  if(mem_itr2->address() == config.xipchain){
                    mem_itr2->Address::set(newchain, mem_itr2->length());
                    fprintf(stderr, "Warning: Moved Chain file start to %08x\n", newchain);
                    break;
                  }
                }

                start = align_64k(last->address_end() + mem_itr2->length());
                
                // move pointer to the end
                config.xipchain = newchain;
                setchain = false;
              }
              else              
                start = align_64k(last->address_end());
            }
            else
              start = mem_itr->address();
            
            size = mem_itr->m_romhdr.physlast - mem_itr->m_romhdr.physfirst + mem_itr->rom_gap;
  
            mem_itr->Address::set(start, size);
          }

          last = mem_itr;
        }
      }
      
      if(config.ram_autosize){
        start = align_64k(start + size);

        if(start > ram_section->address())
          ram_section->Address::set(start, ram_section->address_end() - start);
        else
          ram_section->Address::set(start, ram_section->address_end());
      }    

      for(last = memory_list.end(), mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
        if(mem_itr->type() == NANDIMAGE_TYPE){
          if(config.rom_autosize){
            if(last != memory_list.end())
              start = align_64k(last->address_end());
            else
              start = align_64k(start + size);
            
            size = mem_itr->m_romhdr.physlast - mem_itr->m_romhdr.physfirst + mem_itr->rom_gap;
  
            mem_itr->Address::set(start, size);
          }

          last = mem_itr;
        }
      }

      for(last = memory_list.end(), mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
        if(mem_itr->type() == NANDIMAGE_TYPE || mem_itr->type() == RAMIMAGE_TYPE){
          if(config.dll_addr_autosize){
            if(last != memory_list.end()){
              mem_itr->dll_code_bottom = last->dll_code_bottom - (mem_itr->dll_code_orig - mem_itr->dll_code_start) - mem_itr->dll_code_gap;
              mem_itr->dll_data_bottom = last->dll_data_bottom - (mem_itr->dll_data_orig - mem_itr->dll_data_start);

              if(mem_itr->dll_code_bottom < Memory::m_DLL_CODE_BOTTOM_ADDR){
                mem_itr->dll_data_bottom -= Memory::m_DLL_CODE_BOTTOM_ADDR - mem_itr->dll_code_bottom;
                mem_itr->dll_code_bottom = Memory::m_DLL_CODE_BOTTOM_ADDR;
              }

              // if we're not padded in Module::set_base() --  (I know, this is getting really bad)
              if(!mem_itr->dll_data_split){
                mem_itr->dll_data_bottom -= mem_itr->dll_data_gap;
                
                mem_itr->dll_data_split = mem_itr->dll_data_bottom;
                
                mem_itr->dll_data_bottom -= mem_itr->dll_gap;
              }

              mem_itr->dll_code_bottom &= 0xffff0000;
              mem_itr->dll_data_bottom &= 0xffff0000;
              
              mem_itr->dll_code_orig = last->dll_code_bottom;
              mem_itr->dll_data_orig = last->dll_data_bottom; 
            }
            else{
              mem_itr->dll_code_bottom = mem_itr->dll_code_start - mem_itr->dll_code_gap;
              mem_itr->dll_data_bottom = mem_itr->dll_data_start - mem_itr->dll_data_gap;

              if(mem_itr->dll_code_bottom < Memory::m_DLL_CODE_BOTTOM_ADDR){
                mem_itr->dll_data_bottom -= Memory::m_DLL_CODE_BOTTOM_ADDR - mem_itr->dll_code_bottom;
                mem_itr->dll_code_bottom = Memory::m_DLL_CODE_BOTTOM_ADDR;
              }

              mem_itr->dll_code_bottom &= 0xffff0000;
              mem_itr->dll_data_bottom &= 0xffff0000;
            }

            last = mem_itr;
          }
        }
      }
    }
  }
//*********************************************//  
// non multixip part
  else{
    if(config.autosize){
      for(mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
        if(mem_itr->type() == RAM_TYPE)
          ram_section = mem_itr;
  
        if(mem_itr->type() == RAMIMAGE_TYPE || mem_itr->type() == RAM_TYPE){
          start = min(start, mem_itr->address());
          size += mem_itr->length();
        }
      }

      kernel->memory_iterator()->Address::set(start, size);
      ram_section->Address::set(start, size);
    }
    else{
      for(mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
        if(mem_itr->type() == RAM_TYPE){
          if(kernel->is_arm())
            start = align_64k(Module::s_romhdr.physlast);
          else
            start = kernel->align_page(Module::s_romhdr.physlast);
          
          size = mem_itr->address_end() - start;

          mem_itr->Address::set(start, size);
          printf("RAM AutoSize: RAM Start=%08x RAM Size=%08x\n", start, size);
          
          if(mem_itr->address() > mem_itr->address_end()){
            cerr << "Error: Image is too large for current RAM and RAMIMAGE settings" << endl;
            cerr << "Try setting IMGRAM64=1" << endl;
            return false;
          }
          
          break;
        }      
      }
    }
  }

  return true;
}

/*****************************************************************************/
bool init_kernel(ModuleList::iterator &kernel, ModuleList &module_list, const MemoryList &memory_list, const Config &config){
  for(MemoryList::const_iterator mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++)
    if(mem_itr->type() == RAM_TYPE){
      if(!kernel->init_kernel(config.kernel_fixups, *mem_itr))
        return false;

      bool ret = true;
      for(ModuleList::iterator mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++)
        if(mod_itr->fixup_like_kernel() && !mod_itr->memory_iterator()->is_kernel()){
          cerr << "Error: Module " << mod_itr->name() << " is kernel module but not in kernel region\n";
          ret = false;
        }

      return ret;
    }

  cerr << "Error: No ram section found\n";

  return false;
}

/*****************************************************************************/
bool compute_load_locataions(ModuleList &module_list, const MemoryList &reserve_list, Config &config){
  int offset = 0;
  for(ModuleList::iterator itr = module_list.begin(); itr != module_list.end(); itr++){
    itr->set_load_address(offset);

    offset += itr->get_load_size();

    itr->set_base(reserve_list);

    if(itr->is_dll()){
      static char *suppress = (suppress = getenv("ri_suppress_info")) ? strstr(suppress, "move") : NULL;
      if(!suppress){
        LAST_PASS_PRINT 
          printf("Module %-13s at offset %08x", 
               itr->name().c_str(), 
               itr->memory_iterator()->dll_data_start + File::page_size());
        
        if(itr->is_code_split()) 
          LAST_PASS_PRINT printf(" data, %08x code", itr->memory_iterator()->dll_code_start + File::page_size());

        LAST_PASS_PRINT printf("\n");
      }
    }
  }

  return true;
}

/*****************************************************************************/
bool compact_image(AddressList &hole_list, 
                   CopyList &copy_list, 
                   ModuleList &module_list, 
                   MemoryList &memory_list,
                   FileList &file_list, 
                   const MemoryList &reserve_list, 
                   const Config &config,
                   const MemoryList::iterator &xip_mem){
  DWORD next_avail = xip_mem->address();

  MemoryList::iterator mem_itr;
  ModuleList::iterator mod_itr;
  FileList::iterator file_itr;

  // clean up from previous passes
  hole_list.clear();
  copy_list.clear();

  // put the readony sections in place and find holes
  Module::print_header();
  for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) 
    if(mod_itr->memory_iterator() == xip_mem) 
      FATAL(mod_itr->move_readonly_sections(reserve_list, next_avail, hole_list));

  static char *debug_show = (debug_show = getenv("ri_debug_info")) ? strstr(debug_show, "holes") : NULL;

  // add holes around bin signature in non kernel images
  if(!xip_mem->is_kernel()){
    DWORD temp = xip_mem->address();
    hole_list.push_back(Address(temp, ROM_SIGNATURE_OFFSET));
    hole_list.rbegin()->set_hole();

    temp += ROM_SIGNATURE_OFFSET + 8;
    hole_list.push_back(Address(temp, xip_mem->address() + File::page_size() - temp));
    hole_list.rbegin()->set_hole();
  }  

  Address::size_sort(hole_list);

  if(debug_show)
    Address::dump(hole_list);

  // add great big hole at the end for everything else, actual size is arbitrary, just has to be BIG!
  // (AND HAS TO WATCH OUT FOR ANY RESERVED SECTIONS IN IT'S WAY)
  // check for reserve sections in the way, may have to be changed for XIP's also?
  if(config.xipchain){
    if(xip_mem->address_end() < next_avail){
      fprintf(stderr, "Error: No room left in memory section.  Image is too large for defined regions.\n");
      fprintf(stderr, "  Region %s from %08x to %08x, has grown to %08x\n",
              xip_mem->name().c_str(),
              xip_mem->address(),
              xip_mem->address_end(),
              next_avail);
            
      return false;
    }

    next_avail = File::align_page(next_avail);
    next_avail = max(next_avail, xip_mem->address() + File::page_size());
    next_avail = min(next_avail, xip_mem->address_end());
    
    if(reserve_list.empty() || reserve_list.begin()->address() >= xip_mem->address_end()){
      // reserve section is so far away that it's of no concern
      // we can just add our default size
      hole_list.push_back(Address(next_avail, xip_mem->address_end() - next_avail));
    }
    else{
      for(MemoryList::const_iterator hitr = reserve_list.begin(); 
          hitr != reserve_list.end() && hitr->address()  < xip_mem->address_end(); 
          hitr++){
        if(hitr->address() >= next_avail){
          hole_list.push_back(Address(next_avail, hitr->address() - next_avail));
          next_avail = Memory::find_next_gap(reserve_list, hitr->address(), 0);
        }
      }
  
      hole_list.push_back(Address(next_avail, xip_mem->address_end() - next_avail));
    }
  }
  else{
    next_avail = File::align_page(next_avail);
    
    if(reserve_list.empty() || reserve_list.begin()->address() >= next_avail + 0x08000000){
      // reserve section is so far away that it's of no concern
      // we can just add our default size
      hole_list.push_back(Address(next_avail, 0x08000000));
    }
    else{
      for(MemoryList::const_iterator hitr = reserve_list.begin(); hitr != reserve_list.end(); hitr++){
        if(hitr->address() >= next_avail){
          hole_list.push_back(Address(next_avail, hitr->address() - next_avail));
          next_avail = Memory::find_next_gap(reserve_list, hitr->address(), 0);
        }
      }
  
      hole_list.push_back(Address(next_avail, 0x08000000));
    }
  }

  for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) if(mod_itr->memory_iterator() == xip_mem) FATAL(mod_itr->move_readwrite_sections(hole_list, copy_list));
  for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) if(mod_itr->memory_iterator() == xip_mem) FATAL(mod_itr->move_eo32_structures(hole_list));

  for(mem_itr = memory_list.begin(); mem_itr != memory_list.end(); mem_itr++){
    if(mem_itr->type() == EXTENSION_TYPE){
      if(mem_itr->extension_location.empty() && xip_mem->is_kernel())
        mem_itr->set_extension_location(xip_mem->name());
      
      if(mem_itr->extension_location == xip_mem->name())
        FATAL(mem_itr->reserve_extension(hole_list));
    }
  }

  if(file_list.size()){
    File::print_header();
    for(file_itr = file_list.begin(); file_itr != file_list.end(); file_itr++) if(file_itr->memory_iterator() == xip_mem) FATAL(file_itr->move_locataion(hole_list));
  }

  Module::print_header();
  for(mod_itr = module_list.begin(); mod_itr != module_list.end(); mod_itr++) if(mod_itr->memory_iterator() == xip_mem) FATAL(mod_itr->move_name(hole_list));
  for(file_itr = file_list.begin(); file_itr != file_list.end(); file_itr++) if(file_itr->memory_iterator()   == xip_mem) FATAL(file_itr->move_name(hole_list));

  Address::dump(hole_list);

  return true;
}

