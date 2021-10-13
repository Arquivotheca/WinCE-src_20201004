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
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <tchar.h>

#ifdef UNDER_CE
#include <sysutils.h>
#include <bldver.h> //for CE_MAJOR_VER
#else
#include <assert.h>
#define ASSERT      assert
#define Random      rand
#endif

#include <tchar.h>
#include "thelper.h"


#define     TST_TRACE_BUFFER_SIZE   5*1024
//+ ========================================================================
//- ========================================================================
void TRACE(LPCTSTR szFormat, ...)
{
    TCHAR   szBuffer[TST_TRACE_BUFFER_SIZE] = {0};
    va_list pArgs;

    va_start(pArgs, szFormat);

    if (STRSAFE_E_INSUFFICIENT_BUFFER == StringCchVPrintf (szBuffer, TST_TRACE_BUFFER_SIZE, szFormat, pArgs))
   {
        OutputDebugString(_T("TRACE buffer is too small\n"));
        ASSERT(FALSE);
   }

    va_end(pArgs);

    OutputDebugString(szBuffer);
}

//+ ======================================================================
//- ======================================================================
void Hlp_PrintTestHeader(LPCTSTR pszTestTitle)
{
    TRACE(_T("\n\n=====================================================\n"));
    TRACE(_T("Begin Test %s\n"), pszTestTitle);
    TRACE(_T("=====================================================\n"));

}

//+ ======================================================================
//- ======================================================================
void Hlp_PrintTestFooter(LPCTSTR pszTestTitle)
{
    TRACE(_T("\n\n=====================================================\n"));
    TRACE(_T("End Test %s\n"), pszTestTitle);
    TRACE(_T("=====================================================\n"));

}

//+ ========================================================================
//
//  Function    :   Hlp_GetSysTimeAsFileTime
//
//  Returns TRUE on success (or FALSE otherwise)
//  returns the current SYSTEMTIME as a FILETIME
//
//- ========================================================================
BOOL Hlp_GetSysTimeAsFileTime(FILETIME *retFileTime)
{
    SYSTEMTIME  mySysTime;

    GetSystemTime(&mySysTime);
    if (!SystemTimeToFileTime(&mySysTime, retFileTime))
    GENERIC_FAIL(SystemTimeToFileTime);

    return TRUE;
ErrorReturn :
    return FALSE;
}


//+ ========================================================================
//      Proposed Flags :
//      HLP_FILL_RANDOM
//      HLP_FILL_SEQUENTIAL
//      HLP_FILL_DONT_CARE  -   fastest
//
//- ========================================================================
BOOL Hlp_FillBuffer(__out_ecount(cbBuffer) PBYTE pbBuffer, DWORD cbBuffer, DWORD dwFlags)
{
    DWORD   i=0;
    switch (dwFlags)
    {
        case HLP_FILL_RANDOM :
        for (i=0; i<cbBuffer; i++)
        {
            pbBuffer[i] = (BYTE)Random();
        }
        break;
        case HLP_FILL_SEQUENTIAL :
        for (i=0; i<cbBuffer; i++)
        {
            pbBuffer[i] = (BYTE)i;
            }
            break;
            case HLP_FILL_DONT_CARE :
            memset(pbBuffer, 33, cbBuffer);
            break;
        }

     return TRUE;
}



//+ =================================================================
//- =================================================================
DWORD   Hlp_DumpBuffer(const BYTE* pbBuffer, DWORD cbBuffer)
{
    UINT    ui=0;
    UINT    uiBytesPerLine=30;

    while (ui<cbBuffer)
    {
        if (0==ui%uiBytesPerLine)
        {
            TRACE(_T("\n"));
            TRACE(_T("%04d : "), ui);
            }
        TRACE(_T("%02X "), *(pbBuffer+ui));
        ui++;
     }
    return 1;
}


//+ ======================================================================
//- ======================================================================
void Hlp_GenRandomFileTime(FILETIME *myFileTime)
{
    //  generate random time
    (*myFileTime).dwLowDateTime = Random();
    (*myFileTime).dwHighDateTime = Random();
}


//+ ======================================================================
//
//  Function    :   Hlp_GenStringData
//
//  Description :   generates string data.
//
//  Params :
//  dwFlags         TST_FLAG_READABLE
//                  TST_FLAG_RANDOM (default)
//                  TST_FLAG_ALPHA
//                  TST_FLAG_ALPHA_NUM
//
//  cChars          is the count of characters that the buffer
//                  can hold, including the NULL character.
//
//
//- ======================================================================
LPTSTR  Hlp_GenStringData(__out_ecount(cChars) LPTSTR pszString, DWORD cChars, DWORD dwFlags)
{
    UINT    i=0;
    BYTE    bData=0;
    BOOL    fDone=FALSE;

    ASSERT(pszString);
    ASSERT(cChars);

    if (!cChars)
        return NULL;

    //  Generate cChars-1 characters (leaving space for the terminating NULL)
    UINT len= cChars-1;
    //  Generate cChars-1 characters (leaving space for the terminating NULL)
    for (i=0; i<len; i++)
    {
        fDone = FALSE;
        while(!fDone)
        {
            bData=(BYTE)Random();
            if (bData<0) bData *= (BYTE)-1;
            bData = bData % 0xFF;  //  generate random chars between 0 and 255

            switch (dwFlags)
            {
                case TST_FLAG_READABLE :
                    if ((bData >= 32) && (bData <= 126))
                       fDone=TRUE;
                break;
                case TST_FLAG_ALPHA_NUM :
                    if ((bData >= '0') && (bData <= '9')
                    || (bData >= 'a') && (bData <= 'z')
                    || (bData >= 'A') && (bData <= 'Z'))
                    fDone=TRUE;
                break;
                case TST_FLAG_ALPHA :
                    if ((bData >= 'a') && (bData <= 'z')
                    || (bData >= 'A') && (bData <= 'Z'))
                        fDone=TRUE;
                break;
                case TST_FLAG_NUMERIC :
                     if ((bData >= '0') && (bData <= '9'))
                     fDone=TRUE;
                break;

                default :
                TRACE(_T("Should never reach here. Unknown Flags\n"));
                ASSERT(FALSE);
                GENERIC_FAIL(Hlp_GenStringData);
            }
        }
        pszString[i] = (TCHAR) bData;
    }// for

    //  terminating NULL
    pszString[i] = _T('\0');

    return pszString;
ErrorReturn :
    return NULL;

}


//+ ========================================================================
//
//      Returns the Text of the LastError
//
//- ========================================================================
LPTSTR WINAPI  Hlp_LastErrorToText(DWORD dwLastError, __out_ecount(cbBuffer) LPTSTR pszBuffer, DWORD cbBuffer)
{
    switch(dwLastError)
    {
        case ERROR_CALL_NOT_IMPLEMENTED :
            StringCchCopy(pszBuffer,  cbBuffer, _T("ERROR_CALL_NOT_IMPLEMENTED"));
        break;
        case ERROR_INVALID_PARAMETER  :
            StringCchCopy(pszBuffer, cbBuffer, _T( "ERROR_INVALID_PARAMETER"));
        break;
        case ERROR_NOT_ENOUGH_MEMORY  :
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_NOT_ENOUGH_MEMORY"));
        break;
        case NTE_BAD_FLAGS  :
            StringCchCopy(pszBuffer, cbBuffer,  _T(" NTE_BAD_FLAGS"));
        break;
        case NTE_BAD_KEYSET  :
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_BAD_KEYSET"));
        break;
        case NTE_BAD_KEYSET_PARAM   :
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_BAD_KEYSET_PARAM"));
        break;
        case NTE_BAD_PROV_TYPE  :
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_BAD_PROV_TYPE"));
        break;
        case NTE_BAD_SIGNATURE  :
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_BAD_SIGNATURE"));
        break;
        case NTE_EXISTS  :
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_EXISTS"));
        break;
        case NTE_KEYSET_ENTRY_BAD  :
            StringCchCopy(pszBuffer, cbBuffer, _T(" NTE_KEYSET_ENTRY_BAD"));
        break;
        case NTE_KEYSET_NOT_DEF  :
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_KEYSET_NOT_DEF"));
        break;
        case NTE_NO_MEMORY  :
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_NO_MEMORY"));
        break;
        case NTE_PROV_DLL_NOT_FOUND:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_PROV_DLL_NOT_FOUND"));
        break;
        case NTE_PROV_TYPE_ENTRY_BAD:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_PROV_TYPE_ENTRY_BAD"));
        break;
        case NTE_PROV_TYPE_NO_MATCH:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_PROV_TYPE_NO_MATCH"));
        break;
        case NTE_PROV_TYPE_NOT_DEF:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_PROV_TYPE_NOT_DEF"));
        break;
        case NTE_PROVIDER_DLL_FAIL:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_PROVIDER_DLL_FAIL"));
        break;
        case NTE_SIGNATURE_FILE_BAD:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_SIGNATURE_FILE_BAD"));
        break;
        case ERROR_INVALID_HANDLE:
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_INVALID_HANDLE"));
        break;
        case ERROR_NO_MORE_ITEMS:
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_NO_MORE_ITEMS"));
        break;
        case NTE_BAD_TYPE:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_BAD_TYPE"));
        break;
        case NTE_BAD_UID:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_BAD_UID"));
        break;
        case ERROR_BUSY:
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_BUSY"));
        break;
        case NTE_BAD_ALGID:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_BAD_ALGID"));
        break;
        case NTE_BAD_HASH:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_BAD_HASH"));
        break;
        case NTE_BAD_KEY:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_BAD_KEY"));
        break;
        case NTE_BAD_KEY_STATE:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_BAD_KEY_STATE"));
        break;
        case ERROR_MORE_DATA:
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_MORE_DATA"));
        break;
        case NTE_BAD_PUBLIC_KEY:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_BAD_PUBLIC_KEY"));
        break;
        case NTE_NO_KEY:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_NO_KEY"));
        break;
        case NTE_FAIL :
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_FAIL"));
        break;
        case NTE_BAD_DATA :
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_BAD_DATA"));
        break;
        case NTE_BAD_VER:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_BAD_VER"));
        break;
        case NTE_BAD_LEN :
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_BAD_LEN"));
        break;
        case NTE_DOUBLE_ENCRYPT:
            StringCchCopy(pszBuffer, cbBuffer, _T("NTE_DOUBLE_ENCRYPT"));
        break;

        //  FAT errors
        case ERROR_ALREADY_EXISTS :
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_ALREADY_EXISTS"));
        break;
        case ERROR_DISK_FULL :
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_DISK_FULL"));
        break;

        case ERROR_ACCESS_DENIED :
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_ACCESS_DENIED"));
        break;
        case ERROR_DUP_NAME :
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_DUP_NAME"));
        break;
        case ERROR_SHARING_VIOLATION :
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_SHARING_VIOLATION"));
        break;

        case ERROR_SEEK :
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_SEEK"));
        break;

        case ERROR_INSUFFICIENT_BUFFER :
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_INSUFFICIENT_BUFFER"));
        break;

        case ERROR_FILE_NOT_FOUND :
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_FILE_NOT_FOUND"));
        break;
        case ERROR_FILE_EXISTS :
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_FILE_EXISTS"));
        break;
        case ERROR_PATH_NOT_FOUND :
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_PATH_NOT_FOUND"));
        break;
        case ERROR_BADKEY :
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_BADKEY"));
        break;
        case ERROR_NEGATIVE_SEEK :
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_NEGATIVE_SEEK"));
        break;
        case ERROR_NO_MORE_FILES :
            StringCchCopy(pszBuffer, cbBuffer, _T("ERROR_NO_MORE_FILES"));
        break;


        default :
            StringCchCopy(pszBuffer, cbBuffer, _T("Unknown Error"));
    }
    return pszBuffer;

}


//+ =================================================================
//- =================================================================
void Hlp_DumpLastError(void)
{
    TCHAR rgszBuffer[256]={0};
    Hlp_LastErrorToText(GetLastError(), rgszBuffer, 256);
    TRACE(_T(" %s "), rgszBuffer);
}


//+ =================================================================
//- =================================================================
void Hlp_DumpError(DWORD dwError)
{
    TCHAR rgszBuffer[256]={0} ;
    Hlp_LastErrorToText(dwError, rgszBuffer, 256);
    TRACE(_T(" %s "), rgszBuffer);
}


//+ ========================================================================
//- ========================================================================
DWORD   WINAPI Hlp_CheckLastError(DWORD    dwLastError)
{
    if (GetLastError() != dwLastError)
    {
        printf("Expecting LastError 0x%x got 0x%x\n", dwLastError, GetLastError());
        return 0;
    }
    return 1;
}


//  ======================================================================================
//  Function    :       Hlp_MyReadFileExA
//
//  Synopsis    :       Helper function that takes a file name and returns the file in
//                      a buffer.
//
//  NOTE        :       This is a single pass call.
//
//  Return      :       If the function fails, it returns 0 else it returns the number of bytes
//                      read.
//
//  Changed to TRACE
//
//  ======================================================================================
DWORD  WINAPI Hlp_MyReadFileExA( LPCTSTR pszFileName,
                                PBYTE *ppbFile,
                                DWORD *pdwFile)
{
    HANDLE      hFile=INVALID_HANDLE_VALUE;
    DWORD       dwBytesRead=0, dwFileSize=0;
    DWORD       dwRetVal=0;    // this is initially set to an error value
    PBYTE       pbTemp= NULL;
    DWORD       cbTemp=0;

    if (!ppbFile)
        return 0;

    hFile = CreateFile( pszFileName,
                        GENERIC_READ,
                        0,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);

    if (HFILE_ERROR == (HFILE)hFile)
    {
        TRACE(_T("Failed to open file for read %s.  0x%x\n"), pszFileName, GetLastError());
        goto ErrorReturn;
    }

    dwFileSize = GetFileSize(hFile, NULL);
    cbTemp = dwFileSize;
    pbTemp = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbTemp);
    CHECK_ALLOC(pbTemp);
    memset(pbTemp, 33, cbTemp);

    if (!ReadFile(hFile, (BYTE*)pbTemp, cbTemp, &dwBytesRead, NULL))
    {
        TRACE(_T("Error reading file 0x%x\n"), GetLastError());
        goto ErrorReturn;
    }

    dwRetVal=dwBytesRead;
    *ppbFile = pbTemp;
    *pdwFile = cbTemp;

    ErrorReturn :
    CLOSE_HANDLE(hFile);
    //  don't free pbTemp because the user has to free it.

    return dwRetVal;

}

//  =============================================================================
//  8/27/99 : Changed printf to TRACE(_T)
//  =============================================================================
DWORD    WINAPI Hlp_WriteBufToFile( LPCTSTR pszFileName,
                                    const BYTE* pbBuffer,
                                    DWORD cbBuffer)
{
    DWORD       dwRetVal=0;
    HANDLE      hFile=INVALID_HANDLE_VALUE;
    DWORD       dwBytesWritten=0;

    hFile = CreateFile( pszFileName,
                        GENERIC_WRITE,
                        0,
                        NULL,
                        CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);

    if (INVALID_HANDLE_VALUE == hFile)
    {
        TRACE(_T("CreateFile failed (WRITE) %s.  0x%x\n"), pszFileName, GetLastError());
        goto ErrorReturn;
    }

    if (!WriteFile(
                hFile,
                pbBuffer,
                cbBuffer,
                &dwBytesWritten,
                NULL))
        GENERIC_FAIL(WriteFile);

    dwRetVal = dwBytesWritten;

ErrorReturn :
    CLOSE_HANDLE(hFile);
    return dwRetVal;
}


//+ ============================================================
//
//  All these functions for some reason or another are CE specific
//
//- ============================================================
#ifdef UNDER_CE

//+ ======================================================================
//  lidGerman is defined in lid.h
//  This is a local hack.
//- ======================================================================
BOOL Hlp_Is_German(void)
{
    LANGID  LangId = 0;GetSystemDefaultLangID();

    LangId = GetSystemDefaultLangID();

    if (lidGerman == LangId)
        return TRUE;
    else
        return FALSE;
}

//+ ===============================================================
//  Returns the amount of free memory
//- ===============================================================
DWORD Hlp_GetAvailMem_Physical(void)
{

    DWORD   dwRetVal =0;
    MEMORYSTATUS        memStatus;

    memset(&memStatus, 0, sizeof(MEMORYSTATUS));

    GlobalMemoryStatus(&memStatus);

    dwRetVal = memStatus.dwAvailPhys;

    return dwRetVal;
}

//+ ===============================================================
//  Returns the amount of free memory
//- ===============================================================
DWORD Hlp_GetAvailMem(void)
{
    return Hlp_GetAvailMem_Physical();
}


//+ ===============================================================
//  Returns the amount of free memory
//- ===============================================================
DWORD Hlp_GetAvailMem_Virtual(void)
{
    DWORD   dwRetVal =0;
    MEMORYSTATUS        memStatus;

    memset(&memStatus, 0, sizeof(MEMORYSTATUS));

    GlobalMemoryStatus(&memStatus);

    dwRetVal = memStatus.dwAvailPhys;

    return dwRetVal;
}


//+ ========================================================================
//      Displays the systems current memory division
//- ========================================================================
void Hlp_DisplayMemoryInfo(void)
{
    DWORD   dwStorePages=0;
    DWORD   dwStorePages_Free=0;

    DWORD   dwRamPages=0;
    DWORD   dwRamPages_Free=0;

    DWORD   dwPageSize=0;
    STORE_INFORMATION   StoreInfo;
    MEMORYSTATUS        memStatus;

    memset((PBYTE)&StoreInfo, 0, sizeof(STORE_INFORMATION));
    memset((PBYTE)&memStatus, 0, sizeof(MEMORYSTATUS));

    //  Get the page division information
    if (!GetSystemMemoryDivision(&dwStorePages, &dwRamPages, &dwPageSize))
        GENERIC_FAIL(GetSystemMemoryDivision);


    //  Get information about store pages.
    if (!GetStoreInformation(&StoreInfo))
        GENERIC_FAIL(GetStoreInformation);

    //  Get information about memory pages
    GlobalMemoryStatus(&memStatus);

    dwRamPages_Free = memStatus.dwAvailPhys/dwPageSize;
    dwStorePages_Free = StoreInfo.dwFreeSize/dwPageSize;

    TRACE(_T("SystemMemory Info :\n"));
    TRACE(_T("StorePages : Free= %6d Used= %6d Total= %6d\n"),
                    dwStorePages_Free,
                    dwStorePages - dwStorePages_Free,
                    dwStorePages);

    TRACE(_T("RamPages   : Free= %6d Used= %6d Total= %6d\n"),
                    dwRamPages_Free,
                    dwRamPages - dwRamPages_Free,
                    dwRamPages);
    TRACE(_T("PageSize = %d bytes/page \n"), dwPageSize);

ErrorReturn :
   ;
}


//+ ========================================================================
//  Returns the Num of Pages of Free storage mem
//- ========================================================================
DWORD   Hlp_GetFreeStorePages(void)
{
    DWORD   dwRetVal=0;
    DWORD   dwStorePages=0;
    DWORD   dwRamPages=0;
    DWORD   dwPageSize=0;
    STORE_INFORMATION   StoreInfo;

    memset((PBYTE)&StoreInfo, 0, sizeof(STORE_INFORMATION));

    //  Get the page division information
    if (!GetSystemMemoryDivision(&dwStorePages, &dwRamPages, &dwPageSize))
        GENERIC_FAIL(GetSystemMemoryDivision);


    //  Get information about store pages.
    if (!GetStoreInformation(&StoreInfo))
        GENERIC_FAIL(GetStoreInformation);

    dwRetVal = StoreInfo.dwFreeSize/dwPageSize;

ErrorReturn :
    return dwRetVal;
}


//+ ========================================================================
//  Returns the Num of Pages of Free storage mem
//- ========================================================================
DWORD   Hlp_GetFreePrgmPages(void)
{
    DWORD   dwRetVal=0;
    DWORD   dwStorePages=0;

    DWORD   dwRamPages=0;
    DWORD   dwPageSize=0;

    STORE_INFORMATION   StoreInfo;
    MEMORYSTATUS        memStatus;

    memset((PBYTE)&StoreInfo, 0, sizeof(STORE_INFORMATION));
    memset((PBYTE)&memStatus, 0, sizeof(MEMORYSTATUS));

    //  Get the page division information
    if (!GetSystemMemoryDivision(&dwStorePages, &dwRamPages, &dwPageSize))
        GENERIC_FAIL(GetSystemMemoryDivision);


    //  Get information about memory pages
    GlobalMemoryStatus(&memStatus);

    dwRetVal = memStatus.dwAvailPhys/dwPageSize;

ErrorReturn :
    return dwRetVal;
}


//+ ========================================================================
//  Can either increase or decrease the amount of allocated Program memory
//- ========================================================================
BOOL Hlp_BumpProgramMem(int dwNumPages)
{
    DWORD   dwStorePages=0;
    DWORD   dwRamPages=0;
    DWORD   dwPageSize=0;
    DWORD   dwAdjust=0;
    DWORD   dwResult=0;
    BOOL    fRetVal=0;

    dwAdjust = dwNumPages;

    if (dwNumPages>0)
        TRACE(_T("Increasing Program Mem by %d pages\n"), dwNumPages);
    else
        TRACE(TEXT("Reducing Program Mem by %d pages\r\n"), -1*dwNumPages);

    //  Get the system memory information
    if (!GetSystemMemoryDivision(&dwStorePages, &dwRamPages, &dwPageSize))
        GENERIC_FAIL(GetSystemMemoryDivision);

    dwResult = SetSystemMemoryDivision(dwStorePages-dwAdjust);
    switch(dwResult)
    {
        case SYSMEM_FAILED:
            TRACE( _T( "SetSystemMemoryDivision: ERROR value out of range!\n" ) );
            goto ErrorReturn;
        break;

        case SYSMEM_MUSTREBOOT:
        case SYSMEM_REBOOTPENDING:
            TRACE( _T( "SetSystemMemoryDivision: ERROR reboot request!\r\n" ) );
            goto ErrorReturn;
        break;
    }

    fRetVal=TRUE;
ErrorReturn :
    return fRetVal;
}


//+ ========================================================================
//  Reduces the allocated amount of program memory on the system by the
//  dwNumPages number of pages.
//- ========================================================================
BOOL Hlp_ReduceProgramMem(int dwNumPages)
{
    return Hlp_BumpProgramMem(-dwNumPages);
}


//+ ======================================================================
//      make the Storage and Prgm memory almost equal.
//- ======================================================================
BOOL Hlp_Equalize_Store_Prgm_Mem(void)
{
    UINT dwFreeStorePages=0;
    UINT dwFreePrgmPages=0;
    UINT dwAverage=0;
    UINT dwPrgmDiff=0;
    BOOL    bRetVal=FALSE;

    TRACE(_T("Equalizing Program and Storage free space\n"));
    Hlp_DisplayMemoryInfo();

    //  Get the storage Free space
    dwFreeStorePages = Hlp_GetFreeStorePages();

    //  Get program free space
    dwFreePrgmPages = Hlp_GetFreePrgmPages();

    dwAverage = (dwFreeStorePages+dwFreePrgmPages) / 2;
    dwPrgmDiff = dwAverage - dwFreePrgmPages;


TRACE(TEXT("dwAverage = %d\r\n"), dwAverage);
TRACE(TEXT("dwPrgmDiff = %d\r\n"), dwPrgmDiff);

    bRetVal = Hlp_BumpProgramMem(dwPrgmDiff);

    Hlp_DisplayMemoryInfo();
    return bRetVal;
}


//+ ======================================================================
//      Leave Only 50 pages of store mem
//- ======================================================================
BOOL Hlp_EatAll_Store_Mem(void)
{
    int dwFreeStorePages=0;
    BOOL    bRetVal=FALSE;

    TRACE(_T("Eating up all free Storage Space....\n"));
    Hlp_DisplayMemoryInfo();

    //  Get the storage Free space
    dwFreeStorePages = Hlp_GetFreeStorePages();

    if (dwFreeStorePages-50 >0)
        bRetVal = Hlp_BumpProgramMem(dwFreeStorePages-50);

    Hlp_DisplayMemoryInfo();
    return bRetVal;
}


//+ ======================================================================
//      make the Storage and Prgm memory almost equal.
//  Leaves only dwLeavemem number of storage pages.
//- ======================================================================
BOOL Hlp_EatAll_Store_MemEx(DWORD dwLeaveMem)
{
    int dwFreeStorePages=0;
    BOOL    bRetVal=FALSE;

    TRACE(_T("Eating up all Storage Space leaving %d pages....\n"), dwLeaveMem);
    Hlp_DisplayMemoryInfo();

    //  Get the storage Free space
    dwFreeStorePages = Hlp_GetFreeStorePages();

    //  This would only do it if it implies increasing the prgm mem.
    //  I am now changing it so that it will make sure we are left
    //  with dwLeaveMem pages of storage mem, whether it is
    //  increasing or reducing prgm mem.
    //  if (dwFreeStorePages-dwLeaveMem >0)
    //      bRetVal = Hlp_BumpProgramMem(dwFreeStorePages-dwLeaveMem);

    bRetVal = Hlp_BumpProgramMem(dwFreeStorePages-dwLeaveMem);

    Hlp_DisplayMemoryInfo();
    return bRetVal;
}


//+ ========================================================================
//  This sets the system memory division so that we are left with
//  dwNumPagesToLeave amount of Program memory.
//- ========================================================================
BOOL Hlp_EatAll_Prgm_MemEx(DWORD dwNumPagesToLeave)
{
    BOOL    fRetVal=FALSE;
    DWORD   dwFreePrgmPages = 0;

    dwFreePrgmPages = Hlp_GetFreePrgmPages();
    if (dwFreePrgmPages < dwNumPagesToLeave)
    {
        TRACE(TEXT(">> WARNING : Num of Free Prgm pages = %d. Requesting to set to %d\r\n"), dwFreePrgmPages, dwNumPagesToLeave);
    }

    dwFreePrgmPages -= dwNumPagesToLeave;
    if (!Hlp_ReduceProgramMem(dwFreePrgmPages))
        goto ErrorReturn;

    fRetVal=TRUE;
ErrorReturn :
    return fRetVal;
}


//+ ========================================================================
//+ ========================================================================
DWORD Hlp_GetPageSize(void)
{
    DWORD   dwRetVal=0;
    DWORD   dwStorePages=0;
    DWORD   dwRamPages=0;
    DWORD   dwPageSize=0;

    //  Get the page division information
    if (!GetSystemMemoryDivision(&dwStorePages, &dwRamPages, &dwPageSize))
        GENERIC_FAIL(GetSystemMemoryDivision);
    dwRetVal = dwPageSize;

ErrorReturn :
    return dwRetVal;
}


//+ =================================================================================
//- =================================================================================
void Hlp_DeviceReset(void)
{
    KernelIoControl(IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, NULL );
}


//+ ========================================================================
//- ========================================================================
BOOL Hlp_SetMemPercentage(DWORD dwStorePcent, DWORD dwRamPcent)
{
    BOOL    fRetVal=FALSE;
    DWORD   dwStorePages=0;
    DWORD   dwRamPages=0;
    DWORD   dwPageSize=0;
    float   dwNewStorePages=0;
    DWORD   dwResult=0;

    ASSERT(100 == dwStorePcent+ dwRamPcent);
    if (100 != dwStorePcent+ dwRamPcent)
    {
        TRACE(TEXT(">>>>> ERROR : dwStorePCent=%d + dwRamPcent=%d is not 100 ????\r\n"));
        GENERIC_FAIL(Hlp_SetMemPercentage);
    }

    //  Get the page division information
    if (!GetSystemMemoryDivision(&dwStorePages, &dwRamPages, &dwPageSize))
        GENERIC_FAIL(GetSystemMemoryDivision);


    dwNewStorePages = (float)(dwStorePages+dwRamPages)*((float)dwStorePcent/(float)100);

    TRACE(TEXT("Setting SetSystemMemoryDivision to %d storage pages\r\n"), (DWORD)dwNewStorePages);
    dwResult = SetSystemMemoryDivision((DWORD)dwNewStorePages);
    switch(dwResult)
    {
        case SYSMEM_FAILED:
            TRACE( _T( "SetSystemMemoryDivision: ERROR value out of range!\n" ) );
            goto ErrorReturn;
        break;

        case SYSMEM_MUSTREBOOT:
        case SYSMEM_REBOOTPENDING:
            TRACE( _T( "SetSystemMemoryDivision: ERROR reboot request!\r\n" ) );
            goto ErrorReturn;
        break;
    }


    fRetVal=TRUE;
ErrorReturn :
    return fRetVal;

}


#endif
