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
//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  File:       N C B A S E . C P P
//
//  Contents:   Basic common code.
//
//  Notes:      Pollute this under penalty of death.
//
//----------------------------------------------------------------------------

#include <pch.h>
#pragma hdrstop
#include "ncbase.h"
#include "ncdebug.h"

//+---------------------------------------------------------------------------
//
//  Function:   AddRefObj
//
//  Purpose:    AddRef's the object pointed to by punk by calling
//              punk->AddRef();
//
//  Arguments:
//      punk [in]   Object to be AddRef'd. Can be NULL.
//
//  Returns:    Result of AddRef call.
//
//  Notes:      Using this function to AddRef an object will reduce
//              our code size.
//
ULONG
AddRefObj (
    IUnknown* punk) NOTHROW
{
    return (punk) ? punk->AddRef () : 0;
}

//+---------------------------------------------------------------------------
//
//  Function:   ReleaseObj
//
//  Purpose:    Releases the object pointed to by punk by calling
//              punk->Release();
//
//  Arguments:
//      punk [in]   Object to be released. Can be NULL.
//
//  Returns:    Result of Release call.
//
//  Notes:      Using this function to release a (possibly NULL) object will
//              reduce our code size.
//
ULONG
ReleaseObj (
    IUnknown* punk) NOTHROW
{
    return (punk) ? punk->Release () : 0;
}

//+--------------------------------------------------------------------------
//
//  Function:   DwWin32ErrorFromHr
//
//  Purpose:    Converts the HRESULT to a Win32 error or SetupApi error.
//
//  Arguments:
//      hr [in] The HRESULT to convert
//
//  Returns:    Converted DWORD value.
//
//  Notes:
//
DWORD
DwWin32ErrorFromHr (
    HRESULT hr) NOTHROW
{
    DWORD dw = ERROR_SUCCESS;

    // All success codes convert to ERROR_SUCCESS so we only need to handle
    // failures.
    if (FAILED(hr))
    {
        DWORD dwFacility = HRESULT_FACILITY(hr);

        if (FACILITY_WIN32 == dwFacility)
        {
            dw = HRESULT_CODE(hr);
        }
        else if (FACILITY_ITF == dwFacility)
        {
            dw = ERROR_GEN_FAILURE;
        }
        else
        {
            // cannot convert it
            AssertSz(FALSE, "Facility was not SETUP or WIN32!");
            dw = hr;
        }
    }

    return dw;
}

//+---------------------------------------------------------------------------
//
//  Function:   HrFromLastWin32Error
//
//  Purpose:    Converts the GetLastError() Win32 call into a proper HRESULT.
//
//  Arguments:
//      (none)
//
//  Returns:    Converted HRESULT value.
//
//  Notes:      This is not inline as it actually generates quite a bit of
//              code.
//              If GetLastError returns an error that looks like a SetupApi
//              error, this function will convert the error to an HRESULT
//              with FACILITY_SETUP instead of FACILITY_WIN32
//
HRESULT
HrFromLastWin32Error () NOTHROW
{
    DWORD dwError = GetLastError();
    HRESULT hr;

    // This test is testing SetupApi errors only (this is
    // temporary because the new HRESULT_FROM_SETUPAPI macro will
    // do the entire conversion)
    if (dwError & (APPLICATION_ERROR_MASK | ERROR_SEVERITY_ERROR))
    {
        hr = HRESULT_FROM_SETUPAPI(dwError);
    }
    else
    {
        hr = HRESULT_FROM_WIN32(dwError);
    }
    return hr;
}

//+---------------------------------------------------------------------------
//
//  Function:   HrGetProcAddress
//
//  Purpose:    Loads a libray and returns the address of a procedure within
//                  the library
//
//  Arguments:
//      hModule      [in] The handle to the library module instance
//      pszaFunction [in]  Function to retrieve
//      ppfn         [out] Address of szFunction
//
//  Returns:    S_OK if successful, Win32 converted error if failure.
//
//  Notes:
//
HRESULT
HrGetProcAddress (
    HMODULE     hModule,
    PCSTR       pszaFunction,
    FARPROC*    ppfn)
{
    Assert(hModule);
    Assert(pszaFunction);
    Assert(ppfn);

    HRESULT hr = S_OK;
#ifdef UNDER_CE    
    WCHAR szwFunction[64];
    int cch = strlen(pszaFunction)+1;
    if (cch > sizeof(szwFunction)/sizeof(WCHAR))
        return DISP_E_BUFFERTOOSMALL;
    mbstowcs(szwFunction, pszaFunction,cch );
    *ppfn = GetProcAddressW(hModule, szwFunction);
#else    
    *ppfn = GetProcAddress(hModule, pszaFunction);
#endif    
    if (!*ppfn)
    {
        hr = HrFromLastWin32Error();
        TraceTag(ttidError, "HrGetProcAddress failed: szFunction: %s",
                 pszaFunction);
    }

    TraceError("HrGetProcAddress", hr);
    return hr;
}

//+---------------------------------------------------------------------------
//
//  Function:   HrLoadLibAndGetProcs
//
//  Purpose:    Load a dynamic link library and the addresses of one or
//              more procedures within that library.
//
//  Arguments:
//      pszLibPath         [in]  Path to the DLL to load.
//      cFunctions         [in]  Number of procedures to load.
//      apszaFunctionNames [in]  Array of function names.  (Must be 'cFunctions'
//                               of them.)
//      phmod              [out] Returned handle to the loaded module.
//      apfn               [out] Array of returned pointers to the procedures
//                               loaded.  (Must be 'cFunctions' of them.)
//
//  Returns:    S_OK if all procedures were loaded, S_FALSE if only
//              some of them were, or a Win32 error code.  If only
//              one procedure is to be loaded and it is not, S_FALSE will
//              not be returned, rather, the reason for why the single
//              procedure could not be loaded will be returned.  This allows
//              HrLoadLibAndGetProc to be implemented using this function.
//
//  Notes:      phmod should be freed by the caller using FreeLibrary if
//              the return value is S_OK.
//
HRESULT
HrLoadLibAndGetProcs (
    PCTSTR          pszLibPath,
    UINT            cFunctions,
    const PCSTR*    apszaFunctionNames,
    HMODULE*        phmod,
    FARPROC*        apfn)
{
    Assert (pszLibPath);
    Assert (cFunctions);
    Assert (apszaFunctionNames);
    Assert (phmod);
    Assert (apfn);

    HRESULT hr = S_OK;

    // Load the module and initialize the output parameters.
    //
    HMODULE hmod = LoadLibrary(pszLibPath);
    *phmod = hmod;
    ZeroMemory (apfn, cFunctions * sizeof(FARPROC));

    if (hmod)
    {
        // Get the proc address of each function.
        //
        for (UINT i = 0; i < cFunctions; i++)
        {
            apfn[i] = NULL;
            hr = HrGetProcAddress (hmod, apszaFunctionNames[i], &apfn[i]);

            if (!apfn[i])
            {
                // Couldn't load all functions.  We'll be returning S_FALSE
                // (if their are more than one function.)
                //
                hr = S_FALSE;

                TraceTag (ttidError, "HrLoadLibAndGetProcs: GetProcAddress "
                    "for '%s' failed.",
                    apszaFunctionNames[i]);
            }
        }

        // If we're only loading one function, and it failed,
        // return the failure.
        //
        if ((1 == cFunctions) && !apfn[0])
        {
            hr = HrFromLastWin32Error ();
            FreeLibrary (hmod);
        }
    }
    else
    {
        hr = HrFromLastWin32Error ();
        TraceTag (ttidError, "HrLoadLibAndGetProcs: LoadLibraryW (%S) failed.",
            pszLibPath);
    }

    TraceError ("HrLoadLibAndGetProcs", hr);
    return hr;
}

//+---------------------------------------------------------------------------
//
//  Function:   FFileExists
//
//  Purpose:    Check for file existance. Returns TRUE if file is present,
//              FALSE otherwise (duh). 
//
//  Arguments:  
//      pszFileName [in]  File name to check for.
//
//  Returns:    
//
//  Notes:      
//
BOOL FFileExists(LPTSTR pszFileName)
{
    BOOL    fReturn = TRUE;
    HANDLE  hFile   = NULL;

    hFile = CreateFile(
            pszFileName,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD dwLastError = GetLastError();

        if (dwLastError != ERROR_FILE_NOT_FOUND)
        {
            AssertSz(FALSE, "FFileExists failed for some reason other than FILE_NOT_FOUND");
        }

        fReturn = FALSE;
        goto Exit;
    }

Exit:
    if (hFile)
    {
        CloseHandle(hFile);
        hFile = NULL;
    }

    return fReturn;
}

//+---------------------------------------------------------------------------
//
//  Function:   GenerateUUID64
//
//  Purpose:    Generate a 64-bit UUID. 64-bit UUIDs are easier to deal with than the traditional
//              128-bit GUIDS and seem to offer adequate uniqueness. Odds of collision in million member universe < 1 in 4 million.
//              The version number overlay bits are set to 0001b.
//
//  Arguments:  
//      none
//
//  Returns: 
//          64-bit uid. This does not need to meet the randomness requirements of a true random number generator, since its okay if 
//          successive uuids are correlated.
//
//  Notes:      
//

LONGLONG GenerateUUID64(void)
{
    LARGE_INTEGER perfTime;
    QueryPerformanceCounter(&perfTime);

    static long count;    // ensures random value even if systime and perfcount are the same
    InterlockedIncrement(&count);

    LONGLONG lluuid = perfTime.QuadPart;
    LONGLONG llcount = count;
    lluuid = (lluuid << 32) | llcount;

    // Set the DCE version bits - Version 1 goes into upper nibble, byte 6.
    WORD *pwBase = (WORD*)&lluuid;    // Base of the UUID 8 bytes
    WORD *pwVers = pwBase + 3;      // Location of the last 2 bytes
    *pwVers &= ~0xf000;             // remove the upper four bits
    *pwVers |=  0x1000;             // set the value there to be 1.

    return lluuid;
}
    

