// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------
#ifndef __STORAGEDETECT_H__
#define __STORAGEDETECT_H__

#include <windows.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <storehlp.h>
//#include <storemgr.h>
#include <clparse.h>
#include "main.h"   





/*
    Prototypes:
            Prototypes for each of the individual unit tests
*/

TESTPROCAPI StorageExists(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

//non test prototypes
void ParseCmdLine(LPCTSTR szCmdLine);
DWORD MatchByPart(DWORD dwProfile);
BOOL MatchByVolume(DWORD dwProfile);


/* 
    Tux testproc function table
*/
#ifndef __DEFINE_FTE__
#define __DEFINE_FTE__
#define BEGIN_FTE FUNCTION_TABLE_ENTRY g_lpFTE[] = {
#define FTH(IndentationLevel, Description) { TEXT(Description), IndentationLevel, 0, 0, NULL },
#define FTE(IndentationLevel, Description, UniqueId, Parameter, FunctionAddress) { TEXT(Description), IndentationLevel, Parameter, UniqueId, FunctionAddress },
#define END_FTE { NULL, 0, 0, 0, NULL } };


//Used in function table
//OR these together to form the bitmask parameter that determines
//what profiles will result in a PASS

//When adding support for a new profile, it should be added to 
//the spots indicated below
#define SDMEMORY 0x1
#define SDHCMEMORY 0x2
#define MMC 0x4
#define RAMDISK 0x8
#define MSFLASH 0x10
#define HDPROFILE 0x20
#define CDPROFILE 0x40
#define PCMCIA 0x80
#define USBHDProfile 0x100
#define USBCDProfile 0x200
#define USBFDProfile 0x400


const DWORD gc_dwProfileCount = 11; //increment for each added profile
const DWORD gc_dwSupportedMatchByVolume = SDMEMORY | SDHCMEMORY | MMC;  //SAKI: Add more if supported

DWORD g_bSearchPartitions = TRUE; //set by command line

const DWORD gc_dwProfileMasks[] = {
    SDMEMORY,
    SDHCMEMORY,
    MMC,
    RAMDISK,
    MSFLASH,
    HDPROFILE,
    CDPROFILE,
    PCMCIA,
    USBHDProfile,
    USBCDProfile,
    USBFDProfile
    //NEWPROFILE
};

const TCHAR * gc_sProfileNames[] = {
    L"SDMemory",
    L"SDHCMemory",
    L"MMC",
    L"RamDisk",
    L"MSFlash",
    L"HDProfile",
    L"CDProfile",
    L"PCMCIA",
    L"USBHDProfile",
    L"USBCDProfile",
    L"USBFDProfile"
    //L"NewProfile"
};

BEGIN_FTE
    FTH(0, "Storage Type Detection Tests")
        FTE(1,      "SDMemoryExists test", 0, SDMEMORY, StorageExists)
        FTE(1,      "SDHCMemoryExists test", 1, SDHCMEMORY, StorageExists)
        FTE(1,      "MMCMemoryExists test", 2, MMC, StorageExists)
        FTE(1,      "RamDiskExists test", 3, RAMDISK, StorageExists)
        FTE(1,      "MSFlashExists test", 4, MSFLASH, StorageExists)
        FTE(1,      "HDProfileExists test", 5, HDPROFILE, StorageExists)
        FTE(1,      "CDProfileExists test", 6, CDPROFILE, StorageExists)
        FTE(1,      "PCMCIAExists test", 7, PCMCIA, StorageExists)
        FTE(1,      "USBHDProfileExists test", 8, USBHDProfile, StorageExists)
        FTE(1,      "USBCDProfileExists test", 9, USBCDProfile, StorageExists)
        FTE(1,      "USBFDProfileExists test", 10, USBFDProfile, StorageExists)
        FTE(1,      "AnytypeSDorMMCexists test", 11, SDMEMORY | SDHCMEMORY | MMC, StorageExists)

/*
    If you'd like any portion of your unit test enabled
    to be incorporated into CE Stress, add entries below the "CEStress"
    FTH entry below.  You can re-use TestProcs used anywhere else in 
    this structure, or have specific TestProcs for stress
*/

    FTH(0, "CEStress")
END_FTE

#endif // __DEFINE_FTE


/*
    assorted random info below.
    probably no need to touch this for most cases.
*/

// NT ASSERT macro
#ifdef UNDER_NT
#include <crtdbg.h>
#define ASSERT _ASSERT
#define HFILE FILE*
#define Debug 
#endif


// debugging
#ifdef UNDER_CE
#define Debug NKDbgPrintfW
#endif

#define MODULE_NAME _T("StorageDetect")

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;
extern "C" {
   __declspec(dllexport) CKato* GetKato(void){return g_pKato;}
}

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

#endif // __STORAGEDETECT_H__
