//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

	HRESULT	advise(LPCWSTR pwszUSN, UINT nLifeTime, ICallback* pCallback, DWORD* pdwCookie);
	void	unadvise(DWORD dwCookie);
	HRESULT subscribe(DWORD dwCookie, LPCSTR pszURL);

protected:
	HRESULT init();
    void uninit();
	void dispatch_message(const event_msg_hdr& hdr);
	static DWORD WINAPI listening_thread(void* pThis);

protected:
	// sink
	class sink : public ce::SAXContentHandler
	{
	public:
		sink(LPCWSTR pwszUSN, ICallback* pCallback, SVSThreadPool& Timer)
			: m_hSubscription(NULL),
			  m_dwNotificationType(0),
			  m_pCallback(pCallback),
			  m_strUSN(pwszUSN),
			  m_dwEventSEQ(0),
			  m_timerAlive(Timer, AliveTimerProc),
              m_timerResubscribe(Timer, ResubscribeTimerProc),
			  m_dwTimeoutSeconds(0),
			  m_bInitialEventReceived(false)
		{}

		HRESULT register_notification(LPCWSTR pwszMsgQueueName);
		void	deregister_notification();
		HRESULT subscribe(LPCSTR pszEventsURL = NULL);
		HRESULT unsubscribe();
		HRESULT resubscribe();
		void	event(LPCWSTR pwszEventMessage, DWORD dwEventSEQ);
        void	alive(LPCWSTR pwszUSN, LPCWSTR pwszLocation, LPCWSTR pwszNLS, DWORD dwLifeTime);
		void	byebye(LPCWSTR pwszUSN);

		HANDLE	getSubscriptionHandle()
			{return m_hSubscription; }

		// stop_timers
        void stop_timers()
        {
            m_timerAlive.stop();
            m_timerResubscribe.stop();
        }
        
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

	private:
        class timer
        {
        public:
            timer(SVSThreadPool& ThreadPool, LPTHREAD_START_ROUTINE pfTimerProc)
                : m_ThreadPool(ThreadPool),
                  m_pfTimerProc(pfTimerProc),
                  m_pvContext(NULL),
                  m_dwCookie(0)
            {}

            ~timer()
                {assert(m_dwCookie == 0); }

            // start
            void start(DWORD dwTimeout, VOID *pvContext)
            {
                assert(m_dwCookie == 0);

                m_pvContext = pvContext;
                m_dwCookie = m_ThreadPool.StartTimer(TimerProc, this, dwTimeout);
            }
            
            // stop
            void stop()
            {
                m_ThreadPool.StopTimer(m_dwCookie); 
			    m_dwCookie = 0;
            }

        private:
            static DWORD WINAPI TimerProc(VOID *pvContext)
            {
                timer* pThis = reinterpret_cast<timer*>(pvContext);

                pThis->m_dwCookie = 0;
                return (*pThis->m_pfTimerProc)(pThis->m_pvContext);
            }

        private:
            SVSThreadPool&	        m_ThreadPool;
            DWORD                   m_dwCookie;
            LPTHREAD_START_ROUTINE  m_pfTimerProc;
            LPVOID                  m_pvContext;
        };
    
    public:
        static ce::SAXReader* m_pReader;

    private:
		HANDLE			m_hSubscription;
		DWORD			m_dwNotificationType;
		DWORD			m_dwEventSEQ;
		bool			m_bInitialEventReceived;
		DWORD			m_dwTimeoutSeconds;
		ICallback*		m_pCallback;
		ce::string		m_strEventsURL;
		ce::wstring		m_strUSN;
		ce::string		m_strSID;
		wchar_t			m_pwchQueryString[sizeof("notify:12345678-12345678") + 1];
		char			m_pchQueryString[sizeof("notify:12345678-12345678") + 1];
		timer           m_timerResubscribe;
        timer           m_timerAlive;

		// used during parsing event message
		ce::wstring		m_strName;
		ce::wstring		m_strValue;
		bool			m_bParsingProperty;
		bool			m_bParsingVariable;
	};

	ce::auto_msg_queue		m_hMsgQueue;
	ce::auto_handle			m_hListeningThread;
	ce::list<sink>			m_listSinks;
	ce::critical_section	m_csListSinks;
    ce::critical_section	m_csInit;
	ce::wstring				m_strMsgQueueName;
	ce::auto_handle			m_hEventShuttingDown;
    ce::auto_handle			m_hStarted;
	SVSThreadPool*			m_pThreadPool;
    bool                    m_bInitialized;
};

extern ConnectionPoint* g_pConnectionPoint;

#endif // __CONNECTION_POINT__

