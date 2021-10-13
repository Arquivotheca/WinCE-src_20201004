// reloc.cpp

#include "diffbin.h"

BOOL
HeaderMagicOk(const e32_exe& e32)
/*---------------------------------------------------------------------------*\
 * REVIEW: should this check for the two zero bytes also?
\*---------------------------------------------------------------------------*/
{
    return strcmp(reinterpret_cast<const char*>(&e32.e32_magic[0]),"PE") == 0;

} /* HeaderMagicOk()
   */

HRESULT
DoFixups(CModuleData *pOldMod, CModuleData *pNewMod, LPBYTE pbFixupBuffer,
         DWORD dwLen)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT             hr              = NOERROR;
    o32_rom *           dataptr;
    struct info *       blockptr;
    struct info *       blockstart;
    ULONG               blocksize       = dwLen;
    LPDWORD             FixupAddress;
    LPWORD              currptr;
    WORD                curroff;
    DWORD               offset;
    BOOL                MatchedReflo    =FALSE;
    int                 iLoop;
    LPBYTE              pbO32Addr;
    CSectionData *      pSection        = NULL;

    RETAILMSG(ZONE_FIXUP_VERBOSE, (TEXT("DoFixups: Old e32_vbase=0x%08lx, New e32_vbase=0x%08lx\n"),
        pOldMod->GetE32()->e32_vbase, pNewMod->GetE32()->e32_vbase));

    blockstart = blockptr = (struct info *)pbFixupBuffer;
    while (((ULONG)blockptr < (ULONG)blockstart + blocksize) && blockptr->size) {
        currptr = (LPWORD)(((ULONG)blockptr)+sizeof(struct info));
        if ((DWORD)currptr >= ((DWORD)blockptr+blockptr->size)) {
            blockptr = (struct info *)(((DWORD)blockptr)+blockptr->size);
            continue;
        }
        dataptr = 0;
        while ((ULONG)currptr < ((ULONG)blockptr+blockptr->size)) {
            curroff = *currptr&0xfff;
            if (!curroff && !blockptr->rva) {
                currptr++;
                continue;
            }
            if (!dataptr || (dataptr->o32_rva > blockptr->rva + curroff) ||
                (dataptr->o32_rva + dataptr->o32_vsize <= blockptr->rva + curroff))
            {
                for (iLoop = 0; iLoop < pNewMod->GetE32()->e32_objcnt; iLoop++) {
                    dataptr = pNewMod->GetO32(iLoop);
                    if ((dataptr->o32_rva <= blockptr->rva + curroff) &&
                        (dataptr->o32_rva+dataptr->o32_vsize > blockptr->rva + curroff))
                    {
                        // Got new O32 section... Got to go and find it.
                        offset = pOldMod->GetO32(iLoop)->o32_realaddr - pNewMod->GetO32(iLoop)->o32_realaddr;

                        RETAILMSG(ZONE_FIXUP_VERBOSE, (TEXT("DoFixups: Offset is %8.8lx\n"), offset));

                        pSection = NULL;

                        CHR(pNewMod->GetParentImage()->FindDataPtr(dataptr->o32_dataptr,
                                                                   min(dataptr->o32_psize, dataptr->o32_vsize),
                                                                   &pSection, &pbO32Addr));

                        ASSERT(pSection);
                        ASSERT(pbO32Addr);

                        RETAILMSG(ZONE_FIXUP_VERBOSE, (TEXT("DoFixups: SectionAddr=0x%08lx  SectionPtr=0x%08lx  O32Addr=0x%08lx\n"),
                            pSection->GetSectionHeader()->Address, pSection->GetUncompressedDataPtr(), pbO32Addr));

                        CHR(pSection->BeginReloc(offset));
                        break;
                    }
                }
                CBRA(iLoop != pNewMod->GetE32()->e32_objcnt);
            }
            FixupAddress = (LPDWORD)(blockptr->rva - dataptr->o32_rva +
                (*currptr&0xfff) + pbO32Addr);

            RETAILMSG(ZONE_FIXUP_VERBOSE, (TEXT("DoFixups: type %d, low %8.8lx, page %8.8lx, addr %8.8lx, op %8.8lx\n"),
                (*currptr>>12)&0xf, (*currptr)&0x0fff,blockptr->rva,FixupAddress,*FixupAddress));

            switch ((*currptr >> 12) & 0xF)  {
                                        // No location is necessary; reference is to an absolute
                                        // value
                case IMAGE_REL_BASED_ABSOLUTE:
                    break;

                                        // Word - (32-bits) relocate the entire address.
                case IMAGE_REL_BASED_HIGHLOW:
                    CHR(pSection->AddReloc((DWORD)FixupAddress - (DWORD)pSection->GetUncompressedDataPtr()));
                    *FixupAddress += offset;
                    break;

                case IMAGE_REL_BASED_HIGHADJ:
                case IMAGE_REL_BASED_MIPS_JMPADDR:
                case IMAGE_REL_BASED_MIPS_JMPADDR16:
                case IMAGE_REL_BASED_HIGH:
                case IMAGE_REL_BASED_LOW:
                default :
                    RETAILMSG(1, (TEXT("DoFixups : error : Unsupported fixup type!!! type=%d\n"), (*currptr >> 12) & 0xF));
                    CHRA(E_FAIL);
                    break;
            }
            RETAILMSG(ZONE_FIXUP_VERBOSE, (TEXT("DoFixups: reloc complete, new op %8.8lx\n"), *FixupAddress));
            currptr++;
        }
        blockptr = (struct info *)(((ULONG)blockptr)+blockptr->size);
    }
Error:
    return hr;

} /* DoFixups()
   */

#if 0
// Old code to support ALL reloc types
// Current code ONLY supports IMAGE_REL_BASE_HIGHLOW

            switch ((*currptr >> 12) & 0xF)  {
                                        // MIPS relocation Types:

                                        // No location is necessary; reference is to an absolute
                                        // value
                case IMAGE_REL_BASED_ABSOLUTE:
                    break;

                                        // Reference to the upper 16 bits of a 32-bit address.
                                        // Save the address and go to get REF_LO paired with this one
                case IMAGE_REL_BASED_HIGH:
                    FixupAddressHi = (LPWORD)FixupAddress;
                    MatchedReflo = TRUE;
                    break;

                                        // Direct reference to a 32-bit address. With RISC
                                        // architecture, such an address cannot fit into a single
                                        // instruction, so this reference is likely pointer data
                                        // Low - (16-bit) relocate high part too.
                case IMAGE_REL_BASED_LOW:
                    if (MatchedReflo) {
                        FixupValue = (DWORD)(long)((*FixupAddressHi) << 16) +
                            *(LPWORD)FixupAddress + offset;
                        *FixupAddressHi = (short)((FixupValue + 0x8000) >> 16);
                        MatchedReflo = FALSE;
                    } else {
                        FixupValue = *(short *)FixupAddress + offset;
                    }
                    *(LPWORD)FixupAddress = (WORD)(FixupValue & 0xffff);
                    break;

                                        // Word - (32-bits) relocate the entire address.
                case IMAGE_REL_BASED_HIGHLOW:
                    *FixupAddress += offset;
                    break;

                                        // 32 bit relocation of 16 bit high word, sign extended
                case IMAGE_REL_BASED_HIGHADJ:
                    RETAILMSG(ZONE_FIXUP, (TEXT("DoFixups: Grabbing extra data %8.8lx\n"),*(currptr+1)));
                    *(LPWORD)FixupAddress += (WORD)((  SignExtendShortToLong(*(++currptr)) +offset+0x00008000)>>16);
                    break;

                                        // Reference to the low portion of a 32-bit address.
                                        // jump to 26 bit target (shifted left 2)
                case IMAGE_REL_BASED_MIPS_JMPADDR:
                    FixupValue = ((*FixupAddress)&0x03ffffff) + (offset>>2);
                    *FixupAddress = (*FixupAddress & 0xfc000000) | (FixupValue & 0x03ffffff);
                    break;
                case IMAGE_REL_BASED_MIPS_JMPADDR16: // MIPS16 jal/jalx to 26 bit target (shifted left 2)
                    FixupValue = (*(LPWORD)FixupAddress) & 0x03ff;
                    FixupValue = (((FixupValue >> 5) | ((FixupValue & 0x1f) << 5)) << 16) | *((LPWORD)FixupAddress+1);
                    FixupValue += offset >> 2;
                    *((LPWORD)FixupAddress+1) = (WORD)(FixupValue & 0xffff);
                    FixupValue = (FixupValue >> 16) & 0x3ff;
                    *(LPWORD)FixupAddress = (WORD)((*(LPWORD)FixupAddress & 0x1c00) | (FixupValue >> 5) | ((FixupValue & 0x1f) << 5));
                    break;
                default :
                    RETAILMSG(ZONE_FIXUP, (TEXT("romimage.c(DoFixups) : error : Not doing fixup type %d\n"),*currptr>>12));
                    break;
            }
#endif //0





BOOL
RelocateImageFile(CModuleData *pOldMod, CModuleData *pNewMod, LPCSTR szFilename)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT                 hr              = NOERROR;
    HANDLE                  hf              = INVALID_HANDLE_VALUE;
    ULONG                   pOffset;
    e32_exe                 e32;            // exe header buffer
    DWORD                   dwBytesRead;
    LONG                    lPosHigh = 0;
    DWORD                   dwLoop;
    DWORD                   dwFixupSize     = 0;
    LPBYTE                  pbFixupBuffer   = NULL;
    o32_obj                 o32;

	RETAILMSG(ZONE_VERBOSE, (TEXT("Relocating %s\n"), szFilename));

    // Try to load .rel file here, if we fail, default to old behavior
    hr = DoFixupsRel(pOldMod, pNewMod, szFilename);
    if (SUCCEEDED(hr))
    {
        return TRUE;
    }

	RETAILMSG(ZONE_VERBOSE, (TEXT("REL file not found, using relocations from DLL %s\n"), szFilename));
    // Old behavior- use fixup information inside the dll.

    // attempt to open image file
    CBR((hf = CreateFile(
            szFilename,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,                   // security
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            INVALID_HANDLE_VALUE
            )) != INVALID_HANDLE_VALUE);

    // seek to the offset of the e32 structure
    CBR(SetFilePointer(hf, 0x3c, &lPosHigh, FILE_BEGIN) != 0xffffffff);

    // read in the e32 offset
    CBR(ReadFile(
            hf,
            &pOffset,
            sizeof(ULONG),
            &dwBytesRead,
            NULL
            ));

    // seek to the e32 structure
    CBR(SetFilePointer(hf, pOffset, &lPosHigh, FILE_BEGIN) != 0xffffffff);

    // read in the e32 structure
    CBR(ReadFile(
            hf,
            &e32,
            sizeof(e32),
            &dwBytesRead,
            NULL));

    CBR(HeaderMagicOk(e32));

    dwFixupSize = e32.e32_unit[FIX].size;

    if(dwFixupSize == 0) {
        // No fixups means no work to do!
        hr = NOERROR;
        goto Error;
    }

    CPR(pbFixupBuffer = (LPBYTE)LocalAlloc(LMEM_FIXED, dwFixupSize));

    //
    // Load the fixups
    //

    // Find the O32 section which contains the fixups, and load them into pbFixupBuffer
    for (dwLoop = 0; dwLoop < e32.e32_objcnt; dwLoop++) {

        // read in o32 structure
        CBR(SetFilePointer(hf, pOffset+sizeof(e32_exe)+dwLoop*sizeof(o32_obj), &lPosHigh, FILE_BEGIN) != 0xffffffff);

        CBR(ReadFile(
                hf,
                (LPBYTE)&o32,
                sizeof(o32),
                &dwBytesRead,
                NULL));

        if (o32.o32_rva <= e32.e32_unit[FIX].rva &&
            o32.o32_rva + o32.o32_vsize >= e32.e32_unit[FIX].rva &&
            o32.o32_rva <= e32.e32_unit[FIX].rva + e32.e32_unit[FIX].size &&
            o32.o32_rva + o32.o32_vsize >= e32.e32_unit[FIX].rva + e32.e32_unit[FIX].size)
        {
            CBRA(o32.o32_psize && o32.o32_dataptr);
            CBRA(o32.o32_psize >= dwFixupSize);

            CBR(SetFilePointer(hf, o32.o32_dataptr + e32.e32_unit[FIX].rva - o32.o32_rva, &lPosHigh, FILE_BEGIN) != 0xffffffff);
            CBR(ReadFile(
                    hf,
                    pbFixupBuffer,
                    dwFixupSize,
                    &dwBytesRead,
                    NULL));

            break;
        }
    }

    CloseHandle(hf);                    // close the image file

    //
    // Apply the fixups
    //
    DoFixups(pOldMod, pNewMod, pbFixupBuffer, dwFixupSize);

Error:
    return hr;

} /* RelocateImageFile()
   */

HRESULT
ApplyReverseFixups(CImageData *pimgOld, CImageData *pimgNew)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr          = NOERROR;
    CModuleData *   pOldMod     = NULL;
    CModuleData *   pNewMod     = NULL;
    int             iModOld;
    int             i;
    //
    // Loop through all modules in the new image applying reverse fixups
    //

    for(i = 0; i < pimgNew->GetModuleCount(); i++) {
        ASSERT(pNewMod == NULL);
        ASSERT(pOldMod == NULL);

        if(pNewMod = pimgNew->GetModuleData(i)) {
            LPCSTR szNewFilename = pNewMod->GetFilename();
            if(szNewFilename) {
                iModOld = pimgOld->FindModuleByName(szNewFilename);
                if(iModOld != -1) {
                    if(pOldMod = pimgOld->GetModuleData(iModOld)) {
                        e32_rom *pOldE32 = pOldMod->GetE32();
                        e32_rom *pNewE32 = pNewMod->GetE32();

                        // Only do relocation if it's not an exe and some of the offsets have changed
                        if (DoRelocation(pOldMod, pNewMod, szNewFilename))
                        {
                            RelocateImageFile(pOldMod, pNewMod, szNewFilename);
                        }

                        delete pOldMod;
                        pOldMod = NULL;
                    }
                } else {
                    RETAILMSG(ZONE_VERBOSE, (TEXT("Added module: '%s'  (No reverse fixups needed)\n"), szNewFilename));
                }
            } else {
                RETAILMSG(ZONE_VERBOSE, (TEXT("Error : Unable to get filename for new module index %d\n"), i));
                CHR(E_FAIL);
            }

            delete pNewMod;
            pNewMod = NULL;
        }
    }

    //
    // Loop through the module in the old image to detect deleted modules
    //
    for(i = 0; i < pimgOld->GetModuleCount(); i++) {
        ASSERT(pNewMod == NULL);
        ASSERT(pOldMod == NULL);

        if(pOldMod = pimgOld->GetModuleData(i)) {
            LPCSTR szOldFilename = pOldMod->GetFilename();
            if(szOldFilename) {
                if(pimgNew->FindModuleByName(szOldFilename) == -1) {
                    RETAILMSG(ZONE_VERBOSE, (TEXT("Deleted module: '%s'  (No reverse fixups needed)\n"), szOldFilename));
                }
            } else {
                RETAILMSG(ZONE_VERBOSE, (TEXT("Error : Unable to get filename for old module index %d\n"), i));
                CHR(E_FAIL);
            }

            delete pOldMod;
            pOldMod = NULL;
        }
    }

Error:
    delete pNewMod;
    delete pOldMod;

    return hr;

} /* ApplyReverseFixups()
   */
