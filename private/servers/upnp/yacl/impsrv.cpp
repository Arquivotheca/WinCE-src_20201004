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
///////////////////////////////////////////////////////////////////////////////
//
// impsrv.h - Copyright 1994-2001, Don Box (http://www.donbox.com)
//
// This file contains several function implementations that
// automate implementing servers.
//
//    HINSTANCE GetThisInstance(void); // returns this HINSTANCE
//
// There are two routines for detecting the standard commandline
// arguments:
//
//    PARSE_RESULT SvcParseCommandLine(const char *); 
//    PARSE_RESULT SvcParseCommandLineV(int argc, char **argv); 
//

#ifndef _IMPSRV_CPP
#define _IMPSRV_CPP

#include <windows.h>
#include "impsrv.h"

#if 0    // don't think this works on Windows CE
HINSTANCE STDAPICALLTYPE GetThisInstance(void)
{
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(GetThisInstance, &mbi, sizeof(mbi));
    return (HINSTANCE)mbi.AllocationBase;
}
#endif

#ifdef EXESVC

PARSE_RESULT STDAPICALLTYPE SvcParseCommandLine(char *pszCmdParam)
{
    char szCmdParam[MAX_PATH];
    lstrcpyA(szCmdParam, pszCmdParam);
    CharUpperA(szCmdParam);
    PARSE_RESULT result = PARSE_NORMAL;
    if (strstr(szCmdParam, "/REGSERVER") 
        || strstr(szCmdParam, "-REGSERVER"))
    {
        result = PARSE_REGISTER_SERVER;
    }
    else if (strstr(szCmdParam, "/UNREGSERVER") 
        || strstr(szCmdParam, "-UNREGSERVER"))
    {
        result = PARSE_UNREGISTER_SERVER;
    }
    else if (strstr(szCmdParam, "/AUTOMATION") 
        || strstr(szCmdParam, "-AUTOMATION"))
    {
        result = PARSE_AUTOMATION;
    }
    else if (strstr(szCmdParam, "/EMBEDDING") 
        || strstr(szCmdParam, "-EMBEDDING"))
    {
        result = PARSE_EMBEDDING;
    }
    return result;
}

PARSE_RESULT STDAPICALLTYPE SvcParseCommandLineV(int argc, char **argv, int *pargTag)
{
    PARSE_RESULT result = PARSE_NORMAL;
    int i = 0;
    for (i = 0; i < argc && (result == PARSE_NORMAL); i++)
    {
        if (!strcmpi(argv[i], "/REGSERVER") 
            || !strcmpi(argv[i], "-REGSERVER"))
        {
            result = PARSE_REGISTER_SERVER;
        }
        else if (!strcmpi(argv[i], "/UNREGSERVER") 
            || !strcmpi(argv[i], "-UNREGSERVER"))
        {
            result = PARSE_UNREGISTER_SERVER;
        }
        else if (!strcmpi(argv[i], "/EMBEDDING") 
            || !strcmpi(argv[i], "-EMBEDDING"))
        {
            result = PARSE_EMBEDDING;
        }
        else if (!strcmpi(argv[i], "/AUTOMATION") 
            || !strcmpi(argv[i], "-AUTOMATION"))
        {
            result = PARSE_AUTOMATION;
        }
    }
    if (result != PARSE_NORMAL && pargTag)
    {
        *pargTag = i;
    }
    return result;
}
#else
#endif


#endif
