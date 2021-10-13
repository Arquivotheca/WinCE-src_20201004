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
#ifndef __CONNECTION_POINT__
#define __CONNECTION_POINT__

#include "msgqueue.h"
#include "svsutil.hxx"

#include "auto_xxx.hxx"
#include "sync.hxx"
#include "string.hxx"
#include "list.hxx"
#include "sync.hxx"
#include "sax.h"
#include "assert.h"

#include "HttpChannel.h"
#include "CookieJar.h"

struct event_msg_hdr;

// ConnectionPoint
class ConnectionPoint
{
public:
    // ICallback
    class ICallback
    {
    public:
        virtual void StateVariableChanged(LPCWSTR pwszName, LPCWSTR pwszValue) = 0;
        virtual void ServiceInstanceDied(LPCWSTR pszUSN) = 0;
        virtual void AliveNotification(LPCWSTR pszUSN, LPCWSTR pszLocation, LPCWSTR pwszNLS, DWORD dwLifeTime) = 0;
    };
    
    ConnectionPoint();
    ~ConnectionPoint();

    HRESULT advise(LPCWSTR pwszUSN, UINT nLifeTime, ICallback* pCallback, DWORD* pdwSinkCookie);
    void    unadvise(DWORD dwSinkCookie);
    HRESULT subscribe(DWORD dwSinkCookie, LPCSTR pszURL);

protected:
    HRESULT init();
    void uninit();
    void dispatch_message(const event_msg_hdr& hdr);
    static DWORD WINAPI listening_thread(void* pThis);

public:
    // timer
    class timer
    {
    public:
        timer();
        ~timer();
        HRESULT init(SVSThreadPool* pThreadPool, LPTHREAD_START_ROUTINE pfTimerProc, DWORD dwSinkCookie);
        void start(DWORD dwTimeout);
        void restart(DWORD dwTimeout);
        void stop();

    private:
        SVSThreadPool*          m_pThreadPool;
        DWORD                   m_dwTimerID;
        LPTHREAD_START_ROUTINE  m_pfTimerProc;
        DWORD                   m_dwSinkCookie;
    };

public:
    // sink
    class sink : public ce::SAXContentHandler
    {
    public:
        // ctor and dtor
        sink(DWORD dwSinkCookie = 0);
        ~sink();

        HRESULT init(LPCWSTR pwszUSN, ICallback* pCallback, SVSThreadPool* pTimer);
        HRESULT register_notification(LPCWSTR pwszMsgQueueName);
        void    deregister_notification();
        HRESULT subscribe(LPCSTR pszEventsURL);
        HRESULT unsubscribe();
        HRESULT resubscribe();
        HRESULT fixsubscribe();
        void    event(LPCWSTR pwszEventMessage, DWORD dwEventSEQ);
        void    alive(LPCWSTR pwszUSN, LPCWSTR pwszLocation, LPCWSTR pwszNLS, DWORD dwLifeTime);
        void    byebye(LPCWSTR pwszUSN = NULL);
        void    stop_timers();
        
    private:
        static DWORD WINAPI ResubscribeTimerProc(VOID *pvContext);
        static DWORD WINAPI AliveTimerProc(VOID *pvContext);
    
    // ISAXContentHandler
    private:
        virtual HRESULT STDMETHODCALLTYPE startDocument(void);

        virtual HRESULT STDMETHODCALLTYPE startElement(
            /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
            /* [in] */ int cchNamespaceUri,
            /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
            /* [in] */ int cchLocalName,
            /* [in] */ const wchar_t __RPC_FAR *pwchQName,
            /* [in] */ int cchQName,
            /* [in] */ ISAXAttributes __RPC_FAR *pAttributes);
    
        virtual HRESULT STDMETHODCALLTYPE endElement( 
            /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
            /* [in] */ int cchNamespaceUri,
            /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
            /* [in] */ int cchLocalName,
            /* [in] */ const wchar_t __RPC_FAR *pwchQName,
            /* [in] */ int cchQName);
    
        virtual HRESULT STDMETHODCALLTYPE characters( 
            /* [in] */ const wchar_t __RPC_FAR *pwchChars,
            /* [in] */ int cchChars);

    public:
        static ce::SAXReader* m_pReader;
        HANDLE               m_hSubscription;

    private:
        DWORD                m_dwSinkCookie;
        DWORD                m_dwChannelCookie;
        DWORD                m_dwNotificationType;
        DWORD                m_dwEventSEQ;    // only valid in the SUBSCRIBED state
        DWORD                m_dwTimeoutSeconds;
        ICallback*           m_pCallback;
        ce::string           m_strEventsURL;
        ce::wstring          m_strUSN;
        ce::string           m_strSID;
        wchar_t              m_pwchQueryString[sizeof("notify:12345678-12345678") + 1];
        char                 m_pchQueryString[sizeof("notify:12345678-12345678") + 1];
        timer                m_timerResubscribe;
        timer                m_timerAlive;

        // state tracking
        enum 
        {
            SINK_STATE_UNINITIALIZED,
            SINK_STATE_UNSUBSCRIBED,
            SINK_STATE_SUBSCRIBED,
        }                    m_SinkState;

        // used during parsing event message
        ce::wstring          m_strName;
        ce::wstring          m_strValue;
        bool                 m_bParsingProperty;
        bool                 m_bParsingVariable;
    };

    ce::critical_section  m_csInit;
    ce::auto_msg_queue    m_hMsgQueue;
    ce::auto_handle       m_hListeningThread;
    ce::wstring           m_strMsgQueueName;
    ce::auto_handle       m_hEventShuttingDown;
    ce::auto_handle       m_hStarted;
    SVSThreadPool*        m_pThreadPool;
    bool                  m_bInitialized;
};

extern ConnectionPoint* g_pConnectionPoint;
extern CookieJar<ConnectionPoint::sink,1>    g_CJSink;

#endif // __CONNECTION_POINT__

