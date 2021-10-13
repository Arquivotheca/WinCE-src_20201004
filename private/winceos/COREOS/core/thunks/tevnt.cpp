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
#define BUILDING_FS_STUBS
#include <windows.h>
#include <wmistr.h>
#include <Evntrace.h>
#include <evntprov.h>
#include <mwinbase.h>
#include <eventlogcommon.h>
#include <intsafe.h>

extern "C"
DWORD
WINAPI xxx_GetEventData(HANDLE hEvent);

volatile BYTE * g_TraceBuffer = NULL;


PVOID AllocTraceBuffer ()
{
    PVOID pTraceBuffer = (PVOID) InterlockedExchangePointer (&g_TraceBuffer, NULL);
    if (!pTraceBuffer) {
        pTraceBuffer = (PVOID)LocalAlloc(LPTR, MAX_USER_DATA_SIZE);
    }
    return pTraceBuffer;
}

void FreeTraceBuffer (PVOID pTraceBuffer)
{
    if (pTraceBuffer) {
        pTraceBuffer = (PVOID) InterlockedExchangePointer (&g_TraceBuffer, pTraceBuffer);
        if (pTraceBuffer) {
            LocalFree(pTraceBuffer);
        }
    }
}


#define EVENTLOG_PROVIDER_HANDLE_SENTINEL 0xE1E1E1E1
#define EVENTLOG_TRACE_HANDLE_SENTINEL 0xE2E2E2E2

typedef struct ProviderHandle{
    DWORD Sentinel;
    HANDLE hProvider;
    EventLogFilterMask *pReadOnlyMask;
}ProviderHandle;

typedef struct EventTraceInitialization{
    HANDLE hCallback;
    ControlCallback pCallback;
    PVOID pContext;
}EventTraceInitialization;

typedef struct TraceProviderHandle{
    DWORD Sentinel;
    HANDLE hProvider;
    HANDLE hCallback;
}TraceProviderHandle;

typedef struct TraceLoggerHandle{
    DWORD Sentinel;
    BOOL fEnabled;
    EventTraceCallbackData *pTraceData;
}TraceLoggerHandle;

typedef struct TracedMessage{
    PVOID pData;
    size_t cbData;
}TracedMessage;

EXTERN_C
ULONG xxx_ControlTraceW(
  TRACEHANDLE SessionHandle,
  LPCTSTR SessionName,
  PEVENT_TRACE_PROPERTIES Properties,
  ULONG ControlCode
)
{
    ULONG lResult = NO_ERROR;

    
    HANDLE hSession = (HANDLE)SessionHandle;
    if(!hSession)
    {
        lResult = ControlTraceByName_Trap(
                    SessionName, 
                    Properties, 
                    sizeof(EVENT_TRACE_PROPERTIES), 
                    ControlCode);
    }
    else
    {
        if(ControlCode == EVENT_TRACE_CONTROL_STOP)
        {
            CloseHandle(hSession);
            return lResult;
        }
        lResult = ControlTraceByHandle_Trap(
                    hSession, 
                    Properties, 
                    sizeof(EVENT_TRACE_PROPERTIES), 
                    ControlCode);
    }
    return lResult;
}

EXTERN_C
ULONG xxx_EnableTraceEx(
  LPCGUID ProviderId,
  LPCGUID SourceId,
  TRACEHANDLE TraceHandle,
  ULONG IsEnabled,
  UCHAR Level,
  ULONGLONG MatchAnyKeyword,
  ULONGLONG MatchAllKeyword,
  ULONG EnableProperty,
  PEVENT_FILTER_DESCRIPTOR EnableFilterDesc
  )
{
    ULONG lResult = NO_ERROR;
    lResult = EnableTraceEx_Trap(
                    ProviderId,
                    sizeof(GUID), 
                    SourceId,
                    sizeof(GUID),
                    (HANDLE)TraceHandle,
                    IsEnabled, 
                    Level, 
                    &MatchAnyKeyword, 
                    &MatchAllKeyword,
                    EnableProperty,
                    EnableFilterDesc,
                    sizeof(EVENT_FILTER_DESCRIPTOR));
    return lResult;
}

EXTERN_C
ULONG xxx_EnumerateTraceGuidsEx(
  TRACE_QUERY_INFO_CLASS TraceQueryInfoClass,
  PVOID InBuffer,
  ULONG InBufferSize,
  PVOID OutBuffer,
  ULONG OutBufferSize,
  PULONG ReturnLength
)
{
    ULONG lResult = NO_ERROR;
    lResult = EnumerateTraceGuidsEx_Trap(
                    TraceQueryInfoClass, 
                    InBuffer, 
                    InBufferSize,
                    OutBuffer,
                    OutBufferSize,
                    ReturnLength);
    return lResult;
}

EXTERN_C
ULONG xxx_StartTraceW(
  PTRACEHANDLE pSessionHandle,
  LPCTSTR SessionName,
  PEVENT_TRACE_PROPERTIES Properties
)
{
    *pSessionHandle = NULL;
    LRESULT lResult = NO_ERROR;
    HANDLE hSessionHandle = NULL;
    DWORD cbFilterMask = 0;
    EventLogFilterMask *pFilterMask = (EventLogFilterMask *)&cbFilterMask;
    lResult = StartTraceW_Trap(
                &hSessionHandle, 
                SessionName,
                Properties,
                Properties->Wnode.BufferSize, 
                &pFilterMask);
    if(NO_ERROR == lResult)
    {
        *pSessionHandle = (TRACEHANDLE)hSessionHandle;
    }
    return lResult;
}



EXTERN_C
BOOL xxx_EventProviderEnabled(
  REGHANDLE RegHandle,
  UCHAR Level,
  ULONGLONG Keyword
)
{
    // check in-proc shared heap
    ProviderHandle *pProvider = (ProviderHandle *)RegHandle;
    BOOL fMatchAll = FALSE;
    BOOL fMatchAny = FALSE;
    if((!pProvider) || (pProvider->Sentinel != EVENTLOG_PROVIDER_HANDLE_SENTINEL) || (pProvider->hProvider == INVALID_HANDLE_VALUE) )
    {
        DEBUGCHK(pProvider->Sentinel == EVENTLOG_PROVIDER_HANDLE_SENTINEL);
        return FALSE;
    }

    if(!pProvider->pReadOnlyMask)
    {
        // It's possible that we were unable to create a shared read-only heap.
        // In this case, let all the events through so that the actual masks in 
        // the kernel can determine whether to use the event or not.
        return TRUE;
    }

    if(pProvider->pReadOnlyMask->Level >= Level)
    {
        if(((pProvider->pReadOnlyMask->ullMatchAllKeywords)&Keyword) == pProvider->pReadOnlyMask->ullMatchAllKeywords)
        {
            fMatchAll = TRUE;
        }
        
        if(0xFFFFFFFFFFFFFFFF == pProvider->pReadOnlyMask->ullMatchAnyKeywords)
        {
            fMatchAny = TRUE;
        }
        else
        {
            if((pProvider->pReadOnlyMask->ullMatchAnyKeywords) & Keyword)
            {
                fMatchAny = TRUE;
            }
        }
        if(fMatchAny&fMatchAll)
        {
            return TRUE;
        }
    }
   
    return FALSE;
}


EXTERN_C
BOOL xxx_EventEnabled(
  REGHANDLE RegHandle,
  PCEVENT_DESCRIPTOR pEventDescriptor
)
{
    if(pEventDescriptor)
    {
        return xxx_EventProviderEnabled(
                    RegHandle, 
                    pEventDescriptor->Level, 
                    pEventDescriptor->Keyword);
    }
   
    return FALSE;
}


EXTERN_C
ULONG xxx_EventRegister(
  LPCGUID ProviderId,
  LPVOID Reserved,
  PVOID CallbackContext,
  PREGHANDLE RegisteredHandle
)
{
    *RegisteredHandle = NULL;
    LRESULT lResult;
    ProviderHandle *pProvider = (ProviderHandle *)LocalAlloc(LPTR, sizeof(ProviderHandle));
    if(!pProvider)
    {
        lResult = GetLastError();
        return lResult;
    }
    pProvider->pReadOnlyMask = NULL;
    pProvider->hProvider = NULL;
    pProvider->Sentinel = EVENTLOG_PROVIDER_HANDLE_SENTINEL;

    lResult = EventRegister_Trap(
                            ProviderId, 
                            sizeof(GUID),
                            NULL,
                            0,
                            NULL,
                            0, 
                            &pProvider->hProvider,
                            &pProvider->pReadOnlyMask);
    if(NO_ERROR == lResult)
    {
        __try {
            *RegisteredHandle = (ULONGLONG)pProvider;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            LocalFree(pProvider);
            return ERROR_INVALID_PARAMETER;
        }
    }
    else
    {
        LocalFree(pProvider);
    }
    return lResult;
}

EXTERN_C
ULONG xxx_EventUnregister(
  REGHANDLE RegisteredHandle
)
{
    LRESULT lResult = NO_ERROR;
    ProviderHandle *pProvider = (ProviderHandle *)RegisteredHandle;

    if((pProvider)&&(pProvider->Sentinel == EVENTLOG_PROVIDER_HANDLE_SENTINEL))
    {
        VERIFY(CloseHandle(pProvider->hProvider));
        pProvider->hProvider = NULL;
        pProvider->Sentinel = 0;
        LocalFree(pProvider);
    }
    else
    {
        // 
        // Double free is occurring here.  Check your logic.
        //
        DEBUGCHK(0);
        lResult = ERROR_INVALID_HANDLE_STATE;
    }
    return lResult;
}

EXTERN_C
ULONG xxx_EventWrite(
  REGHANDLE RegisteredHandle,
  PCEVENT_DESCRIPTOR EventDescriptor,
  ULONG UserDataCount,
  PEVENT_DATA_DESCRIPTOR UserData
)
{
    ProviderHandle *pProvider = NULL;
    DWORD dwUserData = 0;
    LRESULT lResult = NO_ERROR;
    if(!RegisteredHandle)
    {
        return ERROR_INVALID_PARAMETER;
    }
    if(!(xxx_EventEnabled(RegisteredHandle, EventDescriptor)))
    {
        lResult = NO_ERROR;
        goto exit;
    }

    if( UserDataCount > MAX_EVENT_DATA_DESCRIPTORS )
    {
        lResult = ERROR_INVALID_PARAMETER;
        goto exit;
    }
     if(UserDataCount > 0)
    {
        if(!UserData)
        {
            lResult = ERROR_INVALID_PARAMETER;
            goto exit;
        }
        //
        // the size of UserDataCount is the number of EVENT_DATA_DESCRIPTORs * sizeof(EVENT_DATA_DESCRIPTOR)
        // there is no guarantee that the data is inlined. 
        //
        if(FAILED(ULongMult(sizeof(EVENT_DATA_DESCRIPTOR), UserDataCount, &dwUserData)))
        {
            lResult = ERROR_INVALID_PARAMETER;
            goto exit;
        }
    }
      
    pProvider = (ProviderHandle *)RegisteredHandle;
    DEBUGCHK(pProvider->Sentinel == EVENTLOG_PROVIDER_HANDLE_SENTINEL);   
    lResult = EventWrite_Trap(
                    pProvider->hProvider,
                    EventDescriptor, 
                    sizeof(EVENT_DESCRIPTOR),
                    UserDataCount, 
                    (PEVENT_DATA_DESCRIPTOR)UserData, 
                    dwUserData);

exit: 
   
    return lResult;
}



EXTERN_C
DWORD TraceProviderCallbackThread(PVOID Context)
{
    if(!Context)
    {
        return 0;
    }
    EventTraceInitialization LocalTraceProvider;
    memcpy(&LocalTraceProvider, Context, sizeof(EventTraceInitialization));
    LocalFree(Context);
    Context = NULL;
    EventTraceCallbackData *pData = NULL;
    WNODE_HEADER LocalWNode;
    DWORD BufferSize;
    TraceLoggerHandle *pController = NULL;
    
    while(WAIT_OBJECT_0 == WaitForSingleObject(LocalTraceProvider.hCallback, INFINITE))
    {
        BufferSize = sizeof(WNODE_HEADER);
        if(!pData)
        {
            pData = (EventTraceCallbackData *)xxx_GetEventData(LocalTraceProvider.hCallback);
            if(!pData)
            {
                //
                // It's possible that a callback occurred before the event data was setup.  The 
                // provider will get a callback once initialization is complete, so this can be 
                // safely ignored.
                //
                continue;
            }
        }
        if(pData->fCloseThread)
        {
            //
            // The provider is shutting down.
            //
            break;
        }
        if(!pController)
        {
            pController = (TraceLoggerHandle *)LocalAlloc(LPTR, sizeof(TraceLoggerHandle));
            if(NULL == pController)
            {
                //
                // This callback will not make it through because there isn't enough memory to 
                // allocate the required handle.  Don't bail on the thread though, because the
                // next callback could have enough memory. 
                //
                DEBUGCHK(0);
                continue;
            }
        }
        memcpy(&LocalWNode, &pData->wNode, sizeof(WNODE_HEADER));
        if(pData->EnableFlags == WMI_DISABLE_EVENTS)
        {
            if(pController->fEnabled == FALSE)
            {
                //
                // This provider has already been called back as not active.  Duplicate
                // calls are unnecessary.
                //
                continue;
            }
            pController->fEnabled = FALSE;
        }
        else if(pData->EnableFlags == WMI_ENABLE_EVENTS)
        {
            if(pData->TraceHandle)
            {
                pController->fEnabled = TRUE;
            }
            else
            {
                //
                // The provider can't be called back as enabled if the tracehandle isn't
                // valid. 
                //
                continue;
            }
        } 
        else
        {
            //
            // There's bad data in the shared heap. Ignore this call. 
            //
            DEBUGCHK(0);
            continue;
        }
        pController->Sentinel = EVENTLOG_TRACE_HANDLE_SENTINEL;
        pController->pTraceData = pData;
        LocalWNode.HistoricalContext = (ULONG64)pController;
        if(NO_ERROR != LocalTraceProvider.pCallback(
                                            (WMIDPREQUESTCODE)pData->EnableFlags, 
                                            LocalTraceProvider.pContext, 
                                            &BufferSize, 
                                            (PVOID)&LocalWNode))
        {
            DEBUGCHK(0);
            //
            // For some reason the callback failed.  As this is all autogenerated code, this is probably
            // a bug in event log. 
            //
        }
    }

    VERIFY(CloseHandle(LocalTraceProvider.hCallback));
    
    LocalWNode.HistoricalContext = NULL;
    if((pController)
        && (pController->fEnabled == TRUE))
    {
        pController->fEnabled = FALSE;
        LocalTraceProvider.pCallback(
                                    WMI_DISABLE_EVENTS, 
                                    LocalTraceProvider.pContext, 
                                    &BufferSize, 
                                    (PVOID)&LocalWNode);
    }
    
    if(pController)
    {
        //
        // Any tracehandles given out will have a referrence to this.  The
        // tracehandles are never closed, so closing this here is the best way
        // to guarantee that memory isn't leaked.
        //
        LocalFree(pController);
    }
    
   
    return 1;
}



EXTERN_C
ULONG xxx_RegisterTraceGuidsW(
    WMIDPREQUEST  RequestAddress,
    PVOID     RequestContext,
    LPCGUID       ControlGuid,
    ULONG         GuidCount,
    PTRACE_GUID_REGISTRATION TraceGuidReg,
    LPCWSTR   MofImagePath,
    LPCWSTR   MofResourceName,
    PTRACEHANDLE RegistrationHandle
    )
{
    LRESULT lResult = NO_ERROR;
    HANDLE hEventHandle = NULL;
    DWORD cbGuidCount;
    HANDLE hThread = NULL;
    HANDLE hCallback = NULL;
    
    EventTraceInitialization *pTraceInit;
    if(FAILED(ULongMult(sizeof(TRACE_GUID_REGISTRATION), GuidCount, &cbGuidCount)))
    {
        return ERROR_ARITHMETIC_OVERFLOW;
    }
    if((!RequestAddress)
        ||(!RequestContext))
    {
        return ERROR_INVALID_PARAMETER;
    }
    pTraceInit = (EventTraceInitialization *)LocalAlloc(LPTR, sizeof(EventTraceInitialization));
    if(!pTraceInit)
    {
        return GetLastError();
    }
    TraceProviderHandle *pProvider = (TraceProviderHandle *)LocalAlloc(LPTR, sizeof(TraceProviderHandle));
    if(NULL == pProvider)
    {
        lResult = GetLastError();
        goto exit;
    }
    hCallback = CreateEvent(
                NULL, 
                FALSE, 
                FALSE, 
                NULL);
    
    if(NULL == hCallback)
    {
        lResult = GetLastError();
        goto exit;
    }
   
    pTraceInit->hCallback = hCallback;
    pTraceInit->pCallback = (ControlCallback)RequestAddress;
    pTraceInit->pContext = RequestContext;
    hThread = CreateThread(
                            NULL, 
                            0, 
                            TraceProviderCallbackThread, 
                            pTraceInit, 
                            0, 
                            NULL);
    if(!hThread)
    {
        lResult = GetLastError();
        goto exit;
    }
    //
    // The thread will free pTraceInit.
    //
    pTraceInit = NULL;
    __try {
        //
        // Need to try/except around this call, otherwise the event and thread
        // could be leaked.
        //
        lResult = RegisterTraceGuidsW_Trap(
                    RequestAddress, 
                    RequestContext,
                    ControlGuid,
                    sizeof(GUID),
                    TraceGuidReg,
                    cbGuidCount,
                    MofImagePath,
                    MofResourceName,
                    &hEventHandle, 
                    (PVOID)hCallback,
                    hThread);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        lResult = ERROR_INVALID_PARAMETER;
        DEBUGCHK(0);
        goto exit;
    }
    
    

exit: 
    if(NO_ERROR == lResult)
    {
        pProvider->Sentinel = EVENTLOG_PROVIDER_HANDLE_SENTINEL;
        pProvider->hProvider = hEventHandle;
        pProvider->hCallback = hCallback;
        *RegistrationHandle = (TRACEHANDLE)pProvider;
    }
    else
    {
        if(pProvider)
        {
            LocalFree(pProvider);
        }
        if(pTraceInit)
        {
            LocalFree(pTraceInit);
        }
        if(hCallback)
        {
            //
            // Closing this handle will also kill the thread.
            //
            VERIFY(CloseHandle(hCallback));
        }
    }
    if(hThread)
    {
        VERIFY(CloseHandle(hThread));
    }
    return lResult;
}

EXTERN_C
ULONG xxx_UnregisterTraceGuids(
    TRACEHANDLE RegistrationHandle
    )
{    
    TraceProviderHandle *pProvider = (TraceProviderHandle *)RegistrationHandle;

    if((!pProvider) 
        || (pProvider->Sentinel != EVENTLOG_PROVIDER_HANDLE_SENTINEL))
    {
        return ERROR_INVALID_HANDLE_STATE;
    }
    VERIFY(CloseHandle(pProvider->hProvider));
    pProvider->Sentinel = 0;
    LocalFree(pProvider);
    return NO_ERROR;
}

EXTERN_C
TRACEHANDLE xxx_GetTraceLoggerHandle(
    PVOID Buffer
    )
{
    // Pointer to a WNODE_HEADER structure. 
    //ETW passes this structure to the provider's ControlCallback function in the Buffer parameter.
    //The HistoricalContext member of WNODE_HEADER contains the session's handle.
    SetLastError(NO_ERROR);
    if(!Buffer)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    TraceLoggerHandle *pControllerHandle = (TraceLoggerHandle *)((WNODE_HEADER *)Buffer)->HistoricalContext;
    if(pControllerHandle->Sentinel != EVENTLOG_TRACE_HANDLE_SENTINEL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    if(!pControllerHandle->fEnabled)
    {
        return NULL;
    }
    return (TRACEHANDLE) pControllerHandle;
}

EXTERN_C
UCHAR xxx_GetTraceEnableLevel(
    TRACEHANDLE TraceHandle
    )
{
    // check in proc mask
    SetLastError(NO_ERROR);
    if(!TraceHandle)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0x0;
    }
    TraceLoggerHandle *pController = (TraceLoggerHandle *)TraceHandle;
    if(pController->Sentinel != EVENTLOG_TRACE_HANDLE_SENTINEL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0x0;
    }
    if(!pController->fEnabled)
    {
        return 0x0;
    }
    return (UCHAR)pController->pTraceData->Mask.Level;
}

EXTERN_C
ULONG xxx_GetTraceEnableFlags(
    TRACEHANDLE TraceHandle
    )
{
    // always mask to 0xffffffff
    SetLastError(NO_ERROR);
    if(!TraceHandle)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0x0;
    }
    TraceLoggerHandle *pController = (TraceLoggerHandle *)TraceHandle;
    if(pController->Sentinel != EVENTLOG_TRACE_HANDLE_SENTINEL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0x0;
    }
    if(!pController->fEnabled)
    {
        return 0x0;
    }
    return (DWORD)pController->pTraceData->Mask.ullMatchAllKeywords;
}




EXTERN_C
ULONG xxx_TraceMessageVa(
    TRACEHANDLE  LoggerHandle,
    ULONG        MessageFlags,
    LPGUID       MessageGuid,
    USHORT       MessageNumber,
    va_list      MessageArgList
    )
{
    LRESULT lResult = NO_ERROR;


    TraceLoggerHandle *pTraceHandle = (TraceLoggerHandle *)LoggerHandle;
    if(!pTraceHandle || !pTraceHandle->pTraceData)
    {
        return ERROR_INVALID_PARAMETER;
    }
    BYTE *pData = NULL;
    TracedMessage CurrentMessage;
    DWORD CurrentOffset = 0;
    BYTE *pCurrentData;
    pData = (BYTE *)AllocTraceBuffer();
    if(!pData)
    {
        lResult = GetLastError();
        goto exit;
    }
    
    //
    // If this causes an exception, then a 4k buffer will be leaked.
    //
    __try
    {
        CurrentMessage = va_arg(MessageArgList, TracedMessage);
        while ((CurrentMessage.pData)&&(CurrentMessage.cbData))
        {
            
            pCurrentData = pData + CurrentOffset;
            CurrentOffset += CurrentMessage.cbData;
            if(CurrentOffset > MAX_USER_DATA_SIZE)
            {
                lResult = ERROR_INVALID_PARAMETER;
                __leave;
            }
            
            memcpy(pCurrentData, CurrentMessage.pData, CurrentMessage.cbData);

            CurrentMessage = va_arg(MessageArgList, TracedMessage);
        }
   
        lResult = TraceMessage_Trap(
                    pTraceHandle->pTraceData->TraceHandle,
                    MessageFlags, 
                    MessageGuid, 
                    sizeof(GUID), 
                    MessageNumber, 
                    pData, 
                    CurrentOffset);
    }
    __finally
    {
        if(pData)
        {
            FreeTraceBuffer(pData);
        }
    }

exit:
   
    return lResult;
}


EXTERN_C
ULONG __cdecl xxx_TraceMessage(
    TRACEHANDLE  LoggerHandle,
    ULONG        MessageFlags,
    LPGUID       MessageGuid,
    USHORT       MessageNumber,
    ... 
    )
{
    LRESULT lResult = NO_ERROR;
    va_list MessageArgList;

    TraceLoggerHandle *pTraceHandle = (TraceLoggerHandle *)LoggerHandle;
    if((NULL == pTraceHandle)
        || (pTraceHandle->Sentinel != EVENTLOG_TRACE_HANDLE_SENTINEL)
        || (!pTraceHandle->pTraceData))
    {
        return ERROR_INVALID_PARAMETER;
    }
    if(!pTraceHandle->fEnabled)
    {
        //
        // The trace was likely deactivated while the caller was processing stuff.
        //
        return NO_ERROR;
    }
   


    va_start(MessageArgList, MessageNumber);
    lResult = xxx_TraceMessageVa(
                    LoggerHandle, 
                    MessageFlags, 
                    MessageGuid, 
                    MessageNumber, 
                    MessageArgList);  
   

    va_end(MessageArgList);
    
  
    
    return lResult;
}



EXTERN_C
HANDLE xxx_CeEventSubscribe(
        LPCWSTR ChannelPath,
        DWORD SubscribeFlags,
        DWORD Level,
        ULONGLONG MatchAnyKeywordMask,
        ULONGLONG MatchAllKeywordMask
        )
{
    HANDLE hSubscriber = NULL;
    HANDLE hInternalHandle = NULL;
    hSubscriber= CeEventSubscribe_Trap(
                        ChannelPath, 
                        SubscribeFlags, 
                        Level, 
                        &MatchAnyKeywordMask, 
                        &MatchAllKeywordMask,
                        (DWORD *)&hInternalHandle);
    if(hSubscriber)
    {
        EventSetData(hSubscriber, (DWORD)hInternalHandle);
    }
    return hSubscriber;
}

EXTERN_C
ULONG xxx_CeEventNext(
  HANDLE hSubscription
)
{
    LRESULT lResult = NO_ERROR;
    HANDLE  hSubscriber = (HANDLE)EventGetData(hSubscription);
    
    lResult = CeEventNext_Trap(hSubscriber);
    return lResult;
}

EXTERN_C
ULONG xxx_CeEventRead(
    HANDLE hSubscription,
    PEVENT_DESCRIPTOR pEventDescriptor,
    PEVENT_DATA_DESCRIPTOR pUserData,
    ULONG UserDataCount,
    ULONG* pUserDataBytes,
    ULONG* pUserDataCount
    )
{

    LRESULT lResult = NO_ERROR;
    HANDLE  hSubscriber = (HANDLE)EventGetData(hSubscription);
    lResult = CeEventRead_Trap( 
                    hSubscriber, 
                    pEventDescriptor, 
                    sizeof(EVENT_DESCRIPTOR),
                    pUserData,
                    UserDataCount,
                    pUserDataBytes, 
                    pUserDataCount);


 
    return lResult;
}

EXTERN_C
ULONG xxx_CeEventClose(
    HANDLE hSubscription
    )
{
    LRESULT lResult = NO_ERROR;
    // 
    if(NULL != hSubscription)
    {
        HANDLE hEventHandle = (HANDLE)EventGetData(hSubscription);
        if(!hEventHandle)
        {
            lResult = ERROR_INVALID_PARAMETER;
        }
        else
        {
            if(hEventHandle)
            {
                CloseHandle(hEventHandle);
            }
            CloseHandle(hSubscription);
        }
    }
    else
    {
        lResult = ERROR_INVALID_PARAMETER;
    }
    return lResult;
}

