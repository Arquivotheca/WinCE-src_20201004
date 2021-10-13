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
//+----------------------------------------------------------------------------
//
//
// File:
//      Debug.cpp
//
// Contents:
//
//      Tracing and debugging support
//
//-----------------------------------------------------------------------------

#include "Headers.h"

#ifdef _DEBUG

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


static DWORD g_dwTraceLevel = DEFAULT_TRACE_LEVEL;

void __stdcall DebugTrace(LPCSTR format, ...)
{
    char buffer[1024] = { 0 };

    va_list va;
    va_start(va, format);
    ::vsprintf(buffer, format, va);
    va_end(va);

#ifndef UNDER_CE
	::OutputDebugStringA(buffer);
#else
	WCHAR buf[_MAX_PATH];
	mbstowcs(buf, buffer, _MAX_PATH);
	::OutputDebugString(buf);
#endif

}

void __stdcall SetTraceLevel(DWORD dwLevel)
{
    g_dwTraceLevel = dwLevel;
}

void __stdcall DebugTraceL(DWORD dwLevel, LPCSTR format, ...)
{
    if(dwLevel > g_dwTraceLevel)
        return;

    char buffer[1024] = { 0 };

    va_list va;
    va_start(va, format);
    ::vsprintf(buffer, format, va);
    va_end(va);

#ifndef UNDER_CE
	::OutputDebugStringA(buffer);
#else
	WCHAR buf[_MAX_PATH];
	mbstowcs(buf, buffer, _MAX_PATH);
	::OutputDebugString(buf);
#endif

}


void _showNode(IXMLDOMNode *pNode)
{
    HRESULT hr = S_OK;
#ifndef UNDER_CE
    DOMNodeType dnt;
#endif

    CAutoBSTR   bstr;
    CVariant cv;

    static const WCHAR * pant[] = { L"NODE_INVALID", L"NODE_ELEMENT", L"NODE_ATTRIBUTE",  L"NODE_TEXT", 
                                    L"NODE_CDATA_SECTION",  L"NODE_ENTITY_REFERENCE",   L"NODE_ENTITY",
                                    L"NODE_PROCESSING_INSTRUCTION", L"NODE_COMMENT",
                                    L"NODE_DOCUMENT", L"NODE_DOCUMENT_TYPE", L"NODE_DOCUMENT_FRAGMENT"
                                    L"NODE_NOTATION" };

    if (!pNode)
    {
        TRACE(("NULL node passed to showNode"));
        return;
    }
        

    CHK( pNode->get_nodeTypeString(&bstr));
    TRACE ( ("Node type  :%S", bstr) );
    bstr.Clear();
    CHK( pNode->get_namespaceURI(&bstr));
    TRACE ( ("   NS:%S", bstr) );
    bstr.Clear();
    CHK( pNode->get_prefix(&bstr));
    TRACE ( ("   Prefix:%S", bstr) );
    bstr.Clear();
    CHK( pNode->get_baseName(&bstr));
    TRACE ( ("   baseName:%S\n", bstr) );
    bstr.Clear();

    
    CHK( pNode->get_nodeValue(&cv));
    TRACE ( ("     value :%.160S\n", V_BSTR(&cv)) );
    ::VariantClear(&cv);


    CHK( pNode->get_text(&bstr));
    TRACE ( ("     text  :%.160S\n", bstr) );
    bstr.Clear();

    CHK( pNode->get_xml(&bstr));
    TRACE ( ("     xml   :%.160S\n", bstr) );
    bstr.Clear();

    

    TRACE ( ("\n") );
Cleanup:
    return;
}

#endif
