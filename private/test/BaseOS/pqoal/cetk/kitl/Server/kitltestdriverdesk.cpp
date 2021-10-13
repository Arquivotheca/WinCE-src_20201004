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
#include <wchar.h>
#include <conio.h>
#include <atlbase.h>
#include <strsafe.h>

#include <conman.h>
#include <conmanlib.h>

#include <Kitlclnt_priv.h>

#include "KITLStressDesk.h"
#include "KITLTestDriverDesk.h"

//Kernel Bootstrap values
//(Can find all the bootstrap values from C:\Users\<username>\AppData\Local\Microsoft\CoreCon\1.0\conman_ds_servicecategory.xsl)
#define CON_ETHERNET_BOOTSTRAP  TEXT("d9a39bb4-f5d2-495d-9020-cc2fbdc4efc7")    //Ethernet connection
#define CON_USB_BOOTSTRAP       TEXT("49e489cc-5bb9-400a-998a-6dc85b0a2f6a")    //USB connection

LONG CKITLTestDriverDesk::m_lRefCount = 0;
CKITLTestDriverDesk *CKITLTestDriverDesk::m_pKITLTestDriver = NULL;

/******************************************************************************
//    Function Name:  CKITLTestDriverDesk::CKITLTestDriverDesk
//    Description:
//        Initializes CKITLTestDriverDesk's member variables.  Note here that the actual
//        instantiation happens on CreateInstance
//    Input:
//        None
//    Output:
//        None
//
******************************************************************************/
CKITLTestDriverDesk::CKITLTestDriverDesk()
{
    m_pBufferPool = NULL;
    m_ppKITLStressDesk = NULL;
    m_phThread = NULL;
    m_hStopTest = NULL;

    m_uKITLStrmID = INVALID_KITLID;
    m_dwThreadCount = 0;
    memset(&m_TestSettings, 0, sizeof(m_TestSettings));
    m_fKitlInitialized = FALSE;
    m_hTerminationThread = NULL;

}

/******************************************************************************
//    Function Name:  CKITLTestDriverDesk::KITLThreadProc
//    Description:
//        Worker thread for CKITLTestDriverDesk
//    Input:
//        __in  LPVOID lpParameter
//    Output:
//        None
//
******************************************************************************/
DWORD WINAPI CKITLTestDriverDesk::KITLThreadProc(
  __in  LPVOID lpParameter
)
{
    HRESULT hr = E_FAIL;

    CKITLStressDesk *pkitlStress = (CKITLStressDesk *) lpParameter;

    hr = pkitlStress->RunTest();

    return hr;
}

/******************************************************************************
//    Function Name:  CKITLTestDriverDesk::CreateInstance
//    Description:
//        Note that this function is NOT Thread safe.  CKITLTestDriverDesk abstracts the
//        creation of multiple threads required to respond to requests coming from the device
//    Input:
//        _deref CKITLTestDriverDesk **ppKITLTestDriver
//          __deref WCHAR *pwszDeviceName
//
//    Output:
//        The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDesk::CreateInstance(__deref CKITLTestDriverDesk **ppKITLTestDriver,
        __deref WCHAR *pwszDeviceName)
{
    HRESULT hr = E_FAIL;

    if (!ppKITLTestDriver || !pwszDeviceName)
    {
        hr = E_INVALIDARG;
        goto Exit;
    }

    if (m_lRefCount)
    {
        *ppKITLTestDriver = m_pKITLTestDriver;
        InterlockedIncrement(&m_lRefCount);
    }
    else
    {
        m_pKITLTestDriver = new CKITLTestDriverDesk();
        if (!m_pKITLTestDriver)
        {
            hr = E_OUTOFMEMORY;
            goto Exit;
        }

        hr = m_pKITLTestDriver->Init(pwszDeviceName);
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
//    Function Name:  CKITLTestDriverDesk::Release
//    Description:
//        Deletes this objects and frees any of its related resources if refcount is set to 0
//    Input:
//        None
//    Output:
//        The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/

HRESULT CKITLTestDriverDesk::Release()
{
    HRESULT hr = E_FAIL;

    if (!m_pKITLTestDriver)
    {
        hr = E_UNEXPECTED;
        goto Exit;
    }

    if (!InterlockedDecrement(&m_lRefCount))
    {
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
//    Function Name:  CKITLTestDriverDesk::Init
//    Description:
//        Initialize this object by creating a dedicated stream to communicate test settings
//        with the desktop component of this test.
//    Input:
//        __deref WCHAR *pwszDeviceName
//    Output:
//        The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDesk::Init(__deref WCHAR *pwszDeviceName)
{
    HRESULT hr = E_FAIL;
    BOOL    bRet = FALSE;
    DWORD dwThreadID = 0;
    DWORD dwRegisterAttempts = 100;
    DWORD dwDefaultTimeout = 200; // in ms

    m_fKitlInitialized = FALSE;

    if (!pwszDeviceName)
    {
        hr = E_INVALIDARG;
        goto Exit;
    }

    hr = StringCchCopyEx(m_wszDeviceName,
        _countof(m_wszDeviceName),
        pwszDeviceName,
        NULL,
        NULL,
        STRSAFE_NULL_ON_FAILURE | STRSAFE_NO_TRUNCATION);
    if (FAILED(hr))
    {
        wprintf(L"Failed to set target device name\n");
        goto Exit;
    }

    m_hStopTest = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (NULL == m_hStopTest)
    {
        hr = E_FAIL;
        goto Exit;
    }

    // Implement a mechanism to unload the test based on user input
    m_hTerminationThread = CreateThread(NULL,
                    NULL,
                    CKITLTestDriverDesk::WaitForTerminate,
                    &m_hStopTest,
                    NULL,
                    &dwThreadID);
    if (!m_hTerminationThread)
    {
        hr = E_FAIL;
        goto Exit;
    }

    m_fKitlInitialized = KITLInitLibrary(REG_KEY_KITL);
    if (!m_fKitlInitialized)
    {
            wprintf (L"Failed to initialize Kitl\n");
            hr = E_FAIL;
            goto Exit;
    }

    wprintf(L"Attempting to connect to: %s\n", m_wszDeviceName);
    wprintf(L"Press q anytime to cancel this test\n");

    // User might want to stop the test so it is important to provide a
    // response to user input in timely fashion
    do
    {
        m_uKITLStrmID = KITLRegisterClient(m_wszDeviceName,
            KITL_TEST_SETTINGS_ID,
            dwDefaultTimeout,
            NULL);
        if (INVALID_KITLID != m_uKITLStrmID)
        {
            break;
        }

        hr = TestDone(&bRet);
        if (FAILED(hr))
        {
            goto Exit;
        }
    } while (--dwRegisterAttempts && !bRet);
    if (INVALID_KITLID == m_uKITLStrmID && !bRet)
    {
        hr = E_FAIL;
        wprintf (L"Failed to register client Kitl: %d\n", GetLastError());
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
//    Function Name:  CKITLTestDriverDesk::Uninit
//    Description:
//        release resources allocated by this object
//    Input:
//        None
//    Output:
//        The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDesk::Uninit()
{
    if (INVALID_KITLID != m_uKITLStrmID)
    {
        KITLDeRegisterClient(m_uKITLStrmID);
        m_uKITLStrmID = INVALID_KITLID;
    }

    if (m_fKitlInitialized)
    {
        KITLDeInitLibrary();
        m_fKitlInitialized = FALSE;
    }

    if (m_pBufferPool)
    {
        delete [] m_pBufferPool;
        m_pBufferPool = NULL;
    }

    if (m_hStopTest)
    {
        SetEvent(m_hStopTest);
        CloseHandle(m_hStopTest);
        m_hStopTest = NULL;
    }
    if (m_hTerminationThread)
    {
        WaitForSingleObject(m_hTerminationThread, INFINITE);
        CloseHandle(m_hTerminationThread);
        m_hTerminationThread = NULL;
    }
    return S_OK;
}


/******************************************************************************
//    Function Name:  CKITLTestDriverDesk::SyncSettings
//    Description:
//        Communicates the test settings with the desktop component.
//    Input:
//        None
//    Output:
//        The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDesk::SyncSettings()
{
    HRESULT hr = E_FAIL;
    BOOL bRet = FALSE;
    BOOL fRet = FALSE;
    DWORD dwReceiveAttempts = 10000;
    DWORD dwDefaultTimeout = 200; // in ms
    DWORD dwBuffSize = 0;

    wprintf(L"\n************************************************\n");
    wprintf(L"Waiting for next test to run\n");
    wprintf(L"press q to force this application to exit\n");
    wprintf(L"************************************************\n");

    do
    {
        dwBuffSize = sizeof(m_TestSettings);

        // Note here that it is up to KITL to retransmit packets
        bRet = KITLRecv(m_uKITLStrmID,
            &m_TestSettings,
            &dwBuffSize,
            dwDefaultTimeout);
        if (bRet)
        {
            break;
        }

        hr = TestDone(&fRet);
        if (FAILED(hr))
        {
            goto Exit;
        }
    } while (--dwReceiveAttempts && !fRet);
    if (fRet)
    {
        wprintf(L"Test Setting's synchronization aborted by the user\n");
        hr = E_FAIL;
        goto Exit;
    }
    else if (!dwReceiveAttempts)
    {
        wprintf(L"Desktop did  not receive settings from device in timely fashion\n");
        hr = E_FAIL;
        goto Exit;
    }
    else if (!bRet)
    {
        wprintf(L"Error hit when receiving packets\n");
        hr = E_FAIL;
        goto Exit;
    }
    if (dwBuffSize != sizeof(m_TestSettings))
    {
        wprintf(L"Unexpected settings received\n");
        hr = E_FAIL;
        goto Exit;
    }

    hr = S_OK;

Exit:
    return hr;
}

/******************************************************************************
//    Function Name:  CKITLTestDriverDesk::SetSettings
//    Description:
//        Applies the curret test settings and allocates needed resources
//    Input:
//        __in TEST_SETTINGS testSettings
//    Output:
//        The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDesk::SetSettings(__in TEST_SETTINGS testSettings)
{
    HRESULT hr = E_FAIL;

    memset(&m_TestSettings, 0, sizeof(m_TestSettings));

    m_TestSettings = testSettings;
    m_dwThreadCount = testSettings.dwThreadCount;

    m_ppKITLStressDesk = new CKITLStressDesk*[m_dwThreadCount];
    if (!m_ppKITLStressDesk)
    {
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

    memset(m_ppKITLStressDesk, 0, m_dwThreadCount * sizeof(CKITLStressDesk*));

    m_phThread = new HANDLE[m_dwThreadCount];
    if (!m_phThread)
    {
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

    memset(m_phThread, 0, m_dwThreadCount * sizeof(HANDLE));

    WCHAR szStreamName[MAX_SVC_NAMELEN] = L"";

    for (DWORD i = 0; i < m_dwThreadCount; i++)
    {
        hr = StringCchPrintf(szStreamName,
            _countof(szStreamName),
            L"%s%d", KITL_STREAM_ID, i);
        if (FAILED(hr))
        {
            goto Exit;
        }

        m_ppKITLStressDesk[i] = new CKITLStressDesk(szStreamName,
            m_wszDeviceName,
            testSettings.dwMinPayLoadSize,
            testSettings.dwMaxPayLoadSize,
            testSettings.ucFlags,
            testSettings.ucWindowSize,
            testSettings.dwRcvIterationsCount,
            testSettings.dwRcvTimeout,
            testSettings.fVerifyRcv ,
            testSettings.dwSendIterationsCount,
            testSettings.cData + static_cast<CHAR>(i),
            testSettings.dwDelay,
            m_hStopTest);
        if (!m_ppKITLStressDesk[i])
        {
            hr = E_OUTOFMEMORY;
            goto Exit;
        }
    }

    hr = S_OK;

Exit:
    if (FAILED(hr))
    {
        wprintf(L"Failed to set test setings 0x%x\n", hr);
        ClearSettings();
    }
    return hr;
}

/******************************************************************************
//    Function Name:  CKITLTestDriverDesk::ClearSettings
//    Description:
//        Free test case release resource and set handle/pointers back to NULL
//    Input:
//        None
//    Output:
//        The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDesk::ClearSettings()
{
    if (m_ppKITLStressDesk)
    {
        for (DWORD i = 0; i < m_dwThreadCount; i++)
        {
            delete m_ppKITLStressDesk[i];
            m_ppKITLStressDesk[i] = NULL;
        }
    }

    delete [] m_ppKITLStressDesk;
    m_ppKITLStressDesk = NULL;


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

    delete [] m_phThread;
    m_phThread = NULL;

    m_dwThreadCount = 0;
    memset(&m_TestSettings, 0, sizeof(m_TestSettings));


    return S_OK;
}

/******************************************************************************
//    Function Name:  CKITLTestDriverDesk::RunTest
//    Description:
//
//    Input:
//      None
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDesk::RunTest()
{
    HRESULT hr = E_FAIL;
    DWORD dwRet = 0;
    DWORD dwThreadID = NULL;

    hr = SyncSettings();
    if (FAILED(hr))
    {
        goto Exit;
    }

    hr = SetSettings(m_TestSettings);
    if (FAILED(hr))
    {
        goto Exit;
    }

    wprintf(L"Starting worker threads\n");

    for (DWORD i = 0; i < m_dwThreadCount; i++)
    {
        m_phThread[i]= CreateThread(NULL,
                        NULL,
                        CKITLTestDriverDesk::KITLThreadProc,
                        m_ppKITLStressDesk[i],
                        NULL,
                        &dwThreadID);
        if (!m_phThread[i])
        {
            goto Exit;
        }
    }

    wprintf(L"worker threads started\n");

    wprintf(L"Waiting for worker threads to exit\n");
    for (DWORD i = 0; i < m_dwThreadCount; i++)
    {
        dwRet = WaitForSingleObject(m_phThread[i], INFINITE);
        if (-1 == dwRet)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto Exit;
        }
    }

    wprintf(L"************************************************\n");
    wprintf(L"           Worker threads exited\n");
    wprintf(L"************************************************\n");

    // Purposely ignore thread exit code
    // Continue to trying to communicate with the device
    hr = S_OK;

Exit:
    ClearSettings();

    return hr;
}


/******************************************************************************
//    Function Name:  CKITLTestDriverDesk::TestDone
//    Description:
//      This function checks whether the test is test was stopped by the user
//
//    Input:
//        __deref BOOL *pfRet:
//
//    Output:
//          The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLTestDriverDesk::TestDone(__deref BOOL *pfRet)
{
    HRESULT hr = E_FAIL;
    DWORD dwRet = 0;
    if (!pfRet)
    {
        hr = E_INVALIDARG;
        goto Exit;
    }
    dwRet = WaitForSingleObject(m_hStopTest, 0);
    if (WAIT_OBJECT_0 == dwRet)
    {
        *pfRet = TRUE;
    }
    else if (WAIT_TIMEOUT == dwRet)
    {
        pfRet = FALSE;
    }
    else
    {
        goto Exit;
    }

    hr = S_OK;

Exit:
    return hr;
}

/******************************************************************************
//    Function Name:  CKITLTestDriverDesk::WaitForTerminate
//    Description:
//      Reads user input and instruct the application to unload if instructed to do so by the user
//
//    Input:
//        __inout_opt LPVOID lpVoid: Always null
//
//    Output:
//          The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
DWORD WINAPI CKITLTestDriverDesk::WaitForTerminate(__in LPVOID lpvArg)
{
    HRESULT hr = E_FAIL;
    int iPeriodToWait = 300;

    // get handle to terminate event
    HANDLE hTerminate = NULL;

    if (!lpvArg)
    {
        return hr;
    }

    hTerminate = *(reinterpret_cast<HANDLE *> (lpvArg));

    // loop, and check for early termination
    do
    {
        if (0 == _kbhit())
        {
            continue;
        }

        char c = (char)tolower(_getch());
        if ('q' == c)
        {
            // signal a halt to all threads
            wprintf(L"Test Exiting per user input\n");
            ::SetEvent(hTerminate);
            break;
        }
    }
    while (WAIT_TIMEOUT == WaitForSingleObject(hTerminate, iPeriodToWait));

    hr = S_OK;

    /////////////////////////////////////////////////////////////////////
    // clean up
    if (NULL != hTerminate)
    {
        CloseHandle(hTerminate);
    }

    return hr;
}

/******************************************************************************
//    Function Name:  GetBootMeName
//    Description:
//      Converts the logical device name to it's corresponding BootMe name
//
//    Input:
//        LPCWSTR pszDeviceName: Logical device name
//        LPWSTR pszBootMeName: Updates the BootMe name
//
//    Return:
//          BOOL - TRUE for success and FALSE for failure
//
******************************************************************************/
BOOL GetBootMeName(LPCWSTR pszDeviceName, LPWSTR pszBootMeName)
{
    HRESULT hr = S_OK;

    CComPtr<IConMan> * piConMan = NULL;
    ICcDatastore * piDatastore = NULL;

    ICcDeviceContainer * piDeviceContainer = NULL;
    ICcPlatformContainer * piPlatformContainer = NULL;
    ICcObjectContainer * piObjContainer = NULL, * piPropObjContainer1 = NULL, * piPropObjContainer2 = NULL, * piPlatObjContainer = NULL;
    ICcCollection * piCollection = NULL;
    ICcObject * piObj = NULL, * piObj2 = NULL, * piProperty1 = NULL, * piPropObj2 = NULL;
    ICcPropertyContainer * piPropContainer1 = NULL, * piPropContainer2 = NULL;
    ICcProperty * piProperty2 = NULL;

    long lCount = 0, lPlatCount = 0, lLoop = 0;
    int iVal = 0;
    BSTR bstrID = NULL, bstrBootMe = NULL, bstrConnectionType = NULL;
    BOOL bRet = FALSE, bFound = FALSE;
    CComBSTR bstrEthKern(L"Ethernet Kernel"), bstrDevice(L"Device Name");
    CComBSTR bstrDeviceName(pszDeviceName);
    CComBSTR bstrKernelBootstrap(L"f975542a-3d12-4750-aaac-81c97b5a23de");
    WCHAR wszConnectionType[MAX_PATH] = L"";

    //If COM initialization fails, return with a failure.
    hr = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if(FAILED(hr))
        return bRet;

    piConMan = new CComPtr<IConMan>;
    hr = ::CoCreateInstance(__uuidof( ConMan ), NULL, CLSCTX_ALL, __uuidof( IConMan ),
        (void **)piConMan);

    if(FAILED(hr))
        goto Cleanup;

    hr = (*piConMan)->get_Datastore(&piDatastore);
    if(FAILED(hr) || !piDatastore)
        goto Cleanup;

    hr = piDatastore->get_DeviceContainer(&piDeviceContainer);
    if(FAILED(hr) || !piDeviceContainer)
        goto Cleanup;


    hr = piDeviceContainer->QueryInterface(&piObjContainer);
    if(FAILED(hr) || !piObjContainer)
        goto Cleanup;


    hr = piObjContainer->EnumerateObjects(&piCollection);
    if(FAILED(hr) || !piCollection)
        goto Cleanup;


    hr = piCollection->get_Count(&lCount);
    if(FAILED(hr) || lCount <= 0)
        goto Cleanup;

    //We need to GUID ID to find out the corresponding BootMe name.
    //We iterate through all the existing devices added to the datastore
    //and find the GUID for the provided DeviceName.
    while((lLoop < lCount) && (bFound == FALSE))
    {
        hr = piCollection->get_Item(lLoop, &piObj);
        if(FAILED(hr) || !piObj)
            break;

        BSTR bstrName = NULL;;

        hr = piObj->get_Name(&bstrName);
        if(FAILED(hr))
            break;

        if(!wcscmp(bstrName, bstrDeviceName))
        {
            hr = piObj->get_ID(&bstrID);
            if(FAILED(hr))
                break;

            bFound = TRUE;
        }

        if(bstrName)
        {
            SysFreeString(bstrName);
        }

        lLoop++;
    }

    //If not found in device container, verify for SDK associated devices in platform container
    if(!bFound)
    {
        //Reset values
        SAFE_RELEASE(piObjContainer);
        SAFE_RELEASE(piCollection);
        SAFE_RELEASE(piObj);

        hr = piDatastore->get_PlatformContainer(&piPlatformContainer);
        if(FAILED(hr) || !piPlatformContainer)
            goto Cleanup;


        hr = piPlatformContainer->QueryInterface(&piObjContainer);
        if(FAILED(hr) || !piObjContainer)
            goto Cleanup;


        hr = piObjContainer->EnumerateObjects(&piCollection);
        if(FAILED(hr) || !piCollection)
            goto Cleanup;


        hr = piCollection->get_Count(&lPlatCount);
        if(FAILED(hr) || lPlatCount <= 0)
            goto Cleanup;

        for(int i = 0; (i < lPlatCount) && (bFound == FALSE); ++i)
        {
            lCount = 0;
            lLoop = 0;

            ICcDeviceContainer * pPlatDeviceContainer = NULL;
            ICcCollection * pPlatCollection = NULL;
            ICcPlatform * pPlatform = NULL;
            ICcObject * pPlatObj = NULL;

            hr = piCollection->get_Item(i, &pPlatObj);
            if(FAILED(hr) || !pPlatObj)
                break;

            pPlatObj->QueryInterface(&pPlatform);
            if(FAILED(hr) || !pPlatform)
                break;

            hr = pPlatform->get_DeviceContainer(&pPlatDeviceContainer);
            if(FAILED(hr) || !pPlatDeviceContainer)
                break;

            hr = pPlatDeviceContainer->QueryInterface(&piPlatObjContainer);
            if(FAILED(hr) || !piPlatObjContainer)
                break;

            hr = piPlatObjContainer->EnumerateObjects(&pPlatCollection);
            if(FAILED(hr) || !pPlatCollection)
                break;

            hr = pPlatCollection->get_Count(&lCount);
            if(FAILED(hr) || lCount <= 0)
                goto Cleanup;


            while((lLoop < lCount) && (bFound == FALSE))
            {
                hr = pPlatCollection->get_Item(lLoop, &piObj);
                if(FAILED(hr) || !piObj)
                    break;

                BSTR bstrName = NULL;

                hr = piObj->get_Name(&bstrName);
                if(FAILED(hr))
                    break;

                if(!wcscmp(bstrName, bstrDeviceName))
                {
                    hr = piObj->get_ID(&bstrID);
                    if(FAILED(hr))
                        break;

                    bFound = TRUE;
                }

                if(bstrName)
                {
                    SysFreeString(bstrName);
                }

                lLoop++;
            }

            SAFE_RELEASE(pPlatObj);
            SAFE_RELEASE(pPlatform);
            SAFE_RELEASE(pPlatCollection);
            SAFE_RELEASE(pPlatDeviceContainer);
        }

        if(bFound)
        {
            hr = piPlatObjContainer->FindObject(bstrID, &piObj2);
            if(FAILED(hr) || !piObj2)
                goto Cleanup;
        }
        else
            goto Cleanup;
    }
    else
    {
        //The GUID for the provided DeviceName is found, the BootMe
        //name can be identified using this GUID.
        hr = piObjContainer->FindObject(bstrID, &piObj2);
        if(FAILED(hr) || !piObj2)
            goto Cleanup;
    }


    hr = piObj2->get_PropertyContainer(&piPropContainer1);
    if(FAILED(hr) || !piPropContainer1)
        goto Cleanup;

    hr = piPropContainer1->QueryInterface(&piPropObjContainer1);
    if(FAILED(hr) || !piPropObjContainer1)
        goto Cleanup;

    //Determine what kind of connection it is. (eg. Ethernet, USB, etc.)
    hr = piPropObjContainer1->FindObject(bstrKernelBootstrap, &piProperty1);
    if(FAILED(hr) || !piProperty1)
        goto Cleanup;

    hr = piProperty1->QueryInterface(&piProperty2);
    if(FAILED(hr) || !piProperty2)
        goto Cleanup;

    hr = piProperty2->get_Value(&bstrConnectionType);
    if(FAILED(hr))
        goto Cleanup;

    iVal = wcscpy_s(wszConnectionType, MAX_PATH, bstrConnectionType);
    if(iVal)
        goto Cleanup;

    //Determine BootMe name based on connection type
    if(!_wcsicmp(wszConnectionType, CON_ETHERNET_BOOTSTRAP))
    {
        //Ethernet connection.
        SAFE_RELEASE(piProperty1);
        SAFE_RELEASE(piProperty2);

        hr = piPropObjContainer1->FindObject(bstrEthKern, &piProperty1);
        if(FAILED(hr) || !piProperty1)
            goto Cleanup;

        hr = piProperty1->get_PropertyContainer(&piPropContainer2);
        if(FAILED(hr) || !piPropContainer1)
            goto Cleanup;

        hr = piPropContainer2->QueryInterface(&piPropObjContainer2);
        if(FAILED(hr) || !piPropObjContainer2)
            goto Cleanup;

        hr = piPropObjContainer2->FindObject(bstrDevice, &piPropObj2);
        if(FAILED(hr) || !piPropObj2)
            goto Cleanup;

        hr = piPropObj2->QueryInterface(&piProperty2);
        if(FAILED(hr) || !piProperty2)
            goto Cleanup;

        hr = piProperty2->get_Value(&bstrBootMe);
        if(FAILED(hr))
            goto Cleanup;

        iVal = wcscpy_s(pszBootMeName, MAX_PATH, bstrBootMe);
        if(iVal == 0)
            bRet = TRUE;
    }
    else if(!_wcsicmp(wszConnectionType, CON_USB_BOOTSTRAP))
    {
        //USB connection.
        iVal = wcscpy_s(pszBootMeName, MAX_PATH, L"USBDevice");
        if(iVal == 0)
            bRet = TRUE;
    }
    else
    {
        //Other connection types. Can be added in the future.
        wprintf(L"Connection type not currently supported.\n");
        goto Cleanup;
    }

    //Cleanup
Cleanup:

    SAFE_RELEASE(piProperty2);
    SAFE_RELEASE(piPropObj2);
    SAFE_RELEASE(piPropObjContainer2);
    SAFE_RELEASE(piPropContainer2);
    SAFE_RELEASE(piProperty1);
    SAFE_RELEASE(piPropObjContainer1);
    SAFE_RELEASE(piPropContainer1);
    SAFE_RELEASE(piObj2);
    SAFE_RELEASE(piObj);
    SAFE_RELEASE(piCollection);
    SAFE_RELEASE(piObjContainer);
    SAFE_RELEASE(piPlatObjContainer);
    SAFE_RELEASE(piDeviceContainer);
    SAFE_RELEASE(piPlatformContainer);
    SAFE_RELEASE(piDatastore);

    if(piConMan)
    {
        piConMan->Release();
        delete piConMan;
        piConMan = NULL;
    }

    if(bstrID)
    {
        SysFreeString(bstrID);
    }

    if(bstrBootMe)
    {
        SysFreeString(bstrID);
    }

    if(bstrConnectionType)
    {
        SysFreeString(bstrConnectionType);
    }

    ::CoUninitialize();

    return bRet;
}

int wmain (int argc, WCHAR *argv[])
{
    HRESULT hr = E_FAIL;
    CKITLTestDriverDesk *pTestDriver = NULL;
    BOOL fRet = FALSE, bCheck = FALSE;
    WCHAR wszDeviceName[MAX_PATH] = L"";
    WCHAR wszBootMeName[MAX_PATH] = L"";
    HANDLE hMutex = NULL;

    if(3 == argc)
    {
        if (!_wcsicmp(argv[1], L"-d"))
        {
            hr = StringCchCopyEx(wszDeviceName,
                _countof(wszDeviceName),
                argv[2],
                NULL,
                NULL,
                STRSAFE_NULL_ON_FAILURE | STRSAFE_NO_TRUNCATION);

            if (FAILED(hr))
            {
                wprintf(L"Failed to set target device name\n");
                goto Exit;
            }

            //Convert to BootMe name
            bCheck = GetBootMeName(wszDeviceName, wszBootMeName);

            if(!bCheck)
                wprintf(L"Unable to detect BootMe name.\n");
        }
        else if (!_wcsicmp(argv[1], L"-b"))
        {
            hr = StringCchCopyEx(wszBootMeName,
                _countof(wszBootMeName),
                argv[2],
                NULL,
                NULL,
                STRSAFE_NULL_ON_FAILURE | STRSAFE_NO_TRUNCATION);

            if (FAILED(hr))
            {
                wprintf(L"Failed to set target device name\n");
                goto Exit;
            }

            bCheck = TRUE;
        }
        else
        {
            wprintf(L"Invalid parameter passed.\n");
        }
    }
    else
    {
        wprintf(L"Invalid number of parameters passed.\n");
    }

    //Error occured - invalid no. of parameters or incorrect parameters or Device name conversion to BootMe name failed.
    //Get BootMe name from user to proceed with tests.
    if(!bCheck)
    {
        printf(
        "\nCommand-line options:\n\n"
        "Usage: KITLSrv [-(d|b)] [Name]\n\n"
        "-d \t Device Name as input \n"
        "-b \t BootMe Name as input \n\n");

        wprintf(L"Enter BootMe name to proceed: ");
        int iRes = wscanf_s( L"%s", wszBootMeName, MAX_PATH);
        if(!iRes)
        {
            wprintf(L"Error: Invalid input.\n");
            goto Exit;
        }
    }

    hMutex = CreateMutex(NULL, TRUE, wszBootMeName);
    if (!hMutex || ERROR_ALREADY_EXISTS == GetLastError())
    {
        goto Exit;
    }

    // Increment ref count
    hr = CKITLTestDriverDesk::CreateInstance(&pTestDriver,
                    wszBootMeName);
    if (FAILED(hr))
    {
        goto Exit;
    }

    // Start the test
    do
    {
        hr = pTestDriver->RunTest();
        if (FAILED(hr))
        {
            goto Exit;
        }

        hr = pTestDriver->TestDone(&fRet);
        if (FAILED(hr))
        {
            goto Exit;
        }
    } while (!fRet);

Exit:
    if (pTestDriver)
    {
        hr = pTestDriver->Release();
    }

    if (hMutex)
    {
        ReleaseMutex(hMutex);
    }

    wprintf(L"KITLSrv Unloading\n");

    return 0;
}

