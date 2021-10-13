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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  TestTux TUX DLL
//
//  Module: main.h
//          Header for all files in the project.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __MAIN_H__
#define __MAIN_H__

////////////////////////////////////////////////////////////////////////////////
// Included files

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <tux.h>
#include <kato.h>
#include <Oleauto.h>
#include <Winnls.h>
#include <voipmanager.h>
#include "IExchangeClient.h"
#include <winbase.h>
////////////////////////////////////////////////////////////////////////////////
// Suggested log verbosities

#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14




////////////////////////////////////////////////////////////////////////////////
//functions in test.cpp defined here
INT PassCmdLineParamstoGlobal();
INT InitializeVoipMgr();
INT CleanUpVoIP();
INT VerifyExchangeClientPressence();
INT VerifyVoIPMgrPressence();
INT CETKVoIPRunSipTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
INT CETKVoIPRunExchTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
VOID CleanUpGlobals();
//////////////////////////////////////////////////////////////////
// call back function for the voipmgr
class CETKVoIPUI : public IVoIPUI
{
public:
    // Constructor/destructor
    CETKVoIPUI();
    ~CETKVoIPUI();

public:
    // IUnknown
	virtual ULONG STDMETHODCALLTYPE AddRef();
	virtual ULONG STDMETHODCALLTYPE Release(); 
	HRESULT STDMETHODCALLTYPE CETKVoIPUI::QueryInterface(REFIID riid, VOID** ppvObj);
public:
	HRESULT RegisterWithSipServer(SIPServerRegistrationParameters *pSipRegParams);
	BSTR GetErrorString();
	HRESULT getMgrState(VoIPManagerState *pulState);
	VOID SetVoipMgr(IVoIPMgr2 *VoIPMgr);
	BOOL IncomingCallSucceeded();
	BOOL OutgoingCallSucceeded();
	STDMETHODIMP OnSystemEvent(
    /*[in]*/ VoIPSystemEvent vseEvent,
    /*[in]*/ INT_PTR iptrParams
    );

	STDMETHODIMP OnCallEvent(
    /*[in]*/ VoIPCallEvent vceEvent,
    /*[in]*/ IVoIPCurrentCall *piCall
    );

	HRESULT Initialize();
	HRESULT Uninitialize();

private:
    // Member controls
	LONG				m_cRefs;				// com ref count
	BSTR				m_bstrErrorString;		// temporary error string
	BSTR				m_bstrURI;				// temporary phone number
	HRESULT				m_hr;					// temporary result value
	IVoIPMgr2			*m_pMgr;				// VoIP Manager, internal only
	IVoIPCurrentCall	*m_piCurrentCall;		// pointer to the current call
	BOOL				m_CallInProgress;		// boolean for if a outgoing call was started
	BOOL				m_CallConnected;		// boolean for if a call was connected
	BOOL				m_CallAnswering;		// boolean for if an incoming call was answered
	BOOL				m_IncomingCall;			// boolean for if an incoming call was detected
};

/////////////////////////////////////////////////////////////////////////////
// This class implements the basic functionality of IExchangeClientRequestCallback interface
// this is where the more indepth tests are actually done (seeing if you actually get
// information 
class CETKExchClientRequestCallback : public IExchangeClientRequestCallback
{
	public:
	//COM required member functions
	virtual ULONG STDMETHODCALLTYPE AddRef();
	virtual ULONG STDMETHODCALLTYPE Release(); 
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvObj);

	//IExchangeClientRequestCallback functions
/* OnShutdown() is called by the IExchangeClient after Uninitialize completes */
	virtual HRESULT STDMETHODCALLTYPE OnShutdown();

/* OnRequestProgress() gets called for any change in the request status */
	virtual HRESULT STDMETHODCALLTYPE OnRequestProgress(
		IExchangeClientRequest *piRequest,
		ExchangeClientRequestStatus ecrs
		);
	//I created this to have the setup done inside the class to make it clean.
	virtual HRESULT InitializeExchangeClient(IExchangeClient **ppIExchangeClient);

// Constructor
	CETKExchClientRequestCallback();
	private:
//	to set the exchange client into this object
	virtual VOID SetExchangeClient(IExchangeClient *pIExchangeClient);
// IExchangeClient object that this is a call back function for
	IExchangeClient *m_pIExchClient;
// For reference counting
	LONG m_cRef;  
};

//Global Variables used by CETK and Tux that are not able to be defined in Globals
//////////////////////////////////////////////////////////////////
typedef struct{
	INT								iTestCaseID;		// test case number
	INT								iRetVal;			// return value for callback function to set
	SIPServerRegistrationParameters	RegParams;			// SIP server parameters (VoIP defined)
	BSTR							bstrOutgoingNumber;	// Number to call out to
	BSTR							bstrErrorString;	// Temporary error string
	BSTR							bstrExchServer;		// Exchange Server Name
	BSTR							bstrExchUserName;	// Exchange User Name
	BSTR							bstrExchPassword;	// Exchange Password
}GLOBAL_VARIABLES;


#endif // __MAIN_H__
