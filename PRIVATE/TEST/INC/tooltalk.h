// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 1997-2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------
//******************************************************************************
//
// TOOLTALK.H
//
// Definition module for the ToolTalk interface.
//
//******************************************************************************

#ifndef __TOOLTALK_H__
#define __TOOLTALK_H__


//******************************************************************************
// Define functions as export when building ToolTalk, and as import when this
// file is included by all other applications.  We only use TOOLTALKAPI on C++
// classes.  For straight C functions, our DEF file will take care of exporting
// them.
//******************************************************************************

#ifndef TOOLTALKAPI
#define TOOLTALKAPI __declspec(dllimport)
#endif


//******************************************************************************
// Define EXTERN_C
//******************************************************************************

#ifndef EXTERN_C
#   ifdef __cplusplus
#      define EXTERN_C extern "C"
#   else
#      define EXTERN_C
#   endif
#endif


//******************************************************************************
// Public Constants
//******************************************************************************

#define TT_MAX_CONNECTION_LENGTH  255

#define TT_TRANSPORT_WINSOCK        1
#define TT_TRANSPORT_SERIAL         2 // not implemented
#define TT_TRANSPORT_CESH           3
#define TT_TRANSPORT_PPSH           TT_TRANSPORT_CESH   // for back. comp.


//******************************************************************************
// Public Types
//******************************************************************************

typedef HANDLE HTTPIPE;

typedef struct _TTPIPEINFOA {
   DWORD dwSize;
   DWORD dwTransport;
   DWORD dwAddress;
   CHAR  szConnection[TT_MAX_CONNECTION_LENGTH + 1];
} TTPIPEINFOA, *PTTPIPEINFOA;

typedef struct _TTPIPEINFOW {
   DWORD dwSize;
   DWORD dwTransport;
   DWORD dwAddress;
   WCHAR szConnection[TT_MAX_CONNECTION_LENGTH + 1];
} TTPIPEINFOW, *PTTPIPEINFOW;

//******************************************************************************
// Public C Interface
//******************************************************************************

EXTERN_C HTTPIPE TTConnectPipeA(DWORD dwTransport, DWORD dwChannel, LPCSTR szConnection);
EXTERN_C HTTPIPE TTConnectPipeW(DWORD dwTransport, DWORD dwChannel, LPCWSTR szConnection);
EXTERN_C HTTPIPE TTListenPipeA(DWORD dwTransport, DWORD dwChannel, LPCSTR szConnection);
EXTERN_C HTTPIPE TTListenPipeW(DWORD dwTransport, DWORD dwChannel, LPCWSTR szConnection);
EXTERN_C HTTPIPE TTAcceptPipe(HTTPIPE hTTPipe);
EXTERN_C BOOL    TTGetPipeInfoA(HTTPIPE hTTPipe, TTPIPEINFOA *pTTPipeInfo);
EXTERN_C BOOL    TTGetPipeInfoW(HTTPIPE hTTPipe, TTPIPEINFOW *pTTPipeInfo);
EXTERN_C BOOL    TTSetPipeUserData(HTTPIPE hTTPipe, DWORD dwUserData);
EXTERN_C DWORD   TTGetPipeUserData(HTTPIPE hTTPipe);
EXTERN_C BOOL    TTReadPipe(HTTPIPE hTTPipe, LPVOID lpBuffer, DWORD dwBytesToRead, LPDWORD lpBytesRead);
EXTERN_C BOOL    TTWritePipe(HTTPIPE hTTPipe, LPCVOID lpBuffer, DWORD dwBytesToWrite, LPDWORD lpBytesWritten);
EXTERN_C BOOL    TTClosePipe(HTTPIPE hTTPipe);

#ifdef UNICODE
#   define _TTPIPEINFO   _TTPIPEINFOA
#   define TTPIPEINFO    TTPIPEINFOW
#   define PTTPIPEINFO   PTTPIPEINFOW
#   define TTConnectPipe TTConnectPipeW
#   define TTListenPipe  TTListenPipeW
#   define TTGetPipeInfo TTGetPipeInfoW
#else
#   define _TTPIPEINFO   _TTPIPEINFOW
#   define TTPIPEINFO    TTPIPEINFOA
#   define PTTPIPEINFO   PTTPIPEINFOA
#   define TTConnectPipe TTConnectPipeA
#   define TTListenPipe  TTListenPipeA
#   define TTGetPipeInfo TTGetPipeInfoA
#endif

#endif // __TOOLTALK_H__
