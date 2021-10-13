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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------

#include <windows.h>
#include <tchar.h>
#include "utils.h"
#include "log.h"
#include "ndp_protocol.h"
#include "ndp_lib.h"
#include "globals.h"

// TBD: Add any test specific
//                 globals
//          function prototypes

//
// TBD: Define your test procs here
//

LPCTSTR g_szTestName = _T("perf_ndis");

DWORD g_logVerbosity = LOG_PASS;
BOOL  g_unbindProtocols = TRUE;
DWORD g_unbindDelay = 1000;
BOOL  g_xlsResults = FALSE;
BOOL  g_receivePeer = FALSE;
BOOL  g_ndisd = FALSE;
BOOL  g_RunAsPerfWinsock = FALSE;
DWORD g_AddendumHdrSize = 54;

TCHAR  g_szAdapter[256] = _T("");
HANDLE g_hAdapter = NULL;

DWORD g_packetsSend = 100000;
DWORD g_minSize = 64;
DWORD g_maxSize = ETH_MAX_FRAME_SIZE;
DWORD g_sizeStep = 64;
DWORD g_poolSize = 64;
BOOL  g_fUseNdisSendOnly = FALSE;
teNDPTestType g_eTestType = SEND_THROUGHPUT;

BOOL EnumNPrintAdapters();
//------------------------------------------------------------------------------

void PrintUsage()
{
   LogMsg(_T("perf_ndis: Usage: tux -o -d perf_ndis -c\"[...options...] adapterName\""));
   //LogMsg(_T("ndp_send [...options...] adapterName"));
   LogMsg(_T(""));
   LogMsg(_T("Options:"));
   LogMsg(_T(""));
   LogMsg(_T("-<?|help>                 : Get these options/help"));
   LogMsg(_T("-enum                     : Enumerate the adapters"));
   LogMsg(_T("-mode <send|recv|ping>    : Find send or recv or ping throughput"));
   LogMsg(_T("-wsock [-hsize <n>]       : Run the test in the same fashion to perf_winsock &"));
   LogMsg(_T("                            take <n> as total header size (default 40+14)"));
   LogMsg(_T("                            For TCP:n=54 & UDP:n=42"));
   LogMsg(_T("-ndisd                    : Run as receive peer/partner <ndisd>"));
   LogMsg(_T("-s                        : Find and use receive peer/partner <ndisd>"));
   LogMsg(_T("-v <n>                    : Log verbosity (0 to 15, default 10)"));   
   LogMsg(_T("-packets <n>              : Packets to send (default 10000)"));
   LogMsg(_T("-size <n>                 : Packets size (min/max/step option are ignored)"));
   LogMsg(_T("-min <n>                  : Minimal packets size (default 64)"));
   LogMsg(_T("-max <n>                  : Maximal packets size (default 1514)"));
   LogMsg(_T("-step <n>                 : Packets size increment (default 64)"));
   LogMsg(_T("-pool <n>                 : Pool of packets used in test (default 64)"));
   LogMsg(_T("-nounbind                 : Suppress protocol unbind"));
   LogMsg(_T("-ubdelay                  : Delay after protocol unbind (default 1000 ms)"));
   LogMsg(_T("-apacketsend              : Uses NdisSend instead of NdisSendPackets for send"));
   LogMsg(_T(""));
   LogMsg(_T(""));
}

//------------------------------------------------------------------------------

BOOL ParseCommands(INT argc, LPTSTR argv[])
{
   BOOL ok = FALSE;
   DWORD value;
   
   //g_receivePeer = GetOptionFlag(&argc, argv, _T("peer"));
   BOOL bHelpReqd = GetOptionFlag(&argc, argv, _T("?"));
   bHelpReqd = GetOptionFlag(&argc, argv, _T("help"));
   if (bHelpReqd) goto cleanUp;

   BOOL bEnum = GetOptionFlag(&argc, argv, _T("enum"));
   if (bEnum) 
   {
       if (!EnumNPrintAdapters())
       {
           LogErr(_T("Error in enumerating the adapters"));
       }
       return FALSE;
   }

   g_receivePeer = GetOptionFlag(&argc, argv, _T("s"));
   g_ndisd = GetOptionFlag(&argc, argv, _T("ndisd"));
   g_unbindProtocols = !GetOptionFlag(&argc, argv, _T("nounbind"));
   g_fUseNdisSendOnly = GetOptionFlag(&argc, argv, _T("apacketsend"));
   g_unbindDelay = GetOptionLong(&argc, argv, g_unbindDelay, _T("ubdelay"));
   g_packetsSend = GetOptionLong(&argc, argv, g_packetsSend, _T("packets"));
   g_poolSize = GetOptionLong(&argc, argv, g_poolSize, _T("pool"));

   g_RunAsPerfWinsock = GetOptionFlag(&argc, argv, _T("wsock"));
   if (g_RunAsPerfWinsock)
       g_AddendumHdrSize = GetOptionLong(&argc, argv, g_AddendumHdrSize, _T("hsize"));

   TCHAR * pwsz = GetOptionString(&argc, argv, _T("mode"));
   if (pwsz)
   {
       if (_tcsicmp(pwsz,_T("send")) == 0)
           g_eTestType = SEND_THROUGHPUT;
       else if (_tcsicmp(pwsz,_T("recv")) == 0)
           g_eTestType = RECV_THROUGHPUT;
       else if (_tcsicmp(pwsz,_T("ping")) == 0)
           g_eTestType = PING_THROUGHPUT;
       else
           goto cleanUp;
   }

   if (g_RunAsPerfWinsock)
   {
       g_sizeStep = 16;
       g_minSize = g_sizeStep + g_AddendumHdrSize;
       g_packetsSend = 10000;
   }
   else 
   {
       value = GetOptionLong(&argc, argv, 0, _T("size"));
       if (value > ETH_MAX_FRAME_SIZE) goto cleanUp;
       if (value > 0) {
              g_minSize = g_maxSize = value;
              g_sizeStep = ETH_MAX_FRAME_SIZE;
       } else  {
           g_minSize = GetOptionLong(&argc, argv, g_minSize, _T("min"));
           if (g_minSize > ETH_MAX_FRAME_SIZE) goto cleanUp;
           g_maxSize = GetOptionLong(&argc, argv, g_maxSize, _T("max"));
           if (g_minSize > ETH_MAX_FRAME_SIZE) goto cleanUp;
           g_sizeStep = GetOptionLong(&argc, argv, g_sizeStep, _T("step"));
           if (g_sizeStep == 0) goto cleanUp;
       }
   }
   if (argc != 1) goto cleanUp;
   StringCchCopy(g_szAdapter, sizeof(g_szAdapter)/sizeof(g_szAdapter[0]), argv[0]);

   ok = TRUE;

cleanUp:
   if (!ok) PrintUsage();
   return ok;
}

//------------------------------------------------------------------------------

int SetOptions(INT argc, LPTSTR argv[])
{
    int rc = ERROR_SUCCESS;

    // This parameter we need there
    g_logVerbosity = GetOptionLong(&argc, argv, g_logVerbosity, _T("v"));

    // Initialize the log subsystem
    LogStartup(g_szTestName, g_logVerbosity, LogKato);
    
    // Let parse test specific options
    if (!ParseCommands(argc, argv))
    {
        rc = ERROR_BAD_ARGUMENTS;
    }
    
    return rc;
}

//------------------------------------------------------------------------------

BOOL EnumNPrintAdapters()
{
    BOOL bRet = FALSE;

#ifdef UNDER_CE

    TCHAR szListOfNICs[256]=_T("");
    TCHAR szListOfPROTOs[256]=_T("");
    TCHAR szList[512]=_T("");

    LogMsg(_T("NDISConfig:****************************************"));

    DWORD dwSize = sizeof(szListOfNICs);
    DWORD dwRet = QueryAdapters(szListOfNICs,dwSize);
    LogMsg(_T("Adapters: %s"),MultiSzToSz(szList,szListOfNICs));

    dwRet = QueryProtocols(szListOfPROTOs,dwSize);
    LogMsg(_T("Protocols: %s"),MultiSzToSz(szList,szListOfPROTOs));

    for (TCHAR * wszAdapterName = &szListOfNICs[0];
             *wszAdapterName != _T('\0');
             wszAdapterName += _tcslen(wszAdapterName) + 1)
             {
                 TCHAR szBindings[512]=_T("");
                 dwRet = QueryBindings(wszAdapterName, szBindings, dwSize);
                 LogMsg(_T("%s <--> %s"),wszAdapterName,MultiSzToSz(szList,szBindings));             
             }
     LogMsg(_T("NDISConfig:****************************************"));
     bRet = TRUE;

#else UNDER_CE

    PtsNtEnumAda psNtEnumAda = NULL;
       PtsNtEnumAda pHead = NULL;

    HRESULT hr = NtEnumAdapters(&psNtEnumAda);
    if (hr == S_OK)
    {
        LogMsg(_T("Network Interface cards"));
        int i = 1;
        pHead = psNtEnumAda;
        while(psNtEnumAda)
        {
            PtsNtAda pNtAda = &(psNtEnumAda->sNtAda);
            LogMsg(_T("Adapter #%2d"),i);
            LogMsg(_T("Display Name : %s"),pNtAda->szwDisplayName);
            LogMsg(_T("Bind Name    : \\Device\\%s"),pNtAda->szwBindName);
            LogMsg(_T("Help Text    : %s"),pNtAda->szwHelpText);
            LogMsg(_T("Id           : %s"),pNtAda->szwId);
            i++;
            psNtEnumAda = psNtEnumAda->next;
        }

        //free the linked list
        while (pHead)
        {
            psNtEnumAda = pHead->next;
            free(pHead);
            pHead = psNtEnumAda;
        }
        bRet = TRUE;
    }

#endif

    return bRet;
}

//------------------------------------------------------------------------------

TESTPROCAPI Perf_ndis(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD dwRetVal = TPR_FAIL;

    // process incoming tux messages
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // return the thread count that should be used to run this test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return TPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    int rc = ERROR_SUCCESS;

    if (g_unbindProtocols) {

#ifdef UNDER_CE
        rc = UnbindProtocol(g_szAdapter, NULL);
#else
        rc = ERROR_NOT_SUPPORTED;
#endif

        if (rc != ERROR_SUCCESS) {
            LogErr(
                _T("Failed unbind protocols from adapter %s with code %d"), 
                g_szAdapter, rc
                );
            goto cleanUp;
        }
        Sleep(g_unbindDelay);
    }
    
    // Load protocol driver...
    g_hAdapter = OpenProtocol();
    if (g_hAdapter == NULL) {
        rc = GetLastError();
        LogErr(_T("Failed open protocol driver with code %d"), rc);
        goto cleanUp;
    }
    
    // Open adapter
    if (!OpenAdapter(g_hAdapter, g_szAdapter)) {
        LogErr(_T("Failed open adapter %s"), g_szAdapter);
        goto cleanUp;
    }

    // Fill in the test proc here
    if (g_ndisd)
    {
        if (ndisd() == TPR_PASS)
            dwRetVal = TPR_PASS;
    }
    else if (SendPerf() == TPR_PASS)
    {
        //
        //  test passed
        //
        dwRetVal = TPR_PASS;
    }
    
cleanUp:
    // cleanup here
    if (g_hAdapter != NULL) {
        CloseAdapter(g_hAdapter);
        CloseProtocol(g_hAdapter);
    }
    
    if (g_unbindProtocols) {

#ifdef UNDER_CE
        rc = BindProtocol(g_szAdapter, NULL);
#else
        rc = ERROR_NOT_SUPPORTED;
#endif

        if (rc != ERROR_SUCCESS) {
            LogErr(
                _T("Failed bind protocols to adapter %s with code %d"), 
                g_szAdapter, rc
                );
        }
    }
    LogCleanup();
    return dwRetVal;
}

//------------------------------------------------------------------------------

#if 0

int WINAPI WinMain(
   HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR szCmdLine, int nShowCmd
)
{
   int rc = ERROR_SUCCESS;
   LPTSTR argv[64];
   int argc = 64;

   // Divide command line to token
   CommandLineToArgs(szCmdLine, &argc, argv);

   if ((rc=SetOptions(argc,argv))!= ERROR_SUCCESS)
   {
       //Error
       return rc;
   }

   return SendPerf();
}

#endif

//------------------------------------------------------------------------------