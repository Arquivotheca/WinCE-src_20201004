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
#ifndef __PROTOCOL_H
#define __PROTOCOL_H

//------------------------------------------------------------------------------

#include "Object.h"
#include "ObjectList.h"

//------------------------------------------------------------------------------

#define PROTOCOL_MAJOR_DRIVER_VERSION 1
#define PROTOCOL_MINOR_DRIVER_VERSION 0

//------------------------------------------------------------------------------

class CDriver;
class CBinding;
extern NDIS_EVENT g_hBindsCompleteEvent;

#include "ReaderWriterLock.h"
#include "RequestBind.h"
#define NDT_MAX_PROTOCOL_INSTANCES  2

//------------------------------------------------------------------------------

class CProtocol : public CObject
{
private:
   // Device name list
   CObjectList m_listDeviceName;
   // List open bindings for protocol
   CObjectList m_listBinding;
   //Is protocol paused?
   BOOL m_bPaused;
   // Guard for a heart beat
   NDIS_SPIN_LOCK m_spinLockBeat;
   // Timer used for a heart beat
   NDIS_TIMER m_timerBeat;       
   // pause events counter
   ULONG m_ulPauseEventsCounter;
    // restart events counter
   ULONG m_ulRestartEventsCounter;
   // guards against multiple pause restarts being handled simultaneously
   NDIS_SPIN_LOCK m_spinLockPauseRestart;
   // number of simultaneous pauses handled: only the first really results in a pause
   ULONG m_ulPauseCount;
   
public:
   NDIS_HANDLE m_hProtocol;      // Protocol registration handle
   UINT m_uiHeartBeatDelay;      // Heart beat delay 
   BOOL m_bLogPackets;           // Log received/send packets
   CRequestBind* m_pBindingRequest;    //HACK: CRequestBind object so that the bind callback can delegate back to the request
   //Event
   NDIS_EVENT m_hRestartedEvent;
   //guard for completing pending send/receives before pause
   CReaderWriterLock* m_pPauseRestartLock;

private:
   static VOID ProtocolHeartBeat(
      IN PVOID SystemSpecific1, IN PVOID FunctionContext,
      IN PVOID SystemSpecific2, IN PVOID SystemSpecific3
   );

public:
   CProtocol();
   virtual ~CProtocol();

   // Register protocol with a given version, flag and name
   NDIS_STATUS RegisterProtocol(
      UCHAR ucMajor, UCHAR ucMinor, UINT uiFlags, BOOL bReceive, LPWSTR szName
   );

   NDIS_STATUS BindAdapter(
      IN NDIS_HANDLE  BindContext,
      IN PNDIS_BIND_PARAMETERS  BindParameters
   );

   VOID DeregisterProtocol();

   void AddBindingToList(CBinding *pBinding);
   void RemoveBindingFromList(CBinding *pBinding);
   void RemoveAllBindingsFromList();
   CBinding *GetBindingListHead();
   
   void AddDeviceNameToList(NDIS_STRING ns);
   void RemoveDeviceNameFromList(NDIS_STRING ns);
   void RemoveAllDeviceNamesFromList();

   NDIS_STATUS PnPEvent(CBinding* pBinding, NET_PNP_EVENT_NOTIFICATION *pPnPEvent);
   void Unload();

   NDIS_STATUS GetProtocolHeader(
      ULONG ulSequenceNumber, PBYTE& pbHeader, UINT *pcbHeader
   );

   void SetHeartBeat(UINT uiHeartBeatDelay);
   void HeartBeat();

   void OnPause();
   void OnRestart();
   
};

//------------------------------------------------------------------------------

#endif
