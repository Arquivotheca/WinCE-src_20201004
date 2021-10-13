//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.\n"
//
// ----------------------------------------------------------------------------
//******************************************************************************
//
// KATO.H
//
// Definition module for the Kato constants and CKato interface
//
//******************************************************************************

#ifndef __KATO_H__
#define __KATO_H__

//******************************************************************************
// Define functions as import when building kato, and as export when this file
// is included by all other applications. We only use KATOAPI on C++ classes.
// For straight C functions, our DEF file will take care of exporting them.
//******************************************************************************

#ifndef KATOAPI
   #define KATOAPI __declspec(dllimport)
#endif

//******************************************************************************
// Define EXTERN_C
//******************************************************************************

#ifndef EXTERN_C
   #ifdef __cplusplus
      #define EXTERN_C extern "C"
   #else
      #define EXTERN_C
   #endif
#endif

//******************************************************************************
// Specify 32 bit pack size to ensure everyone creates the correct size objects
//******************************************************************************

#pragma pack(4)

//******************************************************************************
// Constants
//******************************************************************************

#define KATO_MAX_LEVEL            31
#define KATO_MAX_VERBOSITY        15
#define KATO_MAX_STRING_LENGTH  1023
#define KATO_MAX_DATA_SIZE      1024

//******************************************************************************
// Types
//******************************************************************************

typedef HANDLE HKATO;

typedef struct _KATOCALLBACKSTRUCTW {
   LPARAM  lParam;
   HKATO   hKato;
   DWORD   dwThreadID;
   DWORD   dwTickCount;
   DWORD   dwLevel;
   DWORD   dwLevelID;
   DWORD   dwVerbosity;
   LPCWSTR wszLog;
} KATOCALLBACKSTRUCTW, *LPKATOCALLBACKSTRUCTW;

typedef struct _KATOCALLBACKSTRUCTA {
   LPARAM  lParam;
   HKATO   hKato;
   DWORD   dwThreadID;
   DWORD   dwTickCount;
   DWORD   dwLevel;
   DWORD   dwLevelID;
   DWORD   dwVerbosity;
   LPCSTR  szLog;
} KATOCALLBACKSTRUCTA, *LPKATOCALLBACKSTRUCTA;

typedef BOOL (CALLBACK *LPKATOCALLBACKW)(LPKATOCALLBACKSTRUCTW);
typedef BOOL (CALLBACK *LPKATOCALLBACKA)(LPKATOCALLBACKSTRUCTA);

typedef enum _KATO_FLUSH_TYPE {
   KATO_FLUSH_ON,
   KATO_FLUSH_OFF,
   KATO_FLUSH_NOW,
} KATO_FLUSH_TYPE, *LPKATO_FLUSH_TYPE;

//******************************************************************************
// Common APIs for C and C++ interfaces
//******************************************************************************

EXTERN_C BOOL  WINAPI KatoSetServerW(LPCWSTR wszServer);
EXTERN_C BOOL  WINAPI KatoSetServerA(LPCSTR szServer);
EXTERN_C BOOL  WINAPI KatoGetServerW(LPWSTR wszServer, INT nCount);
EXTERN_C BOOL  WINAPI KatoGetServerA(LPSTR szServer, INT nCount);
EXTERN_C BOOL  WINAPI KatoSetCallbackW(LPKATOCALLBACKW lpCallbackW, LPARAM lParam);
EXTERN_C BOOL  WINAPI KatoSetCallbackA(LPKATOCALLBACKA lpCallbackA, LPARAM lParam);
EXTERN_C BOOL  WINAPI KatoFlush(KATO_FLUSH_TYPE flushType);
EXTERN_C BOOL  WINAPI KatoDebug(BOOL fODSEnabled, DWORD dwMaxLogVersbosity,
                                DWORD dwMaxCommentVersbosity, DWORD dwMaxLevel);
EXTERN_C BOOL  WINAPI KatoDebugEx(BOOL fODSEnabled, BOOL fConsoleEnabled, DWORD dwMaxLogVersbosity,
                                DWORD dwMaxCommentVersbosity, DWORD dwMaxLevel);

EXTERN_C HKATO WINAPI KatoGetDefaultObject(VOID);

//******************************************************************************
// APIs for C interface (C++ applications should use the CKato class)
//******************************************************************************

// Construction and destruction
EXTERN_C HKATO WINAPI  KatoCreateW(LPCWSTR wszName);
EXTERN_C HKATO WINAPI  KatoCreateA(LPCSTR szName);
EXTERN_C BOOL  WINAPI  KatoDestroy(HKATO hKato);

// Unicode functions
EXTERN_C INT   WINAPIV KatoBeginLevelW(HKATO hKato, DWORD dwLevelID, LPCWSTR wszFormat, ...);
EXTERN_C INT   WINAPIV KatoBeginLevelVW(HKATO hKato, DWORD dwLevelID, LPCWSTR wszFormat, va_list pArgs);
EXTERN_C INT   WINAPIV KatoEndLevelW(HKATO hKato, LPCWSTR wszFormat, ...);
EXTERN_C INT   WINAPIV KatoEndLevelVW(HKATO hKato, LPCWSTR wszFormat, va_list pArgs);
EXTERN_C BOOL  WINAPIV KatoLogW(HKATO hKato, DWORD dwVerbosity, LPCWSTR wszFormat, ...);
EXTERN_C BOOL  WINAPIV KatoLogVW(HKATO hKato, DWORD dwVerbosity, LPCWSTR wszFormat, va_list pArgs);
EXTERN_C BOOL  WINAPIV KatoCommentW(HKATO hKato, DWORD dwVerbosity, LPCWSTR wszFormat, ...);
EXTERN_C BOOL  WINAPIV KatoCommentVW(HKATO hKato, DWORD dwVerbosity, LPCWSTR wszFormat, va_list pArgs);

// ASCII functions
EXTERN_C INT   WINAPIV KatoBeginLevelA(HKATO hKato, DWORD dwLevelID, LPCSTR szFormat, ...);
EXTERN_C INT   WINAPIV KatoBeginLevelVA(HKATO hKato, DWORD dwLevelID, LPCSTR szFormat, va_list pArgs);
EXTERN_C INT   WINAPIV KatoEndLevelA(HKATO hKato, LPCSTR szFormat, ...);
EXTERN_C INT   WINAPIV KatoEndLevelVA(HKATO hKato, LPCSTR szFormat, va_list pArgs);
EXTERN_C BOOL  WINAPIV KatoLogA(HKATO hKato, DWORD dwVerbosity, LPCSTR szFormat, ...);
EXTERN_C BOOL  WINAPIV KatoLogVA(HKATO hKato, DWORD dwVerbosity, LPCSTR szFormat, va_list pArgs);
EXTERN_C BOOL  WINAPIV KatoCommentA(HKATO hKato, DWORD dwVerbosity, LPCSTR szFormat, ...);
EXTERN_C BOOL  WINAPIV KatoCommentVA(HKATO hKato, DWORD dwVerbosity, LPCSTR szFormat, va_list pArgs);

// Non-string functions
EXTERN_C BOOL  WINAPI  KatoSetItemData(HKATO hKato, DWORD dwItemData);
EXTERN_C DWORD WINAPI  KatoGetItemData(HKATO hKato);
EXTERN_C BOOL  WINAPI  KatoSendSystemData(HKATO hKato, DWORD dwSystemID, LPCVOID lpcvBuffer, DWORD dwSize);
EXTERN_C DWORD WINAPI  KatoGetCurrentLevel(HKATO hKato);
EXTERN_C INT   WINAPI  KatoGetVerbosityCount(HKATO hKato, DWORD dwVerbosity, DWORD dwLevel);
EXTERN_C BOOL  WINAPI  KatoSetXML(HKATO hKato, BOOL bXML);

//******************************************************************************
// Map function names to the correct APIs based on the UNICODE flag
//******************************************************************************

#ifdef UNICODE
   #define KATOCALLBACKSTRUCT   KATOCALLBACKSTRUCTW
   #define LPKATOCALLBACKSTRUCT LPKATOCALLBACKSTRUCTW
   #define LPKATOCALLBACK       LPKATOCALLBACKW
   #define KatoCreate           KatoCreateW
   #define KatoSetCallback      KatoSetCallbackW
   #define KatoSetServer        KatoSetServerW
   #define KatoGetServer        KatoGetServerW
   #define KatoBeginLevel       KatoBeginLevelW
   #define KatoBeginLevelV      KatoBeginLevelVW
   #define KatoEndLevel         KatoEndLevelW
   #define KatoEndLevelV        KatoEndLevelVW
   #define KatoLog              KatoLogW
   #define KatoLogV             KatoLogVW
   #define KatoComment          KatoCommentW
   #define KatoCommentV         KatoCommentVW
#else
   #define KATOCALLBACKSTRUCT   KATOCALLBACKSTRUCTA
   #define LPKATOCALLBACKSTRUCT LPKATOCALLBACKSTRUCTA
   #define LPKATOCALLBACK       LPKATOCALLBACKA
   #define KatoCreate           KatoCreateA
   #define KatoSetCallback      KatoSetCallbackA
   #define KatoSetServer        KatoSetServerA
   #define KatoGetServer        KatoGetServerA
   #define KatoBeginLevel       KatoBeginLevelA
   #define KatoBeginLevelV      KatoBeginLevelVA
   #define KatoEndLevel         KatoEndLevelA
   #define KatoEndLevelV        KatoEndLevelVA
   #define KatoLog              KatoLogA
   #define KatoLogV             KatoLogVA
   #define KatoComment          KatoCommentA
   #define KatoCommentV         KatoCommentVA
#endif

//******************************************************************************
// CKato - Interface for C++ applications
//******************************************************************************

#ifdef __cplusplus

class KATOAPI CKato {
public:
   // Overlaod new and delete to prevent mismatched heaps (KB:Q122675)
   void* __cdecl operator new(size_t stAllocate);
   void  __cdecl operator delete(void *pvMemory);

   // Construction and destruction
   CKato(LPCWSTR wszName = NULL, BOOL bXML = FALSE);
   CKato(LPCSTR szName,  BOOL bXML = FALSE);
   virtual ~CKato(VOID);

   // Unicode functions
   INT  WINAPIV BeginLevel (DWORD dwLevelID, LPCWSTR wszFormat, ...);
   INT  WINAPI  BeginLevelV(DWORD dwLevelID, LPCWSTR wszFormat, va_list pArgs);
   INT  WINAPIV EndLevel (LPCWSTR wszFormat, ...);
   INT  WINAPI  EndLevelV(LPCWSTR wszFormat, va_list pArgs);
   BOOL WINAPIV Log (DWORD dwVerbosity, LPCWSTR wszFormat, ...);
   BOOL WINAPI  LogV(DWORD dwVerbosity, LPCWSTR wszFormat, va_list pArgs);
   BOOL WINAPIV Comment (DWORD dwVerbosity, LPCWSTR wszFormat, ...);
   BOOL WINAPI  CommentV(DWORD dwVerbosity, LPCWSTR wszFormat, va_list pArgs);

   // ASCII functions
   INT  WINAPIV BeginLevel (DWORD dwLevelID, LPCSTR szFormat, ...);
   INT  WINAPI  BeginLevelV(DWORD dwLevelID, LPCSTR szFormat, va_list pArgs);
   INT  WINAPIV EndLevel (LPCSTR szFormat, ...);
   INT  WINAPI  EndLevelV(LPCSTR szFormat, va_list pArgs);
   BOOL WINAPIV Log (DWORD dwVerbosity, LPCSTR szFormat, ...);
   BOOL WINAPI  LogV(DWORD dwVerbosity, LPCSTR szFormat, va_list pArgs);
   BOOL WINAPIV Comment (DWORD dwVerbosity, LPCSTR szFormat, ...);
   BOOL WINAPI  CommentV(DWORD dwVerbosity, LPCSTR szFormat, va_list pArgs);

   // Non-string functions
   BOOL  WINAPI SetItemData(DWORD dwItemData);
   DWORD WINAPI GetItemData(VOID);
   BOOL  WINAPI SendSystemData(DWORD dwSystemID, LPCVOID lpcvBuffer, DWORD dwSize);
   DWORD WINAPI GetCurrentLevel(VOID);
   INT   WINAPI GetVerbosityCount(DWORD dwVerbosity, DWORD dwLevel = -1);
   BOOL  WINAPI	SetXML(BOOL bXML);

   // Internal data
protected:
   LPVOID	m_lpvKatoData;
   BOOL		m_bXML;
};

#endif // __cplusplus

#pragma pack() // restore packing size to previous state

#endif // __KATO_H__
