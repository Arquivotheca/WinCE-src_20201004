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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// This test cycles the RF attenuation values of two WiFi Access Points in
// oposite directions to force the WiFi Station(s) sharing the Azimuth RF
// isolation enclosures to repeatedly roam back and forth between the APs.
//
// ----------------------------------------------------------------------------

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <winsock2.h>
#include <windows.h>
#include <netmain.h>
#include <cmnprint.h>

#include <APPool_t.hpp>
#include <TestController_t.hpp>
#include <WiFUtils.hpp>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Run-time arguments:
//

// Test server's host name or address:
static TCHAR DefaultServerHost[] = TEXT("localhost");
static TCHAR    *s_pServerHost   = DefaultServerHost;

// Test server's port number:
static TCHAR DefaultServerPort[] = TEXT("33331");
static TCHAR    *s_pServerPort   = DefaultServerPort;

// Total test duration, in seconds:
static int DefaultTestDuration = 900;
static int      s_TestDuration = DefaultTestDuration;

// Time, in seconds, to roam from one AP to the other:
static int DefaultRoamTime = 30;
static int      s_RoamTime = DefaultRoamTime;

// Time, in milliseconds, between attenuation changes:
static int DefaultAttenuationTime = 2000;
static int      s_AttenuationTime = DefaultAttenuationTime;

// ----------------------------------------------------------------------------
//
// External variables for netmain:
//
WCHAR *gszMainProgName = L"AzAtten";
DWORD  gdwMainInitFlags = 0;

// ----------------------------------------------------------------------------
//
// Local test-controller class:
//
class MyTestController_t : public TestController_t
{
private:
    APPool_t *m_pAPPool;
public:
    HRESULT
    Start(APPool_t *pAPPool)
    {
        m_pAPPool = pAPPool;
        return TestController_t::Start();
    }
protected:
    virtual DWORD
    Run(void);
};

// ----------------------------------------------------------------------------
//
// Prints the program usage.
//
void
PrintUsage()
{
	LogAlways(TEXT("++Remote Azimuth attenuation-control tester++"));
	LogAlways(TEXT("%ls [-a change-time] [-d total-time] [-r roam-time] [-s server-name] [-p server-port]"), gszMainProgName);
	LogAlways(TEXT("options:"));
    LogAlways(TEXT("   -a   time between attenuation changes (default %d milliseconds)"), DefaultAttenuationTime);
    LogAlways(TEXT("   -r   time to roam between APs (default %d seconds)"), DefaultRoamTime);
    LogAlways(TEXT("   -d   total test duration (default %d seconds)"), DefaultTestDuration);
    LogAlways(TEXT("   -s   name/address of status-server (default: %s)"), DefaultServerHost);
    LogAlways(TEXT("   -p   port to connect on (default: %s)"), DefaultServerPort);
    LogAlways(TEXT("   -?   show this information"));
}

// ----------------------------------------------------------------------------
//
int 
mymain(int argc, TCHAR **argv) 
{
    MyTestController_t tester;
    APPool_t           apList;
    DWORD              result;
    HRESULT            hr;
    
    CmnPrint_Initialize();
	CmnPrint_RegisterPrintFunctionEx(PT_LOG,     NetLogDebug,   FALSE);
 	CmnPrint_RegisterPrintFunctionEx(PT_WARN1,   NetLogWarning, FALSE);
 	CmnPrint_RegisterPrintFunctionEx(PT_WARN2,   NetLogWarning, FALSE);
	CmnPrint_RegisterPrintFunctionEx(PT_FAIL,    NetLogError,   FALSE);
	CmnPrint_RegisterPrintFunctionEx(PT_VERBOSE, NetLogMessage, FALSE);

	// Parse the run-time arguments.    
	if (QAWasOption(TEXT("?")) || QAWasOption(TEXT("help")))
	{
		PrintUsage();
		return 0;
	}
    QAGetOptionAsInt(TEXT("a"), &s_AttenuationTime);
    QAGetOptionAsInt(TEXT("d"), &s_TestDuration);
    QAGetOptionAsInt(TEXT("r"), &s_RoamTime);
    QAGetOption     (TEXT("s"), &s_pServerHost);
    QAGetOption     (TEXT("p"), &s_pServerPort);

    if (s_AttenuationTime <   500)
        s_AttenuationTime =   500;
    if (s_AttenuationTime > 15000)
        s_AttenuationTime = 15000;
    
    if (s_RoamTime <  15)
        s_RoamTime =  15;
    if (s_RoamTime > 900)
        s_RoamTime = 900;

    if (s_TestDuration <    15)
        s_TestDuration =    15;
    if (s_TestDuration > 86400)
        s_TestDuration = 86400;
    
    if (s_RoamTime < s_AttenuationTime / 1000)
        s_RoamTime = s_AttenuationTime / 1000;
    
    // Initialize WinSock.
    WSADATA wsaData;
	result = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (0 != result)
    {
        LogError(TEXT("WSAStartup failed: %s"), Win32ErrorText(result));
        return(-1);
    }
    
    // Read the initial AP-controller configurations from the registry.
    hr = apList.LoadAPControllers(s_pServerHost, s_pServerPort);
    if (FAILED(hr))
    {
        result = HRESULT_CODE(hr);
        LogError(TEXT("Initial configuration failed: %s"), 
                 Win32ErrorText(result));
        goto Cleanup;
    }

    // Make sure there are two APs in the list.
    if (2 != apList.size())
    {
        result = ERROR_BAD_ARGUMENTS;
        LogError(TEXT("Found %d APs in registry - should be 2."), 
                 apList.size());
        goto Cleanup;
    }

    // Have the test-controller run the test in a sub-thread.
    hr = tester.Start(&apList);
    if (FAILED(hr))
    {
        result = HRESULT_CODE(hr);
        LogError(TEXT("Cannot run test-controller thread: %s"),
                 Win32ErrorText(result));
        goto Cleanup;
    }

    // Wait for the test thread to finish.
    result = tester.Wait(s_TestDuration * 1000);
    if (ERROR_SUCCESS != result && WAIT_TIMEOUT != result)
    {
        LogError(TEXT("Test failed: %s"),
                 Win32ErrorText(result));
        goto Cleanup;
    }

    LogDebug(TEXT("Test succeeded"));
    result = ERROR_SUCCESS;

Cleanup:

    // Close the APControllers.
    apList.Clear();
        
    // Shut down WinSock.
	WSACleanup();
	return (ERROR_SUCCESS == result)? 0 : -1;
}


// ----------------------------------------------------------------------------
//
// Runs the test in a sub-thread.
//
DWORD
MyTestController_t::
Run(void)
{
    HRESULT hr;
    
    for (int loser = 0 ; !IsInterrupted() ; loser = 1 - loser)
    {
        APController_t *apUP = m_pAPPool->GetAP(  loser);
        APController_t *apDN = m_pAPPool->GetAP(1-loser);
        LogDebug(TEXT("Roaming from %s to %s"), &apUP->GetAPName()[0],
                                                &apDN->GetAPName()[0]);

        long roamStart = GetTickCount();
        while (!IsInterrupted())
        {
            long stepStart = GetTickCount();
            long runTime = WiFUtils::SubtractTickCounts(roamStart, stepStart);
            if (runTime >= (s_RoamTime * 1000))
                break;
            double midpt = (double)runTime / (double)(s_RoamTime * 1000);

            double rangUP = apUP->GetMaxAttenuation() - apUP->GetMinAttenuation();
            double rangDN = apDN->GetMaxAttenuation() - apDN->GetMinAttenuation();

            int attnUP = apUP->GetMinAttenuation() + (int)(rangUP * midpt + 0.5);
            int attnDN = apDN->GetMaxAttenuation() - (int)(rangDN * midpt + 0.5);

            if (FAILED(hr = apUP->SetAttenuation(attnUP))
             || FAILED(hr = apUP->SaveConfiguration()))
            {
                LogError(TEXT("Failed setting %s attenuation to %d: %s"),
                         apUP->GetAPName(), attnUP,
                         HRESULTErrorText(hr));
                return HRESULT_CODE(hr);
            } 
            
            if (FAILED(hr = apDN->SetAttenuation(attnDN))
             || FAILED(hr = apDN->SaveConfiguration()))
            {
                LogError(TEXT("Failed setting %s attenuation to %d: %s"),
                         apDN->GetAPName(), attnDN,
                         HRESULTErrorText(hr));
                return HRESULT_CODE(hr);
            }

            long stepTime = WiFUtils::SubtractTickCounts(stepStart, GetTickCount());
            if (s_AttenuationTime > stepTime)
            {
                Sleep(s_AttenuationTime - stepTime);
            }
        }
    }
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
