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
// Module Name:    objcache.h
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

