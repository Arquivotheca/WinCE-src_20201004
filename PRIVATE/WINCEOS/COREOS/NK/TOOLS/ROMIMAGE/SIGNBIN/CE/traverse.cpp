#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <assert.h>

#include "traverse.h"

static DWORD rom_offset;
static SIGNING_CALLBACK call_back;

#define map_addr(addr) ((DWORD)(addr) + rom_offset)

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
  
  // TOC starts right after ROMHDR
  TOCentry *pmod = (TOCentry*)map_addr((promhdr + 1));
  DWORD size; 

  if(!pmod)
    return false;

  for(DWORD i = 0; i < promhdr->nummods; i++, pmod++){
    e32_rom  *pe32 = (e32_rom*)map_addr(pmod->ulE32Offset);
    o32_rom  *po32 = (o32_rom*)map_addr(pmod->ulO32Offset);

    if(!pe32 || !po32)
      return false;

    if(!call_back((DWORD)pmod, sizeof(TOCentry))) return false;
    if(!call_back((DWORD)pe32, sizeof(e32_rom))) return false;

    for(int j = 0; j < pe32->e32_objcnt; j++, po32++){
      if(!call_back((DWORD)po32, sizeof(o32_rom)))
        return false;

      if((IMAGE_SCN_MEM_WRITE & po32->o32_flags) && (IMAGE_SCN_COMPRESSED & po32->o32_flags))
        size = po32->o32_psize;
      else
        size = po32->o32_psize < po32->o32_vsize ? po32->o32_psize : po32->o32_vsize;

      if(size && !is_page_table(promhdr, po32))
        if(!call_back(map_addr(po32->o32_dataptr), size))
          return false;
    }
  }

  return true;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool process_files(const ROMHDR *promhdr){
  assert(promhdr);
  
  FILESentry *pfile = (FILESentry*)map_addr((DWORD)(promhdr + 1) + sizeof(TOCentry)*promhdr->nummods);
  
  for(DWORD i = 0; i < promhdr->numfiles; i++, pfile++){
    if(!call_back((DWORD)pfile, sizeof(FILESentry))) 
      return false;
    
    if(pfile->nCompFileSize)
      if(!call_back(map_addr(pfile->ulLoadOffset), pfile->nCompFileSize))
        return false;
  }

  return true;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool process_image(const ROMHDR *promhdr, DWORD _rom_offset, SIGNING_CALLBACK _call_back){
  bool ret = false;

  if(!promhdr)
    goto EXIT;

  rom_offset = _rom_offset;
  call_back  = _call_back;

  promhdr = (ROMHDR*)map_addr(promhdr);

  if(!call_back((DWORD)promhdr, sizeof(ROMHDR))) goto EXIT;
  if(!process_modules(promhdr)) goto EXIT;
  if(!process_files(promhdr))   goto EXIT;

  ret = true;

EXIT:
  return ret;
}


