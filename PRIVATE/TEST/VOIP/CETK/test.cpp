//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//This is an 80 character formated code page, all lines must fit into 80 cols for printing
//34567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890
//                 1                   2         3         4         5         6         7         8
#include "main.h"
#include "globals.h"

#define MAKEACALL WM_USER + 1
GLOBAL_VARIABLES g_Vars = {0};
//////////////////////////////////////////////////////////////////////
//##################################################################//
//The Foolowing Functions and Defines make it simple to print out 
//the common status's of voip events, conditions, and errors
//
#define PRINTSTATUS(num,string) \
	case (num): \
	Debug(L"Status: %s", L#string); \
	break;

#define PRINTEVENT(num,string) \
	case (num): \
	Debug(L"Event: %s", L#string); \
	break;

VOID PrintError(HRESULT hr)
{
	switch(hr)
	{ 
	PRINTSTATUS(VOIP_E_INVALIDCALLSTATUS,"The call has an invalid status.")
	PRINTSTATUS(VOIP_E_NOTCURRENTCALL,"The call is not a current call.")
	default: Debug(L"Unknown Error."); break;
	} 
}

VOID PrintCallStatus(VoIPCallStatus status)
{
	switch (status)
	{
	PRINTSTATUS(e_vcsInvalid,"Invalid or Idle, they are both 0")
	//PRINTSTATUS(e_vcsIdle,"e_vcsIdle"); this is same case as e_vcsInvalidid
	PRINTSTATUS(e_vcsIncoming,"Incoming")
	PRINTSTATUS(e_vcsAnswering,"Answering")
	PRINTSTATUS(e_vcsInProgress,"In Progress")
	PRINTSTATUS(e_vcsConnected,"Connected")
	PRINTSTATUS(e_vcsDisconnected,"Disconnected")
	PRINTSTATUS(e_vcsHolding,"Holding")
	PRINTSTATUS(e_vcsReferring,"Referring")
	PRINTSTATUS(e_vcsInConference,"In Conference")
	}
}

VOID PrintExchangeClientStatus(ExchangeClientRequestStatus status)
{
	switch (status)
	{
	PRINTSTATUS(e_ecrsPending,"Pending")
	PRINTSTATUS(e_ecrsOutOfMemory,"Out Of Memory")
	PRINTSTATUS(e_ecrsInProgress,"In Progress")
	PRINTSTATUS(e_ecrsSending,"Sending")
	PRINTSTATUS(e_ecrsBypassingOWAPage,"Bypassing OWA Page")
	PRINTSTATUS(e_ecrsSucceeded,"Succeeded")
	PRINTSTATUS(e_ecrsParseFailed,"Parse Failed")
	PRINTSTATUS(e_ecrsHttpFailure,"HttpFailure")
	PRINTSTATUS(e_ecrsFailedToSend,"Failed To Send")
	PRINTSTATUS(e_ecrsFailedToBypassAuthPage,"Failed To Bypass Auth Page")
	PRINTSTATUS(e_ecrsCancelled,"Cancelled")
	default:
	break;
	}
}
 
VOID PrintEvent(VoIPCallEvent vceEvent)
{// Prints out the common call events for VoIP
	switch(vceEvent)
    {
	PRINTEVENT(e_vceStatusChanged,"Call Event: Call Status Changed.")
	PRINTEVENT(e_vceMissed,"Call Event: Missed Call")
	PRINTEVENT(e_vceAutoForwarded,"Unexpected Call Event: Auto Forwarded call")
	PRINTEVENT(e_vceAutoBlocked,"Unexpected Call Event: Auto Blocked call")
	PRINTEVENT(e_vceRedirectSucceeded,"Unexpected Call Event: Redirect Succeeded")
	PRINTEVENT(e_vceRedirectFailed,"Unexpected Call Event: Redirect failed")
	PRINTEVENT(e_vceDropped,"Unexpected Call Event: Dropped Call")
	default:
		Debug(L"Unexpected Event: Unhandled event: x%x",vceEvent);
		break;
    }
}

VOID PrintSipParams(SIPServerRegistrationParameters RegParams)
{// prints out the SIP parameters of a SIPServerRegistrationParameters structure
	Debug(L"User: %s",RegParams.wszAccountName);
	Debug(L"Password: %s",RegParams.wszAccountPassword);
	Debug(L"Auth Type: %s",RegParams.wszAuthType);
	Debug(L"Server: %s",RegParams.wszServer);
	Debug(L"Transport: %s",RegParams.wszTransport);
	Debug(L"URI: %s",RegParams.wszURI);
}
//																	//
//##################################################################//
//////////////////////////////////////////////////////////////////////

INT PassCmdLineParamstoGlobal()
{// This function parses the global variable for the command line that tux passes to us

	WCHAR *wszTemp=NULL,*wszTemp2=NULL;
	/*Can set defaults, make it easy to run tests.
	// For sip tests
	g_Vars.RegParams.wszAccountName = SysAllocString(L"cetvoip\\auto9");
	g_Vars.RegParams.wszAccountPassword = SysAllocString(L"test");
	g_Vars.RegParams.wszAuthType = SysAllocString(L"Kerberos");
	g_Vars.RegParams.wszServer = SysAllocString(L"cev-sip1proxy.cetvoip.nttest.microsoft.com");
	g_Vars.RegParams.wszTransport = SysAllocString(L"TCP");
	g_Vars.RegParams.wszURI = SysAllocString(L"409@cetvoip.nttest.microsoft.com");
	g_Vars.bstrOutgoingNumber = SysAllocString(L"408@cetvoip.nttest.microsoft.com");
	// For exch tests 
	g_Vars.bstrExchUserName = SysAllocString(L"cetvoip\\brian1i1");
	g_Vars.bstrExchPassword = SysAllocString(L"brian1i1");
	g_Vars.bstrExchServer = SysAllocString(L"http://CEV-MSG-01");
	/**/

	//print out the command line
	Debug(L"g_pShellInfo->szDllCmdLine:%s",g_pShellInfo->szDllCmdLine);
	
	wszTemp = wcspbrk(g_pShellInfo->szDllCmdLine,L"/");
	while (wszTemp != NULL)
	{// loop through all the /'s until there are no more
		//Debug(L"wszTemp:%s",wszTemp);
		if(!wcsnicmp(wszTemp,L"/S:",3))
		{//get the SIP server name
			SysFreeString((BSTR)g_Vars.RegParams.wszServer);
            g_Vars.RegParams.wszServer=SysAllocString(wszTemp + 3);
			wcstok((WCHAR*)g_Vars.RegParams.wszServer,L" ");
			Debug(L"SIP Server: %s",g_Vars.RegParams.wszServer);
		}
		else if(!wcsnicmp(wszTemp,L"/N:",3))
		{// Get the Phone Number for this phone
			SysFreeString((BSTR)g_Vars.RegParams.wszURI);
			g_Vars.RegParams.wszURI=SysAllocString(wszTemp + 3);
			wcstok((WCHAR*)g_Vars.RegParams.wszURI,L" ");
			Debug(L"URI: %s",g_Vars.RegParams.wszURI);
		}
		else if(!wcsnicmp(wszTemp,L"/CN:",4))
		{// get the phone number for outgoing calls (Call Number)
			SysFreeString((BSTR)g_Vars.bstrOutgoingNumber);
			g_Vars.bstrOutgoingNumber =SysAllocString(wszTemp + 4);
			wcstok((WCHAR*)g_Vars.bstrOutgoingNumber ,L" ");
			Debug(L"URI to call out to: %s",g_Vars.bstrOutgoingNumber);
		}
		else if(!wcsnicmp(wszTemp,L"/U:",3))
		{// get the SIP User Name
			SysFreeString((BSTR)g_Vars.RegParams.wszAccountName);
			g_Vars.RegParams.wszAccountName=SysAllocString(wszTemp + 3);
			wcstok((WCHAR*)g_Vars.RegParams.wszAccountName,L" ");
			Debug(L"Sip User Name: %s",g_Vars.RegParams.wszAccountName);
		}
		else if(!wcsnicmp(wszTemp,L"/P:",3))
		{// get the SIP Password
			SysFreeString((BSTR)g_Vars.RegParams.wszAccountPassword);
			g_Vars.RegParams.wszAccountPassword=SysAllocString(wszTemp + 3);
			wcstok((WCHAR*)g_Vars.RegParams.wszAccountPassword,L" ");
			Debug(L"Sip Password: %s",g_Vars.RegParams.wszAccountPassword);
		}
		else if(!wcsnicmp(wszTemp,L"/SX:",4))
		{// get the Exchange Server
			SysFreeString((BSTR)g_Vars.bstrExchServer);
			g_Vars.bstrExchServer=SysAllocString(wszTemp + 4);
			wcstok((WCHAR*)g_Vars.bstrExchServer,L" ");
			Debug(L"Exchange Server: %s",g_Vars.bstrExchServer);
		}
		else if(!wcsnicmp(wszTemp,L"/PX:",4))
		{// get teh Exchange Password
			SysFreeString(g_Vars.bstrExchPassword);
			g_Vars.bstrExchPassword=SysAllocString(wszTemp + 4);
			wcstok((WCHAR*)g_Vars.bstrExchPassword,L" ");
			Debug(L"Exchange Account Password: %s",g_Vars.bstrExchPassword);
		}
		else if(!wcsnicmp(wszTemp,L"/UX:",4))
		{// get the Exchange User Name
			SysFreeString(g_Vars.bstrExchUserName);
			g_Vars.bstrExchUserName=SysAllocString(wszTemp + 4);
			wcstok((WCHAR*)g_Vars.bstrExchUserName,L" ");
			Debug(L"Exchange Account: %s",g_Vars.bstrExchUserName);
		}
		else if(!wcsnicmp(wszTemp,L"/T:",3))
		{// get the Transport for the SIP Server
			SysFreeString((BSTR)g_Vars.RegParams.wszTransport);
            g_Vars.RegParams.wszTransport=SysAllocString(wszTemp + 3);
			wcstok((WCHAR*)g_Vars.RegParams.wszTransport,L" ");
			Debug(L"Transport: %s",g_Vars.RegParams.wszTransport);
		}
		else if(!wcsnicmp(wszTemp,L"/A:",3))
		{// get the SIP Authentication Method
			SysFreeString((BSTR)g_Vars.RegParams.wszAuthType);
			g_Vars.RegParams.wszAuthType=SysAllocString(wszTemp + 3);
			wcstok((WCHAR*)g_Vars.RegParams.wszAuthType,L" ");
			Debug(L"Authentication: %s",g_Vars.RegParams.wszAuthType);
		}
		else
		{// Didn't recognize the parameter
			wszTemp2 = wcsdup(wszTemp);
			Debug(L"Unknown Parameter: %s",wcstok(wszTemp2,L" "));
			free(wszTemp2);
		}
		wszTemp = wcspbrk(wszTemp + 2,L"/");
	}
	return S_OK;
}
INT CleanUpVoIP(CETKVoIPUI *pUI)
{// cleand up the VoIP test
	HRESULT hr = S_OK;
	if (pUI)
	{
		hr = pUI->Release();
	}
	return hr;
}
VOID CleanUpGlobals()
{// frees up the global strings that came from the command line, and the error string
	SysFreeString((BSTR)g_Vars.RegParams.wszAccountName);
	SysFreeString((BSTR)g_Vars.RegParams.wszAccountPassword);
	SysFreeString((BSTR)g_Vars.RegParams.wszAuthType);
	SysFreeString((BSTR)g_Vars.RegParams.wszServer);
	SysFreeString((BSTR)g_Vars.RegParams.wszTransport);
	SysFreeString((BSTR)g_Vars.RegParams.wszURI);
	SysFreeString((BSTR)g_Vars.bstrExchPassword);
	SysFreeString((BSTR)g_Vars.bstrExchServer);
	SysFreeString((BSTR)g_Vars.bstrExchUserName);
	if (g_Vars.bstrErrorString) SysFreeString(g_Vars.bstrErrorString);
	ZeroMemory((void*)&g_Vars,sizeof(GLOBAL_VARIABLES));
}

INT CETKVoIPRunSipTest(UINT uMsg,TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
// Check to see if tux wants to execute this test	
	if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

	
	HRESULT hr = S_OK;				// An hresult to use for return values
	MSG msg;						// A message for the message loop
	VoIPManagerState pulState = {0};// A DWORD that represents the state of the VoIP Manager
	LPTSTR LastErrString = NULL;	// A temp string to hold error strings
	
	//get the test case from the structure that tux passes us
	if (lpFTE)	g_Vars.iTestCaseID = lpFTE->dwUniqueID;
	else
	{// didn't get a test case abort
		Debug(L"The Function Table was blank, don't know what test to run.");
		return TPR_ABORT;
	}

	// everything went fine, start running the test
	Debug(L"######## Runnning TestCase %i ########",g_Vars.iTestCaseID);
	
	if(3 == g_Vars.iTestCaseID || 5 == g_Vars.iTestCaseID
		|| 6 == g_Vars.iTestCaseID)
	{
		if(NULL == g_Vars.RegParams.wszServer ||
			NULL == g_Vars.RegParams.wszURI ||
			NULL == g_Vars.RegParams.wszTransport ||
			NULL == g_Vars.RegParams.wszAuthType ||
			(5 == g_Vars.iTestCaseID && NULL == g_Vars.bstrOutgoingNumber) 
			)
		{
			RETAILMSG(1,(L"Command line parameters required"));
			RETAILMSG(1,(L"tux -d voipcetk -c \"/S:server /N:123@domain.com /T:udp /A:digest\""));
			RETAILMSG(1,(L"/S: Sip Server"));
			RETAILMSG(1,(L"/N: SIP URI "));
			RETAILMSG(1,(L"/T: Transport "));
			RETAILMSG(1,(L"/A: Authentication"));
			if(5 == g_Vars.iTestCaseID && NULL == g_Vars.bstrOutgoingNumber)
				RETAILMSG(1,(L"/CN: Number to call out"));
			return TPR_SKIP;
		}
		
		
	}
	//set the global return value (for the event handler to pass back) to pass
	g_Vars.iRetVal = TPR_PASS;

	//create a callback class for the VoIP Events
	CETKVoIPUI	*pUI = new CETKVoIPUI;
	if (NULL == pUI)
	{
		Debug(L"Could not allocate the VoIP Manager Callback class.");
		Debug(L"###### Failed Test Case %d ######",g_Vars.iTestCaseID);
		return TPR_FAIL;
	}

	if (FAILED(pUI->Initialize()))
	{// failed to initialize the VoIP Call back class
		g_Vars.iRetVal = TPR_FAIL;
	}

	if (FAILED(pUI->getMgrState(&pulState)))
	{// failed to get the state of the VoIP Manager
		Debug(L"Could not get state of Voip Manager, VoIP Manager not Present");	
		g_Vars.iRetVal = TPR_FAIL;
	}
	else if (!(pulState & VMS_INITIALIZED))
	{// VoIP Manager is not initialized, exit, it should be at this point
		Debug(L"Manager is not initialized");
		g_Vars.iRetVal = TPR_FAIL;
	}
	if (g_Vars.iTestCaseID == 1 || TPR_FAIL == g_Vars.iRetVal)
	{// if you ae running test case 1, you are done, exit
		if(TPR_FAIL == g_Vars.iRetVal)
			Debug(L"###### Test Case %d Failed, Verify VoIP Manager Presence ######",
				  g_Vars.iTestCaseID);
		else
			Debug(L"###### Test Case %d Passed, Verify VoIP Manager Presence ######",
				  g_Vars.iTestCaseID);

		pUI->Uninitialize();	// uninitialize the VoIP Manager
		while(GetMessage(&msg, NULL, 0, 0))
		{// phone message loop to allow it to uninitialize itself
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		CleanUpVoIP(pUI);
		return g_Vars.iRetVal;
	}

	// if not test case 1 continue
	PrintSipParams(g_Vars.RegParams);	// print out the sip parameters
	if (g_Vars.bstrErrorString) SysFreeString(g_Vars.bstrErrorString);

	// Register with the sip server
	hr = pUI->RegisterWithSipServer(&(g_Vars.RegParams));

	if (FAILED(hr))
	{// failed to register with the sip server.
		Debug(L"Failed attempted registration with the SIP Server:0x%x",hr);
		g_Vars.iRetVal = TPR_FAIL;
	}

	while(GetMessage(&msg, NULL, 0, 0))
	{// phone message loop
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	switch(g_Vars.iTestCaseID)
	{
	case 3: // Case 3 is verifying that you can register with your SIP server
		if (g_Vars.iRetVal == TPR_PASS && SUCCEEDED(hr))
			Debug(L"###### Test Case %d Passed, Verify Registration with SIP Server ######",
				  g_Vars.iTestCaseID);
		else
			Debug(L"###### Test Case %d Failed, Verify Registration with SIP Server ######",
				  g_Vars.iTestCaseID);

		CleanUpVoIP(pUI);
		return g_Vars.iRetVal;
	case 5: // Case 5 is to connect an outgoing call
		if(pUI->OutgoingCallSucceeded())
		{
			Debug(L"###### Test Case %d Passed, Outgoing Call ######",g_Vars.iTestCaseID);
			CleanUpVoIP(pUI);
			return g_Vars.iRetVal;
		}
		else
		{
			Debug(L"###### Test Case %d Failed, Outgoing Call ######",g_Vars.iTestCaseID);
			CleanUpVoIP(pUI);
			return TPR_FAIL;
		}
	case 6: // Case 6 is to answer an incomming call
		if(pUI->IncomingCallSucceeded())
		{
			Debug(L"###### Test Case %d Passed, Incoming Call ######",g_Vars.iTestCaseID);
			CleanUpVoIP(pUI);
			return g_Vars.iRetVal;
		}
		else
		{
			Debug(L"###### Test Case %d Failed, Incoming Call ######",g_Vars.iTestCaseID);
			CleanUpVoIP(pUI);
			return TPR_FAIL;
		}
	default:
		Debug(L"Unsupported Test Case ID for this function: %d",g_Vars.iTestCaseID);
		CleanUpVoIP(pUI);
		return TPR_ABORT;
	}
}

/////////////////////////////////////////////////////////////////////////////////
//this is the class that represents the event handler for the VoIP Manager
//it handels all the events and managemente of the VoIP Manager
CETKVoIPUI::CETKVoIPUI()
{
	m_cRefs			=	0;		// reference count for this object
	m_piCurrentCall	=	NULL;	// a pointer to the current call
	m_CallAnswering =	FALSE;	// boolean for if a call was answered
	m_CallConnected =	FALSE;	// boolean for if a call was connected
	m_CallInProgress=	FALSE;	// boolean for if a call was set in progress
	m_IncomingCall	=	FALSE;	// boolean for if a call was incoming
	AddRef();					// add a reference to this object
}
/*------------------------------------------------------------------------------
    CETKVoIPUI::~CETKVoIPUI
    
    Destructor.
------------------------------------------------------------------------------*/
CETKVoIPUI::~CETKVoIPUI(){}

HRESULT CETKVoIPUI::RegisterWithSipServer(SIPServerRegistrationParameters *pSipRegParams)
{//simply returns the VoIP Manager's registration results
	return m_pMgr->RegisterWithSIPServer(pSipRegParams);
}

HRESULT CETKVoIPUI::getMgrState(VoIPManagerState *pulState)
{// simply returns the state of the VoIP Manager, passthrougth fucntion
	return m_pMgr->get_State(pulState);
}

BOOL CETKVoIPUI::IncomingCallSucceeded()
{//checks the variables to make sure that the incoming call succeeded
	if (!m_IncomingCall) Debug(L"Incoming Call event never fired");
	if (!m_CallAnswering) Debug(L"Answer Call event never fired");
	if (!m_CallConnected) Debug(L"Call Connected event never fired");
	return m_IncomingCall && m_CallAnswering && m_CallConnected;
}
BOOL CETKVoIPUI::OutgoingCallSucceeded()
{//checks the variables to make sure that an outgoing call succeeded
	if (!m_CallInProgress) Debug(L"Call in Progress event never fired");
	if (!m_CallConnected) Debug(L"Call Connected event never fired");
	return m_CallInProgress && m_CallConnected;
}

/*------------------------------------------------------------------------------
    CETKVoIPUI::OnSystemEvent
    
    Callback function from the VoIP Manager for system-related events.
------------------------------------------------------------------------------*/
STDMETHODIMP CETKVoIPUI::OnSystemEvent(
    /*[in]*/ VoIPSystemEvent vseEvent,
    /*[in]*/ INT_PTR iptrParams)
{
    HRESULT hr = S_OK;
    
    switch(vseEvent)
    {
    case e_vseTerminated:
		Debug(L"System Event: Terminated");
		m_pMgr->Release();
		PostQuitMessage(0);
        break;

    case e_vseRegistering:
		Debug(L"System Event: Registering");
        break;
        
    case e_vseRegistrationFailed:
		Debug(L"System Event: Registration Failed");
		this->Uninitialize();
		g_Vars.iRetVal = TPR_FAIL;
		break;

    case e_vseRegistrationSucceeded:
		Debug(L"System Event: Registration Succeeded\n");
		VoIPManagerState pulState = {0};
		if (FAILED(m_pMgr->get_State(&pulState)))
		{// is this redundant 
			Debug(L"Could not get state of Voip Manager");	
			g_Vars.iRetVal = TPR_FAIL;
		}
		else 
		{// switch on the state of the VoIP Manager
			if(pulState & VMS_REGISTERED)
			{
				Debug(L"Voip Manager is Registered");
				g_Vars.iRetVal = TPR_PASS;
			}
			if(VMS_NOT_READY == pulState)
			{
				Debug(L"*******Voip Manager is not Ready");
				g_Vars.iRetVal = TPR_FAIL;
			}
			if (pulState & VMS_SHUTTING_DOWN)
			{
				Debug(L"********Voip Manager is shutting down");
				g_Vars.iRetVal = TPR_FAIL;
			}
			if (pulState & VMS_REREGISTERING)
			{
				Debug(L"Voip Manager is still registering");
				g_Vars.iRetVal = TPR_FAIL;
			}
		}

		switch (g_Vars.iTestCaseID)
		{
			case 3:
				// if test case 3, we are done
				this->Uninitialize();
				break;
			case 5:
				// test case 5 make an outgoing call
				hr = m_pMgr->Call(g_Vars.bstrOutgoingNumber,&(m_piCurrentCall));
				break;
			case 6:
				// test case 6 accept an incoming call
				Debug(L"######## PHONE IS READY TO RECEIVE A CALL ########");
				m_pMgr->get_MyURI(&m_bstrURI);
				Debug(L"######## Call %s ",m_bstrURI);
				SysFreeString(m_bstrURI);
				break;
		}
        break;
    }
    return hr;
}


/*------------------------------------------------------------------------------
    CETKVoIPUI::OnCallEvent
    
    Callback function from the VoIP Manager for call-related events.
------------------------------------------------------------------------------*/
STDMETHODIMP CETKVoIPUI::OnCallEvent(
    /*[in]*/ VoIPCallEvent vceEvent,
    /*[in]*/ IVoIPCurrentCall *piCall)
{
	VoIPCallStatus	callStatus	= e_vcsIdle;
	static int inProgress =0;
	
	PrintEvent(vceEvent);
	if((vceEvent == e_vceStatusChanged) && piCall)
	{
		m_hr = piCall->get_Status(&callStatus);
		if(FAILED(m_hr)) Debug(L"Could not get Call Status, Error:0x%x",m_hr);
		else
		{
			PrintCallStatus(callStatus);
			switch(callStatus)
			{// handle the event based on status
			case e_vcsIdle:	break;	
			case e_vcsAnswering: m_CallAnswering = TRUE; break;
			case e_vcsDisconnected:	this->Uninitialize(); break;
			case e_vcsInProgress: 
				m_CallInProgress = TRUE; 
				// inProgress used to print debug message only once
				if(0 == inProgress && 5 == g_Vars.iTestCaseID)
				{
					RETAILMSG(1,(L"##### The called number should be ringing."));
					RETAILMSG(1,(L"##### Pickup the phone and the test will hangup the call."));
				}
				if(5 == g_Vars.iTestCaseID)
					inProgress++;
				break;
			case e_vcsConnected:
			    m_CallConnected = TRUE;
				Debug(L"Call connected.");			
				m_hr = piCall->Hangup();
				if(FAILED(m_hr))
				{
					Debug(L"Could not hangup the call."); 
					PrintError(m_hr);
					this->Uninitialize();
				}
				break;
			case e_vcsIncoming: // should check the test case
				m_IncomingCall = TRUE;
				m_hr = piCall->Answer(TRUE);
				if(FAILED(m_hr))
				{
					Debug(L"Could not answer the incoming call.");
					PrintError(m_hr);
					this->Uninitialize();
				}
				break;
			/* technically these cases should be handled, but they are irellivent to these cases
			case e_vcsReferring: Debug(L"Call should not have been redirected"); break;
			case e_vcsHolding:
				Debug(L" Call should not have been put on hold, unholding");
				hr = piCall->Unhold();
				if(FAILED(m_hr))
				{
					Debug(L"Could not unhold the call.");
					PrintError(m_hr);
					this->Uninitialize();
				}
				break;
			case e_vcsInConference:
				Debug(L"The call should not be conferance, something is wrong");
				break;
			/**/
			}
		}
	}
	return m_hr;
}

/*------------------------------------------------------------------------------*/
HRESULT CETKVoIPUI::Initialize()
{
   	HRESULT hr = S_OK;
 	//Create a VoIP Manager
	hr = CoCreateInstance(CLSID_VoIPMgr, 
						  NULL, 
						  CLSCTX_INPROC_SERVER, 
						  IID_IVoIPMgr2, 
						  (void**) &(m_pMgr));
	
	if(FAILED(hr))
	{
		Debug(L"FAILED: CoCreateInstance IVoIPMgr2 in "
              L"Initialize() failed with error code: 0x%x", hr);
		return E_FAIL;
	}
	//Initialize the manager

	if (SUCCEEDED(hr))
	{
		hr = m_pMgr->InitializeEx(this);
		if(FAILED(hr))
		{// if the managr fails to initialize then 
			Debug(L"FAILED: IntializeEx in InitialSetup() failed to "
		          L"initialize the VoIP Manager with error code: 0x%x", hr);
			switch(hr)
			{
				case VOIP_E_ALREADYINITIALIZED:
					if(g_Vars.bstrErrorString) SysFreeString(g_Vars.bstrErrorString);
					m_bstrErrorString = SysAllocString(L"The VoIP manager is already "
															L"initialized.");
					break;
				case VOIP_E_BUSY: 
					if(g_Vars.bstrErrorString) SysFreeString(g_Vars.bstrErrorString);
					m_bstrErrorString = SysAllocString(L"The manager is busy.");
					break; 
				case VOIP_E_NOMEDIAMGR: 
					if(g_Vars.bstrErrorString) SysFreeString(g_Vars.bstrErrorString);
					m_bstrErrorString = SysAllocString(L"No media manager is available "
															L"for use by the VoIP manager.");
					break;
				case VOIP_E_PROVISIONING_FAILED: 
					if(g_Vars.bstrErrorString) SysFreeString(g_Vars.bstrErrorString);
					m_bstrErrorString = SysAllocString(L"The method failed to provision the "
															L"phone during initialization.");
					break;
				default:
					if(g_Vars.bstrErrorString) SysFreeString(g_Vars.bstrErrorString);
					m_bstrErrorString = SysAllocString(L"Unknown Error.");
					break;
			}
			Debug(L"Error: 0x%x, %s",hr,m_bstrErrorString);
			SysFreeString(m_bstrErrorString);
			return E_FAIL;
		}

	}
	return S_OK;
}
//--------------------------------------------------------------------------------------------
HRESULT CETKVoIPUI::Uninitialize()
{
    IVoIPUI *piUI = NULL;
    HRESULT hr    = S_OK;

	Debug(L"Uninitializing VoipManager");
	hr = m_pMgr->Uninitialize();
    if(FAILED(hr))
		Debug(L"FAILED: m_pMgr->Uninitialize() failed with returned hr = 0x%x",hr);
    return hr;
}
//////////////////////////////////////////////////////////////////////////////////////
// COM methods
HRESULT STDMETHODCALLTYPE CETKVoIPUI::QueryInterface(REFIID riid, VOID** ppvObj)
{
	if (ppvObj == NULL)
	{
		return E_POINTER;
	}

	if (riid == IID_IUnknown)
	{
	*ppvObj = static_cast<IUnknown*>(this);
		AddRef();
	}
	else if (riid == IID_IVoIPUI)
	{
		*ppvObj = static_cast<CETKVoIPUI*>(this);
		AddRef();
	}
	else
	{
		return E_NOINTERFACE;
	}
	return S_OK;
}

ULONG STDMETHODCALLTYPE CETKVoIPUI::AddRef()  
{ 
	return InterlockedIncrement(&m_cRefs); 
}

ULONG STDMETHODCALLTYPE CETKVoIPUI::Release() 
{ 
	if((InterlockedDecrement(&m_cRefs) == 0))
	{
		delete this;
		return 0;
	}
	return m_cRefs; 
} 

////////////////////////////////////////////////////////////////////////////////
//##############################################################################
//Following is the code to verify the exchange client
////////////////////////////////////////////////////////////////////////////////
INT CETKVoIPRunExchTest(UINT uMsg,TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
// Check to see if tux wants to execute this test	
	if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
	
	IExchangeClient * piExchangeClient = NULL;	// instanciation of the Exchange Client
	HRESULT hr;									// result for general use throughout
	MSG msg;									// a message for the message pump
	DWORD dwErr = 0;							// a holder for GetLastError Returns


	// set the test case into the global variable so the call back function has access to it
	if (lpFTE)	g_Vars.iTestCaseID = lpFTE->dwUniqueID;
	else
	{// didn't have a test case
		Debug(L"The Function Table was blank, don't know what test to run.");
		return TPR_ABORT;
	}
	Debug(L"######## Runnning TestCase %i ########",g_Vars.iTestCaseID);

	if(4 == g_Vars.iTestCaseID)
	{
		if(NULL == g_Vars.bstrExchUserName ||
			NULL == g_Vars.bstrExchPassword ||
			NULL == g_Vars.bstrExchServer)
		{
			RETAILMSG(1,(L"Command line parameters required"));
			RETAILMSG(1,(L"tux -d voipcetk -c \"/SX:http://ExchServer /UX:domain\\user /PX:password\""));
			RETAILMSG(1,(L"/SX: Exchange Server"));
			RETAILMSG(1,(L"/UX: Exchange User Name"));
			RETAILMSG(1,(L"/PX: Exchange Password "));
			return TPR_SKIP;
		}
		
	}
	// Create ExchangeClient
	CETKExchClientRequestCallback*  piCallback = new CETKExchClientRequestCallback;
	if (NULL == piCallback)
	{
		Debug(L"Could not allocate memory for the Exchange Client callback class");
		return TPR_FAIL;
	}

	// set the callback function it the ExchangeClient and initialize it
	hr = piCallback->InitializeExchangeClient(&piExchangeClient);
	if FAILED(hr)
	{// print out why initialization failed
		dwErr = GetLastError(); // get the last error first incase the error occured in 
								// a component below VoIP and we want that error
		SysFreeString(g_Vars.bstrErrorString);
		switch(hr)
		{
		case E_POINTER:
			g_Vars.bstrErrorString = SysAllocString(L"The address "
					L"in piCallback does not point to a valid object."
					L"The Exchange client library cannot initialize.");
		case OWAEC_E_ALREADYINITIALIZED:
			g_Vars.bstrErrorString = SysAllocString(L"This instance of "
					L"the Exchange client library has already been initialized."); 
		default:
			g_Vars.bstrErrorString =SysAllocString(L"Unknown Error");
			break;
		}
		Debug(L"Failed to Initialize the OWA Exchange Client: HR:0x%x, %i, Last"
			  L"Error:0x%x, %i,%s",hr,hr,dwErr,dwErr,g_Vars.bstrErrorString);
		SysFreeString(g_Vars.bstrErrorString);
		piCallback->Release();
		return TPR_FAIL;
	}
	else if (2 == g_Vars.iTestCaseID) 
	{// if running test case 2 then the test is over
		Debug(L"###### Passed Test case 2, Verify OWA Exchange Client Presence ######");
		piCallback->Release();
		return TPR_PASS;
	}

	// if not runnint test case to then contact the server for a list of clients
	ContactsSearchCriteria Criteria = {0};
	Debug(L"Server: %s",g_Vars.bstrExchServer);			
	Debug(L"User Name: %s",g_Vars.bstrExchUserName);	
	Debug(L"Password: %s",g_Vars.bstrExchPassword);
	IExchangeClientRequest *piRequest = NULL;
	// set the server name into the Exchange Client object
	hr = piExchangeClient->SetServer(g_Vars.bstrExchServer);
	// set the credentials into the Exchange Client object
	if(SUCCEEDED(hr)) hr = piExchangeClient->SetCredentials(g_Vars.bstrExchUserName,
															g_Vars.bstrExchPassword);
	// make a request to the server, any will do, we will ask for contacts
	if(SUCCEEDED(hr)) hr = piExchangeClient->RequestContacts(&Criteria, &piRequest);
	if FAILED(hr)
	{// if the call failed outright, print out why and exit
		dwErr = GetLastError();
		switch (hr)
		{
		case E_POINTER:
			if(g_Vars.bstrErrorString) SysFreeString(g_Vars.bstrErrorString);
			g_Vars.bstrErrorString = SysAllocString(L"The address of pCriteria does not point to "
													L"a structure.");
			break;
		case E_OUTOFMEMORY:
			if(g_Vars.bstrErrorString) SysFreeString(g_Vars.bstrErrorString);
			g_Vars.bstrErrorString = SysAllocString(L"Not enough memory was available to initiate "
													L"the request.");
			break;
		case OWAEC_E_NOTINITIALIZED:
			if(g_Vars.bstrErrorString) SysFreeString(g_Vars.bstrErrorString);
			g_Vars.bstrErrorString = SysAllocString(
				L"This instance of the Exchange client has not yet been initialized. "
				L"It must be initialized using the IExchangeClient::Initialize method.");
			break;
		default:
			if(g_Vars.bstrErrorString) SysFreeString(g_Vars.bstrErrorString);
			g_Vars.bstrErrorString = SysAllocString(L"unknown error");
			break;
		}
		Debug(L"Failed to request contacts, hr:0x%x, %i Err:0x%x, %i; %s",
			  hr, hr, dwErr, dwErr, g_Vars.bstrErrorString);
		return TPR_FAIL;
	}

	while(GetMessage(&msg, NULL, 0, 0))
	{// message loop for handeling the request events.
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	switch(g_Vars.iTestCaseID)
	{
	case 4: // test case 4 is verify connection with exchange server
		if (TPR_PASS == g_Vars.iRetVal)
			Debug(L"###### Passed Test case 4, Verify access to exchange server ######");
		return g_Vars.iRetVal;
	default:
		Debug(L"Unsupported Test Case ID");
		return TPR_ABORT;
	}
}
	
////////////////////////////////////////////////////////////////////////////////
// this is the call back class registered with the Exchange client
//
HRESULT CETKExchClientRequestCallback::InitializeExchangeClient(IExchangeClient	**ppIExchangeClient)
{// initializes the client and sets this class as its callback method
	HRESULT hr;

	// create instance of the Exchange Client
	hr = CoCreateInstance(CLSID_ExchangeClient, //clsid
				 		  NULL,                 //outer ref
						  CLSCTX_INPROC_SERVER, //server type
						  IID_IExchangeClient,
						  (void**)ppIExchangeClient);
	if(FAILED(hr))
	{
		Debug(L"Failed to create Instance of Exchange Client");
	}
	// initialize the Exchange client
	if (SUCCEEDED(hr)) hr = (*ppIExchangeClient)->Initialize(this);
	if(FAILED(hr))
	{
		Debug(L"Failed to Initialize the Exchange Client");
	}
	if (SUCCEEDED(hr)) this->SetExchangeClient(*ppIExchangeClient);
	return hr;
}

HRESULT STDMETHODCALLTYPE CETKExchClientRequestCallback::OnRequestProgress(
						  IExchangeClientRequest *piRequest,
						  ExchangeClientRequestStatus ecrs)
{// handle a request progress, just print it out and handle as a success or failure
	PrintExchangeClientStatus(ecrs);
	switch (ecrs)
	{
	case e_ecrsPending:
	case e_ecrsInProgress:
	case e_ecrsSending:
	case e_ecrsBypassingOWAPage:
		// none of these cases mean anything, just skip it.
		break;
  	case e_ecrsSucceeded:
		//On Request success, go ahead and exit message loop
		Debug(L"Exchange Client Request Succeeded!");
		m_pIExchClient->Uninitialize();
		g_Vars.iRetVal = TPR_PASS;
		break;
	// On Request Failure
	case e_ecrsCancelled:
		Debug(L"Why did the attempt get Canceled?");
	case e_ecrsFailedToBypassAuthPage:
	case e_ecrsFailedToSend:
	case e_ecrsHttpFailure:
	case e_ecrsOutOfMemory:
	case e_ecrsParseFailed:
	case e_ecrsNoCredentials:
		// failed, so exit message loop, message printed out above
		Debug(L"Exchange Client Request Failed!");
		g_Vars.iRetVal = TPR_FAIL;
		m_pIExchClient->Uninitialize();
		break;
	default:
		Debug(L"Unknown status: %i", ecrs);
		break;
	}
	return S_OK;
}


VOID CETKExchClientRequestCallback::SetExchangeClient(IExchangeClient *pIExchangeClient)
{//just a pass through so that an Exchange client object isn't needed in calling application
	m_pIExchClient = pIExchangeClient;
}

HRESULT STDMETHODCALLTYPE CETKExchClientRequestCallback::OnShutdown()
{//this function just exits the message loop and cleans up
	delete(m_pIExchClient);
	this->Release();
	PostQuitMessage(0);
	return S_OK;
}

//Constructor
CETKExchClientRequestCallback::CETKExchClientRequestCallback() : m_cRef(1){}

///////////////////////////////////////////////////////////////////////////
//these are the com implimentations 
ULONG STDMETHODCALLTYPE CETKExchClientRequestCallback::AddRef()  
{ 
	return InterlockedIncrement(&m_cRef); 
}

ULONG STDMETHODCALLTYPE CETKExchClientRequestCallback::Release() 
{ 
	if((InterlockedDecrement(&m_cRef) == 0))
	{
		delete this;
		return 0;
	}
	return m_cRef; 
} 

HRESULT STDMETHODCALLTYPE CETKExchClientRequestCallback::QueryInterface(REFIID riid, VOID** ppvObj)
{
	if (ppvObj == NULL)
	{
		return E_POINTER;
	}

	if (riid == IID_IUnknown)
	{
		*ppvObj = static_cast<IUnknown*>(this);
		AddRef();
	}
	else if (riid == IID_IExchangeClientRequestCallback)
	{
		*ppvObj = static_cast<IExchangeClientRequestCallback*>(this);
		AddRef();
	}
	else
	{
		return E_NOINTERFACE;
	}
	return S_OK;
}
