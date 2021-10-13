//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      soapassert.cpp
//
// Contents:
//
//      assert code inside soap toolkit
//
//----------------------------------------------------------------------------------

#include "headers.h"

#ifdef _DEBUG

#include "stackwalk.h"

#ifdef UNDER_CE
    #include "WinCEUtils.h"
#endif


static CSpinlock     assertlock(x_AssertFile);

#if !defined(_WIN64) && !defined(UNDER_CE)

static WCHAR * GetCallStack(void)
{
    try
    {
        StackWalker resolver(GetCurrentProcess());
        Symbol* symbol = NULL;
    
        CONTEXT ctx;
        memset(&ctx, 0, sizeof ctx);
    
        ctx.ContextFlags = CONTEXT_CONTROL;
        GetThreadContext(GetCurrentThread(), &ctx);
    
        symbol = resolver.CreateStackTrace(&ctx);
        int nLen = resolver.GetCallStackSize(symbol);

        CAutoRg<WCHAR>    szStack = new WCHAR[nLen];
        resolver.GetCallStack(symbol, nLen, szStack);
    
        return szStack.PvReturn();
    }
    catch(...)
    {
        return NULL;
    }
}
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: int __stdcall AssertFailure(const char *szFilename, unsigned int Line, const char *sz)
//
//  parameters:
//
//  description:
//        Assert reporting
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
int __stdcall AssertFailure(const char *szFilename, unsigned int Line, const char *sz)
{
    //        get last error before another system call
    DWORD        dw            = GetLastError();

#ifndef UNDER_CE
    static const char acAssertCaption[]    = "Assertion Failure";
#else
    static const WCHAR acAssertCaption[]    = L"Assertion Failure";
#endif

    static const char acAssertFileEnv[] = "SOAP_ASSERTFILE";
    static const WCHAR szNoStackTrace[] = L"*** not available ***";

    const int iBufferSize = 512;

    char acMessageBuffer[iBufferSize+1];
    
#ifndef UNDER_CE
    char szTime[16];
    char szDate[16];
#endif
    
    CLocalSpinlock     m_lock(&assertlock);

    #if !defined(_WIN64) && !defined(UNDER_CE)
        CAutoRg<WCHAR>    _wszStack     = GetCallStack();
        WCHAR *         wszStack    = _wszStack;
        
        if (wszStack == NULL)
            {
            wszStack = (WCHAR *) szNoStackTrace;
            }
                
        int iMaxLen = wcslen(wszStack) +strlen(sz)+ iBufferSize;;
    #else
        WCHAR *         wszStack    = (WCHAR *) szNoStackTrace;
        int iMaxLen = wcslen(wszStack) +strlen(sz)+ iBufferSize;;
    #endif


    CAutoRg<char>    _pszMessage(NULL);
    char * pszMessage;
    
#ifndef UNDER_CE 
    try
        {
        _pszMessage = new char[iMaxLen+1];
        pszMessage  = _pszMessage;
        }
    catch(...)
        {
        pszMessage = acMessageBuffer;
        iMaxLen = iBufferSize;
        }
#else
    _pszMessage = new char[iMaxLen + 1];
    if(!_pszMessage)
    {
        pszMessage = acMessageBuffer;
        iMaxLen = iBufferSize;
    }
    else
    {   
        pszMessage  = _pszMessage;
    }
#endif


    *pszMessage = '\0';
    //"Assert: Rel. 0.0.0.0, <date> <time>
    //                       File: <name> Line: <number> Lasterr.: <number>
    //                       Thread: <id> Proc: <id> Stack: <text>
#ifndef UNDER_CE
    _snprintf(pszMessage,iMaxLen,
        "Assert: Rel. %s, %s %s\r\nFile: %s\tLine: %u\r\nLasterr: %u\r\nMessage:%s\r\nThread:0x%08X\tProcessId:0x%08X\r\nCallstack:\r\n%S\r\n\0",
        VER_PRODUCTVERSION_STR,
        _strdate(szDate),
        _strtime(szTime),
        szFilename,
        Line,
        dw,
        sz,
        GetCurrentThreadId(),
        GetCurrentProcessId(),
        wszStack);
#else
    _snprintf(pszMessage,iMaxLen,
        "Assert: Rel. %s\r\nFile: %s\tLine: %u\r\nLasterr: %u\r\nMessage:%s\r\nThread:0x%08X\tProcessId:0x%08X\r\nCallstack:\r\n%S\r\n\0",
        VER_PRODUCTVERSION_STR,
        szFilename,
        Line,
        dw,
        sz,
        GetCurrentThreadId(),
        GetCurrentProcessId(),
        wszStack);
#endif



    char     acFileName[iBufferSize+1];
    DWORD    dwReturn;

    dwReturn = GetEnvironmentVariableA(acAssertFileEnv, acFileName, iBufferSize);
    if ((dwReturn > 0) && (dwReturn <= iBufferSize))
    {
        HANDLE m_handle = CreateFileA(
                    acFileName,                             // filename
                    GENERIC_WRITE,                            // write mode
                    FILE_SHARE_READ | FILE_SHARE_WRITE,        // other readers are ok
                    NULL,                                     // no security flags
                    OPEN_ALWAYS,                              // always create this file
                    FILE_ATTRIBUTE_NORMAL |                    // some attributes
                    FILE_FLAG_WRITE_THROUGH | FILE_FLAG_SEQUENTIAL_SCAN,
                    NULL);

        if (m_handle != INVALID_HANDLE_VALUE)
        {
            DWORD    dwWritten;

            SetFilePointer(m_handle, 0, NULL, FILE_END);
            WriteFile(m_handle, pszMessage, strlen(pszMessage), &dwWritten, 0);
            CloseHandle(m_handle);


            // and assert it worked
            if (strlen(pszMessage) == (int) dwWritten)
            {
                // we are done here
                return 0;
            }
        }
        // we got here because something in the FileIO went wrong,
        // present a hardcoded errormessage
        dwReturn = 0;

        strcpy(pszMessage, "****** Error in AssertSz Fileoutput ****");
    }

    if ((dwReturn == 0) || (dwReturn > iBufferSize))
    {
        // we have to test again, so we get into this part in the case we
        // have an error situation in the above file output.
#ifndef UNDER_CE
        int id = MessageBoxA(
                    NULL,
                    (LPSTR) pszMessage,
                    (LPSTR) acAssertCaption,
                    MB_SYSTEMMODAL | MB_ICONSTOP | MB_OKCANCEL | MB_SERVICE_NOTIFICATION
                );
        return id;
#else
	 WCHAR msg[_MAX_PATH];
	 mbstowcs(msg, pszMessage, _MAX_PATH);
	 OutputDebugString(msg);
	 /*
        int id = MessageBox(
            NULL,
            msg,
            acAssertCaption,
             MB_ICONSTOP | MB_OKCANCEL
        );*/     
     ASSERT(FALSE);
     return IDOK;
#endif
    }

    return 0;
}

#endif // _DEBUG

