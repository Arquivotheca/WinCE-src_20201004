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
// This test reproduces the behaviour of the netlogctl application using the
// new NetlogManager interface.
//
// ----------------------------------------------------------------------------

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>
#include <netmain.h>
#include <cmnprint.h>

#include <NetlogManager_t.hpp>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// External variables for netmain:
//
WCHAR *gszMainProgName = L"NLMTest";
DWORD  gdwMainInitFlags = 0;

// ----------------------------------------------------------------------------
//
// Prints the program usage.
//
void
PrintUsage()
{
	NetLogMessage(L"++NetlogManager test - netlogctl simulator++");
    NetLogMessage(L"NLMTest load  - load netlog driver.");
    NetLogMessage(L"NLMTest start - start the loggging.");
    NetLogMessage(L"NLMTest stop  - stops the loggging.");
    NetLogMessage(L"NLMTest unload - causes networking to unload the netlog component. (may destabilize system)");
    NetLogMessage(L"NLMTest pkt_size  XX - sets maximum packet size captured.");
    NetLogMessage(L"NLMTest cap_size  XX - sets maximum  ize of half capture file.");
    NetLogMessage(L"NLMTest file  XXX - sets the name of the file to log.");
    NetLogMessage(L"NLMTest usb  XXX - 1 => log usb , 0 => stop logging usb.");
    NetLogMessage(L"NLMTest state - print state.");
#ifdef REMOVED_FOR_YAMAZAKI
    NetLogMessage(L"NLMTest trace - print trace message state for all modules.");
    NetLogMessage(L"NLMTest trace <module> - print trace message state for specified module.");
    NetLogMessage(L"NLMTest trace <module> <filter> - set trace message state for specified module.");
#endif /* REMOVED_FOR_YAMAZAKI */
}

#ifdef REMOVED_FOR_YAMAZAKI
// ----------------------------------------------------------------------------
//
// Print out the current filter settings for the specified module,
// or for all modules if szModuleId is NULL.
//
void
PrintTraceModuleInfo(
    NetlogManager_t *pManager,
	const WCHAR     *pModuleId)

{
	int                                   moduleId;
	struct CXLOG_GET_MODULE_INFO_RESPONSE moduleInfo;

    CmnPrint(PT_VERBOSE, L"%3s %8s %4s %4s", L" ID", L"  Name  ", L"Type", L"Mask");
	CmnPrint(PT_VERBOSE, L"%3s %8s %4s %4s", L"---", L"--------", L"----", L"----");
		
	if (NULL == pModuleId || L'\0' == pModuleId[0])
	{
		// Display list of all modules
		for (moduleId = 0 ; moduleId < 32 ; ++moduleId)
		{
			if (pManager->GetTraceModuleInfo(moduleId, &moduleInfo, 
                                                 sizeof(moduleInfo)))
			{
				CmnPrint(PT_VERBOSE, L"%3u %8hs %4u %04X", 
                         moduleId, moduleInfo.szModuleName, 
                                   moduleInfo.Type, 
                                   moduleInfo.ZoneMask);
			}
		}
	}
	else
	{
		// Display info about one module
		WCHAR *argEnd;
		moduleId = (int)wcstol(pModuleId, &argEnd, 10);
        if (0 > moduleId || NULL == argEnd || L'\0' != argEnd[0]
         || !pManager->GetTraceModuleInfo(moduleId, &moduleInfo,
                                              sizeof(moduleInfo)))  
		{
			CmnPrint(PT_FAIL, L"Unknown module \"%s\"", pModuleId);
		}
		else
		{
			CmnPrint(PT_VERBOSE, L"%3u %8hs %4u %04X", 
                     moduleId, moduleInfo.szModuleName, 
                               moduleInfo.Type, 
                               moduleInfo.ZoneMask);
			for (DWORD zoneId = 0 ; zoneId < moduleInfo.numZones ; zoneId++)
			{
				CmnPrint(PT_VERBOSE, L" %8hs %s", 
                         moduleInfo.szZoneName[zoneId], 
                        (moduleInfo.ZoneMask & (1 << zoneId))? L"ON" : L"OFF");
			}
		}
	}
}
#endif /* REMOVED_FOR_YAMAZAKI */

#ifdef REMOVED_FOR_YAMAZAKI
// ----------------------------------------------------------------------------
//
// Convert the strings to integers and issue a request
// to set the specified module filter settings.
//
void
SetTraceModuleFilter(
    NetlogManager_t *pManager,
	const WCHAR     *pModuleId,
	const WCHAR     *pFilter)
{
    WCHAR *argEnd;
    
    int moduleId = (int)wcstol(pModuleId, &argEnd, 10);
    if (0 > moduleId || NULL == argEnd || L'\0' != argEnd[0])
    {
        CmnPrint(PT_FAIL, L"Unknown module \"%s\"", pModuleId);
    }
    else
    {
    	unsigned long filter = wcstoul(pFilter, &argEnd, 16);
        if (NULL == argEnd || L'\0' != argEnd[0])
        {
            CmnPrint(PT_FAIL, L"Unknown filter \"%s\"", pFilter);
        }
        else
        if (!pManager->SetTraceModuleFilter(moduleId, filter >> 16, 
                                                      filter & 0xFFFF))
        {
    		CmnPrint(PT_FAIL, L"Failed to set module %d filter to 0x%X", 
                     moduleId, filter);
        }
    }
}
#endif /* REMOVED_FOR_YAMAZAKI */

// ----------------------------------------------------------------------------
//
void
wmain(int argc, WCHAR **argv) 
{
    NetLogInitWrapper(L"NLMTest", LOG_DETAIL, NETLOG_DEF, FALSE);
    CmnPrint_Initialize();
	CmnPrint_RegisterPrintFunctionEx(PT_LOG,     NetLogDebug,   FALSE);
 	CmnPrint_RegisterPrintFunctionEx(PT_WARN1,   NetLogWarning, FALSE);
 	CmnPrint_RegisterPrintFunctionEx(PT_WARN2,   NetLogWarning, FALSE);
	CmnPrint_RegisterPrintFunctionEx(PT_FAIL,    NetLogError,   FALSE);
	CmnPrint_RegisterPrintFunctionEx(PT_VERBOSE, NetLogMessage, FALSE);
	if (argc < 2)
	{
		PrintUsage();
		return;
	}

    NetlogManager_t manager;
    if (wcsicmp(argv[1], L"load") == 0)
    {
        if (!manager.Load())
        {
            CmnPrint(PT_FAIL, L"Netlog load failed");
        }
    }    
    else
    if (wcsicmp(argv[1], L"start") == 0)
    {
        if (!manager.Start())
        {
            CmnPrint(PT_FAIL, L"Netlog start failed");
        }
    }
    else 
    if (wcsicmp(argv[1], L"stop") == 0)
    {
        if (!manager.Stop())
        {
            CmnPrint(PT_FAIL, L"Netlog stop failed");
        }
    }
    else 
    if (wcsicmp(argv[1], L"unload") == 0)
    {
        if (!manager.Unload())
        {
            CmnPrint(PT_FAIL, L"Netlog unload failed");
        }
    }
    else 
    if (wcsicmp(argv[1], L"pkt_size") == 0)
    {
        if (argc == 3)
        {
            WCHAR *argEnd;
            long pktSize = wcstol(argv[2], &argEnd, 10);
            if (0L < pktSize && NULL != argEnd && L'\0' == argEnd[0])
            {
                if (!manager.SetMaximumPacketSize(pktSize))
                {
                    CmnPrint(PT_FAIL, L"Failed setting packet size to %ld",
                             pktSize);
                }
            }
            else
            {
                CmnPrint(PT_FAIL, L"%s arg \"%s\" must be a natural number", 
                         argv[1], argv[2]);
            }
        }
        else
        {
            CmnPrint(PT_FAIL, L"%s takes 2 arguments", argv[1]);
        }
    }
    else 
    if (wcsicmp(argv[1], L"cap_size") == 0)
    {
        if (argc == 3 )
        {
            WCHAR *argEnd;
            long capSize = wcstol(argv[2], &argEnd, 10);
            if (0L < capSize && NULL != argEnd && L'\0' == argEnd[0])
            {
                if (!manager.SetMaximumFileSize(capSize))
                {
                    CmnPrint(PT_FAIL, L"Failed setting file size to %ld",
                             capSize);
                }
            }
            else
            {
                CmnPrint(PT_FAIL, L"%s arg \"%s\" must be a natural number",
                         argv[1], argv[2]);
            }
        }
        else
        {
            CmnPrint(PT_FAIL, L"%s takes 2 arguments", argv[1]);
        }
    }
    else 
    if (wcsicmp(argv[1], L"state") == 0)
    {
        CmnPrint(PT_VERBOSE, L"State:");
        CmnPrint(PT_VERBOSE, L"LogUSB: %s", 
                 manager.IsLoggingUSBPackets()? L"true" : L"false");
        CmnPrint(PT_VERBOSE, L"Stopped: %s", 
                 manager.IsLoggingEnabled()? L"false" : L"true");
        CmnPrint(PT_VERBOSE, L"Maximum Packet size to capture: %ld", 
                 manager.GetMaximumPacketSize());
        CmnPrint(PT_VERBOSE, L"1/2 Capture Size: %ld", 
                 manager.GetMaximumFileSize());
        CmnPrint(PT_VERBOSE, L"Capture File Base Name: %s", 
                 manager.GetFileBaseName());
        CmnPrint(PT_VERBOSE, L"Capture File Index: %i", 
                 manager.GetCaptureFileIndex());
    }
    else 
    if (wcsicmp(argv[1], L"usb") == 0)
    {
        if (argc == 3 )
        {
            long usbEnabled = wcstol(argv[2], NULL, 10);
            if (!manager.SetLoggingUSBPackets(0L != usbEnabled))
            {
                CmnPrint(PT_FAIL, L"Failed %s USB packet logging",
                        ((0L != usbEnabled)? L"enabling" : L"disabling"));
            }
        }
        else
        {
            CmnPrint(PT_FAIL, L"%s takes 2 arguments", argv[1]);
        }
    }
    else 
    if (wcsicmp(argv[1], L"file") == 0)
    {
        if (argc == 3) 
        {
            if (!manager.SetFileBaseName(argv[2]))
            {
                CmnPrint(PT_FAIL, L"Failed setting file base-name t- \"%s\"", 
                         argv[2]);
            }
        }
        else
        {
            CmnPrint(PT_FAIL, L"%s takes 2 arguments", argv[1]);
        }
    }
#ifdef REMOVED_FOR_YAMAZAKI
    else 
    if (wcsicmp(argv[1], L"trace") == 0)
    {
        if (argc == 2) 
        {
			// Show trace state for all modules
			PrintTraceModuleInfo(&manager, NULL);
        }
        else if (argc == 3)
		{
			// Show trace state for specified module
			PrintTraceModuleInfo(&manager, argv[2]);
		}
		else if (argc == 4)
		{
			// Set trace state for specified module
			SetTraceModuleFilter(&manager, argv[2], argv[3]);
		}
		else
        {
            CmnPrint(PT_FAIL, L"Usage %s [<moduleId>] [<filter>]", argv[1]);
        }
    }
#endif /* REMOVED_FOR_YAMAZAKI */
    else
    {
        CmnPrint(PT_FAIL, L" Unknown command \"%s\"", argv[1]);
        PrintUsage();
    }
}

// ----------------------------------------------------------------------------
