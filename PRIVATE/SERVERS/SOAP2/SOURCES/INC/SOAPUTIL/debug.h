//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
//
// File:
//      Debug.h
//
// Contents:
//
//      Debugging support macros and declarations
//
//-----------------------------------------------------------------------------


#ifndef __DEBUG_H_INCLUDED__
#define __DEBUG_H_INCLUDED__


#if defined DEBUG && UNDER_CE && !DESKTOP_BUILD
  #define _DEBUG
#endif


#ifdef _DEBUG

#if !defined(_CRTDBG_MAP_ALLOC) && !defined(UNDER_CE)
#define _CRTDBG_MAP_ALLOC
#endif

#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif 



#ifndef UNDER_CE
#if defined(_CRTDBG_MAP_ALLOC) && !defined(UNDER_CE)
#define new         new(_NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define DEBUG_NEW
#endif 
#endif 



#ifdef __cplusplus
extern "C" {
#endif
int  __stdcall AssertFailure(const char *szFilename, unsigned int pLine, const char *sz);
void __stdcall DebugTrace(LPCSTR format, ...);
void __stdcall SetTraceLevel(DWORD dwLevel);
void __stdcall DebugTraceL(DWORD dwLevel, LPCSTR format, ...);
void _showNode(IXMLDOMNode *pNode);
#ifdef __cplusplus
}
#endif

#ifdef _M_IX86
#define _DebugBreak()       __asm { int 3 }
#else
#define _DebugBreak()       DebugBreak()
#endif

#define DEFAULT_TRACE_LEVEL     3

#ifndef UNDER_CE
#undef ASSERT
#undef _ASSERT
#undef _ASSERTE
#undef assert
#undef Assert
#endif

//////////////////////////////////////////////////
// Debugging support

#if !defined(UNDER_CE )|| defined(DESKTOP_BUILD)
#define ASSERT(e)           do { if(!(e) && AssertFailure(__FILE__, __LINE__, #e) == IDCANCEL) _DebugBreak(); } while(0)
#define ASSERTTEXT(e,t)     do { if(!(e) && AssertFailure(__FILE__, __LINE__, t) == IDCANCEL) _DebugBreak(); }  while(0)
#define VERIFY(e)           ASSERT(e)
#else
	#define ASSERTTEXT(e,t) ASSERT(e)
#endif

#define TRACE(x)            DebugTrace x
#define TRACEL(x)           DebugTraceL x
#define DBG_CODE(c)         (c)
#define SET_TRACE_LEVEL(l)  SetTraceLevel(l)

#define showNode(p)         _showNode(p)

#else
#if !defined(UNDER_CE )|| defined(DESKTOP_BUILD)
#define ASSERT(e)           ((void)(0))
#define VERIFY(e)           ((void)(e))
#endif
#define ASSERTTEXT(e,t)     ((void)(0))
#define TRACE(x)            ((void)(0))
#define TRACEL(x)           ((void)(0))
#define DBG_CODE(c)         ((void)(0))
#define SET_TRACE_LEVEL(l)  ((void)(0))

#define showNode(p)         ((void)(0))

#endif


#endif //__DEBUG_H_INCLUDED__
