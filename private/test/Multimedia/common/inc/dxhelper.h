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
#pragma once

#include <windows.h>

// The following format strings will use the process name of the helper (the %s)
// along with the process ID of the helper (%08x) to allow more than one active helper
#define DXH_COMMAND_QUEUE _T("%s_%08x_Commands") // The name of the queue to send commands
#define DXH_RESULTS_QUEUE _T("%s_%08x_Results") // The name of the queue that will receive results
#define DXH_READY_EVENT _T("%s_%08x_Ready") // The name of the event indicating remote helper is ready
#define DXH_GO_EVENT _T("%s_%08x_Go") // The name of the event indicating remote helper is ready

#define MAX_HELPER_NAME 32

#define DXH_MESSAGE_GOODBYE 0
#define DXH_MESSAGE_USER    1000

// All test-specific message classes should inherit from this class
struct DxHelperMessageBase
{
    // The message to send. For user commands, use DXH_MESSAGE_USER as the base
    DWORD dwMessage;

    // This should be set to 0 in all calls to SendCommand
    DWORD dwReserved;
};

template <class _MessageStruct>
class DxHelper
{
public:
    DxHelper() :
        m_hMsgQueueSendCommand(NULL),
        m_hMsgQueueRecvResult(NULL),
        m_dwIndex(0),
        m_fReady(false) 
    {
    }
    
    virtual ~DxHelper()
    {
        CloseHelper();
    }

    virtual HRESULT InitHelper(const TCHAR * tszHelperProcName, PROCESS_INFORMATION * ppiHelper = NULL)
    {
        HANDLE hHelperReady;
        HRESULT hr;
        TCHAR tszMsgQName[1024];
        MSGQUEUEOPTIONS msgQOpts;
        OutputDebugString(_T("Creating Helper Process"));

        hr = StringCchCopy(m_tszHelperProcName, _countof(m_tszHelperProcName), tszHelperProcName);
        if (FAILED(hr))
        {
            return hr;
        }

        if (!CreateProcess(
            m_tszHelperProcName, 
            NULL, 
            NULL, 
            NULL, 
            FALSE, 
            0,
            NULL,
            NULL,
            NULL,
            &m_pi))
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        StringCchPrintf(
            tszMsgQName, 
            _countof(tszMsgQName), 
            DXH_READY_EVENT,
            m_tszHelperProcName,
            m_pi.dwProcessId);

        hHelperReady = CreateEvent(NULL, TRUE, FALSE, tszMsgQName);
        if (NULL == hHelperReady)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (WAIT_TIMEOUT == WaitForSingleObject(hHelperReady, 20000))
        {
            CloseHandle(hHelperReady);
            return E_FAIL;
        }
        CloseHandle(hHelperReady);

        if (ppiHelper)
        {
            *ppiHelper = m_pi;
        }
        
        OutputDebugString(_T("Helper Process Created, finding window"));

        StringCchPrintf(
            tszMsgQName, 
            _countof(tszMsgQName), 
            DXH_COMMAND_QUEUE,
            m_tszHelperProcName,
            m_pi.dwProcessId);

        // dbgout<<"TuxDll: Creating queue: " << tszMsgQName << endl;
        memset(&msgQOpts, 0x00, sizeof(msgQOpts));
        msgQOpts.dwSize = sizeof(msgQOpts);
        msgQOpts.dwFlags = MSGQUEUE_NOPRECOMMIT; // | MSGQUEUE_ALLOW_BROKEN;
        msgQOpts.dwMaxMessages = 0;
        msgQOpts.cbMaxMessage = sizeof(_MessageStruct);
        msgQOpts.bReadAccess = FALSE;

        m_hMsgQueueSendCommand = CreateMsgQueue(tszMsgQName, &msgQOpts);
        if (NULL == m_hMsgQueueSendCommand)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        StringCchPrintf(
            tszMsgQName,
            _countof(tszMsgQName),
            DXH_RESULTS_QUEUE,
            m_tszHelperProcName,
            m_pi.dwProcessId);
//        dbgout<<"TuxDll: Creating queue: " << tszMsgQName << endl;
        msgQOpts.bReadAccess = TRUE;
        m_hMsgQueueRecvResult = CreateMsgQueue(tszMsgQName, &msgQOpts);
        if (NULL == m_hMsgQueueRecvResult)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        StringCchPrintf(
            tszMsgQName, 
            _countof(tszMsgQName), 
            DXH_GO_EVENT,
            m_tszHelperProcName,
            m_pi.dwProcessId);
        hHelperReady = CreateEvent(NULL, TRUE, FALSE, tszMsgQName);
        if (NULL == hHelperReady)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        SetEvent(hHelperReady);

        m_fReady = true;

        return S_OK;
    }
    
    virtual HRESULT CloseHelper()
    {
        HRESULT hrRet = S_OK;
        _MessageStruct msGoodbye;
        msGoodbye.dwMessage = DXH_MESSAGE_GOODBYE;
        msGoodbye.dwReserved = 0;
        hrRet = SendCommand(&msGoodbye, 1000);
        WaitForSingleObject(m_pi.hProcess, 5000);
        CloseMsgQueue(m_hMsgQueueRecvResult);
        CloseMsgQueue(m_hMsgQueueSendCommand);
        m_fReady = false;
        return hrRet;
    }

    // If pMessage is NULL, this will tell the remote process to shutdown
    // If the test is sending messages of a different size, it can use
    // cbMessageSize (this must be less than or equal to sizeof(_MessageStruct)
    // due to the way Message Queues work
    virtual HRESULT SendCommand(
        /* in/out */ _MessageStruct * pMessage,
        DWORD dwTimeout = INFINITE,
        size_t cbMessageSize = sizeof(_MessageStruct))
    {
        DWORD dwMessage;
        DWORD cbRead = 0;
        DWORD dwFlags = 0;

        if (!pMessage)
        {
            return E_POINTER;
        }

        if (cbMessageSize > sizeof(_MessageStruct))
        {
            return E_INVALIDARG;
        }
        if (pMessage->dwReserved != 0)
        {
            return E_INVALIDARG;
        }

        dwMessage = pMessage->dwMessage;
        pMessage->dwReserved = m_dwIndex;

        if (!WriteMsgQueue(
                m_hMsgQueueSendCommand,
                pMessage,
                cbMessageSize,
                dwTimeout,
                0))
        {
//            dbgout << "TuxDll: WriteMsgQueue failed : " << hex((DWORD)m_hrInternal) << endl;
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (!ReadMsgQueue(
                m_hMsgQueueRecvResult,
                pMessage,
                cbMessageSize,
                &cbRead,
                dwTimeout,
                &dwFlags))
        {
//            dbgout << "TuxDll: ReadMsgQueue failed : " << hex((DWORD)m_hrInternal) << endl;
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (dwMessage != pMessage->dwMessage || 
            m_dwIndex != pMessage->dwReserved)
        {
//            dbgout << "TuxDll: Received unexpected data from helper : " << hex((DWORD)m_hrInternal) << endl;
            return E_UNEXPECTED;
        }

        ++m_dwIndex;
        return S_OK;
    }

protected:
    PROCESS_INFORMATION m_pi;
    HANDLE m_hMsgQueueSendCommand;
    HANDLE m_hMsgQueueRecvResult;
    DWORD m_dwIndex;
    TCHAR m_tszHelperProcName[MAX_HELPER_NAME + 1];
    bool m_fReady;
};
