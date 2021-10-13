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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// MCShellDXFrame.cpp : Defines the entry point for the console application.

#include "stdafx.h"
#include "Helper.h"

HRESULT    Helper_hr = -1;

#ifndef _WIN32_WCE        // XP only
HRESULT Helper_SaveGraphFile(IGraphBuilder *pGraph, WCHAR *wszPath) 
{
    const WCHAR wszStreamName[] = L"ActiveMovieGraph"; 

    IStorage *pStorage = NULL;
    Helper_hr = StgCreateDocfile(
        wszPath,
        STGM_CREATE | STGM_TRANSACTED | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
        0, &pStorage);
    if(FAILED(Helper_hr)) 
    {
        return Helper_hr;
    }

    IStream *pStream;
    Helper_hr = pStorage->CreateStream(
        wszStreamName,
        STGM_WRITE | STGM_CREATE | STGM_SHARE_EXCLUSIVE,
        0, 0, &pStream);
    if (FAILED(Helper_hr)) 
    {
        pStorage->Release();    
        return Helper_hr;
    }

    IPersistStream *pPersist = NULL;
    pGraph->QueryInterface(IID_IPersistStream, (void**)&pPersist);
    Helper_hr = pPersist->Save(pStream, TRUE);
    pStream->Release();
    pPersist->Release();
    if (SUCCEEDED(Helper_hr)) 
    {
        Helper_hr = pStorage->Commit(STGC_DEFAULT);
    }
    pStorage->Release();
    return Helper_hr;
}
HRESULT Helper_LoadGraphFile(IGraphBuilder *pGraph, const WCHAR* wszName)
{
    IStorage *pStorage = 0;
    if (S_OK != StgIsStorageFile(wszName))
    {
        return E_FAIL;
    }
    Helper_hr = StgOpenStorage(wszName, 0, 
        STGM_TRANSACTED | STGM_READ | STGM_SHARE_DENY_WRITE, 
        0, 0, &pStorage);
    if (FAILED(Helper_hr))
    {
        return Helper_hr;
    }
    IPersistStream *pPersistStream = 0;
    Helper_hr = pGraph->QueryInterface(IID_IPersistStream,
        reinterpret_cast<void**>(&pPersistStream));
    if (SUCCEEDED(Helper_hr))
    {
        IStream *pStream = 0;
        Helper_hr = pStorage->OpenStream(L"ActiveMovieGraph", 0, 
            STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &pStream);
        if(SUCCEEDED(Helper_hr))
        {
            Helper_hr = pPersistStream->Load(pStream);
            pStream->Release();
        }
        pPersistStream->Release();
    }
    pStorage->Release();
    return Helper_hr;
}
#endif
HRESULT Helper_RegisterDllUnderCE(LPCTSTR tzDllName)
{
    HINSTANCE hInst = LoadLibrary(tzDllName);
    pFuncpfv pfv = NULL;

    if( hInst == NULL)
        return S_FALSE;

    pfv = (pFuncpfv)GetProcAddress(hInst, TEXT("DllRegisterServer"));
    if(pfv == NULL )
    {
        FreeLibrary(hInst);
        return E_FAIL;
    }
    (*pfv)();
    FreeLibrary(hInst);
    return S_OK;
}

HRESULT Helper_GetPinbyName(IBaseFilter *pFilter, LPCWSTR wsPinName, IPin **ppPin)
{
    IEnumPins  *pEnum = NULL;
    IPin       *pPin = NULL;
    PIN_INFO myPinInfo;

    if (ppPin == NULL)    {    return E_POINTER;    }
    Helper_hr = pFilter->EnumPins(&pEnum);
    if (FAILED(Helper_hr))    {    return Helper_hr;    }
    while(pEnum->Next(1, &pPin, 0) == S_OK)
    {
        Helper_hr = pPin->QueryPinInfo(&myPinInfo);
        if (FAILED(Helper_hr))    {    pPin->Release(); pEnum->Release();    return Helper_hr;    }
        if (wcsstr(myPinInfo.achName, wsPinName) != NULL)
        {
            // Found a match. Return the IPin pointer to the caller.
            *ppPin = pPin;
            pEnum->Release();
            return S_OK;
        }
        // Release the pin for the next time through the loop.
        pPin->Release();
    }
    // No more pins. We did not find a match.
    pEnum->Release();
    return E_FAIL;  
}
HRESULT Helper_DisplayPinInfo(IPin *pPin)
{
    PIN_INFO myPinInfo;
    FILTER_INFO    myFilterInfo;
    IEnumMediaTypes * pEnumMediaTypes = NULL;
    ULONG uFetched = 0;
    ULONG uMediaType = 0;
    AM_MEDIA_TYPE * myAmType  = NULL;
    TCHAR tszGuid[1024] = {0};

    Helper_hr = pPin->QueryPinInfo(&myPinInfo);
    //_tprintf(TEXT("\tThe pin name is %s:\n"), myPinInfo.achName);
    //_tprintf(TEXT("\tThe pin direction is %d:\n"), myPinInfo.dir);
    myPinInfo.pFilter->QueryFilterInfo(&myFilterInfo);
    //_tprintf(TEXT("\tThe pin is at Filter %s:\n"), myFilterInfo.achName);
    pPin->EnumMediaTypes(&pEnumMediaTypes);
    while(!FAILED((pEnumMediaTypes->Next(1, &myAmType, NULL))))
    {
        StringFromGUID2(myAmType->majortype, tszGuid, 1024);
        //_tprintf(TEXT("\t Supported Media types on this pin is %s\n"), tszGuid);
    }
    if (FAILED(Helper_hr))        return Helper_hr;
    return S_OK;  
}

HRESULT    Helper_EnumPinsofFilter(IBaseFilter *pFilter, LPCWSTR wsDescription)
{
    IEnumPins  *pEnum = NULL;
    IPin       *pPin = NULL;
    PIN_INFO myPinInfo;
    int iCounter = 0;

    //_tprintf(TEXT("\n\n ============ > The Pins of filter %s are < ============= \n"), wsDescription);
    Helper_hr = pFilter->EnumPins(&pEnum);
    if (FAILED(Helper_hr))
    {
        return Helper_hr;
    }
    while(pEnum->Next(1, &pPin, 0) == S_OK)
    {
        Helper_hr = pPin->QueryPinInfo(&myPinInfo);
        if (FAILED(Helper_hr))
        {
            pPin->Release();
            pEnum->Release();
            return Helper_hr;
        }
        //_tprintf(TEXT("Got %dth Pin as : %s, Direction %d\n"), iCounter++, myPinInfo.achName, myPinInfo.dir);
        // Release the pin for the next time through the loop.
        pPin->Release();
    }
    // No more pins. We did not find a match.
    pEnum->Release();
    return S_OK;
}

HRESULT    Helper_DisplayFilter(IBaseFilter * pFilter)
{
    FILTER_INFO FilterInfo;
    IEnumPins *pEnumPins;
    IPin *pPin;

    //_tprintf(TEXT("\n -- Filter info : \n"));

    if(FAILED(pFilter->QueryFilterInfo(&FilterInfo)))
    {
        //_tprintf(TEXT("failed pFilter->QueryFilterInfo\n"));
    }
    //_tprintf(TEXT("\t Filter    : %s \t< pointer = 0x%x>\n"), FilterInfo.achName, (DWORD) pFilter);
    // Now display all Pins info on this filter
    if(FAILED(pFilter->EnumPins(&pEnumPins)))
    {
        //_tprintf(TEXT("failed pFilter->EnumPins\n"));
    }
    while(pEnumPins->Next(1, &pPin, NULL) == S_OK)
    {
        // IEnumMediaTypes *pEnumMediaType;
        PIN_INFO myPinInfo;
        pPin->QueryPinInfo(&myPinInfo);
        //_tprintf(TEXT("The Pin <%s> support AM_MEDIA_TYPE : \n"), myPinInfo.achName);
/*        _CK(pPin->EnumMediaTypes(&pEnumMediaType), "Get Pin Media Type Info");
        AM_MEDIA_TYPE *pType;
        while(pEnumMediaType->Next(1, &pType, NULL) == S_OK )
        {
            // DisplayType(TEXT("       -> "), pType);
        }
*/
    }
    // The FILTER_INFO structure holds a pointer to the Filter Graph
    // Manager, with a reference count that must be released.
    if (FilterInfo.pGraph != NULL)
    {
        FilterInfo.pGraph->Release();
    }
    return S_OK;
};


HRESULT    Helper_DislplayFilterOfGraph(IGraphBuilder * pGraph)
{
    IEnumFilters *pEnum = NULL;
    IBaseFilter *pFilter;
    ULONG cFetched;
    int iCounter = 0;
    FILTER_INFO FilterInfo;

    //_tprintf(TEXT("\n========= >  Graph Elements < =========== \n"));
    HRESULT hr = pGraph->EnumFilters(&pEnum);
    if (FAILED(hr)) return hr;

    while(pEnum->Next(1, &pFilter, &cFetched) == S_OK)
    {
        hr = pFilter->QueryFilterInfo(&FilterInfo);
        if (FAILED(hr))
        {
            //_tprintf(TEXT("Could not get the filter info\n"));
            continue;  // Maybe the next one will work.
        }
        //_tprintf(TEXT("%dth Filter : %s \t< pointer = 0x%x>\n"), iCounter++, FilterInfo.achName, (DWORD) pFilter);
        // The FILTER_INFO structure holds a pointer to the Filter Graph
        // Manager, with a reference count that must be released.
        if (FilterInfo.pGraph != NULL)
        {
            FilterInfo.pGraph->Release();
        }
        pFilter->Release();
    }
    pEnum->Release();
    return S_OK;
}
HRESULT Helper_GetFilterFromGraphByName(IGraphBuilder * pGraph, IBaseFilter **ppOutFilter, LPCWSTR wsFilterName)
{
    IEnumFilters *pEnum = NULL;
    IBaseFilter *pFilter;
    ULONG cFetched;
    int iCounter = 0;
    FILTER_INFO FilterInfo;

//    *ppOutFilter = NULL;
    HRESULT hr = pGraph->EnumFilters(&pEnum);
    if (FAILED(hr)) return hr;

    while(pEnum->Next(1, &pFilter, &cFetched) == S_OK)
    {
        hr = pFilter->QueryFilterInfo(&FilterInfo);
        if (FAILED(hr))
        {
            //_tprintf(TEXT("Could not get the filter info\n"));
            continue;  // Maybe the next one will work.
        }
        if(wcsstr(FilterInfo.achName, wsFilterName) != NULL)
        {
            //_tprintf(TEXT("Helper_GetFilterFromGraphByName Find the Filter!\n"));
            if(ppOutFilter != NULL)        *ppOutFilter = pFilter;
            if(FilterInfo.pGraph)
                FilterInfo.pGraph->Release();
//            pFilter->Release();
            pEnum->Release();
            return S_OK;
        }
        // The FILTER_INFO structure holds a pointer to the Filter Graph
        // Manager, with a reference count that must be released.
        if (FilterInfo.pGraph != NULL)
        {
            FilterInfo.pGraph->Release();
        }
        pFilter->Release();
    }
    pEnum->Release();
    //_tprintf(TEXT("Helper_GetFilterFromGraphByName does NOT Find the Filter!\n"));
    return E_FAIL;
}
struct myStruct
{
    VIDEOINFOHEADER vid;
    BITMAPINFOHEADER bih;
};

HRESULT Helper_SaveMediaTypeToFile(VIDEOINFOHEADER * vid, LPCTSTR lpFileName)
{
    myStruct T;
    DWORD iNum = 0;
    memcpy(&(T.vid), vid, sizeof(VIDEOINFOHEADER));
    memcpy(&(T.bih), &(vid->bmiHeader), sizeof(BITMAPINFOHEADER));
    DeleteFile(lpFileName);
    HANDLE fHandle = CreateFile(lpFileName, GENERIC_WRITE, NULL, NULL, 
        CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
    WriteFile(fHandle, &T, sizeof(myStruct), &iNum, NULL);
    return S_OK;
}
HRESULT Helper_FindInterfaceFromGraph(IGraphBuilder * pIGraphBuilder, const IID & riid, void ** ppvObject)
{
    IEnumFilters * pIEnumFilters;
    IBaseFilter * pIBaseFilter;
    ULONG cFetched;
    HRESULT hr = pIGraphBuilder->EnumFilters(&pIEnumFilters);

    if(SUCCEEDED(hr))
    {
        hr = E_FAIL;

        while(pIEnumFilters->Next(1, &pIBaseFilter, &cFetched) == S_OK)
        {
            hr = pIBaseFilter->QueryInterface(riid, ppvObject);

            pIBaseFilter->Release();

            if(SUCCEEDED(hr))
            {
                break;
            }
        }

        pIEnumFilters->Release();
    }
    return hr;
}
DWORD Helper_DvrRegQueryValueGenericCb(LPCTSTR pcszKey, LPBYTE pbBuffer, DWORD cbBuffer, DWORD* pdwType)
{
    DWORD dwError;
    HKEY  hKey = NULL;
    DWORD dwDisposition = 0;
    dwError = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                                MC_GUIDEAPP_REGPATH,
                                0,
                                0,
                                0,
                                KEY_READ,
                                0,
                                &hKey,
                                &dwDisposition);
    if (dwError == ERROR_SUCCESS)
    {
        dwError = RegQueryValueEx(hKey,
                        pcszKey,
                        0,
                        pdwType,
                        pbBuffer,
                        &cbBuffer);
        RegCloseKey(hKey);
    }
    return dwError;
}

DWORD Helper_DvrRegQueryValueDWORD(LPCTSTR pcszKey, LPDWORD pDWord)
{
    DWORD dwError;
    DWORD dwType;

    dwError = Helper_DvrRegQueryValueGenericCb(pcszKey, (LPBYTE)pDWord, sizeof(*pDWord), &dwType);
    if (dwError != ERROR_SUCCESS || dwType != REG_DWORD)
    {
        *pDWord = 0;
    }

    return dwError;
}
DWORD Helper_DvrRegSetValueGenericCb(LPCTSTR pcszKey, LPBYTE pbBuffer, DWORD cbBuffer, DWORD dwType)
{
    DWORD dwError;
    HKEY  hKey = NULL;
    DWORD dwDisposition = 0;
    dwError = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                                MC_GUIDEAPP_REGPATH,
                                0,
                                0,
                                0,
                                KEY_ALL_ACCESS,
                                0,
                                &hKey,
                                &dwDisposition);
    if (dwError == ERROR_SUCCESS)
    {
        dwError = RegSetValueEx(hKey,
                        pcszKey,
                        0,
                        dwType,
                        pbBuffer,
                        cbBuffer);
        RegCloseKey(hKey);
    }
    return dwError;
}

DWORD Helper_DvrSetAudioType(GUID AudioType)
{
    DWORD dwError;
    HKEY  hKey = NULL;
    DWORD dwDisposition = 0;
    DWORD cbBuffer;
    LPOLESTR lpolestr = NULL;

    // convert the guid to an olestr (wchar
    StringFromCLSID(AudioType, &lpolestr);

    dwError = RegCreateKeyEx(HKEY_CLASSES_ROOT,
                                DVRSPLT_DEFAUDIOTYPE
                                0,
                                0,
                                0,
                                KEY_ALL_ACCESS,
                                0,
                                &hKey,
                                &dwDisposition);
    if (dwError == ERROR_SUCCESS)
    {
        // the size of the buffer, including the terminating NULL
        // is the length + the null * the size of a wchar/olechar
        cbBuffer = (wcslen(lpolestr) + 1) * sizeof(OLECHAR);

        dwError = RegSetValueEx(hKey,
                       TEXT("DefaultSubtype"),
                        0,
                        REG_SZ,
                        (BYTE *) lpolestr,
                        cbBuffer);
        RegCloseKey(hKey);
    }

    CoTaskMemFree(lpolestr);
    return dwError;
}

DWORD Helper_DvrRegSetValueDWORD(LPCTSTR pcszKey, LPDWORD pDWord)
{
    DWORD dwError;

    dwError = Helper_DvrRegSetValueGenericCb(pcszKey, (LPBYTE)pDWord, sizeof(*pDWord), REG_DWORD);
    return dwError;
}

BOOL Helper_CleanupRecordPath(LPCTSTR tszLocation)
{
    BOOL bSucceeded = TRUE;

    TCHAR PathChars[] = TEXT("0123456789ABCDEF");
    TCHAR NewPath[MAX_PATH];

    for(int LevelOne = 0; LevelOne < (sizeof(PathChars)/sizeof(*(PathChars))); LevelOne++)
    {
        for(int LevelTwo = 0; LevelTwo < (sizeof(PathChars)/sizeof(*(PathChars))); LevelTwo++)
        {
            memset(NewPath, 0, sizeof(NewPath));

            wsprintf(NewPath, TEXT("%s%c\\%c\\"), tszLocation, PathChars[LevelOne], PathChars[LevelTwo]);

            bSucceeded &= Helper_CleanupSingleRecordPath(NewPath);

            RemoveDirectory(NewPath);    
        }

        wsprintf(NewPath, TEXT("%s%c\\"), tszLocation, PathChars[LevelOne]);
        RemoveDirectory(NewPath);
    }

    return bSucceeded;
}

BOOL Helper_CleanupSingleRecordPath(LPCTSTR tszLocation)
{
    HANDLE hSearch;
    BOOL bFound;
    TCHAR tszPath[MAX_PATH];
    WIN32_FIND_DATA FileData;
    TCHAR *tszFileNames[] = { TEXT("*.dvr-dat"), TEXT("*.dvr-idx") };
    int FileNameCount = 2;
    BOOL bSucceeded = TRUE;

    if (!tszLocation)
        return FALSE;

    for(int i = 0; i < FileNameCount; i++)
    {
        wsprintf(tszPath, TEXT("%s%s"), tszLocation, tszFileNames[i]);
        
        hSearch = FindFirstFile( tszPath, &FileData );

        // 
        // If no files match the search string, skip it.
        //
        if( INVALID_HANDLE_VALUE != hSearch )
        {
            do 
            {
                wsprintf(tszPath, TEXT("%s%s"), tszLocation, FileData.cFileName);

                if(!DeleteFile(tszPath))
                {
                    bSucceeded = FALSE;
                }
                   
                bFound = FindNextFile( hSearch, &FileData );
                   
            } while(bFound) ;
         
            FindClose( hSearch );
        }
    }

    return bSucceeded;
}

void
Helper_CreateRandomGuid(GUID* pguid)
{

    // rand only returns between 0 and 0x7fff, so to get a better guid,
    // I need to do some shifting and oring.
    pguid->Data1 = rand() | (rand() << 15) | (rand() << 30);
    pguid->Data2 = (WORD) (rand() | (rand() << 15));
    pguid->Data3 = (WORD) (rand() | (rand() << 15));
    pguid->Data4[0] = (BYTE) rand();
    pguid->Data4[1] = (BYTE) rand();
    pguid->Data4[2] = (BYTE) rand();
    pguid->Data4[3] = (BYTE) rand();
    pguid->Data4[4] = (BYTE) rand();
    pguid->Data4[5] = (BYTE) rand();
    pguid->Data4[6] = (BYTE) rand();
    pguid->Data4[7] = (BYTE) rand();
}
