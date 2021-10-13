#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <assert.h>

#include "traverse.h"
#include "binutils.h"

static DWORD rom_offset;
static SIGNING_CALLBACK call_back;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool is_page_table(const ROMHDR *ptoc, o32_rom *po32){
  assert(ptoc && po32);
  
  // this function is here as a work-around for x86 platform
  // page table sections -- this section, part of nk.exe, is different in
  // ROM than it is in the BIN file so we can't hash it the following 
  // criteria are used to determine if a section is the page table (it is\
  // possible other sections could meet this criteria too, especiall if
  // there are lots of XIP modules
    
  // the section has the same psize and vsize and the real address is within the ROM boundary
  return po32->o32_vsize == po32->o32_psize    && 
         ptoc->physfirst <= po32->o32_realaddr && 
         ptoc->physlast  >= po32->o32_realaddr;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool process_modules(const ROMHDR *promhdr){
  assert(promhdr);

  bool ret = false;

  ROMHDR romhdr;
  read_image(&romhdr, (DWORD)promhdr, sizeof(ROMHDR));

  // TOC starts right after ROMHDR
  TOCentry *pmod = new TOCentry[romhdr.nummods];
  if(!pmod) return false;

  read_image(pmod, (DWORD)(promhdr + 1), sizeof(TOCentry) * romhdr.nummods);

  BYTE *buffer = NULL;
  DWORD size; 
  
  for(DWORD i = 0; i < romhdr.nummods; i++){
    e32_rom e32;
    read_image(&e32, pmod[i].ulE32Offset, sizeof(e32_rom));

    if(!call_back((DWORD)&pmod[i], sizeof(TOCentry))) return false;
    if(!call_back((DWORD)&e32, sizeof(e32_rom))) return false;

    for(int j = 0; j < e32.e32_objcnt; j++){
      o32_rom o32; 
      read_image(&o32, pmod[i].ulO32Offset + j * sizeof(o32_rom), sizeof(o32_rom));

      if(!call_back((DWORD)&o32, sizeof(o32_rom)))
        goto EXIT;

      if((IMAGE_SCN_MEM_WRITE & o32.o32_flags) && (IMAGE_SCN_COMPRESSED & o32.o32_flags))
        size = o32.o32_psize;
      else
        size = o32.o32_psize < o32.o32_vsize ? o32.o32_psize : o32.o32_vsize;

      if(size && !is_page_table(&romhdr, &o32)){
        buffer = new BYTE[size];
        if(!buffer) goto EXIT;

        read_image(buffer, o32.o32_dataptr, size);
        
        if(!call_back((DWORD)buffer, size))
          goto EXIT;

        delete[] buffer;
        buffer = NULL;
      }
    }
  }

  ret = true;
  
EXIT:
  if(buffer) delete[] buffer;
  if(pmod)   delete[] pmod;
  
  return ret;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool process_files(const ROMHDR *promhdr){
  assert(promhdr);

  bool ret = false;

  ROMHDR romhdr;
  read_image(&romhdr, (DWORD)promhdr, sizeof(ROMHDR));

  BYTE *buffer = NULL;
  FILESentry *pfile = new FILESentry[romhdr.numfiles];

  if(!pfile)
    return false;
  
  read_image(pfile, (DWORD)(promhdr + 1) + sizeof(TOCentry)*romhdr.nummods, sizeof(FILESentry) * romhdr.numfiles);

  for(DWORD i = 0; i < romhdr.numfiles; i++){
    if(!call_back((DWORD)&pfile[i], sizeof(FILESentry))) 
      return false;
    
    if(pfile[i].nCompFileSize){
      buffer = new BYTE[pfile[i].nCompFileSize];
      if(!buffer)
        goto EXIT;

      read_image(buffer, pfile[i].ulLoadOffset, pfile[i].nCompFileSize);
      
      if(!call_back((DWORD)buffer, pfile[i].nCompFileSize))
        goto EXIT;

      delete[] buffer;
      buffer = NULL;
    }
  }

  ret = true;

EXIT:
  if(buffer) delete[] buffer;
  if(pfile)  delete[] pfile;
  
  return ret;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool process_image(const ROMHDR *promhdr, DWORD _rom_offset, SIGNING_CALLBACK _call_back){
  bool ret = false;

  if(!promhdr)
    goto EXIT;

//DebugBreak();

  rom_offset = _rom_offset;
  call_back  = _call_back;

  ROMHDR romhdr;

  read_image(&romhdr, (DWORD)promhdr, sizeof(ROMHDR));
  
  if(!call_back((DWORD)&romhdr, sizeof(ROMHDR))) goto EXIT;
  if(!process_modules(promhdr)) goto EXIT;
  if(!process_files(promhdr))   goto EXIT;

  ret = true;

EXIT:
  return ret;
}

