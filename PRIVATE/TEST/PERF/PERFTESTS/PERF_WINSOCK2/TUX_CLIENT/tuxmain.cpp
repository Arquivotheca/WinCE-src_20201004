//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
// tuxmain.cpp

// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#include "tuxmain.h"
#include "loglib.h"
#include "testproc.h"
#include <ntddndis.h>
#include "consetup.h"
#include "tests.h"

// --------------------------------------------------------------------
// Defaults

// Number of buffers to send
const DWORD DEFAULT_NUMBER_OF_BUFFERS = 10000;

// Number of buffers to send back and fourth in ping tests (if 0, then default number of buffers is used)
const DWORD DEFAULT_NUMBER_OF_PING_BUFFERS = 100;

// Default server name
const TCHAR* DEFAULT_SERVER = _T("localhost");

// --------------------------------------------------------------------
// Special settings

// Programmatically attempts to disable VMINI (1 enables this option, 0 disables it)
#define DISABLE_VMINI 0

// --------------------------------------------------------------------
// Globals

CKato *g_pKato = NULL;                 // Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
SPS_SHELL_INFO *g_pShellInfo;          // Global shell info structure.  Set while processing SPM_SHELL_INFO message.

// Buffer/packet sizes to run through
int* g_aTestBufferSizes = NULL;
int g_nTestBufferCount = 0;
int* g_aTestBufferSizesUdp = NULL;
int g_nTestBufferCountUdp = 0;

TCHAR g_tszServerAddressString[MAX_SERVER_NAME_LEN];    // IP address or name of the server

DWORD g_dwNumberOfBuffers=DEFAULT_NUMBER_OF_BUFFERS;            // Number of buffers sent or recv'd
DWORD g_dwNumberOfPingBuffers=DEFAULT_NUMBER_OF_PING_BUFFERS;   // Number of buffers sent during ping-type tests

DWORD g_dwReqSendRateKbps = 0;          // Requested send rate (for recv tests)
DWORD g_dwReqRecvRateKbps = 0;          // Requested recv rate (for throttling on recv)

BOOL g_fDataVerifyFlag = FALSE;         // Specifies whether to verify received data
BOOL g_fDataRandomizeFlag = FALSE;      // Specifies whether to randomize data

WORD  g_wUseIpVer = 4;                  // IP version to use
TCHAR* g_tszTestNameExtension = NULL;   // Specifies test name extension
BOOL g_fGatewayTestOption = FALSE;      // Gateway test option

int g_iThreadPriority = 0;              // Client thread priority
BOOL g_fEnableMonteCarlo = FALSE;       // Specifies whether to enable MonteCarlo logging during test run
BOOL g_fEnableAltCpuMon = FALSE;        // Alternative method of capturing CPU utilization

const INT UDP_RECV_MIN_QUEUE_BYTES = 8192;
int g_iUdpRcvQueueBytes = 32768;        // Specifies UDP recv queue size, in bytes (on Macallan, default is 32768)

// --------------------------------------------------------------------
// Internal function prototypes

#ifdef UNDER_CE
BOOL DoNdisMultiSzIOControl
(
    DWORD    dwCommand,  
    ...
);
BOOL DoNdisIOControl
(
    DWORD    dwCommand,
    LPVOID    pInBuffer,
    DWORD    cbInBuffer,
    LPVOID    pOutBuffer,
    DWORD    *pcbOutBuffer    OPTIONAL
);
#endif
BOOL ProcessCmdLine( LPCTSTR szCmdLine );
VOID ShowUsage();

// --------------------------------------------------------------------
// Tux testproc function table
//

#define WTID_BASE    1000

// extern "C"
#ifdef __cplusplus
extern "C" {
#endif

FUNCTION_TABLE_ENTRY g_lpFTE[] =
{
    _T("Winsock Performance Test"), 0, 0, 0, NULL,

    // TCP tests, Nagle ON (default):
    _T("TCP Send Throughput"),          1, 0,                  WTID_BASE+1,  TestTCPSendThroughput,
    _T("TCP Recv Throughput"),          1, 0,                  WTID_BASE+2,  TestTCPRecvThroughput,
    _T("TCP Ping"),                     1, 0,                  WTID_BASE+3,  TestTCPPing,

    // TCP tests, Nagle OFF:
    _T("TCP Send Throughput Nagle Off"),1, TestOpt_NagleOff,   WTID_BASE+4,  TestTCPSendThroughput,
    _T("TCP Recv Throughput Nagle Off"),1, TestOpt_NagleOff,   WTID_BASE+5,  TestTCPRecvThroughput,
    _T("TCP Ping Nagle Off"),           1, TestOpt_NagleOff,   WTID_BASE+6,  TestTCPPing,

    // UDP tests:
    _T("UDP Send Throughput"),          1, 0,                  WTID_BASE+7,  TestUDPSendThroughput,
    _T("UDP Send Packet Loss"),         1, 0,                  WTID_BASE+8,  TestUDPSendPacketLoss,
    _T("UDP Recv Throughput"),          1, 0,                  WTID_BASE+9,  TestUDPRecvThroughput,
    _T("UDP Recv Packet Loss"),         1, 0,                  WTID_BASE+10, TestUDPRecvPacketLoss,
    _T("UDP Ping"),                     1, 0,                  WTID_BASE+11, TestUDPPing,

    NULL, 0, 0, 0, NULL
};
// --------------------------------------------------------------------

// --------------------------------------------------------------------
BOOL WINAPI 
DllMain(
    HANDLE hInstance, 
    ULONG dwReason, 
    LPVOID lpReserved ) 
// --------------------------------------------------------------------    
{
    UNREFERENCED_PARAMETER(lpReserved);

    switch( dwReason )
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls((HMODULE)hInstance); // not interested in those here
#if CELOG
            CeLogSetZones(CELZONE_MISC | CELZONE_CRITSECT | CELZONE_PRIORITYINV | CELZONE_THREAD | CELZONE_RESCHEDULE, // zone user
                CELZONE_CRITSECT | CELZONE_PRIORITYINV | CELZONE_THREAD | CELZONE_RESCHEDULE,  // zone ce
                0); // zone process
#endif
            return TRUE;

        case DLL_PROCESS_DETACH:
            return TRUE;
    }
    return FALSE;
}

#ifdef __cplusplus
} // end extern "C"
#endif

// --------------------------------------------------------------------
SHELLPROCAPI 
ShellProc(
    UINT uMsg, 
    SPPARAM spParam ) 
// --------------------------------------------------------------------    
{
    LPSPS_BEGIN_TEST pBT = {0};
    LPSPS_END_TEST pET = {0};

#if UNDER_CE && DISABLE_VMINI
    TCHAR    multiSzBuffer[MAX_PATH];
    DWORD    cbBuffer = sizeof(multiSzBuffer);
#endif
    TCHAR   *tszCurrent = NULL;
#ifdef SUPPORT_IPV6
    WORD    wWinsockVer = MAKEWORD(2,2);
#else
    WORD    wWinsockVer = MAKEWORD(1,1);
#endif

    switch (uMsg) {
    
        // Message: SPM_LOAD_DLL
        //
        case SPM_LOAD_DLL:
        {
            // If we are UNICODE, then tell Tux this by setting the following flag.
            #ifdef UNICODE
                ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            #endif

            // turn on kato debugging
            KatoDebug(1, KATO_MAX_VERBOSITY,KATO_MAX_VERBOSITY,KATO_MAX_LEVEL);

            // Get/Create our global logging object.
            g_pKato = (CKato*)KatoGetDefaultObject();

            LogInitialize(
                MODULE_NAME,
#ifdef ENABLE_DEBUGGING_OUTPUT
                DEBUG_MSG
#else
                REQUIRED_MSG
#endif
#ifdef SUPPORT_KATO
                ,g_pKato
#endif                
                );

            const int DefaultTestBufs[]
                = {16, 32, 64, 128, 256, 512, 1024, 1460, 2048, 2920, 4096, 8192};
            
            g_nTestBufferCount = 12;
            g_aTestBufferSizes = new int[g_nTestBufferCount];
            if (g_aTestBufferSizes == NULL)
            {
                Log(ERROR_MSG, TEXT("Memory error in ShellProc(SPM_LOAD_DLL): no memory for TCP buffer sizes."));
                ASSERT(FALSE);
                return SPR_FAIL;
            }
            memcpy(g_aTestBufferSizes, DefaultTestBufs, g_nTestBufferCount * sizeof(int));

            // For UDP exclude the buffer sizes > MTU by default
            // Reason: The 2+ min wait required for system to recover from a supposed
            // UDP fragmentation attack protection algorithm makes the test run for
            // a long time (each iteration lasts no less than 2+ min) - see constant
            // ReassemblyTimeoutMs.
            g_nTestBufferCountUdp = 8;
            g_aTestBufferSizesUdp = new int[g_nTestBufferCountUdp];
            if (g_aTestBufferSizesUdp == NULL)
            {
                Log(ERROR_MSG, TEXT("Memory error in ShellProc(SPM_LOAD_DLL): no memory for UDP buffer sizes."));
                ASSERT(FALSE);
                return SPR_FAIL;
            }
            memcpy(g_aTestBufferSizesUdp, DefaultTestBufs, g_nTestBufferCountUdp * sizeof(int));

            // Set default name of server
            if (MAX_SERVER_NAME_LEN < _tcslen(DEFAULT_SERVER) + 1)
            {
            	Log(ERROR_MSG, TEXT("Default name of server too long. Max length this %d chars."),
            		MAX_SERVER_NAME_LEN);
            	ASSERT(FALSE);
            	return SPR_FAIL;
            }
            _tcscpy(g_tszServerAddressString, DEFAULT_SERVER);

#ifdef UNDER_CE
            g_iThreadPriority = CeGetThreadPriority(GetCurrentThread());
#endif

            return SPR_HANDLED;
        }
        // Message: SPM_UNLOAD_DLL
        //
        case SPM_UNLOAD_DLL: 
            // It's possible that both are pointing to the same memory location
            // (see handling of user options) - have to make sure to release memory only once
            // in this case.
            if (g_aTestBufferSizes != NULL) delete [] g_aTestBufferSizes;
            if (g_aTestBufferSizesUdp != NULL) delete [] g_aTestBufferSizesUdp;

            // Cleanup Winsock resources.
            WSACleanup();

            return SPR_HANDLED;      

        // Message: SPM_SHELL_INFO
        //
        case SPM_SHELL_INFO:
            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
        
            // handle command line parsing
            if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine)
            {
                Log(DEBUG_MSG, TEXT("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);
            }

            if(!ProcessCmdLine(g_pShellInfo->szDllCmdLine))
            {
                Log(ERROR_MSG, TEXT("Invalid option supplied."));
                ShowUsage();
                return SPR_FAIL;
            }

            // Initialize Winsock resources.
            WSADATA wd;
            if (WSAStartup(wWinsockVer, &wd) != 0) 
            {
                Log(ERROR_MSG, _T("WSA Error: %ld"), WSAGetLastError());
                WSACleanup();
                return SPR_FAIL;
            }

            if (!Perf_SetTestName(g_tszTestNameExtension != NULL ? g_tszTestNameExtension : MODULE_NAME)) {
                Log(ERROR_MSG, TEXT("Failed to initialize perflog."));
                ASSERT(FALSE);
                return SPR_FAIL;
            }
            
#if DISABLE_VMINI
#ifdef UNDER_CE

            // Unbind vmini if it is one of the adapters.
            // Get the list of adapters
            if (!DoNdisIOControl(IOCTL_NDIS_GET_ADAPTER_NAMES, NULL, 0, &multiSzBuffer[0], &cbBuffer))
            {
                Log(ERROR_MSG, TEXT("Error executing IOCTL_NDIS_GET_ADAPTER_NAMES"));
                return SPR_FAIL;
            }

            // Check if vmini is one of them
            tszCurrent = &multiSzBuffer[0];

            while (*tszCurrent != TEXT('\0'))
            {
                // Get out of the loop if vmini is found
              if (_tcscmp(tszCurrent, TEXT("VMINI1")) == 0) break;

                // Find the end of the current string
                tszCurrent += _tcslen(tszCurrent);
                tszCurrent++;
            }

            // Unbind vmini, if present (note that this is a side effect, stays so after test finishes)
            if (*tszCurrent != TEXT('\0'))
            {
                // Found vmini in the list of adapters, unbind it
                if (!DoNdisMultiSzIOControl(IOCTL_NDIS_UNBIND_ADAPTER, TEXT("VMINI1"), NULL, NULL))
                {
                    Log(ERROR_MSG, TEXT("Error executing IOCTL_NDIS_UNBIND_ADAPTER on VMINI1"));
                    return SPR_FAIL;
                }

                // Wait for some time, so unbind finishes
                Sleep (1000);
            }
#endif
#endif
            return SPR_HANDLED;

        // Message: SPM_REGISTER
        //
        case SPM_REGISTER:           
            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
#ifdef UNICODE
                return SPR_HANDLED | SPF_UNICODE;
#else
                return SPR_HANDLED;
#endif
            
        // Message: SPM_START_SCRIPT
        //
        case SPM_START_SCRIPT:
            return SPR_HANDLED;

        // Message: SPM_STOP_SCRIPT
        //
        case SPM_STOP_SCRIPT:
            return SPR_HANDLED;

        // Message: SPM_BEGIN_GROUP
        //
        case SPM_BEGIN_GROUP:
            Log(DEBUG_MSG, _T("BEGIN GROUP: %s.DLL"), MODULE_NAME);
            
            return SPR_HANDLED;

        // Message: SPM_END_GROUP
        //
        case SPM_END_GROUP:
            Log(DEBUG_MSG, _T("ShellProc(SPM_END_GROUP, ...) called"));
            
            return SPR_HANDLED;

        // Message: SPM_BEGIN_TEST
        //
        case SPM_BEGIN_TEST:
            Log(DEBUG_MSG, _T("ShellProc(SPM_BEGIN_TEST, ...) called"));

            // Start our logging level.
            pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
                                _T("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                                pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                                pBT->dwRandomSeed);

            return SPR_HANDLED;

        // Message: SPM_END_TEST
        //
        case SPM_END_TEST:
            Log(DEBUG_MSG, _T("ShellProc(SPM_END_TEST, ...) called"));

            // End our logging level.
            pET = (LPSPS_END_TEST)spParam;
            g_pKato->EndLevel(_T("END TEST: \"%s\", %s, Time=%u.%03u"),
                              pET->lpFTE->lpDescription,
                              pET->dwResult == TPR_SKIP ? _T("SKIPPED") :
                              pET->dwResult == TPR_PASS ? _T("PASSED") :
                              pET->dwResult == TPR_FAIL ? _T("FAILED") : _T("ABORTED"),
                              pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);

            return SPR_HANDLED;

        // Message: SPM_EXCEPTION
        //
        case SPM_EXCEPTION:
            Log(DEBUG_MSG, _T("ShellProc(SPM_EXCEPTION, ...) called"));
            g_pKato->Log(LOG_EXCEPTION, _T("Exception occurred!"));
            return SPR_HANDLED;

        default:
            Log(ERROR_MSG, _T("ShellProc received bad message: 0x%X"), uMsg);
            ASSERT(!"Default case reached in ShellProc!");
            return SPR_NOT_HANDLED;
    }
}


// --------------------------------------------------------------------
// Internal functions

const DWORD MAX_OPT_ARG = 256;
const TCHAR END_OF_STRING = (TCHAR)NULL;
#ifndef UNDER_CE
const TCHAR EOF = (TCHAR)NULL;
#endif
const TCHAR BAD_ARG = TCHAR('?');
const TCHAR ARG_FLAG = TCHAR('-');
const TCHAR OPT_ARG = TCHAR(':');

TCHAR optArg[MAX_OPT_ARG];

// --------------------------------------------------------------------
INT
WinMainGetOpt(
    LPCTSTR szCmdLine,
    LPCTSTR szOptions )
// --------------------------------------------------------------------    
{
    static LPCTSTR pCurPos = szCmdLine;   
    LPCTSTR pCurOpt = szOptions;
    LPTSTR pOptArg = optArg;
    UINT cOptArg = 0;
    INT option = 0;
    TCHAR quoteChar = TCHAR(' ');

    // end of string reached, or NULL string
    if( NULL == pCurPos || END_OF_STRING == *pCurPos )
    {
        return EOF;
    }

    // eat leading white space
    while( TCHAR(' ') == *pCurPos )
        pCurPos++;

    // trailing white space
    if( END_OF_STRING == *pCurPos )
    {
       return EOF;
    }

    // found an option
    if( ARG_FLAG != *pCurPos )
    {
        return BAD_ARG;
    }
    
    pCurPos++;

    // found - at end of string
    if( END_OF_STRING == *pCurPos )
    {
        return BAD_ARG;
    }

    // search all options requested
    for( pCurOpt = szOptions; *pCurOpt != END_OF_STRING; pCurOpt++ )
    {
        // found matching option
        if( *pCurOpt == *pCurPos )
        {
            option = (int)*pCurPos;
            
            pCurOpt++;
            pCurPos++;
            if( OPT_ARG == *pCurOpt )
            {
                // next argument is the arg value
                // look for something between quotes or whitespace                
                while( TCHAR(' ') == *pCurPos )
                    pCurPos++;

                if( END_OF_STRING == *pCurPos )
                {
                    return BAD_ARG;
                }

                if( TCHAR('\"') == *pCurPos )
                {
                    // arg value is between quotes
                    quoteChar = *pCurPos;
                    pCurPos++;
                }
                
                else if( ARG_FLAG == *pCurPos )
                {
                    return BAD_ARG;
                }

                else                
                {
                    // arg end at next whitespace
                    quoteChar = TCHAR(' ');
                }
                
                pOptArg = optArg;
                cOptArg = 0;
                
                // TODO: handle embedded, escaped quotes
                while( quoteChar != *pCurPos && END_OF_STRING != *pCurPos  && cOptArg < MAX_OPT_ARG )
                {
                    *pOptArg++ = *pCurPos++;      
                    cOptArg++;
                }

                // missing final quote
                if( TCHAR(' ') != quoteChar && quoteChar != *pCurPos )
                {
                    return BAD_ARG;
                }
                
                // append NULL char to output string
                *pOptArg = END_OF_STRING;

                // if there was no argument value, fail
                if( 0 == cOptArg )
                {
                    return BAD_ARG;
                }   
            }  

            return option;            
        }
    }
    pCurPos++;
    // did not find a macthing option in list
    return BAD_ARG;
}

// --------------------------------------------------------------------
VOID ShowUsage()
// --------------------------------------------------------------------
{
    Log(REQUIRED_MSG, TEXT("Usage: tux -o -d %s [-x <test_id>] -c\"[-s <server>] [-i <ip_version>]"), MODULE_NAME);
    Log(REQUIRED_MSG, TEXT("                            [-b <buffer_size_list>] [-n <num_buffers>]"));
    Log(REQUIRED_MSG, TEXT("                            [-p <num_ping_buffers>] [-r <send_rate>]"));
    Log(REQUIRED_MSG, TEXT("                            [-g <gw_option>] [-x <name_extension>]"));
#ifdef UNDER_CE
    Log(REQUIRED_MSG, TEXT("                            [-k <ce_priority>] [-m <montecarlo_opt>]"));
    Log(REQUIRED_MSG, TEXT("                            [-c <alt_cpu_mon_opt>]"));
#endif
    Log(REQUIRED_MSG, TEXT("                            [-q <udp_recv_queue>]\""));
    Log(REQUIRED_MSG, TEXT(""));
    Log(REQUIRED_MSG, TEXT("Parameters:"));
    Log(REQUIRED_MSG, TEXT("    -s <server>           : Specifies the server name or IP address (default: %s)"), g_tszServerAddressString);
    Log(REQUIRED_MSG, TEXT("    -i <ip_version>       : IP version to use; must be 4 or 6 (default: %d)"), g_wUseIpVer);
    Log(REQUIRED_MSG, TEXT(""));
    Log(REQUIRED_MSG, TEXT("Advanced:"));
    Log(REQUIRED_MSG, TEXT("    -b <buffer_size_list> : Allows listing of comma-delimited buffer sizes to"));
    Log(REQUIRED_MSG, TEXT("                            iterate through, where"));
    Log(REQUIRED_MSG, TEXT("                            <buffer_size_list> = <buffer_size_1>,<buffer_size_2>,...,<buffer_size_N>"));
    Log(REQUIRED_MSG, TEXT("    -n <num_buffers>      : Number of buffers to send/receive (default: %d)."), g_dwNumberOfBuffers);
    Log(REQUIRED_MSG, TEXT("    -p <num_ping_buffers> : Number of buffers to send/receive in ping-type tests"));
    Log(REQUIRED_MSG, TEXT("                            (default %d)."), g_dwNumberOfPingBuffers);
    Log(REQUIRED_MSG, TEXT("    -r <send_rate>        : Rate at which data will be sent in Kbits/s, i.e. how often"));
    Log(REQUIRED_MSG, TEXT("                            send() will be called at the server (or client) depending on"));
    Log(REQUIRED_MSG, TEXT("                            which side is sending data when specific test is run (default: max)."));
    Log(REQUIRED_MSG, TEXT("    -t <recv_rate>        : Rate at which data will be recv'd in Kbps/s, i.e. how often"));
    Log(REQUIRED_MSG, TEXT("                            recv() will be called at the server (or client) depending on"));
    Log(REQUIRED_MSG, TEXT("                            which side is recieving data when specific test is run (default: max)."));
    Log(REQUIRED_MSG, TEXT("    -g <gw_option>        : Gateway test option; enables send throttling on"));
    Log(REQUIRED_MSG, TEXT("                            client part during UDP send throughput (default: %d (%s))"), g_fGatewayTestOption, g_fGatewayTestOption ? _T("Enabled") : _T("Disabled"));
    Log(REQUIRED_MSG, TEXT("    -x <name_extension>   : Test name extension applied to the file name generated by perflog"));
#ifdef UNDER_CE
    Log(REQUIRED_MSG, TEXT("    -k <ce_priority>      : Set priority of send/recv thread on CE (default=%d)"), g_iThreadPriority);
    Log(REQUIRED_MSG, TEXT("    -m <montecarlo_opt>   : Enable/disable MonteCarlo logging during test run (default=%s)"), g_fEnableMonteCarlo ? TEXT("TRUE") : TEXT("FALSE"));
    Log(REQUIRED_MSG, TEXT("    -c <alt_cpu_mon_opt>  : For platforms not supporting GetIdleTime(), can use enable\n"));
    Log(REQUIRED_MSG, TEXT("                            another method of taking CPU measurements (default=%s)"), g_fEnableAltCpuMon ? TEXT("TRUE") : TEXT("FALSE"));
#endif
    Log(REQUIRED_MSG, TEXT("    -q <udp_recv_queue>   : Set size of UDP recv queue, in bytes (default=%d bytes),"), g_iUdpRcvQueueBytes);
    Log(REQUIRED_MSG, TEXT("                            this option applies for all UDP recv tests."));
#if 0
    // TODO: Support these options.
    Log(REQUIRED_MSG, TEXT("    -v <data_verify_flag> : Specifies verification of data: 1 enable, 0 disable (default %d)"), g_fDataVerifyFlag);
    Log(REQUIRED_MSG, TEXT("    -r <data_rand_flag>   : Specifies randomization of data: 1 enable, 0 disable (default %d)"), g_fDataRandomizeFlag);
#endif
}

// --------------------------------------------------------------------
BOOL ProcessCmdLine( LPCTSTR szCmdLine )
// --------------------------------------------------------------------
{
    // parse command line
    int option = 0;

    LPCTSTR szOpt = TEXT("s:i:n:p:b:r:t:g:x:k:m:c:");
    
    while(EOF != (option = WinMainGetOpt(szCmdLine, szOpt)))
    {
        switch(option)
        {
        case TEXT('s'):
            if (_tcslen(optArg) + 1 > MAX_SERVER_NAME_LEN)
            {
                Log(ERROR_MSG, TEXT("Specified server name is too long (max is %d chars)"),
                    MAX_SERVER_NAME_LEN);
                return FALSE;
            }
            _tcscpy(g_tszServerAddressString, optArg);
            break;

        case TEXT('i'):
            g_wUseIpVer = (WORD)_ttoi(optArg);
            if (
#if SUPPORT_IPV6
                g_wUseIpVer != 6 &&
#endif
                g_wUseIpVer != 4)
            {
                Log(ERROR_MSG, TEXT("Invalid IP version: %s"), optArg);
                return FALSE;
            }
            break;
            
        case TEXT('n'):
            g_dwNumberOfBuffers = (UINT)_ttoi(optArg);
            if (0 == g_dwNumberOfBuffers)
            {
                Log(ERROR_MSG, TEXT("Invalid number of buffers: %s"), optArg);
                return FALSE;
            }
            break;

        case TEXT('p'):
            g_dwNumberOfPingBuffers = (UINT)_ttoi(optArg);
            if (0 == g_dwNumberOfPingBuffers)
            {
                Log(ERROR_MSG, TEXT("Invalid number of ping buffers: %s"), optArg);
                return FALSE;
            }
            break;

        case TEXT('b'):
        {
            // User is defining buffer sizes to run through
            // First see how many buffers are defined
            g_nTestBufferCount = 1;
            int k;
            for (k = 0; k < (int)_tcslen(optArg); k += 1)
                if (optArg[k] == _T(',')) g_nTestBufferCount += 1;

            // Allocate space for the defined buffers
            if (g_aTestBufferSizes != NULL) delete [] g_aTestBufferSizes;
            if (g_aTestBufferSizesUdp != NULL) delete [] g_aTestBufferSizesUdp;

            g_aTestBufferSizes = new int[g_nTestBufferCount];
            g_nTestBufferCountUdp = g_nTestBufferCount;
            g_aTestBufferSizesUdp = new int[g_nTestBufferCount];

            if (g_aTestBufferSizes == NULL || g_aTestBufferSizesUdp == NULL)
            {
                Log(ERROR_MSG, TEXT("Memory error in ProcessCmdLine(): could not allocate memory for buffer sizes"));
                ASSERT(FALSE);
                return FALSE;
            }

            // Fill-in buffer user defined buffer sizes
            const int tempsize = 12;
            TCHAR temp[tempsize];
            int len = 0;
            int cur_buf = 0;
            for (k = 0; k < (int)_tcslen(optArg) + 1; k += 1)
            {
                if (optArg[k] == _T(',') || optArg[k] == _T('\0'))
                {
                    temp[len] = _T('\0');
                    g_aTestBufferSizes[cur_buf] = _ttoi(temp);
                    len = 0;
                    cur_buf += 1;

                    Log(DEBUG_MSG, TEXT("User defined buffer %d (of %d) is %d"),
                        cur_buf, g_nTestBufferCount, g_aTestBufferSizes[cur_buf - 1]);
                }
                else
                {
                    temp[len] = optArg[k];
                    len += 1;
                    if (len >= tempsize)
                    {
                        Log(ERROR_MSG, TEXT("User defined buffer #%d is too large to parse (max len is %d)"),
                            cur_buf + 1, tempsize);
                        return FALSE;
                    }
                }
            }
            
            // Use same buffer sizes for UDP as well
            memcpy(g_aTestBufferSizesUdp, g_aTestBufferSizes, g_nTestBufferCount * sizeof(int));
            break;
        }
        case TEXT('r'):
        {
            g_dwReqSendRateKbps = (UINT)_ttoi(optArg);
            if (0 == g_dwReqSendRateKbps)
            {
                Log(ERROR_MSG, TEXT("Invalid send rate supplied at command line: %s"), optArg);
                return FALSE;
            }
            break;
        }
        case TEXT('t'):
        {
            g_dwReqRecvRateKbps = (UINT)_ttoi(optArg);
            if (0 == g_dwReqRecvRateKbps)
            {
                Log(ERROR_MSG, TEXT("Invalid recv rate supplied at command line: %s"), optArg);
                return FALSE;
            }
            break;
        }
        case TEXT('g'):
        {
            g_fGatewayTestOption = (BOOL)_ttoi(optArg);
            break;
        }
        case TEXT('x'):
        {
            g_tszTestNameExtension = new TCHAR[_tcslen(optArg) + 1 + _tcslen(MODULE_NAME) + 1];
            if (g_tszTestNameExtension == NULL)
            {
                Log(ERROR_MSG, TEXT("Failed to allocate memory for test name extension"));
                return FALSE;
            }
            _stprintf(g_tszTestNameExtension, TEXT("%s_%s"), MODULE_NAME, optArg);
            break;
        }
        case TEXT('k'):
        {
#ifdef UNDER_CE
            int temp = _ttoi(optArg);
            if (temp >= 0 && temp <= 255)
                g_iThreadPriority = temp;
            else
                Log(ERROR_MSG, TEXT("Invalid send/recv thread priority specified %d (expected value 0 to 255)"), temp);
#else
            // Just ignore the option
#endif      
            break;
        }
        case TEXT('m'):
        {
#ifdef UNDER_CE
            if (_tcsicmp(optArg, TEXT("TRUE")) == 0)
                g_fEnableMonteCarlo = TRUE;
            else if (_tcsicmp(optArg, TEXT("FALSE")) == 0)
                g_fEnableMonteCarlo = FALSE;
            else
                Log(ERROR_MSG, TEXT("Invalid MonteCarlo option '%s' (expected TRUE or FALSE"), optArg);
#else
            // Just ignore the option
#endif
            break;
        }
        case TEXT('c'):
        {
#ifdef UNDER_CE
            if (_tcsicmp(optArg, TEXT("TRUE")) == 0)
                g_fEnableAltCpuMon = TRUE;
            else if (_tcsicmp(optArg, TEXT("FALSE")) == 0)
                g_fEnableAltCpuMon = FALSE;
            else
                Log(ERROR_MSG, TEXT("Invalid alternative CPU monitor option '%s' (expected TRUE or FALSE"), optArg);              
#else
            // Just ignore the option
#endif
            break;
        }
        case TEXT('q'):
        {
            DWORD temp = _ttoi(optArg);
            if (temp >= UDP_RECV_MIN_QUEUE_BYTES)
                g_iUdpRcvQueueBytes = temp;
            else
                Log(ERROR_MSG, TEXT("Invalid send/recv thread priority specified %ld (expected value >= %ld)"),
                    temp, UDP_RECV_MIN_QUEUE_BYTES);
        }
        default:
            // bad parameters
            return FALSE;
        }
    }

    return TRUE;
}
    
#ifdef UNDER_CE
// --------------------------------------------------------------------
BOOL DoNdisIOControl
// --------------------------------------------------------------------
(
    DWORD    dwCommand,
    LPVOID    pInBuffer,
    DWORD    cbInBuffer,
    LPVOID    pOutBuffer,
    DWORD    *pcbOutBuffer    OPTIONAL
)
//
//    Execute an NDIS IO control operation.
//
{
    HANDLE    hNdis;
    BOOL    bResult = FALSE;
    DWORD    cbOutBuffer;

    hNdis = CreateFile(DD_NDIS_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_ALWAYS, 0, NULL);

    if (hNdis != INVALID_HANDLE_VALUE)
    {
        cbOutBuffer = 0;
        if (pcbOutBuffer)
            cbOutBuffer = *pcbOutBuffer;

        bResult = DeviceIoControl(hNdis,
            dwCommand,
            pInBuffer,
            cbInBuffer,
            pOutBuffer,
            cbOutBuffer,
            &cbOutBuffer,
            NULL);

        if (bResult == FALSE)
            Log(DEBUG_MSG, TEXT("IoControl result=%d"), bResult);

        if (pcbOutBuffer)
            *pcbOutBuffer = cbOutBuffer;

        CloseHandle(hNdis);
    }
    else
    {
        Log(DEBUG_MSG, TEXT("CreateFile of '%s' failed, error=%d"), DD_NDIS_DEVICE_NAME, GetLastError());
    }

    return bResult;
}

// --------------------------------------------------------------------
BOOL DoNdisMultiSzIOControl
// --------------------------------------------------------------------
(
    DWORD    dwCommand,
    ...
)
//
//    Do an IOControl to NDIS, passing to NDIS a multi_sz string buffer.
//    The variable parameters to this function are a NULL terminated list of
//    strings from which to build the multi_sz.
//
{
    TCHAR    multiSzBuffer[256], *tszBuf, *tszArg;
    va_list ArgList;

    va_start (ArgList, dwCommand);

    tszBuf = &multiSzBuffer[0];

    // Build the multi_sz
    while (tszArg = va_arg(ArgList, TCHAR *))
    {
        _tcscpy(tszBuf, tszArg);
        tszBuf += _tcslen(tszArg) + 1;
    }

    // Add the extra terminator to denote end of the multi_sz
    *tszBuf++ = TEXT('\0');

    va_end(ArgList);

    return DoNdisIOControl(dwCommand, &multiSzBuffer[0], (tszBuf - &multiSzBuffer[0]) * sizeof(TCHAR), NULL, NULL);
}
#endif

