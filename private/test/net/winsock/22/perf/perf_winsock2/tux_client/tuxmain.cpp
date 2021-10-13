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
#include "strsafe.h"
#ifdef UNDER_CE
#include <aygshell.h>
#endif

PFN_INITCONSUMECPU initConsumeCpu = NULL;
PFN_STARTCONSUMECPUTIME startConsumeCpuTime = NULL;
PFN_STOPCONSUMECPUTIME stopConsumeCpuTime = NULL;
PFN_GETCPUUSAGE getCpuUsage = NULL;
PFN_DEINITCONSUMECPU deinitConsumeCpu = NULL;

HINSTANCE hinstConsumeCpuLib = NULL; 

BOOL fConsumeCpuDllLoaded = FALSE;

HKEY g_hKey=NULL;

// --------------------------------------------------------------------
// Defaults

const DWORD MAX_INTERFACE_NAME_SIZE = 128;

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
BOOL g_fUserSelectedBuffer = FALSE;     // flag to know whether user has selected buffer size in command line
BOOL g_fUserSelectedNumOfBuffers = FALSE;     // flag to know whether user has selected buffer size in command line

int* g_aTestBufferSizes = NULL;
int g_nTestBufferCount = 0;
int* g_aTestBufferSizesUdp = NULL;
int g_nTestBufferCountUdp = 0;
double* g_aTestPassRate = NULL;

static HANDLE   hTestThread = NULL;
HINSTANCE g_hInstance = NULL;              // Instance of this application

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

BOOL g_fKeepAliveControlChannel = FALSE;// Keep Alive option for Control Channel

DWORD g_dwMinTotalPacketsCpuUtil = DEFAULT_NUMBER_OF_BUFFERS; // The minimum number of packets for TCP cpu util tests
int g_nTestCpuUtilCount=0;              // Count of Cpu utils 
int* g_aTestCpuUtils = NULL;            // Cpu utils array
BOOL g_fTestCpuUtilOptionSet=FALSE;     // Indicate that -o is in use. This option shouldn't be used
                                        // when running the TCP test all CPU utilizations tests 
BOOL g_fConsumeCpu=FALSE;               // this flag is set when -d option is enabled 

//
// This is the time the results window might be displayed. 10 hours
//
const int MAX_TEST_DURATION = 10 * 60 * 60 * 1000;
DWORD g_dwTestTerminateFrequency = MAX_TEST_DURATION;
HANDLE g_hTerminateTest = NULL;

BOOL   g_fGatherIphlpapiStats = FALSE;
BOOL   g_fHardBindToInterface = FALSE;
TCHAR  *g_tszInterfaceToBind;

//
// Display UI with results.
//
BOOL g_fDisplayResultsUI = FALSE;
HANDLE g_hDisplayResultsUI = NULL;
HWND g_hwndEdit = NULL;
HWND g_hMainWnd = NULL;

#define ID_MENU         10
#define IDM_QUIT        100
#define IDS_QUIT        1001

const INT UDP_RECV_MIN_QUEUE_BYTES = 8192;
int g_iUdpRcvQueueBytes = 32768;        // Specifies UDP recv queue size, in bytes (on WinCE 5.00, default is 32768)

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
int InitApp();
unsigned long __stdcall DisplayUIThread(void* pParams);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// --------------------------------------------------------------------
// Tux testproc function table
//

#define WTID_BASE    1000
#define CPU_BASE     2000

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
    _T("TCP SendRecv Throughput"),      1, 0,                  WTID_BASE+12, TestTCPSendRecvThroughput,
    _T("UDP SendRecv Throughput"),      1, 0,                  WTID_BASE+13, TestUDPSendRecvThroughput,

    // TCP tests for various CPU usages:
    _T("TCP Send Throughputs for 10%-100% of the max CPU usage"),1, TestCpuUsages,   CPU_BASE+1,  TestTCPSendThroughput,
    _T("TCP Recv Throughputs for 10%-100% of the max CPU usage"),1, TestCpuUsages,   CPU_BASE+2,  TestTCPRecvThroughput,
    _T("TCP Send Throughputs Nagle Off for 10%-100% of the max CPU usage"),1, TestCpuUsages | TestOpt_NagleOff,   CPU_BASE+3,  TestTCPSendThroughput,
    _T("TCP Recv Throughputs Nagle Off for 10%-100% of the max CPU usage"),1, TestCpuUsages | TestOpt_NagleOff,   CPU_BASE+4,  TestTCPRecvThroughput,
    _T("TCP Send/Recv Trhoughputs for 10%-100% of the max CPU usage"),1, TestCpuUsages, CPU_BASE+5,TestTCPCPUSendRecvThroughput,
    _T("TCP Send/Recv Throughputs Nagle Off for 10%-100% of the max CPU usage"),1, TestCpuUsages | TestOpt_NagleOff,   CPU_BASE+6,  TestTCPCPUSendRecvThroughput,
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
    g_hInstance = (HINSTANCE)hInstance;
    
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

            const int DefaultTestCpuUtils[]
                = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
            g_nTestCpuUtilCount = 10;
            g_aTestCpuUtils = new int[g_nTestCpuUtilCount];
            if (g_aTestCpuUtils == NULL)
            {
                Log(ERROR_MSG, TEXT("Memory error in ShellProc(SPM_LOAD_DLL): no memory for TCP CPU usages."));
                ASSERT(FALSE);
                return SPR_FAIL;
            }
            memcpy(g_aTestCpuUtils, DefaultTestCpuUtils, g_nTestCpuUtilCount * sizeof(int));

            g_nTestBufferCount = 12;
            g_aTestBufferSizes = new int[g_nTestBufferCount];
            if (g_aTestBufferSizes == NULL)
            {
                Log(ERROR_MSG, TEXT("Memory error in ShellProc(SPM_LOAD_DLL): no memory for TCP buffer sizes."));
                ASSERT(FALSE);
                return SPR_FAIL;
            }

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

            g_aTestPassRate = new double[g_nTestBufferCount];
            if(!g_aTestPassRate)
            {
                Log(ERROR_MSG, TEXT("Memory error in ShellProc(SPM_LOAD_DLL): no memory for Test Pass criteia array"));
                ASSERT(FALSE);
                return SPR_FAIL;
            }

            memset(g_aTestPassRate, 0, sizeof(double) * g_nTestBufferCount);
            
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
            _tcscpy_s(g_tszServerAddressString, MAX_SERVER_NAME_LEN, DEFAULT_SERVER);

#ifdef UNDER_CE
            g_iThreadPriority = CeGetThreadPriority(GetCurrentThread());
#endif

            // load consumecpu if presented
            hinstConsumeCpuLib = LoadLibrary(TEXT("consumecpu"));
            if(hinstConsumeCpuLib != NULL)
            {
#ifdef UNDER_CE
                initConsumeCpu = (PFN_INITCONSUMECPU) GetProcAddress(hinstConsumeCpuLib, TEXT("initConsumeCpu"));
                startConsumeCpuTime = (PFN_STARTCONSUMECPUTIME) GetProcAddress(hinstConsumeCpuLib, TEXT("startConsumeCpuTime"));
                stopConsumeCpuTime = (PFN_STOPCONSUMECPUTIME) GetProcAddress(hinstConsumeCpuLib, TEXT("stopConsumeCpuTime"));
                getCpuUsage = (PFN_GETCPUUSAGE) GetProcAddress(hinstConsumeCpuLib, TEXT("getCpuUsage"));
                deinitConsumeCpu = (PFN_DEINITCONSUMECPU) GetProcAddress(hinstConsumeCpuLib, TEXT("deinitConsumeCpu"));
#elif UNDER_NT
                initConsumeCpu = (PFN_INITCONSUMECPU) GetProcAddress(hinstConsumeCpuLib, "initConsumeCpu");
                startConsumeCpuTime = (PFN_STARTCONSUMECPUTIME) GetProcAddress(hinstConsumeCpuLib, "startConsumeCpuTime");
                stopConsumeCpuTime = (PFN_STOPCONSUMECPUTIME) GetProcAddress(hinstConsumeCpuLib, "stopConsumeCpuTime");
                getCpuUsage = (PFN_GETCPUUSAGE) GetProcAddress(hinstConsumeCpuLib, "getCpuUsage");
                deinitConsumeCpu = (PFN_DEINITCONSUMECPU) GetProcAddress(hinstConsumeCpuLib, "deinitConsumeCpu");
#endif
                if((initConsumeCpu != NULL) && (startConsumeCpuTime != NULL) &&
                    (stopConsumeCpuTime != NULL) && (getCpuUsage != NULL) &&
                    (deinitConsumeCpu != NULL))
                {
                    fConsumeCpuDllLoaded = TRUE;
                }
                else
                {
                    FreeLibrary(hinstConsumeCpuLib); 
                }

            }

            return SPR_HANDLED;
        }
        // Message: SPM_UNLOAD_DLL
        //
        case SPM_UNLOAD_DLL: 
            // It's possible that both are pointing to the same memory location
            // (see handling of user options) - have to make sure to release memory only once
            // in this case.
            if (g_aTestCpuUtils != NULL) delete [] g_aTestCpuUtils;
            if (g_aTestBufferSizes != NULL) delete [] g_aTestBufferSizes;
            if (g_aTestBufferSizesUdp != NULL) delete [] g_aTestBufferSizesUdp;
            if (g_tszInterfaceToBind != NULL) LocalFree(g_tszInterfaceToBind);
            if(g_aTestPassRate) delete [] g_aTestPassRate;

            // deinit consumeCpu if it's set in the command line
            if(g_fConsumeCpu) 
                deinitConsumeCpu();

            if(fConsumeCpuDllLoaded)
                FreeLibrary(hinstConsumeCpuLib); 

            // Cleanup Winsock resources.
            WSACleanup();
            
            if(g_fDisplayResultsUI == TRUE)
            {
                //
                // Either wait for window to be closed or timeout (MAX_TEST_DURATION).
                //
                WaitForSingleObject(g_hTerminateTest, g_dwTestTerminateFrequency);
            }
            if(g_hKey!=NULL)
            {
                RegDeleteValue(g_hKey, TEXT("NoRelease"));
                RegCloseKey(g_hKey);
            }
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
            Log(ERROR_MSG, _T("ShellProc received bad message: 0x%X"), uMsg);;
            return SPR_NOT_HANDLED;
    }
}


// --------------------------------------------------------------------
// Internal functions

const DWORD MAX_OPT_ARG = 256;
const TCHAR END_OF_STRING = (TCHAR)NULL;
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
    Log(REQUIRED_MSG, TEXT("                            [-c <alt_cpu_mon_opt>] [-f <output_file_name>] [-u <display_result_UI>]"));
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
    Log(REQUIRED_MSG, TEXT("    -f <output_file_name> : Redirect debug output to file"));
    Log(REQUIRED_MSG, TEXT("    -u <display_result_UI>: Display a UI with results (default=%s)"), g_fDisplayResultsUI? TEXT("TRUE") : TEXT("FALSE"));
    Log(REQUIRED_MSG, TEXT("    -a <gather_net_stats> : Gather Networking statistics (default=%s)"), g_fGatherIphlpapiStats? TEXT("TRUE") : TEXT("FALSE"));
    Log(REQUIRED_MSG, TEXT("    -y <Interface IP>     : Use this interface for Data transfer"));
    Log(REQUIRED_MSG, TEXT("    -o <cpu_util>         : CPU utilization between 10%% and 100%% of the max CPU utilization reached "));
    Log(REQUIRED_MSG, TEXT("                            in which the TCP send/recv rates will be measured. e.g: -o 10 for 10%%."));
    Log(REQUIRED_MSG, TEXT("                            Do NOT use send/recv rate options when this option is used."));
    Log(REQUIRED_MSG, TEXT("    -e <cpu_util>         : Additional CPU utilization between ~10%% and ~90%% to be loaded to the system while running the test"));
#ifdef UNDER_CE
    Log(REQUIRED_MSG, TEXT("    -k <ce_priority>      : Set priority of send/recv thread on CE (default=%d)"), g_iThreadPriority);
    Log(REQUIRED_MSG, TEXT("    -v <KeepAlive>        : Set keep alive option for control channel only (default=%s)"), g_fKeepAliveControlChannel? TEXT("TRUE") : TEXT("FALSE"));
    Log(REQUIRED_MSG, TEXT("    -m <montecarlo_opt>   : Enable/disable MonteCarlo logging during test run (default=%s)"), g_fEnableMonteCarlo ? TEXT("TRUE") : TEXT("FALSE"));
    Log(REQUIRED_MSG, TEXT("    -l <NORlease>         : Enable current directory, TRUE enables current directory,FALSE enables release directory\n"));
    Log(REQUIRED_MSG, TEXT("    -c <alt_cpu_mon_opt>  : For platforms not supporting GetIdleTime(), can use enable\n"));
    Log(REQUIRED_MSG, TEXT("                            another method of taking CPU measurements (default=%s)"), g_fEnableAltCpuMon ? TEXT("TRUE") : TEXT("FALSE"));
#endif
    Log(REQUIRED_MSG, TEXT("    -q <udp_recv_queue>   : Set size of UDP recv queue, in bytes (default=%d bytes),"), g_iUdpRcvQueueBytes);
    Log(REQUIRED_MSG, TEXT("                            this option applies for all UDP recv tests."));
    Log(REQUIRED_MSG, TEXT("    -z <pass_rate_list> : Allows listing of comma-delimited pass rate to"));
    Log(REQUIRED_MSG, TEXT("                            iterate through, where"));
    Log(REQUIRED_MSG, TEXT("                            <pass_rate_list> = <bufSize_PassBandwidthRate_1>,<bufSize_PassBandwidthRate_2>,...,<bufSize_PassBandwidthRate_N>"));
#if 0
    // TODO: Support these options.
    Log(REQUIRED_MSG, TEXT("    -v <data_verify_flag> : Specifies verification of data: 1 enable, 0 disable (default %d)"), g_fDataVerifyFlag);
    Log(REQUIRED_MSG, TEXT("    -r <data_rand_flag>   : Specifies randomization of data: 1 enable, 0 disable (default %d)"), g_fDataRandomizeFlag);
#endif
}

static BOOL SetPassRate(TCHAR* rate)
{
    TCHAR* passRate = rate;
    while(*passRate != _T('\0'))
    {
         if(*passRate == _T('_'))
         {
            *passRate = _T('\0');
            passRate ++;
            break;
         }

         passRate ++;
    }

    if(*passRate == _T('\0'))
       return FALSE;

    int bufSize = _ttoi(rate);
    TCHAR *argEnd = NULL;
    double argValue = _tcstod(passRate,&argEnd);

    if (argEnd == NULL || argEnd[0] != TEXT('\0'))
         return FALSE;    

    for(int index =0; index < g_nTestBufferCount; index ++)
    {
        if(g_aTestBufferSizes[index] == bufSize)
        {
            g_aTestPassRate[index] = argValue;
            return TRUE;
        }
    }

    return FALSE;

}

// --------------------------------------------------------------------
BOOL ProcessCmdLine( LPCTSTR szCmdLine )
// --------------------------------------------------------------------
{
    // parse command line
    int option = 0;

    LPCTSTR szOpt = TEXT("s:i:n:p:b:r:t:g:x:k:m:c:v:f:u:a:y:z:o:e:l:q:");
    
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
            _tcscpy_s(g_tszServerAddressString, MAX_SERVER_NAME_LEN, optArg);
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
            g_fUserSelectedNumOfBuffers = TRUE;
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
            int   oldBufCount = g_nTestBufferCount;
            double* tmpRate = g_aTestPassRate;
            int* oldTestBufSize = g_aTestBufferSizes;
            
            g_nTestBufferCount = 1;
            g_fUserSelectedBuffer = TRUE; // Set this flag to TRUE as user entered buffer size in command line
            int k;
            for (k = 0; k < (int)_tcslen(optArg); k += 1)
                if (optArg[k] == _T(',')) g_nTestBufferCount += 1;

            // Allocate space for the defined buffers
            if (g_aTestBufferSizesUdp != NULL) delete [] g_aTestBufferSizesUdp;

            g_aTestBufferSizes = new int[g_nTestBufferCount];
            g_nTestBufferCountUdp = g_nTestBufferCount;
            g_aTestBufferSizesUdp = new int[g_nTestBufferCount];
            g_aTestPassRate = new double[g_nTestBufferCount];

            if (g_aTestBufferSizes == NULL || g_aTestBufferSizesUdp == NULL || !g_aTestPassRate)
            {
                Log(ERROR_MSG, TEXT("Memory error in ProcessCmdLine(): could not allocate memory for buffer sizes"));
                ASSERT(FALSE);
                if(tmpRate) delete [] tmpRate;
                if(oldTestBufSize) delete [] oldTestBufSize;
                return FALSE;
            }

            memset(g_aTestPassRate, 0, sizeof(double) *g_nTestBufferCount );
            
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
                        
                        if(tmpRate) delete [] tmpRate;
                        if(oldTestBufSize) delete [] oldTestBufSize;
                        return FALSE;
                    }
                }
            }

            // Copy over the possible rate
            for(int index = 0; index < oldBufCount; index ++)
            {
               for(int j = 0; j < g_nTestBufferCount; j++)
               {
                  if(g_aTestBufferSizes[j] ==   oldTestBufSize[index])
                      g_aTestPassRate[j] = tmpRate[index];
               }
            }
            
            // Use same buffer sizes for UDP as well
            memcpy(g_aTestBufferSizesUdp, g_aTestBufferSizes, g_nTestBufferCount * sizeof(int));

            if(tmpRate) delete [] tmpRate;
            if(oldTestBufSize) delete [] oldTestBufSize;
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
            UINT uNameLen = _tcslen(optArg) + 1 + _tcslen(MODULE_NAME) + 1;
            g_tszTestNameExtension = new TCHAR[uNameLen];
            if (g_tszTestNameExtension == NULL)
            {
                Log(ERROR_MSG, TEXT("Failed to allocate memory for test name extension"));
                return FALSE;
            }
            StringCchPrintf(g_tszTestNameExtension, uNameLen, TEXT("%s_%s"), MODULE_NAME, optArg);
            break;
        }
        case TEXT('y'):
        {
            if (_tcslen(optArg) + 1 > MAX_INTERFACE_NAME_SIZE)
            {
                Log(ERROR_MSG, TEXT("Specified interface name is too long (max is %d chars)"),
                    MAX_INTERFACE_NAME_SIZE);
                return FALSE;
            }
            g_fHardBindToInterface = TRUE;
            g_tszInterfaceToBind = (TCHAR *)LocalAlloc(LPTR, sizeof(TCHAR) * MAX_INTERFACE_NAME_SIZE);
            if(g_tszInterfaceToBind == NULL) 
            {
                Log(ERROR_MSG, TEXT("Unable to allocate memory for g_tszInterfaceToBind.\r\n"));
                break;
            }
            StringCbCopy(g_tszInterfaceToBind, sizeof(g_tszInterfaceToBind), optArg);
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
        case TEXT('f'):
        {
            if(FileInitialize(optArg) == FALSE) Log(ERROR_MSG, TEXT("Unable to initialize file %s"), optArg);
            else Log(DEBUG_MSG, TEXT("Initialized output file %s"), optArg);
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
        case TEXT('v'):
        {
#ifdef UNDER_CE
            if (_tcsicmp(optArg, TEXT("TRUE")) == 0)
                g_fKeepAliveControlChannel= TRUE;
            else if (_tcsicmp(optArg, TEXT("FALSE")) == 0)
                g_fKeepAliveControlChannel = FALSE;
            else
                Log(ERROR_MSG, TEXT("Invalid keep alive option '%s' (expected TRUE or FALSE"), optArg);              
#else
            // Just ignore the option
#endif
            break;
        }
        case TEXT('a'):
        {
#ifdef UNDER_CE
            if (_tcsicmp(optArg, TEXT("TRUE")) == 0)
                g_fGatherIphlpapiStats = TRUE;
            else if (_tcsicmp(optArg, TEXT("FALSE")) == 0)
                g_fGatherIphlpapiStats = FALSE;
            else
                Log(ERROR_MSG, TEXT("Invalid IPHlpAPI stats option '%s' (expected TRUE or FALSE"), optArg);              
#else
            // Just ignore the option
#endif
            break;
        }
        case TEXT('u'):
        {
            if (_tcsicmp(optArg, TEXT("TRUE")) == 0)
                g_fDisplayResultsUI = TRUE;
            else if (_tcsicmp(optArg, TEXT("FALSE")) == 0)
                g_fDisplayResultsUI = FALSE;
            else
                Log(ERROR_MSG, TEXT("Invalid display_ui option '%s' (expected TRUE or FALSE)"), optArg);   

            if(g_fDisplayResultsUI == TRUE)
            {
                //
                // start a thread to create the window and wait for messages.
                //
                g_hDisplayResultsUI= CreateThread(
                    NULL,                               // security attributes
                    0,                                  // stack size
                    DisplayUIThread,                    // start function
                    (void *)NULL,                       // parameter to pass to the thread
                    0,                                  // creation flags
                    NULL);
                if (g_hDisplayResultsUI == NULL) return FALSE;

                g_hTerminateTest = CreateEvent(NULL, TRUE, FALSE, NULL);
                if (g_hTerminateTest == NULL) 
                {
#ifdef UNDER_CE
                    RETAILMSG(1, (TEXT("CreateEvent(g_hTerminateTest) Failed (%d)\r\n"), GetLastError()));
#endif
                    return SPR_FAIL;
                }
                
                //
                // Sleep for a little while so the UI window gets updated
                //
                Sleep(500);
            }
            break;
        }
        case TEXT('q'):
        {
            DWORD temp = _ttoi(optArg);
            if (temp >= UDP_RECV_MIN_QUEUE_BYTES)
                g_iUdpRcvQueueBytes = temp;
            else
                Log(ERROR_MSG, TEXT("Invalid UDP Receive Queue Bytes specified %ld (expected value >= %ld)"),
                    temp, UDP_RECV_MIN_QUEUE_BYTES);
        }
        case TEXT('z'):
        {
            // Fill-in buffer user defined buffer sizes
            const int tempsize = 64;
            int k;
            TCHAR temp[tempsize];
            int len = 0;
            for (k = 0; k < (int)_tcslen(optArg) + 1; k += 1)
            {
                if (optArg[k] == _T(',') || optArg[k] == _T('\0'))
                {
                    temp[len] = _T('\0');
                    if(!SetPassRate(temp))
                    {
                        Log(ERROR_MSG, TEXT("User defined passing rate is invalid:  %s)"),
                            temp);
                        return FALSE;
                    }
                        
                    len = 0;
                }
                else
                {
                    temp[len] = optArg[k];
                    len += 1;
                    if (len >= tempsize)
                    {
                        Log(ERROR_MSG, TEXT("User defined passing rate is too large to parse (max len is %d)"),
                            tempsize);
                        return FALSE;
                    }
                }
            }
            
            // Use same buffer sizes for UDP as well
            memcpy(g_aTestBufferSizesUdp, g_aTestBufferSizes, g_nTestBufferCountUdp * sizeof(int));
            break;

        }
        case TEXT('o'):
        {

            g_fTestCpuUtilOptionSet= TRUE; 
            Log(REQUIRED_MSG, TEXT("Note: -o option is applicable to TCP send and recv tests only (tests 1001,1002,1004,1005)."));
            Log(REQUIRED_MSG, TEXT("Other tests with this option enabled won't change results"));
            // User is defining CPU utlizations to run through
            // First see how many CPU utilizations are defined
            int   oldCpuCount = g_nTestCpuUtilCount;
            int* oldTestCpu = g_aTestCpuUtils;
            
            g_nTestCpuUtilCount= 1;
            int k;
            for (k = 0; k < (int)_tcslen(optArg); k += 1)
                if (optArg[k] == _T(',')) g_nTestCpuUtilCount+= 1;

            // Allocate space for the defined buffers
            if (g_aTestCpuUtils!= NULL) delete [] g_aTestCpuUtils;

            g_aTestCpuUtils= new int[g_nTestCpuUtilCount];

            if (g_aTestCpuUtils== NULL )
            {
                Log(ERROR_MSG, TEXT("Memory error in ProcessCmdLine(): could not allocate memory for CPU utilizations"));
                ASSERT(FALSE);
                if(oldTestCpu) delete [] oldTestCpu;
                return FALSE;
            }
            
            // Fill-in buffer user defined buffer sizes
            const int tempsize = 4;
            TCHAR temp[tempsize];
            int len = 0;
            int cur_buf = 0;
            for (k = 0; k < (int)_tcslen(optArg) + 1; k += 1)
            {
                if (optArg[k] == _T(',') || optArg[k] == _T('\0'))
                {
                    temp[len] = _T('\0');
                    g_aTestCpuUtils[cur_buf] = _ttoi(temp);

                    if(g_aTestCpuUtils[cur_buf] < 10 || g_aTestCpuUtils[cur_buf] > 100)
                    {
                        Log(ERROR_MSG, TEXT("Invalid CPU utilization. Valid range: 10%%-100%%."));
                        if(oldTestCpu) delete [] oldTestCpu;
                        return FALSE;
                    }
                    len = 0;
                    cur_buf += 1;

                    Log(DEBUG_MSG, TEXT("User defined CPU usage %d (of %d) is %d"),
                        cur_buf, g_nTestCpuUtilCount, g_aTestCpuUtils[cur_buf - 1]);
                }
                else
                {
                    temp[len] = optArg[k];
                    len += 1;
                    if (len >= tempsize)
                    {
                        Log(ERROR_MSG, TEXT("User defined CPU usage #%d is too large to parse (max len is %d)"),
                            cur_buf + 1, tempsize);
                        
                        if(oldTestCpu) delete [] oldTestCpu;
                        return FALSE;
                    }
                }
            }
            break;
        }
        case TEXT('e'):
        {
            DWORD cpuutil = (UINT)_ttoi(optArg);

            // this option is not enabled unless consumecpu.dll is presented
            if(fConsumeCpuDllLoaded == FALSE)
            {
                Log(ERROR_MSG, TEXT("-e option is not supported"));
                return FALSE;
            }

            if(initConsumeCpu(cpuutil) == FALSE)
            {
                Log(ERROR_MSG, TEXT("Unable to initialize the consumecpu thread. Valid CPU usage range is 10%% to 90%%"));
                return FALSE;
            }
            else
            {
                Log(REQUIRED_MSG, TEXT("Additional ~%u%% CPU loaded to the system while running the test."), cpuutil);
                Log(REQUIRED_MSG, TEXT("NOTE: Result log \"System load\" shows the amount of CPU consumed excluding CPU used to run the test."));
            }
            g_fConsumeCpu = TRUE;

            break;
        }
        case TEXT('l'):
        {
#ifdef UNDER_CE
            
            DWORD dwData = 1, cbData = sizeof(dwData);
            BOOL bRet=FALSE;   
            if(_tcsicmp(optArg, TEXT("TRUE")) == 0)
            {
                if(ERROR_SUCCESS == RegCreateKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\Microsoft\\Perf"),0, NULL, 0 ,0, NULL, &g_hKey, NULL))
                {
                    if(ERROR_SUCCESS != RegSetValueEx(g_hKey,TEXT("NoRelease"), 0, REG_DWORD,(BYTE *)&dwData, cbData))
                    {
                        Log(ERROR_MSG, TEXT("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Perf\\NoRelease can not be set to 1"));
                    }
                    else
                    {
                        bRet=TRUE;  
                    }
                } 
            }
            else if(_tcsicmp(optArg, TEXT("FALSE")) == 0)
            {
                dwData = 0;
                if(ERROR_SUCCESS == RegCreateKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\Microsoft\\Perf"),0, NULL, 0 ,0, NULL, &g_hKey, NULL))
                {
                    if(ERROR_SUCCESS != RegSetValueEx(g_hKey,TEXT("NoRelease"), 0, REG_DWORD,(BYTE *)&dwData, cbData))
                    {
                        Log(ERROR_MSG, TEXT("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Perf\\NoRelease can not be set to 0"));
                    }
                    else
                    {
                        bRet=TRUE;  
                    }
                } 
            }
            else
            {
                Log(ERROR_MSG, TEXT("Invalid NoRelease option '%s' (expected TRUE or FALSE"), optArg);
            }
            if(!bRet)
                return bRet;
#else
            // Just ignore the option
#endif
            break;
        }

        default:
            // bad parameters
            return FALSE;
        }
    }

    return TRUE;
}

//
// WndProc for the Results UI
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{    
    switch (message) 
    {
        case WM_CREATE:
            break;

        case WM_SIZE:
            if (SIZE_MINIMIZED==wParam)         // Hack away the "Smart Minimize" feature
            DestroyWindow(hWnd);               // We need to quit on close to prevent lurking in the backround
            break;

        case WM_DESTROY:
            SetEvent(g_hTerminateTest);
            PostQuitMessage(0);
            break;

        case WM_QUIT:
            SetEvent(g_hTerminateTest);
            break;
            
        case WM_CLOSE:
            SetEvent(g_hTerminateTest);
            break;

        case WM_COMMAND:
            break;
            
        case WM_VSCROLL:
        case WM_KEYDOWN:
            switch (wParam)
            {
                case VK_UP:
                    SendMessage(g_hwndEdit, EM_SCROLL, SB_PAGEUP, 0);
                    break;

                case VK_DOWN:
                    SendMessage(g_hwndEdit, EM_SCROLL, SB_PAGEDOWN, 0);
                    break;

                default:
                    break;
            }
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

//
// Initialize the Results UI
//
int InitApp()
{
    WNDCLASS    wc;
    RECT        rcClient;
    HWND        hMenuBar = NULL;
    HINSTANCE   hShellInstance = NULL;
    BOOL fSuccess = FALSE;

    wc.style            = 0;
    wc.lpfnWndProc      = (WNDPROC) WndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = g_hInstance;
    wc.hIcon            = NULL;
    wc.hCursor          = 0;
    wc.hbrBackground    = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = TEXT("perf_winsock2");

    RegisterClass (&wc);

    // Main window
    g_hMainWnd = CreateWindow ( TEXT("Perf_winsock2"),
                                TEXT("Perf_winsock2"),
                                WS_VISIBLE,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT, 
                                CW_USEDEFAULT,     
                                NULL,                   
                                NULL,           
                                g_hInstance,          
                                NULL);
    
    GetClientRect(GetDesktopWindow(), &rcClient);

    DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | ES_LEFT |
                   ES_MULTILINE | ES_NOHIDESEL | ES_READONLY;

    // Create the window for the edit control window.
    g_hwndEdit = CreateWindow (
                        TEXT("edit"),                       // Class name
                        NULL,                               // Window text
                        dwStyle,                            // Window style
                        0,
                        0,
                        rcClient.right - rcClient.left,
                        rcClient.bottom - rcClient.top,
                        g_hMainWnd,                         // Window handle to the parent window
                        NULL,                               // Control identifier
                        g_hInstance,                        // Instance handle
                        NULL );                             // Specify NULL for this parameter when you create a control


    ShowWindow(g_hMainWnd, SW_SHOW);
    UpdateWindow(g_hMainWnd);

    return 1;
}

unsigned long __stdcall DisplayUIThread(void* pParams)
{
    MSG msg;
    
    //
    // Init app
    //
    InitApp();
    
    while (GetMessage(&msg, NULL, 0, 0)) 
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
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
    tszArg = va_arg(ArgList, TCHAR *);
    while (tszArg)
    {
        _tcscpy_s(tszBuf, 256, tszArg);
        tszBuf += _tcslen(tszArg) + 1;
        tszArg = va_arg(ArgList, TCHAR *);
    }

    // Add the extra terminator to denote end of the multi_sz
    *tszBuf++ = TEXT('\0');

    va_end(ArgList);

    return DoNdisIOControl(dwCommand, &multiSzBuffer[0], (tszBuf - &multiSzBuffer[0]) * sizeof(TCHAR), NULL, NULL);
}
#endif

