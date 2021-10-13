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
//+----------------------------------------------------------------------------
//
// 
// Module Name:    thdcache.h
// 
// Contents:
//
//    ISAPI Extension that services SOAP packages.
//
//-----------------------------------------------------------------------------

#ifndef THDCACHE_H_INCLUDED
#define THDCACHE_H_INCLUDED


OBJ_STATE ThdCacheOKForAccess(void *pvData);
char * ThdCacheGetKey (void *pvData);
void ThdCacheDelete(void * pvData );

// This class implements the per thread object cache
class CThdCachedObj
{
public:
    CThdCachedObj();    
    ~CThdCachedObj();
    HRESULT Init(char *pszWSDL, char * pszWSML,
                ISOAPServer *pISOAPServer);
    inline  char * WSDLFilePath() { return m_pszWSDLFilePath; };
    inline  char * WSMLFilePath() { return m_pszWSMLFilePath; };
    inline  ISOAPServer * ISoapServer() { return m_pISoapServer ; };
    char * getKey();
    OBJ_STATE ChkOKForAccess();
 
private:

    CAutoRefc<ISOAPServer>  m_pISoapServer;
    CAutoRg<char>           m_pszWSDLFilePath;
    FILETIME                m_WSDLLastWrite;
    CAutoRg<char>           m_pszWSMLFilePath;
    FILETIME                m_WSMLLastWrite;


    HRESULT OpenFiles(HANDLE * phWSDL, HANDLE *phWSML);
    HRESULT GetFileLastWriteTime(HANDLE hfile,FILETIME *pFTLastWrite);

};

#endif  //THDCACHE_H_INCLUDED

