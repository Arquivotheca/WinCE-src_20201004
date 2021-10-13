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
#define STRSAFE_NO_A_FUNCTIONS
#define STRSAFE_INLINE
#define _STRSAFE_USE_SECURE_CRT 1

#include <windows.h>
#include <Kitlprot.h>
#include <Halether.h>
#include <Storemgr.h>

#include "KITLStressDev.h"
#include "KITLTestDriverDev.h"
#include "log.h"

extern SPS_SHELL_INFO *g_pShellInfo;


typedef struct _THREAD_PARAMS
    {
        CKITLStressDev* pKITLStressClient;
        CKitlPerfLog* pkitlLog;

        _THREAD_PARAMS(CKITLStressDev* _pDev, CKitlPerfLog* _pkitlLog)
        {
            pKITLStressClient = _pDev;
            pkitlLog = _pkitlLog;
        }
} THREAD_PARAMS;








#define CE_VOLUME_FLAG_NETWORK 0x10

/******************************************************************************
//  Function Name:  relFRD
//    Description:
//      This function determines whether the device is connected via
//      KITL or not
//
//    Input:
//      None
//
//    Output:
//      TRUE: A KITL connnection is being used
//      FALSE: No KITL connection
//
******************************************************************************/
BOOL relFRD()
{
    CE_VOLUME_INFO attrib = {0};

    BOOL fRet = CeGetVolumeInfo(L"\\Release", CeVolumeInfoLevelStandard, &attrib); // dwAttributes 0x0 dwFlags 0x10
    if (!fRet)
    {
        return FALSE;
    }
    else if (attrib.dwFlags == CE_VOLUME_FLAG_NETWORK)
    {
        return TRUE;
    }
    return FALSE;
}





// Used to implement the Singleton pattern
LONG CKITLTestDriverDev::m_lRefCount = 0;
CKITLTestDriverDev *CKITLTestDriverDev::m_pKITLTestDriver = NULL;

/******************************************************************************
//  Function Name:  CKITLTestDriverDev::CKITLTestDriverDev
//    Description:
//      Initializes CKITLTestDriverDev's member variables.  Note here that the actual
//      instantiation happens on CreateInstance
//
//    Input:
//      None
//
//    Output:
//      None
//
******************************************************************************/
CKITLTestDriverDev::CKITLTestDriverDev()
{
    m_pBufferPool = NULL;
    m_ppKITLStressClient = NULL;
    m_phThread = NULL;
    m_hCancelTest = NULL;

    m_uKITLStrmID = 0;
    m_ucWindowSize = 0;
    m_dwThreadCount = 0;
    memset(&m_TestSettings, 0, sizeof(m_TestSettings));
}

/******************************************************************************
//    Function Name:  CKITLTestDriverDev::KITLThreadProc
//    Description:
//      Worker thread for CKITLTestDriverDev
//
//    Input:
//      __in_opt  LPVOID lpParameter
//
//    Output:
//      None
//
******************************************************************************/
DWORD WINAPI CKITLTestDriverDev::KITLThreadProc(
  __in_opt  LPVOID lpParameter
)
{
    HRESULT hr = E_FAIL;

    if(lpParameter) {
        CKITLStressDev *pkitlStress = reinterpret_cast<THREAD_PARAMS*>(lpParameter)->pKITLStressClient;
        CKitlPerfLog* pkitlLog = reinterpret_cast<THREAD_PARAMS*>(lpParameter)->pkitlLog;
        hr = pkitlStress->RunTest(pkitlLog);
    }

    return hr;
}


/******************************************************************************
//    Function Name:  CKITLTestDriverDev::CreateInstance
//    Description:
//      Note that this function is NOT Thread safe.  CKITLTestDriverDev is created when tux is
//      loading and is shared by any number of test cases run.  As only one instance of this
//      object got to exist at a time, this contructs implements Singleton pattern to acheive
//      this requirement
//
//    Input:
//        __deref CKITLTestDriverDev **ppKITLTestDriver:
//
//    Output:
//        The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDev::CreateInstance(__deref CKITLTestDriverDev **ppKITLTestDriver)
{
    HRESULT hr = E_FAIL;

    if (!ppKITLTestDriver)
    {
        hr = E_INVALIDARG;
        FAILLOGV();
        goto Exit;
    }

    if (m_lRefCount)
    {
        *ppKITLTestDriver = m_pKITLTestDriver;
        InterlockedIncrement(&m_lRefCount);
    }
    else
    {
        m_pKITLTestDriver = new CKITLTestDriverDev();
        if (!m_pKITLTestDriver)
        {
            hr = E_OUTOFMEMORY;
            FAILLOGV();
            goto Exit;
        }

        // Init fails if another instance of this object is running under a different
        // process instance.  This ensure that only one run is performed at
        // a time
        hr = m_pKITLTestDriver->Init();
        if (FAILED(hr))
        {
            delete m_pKITLTestDriver;
            goto Exit;
        }

        InterlockedIncrement(&m_lRefCount);
        *ppKITLTestDriver = m_pKITLTestDriver;
    }

    hr = S_OK;

Exit:

    return hr;


}

/******************************************************************************
//    Function Name:  CKITLTestDriverDev::Release
//    Description:
//      Deletes this objects and frees any of its related resources if refcount is set to 0
//    Input:
//      None
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDev::Release()
{
    HRESULT hr = E_FAIL;

    if (!m_pKITLTestDriver)
    {
        hr = E_UNEXPECTED;
        FAILLOGV();
        goto Exit;
    }

    if (!InterlockedDecrement(&m_lRefCount))
    {
        // Failure to uninitialize could lead to future CallEdbgRegisterClient failures
        // Make sure this function fails if uninitalize does not complete properly
        hr = m_pKITLTestDriver->Uninit();
        delete m_pKITLTestDriver;
        if (FAILED(hr))
        {
            goto Exit;
        }
    }

    hr = S_OK;

Exit:
    return hr;
}

/******************************************************************************
//    Function Name:  CKITLTestDriverDev::Init
//    Description:
//      Initialize this object by creating a dedicated stream to communicate test settings
//      with the desktop component of this test.
//
//    Input:
//      None
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDev::Init()
{
    HRESULT hr = E_FAIL;
    DWORD   dwRet = 0;
    BOOL    bRet = FALSE;

    // Create an event to signal when the test should terminate prematurely.
    m_hCancelTest = CreateEvent(NULL, TRUE, FALSE, KITL_CANCEL_TEST);
    if (!m_hCancelTest)
    {
        FAILLOGV();
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
    }
    dwRet = GetLastError();
    if (ERROR_ALREADY_EXISTS == dwRet)
    {
        FAILLOG(L"Another instance of KITL Stress is already running");
        hr = HRESULT_FROM_WIN32(dwRet);
        goto Exit;
    }

    // Initialize the pool size as documented on CallEdbgRegisterClient
    UINT uiPoolSize = m_ucWindowSize * 2 * KITL_MTU;

    m_pBufferPool = new UCHAR[uiPoolSize];
    if (!m_pBufferPool)
    {
        FAILLOGV();
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

    // On success, m_uKITLStrmID will never be set to NULL
    // CallEdbgRegisterClient declared without const, so don't
    // worry about const string assignment to non const function
    PREFAST_SUPPRESS( 25048, "External API definition we can't change")
    bRet = CallEdbgRegisterClient(&m_uKITLStrmID,
          KITL_TEST_SETTINGS_ID,
          KITL_CFGFL_STOP_AND_WAIT,
          1,  // Window Size
          m_pBufferPool);

    if (!bRet || !m_uKITLStrmID)
    {
        FAILLOGV();
        hr = E_FAIL;
        goto Exit;
    }

    hr = S_OK;

Exit:
    if (FAILED(hr))
    {
        Uninit();
    }
    return hr;

}

/******************************************************************************
//    Function Name:  CKITLTestDriverDev::Uninit
//    Description:
//      Release resources allocated by this object
//
//    Input:
//      None
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDev::Uninit()
{
    BOOL bRet = FALSE;
    if (m_uKITLStrmID)
    {
        bRet = CallEdbgDeregisterClient(m_uKITLStrmID);
        if (!bRet)
        {
            FAILLOGV();
            FAILLOG(L"KITLStream Uninit failed."\
                L"Please reset the device to recover");
        }
        m_uKITLStrmID = 0;
    }

    if (m_pBufferPool)
    {
        delete [] m_pBufferPool;
        m_pBufferPool = NULL;
    }

    if (m_hCancelTest)
    {
        SetEvent(m_hCancelTest);
        CloseHandle(m_hCancelTest);
        m_hCancelTest = NULL;
    }
    return S_OK;
}


/******************************************************************************
//    Function Name:  CKITLTestDriverDev::SyncSettings
//    Description:
//      Communicates the test settings with the desktop component.
//
//    Input:
//      None
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDev::SyncSettings()
{
    HRESULT hr = E_FAIL;
    BOOL bRet = FALSE;

    if (!m_uKITLStrmID)
    {
        hr = E_INVALIDARG;
        goto Exit;
    }

    // Note that KITL will take care of retransmitting the packet
    // in the event the desktop does not respond in timely fashion
    bRet = CallEdbgSend(m_uKITLStrmID,
            (UCHAR*)(&m_TestSettings),
            sizeof(m_TestSettings));
    if (!bRet)
    {
        FAILLOGV();
        FAILLOG(L"Failed to synchronize test settings' with the desktop");
        hr = E_FAIL;
        goto Exit;
    }

    hr = S_OK;

Exit:
    return hr;
}

/******************************************************************************
//    Function Name:  CKITLTestDriverDev::SetSettings
//    Description:
//      Applies the current test settings and allocates needed resources
//
//    Input:
//      __in TEST_SETTINGS testSettings
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDev::SetSettings(__in const TEST_SETTINGS& testSettings)
{
    HRESULT hr = E_FAIL;

    memset(&m_TestSettings, 0, sizeof(m_TestSettings));

    m_TestSettings = testSettings;
    m_dwThreadCount = testSettings.dwThreadCount;

    m_ppKITLStressClient = new CKITLStressDev*[m_dwThreadCount];
    if (!m_ppKITLStressClient)
    {
        FAILLOGV();
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

    memset(m_ppKITLStressClient, 0, m_dwThreadCount * sizeof(CKITLStressDev*));

    m_phThread = new HTHREAD[m_dwThreadCount];
    if (!m_phThread)
    {
        FAILLOGV();
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

    memset(m_phThread, 0, m_dwThreadCount * sizeof(HANDLE));

    CHAR szStreamName[MAX_SVC_NAMELEN] = "";
    WCHAR wszStreamName[MAX_SVC_NAMELEN] = L"";
    BOOL fUsedDefaultChar = FALSE;


    for (DWORD i = 0; i < m_dwThreadCount; i++)
    {
        hr = StringCchPrintf(
                    wszStreamName,
                    _countof(wszStreamName),
                    L"%S%d",
                    KITL_STREAM_ID,
                    i);
        if (FAILED(hr))
        {
            goto Exit;
        }
        //Converting Unicode to ASCII for all the nw APIs
        int result = WideCharToMultiByte(
                            CP_OEMCP,            //OEM code page
                            0,                    // No flag used
                            wszStreamName,
                            wcslen(wszStreamName),
                            szStreamName,
                            countof(szStreamName),
                            NULL,
                            &fUsedDefaultChar);
        if(result == 0 || fUsedDefaultChar == TRUE)
        {
            hr = E_FAIL;
            FAILLOGV();
            return hr;
        }

        m_ppKITLStressClient[i] = new CKITLStressDev(szStreamName,
            testSettings.dwMinPayLoadSize,
            testSettings.dwMaxPayLoadSize,
            testSettings.ucFlags,
            testSettings.ucWindowSize,
            testSettings.dwRcvIterationsCount,
            testSettings.dwRcvTimeout,
            testSettings.fVerifyRcv ,
            testSettings.dwSendIterationsCount,
            testSettings.cData + static_cast<CHAR>(i),
            testSettings.dwDelay);
        if (!m_ppKITLStressClient[i])
        {
            FAILLOGV();
            hr = E_OUTOFMEMORY;
            goto Exit;
        }
    }

    hr = S_OK;

Exit:
    if (FAILED(hr))
    {
        ClearSettings();
    }
    return hr;
}

/******************************************************************************
//    Function Name:  CKITLTestDriverDev::ClearSettings
//    Description:
//      Free test case release resource and set handle/pointers back to NULL
//
//    Input:
//      None
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDev::ClearSettings()
{
    memset(&m_TestSettings, 0, sizeof(m_TestSettings));

    if (m_ppKITLStressClient)
    {
        for (DWORD i = 0; i < m_dwThreadCount; i++)
        {
            delete m_ppKITLStressClient[i];
            m_ppKITLStressClient[i] = NULL;
        }
    }

    if (m_phThread)
    {
        for (DWORD i = 0; i < m_dwThreadCount; i++)
        {
            if (m_phThread[i])
            {
                CloseHandle(m_phThread[i]);
                m_phThread[i] = NULL;
            }
        }
    }

    m_dwThreadCount = 0;

    delete [] m_phThread;
    m_phThread = NULL;

    delete [] m_ppKITLStressClient;
    m_ppKITLStressClient = NULL;

    return S_OK;
}

/******************************************************************************
//    Function Name:  CKITLTestDriverDev::RunTest
//    Description:
//      This function reads the actual settings to be used to run this test, shares them with
//      the desktop component and then starts the worker threads that will tackle the actual
//      KITL tests.
//
//    Input:
//      __in TEST_SETTINGS testSettings:
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDev::RunTest(__in const TEST_SETTINGS& testSettings, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    HRESULT hr = E_FAIL;
    DWORD dwRet = 0;
    BOOL bRet = FALSE;
    DWORD dwThreadExitCode = 0;
    CKitlPerfLog *lpkitlLog = NULL;
    THREAD_PARAMS** arrThdParam = NULL;

    // Check to see if the device is connected up via KITL
    if (relFRD() == FALSE)
    {
        // no KITL connection
        LOG(L"Device is not attached to Platform Builder. Test is exiting");
        goto Exit;
    }

    // Apply testSettings passed by as argument to the internal state of this object
    hr = SetSettings(testSettings);
    if (FAILED(hr))
    {
        goto Exit;
    }

    LOG(L"TestCase settings' retrieved");

    // Share the test settings with the desktop component
    hr = SyncSettings();
    if (FAILED(hr))
    {
        goto Exit;
    }

    LOG(L"TestCase settings' shared with Desktop component");

    DWORD dwThreadID;
    arrThdParam  = new THREAD_PARAMS*[m_dwThreadCount];
    if (arrThdParam == NULL)
    {
        FAILLOGV();
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

    if (!lpFTE || !lpFTE->lpDescription || !lpFTE->lpDescription[0] )
    {
        FAILLOGV();
        hr = E_FAIL;
        goto Exit;
    }


    PREFAST_SUPPRESS( 6401, TEXT("By design. Non US locale support may be added later\r\n"));
    if ( CompareString(LOCALE_SYSTEM_DEFAULT, NORM_IGNORECASE|NORM_IGNOREKANATYPE|NORM_IGNOREWIDTH,
            g_pShellInfo->szDllCmdLine, -1, TEXT("-BTSPerf"), -1) == CSTR_EQUAL)
    {
        lpkitlLog = new CKitlPerfLog(m_dwThreadCount,
            lpFTE->lpDescription,
            g_pShellInfo->hLib);

        if (lpkitlLog == NULL)
        {
            FAILLOGV();
            hr = E_FAIL;
            goto Exit;
        }
    }


    // Create worker thread that actually perform the tests
    for (DWORD i = 0; i < m_dwThreadCount; i++)
    {
        THREAD_PARAMS* lpThrdParms = new THREAD_PARAMS(m_ppKITLStressClient[i], lpkitlLog);
        ASSERT(lpThrdParms);
        if (lpThrdParms == NULL)
        {
            FAILLOGV();
            hr = E_OUTOFMEMORY;
            goto Exit;
        }

        arrThdParam[i] = lpThrdParms;

        m_phThread[i]= CreateThread(NULL,
                        NULL,
                        CKITLTestDriverDev::KITLThreadProc,
                        lpThrdParms,
                        NULL,
                        &dwThreadID);
        if (!m_phThread[i])
        {
            FAILLOGV();
            hr = E_FAIL;
            goto Exit;
        }
    }

    LOG(L"Worker threads created");

    // Wince Does not fully implement WaitForMultipleObject
    // loop through the handles to work arround it
    for (DWORD i = 0; i < m_dwThreadCount; i++)
    {
        dwRet = WaitForSingleObject(m_phThread[i], INFINITE);
        if (-1 == dwRet)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto Exit;
        }
    }

    for (DWORD i = 0; i < m_dwThreadCount; i++)
    {
        bRet = GetExitCodeThread(m_phThread[i], &dwThreadExitCode);
        if (!bRet)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto Exit;
        }
        // No need to check for STILL_ACTIVE, threads already terminated
        if (FAILED(dwThreadExitCode))
        {
            LOG(L"Thread with handle 0x%x failed with error code 0x%x",
                m_phThread[i],
                dwThreadExitCode);
            hr = dwThreadExitCode;
            goto Exit;
        }
    }

    LOG(L"Test exiting");
    hr = S_OK;

Exit:

    if(lpkitlLog) delete lpkitlLog;

    if(arrThdParam) {
        for (DWORD i = 0; i < m_dwThreadCount; i++)
        {
            delete arrThdParam[i];
        }
        delete []arrThdParam;
    }
    ClearSettings();

    return hr;
}
