//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include <binfs.h>

// Debug Zone
#ifdef DEBUG
DBGPARAM dpCurSettings =
{
    TEXT("BINFS"),
    {
        TEXT("Init"),
        TEXT("Deinit"),
        TEXT("Main"),
        TEXT("API"),
        TEXT("I/O"),
        TEXT("Undefined"),
        TEXT("Undefined"),
        TEXT("Undefined"),
        TEXT("Undefined"),
        TEXT("Undefined"),
        TEXT("Undefined"),
        TEXT("Helper"),
        TEXT("Current"),
        TEXT("Undefined"),
        TEXT("Warning"), 
        TEXT("Error"),
    },
    ZONEMASK_INIT
};
#endif


#ifdef DEBUG
void PrintROMHeader(ROMHDR *pRomHdr)
{
    TCHAR szCpuName[20];
    
    DEBUGMSG( ZONE_INIT, (TEXT("========================== ROMHDR =========================")));
    DEBUGMSG( ZONE_INIT, (L"    DLL First           : 0x%08X  \n", pRomHdr->dllfirst));
    DEBUGMSG( ZONE_INIT, (L"    DLL Last            : 0x%08X  \n", pRomHdr->dlllast));
    DEBUGMSG( ZONE_INIT, (L"    Physical First      : 0x%08X  \n", pRomHdr->physfirst));
    DEBUGMSG( ZONE_INIT, (L"    Physical Last       : 0x%08X  \n", pRomHdr->physlast));
    DEBUGMSG( ZONE_INIT, (L"    RAM Start           : 0x%08X  \n", pRomHdr->ulRAMStart));
    DEBUGMSG( ZONE_INIT, (L"    RAM Free            : 0x%08X  \n", pRomHdr->ulRAMFree));
    DEBUGMSG( ZONE_INIT, (L"    RAM End             : 0x%08X  \n", pRomHdr->ulRAMEnd));
    DEBUGMSG( ZONE_INIT, (L"    Kernel flags        : 0x%08X  \n", pRomHdr->ulKernelFlags));
    DEBUGMSG( ZONE_INIT, (L"    Prof Symbol Offset  : 0x%08X  \n", pRomHdr->ulProfileOffset));
    DEBUGMSG( ZONE_INIT, (L"    Num Copy Entries    : %10d    \n", pRomHdr->ulCopyEntries));
    DEBUGMSG( ZONE_INIT, (L"    Copy Entries Offset : 0x%08X  \n", pRomHdr->ulCopyOffset));
    DEBUGMSG( ZONE_INIT, (L"    Num Modules         : %10d    \n", pRomHdr->nummods));
    DEBUGMSG( ZONE_INIT, (L"    Num Files           : %10d    \n", pRomHdr->numfiles));
    DEBUGMSG( ZONE_INIT, (L"    Kernel Debugger     : %10s\n", pRomHdr->usMiscFlags & 0x1 ? "Yes" : "No"));
    
    switch(pRomHdr->usCPUType) {
    case IMAGE_FILE_MACHINE_SH3:
        wcscpy( szCpuName, (L"(SH3)\n"));
        break;
    case IMAGE_FILE_MACHINE_SH3E:
        wcscpy( szCpuName, (L"(SH3e)\n"));
        break;
    case IMAGE_FILE_MACHINE_SH3DSP:
        wcscpy( szCpuName, (L"(SH3-DSP)\n"));
        break;
    case IMAGE_FILE_MACHINE_SH4:
        wcscpy( szCpuName, (L"(SH4)\n"));
        break;
    case IMAGE_FILE_MACHINE_I386:
        wcscpy( szCpuName, (L"(x86)\n"));
        break;
    case IMAGE_FILE_MACHINE_THUMB:
        wcscpy( szCpuName, (L"(Thumb)\n"));
        break;
    case IMAGE_FILE_MACHINE_ARM:
        wcscpy( szCpuName, (L"(ARM)\n"));
        break;
    case IMAGE_FILE_MACHINE_POWERPC:
        wcscpy( szCpuName, (L"(PPC)\n"));
        break;
    case IMAGE_FILE_MACHINE_R4000:
        wcscpy( szCpuName, (L"(R4000)\n"));
        break;
    case IMAGE_FILE_MACHINE_MIPS16:
        wcscpy( szCpuName, (L"(MIPS16)\n"));
        break;
    case IMAGE_FILE_MACHINE_MIPSFPU:
        wcscpy( szCpuName, (L"(MIPSFPU)\n"));
        break;
    case IMAGE_FILE_MACHINE_MIPSFPU16:
        wcscpy( szCpuName, (L"(MIPSFPU16)\n"));
        break; 
    default:
        wcscpy( szCpuName, (L"(Unknown)"));
    }
    
    DEBUGMSG( ZONE_INIT, (L"    CPU                 :     0x%04x%s", pRomHdr->usCPUType, szCpuName)); 
    DEBUGMSG( ZONE_INIT, (L"    Extensions          : 0x%08X\n", pRomHdr->pExtensions));
    DEBUGMSG( ZONE_INIT, (TEXT("===========================================================")));
}


void PrintFileListing(LPVOID pv)
{
    SYSTEMTIME st;
    BinDirList *pDirectory = (BinDirList *)pv;
    DEBUGMSG( ZONE_INIT, (TEXT("======================================== Files ===========================================")));
    while(pDirectory) {
        FileTimeToSystemTime(&pDirectory->ft, &st);
        DEBUGMSG( ZONE_INIT,  (L"     %2d/%02d/%04d  %02d:%02d:%02d  %c%c%c%c(0x%08X) %10d %10d  %18s (ROM 0x%08X)\n", 
            st.wMonth, st.wDay, st.wYear,
            st.wHour, st.wMinute, st.wSecond,
            (pDirectory->dwAttributes & FILE_ATTRIBUTE_COMPRESSED ? L'C' : L'_'),
            (pDirectory->dwAttributes & FILE_ATTRIBUTE_HIDDEN     ? L'H' : L'_'),
            (pDirectory->dwAttributes & FILE_ATTRIBUTE_READONLY   ? L'R' : L'_'),
            (pDirectory->dwAttributes & FILE_ATTRIBUTE_SYSTEM     ? L'S' : L'_'),
            pDirectory->dwAttributes,
            (pDirectory->dwAttributes & FILE_ATTRIBUTE_COMPRESSED ? pDirectory->dwCompFileSize: 0),
            pDirectory->dwRealFileSize, pDirectory->szFileName, pDirectory->dwAddress));
        pDirectory = pDirectory->pNext;            
    }        
    DEBUGMSG( ZONE_INIT, (TEXT("==========================================================================================")));
}

#endif
