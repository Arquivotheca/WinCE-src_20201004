//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//  
//------------------------------------------------------------------------------
#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#include <pehdr.h>
#include <romldr.h>

#define BUFFER_SIZE  64*1024
BYTE pBuffer[BUFFER_SIZE];
BYTE pBufferSym[BUFFER_SIZE];
DWORD dwFreq;
DWORD dwHighTime = 0;

LPTSTR g_szFilename;
BOOL   g_fPrintData;
BOOL   g_fPrintRecords;
BOOL   g_fGenerateSRE;
BOOL   g_fGenerateROM;
BOOL   g_fPrintTOC;
BOOL   g_fPrintSYM;
BOOL   g_fPrintOBJ;
BOOL   g_fCheckFiles;
HANDLE hFile;
HANDLE hSRE;
HANDLE hRaw[8];  // should support a rom width of 8 in a 64 bit environment
DWORD  g_dwImageStart;
DWORD  g_dwImageLength;
DWORD  g_dwNumRecords;
DWORD  g_pTOC = 0;
DWORD  g_dwROMOffset;
DWORD  g_dwNumFiles;
DWORD  g_dwNumModules;
DWORD  g_dwNumCopySects;
DWORD  g_dwProfileOffset;
DWORD  g_dwProfileAdjust;
DWORD  g_dwCopyOffset;
DWORD  g_dwStartAddr;

DWORD  g_iRomStartAddr;
DWORD  g_iRomLength;
DWORD  g_iRomWidth;

typedef struct _RECORD_DATA {
    DWORD dwStartAddress;
    DWORD dwLength;
    DWORD dwChecksum;
    DWORD dwFilePointer;
} RECORD_DATA, *PRECORD_DATA;

#define MAX_RECORDS 2048
RECORD_DATA g_Records[MAX_RECORDS];


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
printHelp(void)
{

  /* 
    This #define is put here to make the programs cvrtbin and view bin look like two 
    different programs to the user to avoid any onfusion of syntax and meaning implied 
    in the names.  Otherwise this could/should be one app due to the amount of code 
    reuse.
  */
#ifdef CVRTBIN    
    printf("Usage: cvrtbin [ options ] <filename>\r\n");
    printf("Options:\r\n");
    printf("  -s[re]      Generates SRE file from BIN\r\n");
    printf("  -r[om]      Generates ROM file from BIN *\r\n\r\n");
    printf("    * ROM conversion requires the following options to be set also...\r\n");
    printf("    -a[ddress]  Rom starting address\r\n");
    printf("    -w[idth]    Rom width 8, 16, or 32 bits\r\n");
    printf("    -l[ength]   Rom length as a hex value\r\n");
#else
    printf("Usage: viewbin [ options ] <filename>\r\n");
    printf("Options:\r\n");
    printf("  -d[ata]     Prints all data bytes (potentially huge output!)\r\n");
    printf("  -t[oc]      Prints Table of Contents\r\n");
    printf("  -o[bj]      Prints Table of Contents and Objects Information\r\n");
    printf("  -r[ec]      Prints Record Information\r\n");
    printf("  -sym        Prints Profiling Symbol Information\r\n");
#endif
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
TCHAR* 
getNextWord(
    TCHAR **ppCmdLine, 
    TCHAR *pStorage
    )
{
    TCHAR *pWord;

    // First restore the saved char.
    **ppCmdLine = *pStorage;

    // Move over initial white space, and save the start of the word. 
    while (**ppCmdLine && (**ppCmdLine == TEXT(' ') || **ppCmdLine == TEXT('\t'))) 
        (*ppCmdLine)++;
    pWord = *ppCmdLine;

    // Move over the word, store the char right after the word, and zero-terminate the word.
    while (**ppCmdLine && **ppCmdLine != TEXT(' ') && **ppCmdLine != TEXT('\t')) 
        (*ppCmdLine)++;
    *pStorage = **ppCmdLine;
    **ppCmdLine = 0;

    // Return start of word, or zero (in case the word is empty).
    if (*pWord) 
        return pWord;
    else
        return 0;
}   



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
parseCmdLine(
    TCHAR *pCmdLine
    )
{
    TCHAR storage = *pCmdLine;  // getNextWord needs to store a char
    TCHAR *word;

    //
    // Eat the EXE name first.
    //
    word = getNextWord(&pCmdLine, &storage);

    // Keep getting words from the command line.
    while (word = getNextWord(&pCmdLine, &storage)) {

#ifdef CVRTBIN
        if (!_tcscmp(TEXT("-sre"), word) || !_tcscmp(TEXT("-s"), word)) {
            g_fGenerateSRE = TRUE;
        }
        else if (!_tcscmp(TEXT("-rom"), word) || !_tcscmp(TEXT("-r"), word)) {
            g_fGenerateROM = TRUE;
        }
        else if (!_tcscmp(TEXT("-address"), word) || !_tcscmp(TEXT("-a"), word)) {
            g_iRomStartAddr = strtoul(getNextWord(&pCmdLine, &storage), NULL, 16);
        }
        else if (!_tcscmp(TEXT("-length"), word) || !_tcscmp(TEXT("-l"), word)) {
            g_iRomLength = strtoul(getNextWord(&pCmdLine, &storage), NULL, 16);
        }
        else if (!_tcscmp(TEXT("-width"), word) || !_tcscmp(TEXT("-w"), word)) {
            g_iRomWidth = strtoul(getNextWord(&pCmdLine, &storage), NULL, 10);
        }
#else        
        if (!_tcscmp(TEXT("-data"), word) || !_tcscmp(TEXT("-d"), word)) {
            g_fPrintData = TRUE;
        }
        else if (!_tcscmp(TEXT("-toc"), word) || !_tcscmp(TEXT("-t"), word)) {
            g_fPrintTOC = TRUE;
        }
        else if (!_tcscmp(TEXT("-obj"), word) || !_tcscmp(TEXT("-o"), word)) {
            g_fPrintOBJ = TRUE;
        }
        else if (!_tcscmp(TEXT("-sym"), word)) {
            g_fPrintSYM = TRUE;
        }
        else if (!_tcscmp(TEXT("-rec"), word) || !_tcscmp(TEXT("-r"), word)) {
            g_fPrintRecords = TRUE;
        }
        else if (!_tcscmp(TEXT("-checkfiles"), word) || !_tcscmp(TEXT("-c"), word)) {
            g_fCheckFiles = TRUE;
        }
//        else if (!_tcscmp(TEXT("-n"), word)) {
//            if (word = getNextWord(&pCmdLine, &storage)) {
//                g_nIterations = (int) _ttol(word);
//                if (g_nIterations > MAX_ITERATIONS) {
//                    g_nIterations = MAX_ITERATIONS;
//                }
//
//            } else {
//                RETAILMSG(1, (TEXT("Error -n (%s)\r\n"), word));
//                return FALSE;
//            }
//        } 
#endif
        else if (word[0] == '-') {
            printf("Unknown option... %s\n", word);
            goto EXIT;
        }
        else {
            g_szFilename = word;
        }
    }

    if (g_szFilename == NULL) {
        printf("Filename required\n");
        goto EXIT;
    }

    if(g_fGenerateROM){
        if(!g_iRomLength){
            printf("ROM conversion specified but missing length option\n");
            goto EXIT;
        }
        if(!g_iRomStartAddr){
            printf("ROM conversion specified but missing address option\n");
            goto EXIT;
        }
        if(!g_iRomWidth){
            printf("ROM conversion specified but missing width option\n");
            goto EXIT;
        }
    }

    return TRUE;

EXIT:
    printHelp();
    return FALSE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
CheckBinFile(
    DWORD dwAddress,
    DWORD dwLength
    )
{
    DWORD i;

    //
    // Adjust if needed
    //
    dwAddress += g_dwROMOffset;

    for (i = 0; i < g_dwNumRecords; i++) {
        if (g_Records[i].dwStartAddress <= dwAddress &&
            g_Records[i].dwStartAddress + g_Records[i].dwLength >= (dwAddress + dwLength)) {
            return TRUE;
        }
    }
    return FALSE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
ReadBinFile(
    PBYTE pDestBuffer,
    DWORD dwAddress,
    DWORD dwLength,
    DWORD dwROMOffsetRead
    )
{
    DWORD dwRead;
    BOOL  fRet;
    DWORD i;
    DWORD dwDiff;

    if (dwLength > BUFFER_SIZE) {
        printf ("Trying to read %d bytes (too big)\n", dwLength);
        return FALSE;
    }

    //
    // Adjust if needed
    //
    dwAddress += dwROMOffsetRead;

    for (i = 0; i < g_dwNumRecords; i++) {
        if (g_Records[i].dwStartAddress <= dwAddress &&
            g_Records[i].dwStartAddress + g_Records[i].dwLength >= (dwAddress + dwLength)) {
            
            //
            // Offset into the record.
            //
            dwDiff = dwAddress - g_Records[i].dwStartAddress;

            SetFilePointer(hFile, g_Records[i].dwFilePointer + dwDiff, NULL, FILE_BEGIN);

            if (dwLength) {
                fRet = ReadFile(hFile, pDestBuffer, dwLength, &dwRead, NULL);
            
                if (!fRet || dwRead != dwLength) {
                    return FALSE;
                }
            } else {
                int count = 0;
                do {
                    fRet = ReadFile(hFile, pDestBuffer, 1, &dwRead, NULL);
                    count++;
                } while (*pDestBuffer++);

                // no string here to read or string left record, then must have been the wrong record
                if(!count || g_Records[i].dwStartAddress + g_Records[i].dwLength < (dwAddress + count)){
                  pDestBuffer -= count;
                  continue;
                }
            }
            return TRUE;
        }
    }
    return FALSE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
ComputeRomOffset()
{
    DWORD i;
    BOOL fFoundIt = FALSE;
    BOOL fRet;
    DWORD dwROMOffsetRead;
    DWORD header[2] = {0};
    ROMHDR *dwpTOC = 0;
    static DWORD last_signature = 0;

    for (i = last_signature; i < g_dwNumRecords; i++) {
        //
        // no pTOC and not an 8 byte record... skip
        //
        if(!dwpTOC){
            if(g_Records[i].dwLength != 8)
              continue;

            // 
            // check for signature and pTOC
            // 
            fRet = ReadBinFile((BYTE*)header, g_Records[i].dwStartAddress, 8, 0);
    
            if(!fRet || header[0] != ROM_SIGNATURE)
                continue;

            //
            // set pTOC and save current search and start record search over
            //
            dwpTOC = (ROMHDR*)header[1];
            last_signature = i + 1;
            i = 0;
        }

        //
        // if it's not a TOC... skip
        //
        if(g_Records[i].dwLength == sizeof(ROMHDR)){
                
            //
            // If this _IS_ the TOC record, compute the ROM Offset.
            //
            dwROMOffsetRead = (DWORD) g_Records[i].dwStartAddress - (DWORD) dwpTOC;

            printf("Checking record #%d for potential TOC (ROMOFFSET = 0x%08X)\n", i, dwROMOffsetRead);
            //
            // Read out the record to verify. (unadjusted)
            //
            fRet = ReadBinFile(pBuffer, g_Records[i].dwStartAddress, sizeof(ROMHDR), 0);

            if (fRet) {
                ROMHDR *pTOC = (ROMHDR*) pBuffer;

                if(pTOC->physfirst > (DWORD)dwpTOC || pTOC->physlast < (DWORD)dwpTOC){
//                    printf ("NOTICE! Record %d looked like a TOC at 0x%08x except Phys first = 0x%08X, and Phys last = 0x%08X\r\n", i, dwpTOC, pTOC->physfirst, pTOC->physlast);
                    continue;
                }
            
                if ((pTOC->physfirst >= (g_dwImageStart - dwROMOffsetRead)) &&
                    (pTOC->physlast  <= (g_dwImageStart - dwROMOffsetRead) + g_dwImageLength)) {

                    //
                    // Extra sanity check...
                    //
                    if ((DWORD)(HIWORD(pTOC->dllfirst) << 16) <= pTOC->dlllast && 
                        (DWORD)(LOWORD(pTOC->dllfirst) << 16) <= pTOC->dlllast) { 

						printf("Found pTOC  = 0x%08x\n", dwpTOC);
                        fFoundIt = TRUE;
                        break;
                    } else {
                        printf ("NOTICE! Record %d looked like a TOC except DLL first = 0x%08X, and DLL last = 0x%08X\r\n", i, pTOC->dllfirst, pTOC->dlllast);
                    }
                } else {
                    printf ("NOTICE! Record %d looked like a TOC except Phys first = 0x%08X, and Phys last = 0x%08X\r\n", i, pTOC->physfirst, pTOC->physlast);
                }
            }
        }

        //
        // didn't find it so reset signature search
        // 
        if(i == g_dwNumRecords - 1){
            i = last_signature - 1;
            dwpTOC = 0;
        }

    }

    if (fFoundIt) {
        g_dwROMOffset = dwROMOffsetRead;
        g_pTOC = (DWORD)dwpTOC;

        printf("ROMOFFSET = 0x%08X\n", g_dwROMOffset);

        return TRUE;
    }

    g_dwROMOffset = 0;
    g_pTOC = 0;

    return FALSE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
PrintTOC(){
  BOOL  fRet;
  SYSTEMTIME st;

  if (!g_pTOC)
    return;
    
  //-------------------------------------------
  // Print header
  //
  fRet = ReadBinFile(pBuffer, g_pTOC, sizeof(ROMHDR), g_dwROMOffset);

  if (fRet) {
    ROMHDR* pTOC = (ROMHDR*) pBuffer;
    ROMPID Pid;
    int i;
      
    g_dwNumModules = pTOC->nummods;
    g_dwNumFiles = pTOC->numfiles;
    g_dwNumCopySects = pTOC->ulCopyEntries;
    g_dwCopyOffset = pTOC->ulCopyOffset;
    g_dwProfileOffset = pTOC->ulProfileOffset;

    printf("\n");
    printf("ROMHDR ----------------------------------------\n");
    printf("    DLL First           : 0x%08X  \n", pTOC->dllfirst);
    printf("    DLL Last            : 0x%08X  \n", pTOC->dlllast);
    printf("    Physical First      : 0x%08X  \n", pTOC->physfirst);
    printf("    Physical Last       : 0x%08X  \n", pTOC->physlast);
    printf("    RAM Start           : 0x%08X  \n", pTOC->ulRAMStart);
    printf("    RAM Free            : 0x%08X  \n", pTOC->ulRAMFree);
    printf("    RAM End             : 0x%08X  \n", pTOC->ulRAMEnd);
    printf("    Kernel flags        : 0x%08X  \n", pTOC->ulKernelFlags);
    printf("    Prof Symbol Offset  : 0x%08X  \n", pTOC->ulProfileOffset);
    printf("    Num Copy Entries    : %10d    \n", pTOC->ulCopyEntries);
    printf("    Copy Entries Offset : 0x%08X  \n", pTOC->ulCopyOffset);
    printf("    Num Modules         : %10d    \n", pTOC->nummods);
    printf("    Num Files           : %10d    \n", pTOC->numfiles);
    printf("    Kernel Debugger     : %10s\n", pTOC->usMiscFlags & 0x1 ? "Yes" : "No");
    printf("    CPU                 :     0x%04x ", pTOC->usCPUType); 

    switch(pTOC->usCPUType) {
      case IMAGE_FILE_MACHINE_SH3:
        printf("(SH3)\n");
        break;
      case IMAGE_FILE_MACHINE_SH3E:
        printf("(SH3e)\n");
        break;
      case IMAGE_FILE_MACHINE_SH3DSP:
        printf("(SH3-DSP)\n");
        break;
      case IMAGE_FILE_MACHINE_SH4:
        printf("(SH4)\n");
        break;
      case IMAGE_FILE_MACHINE_I386:
        printf("(x86)\n");
        break;
      case IMAGE_FILE_MACHINE_THUMB:
        printf("(Thumb)\n");
        break;
      case IMAGE_FILE_MACHINE_ARM:
        printf("(ARM)\n");
        break;
      case IMAGE_FILE_MACHINE_POWERPC:
        printf("(PPC)\n");
        break;
      case IMAGE_FILE_MACHINE_R4000:
        printf("(R4000)\n");
        break;
      case IMAGE_FILE_MACHINE_MIPS16:
        printf("(MIPS16)\n");
        break;
      case IMAGE_FILE_MACHINE_MIPSFPU:
        printf("(MIPSFPU)\n");
        break;
      case IMAGE_FILE_MACHINE_MIPSFPU16:
        printf("(MIPSFPU16)\n");
        break; 
      default:
        printf("(Unknown)");
    }

    printf("    Extensions          : 0x%08X\n", pTOC->pExtensions);

    if (pTOC->pExtensions) {
      if (ReadBinFile((PBYTE)(&Pid), (DWORD)pTOC->pExtensions, sizeof(ROMPID), g_dwROMOffset)) {
        printf("\n");
        printf("ROMHDR Extensions -----------------------------\n");
  
        for (i = 0; i < PID_LENGTH; i++)
          printf("    PID[%d] = 0x%08X\n", i, Pid.dwPID[i]);

        printf("    Next: %08x\n", Pid.pNextExt);

        while(Pid.pNextExt){
          printf("\n    -- Location: %08x\n", Pid.pNextExt);
          
          if(ReadBinFile((PBYTE)(&Pid), (DWORD)Pid.pNextExt, sizeof(ROMPID), g_dwROMOffset)){
            printf("    Name: %-24s\n"
                   "    Type:     %08x\n"
                   "    pData:    %08x\n"
                   "    Length:   %08x\n"
                   "    Reserved: %08x\n"
                   "    Next:     %08x\n",
                   Pid.name,
                   Pid.type,
                   Pid.pdata,
                   Pid.length,
                   Pid.reserved,
                   Pid.pNextExt); 
            
            if(strcmp(Pid.name, "chain information") == 0){
              char *buffer = malloc(Pid.length);

              if(!buffer){
                printf("Error: failed allocating extention buffer\n");
                return;
              }
              
              if(ReadBinFile(buffer, (DWORD)Pid.pdata, Pid.length, g_dwROMOffset)){
                DWORD i;
                for(i = 0; i < Pid.length/sizeof(XIPCHAIN_SUMMARY); i++){
                  printf("      Addr:     %08x\n"
                         "      MaxLenth: %08x\n"
                         "      Order:    %04x\n"
                         "      Flags:    %04x\n"
                         "      reserved: %08x\n",
                         ((XIPCHAIN_SUMMARY*)buffer)[i].pvAddr,
                         ((XIPCHAIN_SUMMARY*)buffer)[i].dwMaxLength,
                         ((XIPCHAIN_SUMMARY*)buffer)[i].usOrder,
                         ((XIPCHAIN_SUMMARY*)buffer)[i].usFlags,
                         ((XIPCHAIN_SUMMARY*)buffer)[i].reserved);
                }
              }
            }
          } else {
            printf("Couldn't locate TOC Extensions\n");
          }
        }
      } else {
        printf("Couldn't locate TOC Extensions\n");
      }
    }
  } else {
    printf("Couldn't locate TOC data\n");
  }
  
  
  //-------------------------------------------
  // Print Copy Sections
  //
  if (g_dwNumCopySects){
    fRet = ReadBinFile(pBuffer, (DWORD) g_dwCopyOffset, sizeof(COPYentry) * g_dwNumCopySects, g_dwROMOffset);

    if (fRet) {
      COPYentry* pce = NULL;
      DWORD i = 0;
  
      printf("\n");
      printf("COPY Sections ---------------------------------\n");
     
      for (i=0 ; i < g_dwNumCopySects ; i++) {
          pce = (((COPYentry*)pBuffer) + i);
          printf("    Src: 0x%08X   Dest: 0x%08X   CLen: 0x%-6X   DLen: 0x%-6X\n", pce->ulSource, pce->ulDest, pce->ulCopyLen, pce->ulDestLen);
          if (pce->ulDest == g_dwProfileOffset) {
              g_dwProfileAdjust = pce->ulSource - pce->ulDest;
              g_dwProfileOffset += g_dwProfileAdjust;
          }
      }
    } else {
      printf("Couldn't locate copy secitons\n");
    }
  }


  //-------------------------------------------
  // Print Modules
  //
  fRet = ReadBinFile(pBuffer, (DWORD) g_pTOC + sizeof(ROMHDR), sizeof(TOCentry) * g_dwNumModules, g_dwROMOffset);
  
  if (fRet) {
    TOCentry* pte = (TOCentry*) pBuffer;
    e32_rom e32, *pe32 = &e32;
    o32_rom o32, *po32 = &o32;
    DWORD i, j;
    char szFilename[MAX_PATH];

    printf("\n");
    printf("MODULES ---------------------------------------\n");

    for (i=0;i < g_dwNumModules; i++) {
      fRet = ReadBinFile((PBYTE) szFilename, (DWORD) pte[i].lpszFileName, 0, g_dwROMOffset);
      FileTimeToSystemTime(&(pte[i].ftTime), &st);

      if (!g_fPrintOBJ) {
        printf("    %2d/%02d/%04d  %02d:%02d:%02d  %10d  %s \n", 
               st.wMonth, st.wDay, st.wYear,
               st.wHour, st.wMinute, st.wSecond,
               pte[i].nFileSize, szFilename);
      } else {
        printf("    ==== %s ===============================\n", szFilename);

        printf("    TOCentry (%s) -------------------------\n", szFilename);
        printf("        dwFileAttributes    : 0x%X\n", pte[i].dwFileAttributes);
        printf("        ftTime              : %2d/%02d/%04d  %02d:%02d:%02d\n",
          st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond);
        printf("        nFileSize           : 0x%X (%d)\n", pte[i].nFileSize, pte[i].nFileSize);
        printf("        ulE32Offset         : 0x%X\n", pte[i].ulE32Offset);
        printf("        ulO32Offset         : 0x%X\n", pte[i].ulO32Offset);
        printf("        ulLoadOffset        : 0x%X\n", pte[i].ulLoadOffset);
    
        if (ReadBinFile((PBYTE)pe32, pte[i].ulE32Offset, sizeof(e32_rom), g_dwROMOffset)) {
          printf("    e32_rom (%s) --------------------------\n", szFilename);
          printf("        e32_objcnt          : %d\n", pe32->e32_objcnt);
          printf("        e32_imageflags      : 0x%X\n", pe32->e32_imageflags);
          printf("        e32_entryrva        : 0x%X\n", pe32->e32_entryrva);
          printf("        e32_vbase           : 0x%X\n", pe32->e32_vbase);
          printf("        e32_subsysmajor     : 0x%X\n", pe32->e32_subsysmajor);
          printf("        e32_subsysminor     : 0x%X\n", pe32->e32_subsysminor);
          printf("        e32_stackmax        : 0x%X\n", pe32->e32_stackmax);
          printf("        e32_vsize           : 0x%X\n", pe32->e32_vsize);
          
          for (j = 0; j < pe32->e32_objcnt; j++) {
            printf("    o32_rom[%d] (%s) ------------------------\n", j, szFilename);
            
            if (ReadBinFile((PBYTE)po32, pte[i].ulO32Offset + j * sizeof(o32_rom), sizeof(o32_rom), g_dwROMOffset)) {
              printf("        o32_vsize           : 0x%X\n", po32->o32_vsize);
              printf("        o32_rva             : 0x%X\n", po32->o32_rva);
              printf("        o32_psize           : 0x%X\n", po32->o32_psize);
              printf("        o32_dataptr         : 0x%X\n", po32->o32_dataptr);
              printf("        o32_realaddr        : 0x%X\n", po32->o32_realaddr);
              printf("        o32_flags           : 0x%X\n", po32->o32_flags);
            } else {
              printf("ERROR: Couldn't locate o32_rom data\n");
            }     
          }
          
          printf("\n");

        } else {
          printf("ERROR: Couldn't locate e32_rom data\n");
        }
      }
    }
  } else {
    printf("ERROR: Couldn't locate Modules data\n");
  }
  
  //-------------------------------------------
  // Print Files
  //
  fRet = ReadBinFile(pBuffer, (DWORD) g_pTOC + sizeof(ROMHDR) + (sizeof(TOCentry) * g_dwNumModules), sizeof(FILESentry) * g_dwNumFiles, g_dwROMOffset);

  if (fRet) {
    FILESentry* pfe = (FILESentry*) pBuffer;
    DWORD i;
    char szFilename[MAX_PATH];

    printf("\n");
    printf("FILES ----------------------------------------\n");

    for (i=0;i < g_dwNumFiles; i++) {
      fRet = ReadBinFile((PBYTE) szFilename, (DWORD) pfe[i].lpszFileName, 0, g_dwROMOffset);
      FileTimeToSystemTime(&(pfe[i].ftTime), &st);
      printf("     %2d/%02d/%04d  %02d:%02d:%02d  %c%c%c%c %10d %10d  %24s (ROM 0x%08X)\n", 
             st.wMonth, st.wDay, st.wYear,
             st.wHour, st.wMinute, st.wSecond,
             (pfe[i].dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED ? 'C' : '_'),
             (pfe[i].dwFileAttributes & FILE_ATTRIBUTE_HIDDEN     ? 'H' : '_'),
             (pfe[i].dwFileAttributes & FILE_ATTRIBUTE_READONLY   ? 'R' : '_'),
             (pfe[i].dwFileAttributes & FILE_ATTRIBUTE_SYSTEM     ? 'S' : '_'),
             (pfe[i].dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED ? pfe[i].nCompFileSize : 0),
             pfe[i].nRealFileSize, szFilename, pfe[i].ulLoadOffset);
          
      if (g_fCheckFiles) {
        DWORD dwSize;

        if (pfe[i].dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED)
          dwSize = pfe[i].nCompFileSize;
        else 
          dwSize = pfe[i].nRealFileSize;
                                      
        fRet = CheckBinFile(pfe[i].ulLoadOffset, dwSize);
        if (!fRet)
          printf("File not in record space. Potentially truncated file!\n");
      }
    }
  } else {
    printf("Couldn't locate Files data\n");
  }

    //-------------------------------------------
    // Print Symbols
    //
    if (g_fPrintSYM) {
        fRet = ReadBinFile(pBuffer, (DWORD) g_dwProfileOffset, sizeof(PROFentry) * g_dwNumModules, g_dwROMOffset);

        if (fRet) {
            DWORD i;
            PROFentry* pe = (PROFentry*) pBuffer;

            printf("\n");
            printf("SYMBOLS ---------------------------------------\n");

            for (i=0;i < g_dwNumModules; i++) {

                printf(" MOD (%2d, %2d), 0x%08X - 0x%08X, numsyms  = %4d, HitAddr = 0x%08X, SymAddr = 0x%08X\n", pe[i].ulModNum, pe[i].ulSectNum, pe[i].ulStartAddr, pe[i].ulEndAddr, pe[i].ulNumSym, pe[i].ulHitAddress, pe[i].ulSymAddress);
                fRet = ReadBinFile(pBufferSym, (DWORD) pe[i].ulHitAddress + g_dwProfileAdjust, sizeof(SYMentry) * pe[i].ulNumSym, g_dwROMOffset);

                if (fRet) {
                    DWORD j;
                    SYMentry* pse = (SYMentry*) pBufferSym;
                    char szFunction[128];
                    PBYTE pTraverse = (PBYTE)pe[i].ulSymAddress;

                    for (j=0; j < pe[i].ulNumSym; j++) {
                        
                        char thischar;
                        int charindex = 0;

                        do {
                            
                            fRet = ReadBinFile(&thischar, (DWORD) pTraverse++, sizeof(char), g_dwROMOffset);
                            szFunction[charindex++] = thischar;

                        } while (fRet && thischar != '\0');

                        printf("       0x%08X : %s\n", pse[j].ulFuncAddress, szFunction);


                    }

                }
            }
        } else {
          printf("ERROR: Couldn't locate Modules data\n");
        }


    }

}



#define ADDR_SUM(x) (( ((x) & 0xff000000) >> 24) + \
                     ( ((x) & 0x00ff0000) >> 16) + \
                     ( ((x) & 0x0000ff00) >> 8)  + \
                     ( ((x) & 0x000000ff) >> 0)    \
                     )

#define CHKSUM(x) ((unsigned char) (~(x)  & 0x000000ff))

#define SRE_DATA_SIZE 0x28

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPCTSTR GetLastErrorString(void){
  static LPTSTR  errorMessage  = NULL;
  ULONG  errorMsgSize  = 0;

  // free the last message so that we don't have more than one 
  // out at a time and limit what would otherwise be a leak.
  if(errorMessage)
    LocalFree(errorMessage);
      
  if(!(errorMsgSize = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                    FORMAT_MESSAGE_FROM_SYSTEM     |
                                    FORMAT_MESSAGE_IGNORE_INSERTS,
                                    NULL,
                                    GetLastError(),
                                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                    (LPTSTR) &errorMessage,
                                    0,
                                    NULL))){
    fprintf(stderr, TEXT("GetLastErrorString failed [%x]"), GetLastError());
    errorMessage = NULL;
  }
                   
  return errorMessage;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PTCHAR BaseName(LPTSTR filename){
  static TCHAR base[MAX_PATH];
  PTCHAR ptr;

  strcpy(base, filename);

  ptr = strchr(base, '.');
  if(ptr) 
      *ptr = '\0';

  return base;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL WriteSREHeader(){
    BOOL fRet;
    DWORD dwWrite;
    TCHAR temp[MAX_PATH];

    sprintf(temp, "%s.sre", BaseName(g_szFilename));
    hSRE = CreateFile (temp, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 
                       NULL, OPEN_ALWAYS, 0, 0);

    if (hSRE == INVALID_HANDLE_VALUE) {
        printf ("Error opening '%s'\n", temp);
        return FALSE;
    }

    fRet = WriteFile (hSRE, "S0030000FC\n", 11, &dwWrite, NULL);
    if (!fRet || dwWrite != 11) {
        printf("Error writing SRE file '%s' (%d)\n", temp, __LINE__);
        CloseHandle (hSRE);
        hSRE = INVALID_HANDLE_VALUE;
        return FALSE;
    }

    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL WriteSRERecord (const RECORD_DATA *rec){
    DWORD dwRecAddr, dwRecLen;
    DWORD dwRead, dwReadReq;
    DWORD dwWrite;
    DWORD chksum = 0;
    BOOL  fRet;
    char szAscii[256] = {0};
    DWORD i, j;

    if (hSRE == INVALID_HANDLE_VALUE)
        return FALSE;

    dwRecLen = rec->dwLength;
    dwRecAddr = rec->dwStartAddress;

    // set file pointer to record location
    SetFilePointer (hFile, rec->dwFilePointer, NULL, FILE_BEGIN);
        
    for(i = dwRecLen; i > 0; ){
        dwReadReq = (i > SRE_DATA_SIZE ? SRE_DATA_SIZE : i);
        i -= dwReadReq;

        chksum = dwReadReq + 5 + ADDR_SUM (dwRecAddr); // +5 for address and checksum
        
        sprintf (szAscii, "S3%02X%08X", (unsigned char)(dwReadReq + 5), dwRecAddr); // +5 for address and checksum
        dwRecAddr += dwReadReq;

        fRet = ReadFile (hFile, pBuffer, dwReadReq, &dwRead, NULL);
        if (!fRet || dwRead != dwReadReq)
            break;

        for (j = 0; j < dwReadReq; j++){
          chksum += pBuffer[j];
          sprintf (szAscii + strlen(szAscii), "%02X", pBuffer[j]);
        }

        sprintf (szAscii + strlen(szAscii), "%02X\n", CHKSUM (chksum));

        fRet = WriteFile(hSRE, szAscii, strlen (szAscii), &dwWrite, NULL);
        if (!fRet || dwWrite != strlen (szAscii)) {
            printf ("Error writing SRE file (%d)\n", __LINE__);
            CloseHandle (hSRE);
            hSRE = INVALID_HANDLE_VALUE;
            return FALSE;
        }
    }

    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL WriteSREFooter (){
    BOOL fRet;
    DWORD dwWrite;
    char szAscii[256] = {0};
    DWORD dwStartOfExecution = g_dwROMOffset + g_dwStartAddr;
    
    if (hSRE == INVALID_HANDLE_VALUE)
        return FALSE;

    sprintf (szAscii, "S705%08X%02X\n", dwStartOfExecution, CHKSUM (ADDR_SUM (dwStartOfExecution) + 5));
    fRet = WriteFile (hSRE, szAscii, strlen(szAscii), &dwWrite, NULL);
    if(!fRet || dwWrite != 15){
        printf ("Error writing SRE file (%d)\n", __LINE__);
    }

    printf("SRE file completed!\n");

    CloseHandle (hSRE); 
    hSRE = INVALID_HANDLE_VALUE;
    return TRUE;
}


#define NATIVE_SIZE 32 // bit'ness of platform
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL CloseRawFiles(){
    DWORD i;
    
    // close all handles
    for(i = 0; i < sizeof(hRaw) / sizeof(HANDLE); i++){
        CloseHandle(hRaw[i]);
        hRaw[i] = INVALID_HANDLE_VALUE;
    }
    
    return TRUE;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL OpenRawFiles(DWORD parts){
    static int suffix = 0;
    DWORD i;
  
    CloseRawFiles();
    
    // open new handles
    for(i = 0; i < parts; i++){
        char temp[MAX_PATH];
        sprintf(temp, "%s.nb%d", BaseName(g_szFilename), suffix++);

        DeleteFile(temp);
    
        hRaw[i] = CreateFile (temp, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 
                              NULL, OPEN_ALWAYS, 0, 0);
    
        if (hRaw[i] == INVALID_HANDLE_VALUE) {
            printf ("Error opening %s\n", temp);
            CloseRawFiles();
            return FALSE;
        }
    }
    
    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL WriteROMs(){
    DWORD dwRead;
    DWORD dwWrite;
    BOOL  fRet;
    DWORD i, j, k;
    BYTE *pData, *ptr;
    DWORD dwRomCount;
    DWORD dwRomParts;
    DWORD dwRomSize;
    DWORD dwWriteSize;

    // compute roms needed
    dwRomCount = g_dwImageLength / g_iRomLength;
    if(g_dwImageLength % g_iRomLength)
        dwRomCount++;

    // compute how many files to span a native machine size value ie. 16 bit rom and 32 bit platform == 2 roms wide
    dwRomParts = NATIVE_SIZE / g_iRomWidth;
    dwWriteSize = NATIVE_SIZE / dwRomParts / 8; // bytes to write at one time
    dwRomSize = dwRomCount * dwRomParts * g_iRomLength;

    pData = (BYTE *) malloc(dwRomSize);
    if(!pData){
        printf("Error: Could not allocate memory for raw data\n");
        return FALSE;
    }

    memset(pData, 0, dwRomSize);
    
    for(i = 0; i < g_dwNumRecords; i++){
        if(!g_Records[i].dwStartAddress && !g_Records[i].dwChecksum) // ignore the jump address
            continue;
        
        printf("start %08x length %08x\n", g_Records[i].dwStartAddress, g_Records[i].dwLength);

        if(g_Records[i].dwStartAddress - g_iRomStartAddr > dwRomSize){
            printf("Warning: Record outside of ROM range, record skipped\n");
            continue;
        }

        SetFilePointer (hFile, g_Records[i].dwFilePointer, NULL, FILE_BEGIN);

        fRet = ReadFile (hFile, 
                         pData + g_Records[i].dwStartAddress - g_iRomStartAddr, // g_dwImageStart, 
                         g_Records[i].dwLength, 
                         &dwRead, 
                         NULL);

        if (!fRet || dwRead != g_Records[i].dwLength){
            printf("Error: Could not read record %d\n", i);
        }
    }

    ptr = pData;
    printf("Progress...\n");
    for(i = 0; i < dwRomCount; i++){
        OpenRawFiles(dwRomParts);

        // opitmization for native machine sized roms
        if(dwRomParts == 1)
            dwWriteSize = dwRomSize;
        
        for(j = 0; j < g_iRomLength / dwWriteSize; j++){
            static DWORD percent = -1;

            if(percent != (ptr - pData) * 100 / dwRomSize){ 
                percent = (ptr - pData) * 100 / dwRomSize;
                printf("\r%d%%", percent);
                fflush(stdout);
            }
                
            for(k = 0; k < dwRomParts; k++, ptr += dwWriteSize){
                WriteFile(hRaw[k], ptr, dwWriteSize, &dwWrite, NULL);
                if (!fRet || dwWrite != dwWriteSize) {
                    printf ("Error writing Raw file (%s)\n", GetLastErrorString());
                    break;
                }
            }
        }
            
        CloseRawFiles();
    }

    free(pData);

    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    DWORD i, j, t, dwGap;
    DWORD dwRecAddr, dwRecLen, dwRecChk;
    DWORD dwRead, dwReadReq;
    BOOL  fRet;
    LPTSTR pszCmdLine;
    char szAscii[17];

    pszCmdLine = GetCommandLine();

    if(getenv("d_break"))
      DebugBreak();

    // Parse command line parameters (updates the ui variables).
    if (!parseCmdLine(pszCmdLine))  
       return 0;
   
    
    printf("ViewBin... %s\n", g_szFilename);
    
    
    hFile = CreateFile(g_szFilename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 
                       NULL, OPEN_EXISTING, 0, 0);

    if (hFile == INVALID_HANDLE_VALUE) {
        printf ("Error opening %s\n", g_szFilename);
        return 0;
    }
    

    fRet = ReadFile(hFile, pBuffer, 7, &dwRead, NULL);
    
    if (!fRet || dwRead != 7) {
        printf("Error reading %s (%d)\n", g_szFilename, __LINE__);
        CloseHandle(hFile);
        return 0;
    }
    
    if (memcmp( pBuffer, "B000FF\x0A", 7 )) {
        printf("Missing initial signature (BOOOFF\x0A). Not a BIN file\n");
        CloseHandle(hFile);
        return 0;
    }

    //
    // Read the image header
    // 
    fRet = ReadFile(hFile, &g_dwImageStart, sizeof(DWORD), &dwRead, NULL);

    if (!fRet || dwRead != sizeof(DWORD)) {
        CloseHandle(hFile);
        return 0;
    }

    fRet = ReadFile(hFile, &g_dwImageLength, sizeof(DWORD), &dwRead, NULL);

    if (!fRet || dwRead != sizeof(DWORD)) {
        CloseHandle(hFile);
        return 0;
    }

    printf("Image Start = 0x%08X, length = 0x%08X\n", g_dwImageStart, g_dwImageLength);

    szAscii[16] = '\0';
    //
    // Now read the records.
    //
    while (1) {
                
        //
        // Record address
        //
        fRet = ReadFile(hFile, &dwRecAddr, sizeof(DWORD), &dwRead, NULL);

        if (!fRet || dwRead != sizeof(DWORD)) {
            break;
        }

        //
        // Record length
        //
        fRet = ReadFile(hFile, &dwRecLen, sizeof(DWORD), &dwRead, NULL);

        if (!fRet || dwRead != sizeof(DWORD)) {
            break;
        }

        //
        // Record checksum
        //
        fRet = ReadFile(hFile, &dwRecChk, sizeof(DWORD), &dwRead, NULL);

        if (!fRet || dwRead != sizeof(DWORD)) {
            break;
        }

        g_Records[g_dwNumRecords].dwStartAddress = dwRecAddr;
        g_Records[g_dwNumRecords].dwLength       = dwRecLen;
        g_Records[g_dwNumRecords].dwChecksum     = dwRecChk;
        g_Records[g_dwNumRecords].dwFilePointer  = SetFilePointer(hFile, 0, NULL, FILE_CURRENT);

        if (g_fPrintRecords || g_fPrintData) {
            printf("Record [%3d] : Start = 0x%08X, Length = 0x%08X, Chksum = 0x%08X\n", g_dwNumRecords, dwRecAddr, dwRecLen, dwRecChk);
        }
        g_dwNumRecords++;
        
        if (dwRecAddr == 0 && dwRecChk == 0) {
            g_dwStartAddr = dwRecLen;
            printf ("\t\tStart address = 0x%08X\n", g_dwStartAddr);
            break;
        }
        
        //
        // Read or skip over the data
        //
        if (g_fPrintData) {
            DWORD chksum = 0;
            //
            // Print all the data
            //
            for (i = dwRecLen, t=0; i > 0; ) {
                dwReadReq = (i > BUFFER_SIZE ? BUFFER_SIZE : i);
                
                fRet = ReadFile(hFile, pBuffer, dwReadReq, &dwRead, NULL);
            
                if (!fRet || dwRead != dwReadReq) {
                    break;
                }
                i -= dwReadReq;
            
                for (j = 0; j < dwReadReq; j++, t++) {
                    if (t % 16 == 0) {
                        printf (" 0x%08X :", t + dwRecAddr);
                    }
                    if (t % 4 == 0) {
                        printf (" ");
                    }

                    printf ("%02X", pBuffer[j]);
                    chksum += pBuffer[j];
                    
                    if (pBuffer[j] < 0x20 || pBuffer[j] > 0x7E) {
                        szAscii[j%16] = '.';
                    } else {
                        szAscii[j%16] = pBuffer[j];
                    }
                    if ((j+1) % 16 == 0) {
                        printf (" %s \n", szAscii);
                    }
                }
            
            }
            
            //
            // Correct the last line (if partially printed a line)
            //
            dwGap = 16 - (j % 16);
            if (dwGap != 16) {
                for (; (j % 16); j++, t++) {
                    
                    if (t % 4 == 0) {
                        printf (" ");
                    }
                    printf ("  ");
                    szAscii[j%16] = ' ';
                }
            
                printf (" %s \n", szAscii);
            }

            
            printf ("\n");

            if(chksum != dwRecChk)
                printf(" Chksum error found in this record\n");
            else
                printf(" Chksum valid\n");
        
        } else {
            SetFilePointer(hFile, dwRecLen, NULL, FILE_CURRENT);
        }

    }

    while(ComputeRomOffset())
        if (g_fPrintTOC || g_fPrintOBJ)
            PrintTOC();

    if (g_fGenerateROM)
        WriteROMs();
  
    if (g_fGenerateSRE) {
        WriteSREHeader();

        for(i = 0; i < g_dwNumRecords; i++)
            WriteSRERecord(&g_Records[i]);

        WriteSREFooter();
    }

    CloseHandle(hFile);
    printf("Done.\n");
    return 0;
}
