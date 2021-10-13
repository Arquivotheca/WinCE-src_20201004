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
#ifndef __NOTIFY__
#define __NOTIFY__

#include "httpext.h"

#include "auto_xxx.hxx"
#include "string.hxx"
#include "list.hxx"
#include "sync.hxx"


// msg_queue
class msg_queue
{
public:
    msg_queue(LPCWSTR pwszName, HANDLE hOwner)
        : m_strName(pwszName),
          m_bWriteTerminateMsg(false),
          m_hOwner(hOwner)
    {}

    void init();
    void deinit();

    // operator HANDLE
    operator HANDLE()
        {return m_handle; }

    // write_terminate_msg
    void write_terminate_msg()
        {m_bWriteTerminateMsg = true; }

    // check_terminate_msg
    bool check_terminate_msg()
    {
        if(m_bWriteTerminateMsg)
        {
            int x = 0;

            // write terminate msg so that reader knows that previous event message wasn't finished and must be discarded
            if(!WriteMsgQueue(m_handle, &x, 1, 0, 0))
            {
                return false;
            }
        }

        m_bWriteTerminateMsg = false;

        return true;
    }

    // lock
    void lock()
        {m_cs.lock(); }

    // unlock
    void unlock()
        {m_cs.unlock(); }

    // name
    const ce::wstring& name()
        {return m_strName; }

    // getOwner
    HANDLE owner()
        {return m_hOwner; }

private:
    ce::smart_msg_queue             m_handle;
    ce::critical_section_with_copy  m_cs;
    ce::wstring                     m_strName;
    HANDLE                          m_hOwner;
    bool                            m_bWriteTerminateMsg;
};


class Watermark
{
    public:
        Watermark(LONG maxWatermark, LONG initialWatermark = 0) :   watermarkLevel(NULL),
                                                                    maxWatermarkLevel(maxWatermark),
                                                                    initialized(false)
        {
            watermarkLevel = new LONG;
            if (!watermarkLevel)
            {
                return;
            }
            *watermarkLevel = initialWatermark;
            initialized = true;
        }
        ~Watermark() {}

        bool IncrementWatermark()
        {
            if(!initialized ) 
            {
                return false;
            }
            LONG level = InterlockedIncrement(watermarkLevel);
            return level <= maxWatermarkLevel;
        }
        void DecrementWatermark()
        {
            if(! initialized )
            {
                return;
            }
            InterlockedDecrement(watermarkLevel);
        }

    private:
        ce::smart_ptr<LONG> watermarkLevel;
        LONG maxWatermarkLevel;
        bool initialized;
};

// notification_sink
class notification_sink
{
public:
    notification_sink(DWORD dwType, LPCWSTR pwszUSN, LPCWSTR pwszQueryString, msg_queue& MsgQueue, HANDLE hOwner);

    void event(LPCWSTR pwszMessage, DWORD dwEventSEQ);
    void byebye(PSSDP_REQUEST pSsdpRequest);
    void alive(PSSDP_REQUEST pSsdpRequest);

    const ce::wstring& getQueryString()
        {return m_strQueryString; }

    const ce::wstring& getUSN()
        {return m_strUSN; }

    const DWORD getType()
        {return m_dwType; }

    HANDLE getHandle()
        {return this; }

    HANDLE getOwner()
        {return m_hOwner; }

    Watermark getWatermark()
        {return m_Watermark; }

private:
    DWORD                           m_dwType;
    ce::wstring                     m_strQueryString;
    ce::wstring                     m_strUSN;
    ce::critical_section_with_copy  m_cs;
    HANDLE                          m_hOwner;
    msg_queue                      &m_MsgQueue;
    Watermark                       m_Watermark;
};


// notification_mgr
class notification_mgr
{
public:
    HANDLE  register_notification(DWORD dwNotificationType, LPCWSTR pwszUSN, LPCWSTR pwszQueryString, LPCWSTR pwszMsgQueue, HANDLE hOwner);
    void    deregister_notification(HANDLE hSubscription);
    void    cleanup_notifications(HANDLE hOwner);
    bool    alive_byebye(PSSDP_REQUEST pSsdpRequest);
    DWORD   event(LPEXTENSION_CONTROL_BLOCK pecb);

private:
    ce::list<msg_queue>             m_listMsgQueues;
    ce::list<notification_sink>     m_listSinks;
    ce::critical_section            m_cs;
};

extern notification_mgr* g_pNotificationMgr;

#endif //__NOTIFY__
