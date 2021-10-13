//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
// 
// File:    thdcache.cpp
// 
// Contents:
//
//  Implements per thread object cache
//
//
//
//
//-----------------------------------------------------------------------------
#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif 
#include "isapihdr.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: ThdCacheOKForAccess(void *pvData)
//
//  parameters:
//          
//  description:
//     Checks file access permissions and staleness     
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
OBJ_STATE ThdCacheOKForAccess(void *pvData)         
{
    CThdCachedObj *pThdCahcedObj;

    pThdCahcedObj = (CThdCachedObj *)pvData;
    
    return pThdCahcedObj->ChkOKForAccess();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: ThdCacheGetKey(void *pvData)
//
//  parameters:
//          
//  description:
//     Returns the hash key for the cached object     
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
char * ThdCacheGetKey (void *pvData)
{
    CThdCachedObj *pThdCahcedObj;

    pThdCahcedObj = (CThdCachedObj *)pvData;
    
    return pThdCahcedObj->getKey();

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: ThdCacheDelete(void * pvData)
//
//  parameters:
//          
//  description:
//      Deletes the cached object        
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
void ThdCacheDelete(void * pvData)
{
    CThdCachedObj *pThdCahcedObj;

    pThdCahcedObj = (CThdCachedObj *)pvData;    
    delete pThdCahcedObj;
    return;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CThdCachedObj::CThdCachedObj()
//
//  parameters:
//          
//  description:
//          CThdCachedObj constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CThdCachedObj::CThdCachedObj() : m_pszWSDLFilePath(NULL), m_pszWSMLFilePath(NULL)
{
    m_WSDLLastWrite.dwLowDateTime = 0;
    m_WSDLLastWrite.dwHighDateTime = 0;
    m_WSMLLastWrite.dwLowDateTime = 0;
    m_WSMLLastWrite.dwHighDateTime = 0;        
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CThdCachedObj::~CThdCachedObj()
//
//  parameters:
//          
//  description:
//          CThdCachedObj destructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CThdCachedObj::~CThdCachedObj()
{
    // We want to check for validity before releasing ISoapServer pointer 
    // Since we may get an AV during thread pool shutdown if mssoap.dll is  
    // already released.
    IUnknown   *punk;
    punk = m_pISoapServer.PvReturn();
    if (punk)
        SafeRelease(punk, sizeof(punk));
    
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CThdCachedObj::getKey()
//
//  parameters:
//          
//  description:
//          Returns hash key for the object
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
char * CThdCachedObj::getKey()
{

    return m_pszWSDLFilePath;    
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CThdCachedObj::Init(char * pszWSDL, char * pszWSML, ISOAPServer *pISOAPServer)
//
//  parameters:
//          
//  description:
//          Initializes the cached object 
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CThdCachedObj::Init(char * pszWSDL, char * pszWSML,
            ISOAPServer *pISOAPServer)
{
    HRESULT     hr;
    HANDLE      hWSDL = INVALID_HANDLE_VALUE;        
    HANDLE      hWSML = INVALID_HANDLE_VALUE;

    
    m_pszWSDLFilePath = new char[strlen(pszWSDL)+1];    
    m_pszWSMLFilePath = new char[strlen(pszWSML)+1];
    if ((m_pszWSMLFilePath == NULL) || (m_pszWSMLFilePath == NULL))
        return E_OUTOFMEMORY;
    strcpy(m_pszWSDLFilePath, pszWSDL);
    strcpy(m_pszWSMLFilePath, pszWSML);
    m_pISoapServer = pISOAPServer;

    // Get the timestamps of the files
    hr = OpenFiles(&hWSDL, &hWSML);
    if (hr == S_OK)
    {
        hr = GetFileLastWriteTime(hWSDL,&m_WSDLLastWrite);
        if (hr == S_OK)
        {
            hr = GetFileLastWriteTime(hWSML, &m_WSMLLastWrite);
        }    
    }    
    
    if (hWSDL != INVALID_HANDLE_VALUE)
        CloseHandle(hWSDL);
    if (hWSML != INVALID_HANDLE_VALUE)
        CloseHandle(hWSML);

    return hr;

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CThdCachedObj::ChkOKForAccess()
//
//  parameters:
//          
//  description:
//          Checks to see if current user has access permissions to this
//          object and that the object is not stale
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
OBJ_STATE CThdCachedObj::ChkOKForAccess()
{
    HRESULT     hr;
    HANDLE      hWSDL = INVALID_HANDLE_VALUE;        
    HANDLE      hWSML = INVALID_HANDLE_VALUE;
    OBJ_STATE   objstate;
    FILETIME WSDLLastWrite; 
    FILETIME WSMLLastWrite;

    // We open both WSDL and WSML files and check their timestamp here.
    //  We should be running under user priviledges right now so that
    //  if the user doesn't have access rights to these files, the open will fail.  

    hr = OpenFiles(&hWSDL, &hWSML);
    if (hr != S_OK)
    {
        switch(HRESULT_CODE(hr))
        {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
                objstate = OBJ_NOTFOUND;
                break;
            case ERROR_ACCESS_DENIED: 
                objstate = OBJ_ACCESS_DENIED;
                break;
            default:
                objstate = OBJ_ACCESS_DENIED;
                break;
        }
        CHK(hr);
    }
    objstate = OBJ_ACCESS_DENIED;
    CHK(GetFileLastWriteTime(hWSDL,&WSDLLastWrite));
    CHK(GetFileLastWriteTime(hWSML, &WSMLLastWrite));

    // Compare two timestamps
    if (memcmp((void *)&WSDLLastWrite, (void *)&m_WSDLLastWrite, sizeof(FILETIME)) ||
        memcmp((void *)&WSMLLastWrite, (void *)&m_WSMLLastWrite, sizeof(FILETIME)))
    {
        objstate = OBJ_STALE;
        goto Cleanup;
    };    
    // Everything is OK if we come here
    objstate = OBJ_OK;
    
Cleanup:

    if (hWSDL != INVALID_HANDLE_VALUE)
        CloseHandle(hWSDL);
    if (hWSML != INVALID_HANDLE_VALUE)
        CloseHandle(hWSML);    
    return objstate;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CThdCachedObj::OpenFiles(HANDLE * phWSDL, HANDLE *phWSML)
//
//  parameters:
//          
//  description:
//      Open the WSDL and WSML files. This must be run in user security
//      so that the appropriate user permissions will be checked
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CThdCachedObj::OpenFiles(HANDLE * phWSDL, HANDLE *phWSML)
{
    HRESULT     hr = S_OK;
    
    *phWSDL = CreateFileA(m_pszWSDLFilePath, GENERIC_READ,
            FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);            
    if (*phWSDL == INVALID_HANDLE_VALUE)
    {
        CHK(HRESULT_FROM_WIN32(GetLastError()));
    }    
    
    *phWSML = CreateFileA(m_pszWSMLFilePath, GENERIC_READ,
            FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);            
    if (*phWSML == INVALID_HANDLE_VALUE)
    {
        CHK(hr = HRESULT_FROM_WIN32(GetLastError()));
    }    
    
Cleanup:
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CThdCachedObj::GetFileLastWriteTime(HANDLE hfile, FILETIME *pFTLastWrite)
//
//  parameters:
//          
//  description:
//          Obtains the timestamp for the last write of the given file handle
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT CThdCachedObj::GetFileLastWriteTime(HANDLE hfile,
        FILETIME *pFTLastWrite)
{
    HRESULT     hr = S_OK;

    if (!GetFileTime(hfile, NULL, NULL, pFTLastWrite))
    {
        CHK(hr = HRESULT_FROM_WIN32(GetLastError()));
    }
                
Cleanup:
    return hr;
}

