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
#include <coredll.h>
#include <twinuser.h>
#include <ehm.h>
#include "ResourceModuleCache.hpp"

static ResourceModuleCache g_ResourceModuleCache;
#define DBGRSRC     DBGSHELL

/// <summary>
/// Find resource in a given library name 
/// </summary>
/// <param name="pwszModuleName">The module name.</param>
/// <param name="pwszResourceName">The resource name </param>
/// <param name="dwResourceType">The resource type </param>
/// <param name="phModule">The pointer of this module if the resource is found</param>
/// <returns>Returns the resource handle if the resource is found, or NULL if not.</returns>
extern "C"
static 
HRSRC
FindResourceInLibrary(
    LPCWSTR pwszModuleName,
    LPCWSTR pwszResourceName,
    LPCWSTR pwszResourceType,
    HMODULE *phModule
)
{
    HRESULT hr = S_OK;
    HRSRC hRSRC = NULL;
    HMODULE hModule = NULL;
    
    ASSERT(NULL != phModule);

    // Only the result for the last fall back should be kept.
    SetLastError(NO_ERROR);

    hModule = LoadLibraryEx(pwszModuleName, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CBR(NULL != hModule);    

    hRSRC = FindResource(hModule, pwszResourceName, pwszResourceType);
    CBR(NULL != hRSRC);

    *phModule = hModule;
     
Error:
    if (NULL == hRSRC && NULL != hModule)
    {
        // If the resource is not found, free the library
        FreeLibrary(hModule);
    }
    
    return hRSRC;
}

/// <summary>
/// The implementation of FindResourceForDisplayConfiguration
/// </summary>
/// <param name="pwszModuleBase">The base module name.</param>
/// <param name="phModule">the handle of the module in which the resource is found</param>
/// <param name="pwszName">The resource name </param>
/// <param name="pwszType">The resource type </param>
/// <param name="dwDisplayConfigurationID">The display configuration ID fetched from OpenDisplayConfiguration</param>
/// <returns>Returns the resource handle if the resource is found, or NULL if not.</returns>
extern "C" 
HRSRC 
WINAPI 
FindResourceForDisplayConfiguration_I(
    __in_z LPCWSTR pwszModuleBase, 
    __out HMODULE *phModule, 
    __in_z LPCWSTR pwszName, 
    __in_z LPCWSTR pwszType, 
    DWORD dwDisplayConfigurationID
)
{
    HRESULT hr = S_OK;
    HRSRC hRSRC = NULL;
    LPWSTR *prgwszFallbackModuleNames = NULL;
    DWORD cszModuleNames = 0;
    HMODULE hModule = NULL;
    HRSRC hRSRCTemp = NULL;
    BOOL bRet = FALSE;
    WCHAR wszActualType[MAX_PATH];
    LPCWSTR pwszModuleName;
    DWORD dwFallbackStepID;
   
    CBREx(
        NULL != pwszModuleBase &&
        NULL != phModule &&
        NULL != pwszName &&
        NULL != pwszType, 
        E_INVALIDARG
        );
    
    // If the resource is actually a number, transfer it into a real string
    if (0 == ((DWORD)pwszType) >> 16)
    {
        StringCchPrintf(wszActualType, MAX_PATH, L"#%d", (DWORD)pwszType);
    }
    // else, just use the pwszType
    else
    {
        StringCchCopy(wszActualType, MAX_PATH, pwszType);
    }
    
    // Get the fallback module names from gwes
    bRet = GetFallbackModuleNamesForDisplayConfiguration(
        pwszModuleBase, 
        &prgwszFallbackModuleNames, 
        &cszModuleNames, 
        wszActualType, 
        dwDisplayConfigurationID
        );
    
    CBR(bRet);
    ASSERT(NULL != prgwszFallbackModuleNames);
    ASSERT(0 < cszModuleNames);

#ifdef DEBUG
    if (((DWORD)pwszName) >> 16)
    {
        DEBUGMSG(DBGRSRC, (L"FindResourceForDisplayConfiguration looking for named resource '%s', type '%s' in base module %s, %d fallback steps\r\n", pwszName, wszActualType, pwszModuleBase, cszModuleNames));
    }
    else
    {
        DEBUGMSG(DBGRSRC, (L"FindResourceForDisplayConfiguration looking for numeric resource %d, type '%s' in base module %s, %d fallback steps\r\n", (DWORD)pwszName, wszActualType, pwszModuleBase, cszModuleNames));
    }
#endif // DEBUG
    if (g_ResourceModuleCache.Find(pwszModuleBase, pwszName, wszActualType, &dwFallbackStepID))
    {
        ASSERT(dwFallbackStepID >=0);
        if (dwFallbackStepID < cszModuleNames)
        {
            pwszModuleName = prgwszFallbackModuleNames[dwFallbackStepID];
        }
        else
        {
            ASSERT(dwFallbackStepID == cszModuleNames);
            pwszModuleName = pwszModuleBase;
        }
        hRSRCTemp = FindResourceInLibrary(pwszModuleName, pwszName, pwszType, &hModule);
        ASSERT(NULL != hRSRCTemp);
        DEBUGMSG(DBGRSRC, (L"FindResourceForDisplayConfiguration looking in module %s\r\n", pwszModuleName));
    }
    else
    {
        // Find the resource in the fallback module list
        for (DWORD i = 0; i < cszModuleNames; i++)
        {  
            DEBUGMSG(DBGRSRC, (L"FindResourceForDisplayConfiguration looking in module %s\r\n", prgwszFallbackModuleNames[i]));
            hRSRCTemp = FindResourceInLibrary(prgwszFallbackModuleNames[i], pwszName, pwszType, &hModule);
            if (NULL != hRSRCTemp)
            {
                // The resource is found in current satellite module
                DEBUGMSG(DBGRSRC, (L"FindResourceForDisplayConfiguration found resource!\r\n"));
                g_ResourceModuleCache.AddToCache(pwszModuleBase, pwszName, wszActualType, i);
                break;
            }      
        }
        
        if (NULL == hRSRCTemp)
        {
            // The resource is not in the satellite modules, try the base module
            DEBUGMSG(DBGRSRC, (L"FindResourceForDisplayConfiguration looking in base module\r\n"));
            hRSRCTemp = FindResourceInLibrary(pwszModuleBase, pwszName, pwszType, &hModule);       
            if (NULL != hRSRCTemp)
            {
                g_ResourceModuleCache.AddToCache(pwszModuleBase, pwszName, wszActualType, cszModuleNames);
            }   
        }
    }

    // If the resource is not found in both satellite modules and base module, just return the error of LoadLibraryEx/FindResource
    CBR(NULL != hRSRCTemp);     

    // Set the return values if the resource is found successfully
    hRSRC = hRSRCTemp;
    *phModule = hModule;

Error:    
    if (NULL != prgwszFallbackModuleNames)
    {
        // Free the resource which allocated by the GetFallbackModuleNamesForDisplayConfiguration.
        for (DWORD i = 0; i < cszModuleNames; i++)
        {
            // Free every strings in the fallback name list
            if (NULL != prgwszFallbackModuleNames[i])
            {
                LocalFree((HLOCAL)prgwszFallbackModuleNames[i]);
            }
        }
        // Free the name list array
        LocalFree((HLOCAL)prgwszFallbackModuleNames);
    }
    
    switch (hr)
    {
        case E_INVALIDARG:
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            break;
        }
        default:
        {
            DEBUGMSG(DBGRSRC, (L"FindResourceForDisplayConfiguration couldn't find resource!\r\n"));
            break;
        }            
    }
    return hRSRC;
}

