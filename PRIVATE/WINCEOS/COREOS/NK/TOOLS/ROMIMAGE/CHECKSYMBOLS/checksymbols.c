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
DWORD dwFreq;
DWORD dwHighTime = 0;

BOOL g_fRet;
LPTSTR g_szFilename;
BOOL   g_fPrintData;
BOOL   g_fPrintRecords;
BOOL   g_fGenerateSRE;
BOOL   g_fGenerateROM;
BOOL   g_fPrintTOC;
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
    printf("Usage: checksymbols <filename>\r\n");
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

        if (!_tcscmp(TEXT("-data"), word) || !_tcscmp(TEXT("-d"), word)) {
            g_fPrintData = TRUE;
        }
        else if (!_tcscmp(TEXT("-toc"), word) || !_tcscmp(TEXT("-t"), word)) {
            g_fPrintTOC = TRUE;
        }
        else if (!_tcscmp(TEXT("-obj"), word) || !_tcscmp(TEXT("-o"), word)) {
            g_fPrintOBJ = TRUE;
        }
        else if (!_tcscmp(TEXT("-rec"), word) || !_tcscmp(TEXT("-r"), word)) {
            g_fPrintRecords = TRUE;
        }
        else if (!_tcscmp(TEXT("-checkfiles"), word) || !_tcscmp(TEXT("-c"), word)) {
            g_fCheckFiles = TRUE;
        }
        else if (word[0] == '-') {
            printf("Unknown option... %s\n", word);
            goto EXIT;
        }
        else {
            g_szFilename = word;
        }
    }

    g_fPrintTOC = TRUE;

    if (g_szFilename == NULL) {
        printf("Filename required\n");
        goto EXIT;
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
DWORD 
ComputeRomOffset()
{
    DWORD i;
    BOOL fFoundIt = FALSE;
    BOOL fRet;
    DWORD dwROMOffsetRead;

    for (i = 0; i < g_dwNumRecords; i++) {
        //
        // Check for potential TOC records
        //
        if (g_Records[i].dwLength == sizeof(ROMHDR)) {
            
            //
            // If this _IS_ the TOC record, compute the ROM Offset.
            //
            dwROMOffsetRead = (DWORD) g_Records[i].dwStartAddress - (DWORD) g_pTOC;

//            printf("Checking record #%d for potential TOC (ROMOFFSET = 0x%08X)\n", i, dwROMOffsetRead);
            //
            // Read out the record to verify. (unadjusted)
            //
            fRet = ReadBinFile(pBuffer, g_Records[i].dwStartAddress, sizeof(ROMHDR), 0);
    
            if (fRet) {
                ROMHDR* pTOC = (ROMHDR*) pBuffer;
            
                if ((pTOC->physfirst == (g_dwImageStart - dwROMOffsetRead)) &&
                    (pTOC->physlast  == (g_dwImageStart - dwROMOffsetRead) + g_dwImageLength)) {

                    //
                    // Extra sanity check...
                    //
                    if (pTOC->dllfirst <= pTOC->dlllast && pTOC->dlllast == 0x02000000) { 
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
    }

    if (fFoundIt) {
        g_dwROMOffset = dwROMOffsetRead;
    } else {
        g_dwROMOffset = 0;
    }
//    printf("ROMOFFSET = 0x%08X\n", g_dwROMOffset);

    return 0;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
PrintTOC()
{
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

        g_dwNumModules = pTOC->nummods;
        g_dwNumFiles = pTOC->numfiles;
        g_dwNumCopySects = pTOC->ulCopyEntries;
        g_dwCopyOffset = pTOC->ulCopyOffset;

    } else {
        printf("Couldn't locate TOC data\n");
        return;
    }


    //-------------------------------------------
    // Print Modules
    //
    fRet = ReadBinFile(pBuffer, (DWORD) g_pTOC + sizeof(ROMHDR), sizeof(TOCentry) * g_dwNumModules, g_dwROMOffset);

    if (fRet) {
        TOCentry* pte = (TOCentry*) pBuffer;
        e32_rom e32, *pe32 = &e32;
        o32_rom o32, *po32 = &o32;
        DWORD i;
        char szFilename[MAX_PATH];
        char szStatus[128];
        FILETIME ft;

        printf("\n");

        for (i=0;i < g_dwNumModules; i++) {

            HANDLE hFile;

            sprintf(szStatus, "okay");
            fRet = ReadBinFile((PBYTE) szFilename, (DWORD) pte[i].lpszFileName, 0, g_dwROMOffset);
            FileTimeToSystemTime(&(pte[i].ftTime), &st);

            hFile = CreateFile(szFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

            if (hFile == INVALID_HANDLE_VALUE) {
                sprintf(szStatus, "FILE NOT FOUND!");
                g_fRet = TRUE;
            } else {
                if (GetFileTime(hFile, NULL, NULL, &ft)) {
                    if (CompareFileTime(&(pte[i].ftTime), &ft)) {
                        sprintf(szStatus, "Timestamps do not match!");
                        g_fRet = TRUE;
                    } else {
                        if (GetFileSize(hFile, NULL) != pte[i].nFileSize) {
                            sprintf(szStatus, "File sizes do not match!");
                            g_fRet = TRUE;
                        }
                    }
                } else {
                    sprintf(szStatus, "Couldn't get file time!");
                    g_fRet = TRUE;
                }
                CloseHandle(hFile);
            }
            printf("    %2d/%02d/%04d  %02d:%02d:%02d  %10d  %20s (%s)\n", 
                   st.wMonth, st.wDay, st.wYear,
                   st.wHour, st.wMinute, st.wSecond,
                   pte[i].nFileSize, szFilename, szStatus);
        }
    } else {
        printf("ERROR: Couldn't locate Modules data\n");
    }

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    DWORD dwRecAddr, dwRecLen, dwRecChk;
    DWORD dwRead;
    BOOL  fRet;
    LPTSTR pszCmdLine;
    char szAscii[17];

    pszCmdLine = GetCommandLine();

    // Parse command line parameters (updates the ui variables).
    if (!parseCmdLine(pszCmdLine))  
       return 0;
   
    
    printf("CHECKSYMBOLS... %s\n", g_szFilename);
    
    
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

//    printf("Image Start = 0x%08X, length = 0x%08X\n", g_dwImageStart, g_dwImageLength);

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
        
        if (dwRecAddr == 0) {
            g_dwStartAddr = dwRecLen;
            break;
        }
        
        SetFilePointer(hFile, dwRecLen, NULL, FILE_CURRENT);

    }

    //
    // Find pTOC
    //
    fRet = ReadBinFile(pBuffer, g_dwImageStart + 0x40, 8, 0);
    if (fRet) {
        g_pTOC = *((PDWORD) (pBuffer + 4));
    } else {
        printf ("Couldn't find pTOC @ Image Start (0x%08X) + 0x40\n", g_dwImageStart);
        return TRUE;
    }

    ComputeRomOffset();

    if (g_fPrintTOC || g_fPrintOBJ) {
        PrintTOC();
    }

    CloseHandle(hFile);
    return g_fRet;
}
