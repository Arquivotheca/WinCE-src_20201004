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
#include "CClientManager.h"





//------------------------------------------------------------------------------
// CClientManager 
//
CClientManager::CClientManager():
        m_dwClientCount(0),
        m_pProfiles(NULL),
        m_pClients(NULL),
        m_pOutVals(NULL),
        m_pClientHandles(NULL),
        m_bInitialized(FALSE),
        m_bCustomInit(FALSE),
        m_dwHealthySpawns(0),
        m_dwTimeout(0)
{
    LOG_START();
    ASSERT(0);
    // Shouldn't be called.... 
    LOG_END();
}//Default

//------------------------------------------------------------------------------
// CClientManager 
//
CClientManager::CClientManager( 
    DWORD dwClientCount, 
    __deref_inout ClientProfile* pClientProfiles,
    __out_ecount_opt(dwClientCount)DWORD* pOutVals ):
        m_dwClientCount( dwClientCount ),
        m_pProfiles(pClientProfiles),
        m_pClients(NULL),
        m_pOutVals(pOutVals),
        m_pClientHandles(NULL),
        m_bInitialized(FALSE),
        m_bCustomInit(TRUE),
        m_dwHealthySpawns(0),
        m_dwTimeout(DEFAULT_TIMEOUT)
{
    LOG_START();
    BOOL bResult = FALSE;
    
    //Validate our inputs
    VERIFY_STEP("Verifying Input/Output Buffer",(pClientProfiles!=NULL),FAIL_EXIT );    
    VERIFY_STEP("Verifying Thread Count",((m_dwClientCount > MIN_THREAD_CNT)&(m_dwClientCount <= MAX_THREAD_CNT)),FAIL_EXIT);
    
    //Setup our critical sections... try to avoid nightmare debugging scenarios
    InitializeCriticalSection(&m_CSBlock);

    bResult = InitializeClients();
    VERIFY_STEP( "Client Initialization",(bResult),FAIL_EXIT);
    
    m_bInitialized = TRUE;
DONE:
    if( !m_bInitialized )
    {
        LOG_WARN( "Constructor Initialization Failed" );
        ASSERT(0);
    }    
    LOG_END();
}//UberCustom


//------------------------------------------------------------------------------
// CClientManager 
//
CClientManager::CClientManager(    
    DWORD dwClientCount,
    __nullterminated LPCWSTR pszLibrary,
    __nullterminated LPCWSTR pszEntryPt,
    __deref_inout ClientPayload* pPayload,
    __out_ecount_opt(dwClientCount)DWORD* pOutVals ):
        m_dwClientCount(dwClientCount),
        m_pProfiles(NULL),
        m_pClients(NULL),
        m_pOutVals(pOutVals),
        m_pClientHandles(NULL),
        m_bInitialized(FALSE),
        m_bCustomInit(FALSE),
        m_dwHealthySpawns(0),
        m_dwTimeout(DEFAULT_TIMEOUT)
{
    LOG_START();
    BOOL bResult = FALSE;
    
    //Validate our inputs
    VERIFY_STEP("Verifying Library Name", (pszLibrary != NULL), FAIL_EXIT);
    VERIFY_STEP("Verifying Entry Point Name", (pszEntryPt != NULL), FAIL_EXIT);
    VERIFY_STEP("Verifying Payload Pointer",((pPayload!=NULL)),FAIL_EXIT );    
    VERIFY_STEP("Verifying Thread Count",((m_dwClientCount > MIN_THREAD_CNT)&(m_dwClientCount <= MAX_THREAD_CNT)),FAIL_EXIT);
    
    //Create our client profile. In this case, the same profile is sent
    //to all the worker threads.
    m_pProfiles = new ClientProfile[ m_dwClientCount ];
    VERIFY_STEP( "Creating Profile Array",(m_pProfiles != NULL),FAIL_EXIT);

    for( DWORD i = 0; i < m_dwClientCount; i++ )
    {
        m_pProfiles[i].nPriority = THREAD_PRIORITY_NORMAL;
        m_pProfiles[i].pPayload = pPayload;
        m_pProfiles[i].pszEntryPt = pszEntryPt;
        m_pProfiles[i].pszLibrary = pszLibrary;
    }

    //Setup our critical sections... try to avoid nightmare debugging scenarios
    InitializeCriticalSection(&m_CSBlock);
    
    bResult = InitializeClients();
    VERIFY_STEP( "Client Initialization",(bResult),FAIL_EXIT);
    

    
    m_bInitialized = TRUE;
DONE:
    if( !m_bInitialized )
    {
        LOG_WARN( "Constructor Initialization Failed" );
        ASSERT(0);
    }
    
    LOG_END();
}//CClientManager



//------------------------------------------------------------------------------
// InitializeClients
//
BOOL CClientManager::InitializeClients()
{
    LOG_START();
    EnterCriticalSection(&m_CSBlock);
    
    BOOL bResult = FALSE;

    //We need a profile to continue from here
    VERIFY_STEP( "Verifying Client Profiles",(m_pProfiles!= NULL),FAIL_EXIT);

    m_pClientHandles = new HTHREAD[ m_dwClientCount ];
    VERIFY_STEP( "Creating Client Handles",(m_pClientHandles != NULL),FAIL_EXIT);
    
    m_pClients = new ClientInfo[ m_dwClientCount ];
    VERIFY_STEP( "Creating Client Control",(m_pClients != NULL),FAIL_EXIT);
    
    for( DWORD i = 0; i < m_dwClientCount; i++ )
    {
        
        //1.Attempt to load test library
        RETAILMSG( SNS_DBG, (_T("Loading %s...."), m_pProfiles[i].pszLibrary ) );
        m_pClients[i].hInstDLL = LoadLibrary(m_pProfiles[i].pszLibrary);
        if( m_pClients[i].hInstDLL != m_pClients[i].hInstDLL )
        {
            LOG_WARN( "Could not access entry point" );    
            continue;
        } 


        //2. Can we access the specified entry point? 
        RETAILMSG( SNS_DBG, (_T("Accessing Entry Point: %s...."), m_pProfiles[i].pszLibrary ) );
        m_pClients[i].fncClientEntry = (SNS_CLIENT_FNPTR) GetProcAddress(
            m_pClients[i].hInstDLL, 
            m_pProfiles[i].pszEntryPt);
        
        if( m_pClients[i].fncClientEntry == NULL )
        {
            LOG_WARN( "Could not access entry point" );    
            continue;
        } 
        

        //3. Generate the client threads - callers responsibility to ensure
        //that the client data matches the expected type
        RETAILMSG( SNS_DBG, (_T("Creating Client: %d/%d"),i, m_dwClientCount)); 
        m_pClientHandles[i] = CreateThread(
            NULL,
            0, 
            (LPTHREAD_START_ROUTINE)(m_pClients[i].fncClientEntry),
            m_pProfiles[i].pPayload,
            CREATE_SUSPENDED,
            &(m_pClients[i].dwThreadId) );
        
        if( m_pClientHandles[i] == INVALID_HANDLE_VALUE )
        {
            LOG_WARN( "Client thread creation failed" );    
            continue;    
        }  
        else
        {
            bResult = SetClientThreadPriority( m_pClientHandles[i], m_pProfiles[i].nPriority );
            VERIFY_STEP( "Verifying Thread Priority Setting",( bResult ),FAIL_EXIT );
        }


        //4. The client info array is what we will use through out. 
        m_pClients[i].pHandle = &(m_pClientHandles[i]);
        m_pClients[i].pClientProfile = &(m_pProfiles[i]);

        //5. Used to track returning threads. 
        m_dwHealthySpawns++;
    }
    
    VERIFY_STEP( "Verifying Available Thread Pool",( m_dwHealthySpawns != 0),FAIL_EXIT );

    bResult = TRUE;
DONE:
    LOG_END();
    LeaveCriticalSection(&m_CSBlock);
    return bResult;
}//InitializeClients




//------------------------------------------------------------------------------
// ~CClientManager
//
CClientManager::~CClientManager()
{
    LOG_START();
    m_bInitialized = FALSE;
    BOOL bResult = FALSE;

    for( DWORD i = 0; i < m_dwClientCount; i++ )
    {        
     
        if( m_pClients )
        {

            if( m_pClients[i].hInstDLL)
            {
                bResult = FreeLibrary(m_pClients[i].hInstDLL);
                if( !bResult )
                {
                    LOG_WARN( "Could not free library..." );   
                }
            }           
            m_pClients[i].pClientProfile = NULL;
            m_pClients[i].pHandle = NULL;
            m_pClients[i].fncClientEntry = NULL;
        }            



        if( m_pClientHandles )
        {       
            if( m_pClientHandles[i] != INVALID_HANDLE_VALUE )
            {
              
               bResult = CloseHandle( m_pClientHandles[i] );        

               if( !bResult )
               {
                    LOG_WARN( "Could not free library..." );   
               }

            }       
        }
    }

    if( m_pClients )
    {
        delete[] m_pClients;
    }

    if( !m_bCustomInit )
    {
        if( m_pProfiles )
        {
            delete[] m_pProfiles;
        }
    }

    if( m_pClientHandles )
    {
        delete[] m_pClientHandles;
    }

    m_dwClientCount = 0;
    m_pProfiles = NULL;
    m_pClients = NULL;
    m_pClientHandles = NULL;
    m_bCustomInit = FALSE;
    m_dwHealthySpawns = 0;


    //Clean up our critical section
    DeleteCriticalSection(&m_CSBlock);
        
    LOG_END();
}//~CClientManager


//------------------------------------------------------------------------------
// SetTimeout
//
BOOL CClientManager::SetTimeout(DWORD dwTimeout)
{
    LOG_START();
    EnterCriticalSection(&m_CSBlock);
    BOOL bResult = FALSE;

    VERIFY_STEP( "Setting Client Run Timeout", (dwTimeout < INFINITE), FAIL_EXIT );
    m_dwTimeout = dwTimeout;
    
    bResult = TRUE;
DONE:    
    LeaveCriticalSection(&m_CSBlock);
    LOG_END();
    return bResult;
}//SetTimeout


//------------------------------------------------------------------------------
// RunClients
//
BOOL CClientManager::RunClients()
{
    LOG_START();
    EnterCriticalSection(&m_CSBlock);
    BOOL bResult = FALSE;
    BOOL bDoneWaiting = FALSE;
    DWORD dwCompleted = 0;
    DWORD dwFailed = 0;
    DWORD dwWaitResult = 0;
    DWORD dwRetIdx = 0;
    DWORD dwRetVal = 0;
    HANDLE* pHandleCopy = NULL; 
    HANDLE hCurrentThread = INVALID_HANDLE_VALUE;

    VERIFY_STEP( "Checking object initialization", m_bInitialized, FAIL_EXIT );
                 

    //Wake them all up
    for( DWORD i = 0; i < m_dwClientCount; i++ )
    {
        if( m_pClients[i].pHandle != INVALID_HANDLE_VALUE )
        {
            RETAILMSG( SNS_DBG, (_T("Waking TID: 0x%08X with Priority: %d"), 
                m_pClients[i].dwThreadId, m_pClients[i].pClientProfile->nPriority));
  
            DWORD dwResume =  ResumeThread( *(m_pClients[i].pHandle) );

            if( dwResume == 0xFFFFFFFF )
            {
                LOG_WARN( "Client thread wake-up failed" );   
                m_dwHealthySpawns--;
            }
            else if( dwResume == 0 )
            {
                LOG_WARN( "Thread was never suspended - possible timing errors may occur" );   
            }
            else if( dwResume > 1 )
            {
                LOG_WARN( "Thread can't be re-started" );   
                m_dwHealthySpawns--;
            }            
        }
    }        

    //We need a way to know which thread returned and not to wait on them again
    //since an array of handles is passed to WaitForMultipleObjects, setting 
    //the just returned thread handle to our current handle ensures that
    //the Wait function will not continually return as soon as one thread
    //is signaled. Using INVALID_HANDLE_VALUE yields to many runtime errors 
    //while a dynamically changing list would be too costly. 
    pHandleCopy = new HANDLE[m_dwClientCount];
    VERIFY_STEP( "Allocating Temporary Handle Array", (pHandleCopy != NULL), FAIL_EXIT );
    memcpy_s( pHandleCopy, m_dwClientCount * sizeof(HANDLE), m_pClientHandles, m_dwClientCount * sizeof(HANDLE));
    hCurrentThread = GetCurrentThread();

    do
    {
        dwRetIdx = 0xFFFFFFFF;
        dwRetVal = 0xFFFFFFFF;

        dwWaitResult = WaitForMultipleObjects( 
            m_dwClientCount,
            pHandleCopy,
            FALSE,
            m_dwTimeout );

        if( dwWaitResult == WAIT_FAILED )
        {       
            LOG_WARN( "One or more threads Failed to return" ); 
            dwFailed++;
        }
        else if( dwWaitResult == WAIT_TIMEOUT )
        {
             LOG_WARN( "One or more threads have timed out" ); 
             dwFailed++;
        }
        else
        {
            dwRetIdx = dwWaitResult - WAIT_OBJECT_0;
            dwRetVal = m_pProfiles[dwRetIdx].pPayload->dwOutVal;
            if( m_pOutVals )
            { 
                m_pOutVals[dwRetIdx] = dwRetVal;
            }
            
            pHandleCopy[dwRetIdx] = hCurrentThread;

            dwCompleted++;
        }


        RETAILMSG( SNS_DBG, (_T("Client(%d):\tWait Return:0x%08X\tReturnValue:0x%08X\tTotal Failed: %d\tTotal Success: %d "), 
            dwRetIdx, dwWaitResult, dwRetVal, dwFailed, dwCompleted));

        if(dwFailed >= MAX_FAILURES)
        {
            LOG_WARN( "Maximum Failures Reached - Aborting run!" ); 
            bDoneWaiting = TRUE;
        }
    }
    while( ( dwCompleted != m_dwHealthySpawns ) && ( !bDoneWaiting )  );

    VERIFY_STEP( "Client Return", !bDoneWaiting, FAIL_EXIT );

    bResult = TRUE;    
DONE:

    if( pHandleCopy )
        delete[] pHandleCopy;

    DisplayThreadInformation();
    LeaveCriticalSection(&m_CSBlock);
    LOG_END();
    return bResult;
}//RunClients



//------------------------------------------------------------------------------
// SetClientThreadPriority
//
BOOL CClientManager::SetClientThreadPriority( HTHREAD hClient, int& nPri )
{
    LOG_START();
    BOOL bResult = FALSE;


    if( nPri == RANDOM_PRI )
    {
        bResult = SetThreadPriority( hClient, RANGE_RAND(THREAD_PRIORITY_TIME_CRITICAL, THREAD_PRIORITY_IDLE));
        nPri =  GetThreadPriority( hClient );
    }
    else
    {
        if( nPri > THREAD_PRIORITY_IDLE || nPri < THREAD_PRIORITY_TIME_CRITICAL )
        {
            nPri = THREAD_PRIORITY_NORMAL;
            LOG_WARN( "Invalid Thread Priority Specified" );
        }
        bResult = SetThreadPriority( hClient, nPri);        
    }
    
    LOG_END();
    return bResult;
}//SetClientThreadPriority


//------------------------------------------------------------------------------
// DisplayThreadInformation
//
void CClientManager::DisplayThreadInformation()
{
    LOG_START();
    
    if(!m_bInitialized)
        return;
    
    FILETIME CreationTime = {0};
    FILETIME ExitTime = {0};
    FILETIME KernelTime = {0};
    FILETIME UserTime = {0};
    BOOL bVH = FALSE;
    BOOL bVT = FALSE;
    BOOL bEC = FALSE;
    DWORD dwExitCode = 0;

    for( DWORD i = 0; i < m_dwClientCount; i++ )        
    {
        bVH = ( *(m_pClients[i].pHandle) != INVALID_HANDLE_VALUE );

        if( bVH )
        {
            bVT =  GetThreadTimes( *(m_pClients[i].pHandle), &CreationTime, &ExitTime, &KernelTime, &UserTime );
            bEC = GetExitCodeThread( *(m_pClients[i].pHandle),&dwExitCode);
        }
        
        GetExitCodeThread (*(m_pClients[i].pHandle), &dwExitCode );
        RETAILMSG( SNS_DBG, (_T("------------------------------------------")));
        RETAILMSG( SNS_DBG, (_T("  Client Index........%d"), i ));
        RETAILMSG( SNS_DBG, (_T("  Thread Id...........0x%08X"), bVH?(m_pClients[i].dwThreadId):(0)));
        RETAILMSG( SNS_DBG, (_T("  Thread Priority.....%d"), bVH?(m_pClients[i].pClientProfile->nPriority):(0)));
        RETAILMSG( SNS_DBG, (_T("  Exit Code...........0x%08X"), bEC?(dwExitCode):(0xFFFFFFFF)));
        RETAILMSG( SNS_DBG, (_T("  Creation Time.......%I64u"), bVT?((((ULARGE_INTEGER*)(&CreationTime))->QuadPart)):(0)));
        RETAILMSG( SNS_DBG, (_T("  Exit Time...........%I64u"), bVT?((((ULARGE_INTEGER*)(&ExitTime))->QuadPart)):(0)));
        RETAILMSG( SNS_DBG, (_T("  Kernel Time.........%I64u"), bVT?((((ULARGE_INTEGER*)(&KernelTime))->QuadPart)):(0)));
        RETAILMSG( SNS_DBG, (_T("  User Time...........%I64u"), bVT?((((ULARGE_INTEGER*)(&UserTime))->QuadPart)):(0))); 

        if( m_pOutVals )
        {
            RETAILMSG( SNS_DBG, (_T("  Client Result ......0x%08X"), m_pOutVals[i]));
        }
        else
        {
            RETAILMSG( SNS_DBG, (_T("  Client Result ......[Not Specified]")));
        }
        RETAILMSG( SNS_DBG, (_T("------------------------------------------\n")));
    }

    RETAILMSG( SNS_DBG,(_T(".\n\n")));
    LOG_END();

}

