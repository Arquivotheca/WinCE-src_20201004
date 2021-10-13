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
// This test cycles through a series of Access Points using RF attenuators
// to simulate roaming out of the range of each in turn to force the mobile
// device(s) under test to disassociate and re-associate to maintain continuous
// network connectivity.
//
// Two additional states are thrown in to simulate a real-world environment.
// In the first state, all the Access Points are fully-attenuated and, hence,
// the mobile devices are forced to use the cellular network (GPRS).
//
// In the last, optional, state, a USB connection is switched on connecting
// one of the mobile devices, through a desktop, to the wired network (DTPT).
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
#include <inc/auto_xxx.hxx>

#include <APPool_t.hpp>
#include <WiFUtils.hpp>
#include <TestController_t.hpp>
#include <USBClicker_t.hpp>

#if UNDER_CE
#include <NetLogManager_t.hpp>
#endif

#include <strsafe.h>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Run-time arguments:
//

// Total test duration, in seconds:
static int DefaultTestDuration = 900;
static int MinimumTestDuration = 60;
static int      s_TestDuration = DefaultTestDuration;

// Time, in seconds, to roam from one AP to the next:
static int DefaultRoamTime = 90;
static int MinimumRoamTime = 30;
static int      s_RoamTime = DefaultRoamTime;

// Time, in seconds, between each attenuation-adjustment step:
static int DefaultStepTime = 4;
static int MinimumStepTime = 1;
static int      s_StepTime = DefaultStepTime;

// Percentage-attenuation where the signal-strength of two APs should meet:
// Low values increase the possibility of simulating an RF "dead spot"
// between each AP.
static int DefaultCrossoverPct = 75;
static int      s_CrossoverPct = DefaultCrossoverPct;

// USB Clicker registry key:
static TCHAR DefaultUSBRegiKey[] = TEXT("Software\\Microsoft\\CETest\\WiFiRoamUSB");
static TCHAR    *s_pUSBRegiKey = DefaultUSBRegiKey;

#ifdef UNDER_CE
// Netlog capture-file directory-name (optional):
static TCHAR *s_pNetlogFilePath = NULL;
#endif

// Test server's host name or address:
static TCHAR DefaultServerHost[] = TEXT("localhost");
static TCHAR    *s_pServerHost   = DefaultServerHost;

// Test server's port number:
static TCHAR DefaultServerPort[] = TEXT("33331");
static TCHAR    *s_pServerPort   = DefaultServerPort;

// ----------------------------------------------------------------------------
//
// External variables for netmain:
//
WCHAR *gszMainProgName = L"WiFiRoamTest";
DWORD  gdwMainInitFlags = 0;

// ----------------------------------------------------------------------------
//
// Local test-controller class:
//
class MyTestController_t : public TestController_t
{
public:

    // APController list:
    APPool_t *m_pAPPool;

    // USB clicker interface (optional):
    USBClicker_t *m_pClicker;

#ifdef UNDER_CE
    // Netlog manager interface (optional):
    NetlogManager_t *m_pNetlog;
#endif

    MyTestController_t(void)
        : m_pClicker(NULL)
#ifdef UNDER_CE
         ,m_pNetlog(NULL)
#endif
    { }

    virtual DWORD Run(void);
};

// ----------------------------------------------------------------------------
//
// Prints the program usage.
//
static void
PrintUsage()
{
	LogAlways(TEXT("++WiFi Roaming Test++"));
	LogAlways(TEXT("%ls [options]"), gszMainProgName);
	LogAlways(TEXT("options:"));
    LogAlways(TEXT("   -v   verbose debug output"));
    LogAlways(TEXT("   -z   send debug output to standard output"));
    LogAlways(TEXT("   -d   total test duration (default %d seconds)"), DefaultTestDuration);
    LogAlways(TEXT("   -r   time to roam between APs (default %d seconds)"), DefaultRoamTime);
    LogAlways(TEXT("   -a   time between attenuator steps (default %d seconds)"), DefaultStepTime);
    LogAlways(TEXT("   -c   AP's RSSI crossover (default %d%% attenuation, min=50%%)"), DefaultCrossoverPct);
    LogAlways(TEXT("   -s   name/address of server (default \"%s\")"), DefaultServerHost);
    LogAlways(TEXT("   -p   server port (default: \"%s\")"), DefaultServerPort);
    LogAlways(TEXT("   -u   USB clicker registry key (default \"%s\")"), DefaultUSBRegiKey);
#ifdef UNDER_CE
    LogAlways(TEXT("   -n   Netlog capture-file directory-name (default none)"));
#endif
    LogAlways(TEXT("   -?   show this information"));
}

// ----------------------------------------------------------------------------
//
int 
mymain(int argc, WCHAR **argv) 
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

	// Parse and validate the run-time arguments.    
	if (QAWasOption(TEXT("?")) || QAWasOption(TEXT("help")))
	{
		PrintUsage();
		return 0;
	}
    QAGetOptionAsInt(TEXT("d"), &s_TestDuration);
    QAGetOptionAsInt(TEXT("r"), &s_RoamTime);
    QAGetOptionAsInt(TEXT("a"), &s_StepTime);
    QAGetOptionAsInt(TEXT("c"), &s_CrossoverPct);
    QAGetOption     (TEXT("s"), &s_pServerHost);
    QAGetOption     (TEXT("p"), &s_pServerPort);
    QAGetOption     (TEXT("u"), &s_pUSBRegiKey);
#ifdef UNDER_CE
    QAGetOption     (TEXT("n"), &s_pNetlogFilePath);
#endif

    if (s_TestDuration < MinimumTestDuration)
        s_TestDuration = MinimumTestDuration;
    
    if (s_RoamTime < MinimumRoamTime)
        s_RoamTime = MinimumRoamTime;
    if (s_RoamTime > s_TestDuration)
        s_RoamTime = s_TestDuration;
    
    if (s_StepTime < MinimumStepTime)
        s_StepTime = MinimumStepTime;
    if (s_StepTime > s_RoamTime)
        s_StepTime = s_RoamTime;
    
    if (s_CrossoverPct < 50)
        s_CrossoverPct = 50;
    if (s_CrossoverPct > 100)
        s_CrossoverPct = 100;

    // Initialize WinSock.
    WSADATA wsaData;
	result = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (0 != result)
    {
        result = GetLastError();
        LogError(TEXT("WSAStartup failed: %s"), Win32ErrorText(result));
        return(-1);
    }
    
    // Read the initial AP-controller configurations from the registry.
    hr = apList.LoadAPControllers(s_pServerHost, s_pServerPort);
    if (FAILED(hr))
    {
        result = HRESULT_CODE(hr);
        LogError(TEXT("Initial configuration failed: %s"), 
                 HRESULTErrorText(hr));
        goto Cleanup;
    }
    if (apList.size() < 1)
    {
        result = ERROR_INVALID_PARAMETER;
        LogError(TEXT("Initial configuration failed: no Access Points")); 
        goto Cleanup;
    }

    // Configure the USB clicker, if any.
    if (NULL != s_pUSBRegiKey && L'\0' != s_pUSBRegiKey[0])
    {
        tester.m_pClicker = new USBClicker_t;
        if (NULL == tester.m_pClicker)
        {
            result = ERROR_OUTOFMEMORY;
            LogError(TEXT("Failed allocating USB clicker"));
            goto Cleanup;
        }
        
        hr = tester.m_pClicker->LoadFromRegistry(s_pUSBRegiKey);
        if (FAILED(hr))
        {
            result = HRESULT_CODE(hr);
            LogError(TEXT("Initial USB Clicker config failed: %s"), 
                     HRESULTErrorText(hr));
            goto Cleanup;
        }
    }

#ifdef UNDER_CE
    // Initialize the Netlog manager, if any.
    if (NULL != s_pNetlogFilePath)
    {
        tester.m_pNetlog = new NetlogManager_t;
        if (NULL == tester.m_pNetlog)
        {
            result = ERROR_OUTOFMEMORY;
            LogError(TEXT("Failed allocating Netlog manager"));
            goto Cleanup;
        }
        
        if (!tester.m_pNetlog->Load())
        {
            result = GetLastError();
            LogError(TEXT("Load of netlog driver failed: %s"), 
                     Win32ErrorText(result));
            goto Cleanup;
        }
    }
#endif

    // Have the test-controller run the test in a sub-thread.
    tester.m_pAPPool = &apList;
    hr = tester.Start();
    if (FAILED(hr))
    {
        result = HRESULT_CODE(hr);
        LogError(TEXT("Cannot run test-controller thread: %s"),
                 HRESULTErrorText(hr));
        goto Cleanup;
    }

    // Wait for the test thread to finish.
    result = tester.Wait(s_TestDuration * 1000, (s_RoamTime + 20) * 1000);
    if (ERROR_SUCCESS != result && WAIT_TIMEOUT != result)
    {
        LogError(TEXT("Test failed: %s"), Win32ErrorText(result));
        goto Cleanup;
    }

    LogDebug(TEXT("Test succeeded"));
    result = ERROR_SUCCESS;

Cleanup:

    // Close the APControllers, USBClicker and NetlogManager.
    apList.Clear();
    if (NULL != tester.m_pClicker)
    {
        delete tester.m_pClicker;
    }
#ifdef UNDER_CE
    if (NULL != tester.m_pNetlog)
    {
        tester.m_pNetlog->Unload();
        delete tester.m_pNetlog;
    }
#endif
        
    // Shut down WinSock.
	WSACleanup();
	return (ERROR_SUCCESS == result)? 0 : -1;
}

// ----------------------------------------------------------------------------
//
// Gets the current system time in milliseconds since Jan 1, 1601.
//
static _int64
GetWallClockTime(void)
{
    SYSTEMTIME sysTime;
    FILETIME  fileTime;
    ULARGE_INTEGER itime;
    GetSystemTime(&sysTime);
    SystemTimeToFileTime(&sysTime, &fileTime);
    memcpy(&itime, &fileTime, sizeof(itime));
    return _int64(itime.QuadPart) / _int64(10000);
}

// ----------------------------------------------------------------------------
//
// Runs the test in a sub-thread so the main thread can stop it when
// the overall test time runs out.
//
DWORD
MyTestController_t::
Run(void)
{
    HRESULT hr;
    DWORD result;

    TCHAR cycleHead[40];
    int   cycleHeadChars = COUNTOF(cycleHead);

    int lastAP, thisAP, nextAP;
    lastAP = thisAP = nextAP = 0;

    const TCHAR *pThisAPName, *pNextAPName;
    pThisAPName = pNextAPName = TEXT("GPRS");

    static const int MaximumAttenuationErrors = 8;
    static const int MaximumUSBClickerErrors  = 10;
    int numberAttenuationErrors = 0;
    int numberUSBClickerErrors  = 0;
        
    // These flags indicate the current signal-strength and attenuation
    // status of each of the Access Points.
    enum SignalStatus {
         SignalMinimum = 0 // Attenuation maximum, signal absent
        ,SignalRising      // Attenuation decreasing, signal-strength rising
        ,SignalMaximum     // Attenuation minimum, signal maximum
        ,SignalFalling     // Attenuation increasing, signal-strength falling
    }   *SignalState = NULL;

    // Allocate the signal-state matrix.
    int numberAPs = m_pAPPool->size();
    size_t stateSize = sizeof(SignalStatus) * numberAPs;
    if (numberAPs > 0 && stateSize < sizeof(SignalStatus) * 1000)
        SignalState = (SignalStatus *) LocalAlloc(LPTR, stateSize);
    if (NULL == SignalState)
    {
        LogError(TEXT("Failed allocating %u bytes for signal-states"),
                 stateSize);
        return ERROR_OUTOFMEMORY;
    } 
    ce::auto_local_mem SignalStateMemory(SignalState);

    // Initialize the AP attenuators.
    for (int apx = 0 ; apx < numberAPs ; ++apx)
    {
        APController_t *pAP = m_pAPPool->GetAP(apx);
        int newAtten;
        if (apx == 0) 
        {
            pThisAPName = pNextAPName = pAP->GetAPName();
            newAtten = pAP->GetMinAttenuation();
            SignalState[apx] = SignalMaximum;
        }
        else
        {
            newAtten = pAP->GetMaxAttenuation();
            SignalState[apx] = SignalMinimum;
        }
        pAP->SetAttenuation(newAtten);
        hr = pAP->SaveConfiguration();
        if (FAILED(hr))
        {
            LogError(TEXT("Failed setting %s attenuation to %ddbs: %s"),
                     pAP->GetAPName(), newAtten,
                     HRESULTErrorText(hr));
            return HRESULT_CODE(hr);
        } 
    }

    // Disconnect the USB port (if any).
    if (m_pClicker)
    {
        result = m_pClicker->Remove();
        if (ERROR_SUCCESS != result)
        {
            LogError(TEXT("Failed disconnecting USB port: %s"),
                     Win32ErrorText(result));
            return result;
        }
    }

    // Ordering is important here. We want to force the mobile devices
    // to roam from one AP to the next without pause. To enable that to
    // happen, the "receiving" AP must be turned on before the "losing"
    // AP is turned off. Additionally, we want to test the device's
    // ability to detect the USB, wired-network, connection and disable
    // the WiFi network. For that reason, the last AP is NOT turned off
    // when the USB is connected.
    //
    // States (assuming two APs and a USB connection):
    //
    //      State   AP1     AP2     USB
    //        0     ON      OFF     OFF     associated with AP1
    //        1     OFF     ON      OFF     associated with AP2
    //        2     OFF     ON      ON      USB/DTPT (optional)
    //        3     OFF     OFF     OFF     GPRS
    //
    int maxState = numberAPs;
    if (m_pClicker)
         maxState += 2; // APs, cell-network and USB/DTPT
    else maxState += 1; // just APs and cell-network

    // Calculate the timing parameters.
    int maxSlice = (s_RoamTime + s_StepTime - 1) / s_StepTime;
   _int64 stateTime = s_RoamTime * 1000;    // time to roam between APs
   _int64 cycleTime = stateTime * maxState; // time to finish complete cycle
   _int64 sliceTime = stateTime / maxSlice; // time between attenuator changes

    int lastCycle =  0;
    int lastState = -1;
    int lastSlice = -1;
   _int64 startTime = GetWallClockTime();
    while (!IsInterrupted())
    {
       _int64 runTime = GetWallClockTime() - startTime;
        int cycle = int(runTime / cycleTime) + 1;
        int state = int(runTime / stateTime) % maxState;
        int slice = int(runTime / sliceTime) % maxSlice;

        // If we've reached a new cycle...
        if (lastCycle != cycle)
        {
            lastCycle = cycle;

            // Generate a new cycle header.
            hr = StringCchPrintf(cycleHead, cycleHeadChars,
                                TEXT("Cycle %03d:"), cycle);
            if (FAILED(hr))
                SafeCopy(cycleHead, TEXT("Cycle nnn:"), cycleHeadChars);

#ifdef UNDER_CE
            // If we're supposed to capture a netlog file, stop the current
            // netlog capture (if any), change the cycle's base-name and
            // start another.
            if (m_pNetlog)
            {
                if (!m_pNetlog->Stop())
                {
                    result = GetLastError();
                    LogError(TEXT("Cannot stop netlog capture: %s"),
                             Win32ErrorText(result));
                 // return result;
                }

                TCHAR buff[MAX_PATH];
                int   buffChars = COUNTOF(buff);

                hr = StringCchPrintf(buff, buffChars,
                                     TEXT("%s%03d"), s_pNetlogFilePath, cycle);
                if (FAILED(hr))
                    return HRESULT_CODE(hr);

                if (!m_pNetlog->SetFileBaseName(buff)
                 || !m_pNetlog->Start())
                {
                    result = GetLastError();
                    LogError(TEXT("Cannot start netlog capture: %s"),
                             Win32ErrorText(result));
                 // return result;
                }
            }
#endif
        }

        // If we've reached a new state...
        if (lastState != state)
        {
            lastState = state;

            // Tell the server our new status.
            hr = m_pAPPool->SetTestStatus(MAKELONG(state,cycle));
            if (FAILED(hr))
            {
                LogError(TEXT("Failed setting status at %s:%s: %s"),
                         s_pServerHost, s_pServerPort,
                         HRESULTErrorText(hr));
             // return HRESULT_CODE(hr);
            }

            // Describe the new state, determine the signal-strength
            // adjustments required and (if necessary) (dis)connect
            // the USB port.
            lastAP = thisAP;
            thisAP = nextAP;
            pThisAPName = pNextAPName;
            if (numberAPs > lastAP)
                SignalState[lastAP] = SignalMinimum;
            SignalState[thisAP] = SignalFalling;
            if (numberAPs > state)
            {
                nextAP = state+1;

                if (numberAPs > nextAP)
                {
                    SignalState[nextAP] = SignalRising;
                    pNextAPName = m_pAPPool->GetAP(nextAP)->GetAPName();
                    LogDebug(TEXT("%s roaming from %s to %s"),
                             cycleHead, pThisAPName, pNextAPName);
                }
                else
                {
                    nextAP = thisAP;
                    pNextAPName = pThisAPName;
                    if (m_pClicker)
                    {
                        SignalState[thisAP] = SignalMaximum;
                        LogDebug(TEXT("%s associated with %s"),
                                 cycleHead, pNextAPName);
                    }
                    else
                    {
                        LogDebug(TEXT("%s roaming from %s to GPRS"),
                                 cycleHead, pNextAPName);
                    }
                }
            }
            else
            if (m_pClicker && numberAPs == state)
            {
                nextAP = 0;
                pNextAPName = pThisAPName;

                LogDebug(TEXT("%s connected to DTPT within %s region"),
                         cycleHead, pNextAPName);

                result = m_pClicker->Insert();
                if (ERROR_SUCCESS == result)
                {
                    numberUSBClickerErrors = 0;
                }
                else
                {
                    LogError(TEXT("Failed connecting USB port: %s"),
                             Win32ErrorText(result));
                    if (++numberUSBClickerErrors >= MaximumUSBClickerErrors)
                        return result;
                }
            }
            else
            {
                SignalState[thisAP] = SignalMinimum;
                nextAP = 0;
                SignalState[nextAP] = SignalRising;
                pNextAPName = m_pAPPool->GetAP(nextAP)->GetAPName();

                LogDebug(TEXT("%s on GPRS - roaming to %s"),
                         cycleHead, pNextAPName);

                if (m_pClicker)
                {
                    result = m_pClicker->Remove();
                    if (ERROR_SUCCESS == result)
                    {
                        numberUSBClickerErrors = 0;
                    }
                    else
                    {
                        LogError(TEXT("Failed disconnecting USB port: %s"),
                                 Win32ErrorText(result));
                        if (++numberUSBClickerErrors >= MaximumUSBClickerErrors)
                            return result;
                    }
                }
            }

            // Clear the cycle header.
            for (int cx = 0 ; cx < cycleHeadChars ; ++cx)
                if (cycleHead[cx])
                    cycleHead[cx] = TEXT(' ');
                else break;
        }

        // If we've reached a new attenuation state, adjust the Access
        // Point RF attenuators.
        if (lastSlice != slice)
        {
            lastSlice = slice;

            double slope = (double) (runTime % stateTime)
                         / (double)            stateTime;
            double dnRssi = (slope - 0.5) * 2;
            if      (dnRssi < 0.0) dnRssi = 0.0;
            else if (dnRssi > 1.0) dnRssi = 1.0;
            double upRssi = (slope - (double)(100 - s_CrossoverPct) / 100.0) * 2;
            if      (upRssi < 0.0) upRssi = 0.0;
            else if (upRssi > 1.0) upRssi = 1.0;

            for (int apx = 0 ; numberAPs > apx ; ++apx)
            {
                APController_t *pAP = m_pAPPool->GetAP(apx);
                int minAtten = pAP->GetMinAttenuation();
                int maxAtten = pAP->GetMaxAttenuation();
                int newAtten;

                double attenRange;
                switch (SignalState[apx])
                {
                    case SignalMinimum:
                        newAtten = maxAtten;
                        break;
                    case SignalRising:
                        attenRange = maxAtten - minAtten;
                        if (apx == 0)
                        {
                            newAtten = maxAtten
                                     - (int)((attenRange * dnRssi) + 0.5);
                        }
                        else
                        {
                            newAtten = maxAtten
                                     - (int)((attenRange * upRssi) + 0.5);
                        }
                        break;
                    case SignalMaximum:
                        newAtten = minAtten;
                        break;
                    case SignalFalling:
                        attenRange = maxAtten - minAtten;
                        newAtten = minAtten
                                 + (int)((attenRange * dnRssi) + 0.5);
                        break;
                }

                if (pAP->GetAttenuation() != newAtten)
                {
                    hr = pAP->SetAttenuation(newAtten);
                    if (FAILED(hr))
                    {
                        LogError(TEXT("Failed setting %s attenuation to %d: %s"),
                                 pAP->GetAPName(), newAtten,
                                 HRESULTErrorText(hr));
                        return HRESULT_CODE(hr);
                    }
                    
                    hr = pAP->SaveConfiguration();
                    if (SUCCEEDED(hr))
                    {
                        numberAttenuationErrors = 0;
                    }
                    else
                    {
                        LogError(TEXT("Failed updating %s attenuation to %d: %s"),
                                 pAP->GetAPName(), newAtten,
                                 HRESULTErrorText(hr));
                        if (++numberAttenuationErrors >= MaximumAttenuationErrors)
                            return HRESULT_CODE(hr);
                    }
                    
                //  LogDebug(TEXT("Set %s attenuation to %d"),
                //           pAP->GetAPName(), newAtten);
                }
            }
        }

        Sleep(250);
    }

    return 0;
}

// ----------------------------------------------------------------------------
