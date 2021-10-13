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

#include <DxTestHelper.h>

#define MSGQTIMEOUT 100

template <class _Message>
class DxHelperClient
{
public:
    DxHelperClient() :
        m_hReady(NULL),
        m_hMsgQueueRecvCommand(NULL),
        m_hMsgQueueSendResult(NULL),
        m_fFinished(false)
    {
    }

    ~DxHelperClient()
    {
        Cleanup();
    }

    HRESULT InitClient(const TCHAR * tszProcName)
    {
        TCHAR tszName[1024];
        HRESULT hr;
        HANDLE hGoEvent = NULL;
        MSGQUEUEOPTIONS msgQOpts;
    
        hr = StringCchCopy(m_tszProcName, _countof(m_tszProcName), tszProcName);
        if (FAILED(hr))
        {
            return hr;
        }

        StringCchPrintf(
            tszName, 
            _countof(tszName), 
            DXH_COMMAND_QUEUE,
            m_tszProcName,
            GetCurrentProcessId());
//        info(_T("Helper: Creating queue: %s"), tszName);
    
        memset(&msgQOpts, 0x00, sizeof(msgQOpts));
        msgQOpts.dwSize = sizeof(msgQOpts);
        msgQOpts.dwFlags = MSGQUEUE_NOPRECOMMIT; // | MSGQUEUE_ALLOW_BROKEN;
        msgQOpts.dwMaxMessages = 0;
        msgQOpts.cbMaxMessage = sizeof(_Message);
        msgQOpts.bReadAccess = TRUE;
    
        m_hMsgQueueRecvCommand = CreateMsgQueue(tszName, &msgQOpts);
        if (NULL == m_hMsgQueueRecvCommand)
        {
//            info(_T("Helper: Error creating recv messageQ %#x"), HRESULT_FROM_WIN32(GetLastError()));
            return HRESULT_FROM_WIN32(GetLastError());
        }
    
        StringCchPrintf(
            tszName,
            _countof(tszName),
            DXH_RESULTS_QUEUE,
            m_tszProcName,
            GetCurrentProcessId());
//        info(_T("Helper: Creating queue: %s"), tszName);
        msgQOpts.bReadAccess = FALSE;
        m_hMsgQueueSendResult = CreateMsgQueue(tszName, &msgQOpts);
        if (NULL == m_hMsgQueueSendResult)
        {
//            info(_T("Helper: Error creating send messageQ %#x"), HRESULT_FROM_WIN32(GetLastError()));
            return HRESULT_FROM_WIN32(GetLastError());
        }
    
        StringCchPrintf(
            tszName, 
            _countof(tszName), 
            DXH_GO_EVENT,
            m_tszProcName,
            GetCurrentProcessId());
        hGoEvent = CreateEvent(NULL, TRUE, FALSE, tszName);
        if (!hGoEvent)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        StringCchPrintf(
            tszName, 
            _countof(tszName), 
            DXH_READY_EVENT,
            m_tszProcName,
            GetCurrentProcessId());
        m_hReady = CreateEvent(NULL, TRUE, FALSE, tszName);
        if (!m_hReady)
        {
            CloseHandle(hGoEvent);
            return HRESULT_FROM_WIN32(GetLastError());
        }
        SetEvent(m_hReady);
        if (WAIT_OBJECT_0 != WaitForSingleObject(hGoEvent, 20000))
        {
            CloseHandle(hGoEvent);
            return HRESULT_FROM_WIN32(GetLastError());
        }
        CloseHandle(hGoEvent);
        return S_OK;
    }

    HRESULT RunClient()
    {
        _Message msg;
        DWORD cbRead = 0;
        DWORD dwFlags = 0;
        HRESULT hr = S_OK;
        m_fFinished = false;
        
        while (!m_fFinished)
        {
            // Read from the msgq, if nothing for 100ms, take care of window messages
            if (!ReadMsgQueue(
                    m_hMsgQueueRecvCommand,
                    &msg,
                    sizeof(msg),
                    &cbRead,
                    MSGQTIMEOUT,
                    &dwFlags))
            {
                if (GetLastError() != ERROR_TIMEOUT)
                {
                    hr = HRESULT_FROM_WIN32(GetLastError());
//                    info(_T("DDSCL Helper hit critical error reading command: %08x\r\n"), hr);
                    break;
                }
                continue;
            }
            
            if (DXH_MESSAGE_GOODBYE == msg.dwMessage)
            {
                // Give the client code a chance to process the goodbye message as well,
                // if cleanup is needed
                m_fFinished = true;
            }
            hr = ProcessCommand(&msg, cbRead);
            if (FAILED(hr))
            {
                break;
            }
    
            if (!WriteMsgQueue(
                    m_hMsgQueueSendResult,
                    &msg,
                    sizeof(msg),
                    5000,
                    0))
            {
                hr = HRESULT_FROM_WIN32(GetLastError());
//                info(_T("DDSCL Helper hit critical error writing result: %08x\r\n"), hr);
                break;
            }
        }
        return hr;
    }

    HRESULT Cleanup()
    {
        if (m_hReady)
        {
            CloseHandle(m_hReady);
            m_hReady = NULL;
        }
        if (m_hMsgQueueSendResult)
        {
            CloseMsgQueue(m_hMsgQueueSendResult);
            m_hMsgQueueSendResult = NULL;
        }
        if (m_hMsgQueueRecvCommand)
        {
            CloseMsgQueue(m_hMsgQueueRecvCommand);
            m_hMsgQueueRecvCommand = NULL;
        }
        return S_OK;
    }

protected:
    virtual HRESULT ProcessCommand(_Message * pMsg, size_t cbMessageSize) = 0;

    TCHAR m_tszProcName[MAX_HELPER_NAME + 1];
    HANDLE m_hReady;
    HANDLE m_hMsgQueueSendResult;
    HANDLE m_hMsgQueueRecvCommand;
    bool m_fFinished;
};
