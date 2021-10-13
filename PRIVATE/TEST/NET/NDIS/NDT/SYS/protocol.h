//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __PROTOCOL_H
#define __PROTOCOL_H

//------------------------------------------------------------------------------

#include "Object.h"
#include "ObjectList.h"

//------------------------------------------------------------------------------

class CDriver;
class CBinding;

//------------------------------------------------------------------------------

class CProtocol : public CObject
{
private:
   // Device name list
   CObjectList m_listDeviceName;
   // List open bindings for protocol
   CObjectList m_listBinding;
   // Event 
   NDIS_EVENT m_hBindsCompleteEvent;
   // Guard for a heart beat
   NDIS_SPIN_LOCK m_spinLockBeat;
   // Timer used for a heart beat
   NDIS_TIMER m_timerBeat;                
   
public:
   CDriver* m_pDriver;           // Parent object
   NDIS_HANDLE m_hProtocol;      // Protocol registration handle
   UINT m_uiHeartBeatDelay;      // Heart beat delay 
   BOOL m_bLogPackets;           // Log received/send packets
   
private:
   static VOID ProtocolHeartBeat(
      IN PVOID SystemSpecific1, IN PVOID FunctionContext,
      IN PVOID SystemSpecific2, IN PVOID SystemSpecific3
   );

public:
   CProtocol(CDriver *pDriver);
   virtual ~CProtocol();

   // Register protocol with a given version, flag and name
   NDIS_STATUS RegisterProtocol(
      UCHAR ucMajor, UCHAR ucMinor, UINT uiFlags, BOOL bReceive, LPWSTR szName
   );

   NDIS_STATUS BindAdapter(
      NDIS_HANDLE hBindContext, NDIS_STRING *pnsDeviceName, 
      PVOID pvSystemSpecific1, PVOID pvSystemSpecific2
   );

   NDIS_STATUS DeregisterProtocol();

   void AddBindingToList(CBinding *pBinding);
   void RemoveBindingFromList(CBinding *pBinding);
   void RemoveAllBindingsFromList();

   void AddDeviceNameToList(NDIS_STRING ns);
   void RemoveDeviceNameFromList(NDIS_STRING ns);
   void RemoveAllDeviceNamesFromList();

   NDIS_STATUS PnPEvent(CBinding* pBinding, NET_PNP_EVENT *pPnPEvent);
   void Unload();

   NDIS_STATUS GetProtocolHeader(
      ULONG ulSequenceNumber, PBYTE& pbHeader, UINT *pcbHeader
   );

   void SetHeartBeat(UINT uiHeartBeatDelay);
   void HeartBeat();
   
};

//------------------------------------------------------------------------------

#endif
