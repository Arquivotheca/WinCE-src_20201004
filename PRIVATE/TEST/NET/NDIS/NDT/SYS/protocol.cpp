//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "NDT.h"
#include "Driver.h"
#include "Protocol.h"
#include "Binding.h"
#include "RequestSend.h"
#include "Log.h"

//------------------------------------------------------------------------------

#define NDT_MAX_PROTOCOL_INSTANCES  2

LONG g_nProtocol = 0;
CProtocol* g_apProtocol[NDT_MAX_PROTOCOL_INSTANCES];

//------------------------------------------------------------------------------

VOID ProtocolBindAdapter0(
   OUT PNDIS_STATUS Status, IN NDIS_HANDLE BindContext,    
   IN PNDIS_STRING DeviceName, IN PVOID SystemSpecific1,
   IN PVOID SystemSpecific2
);

VOID ProtocolBindAdapter1(
   OUT PNDIS_STATUS Status, IN NDIS_HANDLE BindContext,    
   IN PNDIS_STRING DeviceName, IN PVOID SystemSpecific1,
   IN PVOID SystemSpecific2
);

BIND_HANDLER g_aProtocolBindAdapter[NDT_MAX_PROTOCOL_INSTANCES] = {
   ProtocolBindAdapter0, ProtocolBindAdapter1
};

//------------------------------------------------------------------------------

VOID ProtocolUnload0();

VOID ProtocolUnload1();

UNLOAD_PROTOCOL_HANDLER g_aProtocolUnload[NDT_MAX_PROTOCOL_INSTANCES] = {
   ProtocolUnload0, ProtocolUnload1
};

//------------------------------------------------------------------------------

NDIS_STATUS ProtocolPnPEvent0(
   IN NDIS_HANDLE ProtocolBindingContext, IN PNET_PNP_EVENT NetPnPEvent
);

NDIS_STATUS ProtocolPnPEvent1(
   IN NDIS_HANDLE ProtocolBindingContext, IN PNET_PNP_EVENT NetPnPEvent
);

PNP_EVENT_HANDLER g_aProtocolPnPEvent[NDT_MAX_PROTOCOL_INSTANCES] = {
   ProtocolPnPEvent0, ProtocolPnPEvent1
};

//------------------------------------------------------------------------------

static VOID ProtocolBindAdapter0(
   OUT PNDIS_STATUS Status, IN NDIS_HANDLE BindContext,    
   IN PNDIS_STRING DeviceName, IN PVOID SystemSpecific1,
   IN PVOID SystemSpecific2
)
{
   // Log entry
   Log(
      NDT_DBG_BIND_ADAPTER_0_ENTRY, BindContext, DeviceName->Buffer, 
      SystemSpecific1, SystemSpecific2
   );

   *Status = g_apProtocol[0]->BindAdapter(
      BindContext, DeviceName, SystemSpecific1, SystemSpecific2
   );

   // Log exit
   Log(NDT_DBG_BIND_ADAPTER_0_EXIT, *Status);
}

//------------------------------------------------------------------------------

static VOID ProtocolBindAdapter1(
   OUT PNDIS_STATUS Status, IN NDIS_HANDLE BindContext,    
   IN PNDIS_STRING DeviceName, IN PVOID SystemSpecific1,
   IN PVOID SystemSpecific2
)
{
   // Log entry
   Log(
      NDT_DBG_BIND_ADAPTER_1_ENTRY, BindContext, DeviceName->Buffer, 
      SystemSpecific1, SystemSpecific2
   );

   *Status = g_apProtocol[1]->BindAdapter(
      BindContext, DeviceName, SystemSpecific1, SystemSpecific2
   );

   // Log exit
   Log(NDT_DBG_BIND_ADAPTER_1_EXIT, *Status);
}

//------------------------------------------------------------------------------

VOID ProtocolUnload0()
{
   Log(NDT_DBG_PROTOCOL_UNLOAD_0_ENTRY);
   g_apProtocol[0]->Unload();
   Log(NDT_DBG_PROTOCOL_UNLOAD_0_EXIT);
}

//------------------------------------------------------------------------------

VOID ProtocolUnload1()
{
   Log(NDT_DBG_PROTOCOL_UNLOAD_1_ENTRY);
   g_apProtocol[1]->Unload();
   Log(NDT_DBG_PROTOCOL_UNLOAD_1_EXIT);
}

//------------------------------------------------------------------------------

NDIS_STATUS ProtocolPnPEvent0(
   IN NDIS_HANDLE ProtocolBindingContext, IN PNET_PNP_EVENT NetPnPEvent
)
{
   Log(NDT_DBG_PNP_EVENT_0_ENTRY, ProtocolBindingContext, NetPnPEvent);
   CBinding *pBinding = (CBinding *)ProtocolBindingContext;
   NDIS_STATUS Status = g_apProtocol[0]->PnPEvent(pBinding, NetPnPEvent);
   Log(NDT_DBG_PNP_EVENT_0_EXIT, Status);
   return Status;
}

//------------------------------------------------------------------------------

NDIS_STATUS ProtocolPnPEvent1(
   IN NDIS_HANDLE ProtocolBindingContext, IN PNET_PNP_EVENT NetPnPEvent
)
{
   Log(NDT_DBG_PNP_EVENT_1_ENTRY, ProtocolBindingContext, NetPnPEvent);
   CBinding *pBinding = (CBinding *)ProtocolBindingContext;
   NDIS_STATUS Status = g_apProtocol[1]->PnPEvent(pBinding, NetPnPEvent);
   Log(NDT_DBG_PNP_EVENT_1_EXIT, Status);
   return Status;
}

//------------------------------------------------------------------------------

VOID CProtocol::ProtocolHeartBeat(
   IN PVOID SystemSpecific1, IN PVOID FunctionContext,
   IN PVOID SystemSpecific2, IN PVOID SystemSpecific3
)
{
   CProtocol* pProtocol = (CProtocol*)FunctionContext;
   ASSERT(pProtocol->m_dwMagic == NDT_MAGIC_PROTOCOL);
   pProtocol->HeartBeat();
}

//------------------------------------------------------------------------------

CProtocol::CProtocol(CDriver *pDriver)
{
   m_dwMagic = NDT_MAGIC_PROTOCOL;
   m_hProtocol = NULL;
   m_pDriver = pDriver; m_pDriver->AddRef();
   m_uiHeartBeatDelay = 0;
   m_bLogPackets = FALSE;
   NdisInitializeEvent(&m_hBindsCompleteEvent);
   NdisAllocateSpinLock(&m_spinLockBeat);
   NdisInitializeTimer(&m_timerBeat, CProtocol::ProtocolHeartBeat, this);
}

//------------------------------------------------------------------------------

CProtocol::~CProtocol()
{
   ASSERT(m_hProtocol == NULL);
   NdisFreeSpinLock(&m_spinLockBeat);
   m_pDriver->Release();
}

//------------------------------------------------------------------------------

NDIS_STATUS CProtocol::RegisterProtocol(
   UCHAR ucMajor, UCHAR ucMinor, UINT uiFlags, BOOL bReceive, LPWSTR szName
)
{
   // No more protocol instances than NDT_MAX_PROTOCOL_INSTANCES
   LONG nInstance = NdisInterlockedIncrement(&g_nProtocol);
   if (nInstance-- > NDT_MAX_PROTOCOL_INSTANCES) {
      NdisInterlockedDecrement(&g_nProtocol);
      return NDIS_STATUS_RESOURCES;
   }
   // Set a global protocol context
   g_apProtocol[nInstance] = this;

   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   NDIS_PROTOCOL_CHARACTERISTICS pc;
   UINT cbPC = 0;
   
   NdisZeroMemory(&pc, sizeof(pc));

   // NDIS 3.0 characteristics
   pc.Ndis40Chars.Ndis30Chars.MajorNdisVersion = ucMajor;
   pc.Ndis40Chars.Ndis30Chars.MinorNdisVersion = ucMinor;
   pc.Ndis40Chars.Ndis30Chars.Flags = uiFlags;
   NdisInitUnicodeString(&(pc.Ndis40Chars.Ndis30Chars.Name), szName);
   pc.Ndis40Chars.Ndis30Chars.OpenAdapterCompleteHandler = 
      CBinding::ProtocolOpenAdapterComplete;
   pc.Ndis40Chars.Ndis30Chars.CloseAdapterCompleteHandler = 
      CBinding::ProtocolCloseAdapterComplete;
   pc.Ndis40Chars.Ndis30Chars.SendCompleteHandler = 
      CBinding::ProtocolSendComplete;
   pc.Ndis40Chars.Ndis30Chars.TransferDataCompleteHandler = 
      CBinding::ProtocolTransferDataComplete;
   pc.Ndis40Chars.Ndis30Chars.ResetCompleteHandler = 
      CBinding::ProtocolResetComplete;
   pc.Ndis40Chars.Ndis30Chars.RequestCompleteHandler = 
      CBinding::ProtocolRequestComplete;
   pc.Ndis40Chars.Ndis30Chars.ReceiveHandler = 
      CBinding::ProtocolReceive;
   pc.Ndis40Chars.Ndis30Chars.ReceiveCompleteHandler = 
      CBinding::ProtocolReceiveComplete;
   pc.Ndis40Chars.Ndis30Chars.StatusHandler = 
      CBinding::ProtocolStatus;
   pc.Ndis40Chars.Ndis30Chars.StatusCompleteHandler = 
      CBinding::ProtocolStatusComplete;

   // NDIS 4.0 characteristics
   pc.Ndis40Chars.BindAdapterHandler = 
      g_aProtocolBindAdapter[nInstance];
   pc.Ndis40Chars.UnbindAdapterHandler = 
      CBinding::ProtocolUnbindAdapter;
   pc.Ndis40Chars.PnPEventHandler = 
      g_aProtocolPnPEvent[nInstance];
   pc.Ndis40Chars.UnloadHandler = 
      g_aProtocolUnload[nInstance];

   if (bReceive) {
      pc.Ndis40Chars.ReceivePacketHandler =
         CBinding::ProtocolReceivePacket;
   }
    
   // Size for NDIS 4.0
   cbPC = sizeof(NDIS40_PROTOCOL_CHARACTERISTICS);

   // Did we want to support NDIS 5.x
   if (ucMajor > 4) {

      pc.CoSendCompleteHandler = 
         CBinding::ProtocolCoSendComplete;
      pc.CoStatusHandler = 
         CBinding::ProtocolCoStatus;
      pc.CoReceivePacketHandler = 
         CBinding::ProtocolCoReceivePacket;
      pc.CoAfRegisterNotifyHandler = 
         CBinding::ProtocolCoAfRegisterNotify;

      cbPC = sizeof(NDIS50_PROTOCOL_CHARACTERISTICS);
   }
    
   // Register protocol
   NdisRegisterProtocol(&status, &m_hProtocol, &pc, cbPC);

   // Wait for binding complete
   NdisWaitEvent(&m_hBindsCompleteEvent, 0);

   // We are done
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CProtocol::DeregisterProtocol()
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;

   // Stop heart beat
   SetHeartBeat(0);
   
   NdisDeregisterProtocol(&status, m_hProtocol);
   m_hProtocol = NULL;

   // Just for case
   ASSERT(status != NDIS_STATUS_PENDING);
   
   // We don't need list of devices....
   RemoveAllDeviceNamesFromList();
   return status;
}

//------------------------------------------------------------------------------
//
// We don't open an adapter here so just save a device name for later use
//
NDIS_STATUS CProtocol::BindAdapter(
   NDIS_HANDLE hBindContext, NDIS_STRING *pnsDeviceName, 
   PVOID pvSystemSpecific1, PVOID pvSystemSpecific2
)
{
   // TODO: Save adapter name for later use

   // We are done
   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

void CProtocol::Unload()
{
   RemoveAllBindingsFromList();
   RemoveAllDeviceNamesFromList();
}

//------------------------------------------------------------------------------

NDIS_STATUS CProtocol::PnPEvent(CBinding *pBinding, NET_PNP_EVENT *pPnPEvent)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;

   if (pBinding != NULL) {
      ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
      //status = pBinding->PnPEvent(pPnPEvent);
   } else {
      switch (pPnPEvent->NetEvent) {
      case NetEventBindsComplete:
         NdisSetEvent(&m_hBindsCompleteEvent);
         break;
      }
   }
   return status;
}

//------------------------------------------------------------------------------

void CProtocol::AddBindingToList(CBinding *pBinding)
{
   // Nobody else should play with a list
   m_listBinding.AcquireSpinLock();
   // Add to queue
   m_listBinding.AddTail(pBinding);
   // We hold a reference
   pBinding->AddRef();
   // We are done
   m_listBinding.ReleaseSpinLock();
}

//------------------------------------------------------------------------------

void CProtocol::RemoveBindingFromList(CBinding *pBinding)
{
   // Nobody else should play with a list
   m_listBinding.AcquireSpinLock();
   // Remove it from list
   m_listBinding.Remove(pBinding);
   // We don't hold a reference anymore
   pBinding->Release();
   // We are done
   m_listBinding.ReleaseSpinLock();
}

//------------------------------------------------------------------------------

void CProtocol::RemoveAllBindingsFromList()
{
   CBinding *pBinding = NULL;

   // Nobody else should play with a list
   m_listBinding.AcquireSpinLock();
   // First release all references
   while ((pBinding = (CBinding*)m_listBinding.GetHead()) != NULL) {
      // Remove binding from a list
      m_listBinding.Remove(pBinding);
      // We don't hold a reference anymore
      pBinding->Release();
   }
   m_listBinding.ReleaseSpinLock();
}

//------------------------------------------------------------------------------

void CProtocol::AddDeviceNameToList(NDIS_STRING ns)
{
   // Nobody else should play with a list
   m_listDeviceName.AcquireSpinLock();
   // We are done
   m_listDeviceName.ReleaseSpinLock();
}

//------------------------------------------------------------------------------

void CProtocol::RemoveDeviceNameFromList(NDIS_STRING ns)
{
   // Nobody else should play with a list
   m_listDeviceName.AcquireSpinLock();
   
   // We are done
   m_listDeviceName.ReleaseSpinLock();
}

//------------------------------------------------------------------------------

void CProtocol::RemoveAllDeviceNamesFromList()
{
   // Nobody else should play with a list
   m_listDeviceName.AcquireSpinLock();

   // We are done
   m_listDeviceName.ReleaseSpinLock();
}

//------------------------------------------------------------------------------

void CProtocol::SetHeartBeat(UINT uiHeartBeatDelay)
{
   BOOLEAN bCanceled = FALSE;
   
   NdisAcquireSpinLock(&m_spinLockBeat);
   if (uiHeartBeatDelay != 0 && m_uiHeartBeatDelay == 0) {
      m_uiHeartBeatDelay = uiHeartBeatDelay;
      NdisSetTimer(&m_timerBeat, m_uiHeartBeatDelay);
   } else if (uiHeartBeatDelay == 0 && m_uiHeartBeatDelay != 0) {
      m_uiHeartBeatDelay = 0;
      NdisCancelTimer(&m_timerBeat, &bCanceled);
   } else {
      m_uiHeartBeatDelay = uiHeartBeatDelay;
   }
   NdisReleaseSpinLock(&m_spinLockBeat);
}

//------------------------------------------------------------------------------

void CProtocol::HeartBeat()
{
   CBinding* pBinding = NULL;
   CRequestSend* pRequestSend = NULL;
   
   // Nobody else should play with a list
   m_listBinding.AcquireSpinLock();
   Log(NDT_INF_BEAT_LOG1, m_listBinding.m_uiItems);
   pBinding = (CBinding*)m_listBinding.GetHead();
   while (pBinding != NULL) {
      Log(NDT_INF_BEAT_LOG2, 
         pBinding->m_listFreeRecvPackets.m_uiItems,
         pBinding->m_listFreeSendPackets.m_uiItems,
         pBinding->m_listSentPackets.m_uiItems,
         pBinding->m_listReceivedPackets.m_uiItems
      );

      pRequestSend = pBinding->FindSendRequest(FALSE);
      if (pRequestSend != NULL) {
         Log(NDT_INF_BEAT_SEND, pRequestSend->m_ulPacketsToSend);
         pRequestSend->Release();
      }

      pBinding = (CBinding*)m_listBinding.GetNext(pBinding);
   }
   m_listBinding.ReleaseSpinLock();

   // Reschedule beat
   NdisAcquireSpinLock(&m_spinLockBeat);
   if (m_uiHeartBeatDelay != 0) {
      NdisSetTimer(&m_timerBeat, m_uiHeartBeatDelay);
   }
   NdisReleaseSpinLock(&m_spinLockBeat);
}

//------------------------------------------------------------------------------
