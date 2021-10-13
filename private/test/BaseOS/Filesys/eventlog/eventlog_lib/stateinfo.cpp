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
#define EVENTLOG_TEST

#include <windows.h>
#include <evntprov.h>
#include <evntcons.h>
#include <evntrace.h>
#include <eventlogcommon.h>
#include <eventlogsubscriber.h>
#include "StateInfo.h"



HRESULT PrintChannelInformation(EventLogChannel *pChannel)
{
     // NOTE: ChannelName, and other pointers are NOT initialized (m_p*).
    NKDbgPrintfW(L"Channel Information: ");
    NKDbgPrintfW(L"Id: %u", pChannel->m_ChannelID);
    NKDbgPrintfW(L"Type: %u", pChannel->m_ChannelType);
    NKDbgPrintfW(L"ClockType: %u", pChannel->m_ClockType);
    NKDbgPrintfW(L"Enabled: %u", pChannel->m_fEnabled);
    NKDbgPrintfW(L"Default Level: %u", pChannel->m_RegistryFilterMask.Level);
    NKDbgPrintfW(L"Default KeywordAll: %x", pChannel->m_RegistryFilterMask.ullMatchAllKeywords);
    NKDbgPrintfW(L"Default KeywordAny: %x", pChannel->m_RegistryFilterMask.ullMatchAnyKeywords);
    NKDbgPrintfW(L"Number of Subscribers - this included this TEST subscriber: %u", pChannel->m_SubscriberReferences);
    NKDbgPrintfW(L"Merged Level: %u", pChannel->m_MergedFilterMask.Level);
    NKDbgPrintfW(L"Merged KeywordAll: %x", pChannel->m_MergedFilterMask.ullMatchAllKeywords);
    NKDbgPrintfW(L"Merged KeywordAny: %x", pChannel->m_MergedFilterMask.ullMatchAnyKeywords);
    NKDbgPrintfW(L"Number of Providers: %u", pChannel->m_ProviderReferences);
    return S_OK;
}


HRESULT PrintCircularBufferInformation(CircularEventBuffer *pEventBuffer)
{
    NKDbgPrintfW(L"CircularBuffer Information: ");
    NKDbgPrintfW(L"BufferSize: %08x", pEventBuffer->m_BufferSize);
    NKDbgPrintfW(L"Enabled: %u", pEventBuffer->m_fEnabled);
    //NKDbgPrintfW(L"Default Level: %u", pEventBuffer->m_FilterMask.Level);
    //NKDbgPrintfW(L"Default KeywordAll: %x", pEventBuffer->m_FilterMask.ullMatchAllKeywords);
    //NKDbgPrintfW(L"Default KeywordAny: %x", pEventBuffer->m_FilterMask.ullMatchAnyKeywords);
    NKDbgPrintfW(L"Write pointer offset: %08x", pEventBuffer->m_pWritePointer);
    NKDbgPrintfW(L"Oldest valid event offset: %08x", pEventBuffer->m_pOldestValidEvent);
    NKDbgPrintfW(L"Last event written offset: %08x", pEventBuffer->m_pLastWrittenEvent);
    return S_OK;
}


HRESULT PrintEventInformation(CircularEventBuffer *pEventBuffer, BYTE *pBuffer)
{

 // some conversion must take place because the pointers into the byte * buffer are now offsets, rather
    // than pointers. 
    pEventBuffer->m_pBuffer = (CircularEventData*)pBuffer;
    pEventBuffer->m_pLastWrittenEvent = pEventBuffer->m_pLastWrittenEvent ? (CircularEventData*)(pBuffer + (BYTE)pEventBuffer->m_pLastWrittenEvent) : NULL;
    pEventBuffer->m_pOldestValidEvent = pEventBuffer->m_pOldestValidEvent ? (CircularEventData*)(pBuffer + (BYTE)pEventBuffer->m_pOldestValidEvent) : NULL;
    pEventBuffer->m_pWritePointer = (pBuffer + (BYTE)pEventBuffer->m_pWritePointer);

    // we can spit out the events as much as they make sense
    PublishedEvent * pCurrentEvent = (PublishedEvent *)pEventBuffer->m_pOldestValidEvent;
    BOOL fBufferFilled = TRUE;
    if(!pCurrentEvent)
    {
        pCurrentEvent = (PublishedEvent *)pEventBuffer->m_pBuffer;
        fBufferFilled = FALSE;
    }
    NKDbgPrintfW(L"Events in the Buffer: ");
    while((pCurrentEvent != (PublishedEvent *)pEventBuffer->m_pWritePointer)&&(fBufferFilled))
    {
        fBufferFilled = FALSE;
        
        NKDbgPrintfW(L"Information for Event Id: %u", pCurrentEvent->EventDescriptor.Id);
        NKDbgPrintfW(L"Channel: %u", pCurrentEvent->EventDescriptor.Channel);
        NKDbgPrintfW(L"Level: %u", pCurrentEvent->EventDescriptor.Level);
        NKDbgPrintfW(L"Keyword: %x", pCurrentEvent->EventDescriptor.Keyword);
        NKDbgPrintfW(L"Version: %u", pCurrentEvent->EventDescriptor.Version);
        NKDbgPrintfW(L"Number of data given to event: %u", pCurrentEvent->UserDataCount);
        // You COULD print out the data here with another for loop, as all the data pointers 
        // have been converted to offsets.  There will be X EventDataDescriptors and they'll 
        // contain the offset from the start of pCurrentEvent->dwUserDataOffset.  See 
        // developr/arianeja/scratch/eventlogplugin/eventlogplugin.cpp for details.

        
        // The update to the next event is complicated because we can't simply use the pointer that is
        // provided in the event, unless I went through the events before I copied over the buffer, 
        // which is somewhat cumbersome.  If this becomes a problem, then I will make all the pointers
        // offsets. 
        pCurrentEvent = (PublishedEvent *)(((BYTE *)pCurrentEvent) + pCurrentEvent->cbEventData);
        if((BYTE)pCurrentEvent % sizeof(DWORD))
        {
            pCurrentEvent = (PublishedEvent *)((BYTE *)pCurrentEvent + (sizeof(DWORD) - ((BYTE)pCurrentEvent)%sizeof(DWORD)));
        }
        if(pCurrentEvent > pEventBuffer->m_pBuffer + pEventBuffer->m_BufferSize)
        {
            pCurrentEvent = (PublishedEvent *)pEventBuffer->m_pBuffer;
        }
    }

    return S_OK;
}


HRESULT getChannelInfo(WCHAR * szChannelName, EventLogChannel *pChannel, CircularEventBuffer *pEventBuffer)
{
    // Get the channel's current information. 
    HKEY hChannelKey = NULL;
    HANDLE hTestHandle = NULL;
    DWORD hr = ERROR_SUCCESS;

    if(!g_bTestHooksEnabled)
    {
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }
    
    if(!pChannel)
    {
        hr = ERROR_INVALID_DATA;
        NKDbgPrintfW(L"pChannel param was not initialized");
        goto exit;
    }
    
    if(!pEventBuffer)
    {
        hr = ERROR_INVALID_DATA;
        NKDbgPrintfW(L"pEventBuffer param was not initialized");
        goto exit;
    }
    DWORD dwLevel = 0xFFFFFFFF;
    ULONGLONG KeywordMatchAll = 0xFFFFFFFFFFFFFFFF;
    ULONGLONG KeywordMatchAny = 0xFFFFFFFFFFFFFFFF;   
    LRESULT lResult = NO_ERROR;
    hTestHandle = CeEventSubscribe(
                                szChannelName,
                                NULL, 
                                dwLevel, 
                                KeywordMatchAny, 
                                KeywordMatchAll);
    lResult = GetLastError();
    if(ERROR_TOO_MANY_NAMES != lResult)
    {
        hr = ERROR_SERVICE_DOES_NOT_EXIST;
        NKDbgPrintfW(L"Handle returned was not the eventlog test handle: %08x", hTestHandle);
        CeEventClose(hTestHandle);
        goto exit;
    }
    
    // Get the Channel and Circular Buffer Information 
    DWORD dwBufferSize = 0;
    lResult = CeEventRead(
                hTestHandle,
                (PEVENT_DESCRIPTOR)pChannel,
                pEventBuffer,
                sizeof(CircularEventBuffer),
                &dwBufferSize,
                NULL);
    if(NO_ERROR != lResult)
    {
        hr = HRESULT_FROM_WIN32(lResult);
        NKDbgPrintfW(L"CeEventRead when attempting to get the channel information returned: %u", lResult);        
        ASSERT(0);
        goto exit;
    }
    
    
exit: 
    if(hChannelKey)
    {
       
        RegCloseKey(hChannelKey);
    }
    if(hTestHandle && hr != ERROR_SERVICE_DOES_NOT_EXIST)
    {
        CeEventClose(hTestHandle);
    }
       
    return hr;
}
