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
// File:   request.cpp
// 
// Contents:
//
//  ISAPI Extension that services SOAP packages.
//
//-----------------------------------------------------------------------------
#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif

#include "isapihdr.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CRequest::Validate()
//
//  parameters:
//          
//  description:
//          Validates that the input message is a valid soap message
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT CRequest::Validate()
{
    // Anything that requires path/input validation will go here

    // Check against max POST size
    if(!stricmp(m_pECB->lpszMethod, "POST"))
    {
        if (m_pECB->cbTotalBytes > g_cbMaxPost)
        {
            m_pIStreamOut.SetErrorCode(Error_BadRequest);
            return E_ACCESSDENIED;
        }
    }
    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CRequest::ProcessParams()
//
//  parameters:
//          
//  description:
//           
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT CRequest::ProcessParams()
{

    return S_OK;

}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CRequest::GetHeaderParam(char *pstrHdr)
//
//  parameters:
//          
//  description:
//      Obtains the requested header value from the HTTP headers stream
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
BSTR CRequest::GetHeaderParam(char *pstrHdr)
{
    char *pstr;
    char tempbuf[ONEK + 1];
    char tempbuf2[sizeof(WCHAR) * ONEK + 2];
    WCHAR   *pwstr;
    DWORD   cbSize = sizeof(tempbuf);
    DWORD   cchLen;
    BSTR    bstrValue;

    cchLen = strlen(pstrHdr);
    if (cchLen > (ONEK - 5))
        return NULL;
    strcpy(tempbuf2, "HTTP_");
    strncat(tempbuf2, pstrHdr, ONEK);
    if (m_pECB->GetServerVariable(m_pECB->ConnID, tempbuf2, (char *)tempbuf, &cbSize))
    {
        if (cbSize > ONEK)
            cbSize = ONEK;
        tempbuf[cbSize] = 0;
        cchLen = strlen(tempbuf);
        pstr = tempbuf;
        // Remove the quotes
        if ((*pstr == '\'') || (*pstr == '"'))
        {
            pstr++;
            cchLen -=2;
            pstr[cchLen] = 0;
        }    
        // Convert it to WCHAR and put it in a BSTR
        pwstr = (WCHAR *)tempbuf2;
        cchLen = CwchMultiByteToWideChar(0, pstr, cchLen, pwstr, ONEK);
        pwstr[cchLen] = 0;
        bstrValue = ::SysAllocString(pwstr);
        return bstrValue;
    }        
    return (BSTR)NULL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CRequest::ProcessPathInfo()
//
//  parameters:
//          
//  description:
//      Get the vname, convert the vname to physical path name
//          for the WSDL and WSML files
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT CRequest::ProcessPathInfo()
{

    char *          pszPathInfo = m_pECB->lpszPathInfo; 
    char *          pszPathTranslated = m_pECB->lpszPathTranslated;
    
#ifndef UNDER_CE
    ULONG           cbFileLen;
    char            szApplPath[FOURK+1];
    DWORD           cchApplPath = FOURK;
    char *          psz;
    char            szApplMDPath[ONEK+1];
    DWORD           cchApplMDPath = ONEK;
    char            szURL[FOURK+1];
#else //in CE we have a 4k guard page, so moving these stack
      //allocations onto the heap
    char            *szApplPath = new char[FOURK+1];
    DWORD           cchApplPath = FOURK;
    char            *szApplMDPath = new char[ONEK+1];
    DWORD           cchApplMDPath = ONEK;
    HRESULT         hr = E_FAIL;
    
    CHK_MEM(szApplPath);
    CHK_MEM(szApplMDPath);
#endif 
    



    // We process two types of URLs
    //  1 - http://servername/virtualdirname/.../soapisap.dll
    //  2 - http://servername/virtualdirname/.../filename.wsdl
    //  In case 1, the virtualdirname is the name of the WSDL file
    //  In case 2, the filename.wsdl is the name of the WSDL file
    //  In both cases the WSDL file should be located in the directory 
    //  pointed by the URL.
    
    // Construct the WSDL file path & name
    if (pszPathInfo && *pszPathInfo)
    {
        // We have a .wsdl file in the URL
        cchApplPath = strlen(pszPathTranslated);
        if ((cchApplPath > FOURK) || (cchApplPath < 5) ||   
            (_strnicmp(&(pszPathTranslated[cchApplPath - 5]), ".WSDL", 5)))
        {
            m_pIStreamOut.SetErrorCode(Error_BadRequest);
#ifndef UNDER_CE
            return E_INVALIDARG;
#else       
            hr = E_INVALIDARG;
            goto Cleanup;
#endif 
        }        
        strncpy(szApplPath, pszPathTranslated, cchApplPath+1);
    }
    else
    {
        // URL with soapisap.dll 
        // Get the physical path for the virtual directory
        m_pECB->GetServerVariable(
            m_pECB->ConnID, "APPL_PHYSICAL_PATH", szApplPath, &cchApplPath);
        //
        // Get the application meta directory path
        // The last section in this path gives us the vname, which should be
        //  the same as WSDL file name.
        m_pECB->GetServerVariable(
            m_pECB->ConnID, "APPL_MD_PATH", szApplMDPath, &cchApplMDPath);
            
        ForwardToBackslash(szApplMDPath);
        cchApplMDPath--;
        if (szApplMDPath[cchApplMDPath] == 0)
            cchApplMDPath--;  // Point to last character before NULL
        while ((cchApplMDPath > 0) && (szApplMDPath[cchApplMDPath] != '\\'))
        {
            cchApplMDPath--;
        }
        if (cchApplMDPath > 0)
            cchApplMDPath++;    
        
        ForwardToBackslash(szApplPath);
        cchApplPath--;
        if (szApplPath[cchApplPath] == 0)
            cchApplPath--;  // Point to last character before NULL
        if ((cchApplPath > 0) && (szApplPath[cchApplPath] != '\\'))
            szApplPath[cchApplPath] = '\\';
        cchApplPath++;    
        while((cchApplPath < FOURK-6) && (szApplMDPath[cchApplMDPath] != '\0'))
            szApplPath[cchApplPath++] = szApplMDPath[cchApplMDPath++];
        ASSERT(cchApplPath + 5 < FOURK);   
        szApplPath[cchApplPath++] = '.' ;
        szApplPath[cchApplPath++] = 'W' ;
        szApplPath[cchApplPath++] = 'S' ;
        szApplPath[cchApplPath++] = 'D' ;
        szApplPath[cchApplPath++] = 'L' ;
        szApplPath[cchApplPath] = '\0' ;
    }    
    if (cchApplPath < 2) 
    {
        ASSERT(FALSE);
        m_pIStreamOut.SetErrorCode(Error_BadRequest);       
#ifndef UNDER_CE
        return E_INVALIDARG;
#else   
        hr = E_INVALIDARG;
        goto Cleanup;
#endif 
    }    
    m_pszWSDLFilePath = new char[cchApplPath+1];    
    m_pszWSMLFilePath = new char[cchApplPath+1];
    if ((m_pszWSMLFilePath == NULL) || (m_pszWSMLFilePath == NULL))
    {
        ASSERT(FALSE);
        m_pIStreamOut.SetErrorCode(Error_BadRequest);               
#ifndef UNDER_CE
        return E_OUTOFMEMORY;
#else
        hr = E_OUTOFMEMORY;
        goto Cleanup;
#endif 
    }
    strcpy(m_pszWSDLFilePath, szApplPath);
    strcpy(m_pszWSMLFilePath, szApplPath);
    m_pszWSMLFilePath[cchApplPath - 2] = 'm';
    
#ifdef UNDER_CE
    hr = S_OK;
Cleanup:
    if(szApplPath)
        delete [] szApplPath;
    if(szApplMDPath)
        delete [] szApplMDPath;
        
    return hr;
#else
    return S_OK;
#endif 
 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CRequest::ExecuteRequest()
//
//  parameters:
//          
//  description:
//          Executes the given request
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT CRequest::ExecuteRequest()
{
    HRESULT     hr = S_OK;
    VARIANT      varInput;
    CObjCache *  pObjCache = m_pThreadInfo->pObjCache;
    CThdCachedObj *pCachedObj = NULL;
    OBJ_STATE      objstate;
    CAutoBSTR    bstrSoapAction;
    CAutoRefc<IStream> pIStrmOut;

    
    VariantInit(&varInput);
    
    // Find out if we already have a cached SoapServer object
    //  servicing this file
    ASSERT(pObjCache);

    objstate = pObjCache->Find (m_pszWSDLFilePath, (void **) &pCachedObj);
    switch (objstate)
    {
    case OBJ_OK:
        // Everything is OK. We use this object
        ASSERT(pCachedObj);
        break;
    case OBJ_ACCESS_DENIED:
        // Current user does not have access permissions to these files
        hr = E_ACCESSDENIED;
        m_pIStreamOut.SetErrorCode(Error_AccessDenied);
        return hr;
    case OBJ_STALE:
        // Files have been modified since last use here
    case OBJ_NOTFOUND:
        ASSERT(pCachedObj == NULL);
        break;
    }

    if (!pCachedObj)
    {
        // Object not in cache. We need to create one
        CHK(InitObjectForCache(&pCachedObj));
        CHK(pObjCache->Insert(m_pszWSDLFilePath, (void *) pCachedObj))
    }
    CHK(bstrSoapAction.Assign(GetHeaderParam("SOAPAction"), FALSE));
    ASSERT(pCachedObj);
    
    V_VT(&varInput) = VT_UNKNOWN;
    V_UNKNOWN(&varInput) = &m_pIStreamIn;

    CHK(m_pIStreamOut.QueryInterface(IID_IStream, (void **) &pIStrmOut));
    hr = pCachedObj->ISoapServer()->SoapInvoke(varInput, pIStrmOut, bstrSoapAction);
    
Cleanup:
    return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CRequest::InitObjectForCache(CThdCachedObj ** ppCachedObj)
//
//  parameters:
//          
//  description:
//
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT CRequest::InitObjectForCache(CThdCachedObj ** ppCachedObj)
{
    HRESULT     hr = S_OK;
    CThdCachedObj *pCachedObj = NULL;
    CAutoRefc<ISOAPServer> pISoapServer;
    CAutoBSTR       bstrWSDL;
    CAutoBSTR       bstrWSML;
    DWORD           wchApplPath = FOURK;
    
#ifndef UNDER_CE
    WCHAR           wszApplPath[FOURK+1];
    FILETIME *      pFTWSDL;
    FILETIME *      pFTWSML;
#else
    WCHAR          *wszApplPath = new WCHAR[FOURK+1];
#endif 


    *ppCachedObj = NULL;
    
    // Create the SOAPServer object for this
    hr = CoCreateInstance(CLSID_SoapServer, NULL,
        CLSCTX_INPROC_SERVER, IID_ISOAPServer, (LPVOID*)&pISoapServer);
    if (hr != S_OK)
    {
        m_pIStreamOut.SetErrorCode(Error_ServiceUnavailable);
#ifndef UNDER_CE
        return hr;
#else
        goto Cleanup;
#endif 
    }
        
    wchApplPath = MultiByteToWideChar(
                    CP_ACP, 
                    0, 
                    m_pszWSDLFilePath, 
                   (int)strlen(m_pszWSDLFilePath) + 1, 
                    wszApplPath, 
                    (int)wchApplPath);
    if (wchApplPath < 2) 
    {
        m_pIStreamOut.SetErrorCode(Error_BadRequest);
#ifndef UNDER_CE
        return E_FAIL;
#else
        hr = E_FAIL;
        goto Cleanup;
#endif 
    }    

    bstrWSDL.Assign((BSTR)wszApplPath);
    wszApplPath[wchApplPath - 3] = (WCHAR)'M';
    bstrWSML.Assign((BSTR)wszApplPath);
    // Initialize the SoapServer object with WSDL and WSML files
    // Pass the SOAP message to the SOAP Server object    
    if (!pISoapServer || bstrWSDL.isEmpty() || bstrWSML.isEmpty())
    {
        ASSERT(FALSE);
        m_pIStreamOut.SetErrorCode(Error_ServiceUnavailable);
#ifndef UNDER_CE
        return E_FAIL;
#else
        hr = E_FAIL;
        goto Cleanup;
#endif 
    }
    
    hr = pISoapServer->Init(bstrWSDL, bstrWSML);
    if (hr != S_OK)
    {
        m_pIStreamOut.SetErrorCode(Error_ServiceUnavailable);
#ifndef UNDER_CE
        return hr;
#else
        goto Cleanup;
#endif 
    }    
    pCachedObj = new CThdCachedObj();
    if (!pCachedObj)
    {
        m_pIStreamOut.SetErrorCode(Error_ServiceUnavailable);
#ifndef UNDER_CE
        return E_OUTOFMEMORY;
#else
        hr = E_OUTOFMEMORY;
        goto Cleanup;        
#endif 
    }  
    hr = pCachedObj->Init(m_pszWSDLFilePath, m_pszWSMLFilePath,
        pISoapServer.PvReturn());
    if (hr != S_OK)
    {
        m_pIStreamOut.SetErrorCode(Error_ServiceUnavailable);
#ifndef UNDER_CE
        return hr;
#else       
        goto Cleanup;
#endif 
    } 

    // All is OK;
    *ppCachedObj = pCachedObj;
   
#ifndef UNDER_CE
    return S_OK;
#else
    hr = S_OK;
    
Cleanup:
    
    if(wszApplPath)
        delete [] wszApplPath;
        
    return hr;
#endif 
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CRequest::ProcessGet(BOOL isHead)
//
//  parameters:
//          
//  description:
//          Executes the given GET or HEAD request
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT CRequest::ProcessGet(BOOL isHead)
{
    HRESULT     hr = S_OK;
    char *      pszPathInfo = m_pECB->lpszPathInfo; 
    char *      pszPathTranslated = m_pECB->lpszPathTranslated;
    DWORD       cchApplPath;
    HANDLE      hFile = INVALID_HANDLE_VALUE;
    CFileStream *pFileStrm = NULL;
#ifndef UNDER_CE
    char *      tempbuf[FOURK + 1];
#endif 
    DWORD       cbFileHigh;
    DWORD       cbFileLow;
    ULARGE_INTEGER  ucbFile;

    if (!pszPathInfo || !(*pszPathInfo))
    {
        m_pIStreamOut.SetErrorCode(Error_NotFound);
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }   
    // We must have a .wsdl file in the URL
    cchApplPath = strlen(pszPathTranslated);
    if ((cchApplPath > FOURK) || (cchApplPath < 5) ||   
            (_strnicmp(&(pszPathTranslated[cchApplPath - 5]), ".WSDL", 5)))
    {
        m_pIStreamOut.SetErrorCode(Error_BadRequest);
        return E_FAIL;
    }        
    
    hFile = CreateFileA(pszPathTranslated, GENERIC_READ,
            FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);            
    if (hFile == INVALID_HANDLE_VALUE)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        switch(HRESULT_CODE(hr))
        {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
                m_pIStreamOut.SetErrorCode(Error_NotFound);
                break;
            case ERROR_ACCESS_DENIED:
                m_pIStreamOut.SetErrorCode(Error_AccessDenied);
                break;
            default:
                m_pIStreamOut.SetErrorCode(Error_BadRequest);
                break;
        }
        CHK(hr);
    }    
    cbFileLow = GetFileSize(hFile, &cbFileHigh);
    if ((cbFileLow == 0) && (cbFileHigh == 0))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        m_pIStreamOut.SetErrorCode(Error_BadRequest);
        CHK(hr);
    }
    ucbFile.LowPart = cbFileLow;
    ucbFile.HighPart = (ULONG)cbFileHigh;
    m_pIStreamOut.SetSize(ucbFile);
    
    if (isHead)
    {
        m_pIStreamOut.SetHeadersOnly();
    }
    else
    {
        pFileStrm = new CSoapObject<CFileStream>(INITIAL_REFERENCE);
        
#ifdef UNDER_CE
       if(!pFileStrm)
       {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
       }     
#endif 
        hr = pFileStrm->Initialize(hFile); 
        hFile = INVALID_HANDLE_VALUE;
        CHK(m_pIStreamOut.Write((IStream *)pFileStrm));
    }
    hr = S_OK;
    
Cleanup:
    if (hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);
    ::ReleaseInterface(pFileStrm);
    return hr;
}    

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CRequest::SetErrorCode(ERROR_CODE errcode)
//
//  parameters:
//          
//  description:
//          Sets up for http error return to the client
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRequest::SetErrorCode(ERROR_CODE errcode)
{
    m_pIStreamOut.SetErrorCode(errcode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CRequest::FlushContents()
//
//  parameters:
//          
//  description:
//          Flushes out the contents
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRequest::FlushContents()
{
    m_pIStreamOut.FlushBuffer();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CRequest::WriteFaultMessage()
//
//  parameters:
//          
//  description:
//          Flushes out the contents
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRequest::WriteFaultMessage(HRESULT hr, BSTR bstrActor)
{

    // An error occured and we have to send the client back an error fault.
    m_pIStreamOut.WriteFaultMessage(hr, bstrActor);
    
}

