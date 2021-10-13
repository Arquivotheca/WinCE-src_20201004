//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------


#include <WINDOWS.H>
#include <TCHAR.H>
#include <TUX.H>
#include <KATOEX.H>
#include "xwifi11b_testproc.h"

#include <windev.h>
#include <ndis.h>
#include <msgqueue.h>

extern CKato *g_pKato;
extern SPS_SHELL_INFO *g_pShellInfo;


extern WCHAR* g_szWiFiAdapter1;

/*
g_pKato->Log(LOG_FAIL,
g_pKato->Log(LOG_ABORT,
g_pKato->Log(LOG_DETAIL,
g_pKato->Log(LOG_COMMENT
g_pKato->Log(LOG_SKIP
g_pKato->Log(LOG_NOT_IMPLEMENTED
*/




WCHAR *g_szPredefinedAdhocSSIDs[] =
{
    L"ADHOC1",
    L"adhoc",
    L"1",
    L"a"
    L"1234567890",
    L"abcdefghij",
    L"abcdefghijklmnopqrst",
    L"abcdefghijklmnopqrstabcdefghij32",
    L"abc    def",
    L"!@#!#@$!@!#$%",
    L"><p. :[",
    NULL
};


TESTPROCAPI
Test_Adhoc_Ssid
(
UINT uMsg,
TPPARAM tpParam,
LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    FindWirelessNetworkDevice();
    if(!g_szWiFiAdapter1)
    {
        g_pKato->Log(LOG_FAIL, L"no wifi adapter");
        return TPR_FAIL;
    }

    if(AdapterHasNoneZeroIpAddress(g_szWiFiAdapter1))
    {
        g_pKato->Log(LOG_COMMENT, L"wifi adapter %s has already connection", g_szWiFiAdapter1);
        g_pKato->Log(LOG_COMMENT, L"disconnecting it now");
        ResetPreferredList(g_szWiFiAdapter1);
        if(WaitForLoosingNetworkConnection(g_szWiFiAdapter1) == FALSE)
        {
            g_pKato->Log(LOG_FAIL, L"fail to disconnect ");
            return TPR_FAIL;
        }
    }

    g_pKato->Log(LOG_COMMENT, L"wifi adapter = %s", g_szWiFiAdapter1);

    // adhoc net, test various ssids
    WCHAR szAdhocSSID[128];
    for(unsigned i=0; g_szPredefinedAdhocSSIDs[i]; i++)
    {
        g_pKato->Log(LOG_COMMENT, L"connecting to %s", g_szPredefinedAdhocSSIDs[i]);

        wsprintf(szAdhocSSID, L"-ssid %s:-adhoc", g_szPredefinedAdhocSSIDs[i]);
        AddSsidToThePreferredList(g_szWiFiAdapter1, szAdhocSSID);
        Sleep(5000);
        if(WaitForGainingNetworkConnection(g_szWiFiAdapter1) == FALSE)
        {
            g_pKato->Log(LOG_FAIL, L"fail to get an IP address after connecting to the adhoc %s", g_szPredefinedAdhocSSIDs[i]);
            break;
        }
        ResetPreferredList(g_szWiFiAdapter1);
        if(WaitForLoosingNetworkConnection(g_szWiFiAdapter1) == FALSE)
        {
            g_pKato->Log(LOG_FAIL, L"fail to disconnect after removing the adhoc %s", g_szPredefinedAdhocSSIDs[i]);
            break;
        }
    }

	if(g_pKato->GetVerbosityCount(LOG_FAIL))
		return TPR_FAIL;
	return TPR_PASS;
}
