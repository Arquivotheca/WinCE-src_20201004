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

#ifndef _C_CLIENT_MANAGER_
#define _C_CLIENT_MANAGER_

#include "snsDefines.h"



class CClientManager
{

public:



    static const int   RANDOM_PRI = -1;
    
    struct ClientPayload
    {       
        VOID*   pBufIn;
        DWORD   dwLenIn;;
        DWORD   dwOutVal;
    };

    struct ClientProfile 
    {
        LPCWSTR         pszLibrary;
        LPCWSTR         pszEntryPt;
        INT             nPriority;
        ClientPayload*  pPayload;        
    };

    typedef void (*SNS_CLIENT_FNPTR) (CClientManager::ClientPayload* pPayload);

    ~CClientManager();        
    CClientManager();

    CClientManager(
        DWORD dwClientCount, 
        __deref_inout ClientProfile* pClientProfiles,
        __out_ecount_opt(dwClientCount)DWORD* pOutVals );
        
    CClientManager(    
        DWORD dwClientCount,
        __nullterminated LPCWSTR pszLibrary,
        __nullterminated LPCWSTR pszEntryPt,
        __deref_inout ClientPayload* pPayload,
        __out_ecount_opt(dwClientCount)DWORD* pOutVals );
       
    BOOL RunClients();
    BOOL SetTimeout( DWORD dwTimeout );

protected:
    static const DWORD MIN_THREAD_CNT = 0;
    static const DWORD MAX_THREAD_CNT = 10;
    static const DWORD DEFAULT_TIMEOUT = ONE_MINUTE;
    static const DWORD MAX_FAILURES = 100;
    
    struct ClientInfo
    {
        HTHREAD*          pHandle;
        DWORD            dwThreadId;
        ClientProfile*   pClientProfile;
        HINSTANCE        hInstDLL;
        SNS_CLIENT_FNPTR fncClientEntry;
    };


private:
    void DisplayThreadInformation();
    BOOL SetClientThreadPriority( HTHREAD hClient, int& nPri );
    BOOL InitializeProfiles(LPCWSTR pszLibrary, LPCWSTR pszEntryPt);
    BOOL InitializeClients();
    
    //No copies... we don't need that headache
    CClientManager( CClientManager& );
    CClientManager& operator=(const CClientManager& x);

    //Data Members
    ClientInfo* m_pClients;     //Array of client & thread information
    ClientProfile* m_pProfiles; //Array of client specific information   
    HTHREAD* m_pClientHandles;   //Array of handles to client threads
    DWORD* m_pOutVals;          //Optional array of client result values
    CRITICAL_SECTION m_CSBlock; //Protection++    
    DWORD m_dwClientCount;      
    DWORD m_dwHealthySpawns;    //Use to track how many clients we actually have
    DWORD m_dwTimeout;          
    BOOL m_bInitialized;
    BOOL m_bCustomInit;
    
    

};
#endif //_C_CLIENT_MANAGER_

