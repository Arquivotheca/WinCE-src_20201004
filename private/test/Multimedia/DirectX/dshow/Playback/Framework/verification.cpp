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
////////////////////////////////////////////////////////////////////////////////
//
//  Verifier objects
//
//
////////////////////////////////////////////////////////////////////////////////

#include "logging.h"
#include "globals.h"
#include "TestDesc.h"
#include "TestGraph.h"
#include "PinEnumerator.h"
#include "StringConversions.h"
#include "utility.h"
#include "Verification.h"
#include "strsafe.h"


CGraphVerifier::CGraphVerifier()
{
    // Initialize Configuration-based member variables
    m_szName = NULL;
    m_hExit = NULL;
#ifdef UNDER_CE
    m_hMQIN = NULL;
    m_hMQOUT = NULL;
#endif
    m_hThread = NULL;
    m_szName = NULL;
    m_Location = VLOC_LOCAL;
    m_Type = VerificationEndMarker;
    
    // Initialize our flag indicating whether or not verification was successful.  This will *NOT* be reset or altered in the base class. It's up to the derived
    // class to manage this variable.
    m_hr = S_OK;

    // Initialialize state member variables.
    m_bEOS = false;
    m_bFirstSample = false;
    m_llFirstSampleTime = 0;
    m_llEOSTime = 0;
    m_llSetStart = 0;
    m_llSetStop = 0;
    m_dSetRate = 1.0;
}

CGraphVerifier::~CGraphVerifier()
{
    if(m_szName)
        delete[] m_szName;

    if(m_Location == VLOC_SERVER)
    {
        // Tell the message pump to stop
        SignalExit();

        // Wait for the pump to exit.
        if(WAIT_OBJECT_0 != WaitForSingleObject(m_hThread, VERIFICATION_EXIT_TIMEOUT_MS))
        {
            // We don't want to block forever, but I'm unsure if we want to forcibly terminate.
            // Initial thought is to do nothing in this case and let the OS close out the thread when 
            // quartz.dll unloads and the process terminates.  Unfortunately, in the case of running in the remote chamber,
            // the server process for quartz is never going to terminate, so the thread would still run forever.

            // For now, doing nothing with the hope that we don't run into this since there isn't a good workaround (TerminateThread could cause deadlocks).
        }

        if(m_hExit)
            CloseHandle(m_hExit);
    }

    if(m_Location == VLOC_CLIENT)
    {
#ifdef UNDER_CE 
        if(m_hMQIN)
        {
            LOG(TEXT("%S: Closing CLIENT incoming QUEUE"), __FUNCTION__);
            if(!CloseMsgQueue(m_hMQIN))
            {
                HRESULT hr = HRESULT_FROM_WIN32 (GetLastError());
                LOG(_T("Failed with hr=%x"), hr);
            }
        }
        

        if(m_hMQOUT)
        {
            LOG(TEXT("%S: Closing CLIENT outgoing QUEUE"), __FUNCTION__);
            if(!CloseMsgQueue(m_hMQOUT))
            {
                HRESULT hr = HRESULT_FROM_WIN32 (GetLastError());
                LOG(_T("Failed with hr=%x"), hr);
            }
        }
#endif 
    }  
    
}

HRESULT CGraphVerifier::Init(void* pOwner, VerificationType type, VerificationLocation location, TCHAR* szName, void *pVerificationData)
{
    HRESULT hr = S_OK;
    size_t pcch = 0;
    WCHAR* pszOutName = NULL, *pszInName = NULL;
    
    m_Location = location;
    m_Type = type;

    HANDLE hThreadCreated = NULL;

    if(szName)
    {
        if(FAILED(hr = StringCchLength(szName, TEST_MAX_PATH - 16, &pcch)))
            goto cleanup;

        if((m_szName = new TCHAR[pcch + 1]) == 0)
        {
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }

        if(FAILED(hr = StringCchCopy(m_szName, pcch + 1, szName)))
            goto cleanup;
    }
    else
        hr = E_INVALIDARG;

    // Create the message pump if this is a verifier attached to a remote filter graph
    if(location == VLOC_SERVER)
    {
        // Initialize the event we will use to stop the pump
        if((m_hExit = CreateEvent(NULL, false, false, NULL)) == 0)
        {
            hr = E_FAIL;
            goto cleanup;
        }
        
        if((m_hStarted = CreateEvent(NULL, false, false, NULL)) == 0)
        {
            hr = E_FAIL;
            goto cleanup;
        }
        
        // Start the pump
        if((m_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(VerifierThreadProc), this, 0, NULL)) == 0)
        {
            hr = E_FAIL;
            goto cleanup;
        }
    
        if(WAIT_OBJECT_0 != WaitForSingleObject(m_hStarted, VERIFICATION_START_TIMEOUT_MS))
        {
            hr = E_FAIL;
            goto cleanup;
        }
    }

    // Create the queues if this is a client verifier that talks to a verifier attached to a remote filter graph
    if(location == VLOC_CLIENT)
    {
#ifdef UNDER_CE        
        MSGQUEUEOPTIONS mqo_in, mqo_out;
        if((pszInName = new WCHAR[pcch + 16]) == 0)
        {
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }

        if(FAILED(hr = StringCbCopy(pszInName, (pcch + 16) * sizeof(WCHAR) ,  m_szName)))
        goto cleanup;
    
        if(FAILED(hr = StringCbCatW(pszInName, (pcch + 16) * sizeof(WCHAR), L"- from remote")))
            goto cleanup;

        if((pszOutName = new WCHAR[pcch + 16]) == 0)
        {
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }

        if(FAILED(hr = StringCbCopy(pszOutName, (pcch + 16) * sizeof(WCHAR) ,  m_szName)))
            goto cleanup;
        
        if(FAILED(hr = StringCbCatW(pszOutName, (pcch + 16) * sizeof(WCHAR), L"- to remote")))
            goto cleanup;
        
        mqo_in.dwSize = sizeof(MSGQUEUEOPTIONS);
        mqo_in.dwFlags = MSGQUEUE_ALLOW_BROKEN | MSGQUEUE_NOPRECOMMIT;     
        mqo_in.dwMaxMessages = 1;                   // 1 message allowed in the queue at a time (synchronous IO)
        mqo_in.cbMaxMessage = MAX_VERIFICATION_MESSAGE_SIZE;           
        mqo_in.bReadAccess = TRUE;    

        mqo_out.dwSize = sizeof(MSGQUEUEOPTIONS);
        mqo_out.dwFlags = MSGQUEUE_ALLOW_BROKEN | MSGQUEUE_NOPRECOMMIT;      
        mqo_out.dwMaxMessages = 1;                   // 1 message allowed in the queue at a time (synchronous IO)
        mqo_out.cbMaxMessage = MAX_VERIFICATION_MESSAGE_SIZE;           
        mqo_out.bReadAccess = FALSE;  

        LOG(TEXT("%S: Client Connecting to Input Queue %s"), __FUNCTION__, pszInName);
        if((m_hMQIN = CreateMsgQueue(pszInName, &mqo_in)) == 0)
        {    
            hr = HRESULT_FROM_WIN32 (GetLastError());
            LOG(_T("Failed with hr=%x"), hr);    
            goto cleanup;
        }
        LOG(_T("Succeeded"));

        LOG(TEXT("%S: Client Connecting to Output Queue %s"), __FUNCTION__, pszOutName);
        if((m_hMQOUT = CreateMsgQueue(pszOutName, &mqo_out)) == 0)
        {
            hr = HRESULT_FROM_WIN32 (GetLastError());
            LOG(_T("Failed with hr=%x"), hr);
            goto cleanup;
        }
        LOG(_T("Succeeded"));
#endif
    }

cleanup:

    if(pszInName)
        delete[] pszInName;

    if(pszOutName)
        delete[] pszOutName;
    
    return hr;
}



HRESULT CGraphVerifier::GetResult(VerificationType type, void *pVerificationData, DWORD* pdwSize)
{
    HRESULT hr = S_OK;
    VerificationRequest* pRequest = NULL;
    VerificationResponse* pResponse = NULL;
    DWORD dwBytesRead = 0;
    DWORD dwFlags = 0;
    DWORD dwResponseSize = 0;

    if(m_Location == VLOC_CLIENT)
    {
#ifdef UNDER_CE
        // Create the request
        if((pRequest = (VerificationRequest*)new BYTE[sizeof(VerificationRequest)]) == 0)
        {
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }

        pRequest->cbDataSize = 0;
        pRequest->pData = NULL;
        pRequest->command = VCMD_GETRESULT;

        // Create the response buffer
        dwResponseSize = sizeof(VerificationResponse);

        if(pVerificationData && pdwSize && *pdwSize)
            dwResponseSize += *pdwSize;
            
        if((pResponse = (VerificationResponse*) new BYTE[dwResponseSize]) == 0)
        {
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }

        // Send the request to the remote verifier
        if(!WriteMsgQueue(m_hMQOUT, 
                         (void*)(pRequest), 
                         sizeof(VerificationRequest) + pRequest->cbDataSize, 
                         0, 
                         0))
        {
            hr = HRESULT_FROM_WIN32 (GetLastError());
            goto cleanup;
        }

        // wait for a response
        if(!ReadMsgQueue(m_hMQIN, 
                         (void*)(pResponse), 
                         dwResponseSize, 
                         &dwBytesRead, 
                         VERIFICATION_OPERATION_TIMEOUT_MS, 
                         &dwFlags))
        {
            hr = HRESULT_FROM_WIN32 (GetLastError());
            goto cleanup;
        }

        // Check to see if the response is valid
        if(pdwSize && dwBytesRead != (sizeof(VerificationResponse) + pResponse->cbDataSize))
        {
            hr = E_FAIL;
            goto cleanup;
        }

        // Return the relevant data
        hr = pResponse->hr;

        if(pVerificationData && pdwSize && *pdwSize)
        {
            pResponse->pData = (BYTE*)pResponse + sizeof(VerificationResponse);  // Fix data pointer to current process address space
            memcpy(pVerificationData, pResponse->pData, pResponse->cbDataSize);
        }
#endif         
    }
    else
        hr = S_OK;
       

cleanup:
    if(pResponse)
        delete[] (BYTE*)pResponse;

    if(pRequest)
        delete[] (BYTE*)pRequest;
    return hr;
}
  


HRESULT CGraphVerifier::ProcessEvent(GraphEvent event, void* pEventData)
{    
    HRESULT hr = S_OK;
    GraphEndOfStream *pEOS = NULL;
    GraphSample         *pSample = NULL;
    GraphNewSegment *pNewSegment = NULL;

    if(m_Location == VLOC_CLIENT)
    {
        // Initiating a ProcessEvent call from a test client is currently not supported.  Is this even something that a test would likely do?
        // Normally ProcessEvent is triggered by an event happening within the tap filter...
        hr = E_NOTIMPL;
        goto cleanup;
    }
    else
    {
        CAutoLock cLock(&m_CritSec);
        
        switch ( event )
        {
        case SAMPLE: 
            if(m_bFirstSample)
                break;
            pSample = (GraphSample *)pEventData;
            if ( pSample )
            {
                m_bFirstSample = true;
                m_llFirstSampleTime = pSample->ats;
            }
            break;
        case EOS:
            pEOS = (GraphEndOfStream *)pEventData;
            if(pEOS)
            {
                m_bEOS = true;
                m_llEOSTime = pEOS->ats;
            }
            break;
        case NEW_SEG:
            pNewSegment = (GraphNewSegment *)pEventData;
            if(pNewSegment)
            {
                m_llSetStart = pNewSegment->start;
                m_llSetStop = pNewSegment->stop;
                m_dSetRate = pNewSegment->rate;
            }
            break;
        default:
            break;
        }
    }
 
cleanup:

    return hr;
}

HRESULT CGraphVerifier::Start()
{
    HRESULT hr = S_OK;
    VerificationRequest* pRequest = NULL;
    VerificationResponse vResponse;
    DWORD dwBytesRead = 0;
    DWORD dwFlags = 0;
    
    if(m_Location == VLOC_CLIENT)
    {
#ifdef UNDER_CE
        // Create the request
        if((pRequest = (VerificationRequest*)new BYTE[sizeof(VerificationRequest)]) == 0)
        {
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }

        pRequest->cbDataSize = 0;
        pRequest->pData = NULL;
        pRequest->command = VCMD_START;

        // Send the request to the remote verifier
        if(!WriteMsgQueue(m_hMQOUT, 
                         (void*)(pRequest), 
                         sizeof(VerificationRequest) +pRequest->cbDataSize, 
                         0, 
                         0))
        {
            hr = HRESULT_FROM_WIN32 (GetLastError());
            goto cleanup;
        }

        // wait for a response
        if(!ReadMsgQueue(m_hMQIN, 
                         (void*)(&vResponse), 
                         sizeof(VerificationResponse), 
                         &dwBytesRead, 
                         VERIFICATION_OPERATION_TIMEOUT_MS, 
                         &dwFlags))
        {
            hr = HRESULT_FROM_WIN32 (GetLastError());
            goto cleanup;
        }

        hr = vResponse.hr;
#endif        
    }

cleanup:

    return hr;
    
}

HRESULT CGraphVerifier::Stop()
{
    HRESULT hr = S_OK;
    VerificationRequest* pRequest = NULL;
    VerificationResponse vResponse;
    DWORD dwBytesRead = 0;
    DWORD dwFlags = 0;
    
    if(m_Location == VLOC_CLIENT)
    {
#ifdef UNDER_CE
        // Create the request
        if((pRequest = (VerificationRequest*)new BYTE[sizeof(VerificationRequest)]) == 0)
        {
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }

        pRequest->cbDataSize = 0;
        pRequest->pData = NULL;
        pRequest->command = VCMD_STOP;


        // Send the request to the remote verifier
        if(!WriteMsgQueue(m_hMQOUT, 
                         (void*)(pRequest), 
                         sizeof(VerificationRequest) + pRequest->cbDataSize, 
                         0, 
                         0))
        {
            hr = HRESULT_FROM_WIN32 (GetLastError());
            goto cleanup;
        }

        // wait for a response
        if(!ReadMsgQueue(m_hMQIN, 
                         (void*)(&vResponse), 
                         sizeof(VerificationResponse), 
                         &dwBytesRead, 
                         VERIFICATION_OPERATION_TIMEOUT_MS, 
                         &dwFlags))
        {
            hr = HRESULT_FROM_WIN32 (GetLastError());
            goto cleanup;
        }

        hr = vResponse.hr;
#endif        
    }

cleanup:
    if(pRequest)
        delete[] pRequest;

    return hr;
}

HRESULT CGraphVerifier::Reset()
{
    HRESULT hr = S_OK;
    VerificationRequest* pRequest = NULL;
    VerificationResponse vResponse;
    DWORD dwBytesRead = 0;
    DWORD dwFlags = 0;
    
    if(m_Location == VLOC_CLIENT)
    {
#ifdef UNDER_CE 
        // Create the request
        if((pRequest = (VerificationRequest*)new BYTE[sizeof(VerificationRequest)]) == 0)
        {
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }

        pRequest->cbDataSize = 0;
        pRequest->pData = NULL;
        pRequest->command = VCMD_RESET;

        // Send the request to the remote verifier
        if(!WriteMsgQueue(m_hMQOUT, 
                         (void*)(pRequest), 
                         sizeof(VerificationRequest) + pRequest->cbDataSize, 
                         0, 
                         0))
        {
            hr = HRESULT_FROM_WIN32 (GetLastError());
            goto cleanup;
        }
    
     
        // wait for a response
        if(!ReadMsgQueue(m_hMQIN, 
                         (void*)(&vResponse), 
                         sizeof(VerificationResponse), 
                         &dwBytesRead, 
                         VERIFICATION_OPERATION_TIMEOUT_MS, 
                         &dwFlags))
        {
            hr = HRESULT_FROM_WIN32 (GetLastError());
            goto cleanup;
        }

        hr = vResponse.hr;
#endif        
    }
    else
    {
        CAutoLock cLock(&m_CritSec);

        // We only reset state member variables.  Configuration-based members remain their current values.
        m_bEOS = false;
        m_bFirstSample = false;
        m_llFirstSampleTime = 0;
        m_llEOSTime = 0;
        m_llSetStart = 0;
        m_llSetStop = 0;
        m_dSetRate = 1.0;
    }

cleanup:
    if(pRequest)
        delete[] pRequest;

    return hr;
}

HRESULT CGraphVerifier::Update(void* pData, DWORD* pdwSize)
{
    HRESULT hr = S_OK;
    VerificationRequest* pRequest = NULL;
    VerificationResponse vResponse;
    DWORD dwBytesRead = 0;
    DWORD dwFlags = 0;

    if(!pData || !pdwSize)
    {
        hr = E_INVALIDARG;
        goto cleanup;
    }
    
    if(m_Location == VLOC_CLIENT)
    {
#ifdef UNDER_CE
        // Create the request
        if((pRequest = (VerificationRequest*)new BYTE[sizeof(VerificationRequest)]) == 0)
        {
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }

        pRequest->cbDataSize = *pdwSize;
        pRequest->pData = (BYTE*)pRequest+ sizeof(VerificationRequest);
        pRequest->command = VCMD_UPDATE;

        memcpy(pRequest->pData, pData, *pdwSize);

        // Send the request to the remote verifier
        if(!WriteMsgQueue(m_hMQOUT, 
                         (void*)(pRequest), 
                         sizeof(VerificationRequest) + pRequest->cbDataSize, 
                         0, 
                         0))
        {
            hr = HRESULT_FROM_WIN32 (GetLastError());
            goto cleanup;
        }
    
     
        // wait for a response
        if(!ReadMsgQueue(m_hMQIN, 
                         (void*)(&vResponse), 
                         sizeof(VerificationResponse), 
                         &dwBytesRead, 
                         VERIFICATION_OPERATION_TIMEOUT_MS, 
                         &dwFlags))
        {
            hr = HRESULT_FROM_WIN32 (GetLastError());
            goto cleanup;
        }

        hr = vResponse.hr;
#endif        
    }

cleanup:
    if(pRequest)
        delete[] pRequest;

    return hr;
    
}

TCHAR* CGraphVerifier::GetName()
{
    return m_szName;
}

HRESULT CGraphVerifier::SignalExit()
{
    HRESULT hr = S_OK;

    if(!m_hExit || !(SetEvent(m_hExit)))
        hr = E_FAIL;
        
    return hr;
}

HRESULT CGraphVerifier::SignalStarted()
{
    HRESULT hr = S_OK;

    if(!m_hStarted || !(SetEvent(m_hStarted)))
        hr = E_FAIL;
        
    return hr;
}

HANDLE CGraphVerifier::GetExitEvent()
{
    return m_hExit;
}

VerificationType CGraphVerifier::GetType()
{
    return m_Type;
}

DWORD WINAPI VerifierThreadProc( __in LPVOID lpParameter)
{
    HRESULT hr = S_OK;
    DWORD dwRetVal = 1;
    WCHAR* pszOutName = NULL, *pszInName = NULL;
    size_t pcch = 0;
    BYTE* pInMessageBuffer = NULL;
    BYTE* pOutMessageBuffer = NULL;
    DWORD dwBytesRead = 0;
    DWORD dwMsgFlags = 0;
    CGraphVerifier* pVerifier = NULL;
    VerificationRequest* pRequest = NULL;
    VerificationResponse* pResponse = NULL;
    HANDLE hTriggers[2];
    BOOLEAN bStarted = false;

#ifdef UNDER_CE
    MSGQUEUEOPTIONS mqo_in, mqo_out;
    HANDLE hMQ_IN = NULL;
    HANDLE hMQ_OUT = NULL;


    if(!lpParameter)
    {
        hr = E_INVALIDARG;
        goto cleanup;
    }

    // Save a pointer to the verifier object that created this thread.  We will call its update functions from the message pump.
    pVerifier = (CGraphVerifier*)lpParameter;

    // Connect to incoming and outgoing queues for this verifier and get ready to read & write from them.
    // Note: The message queues will always be created on the client side prior to this function being called.

    // Formulate the queue names
    if(FAILED(hr = StringCchLength(pVerifier->GetName(), TEST_MAX_PATH - 16, &pcch)))
        goto cleanup;

    if((pszInName = new WCHAR[pcch + 16]) == 0)
    {
        hr = E_OUTOFMEMORY;
        goto cleanup;
    }

    if(FAILED(hr = StringCbCopy(pszInName, (pcch + 16) * sizeof(WCHAR) ,  pVerifier->GetName())))
        goto cleanup;
    
    if(FAILED(hr = StringCbCatW(pszInName, (pcch + 16) * sizeof(WCHAR), L"- to remote")))
        goto cleanup;

    if((pszOutName = new WCHAR[pcch + 16]) == 0)
    {
        hr = E_OUTOFMEMORY;
        goto cleanup;
    }

    if(FAILED(hr = StringCbCopy(pszOutName, (pcch + 16) * sizeof(WCHAR) ,  pVerifier->GetName())))
        goto cleanup;
    
    if(FAILED(hr = StringCbCatW(pszOutName, (pcch + 16) * sizeof(WCHAR), L"- from remote")))
        goto cleanup;

    //Set up config info
    mqo_in.dwSize = sizeof(MSGQUEUEOPTIONS);
    mqo_in.dwFlags = MSGQUEUE_ALLOW_BROKEN | MSGQUEUE_NOPRECOMMIT;     
    mqo_in.dwMaxMessages = 1;                   // 1 message allowed in the queue at a time (synchronous IO)
    mqo_in.cbMaxMessage = MAX_VERIFICATION_MESSAGE_SIZE;           
    mqo_in.bReadAccess = TRUE;    

    mqo_out.dwSize = sizeof(MSGQUEUEOPTIONS);
    mqo_out.dwFlags = MSGQUEUE_ALLOW_BROKEN | MSGQUEUE_NOPRECOMMIT;      
    mqo_out.dwMaxMessages = 1;                   // 1 message allowed in the queue at a time (synchronous IO)
    mqo_out.cbMaxMessage = MAX_VERIFICATION_MESSAGE_SIZE;           
    mqo_out.bReadAccess = FALSE;  


    // Connect to the queues
    LOG(TEXT("%S: Connecting to Server Queue %s"), __FUNCTION__, pszInName);
    if((hMQ_IN = CreateMsgQueue(pszInName, &mqo_in)) == 0)
    {
        hr = HRESULT_FROM_WIN32 (GetLastError());
        LOG(_T("Failed with hr=%x"), hr);
        goto cleanup;
    }
    LOG(TEXT("Succeeded"));

    LOG(TEXT("%S: Connecting to Server Queue %s"), __FUNCTION__, pszOutName);
    if((hMQ_OUT = CreateMsgQueue(pszOutName, &mqo_out)) == 0)
    {
        hr = HRESULT_FROM_WIN32 (GetLastError());
        LOG(_T("Failed with hr=%x"), hr);
        goto cleanup;
    }
    LOG(TEXT("Succeeded"));

    // Allocate our queue message buffers
    if((pInMessageBuffer = new BYTE[MAX_VERIFICATION_MESSAGE_SIZE]) == 0)
    {
        hr = E_OUTOFMEMORY;
        goto cleanup;
    }

    if((pOutMessageBuffer = new BYTE[MAX_VERIFICATION_MESSAGE_SIZE]) == 0)
    {
        hr = E_OUTOFMEMORY;
        goto cleanup;
    }

    pRequest = (VerificationRequest*)pInMessageBuffer;
    pResponse = (VerificationResponse*)pOutMessageBuffer;
    

    // Items the pump will block waiting for    
    hTriggers[0] = hMQ_IN;
    hTriggers[1] = pVerifier->GetExitEvent();

    
    // Message Pump
    while(1)
    {
        if(!bStarted)
        {
            if(FAILED(pVerifier->SignalStarted()))
                goto cleanup;

            bStarted = true;
        }

        // Wait for an incoming command from the test client verifier object
        // We will exit if we get an error or a signal from anything but the incoming queue (including the exit event)
        if(WAIT_OBJECT_0 != WaitForMultipleObjects(2, hTriggers, false, INFINITE))
            goto cleanup;   

        // Read & parse the command
        if(!ReadMsgQueue(hMQ_IN, 
                        (void*)pInMessageBuffer, 
                        MAX_VERIFICATION_MESSAGE_SIZE,
                        &dwBytesRead,
                        0,
                        &dwMsgFlags))
        {
            hr = HRESULT_FROM_WIN32 (GetLastError());
            goto cleanup;
        }

        switch(pRequest->command)
        {
        case VCMD_GETRESULT:
            pResponse->cbDataSize = MAX_VERIFICATION_MESSAGE_SIZE - sizeof(VerificationResponse); // tell the verifier we have this much room for the result   
            pResponse->pData = (BYTE*)pResponse + sizeof(VerificationResponse);
            pResponse->hr = pVerifier->GetResult(pVerifier->GetType(), pResponse->pData, &(pResponse->cbDataSize)); // verifier assigns actual size used  
            break;

        case VCMD_RESET:
            pResponse->cbDataSize = 0;
            pResponse->pData = NULL;
            pResponse->hr = pVerifier->Reset();
            break;

        case VCMD_START:
            pResponse->cbDataSize = 0;
            pResponse->pData = NULL;
            pResponse->hr = pVerifier->Start();
            break;

        case VCMD_STOP:
            pResponse->cbDataSize = 0;
            pResponse->pData = NULL;
            pResponse->hr = pVerifier->Stop();      
            break;

        case VCMD_UPDATE:
            pResponse->cbDataSize = 0;
            pResponse->pData = NULL;
            pResponse->hr = pVerifier->Update(pRequest->pData, &(pRequest->cbDataSize));
            break;

        default:
            hr = E_INVALIDARG;
            goto cleanup;

        };

        // Send the response
        if(!WriteMsgQueue(hMQ_OUT, 
                        (void*)pResponse, 
                        sizeof(VerificationResponse) + pResponse->cbDataSize,
                        0,
                        dwMsgFlags))
        {
            // We are always communicating in a synchronous fashion between the verifier 'client' and 'server', so there will always be
            // be a guaranteed maximum 1 message total in the read & write queues at a time, which will be processed prior to another message
            // being added.
            // Therefore, there shouldn't be a valid reason why this should fail.
            hr = HRESULT_FROM_WIN32 (GetLastError());
            goto cleanup;
        }                                                       
    }

cleanup:

    if(hMQ_IN)
    {
        LOG(TEXT("%S: Closing incoming SERVER Queue to Remote"), __FUNCTION__);    
        if(!CloseMsgQueue(hMQ_IN))
        {
            HRESULT hr = HRESULT_FROM_WIN32 (GetLastError());
            LOG(_T("Failed with hr=%x"), hr);
        }
    }

    if(hMQ_OUT)
    {
        LOG(TEXT("%S: Closing outgoing SERVER Queue from Remote"), __FUNCTION__);    
        if(!CloseMsgQueue(hMQ_OUT))
        {
            HRESULT hr = HRESULT_FROM_WIN32 (GetLastError());
            LOG(_T("Failed with hr=%x"), hr);
        }
    }
    
    if(pszInName)
        delete[] pszInName;

    if(pszOutName)
        delete[] pszOutName;

    if(pOutMessageBuffer)
        delete[] pOutMessageBuffer;

    if(pInMessageBuffer)
        delete[] pInMessageBuffer;


    if(FAILED(hr))
        dwRetVal = 0; 

#endif    
    return dwRetVal; 
 
}


// Initialize the static instances.
CVerificationMgr *CVerificationMgr::m_VerificationMgr = NULL;
DWORD CVerificationMgr::m_dwRefCount = 0;
HANDLE CVerificationMgr::m_hMutex = 0;

// ctor
CVerificationMgr::CVerificationMgr()
{
    m_VerificationMgr = NULL;
    m_TestVerifications.clear();
    m_TestVerifiers.clear();
    m_dwCurrentIndex = 0;
    m_bInitialized = FALSE;
}

// dtor
CVerificationMgr::~CVerificationMgr()
{
    itVerification iterator = m_TestVerifications.begin();
    while ( iterator != m_TestVerifications.end() )
    {
        VerificationInfo* pInfo = iterator->second;
        if ( pInfo )
            delete pInfo;
        iterator++;
    }
    m_TestVerifications.clear();
    ReleaseVerifiers();
}

CVerificationMgr *
CVerificationMgr::Instance()
{
    MutexWait mw(m_hMutex);

    if(!m_VerificationMgr)
        m_VerificationMgr = new CVerificationMgr();
    
    m_dwRefCount++;

    return m_VerificationMgr;
}

HRESULT CVerificationMgr::Release()
{
    HRESULT hr = S_OK;
    MutexWait mw(m_hMutex);

    m_dwRefCount--;

    if(m_dwRefCount == 0)
    {
        delete m_VerificationMgr;
        m_VerificationMgr = NULL;
    }

    return hr;
}

void CVerificationMgr::GlobalInit()
{
    TCHAR szMutexName[MAX_PATH];
    
    StringCchPrintf(szMutexName, MAX_PATH, _T("Global\\CVerifMgr:%d"), GetCurrentProcessId);

    m_hMutex = CreateMutex(NULL,
                            FALSE,
                            szMutexName);                           
}

void CVerificationMgr::GlobalRelease()
{
    CloseHandle(m_hMutex);
    m_hMutex = 0;
}

BOOL    
CVerificationMgr::PopulateMap( VerificationMap *pMap )
{
    if ( !pMap ) return FALSE;
    itVerification it;
    for ( it = pMap->begin(); it != pMap->end(); it++ )
    {
        m_TestVerifications.insert( VerificationMap::value_type( it->first, it->second ) );
    }
    return    TRUE;
}

DWORD   
CVerificationMgr::GetNumberOfVerifications()
{
    return m_TestVerifications.size();
}

VerificationInfo *
CVerificationMgr::GetVerificationInfo( VerificationType type )
{
    itVerification it;
    it = m_TestVerifications.find( type );
    if ( it == m_TestVerifications.end() )
        return 0x0;

    return it->second;
}

VerificationInfo *
CVerificationMgr::GetVerificationInfo( PCSTR verificationName )
{
    itVerification it;
    VerificationInfo *pInfo = NULL;

    TCHAR szVerificationName[128];

    for ( it = m_TestVerifications.begin();
          it != m_TestVerifications.end();
          it++ )
    {
        pInfo = it->second;
        if ( !pInfo ) continue;
        CharToTChar( verificationName, szVerificationName, countof(szVerificationName) );
        if ( !_tcscmp( szVerificationName, pInfo->szVerificationName ) )
            break;
    }
    
    if ( it == m_TestVerifications.end() ) return NULL;

    return pInfo;
}

HRESULT 
CVerificationMgr::AddVerifier( VerificationType type, 
                               CGraphVerifier* pVerifier )
{
    // Add to our list of verifiers
    m_TestVerifiers.insert( VerifierMap::value_type(type, pVerifier) );

    return S_OK;
}

HRESULT 
CVerificationMgr::EnableVerification( VerificationType type, 
                                    VerificationLocation location,
                                      void *pVerificationData,
                                      TestGraph *pTestGraph,
                                      CGraphVerifier** ppVerifier )
{
    HRESULT hr = S_OK;
    CGraphVerifier* pVerifier = NULL;
    VerificationInitInfo* pInitInfo = NULL;
    DWORD dwSize = sizeof(VerificationInitInfo);

    if(pVerificationData)
    {
        // Just get the data size so we can copy the data.
        pInitInfo = (VerificationInitInfo*)pVerificationData;
        dwSize += pInitInfo->cbDataSize;
    }

    if((pInitInfo = (VerificationInitInfo*) new BYTE[dwSize]) == 0)
    {
        hr = E_OUTOFMEMORY;
        goto cleanup;
    }

    if(pVerificationData)
    {
        memcpy(pInitInfo, pVerificationData, dwSize);
        pInitInfo->pData = (BYTE*)pInitInfo + sizeof(VerificationInitInfo);
    }
    else
    {
        pInitInfo->pData = NULL;
        pInitInfo->cbDataSize = 0;
    }

    pInitInfo->type = type;
    pInitInfo->location = location;
    pInitInfo->ppVerifier = ppVerifier;
    
    m_QueuedVerifiers.push_back(pInitInfo);

cleanup:

    return hr;
}

HRESULT
CVerificationMgr::AddQueuedVerifiers(TestGraph* pTestGraph)
{
    HRESULT hr = S_OK;
    VerificationInitInfo* pInitInfo = NULL;
    CGraphVerifier* pVerifier = NULL;
    
    itInitInfo iterator = m_QueuedVerifiers.begin();

    while(iterator != m_QueuedVerifiers.end())
    {
        if(pInitInfo)
        {
            delete[] pInitInfo;
            pInitInfo = NULL;
        }

        pInitInfo = *iterator;

        switch(pInitInfo->location)
        { 
        case VLOC_CLIENT:
            // Attempt to create a client stub.  If this fails, try to create
            // the verifier locally.
            if(S_OK == (hr = CreateGraphVerifier(pInitInfo->type, VLOC_CLIENT, pInitInfo->pData, pTestGraph, &pVerifier )))
                break;
            
        case VLOC_LOCAL:
            hr = CreateGraphVerifier(pInitInfo->type, VLOC_LOCAL, pInitInfo->pData, pTestGraph, &pVerifier );
            break;
            
        default:        
            hr = E_FAIL;        
            goto cleanup;
        };

        iterator++;

        // Ignore instances where the verifier factory doesn't exist
        if(hr == E_NOTIMPL)
        {
            hr = S_OK;
            continue;
        }

        // Add to our list of verifiers
        m_TestVerifiers.insert( VerifierMap::value_type(pInitInfo->type, pVerifier));

        // Give a reference to the verifier back to the test app.
        if ( pInitInfo->ppVerifier )
            *(pInitInfo->ppVerifier) = pVerifier;

        
    }

cleanup:
    if(pInitInfo)
        delete[] pInitInfo;

    m_QueuedVerifiers.clear();

    return hr;
    
}


void 
CVerificationMgr::ReleaseVerifiers()
{
    itVerifier iterator = m_TestVerifiers.begin();
    while ( iterator != m_TestVerifiers.end() )
    {
        CGraphVerifier* pGraphVerifier = iterator->second;
        if ( pGraphVerifier )
        {
            pGraphVerifier->Stop();
            delete pGraphVerifier;
        }
        iterator++;
    }
    m_TestVerifiers.clear();
}

HRESULT 
CVerificationMgr::StartVerification()
{
    HRESULT hr = S_OK;
    itVerifier iterator = m_TestVerifiers.begin();
    while ( iterator != m_TestVerifiers.end() )
    {
        VerificationType type = (VerificationType)iterator->first;
        CGraphVerifier* pGraphVerifier = iterator->second;
        hr = pGraphVerifier->Start();
        if ( FAILED_F(hr) )
            break;
        iterator++;
    }
    return hr;
}

void 
CVerificationMgr::StopVerification()
{
    itVerifier iterator = m_TestVerifiers.begin();
    while ( iterator != m_TestVerifiers.end() )
    {
        CGraphVerifier* pGraphVerifier = iterator->second;
        pGraphVerifier->Stop();
        iterator++;
    }
}

void 
CVerificationMgr::ResetVerification()
{
    itVerifier iterator = m_TestVerifiers.begin();
    while (iterator != m_TestVerifiers.end())
    {
        CGraphVerifier* pGraphVerifier = iterator->second;
        pGraphVerifier->Reset();
        iterator++;
    }
}

HRESULT 
CVerificationMgr::GetVerificationResult( VerificationType type, 
                                         void *pVerificationResult,
                                         DWORD cbDataSize)
{
    itVerifier iterator = m_TestVerifiers.find( type );
    if ( iterator != m_TestVerifiers.end() )
    {
        CGraphVerifier* pGraphVerifier = iterator->second;
        return pGraphVerifier->GetResult( type, pVerificationResult, &cbDataSize);
    }
    else 
        return E_FAIL;
}

HRESULT 
CVerificationMgr::GetVerificationResults()
{
    HRESULT hr = S_OK;
    itVerifier iterator = m_TestVerifiers.begin();
    while ( iterator != m_TestVerifiers.end() )
    {
        VerificationType type = (VerificationType)iterator->first;
        CGraphVerifier* pGraphVerifier = iterator->second;
        hr = pGraphVerifier->GetResult( type );
        if ( FAILED_F(hr) )
            break;
        iterator++;
    }
    return hr;
}

HRESULT 
CVerificationMgr::UpdateVerifier( VerificationType type,
                        void *pData,
                        DWORD cbDataSize)
{
    itVerifier iterator = m_TestVerifiers.find( type );
    if ( iterator != m_TestVerifiers.end() )
    {
        CGraphVerifier* pGraphVerifier = iterator->second;
        return pGraphVerifier->Update( pData, &cbDataSize);
    }
    else 
        return E_FAIL;
}


const TCHAR* 
GetVerificationString( VerificationType type )
{
    VerificationInfo *pInfo = NULL;    
    CVerificationMgr* pMgr = CVerificationMgr::Instance();
    pInfo = pMgr->GetVerificationInfo( type );
    pMgr->Release();

    if ( !pInfo ) return NULL;
    return pInfo->szVerificationName;
}

bool
IsVerifiable( VerificationType type )
{
    VerificationInfo *pInfo = NULL;
    CVerificationMgr* pMgr = CVerificationMgr::Instance();
    pInfo = pMgr->GetVerificationInfo( type );
    pMgr->Release();

    if ( !pInfo ) return FALSE;
    return TRUE;
}

HRESULT 
CreateGraphVerifier( VerificationType type,
                       VerificationLocation location,
                       void *pVerificationData, 
                       void *pOwner, 
                       CGraphVerifier **ppGraphVerifier )
{
    VerificationInfo *pInfo = NULL;    
    CVerificationMgr* pMgr = CVerificationMgr::Instance();
    pInfo = pMgr->GetVerificationInfo( type );
    pMgr->Release();

    if ( !pInfo ) return FALSE;

    if ( pInfo->fpVerifierFactory )
    {
        return pInfo->fpVerifierFactory( type,
                                    location,
                                    pInfo->szVerificationName,
                                    pVerificationData, 
                                    pOwner, 
                                    ppGraphVerifier );
    }

    return E_NOTIMPL;
}

// This is the call back method from the tap filter. This call needs to be relayed to the verifier
HRESULT 
GenericTapFilterCallback( GraphEvent event, 
                          void *pGraphEventData, 
                          void *pTapCallbackData, 
                          void *pObject )
{
    HRESULT hr = S_OK;
    CGraphVerifier *pVerifier = (CGraphVerifier*)pObject;
    
    if(!pVerifier)
    {
        hr = E_INVALIDARG;
        goto cleanup;
    }

    hr = pVerifier->ProcessEvent(event, pGraphEventData);

cleanup:

    return hr;
}


#if 0
BOOL    
CVerificationMgr::InitializeVerifications( CXMLElement *pVerificationDLLs )
{
    if ( !pVerificationDLLs )
        return FALSE;

    // verification DLL name, path
    HINSTANCE hDLL = NULL;
    PCSTR   szTempName;
    PCSTR   szTempPath;
    char    szDLLTemp[512];
    TCHAR    szDLL[512];
    ZeroMemory( szDLLTemp, 512 );
    ZeroMemory( szDLL, 512 );

    CXMLElement *pDLLElem = pVerificationDLLs->GetFirstChildElement();
    CXMLAttribute *pDLLAttr = 0x0;

    typedef VerificationMap *(*fpGetVerifications)();
    fpGetVerifications fpGetMaps = 0x0;

    while ( pDLLElem )
    {
        pDLLAttr = pDLLElem->GetFirstAttribute();
        while ( pDLLAttr )
        {
            szTempName = szTempPath = 0x0;
            if ( strcmp( "name", pDLLAttr->GetName() )== 0 )
            {
                szTempName = pDLLAttr->GetValue();
            }
            else if ( strcmp( "path", pDLLAttr->GetName()) == 0 )
            {
                szTempPath = pDLLAttr->GetValue();
            }
            pDLLAttr = pDLLAttr->GetNextAttribute();
        }

        strcpy( szDLLTemp, szTempPath );
        strcat( szDLLTemp, szTempName );
        
        CharToTChar( szDLLTemp, szDLL, 512 );

        hDLL = LoadLibrary( szDLL );
        fpGetMaps = (fpGetVerifications)GetProcAddress( hDLL, TEXT("GetVerficationMap") );
        // populate the maps
        if ( !PopulateMap( fpGetMaps() ) )
            goto error;

        pDLLElem = pDLLElem->GetNextSiblingElement();
    }

    return TRUE;
error:
    return FALSE;
}
#endif 
