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
#include "tuxstuff.h"

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

SOCKADDR_IN g_saServerAddr;
SOCKADDR_IN6 g_saServerAddrV6;
BOOL g_IPv6Detected=TRUE;

BOOL GetRemoteAddress(SOCKADDR_IN *psaAddr, LPCTSTR szName);
BOOL GetRemoteAddressV6(SOCKADDR_IN6 *psaAddr, LPCTSTR szName);
VOID PrintIPv6Addr(SOCKADDR_IN6 *psaAddr);

//******************************************************************************
//***** Windows CE specific code
//******************************************************************************
#ifdef UNDER_CE

#ifndef STARTF_USESIZE
#define STARTF_USESIZE     0x00000002
#endif

#ifndef STARTF_USEPOSITION
#define STARTF_USEPOSITION 0x00000004
#endif

#ifndef ZeroMemory
#define ZeroMemory(Destination,Length) memset(Destination, 0, Length)
#endif

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {
   UNREFERENCED_PARAMETER(hInstance); 
   UNREFERENCED_PARAMETER(dwReason); 
   UNREFERENCED_PARAMETER(lpReserved);
   return TRUE;
}
#endif

#ifndef UNDER_CE

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {
   UNREFERENCED_PARAMETER(hInstance); 
   UNREFERENCED_PARAMETER(dwReason); 
   UNREFERENCED_PARAMETER(lpReserved);
   return TRUE;
}

#define DEBUGZONE(b)        (ZoneMask & (0x00000001<<b))
#define DEBUGMSG(exp, p)    ((exp) ? _tprintf p, 1 : 0)
//#define RETAILMSG(exp, p)    ((exp) ? _tprintf p, 1 : 0)
#define ASSERT assert

#endif

static FUNCTION_TABLE_ENTRY g_lpSkipFTE[] = {
// INVALI_TIMEOUT cases are not applicable in WM7
TEXT(         "IPV4_ICMP_INVALID_TIMEOUT"                    ), 3,                ICMP_INVALID_TIMEOUT, ICMP_SEND_ECHO+ 11, IcmpAPITest,
TEXT(         "IPV6_ICMP_INVALID_TIMEOUT"                    ), 3,USE_IPv6 |      ICMP_INVALID_TIMEOUT, ICMP_SEND_ECHO+ 111, IcmpAPITest,
TEXT(         "ASYNC_ICMP_INVALID_TIMEOUT"                   ), 3,USE_ASYNC |      ICMP_INVALID_TIMEOUT, ICMP_SEND_ECHO+ 211, IcmpAPITest,
// end of list
NULL,                                                       0,                                 0,                     0, NULL,
};

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
TEXT(   "ICMP API Tests"                                     ), 1,                                    0,                    0, NULL,
TEXT(   "IcmpSendEcho() Tests"                               ), 2,                                    0,                    0, NULL,
TEXT(         "IPV4_ICMP_INVALID_HANDLE"                     ), 3,                 ICMP_INVALID_HANDLE, ICMP_SEND_ECHO+  1, IcmpAPITest,
TEXT(         "IPV4_ICMP_NULL_ADDR"                             ), 3,                          ICMP_NULL_ADDR, ICMP_SEND_ECHO+  2, IcmpAPITest,
TEXT(         "IPV4_ICMP_NULL_SEND_BUFFER"                   ), 3,                  ICMP_NULL_SEND_BUFFER, ICMP_SEND_ECHO+  3, IcmpAPITest,
TEXT(         "IPV4_ICMP_SEND_BUFFER_FRAG"                   ), 3,                  ICMP_SEND_BUFFER_FRAG, ICMP_SEND_ECHO+  4, IcmpAPITest,
TEXT(         "IPV4_ICMP_SEND_BUFFER_NO_FRAG"                ), 3,              ICMP_SEND_BUFFER_NO_FRAG, ICMP_SEND_ECHO+  5, IcmpAPITest,
TEXT(         "IPV4_ICMP_SEND_BUFFER_TOO_SMALL"              ), 3,          ICMP_SEND_BUFFER_TOO_SMALL, ICMP_SEND_ECHO+  6, IcmpAPITest,
TEXT(         "IPV4_ICMP_INVALID_SEND_LEN"                   ), 3,               ICMP_INVALID_SEND_LEN, ICMP_SEND_ECHO+  7, IcmpAPITest,
TEXT(         "IPV4_ICMP_NULL_REPLY_BUFFER"                  ), 3,              ICMP_NULL_REPLY_BUFFER, ICMP_SEND_ECHO+  8, IcmpAPITest,
TEXT(         "IPV4_ICMP_INVALID_REPLY_LENGTH"               ), 3,           ICMP_INVALID_REPLY_LENGTH, ICMP_SEND_ECHO+  9, IcmpAPITest,
TEXT(         "IPV4_ICMP_INVALID_SEND_OPTIONS"               ), 3,           ICMP_INVALID_SEND_OPTIONS, ICMP_SEND_ECHO+ 10, IcmpAPITest,
TEXT(         "IPV4_ICMP_UNREACHABLE_DESTINATION"            ), 3,            ICMP_UNREACH_DESTINATION, ICMP_SEND_ECHO+ 12, IcmpAPITest,
TEXT(         "IPV4_ICMP_VALID_REQUEST"                      ), 3,                  ICMP_VALID_REQUEST, ICMP_SEND_ECHO+ 13, IcmpAPITest,
TEXT(   "Icmp6SendEcho2() Tests"                             ), 2,                                    0,                     0, NULL,
TEXT(         "IPV6_ICMP_INVALID_HANDLE"                     ), 3,USE_IPv6 |       ICMP_INVALID_HANDLE, ICMP_SEND_ECHO+ 101, IcmpAPITest,
TEXT(         "IPV6_ICMP_NULL_ADDR"                             ), 3,USE_IPv6 |            ICMP_NULL_ADDR, ICMP_SEND_ECHO+ 102, IcmpAPITest,
TEXT(         "IPV6_ICMP_NULL_SEND_BUFFER"                   ), 3,USE_IPv6 |     ICMP_NULL_SEND_BUFFER, ICMP_SEND_ECHO+ 103, IcmpAPITest,
TEXT(         "IPV6_ICMP_SEND_BUFFER_FRAG"                   ), 3,USE_IPv6 |     ICMP_SEND_BUFFER_FRAG, ICMP_SEND_ECHO+ 104, IcmpAPITest,
TEXT(         "IPV6_ICMP_SEND_BUFFER_NO_FRAG"                ), 3,USE_IPv6 |  ICMP_SEND_BUFFER_NO_FRAG, ICMP_SEND_ECHO+ 105, IcmpAPITest,
TEXT(         "IPV6_ICMP_SEND_BUFFER_TOO_SMALL"              ), 3,USE_IPv6 |ICMP_SEND_BUFFER_TOO_SMALL, ICMP_SEND_ECHO+ 106, IcmpAPITest,
TEXT(         "IPV6_ICMP_INVALID_SEND_LEN"                   ), 3,USE_IPv6 |     ICMP_INVALID_SEND_LEN, ICMP_SEND_ECHO+ 107, IcmpAPITest,
TEXT(         "IPV6_ICMP_NULL_REPLY_BUFFER"                  ), 3,USE_IPv6 |    ICMP_NULL_REPLY_BUFFER, ICMP_SEND_ECHO+ 108, IcmpAPITest,
TEXT(         "IPV6_ICMP_INVALID_REPLY_LENGTH"               ), 3,USE_IPv6 | ICMP_INVALID_REPLY_LENGTH, ICMP_SEND_ECHO+ 109, IcmpAPITest,
TEXT(         "IPV6_ICMP_INVALID_SEND_OPTIONS"               ), 3,USE_IPv6 | ICMP_INVALID_SEND_OPTIONS, ICMP_SEND_ECHO+ 110, IcmpAPITest,
TEXT(         "IPV6_ICMP_UNREACHABLE_DESTINATION"            ), 3,USE_IPv6 |  ICMP_UNREACH_DESTINATION, ICMP_SEND_ECHO+ 112, IcmpAPITest,
TEXT(         "IPV6_ICMP_VALID_REQUEST"                      ), 3,USE_IPv6 |        ICMP_VALID_REQUEST, ICMP_SEND_ECHO+ 113, IcmpAPITest,
TEXT(   "IcmpSendEcho2() Tests"                              ), 2,                                    0,                     0, NULL,
TEXT(         "ASYNC_ICMP_INVALID_HANDLE"                    ), 3,USE_ASYNC |       ICMP_INVALID_HANDLE, ICMP_SEND_ECHO+ 201, IcmpAPITest,
TEXT(         "ASYNC_ICMP_NULL_ADDR"                         ), 3,USE_ASYNC |            ICMP_NULL_ADDR, ICMP_SEND_ECHO+ 202, IcmpAPITest,
TEXT(         "ASYNC_ICMP_NULL_SEND_BUFFER"                  ), 3,USE_ASYNC |     ICMP_NULL_SEND_BUFFER, ICMP_SEND_ECHO+ 203, IcmpAPITest,
TEXT(         "ASYNC_ICMP_SEND_BUFFER_FRAG"                  ), 3,USE_ASYNC |     ICMP_SEND_BUFFER_FRAG, ICMP_SEND_ECHO+ 204, IcmpAPITest,
TEXT(         "ASYNC_ICMP_SEND_BUFFER_NO_FRAG"               ), 3,USE_ASYNC |  ICMP_SEND_BUFFER_NO_FRAG, ICMP_SEND_ECHO+ 205, IcmpAPITest,
TEXT(         "ASYNC_ICMP_SEND_BUFFER_TOO_SMALL"             ), 3,USE_ASYNC |ICMP_SEND_BUFFER_TOO_SMALL, ICMP_SEND_ECHO+ 206, IcmpAPITest,
TEXT(         "ASYNC_ICMP_INVALID_SEND_LEN"                  ), 3,USE_ASYNC |     ICMP_INVALID_SEND_LEN, ICMP_SEND_ECHO+ 207, IcmpAPITest,
TEXT(         "ASYNC_ICMP_NULL_REPLY_BUFFER"                 ), 3,USE_ASYNC |    ICMP_NULL_REPLY_BUFFER, ICMP_SEND_ECHO+ 208, IcmpAPITest,
TEXT(         "ASYNC_ICMP_INVALID_REPLY_LENGTH"              ), 3,USE_ASYNC | ICMP_INVALID_REPLY_LENGTH, ICMP_SEND_ECHO+ 209, IcmpAPITest,
TEXT(         "ASYNC_ICMP_INVALID_SEND_OPTIONS"              ), 3,USE_ASYNC | ICMP_INVALID_SEND_OPTIONS, ICMP_SEND_ECHO+ 210, IcmpAPITest,
TEXT(         "ASYNC_ICMP_UNREACHABLE_DESTINATION"           ), 3,USE_ASYNC |  ICMP_UNREACH_DESTINATION, ICMP_SEND_ECHO+ 212, IcmpAPITest,
TEXT(         "ASYNC_ICMP_VALID_REQUEST"                     ), 3,USE_ASYNC |        ICMP_VALID_REQUEST, ICMP_SEND_ECHO+ 213, IcmpAPITest,
TEXT(   "IcmpParseReplies() Tests"                           ), 2,                                    0,                     0, NULL,
TEXT(         "PARSE4_INVALID_BUFFER"                        ), 3,                    ICMP_INVALID_BUFFER, ICMP_SEND_ECHO+ 301, IcmpAPITest,
TEXT(         "PARSE4_INVALID_LEN"                             ), 3,                       ICMP_INVALID_LEN, ICMP_SEND_ECHO+ 302, IcmpAPITest,
TEXT(         "PARSE4_VALID_PARAMS"                          ), 3,                    ICMP_VALID_PARAMS, ICMP_SEND_ECHO+ 303, IcmpAPITest,
TEXT(   "Icmp6ParseReplies() Tests"                          ), 2,                                    0,                     0, NULL,
TEXT(         "PARSE6_INVALID_BUFFER"                        ), 3,USE_IPv6 |        ICMP_INVALID_BUFFER, ICMP_SEND_ECHO+ 401, IcmpAPITest,
TEXT(         "PARSE6_INVALID_LEN"                             ), 3,USE_IPv6 |           ICMP_INVALID_LEN, ICMP_SEND_ECHO+ 402, IcmpAPITest,
TEXT(         "PARSE6_VALID_PARAMS"                          ), 3,USE_IPv6 |          ICMP_VALID_PARAMS, ICMP_SEND_ECHO+ 403, IcmpAPITest,
NULL,                                                       0,                                 0,                     0, NULL  // marks end of list
};




//******************************************************************************
//***** ShellProc()
//******************************************************************************

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) {
    
    switch (uMsg) {
        
        //------------------------------------------------------------------------
        // Message: SPM_LOAD_DLL
        //
        // Sent once to the DLL immediately after it is loaded.  The spParam 
        // parameter will contain a pointer to a SPS_LOAD_DLL structure.  The DLL
        // should set the fUnicode member of this structre to TRUE if the DLL is
        // built with the UNICODE flag set.  By setting this flag, Tux will ensure
        // that all strings passed to your DLL will be in UNICODE format, and all
        // strings within your function table will be processed by Tux as UNICODE.
        // The DLL may return SPR_FAIL to prevent the DLL from continuing to load.
        //------------------------------------------------------------------------
        
    case SPM_LOAD_DLL: {
        RETAILMSG(1, (TEXT("ShellProc(SPM_LOAD_DLL, ...) called\r\n")));
        
        // If we are UNICODE, then tell Tux this by setting the following flag.
#ifdef UNICODE
        ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif
        return SPR_HANDLED;
                       }
        
        //------------------------------------------------------------------------
        // Message: SPM_UNLOAD_DLL
        //
        // Sent once to the DLL immediately before it is unloaded.
        //------------------------------------------------------------------------
        
    case SPM_UNLOAD_DLL: {
        RETAILMSG(1, (TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called\r\n")));
        
        return SPR_HANDLED;
                         }
        
        //------------------------------------------------------------------------
        // Message: SPM_SHELL_INFO
        //
        // Sent once to the DLL immediately after SPM_LOAD_DLL to give the DLL
        // some useful information about its parent shell and environment.  The
        // spParam parameter will contain a pointer to a SPS_SHELL_INFO structure.
        // The pointer to the structure may be stored for later use as it will
        // remain valid for the life of this Tux Dll.  The DLL may return SPR_FAIL
        // to prevent the DLL from continuing to load.
        //------------------------------------------------------------------------
        
    case SPM_SHELL_INFO: {
        LPTSTR szServer=TEXT("localhost"), szServerV6=TEXT("::1");
        TCHAR szCommandLine[MAX_PATH];
        int argc = 32;
        LPTSTR argv[32];
        WSAData wsaData;
        
        // Store a pointer to our shell info for later use.
        g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
        
        RETAILMSG(1, (TEXT("ShellProc(SPM_SHELL_INFO, ...) called\r\n")));

        // If there's a commandline, parse it
        if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine) 
        {
            // Parse the command line
            StringCchCopy(szCommandLine, MAX_PATH, g_pShellInfo->szDllCmdLine);
            CommandLineToArgs(szCommandLine, &argc, argv);

            StrictOptionsOnly(TRUE);

            GetOption(argc, argv, _T("s"), &szServer);
            GetOption(argc, argv, _T("s6"), &szServerV6);
        }

        // Set the global address variables.
        if(WSAStartup(MAKEWORD(2,2), &wsaData))
        {
            RETAILMSG(1,(TEXT("WSAStartup() failed\r\n")));
            return SPR_FAIL;
        }
        
        if(szServer)
        {
            if(GetRemoteAddress(&g_saServerAddr, szServer) == FALSE)
            {
                RETAILMSG(1, (TEXT("GetRemoteAddress() failed. Revert to IPv4 address=localhost\r\n")));

                GetRemoteAddress(&g_saServerAddr, TEXT("localhost"));
            }
        }
        else
        {
            RETAILMSG(1,(TEXT("Must specify a IPv4 server name/address!\r\n")));
            return SPR_FAIL;
        }
        
        if(szServerV6) 
        {
            if (GetRemoteAddressV6(&g_saServerAddrV6, szServerV6) == FALSE)
            {
                RETAILMSG(1, (TEXT("GetRemoteAddressV6() failed. Skip all IPv6 ICMP tests.\r\n")));
                g_IPv6Detected = FALSE;
            }
        }
        
        WSACleanup();
        
        return SPR_HANDLED;
                         }
        
        //------------------------------------------------------------------------
        // Message: SPM_REGISTER
        //
        // This is the only ShellProc() message that a DLL is required to handle
        // (except for SPM_LOAD_DLL if you are UNICODE).  This message is sent
        // once to the DLL immediately after the SPM_SHELL_INFO message to query
        // the DLL for it's function table.  The spParam will contain a pointer to
        // a SPS_REGISTER structure.  The DLL should store its function table in
        // the lpFunctionTable member of the SPS_REGISTER structure.  The DLL may
        // return SPR_FAIL to prevent the DLL from continuing to load.
        //------------------------------------------------------------------------
        
    case SPM_REGISTER: {
        RETAILMSG(1, (TEXT("ShellProc(SPM_REGISTER, ...) called\r\n")));
        ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
#ifdef UNICODE
        return SPR_HANDLED | SPF_UNICODE;
#else
        return SPR_HANDLED;
#endif
                       }
        
        //------------------------------------------------------------------------
        // Message: SPM_START_SCRIPT
        //
        // Sent to the DLL immediately before a script is started.  It is sent to
        // all Tux DLLs, including loaded Tux DLLs that are not in the script.
        // All DLLs will receive this message before the first TestProc() in the
        // script is called.
        //------------------------------------------------------------------------
        
    case SPM_START_SCRIPT: {
        RETAILMSG(1, (TEXT("ShellProc(SPM_START_SCRIPT, ...) called\r\n")));
        return SPR_HANDLED;
                           }
        
        //------------------------------------------------------------------------
        // Message: SPM_STOP_SCRIPT
        //
        // Sent to the DLL when the script has stopped.  This message is sent when
        // the script reaches its end, or because the user pressed stopped prior
        // to the end of the script.  This message is sent to all Tux DLLs,
        // including loaded Tux DLLs that are not in the script.
        //------------------------------------------------------------------------
        
    case SPM_STOP_SCRIPT: {
        RETAILMSG(1, (TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called\r\n")));
        return SPR_HANDLED;
                          }
        
        //------------------------------------------------------------------------
        // Message: SPM_BEGIN_GROUP
        //
        // Sent to the DLL before a group of tests from that DLL is about to be
        // executed.  This gives the DLL a time to initialize or allocate data for
        // the tests to follow.  Only the DLL that is next to run receives this
        // message.  The prior DLL, if any, will first receive a SPM_END_GROUP
        // message.  For global initialization and de-initialization, the DLL
        // should probably use SPM_START_SCRIPT and SPM_STOP_SCRIPT, or even
        // SPM_LOAD_DLL and SPM_UNLOAD_DLL.
        //------------------------------------------------------------------------
        
    case SPM_BEGIN_GROUP: {
        RETAILMSG(1, (TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called\r\n")));
        return SPR_HANDLED;
                          }
        
        //------------------------------------------------------------------------
        // Message: SPM_END_GROUP
        //
        // Sent to the DLL after a group of tests from that DLL has completed
        // running.  This gives the DLL a time to cleanup after it has been run.
        // This message does not mean that the DLL will not be called again to run
        // tests; it just means that the next test to run belongs to a different
        // DLL.  SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL to track when it
        // is active and when it is not active.
        //------------------------------------------------------------------------
        
    case SPM_END_GROUP: {
        RETAILMSG(1, (TEXT("ShellProc(SPM_END_GROUP, ...) called\r\n")));
        return SPR_HANDLED;
                        }
        
        //------------------------------------------------------------------------
        // Message: SPM_BEGIN_TEST
        //
        // Sent to the DLL immediately before a test executes.  This gives the DLL
        // a chance to perform any common action that occurs at the beginning of
        // each test, such as entering a new logging level.  The spParam parameter
        // will contain a pointer to a SPS_BEGIN_TEST structure, which contains
        // the function table entry and some other useful information for the next
        // test to execute.  If the ShellProc function returns SPR_SKIP, then the
        // test case will not execute.
        //------------------------------------------------------------------------
        
    case SPM_BEGIN_TEST: {
        RETAILMSG(1, (TEXT("ShellProc(SPM_BEGIN_TEST, ...) called\r\n")));
        
        Sleep(1000);
        // Start our logging level.
        LPSPS_BEGIN_TEST pBT = (LPSPS_BEGIN_TEST)spParam;
        RETAILMSG( 1, 
            ( TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u\r\n"),
            pBT->lpFTE->lpDescription, pBT->dwThreadCount,
            pBT->dwRandomSeed ) );
        
        return SPR_HANDLED;
                         }
        
        //------------------------------------------------------------------------
        // Message: SPM_END_TEST
        //
        // Sent to the DLL after a single test executes from the DLL.  This gives
        // the DLL a time perform any common action that occurs at the completion
        // of each test, such as exiting the current logging level.  The spParam
        // parameter will contain a pointer to a SPS_END_TEST structure, which
        // contains the function table entry and some other useful information for
        // the test that just completed.
        //------------------------------------------------------------------------
        
    case SPM_END_TEST: {
        RETAILMSG(1, (TEXT("ShellProc(SPM_END_TEST, ...) called\r\n")));
        
        // End our logging level.
        LPSPS_END_TEST pET = (LPSPS_END_TEST)spParam;

        RETAILMSG( 1, 
            ( TEXT("END TEST: \"%s\", %s, Time=%u.%03u\r\n"),
            pET->lpFTE->lpDescription,
            pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
        pET->dwResult == TPR_PASS ? TEXT("PASSED") :
        pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
            pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000 ) );
        
        return SPR_HANDLED;
                       }
        
        //------------------------------------------------------------------------
        // Message: SPM_EXCEPTION
        //
        // Sent to the DLL whenever code execution in the DLL causes and exception
        // fault.  By default, Tux traps all exceptions that occur while executing
        // code inside a Tux DLL.
        //------------------------------------------------------------------------
        
    case SPM_EXCEPTION: {
        RETAILMSG(1, (TEXT("ShellProc(SPM_EXCEPTION, ...) called\r\n")));
        return SPR_HANDLED;
                        }
   }
   
   return SPR_NOT_HANDLED;
}

//******************************************************************************
//***** Internal Functions
//******************************************************************************
#define MAX_NAME_LENGTH        128

BOOL GetRemoteAddress(SOCKADDR_IN *psaAddr, LPCTSTR szName)
{
    char szNameASCII[MAX_NAME_LENGTH];
    HOSTENT *lpHostData;
    size_t retVal = 0;
    
#if defined UNICODE
    wcstombs_s(&retVal, szNameASCII, MAX_NAME_LENGTH, szName, sizeof(szNameASCII));
#else
    strncpy(szNameASCII, szName, sizeof(szNameASCII));
#endif
    
    memset(psaAddr, 0, sizeof(psaAddr));
    psaAddr->sin_family = AF_INET;
    
    if((psaAddr->sin_addr.s_addr = inet_addr(szNameASCII)) == INADDR_NONE)
    {
        if((lpHostData = gethostbyname(szNameASCII)) == NULL)
        {
            RETAILMSG(1,
                (TEXT("Error getting host information for %hs\r\n"), szNameASCII));
            
            return FALSE;
        }
        else
        {
            memcpy(&psaAddr->sin_addr.s_addr, lpHostData->h_addr_list[0],
                sizeof(IN_ADDR));
            
            RETAILMSG(1,
                (TEXT("Setting IPv4 server address to 0x%08x\r\n"), htonl(psaAddr->sin_addr.s_addr)));
        }
    }
    
    return TRUE;
}

BOOL GetRemoteAddressV6(SOCKADDR_IN6 *psaAddr, LPCTSTR szName)
{
    char szNameASCII[MAX_NAME_LENGTH];
    ADDRINFO AddrHints, *pAddrInfo;
    size_t retVal = 0;
    
#if defined UNICODE
    wcstombs_s(&retVal, szNameASCII, MAX_NAME_LENGTH, szName, sizeof(szNameASCII));
#else
    strncpy(szNameASCII, szName, sizeof(szNameASCII));
#endif
    
    memset(psaAddr, 0, sizeof(psaAddr));
    psaAddr->sin6_family = AF_INET6;
    
    memset(&AddrHints, 0, sizeof(AddrHints));
    AddrHints.ai_family = AF_INET6;
    
    if(getaddrinfo(szNameASCII, "100", &AddrHints, &pAddrInfo))
    {
        RETAILMSG(1,
            (TEXT("Error getting host information for %hs\r\n"), szNameASCII));

        freeaddrinfo(pAddrInfo);
        return FALSE;
    }
    else
    {
        memcpy(&psaAddr->sin6_addr.s6_addr, &(((SOCKADDR_IN6 *)(pAddrInfo->ai_addr))->sin6_addr.s6_addr),
            sizeof(struct in_addr6));

        PrintIPv6Addr(psaAddr);
    }
    
    freeaddrinfo(pAddrInfo);
    
    return TRUE;
}

void PrintIPv6Addr(SOCKADDR_IN6 *psaAddr)
{
    RETAILMSG(1,
        (TEXT("   0x%04x%04x%04x%04x%04x%04x%04x%04x\r\n"), 
        htons((*(WORD *)&(psaAddr->sin6_addr.s6_addr[0]))), htons((*(WORD *)&(psaAddr->sin6_addr.s6_addr[2]))),
        htons((*(WORD *)&(psaAddr->sin6_addr.s6_addr[4]))), htons((*(WORD *)&(psaAddr->sin6_addr.s6_addr[6]))),
        htons((*(WORD *)&(psaAddr->sin6_addr.s6_addr[8]))), htons((*(WORD *)&(psaAddr->sin6_addr.s6_addr[10]))),
        htons((*(WORD *)&(psaAddr->sin6_addr.s6_addr[12]))), htons((*(WORD *)&(psaAddr->sin6_addr.s6_addr[14]))) ));
}
