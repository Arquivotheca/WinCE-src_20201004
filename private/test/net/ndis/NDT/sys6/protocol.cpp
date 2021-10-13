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
#include "StdAfx.h"
#include "NDT.h"
#include "Driver.h"
#include "Protocol.h"
#include "Binding.h"
#include "RequestSend.h"
#include "Log.h"
#include "Ndis.h"

//------------------------------------------------------------------------------

CObjectList g_ProtocolList;

//------------------------------------------------------------------------------

static NDIS_STATUS ProtocolBindAdapterEx0(
   IN NDIS_HANDLE ProtocolDriverContext, IN NDIS_HANDLE BindContext,    
   IN PNDIS_BIND_PARAMETERS BindParameters
);

//------------------------------------------------------------------------------

VOID ProtocolUninstall0();

//------------------------------------------------------------------------------

NDIS_STATUS ProtocolNetPnpEvent0(
   IN NDIS_HANDLE ProtocolBindingContext, IN PNET_PNP_EVENT_NOTIFICATION NetPnPEvent
);


//------------------------------------------------------------------------------

NDIS_STATUS ProtocolBindAdapterEx0(
   IN NDIS_HANDLE ProtocolDriverContext, IN NDIS_HANDLE BindContext,    
   IN PNDIS_BIND_PARAMETERS BindParameters
)
{
    NDIS_STATUS Status;

   Log(
      NDT_DBG_BIND_ADAPTER_0_ENTRY, ProtocolDriverContext, BindContext, BindParameters, BindParameters->AdapterName
   );

   CProtocol *pProtocol = (CProtocol *) ProtocolDriverContext;

   ASSERT(pProtocol);
   
   Status = pProtocol->BindAdapter(
       BindContext, BindParameters
   );

   // Log exit
   Log(NDT_DBG_BIND_ADAPTER_0_EXIT, Status);

   return Status;
}

//------------------------------------------------------------------------------

VOID ProtocolUninstall0()
{
   Log(NDT_DBG_PROTOCOL_UNLOAD_0_ENTRY);
   //TODO: Remove this before checkin. Just want to see if this gets called
   ASSERT(FALSE);
   //g_apProtocol[0]->Unload();
   Log(NDT_DBG_PROTOCOL_UNLOAD_0_EXIT);
}

//------------------------------------------------------------------------------

NDIS_STATUS ProtocolNetPnpEvent0(
   IN NDIS_HANDLE ProtocolBindingContext, IN PNET_PNP_EVENT_NOTIFICATION NetPnPEvent
)
{
   NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
   Log(NDT_DBG_PNP_EVENT_0_ENTRY, ProtocolBindingContext, NetPnPEvent);
   CBinding *pBinding = (CBinding *)ProtocolBindingContext;

   if (NetPnPEvent->NetPnPEvent.NetEvent == NetEventBindsComplete) {
     NdisSetEvent(&g_hBindsCompleteEvent);
   }
   else
       {
          //Probably a pause or restart event
       if (pBinding && pBinding->m_pProtocol)
       {
           Status = pBinding->m_pProtocol->PnPEvent(pBinding, NetPnPEvent);
       }
       }
   Log(NDT_DBG_PNP_EVENT_0_EXIT, Status);
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

CProtocol::CProtocol()
{
   m_dwMagic = NDT_MAGIC_PROTOCOL;
   m_bPaused = FALSE;
   NdisAllocateSpinLock(&m_spinLockBeat);
   NdisInitializeTimer(&m_timerBeat, CProtocol::ProtocolHeartBeat, this);
   m_ulPauseEventsCounter = 0;
   m_ulRestartEventsCounter = 0;
   NdisAllocateSpinLock(&m_spinLockPauseRestart);
   m_ulPauseCount = 0;
   m_hProtocol = NULL;
   m_uiHeartBeatDelay = 0;
   m_bLogPackets = FALSE;
   m_pBindingRequest = NULL;
   NdisInitializeEvent(&m_hRestartedEvent);
   m_pPauseRestartLock = new CReaderWriterLock();
}

//------------------------------------------------------------------------------

CProtocol::~CProtocol()
{
   ASSERT(m_hProtocol == NULL);
   m_pPauseRestartLock->Release();
   NdisFreeEvent(&m_hRestartedEvent);
   NdisFreeSpinLock(&m_spinLockBeat);
   NdisFreeSpinLock(&m_spinLockPauseRestart);
   
}

//------------------------------------------------------------------------------

NDIS_STATUS CProtocol::RegisterProtocol(
   UCHAR ucMajor, UCHAR ucMinor, UINT uiFlags, BOOL bReceive, LPWSTR szName
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   NDIS_PROTOCOL_DRIVER_CHARACTERISTICS pc;

   NdisZeroMemory(&pc, sizeof(pc));

   pc.Header.Type = NDIS_OBJECT_TYPE_PROTOCOL_DRIVER_CHARACTERISTICS,
   pc.Header.Size = sizeof(NDIS_PROTOCOL_DRIVER_CHARACTERISTICS);
   pc.Header.Revision = NDIS_PROTOCOL_DRIVER_CHARACTERISTICS_REVISION_1;

   pc.MajorNdisVersion = 6;
   pc.MinorNdisVersion = 0;

   pc.MajorDriverVersion = PROTOCOL_MAJOR_DRIVER_VERSION;
   pc.MinorDriverVersion = PROTOCOL_MINOR_DRIVER_VERSION;

   NdisInitUnicodeString(&(pc.Name), szName);

   pc.BindAdapterHandlerEx = ProtocolBindAdapterEx0;
   pc.UnbindAdapterHandlerEx = CBinding::ProtocolUnbindAdapterEx;
   pc.OpenAdapterCompleteHandlerEx = CBinding::ProtocolOpenAdapterCompleteEx;
   pc.CloseAdapterCompleteHandlerEx = CBinding::ProtocolCloseAdapterCompleteEx;
   pc.NetPnPEventHandler = ProtocolNetPnpEvent0;
   pc.OidRequestCompleteHandler = CBinding::ProtocolOidRequestComplete;
   pc.StatusHandlerEx = CBinding::ProtocolStatusEx;   


   //Ignoring bReceive since we can't register a protocol driver without a receive handler in 6
   pc.ReceiveNetBufferListsHandler = CBinding::ProtocolReceiveNetBufferLists;
   pc.SendNetBufferListsCompleteHandler = CBinding::ProtocolSendNetBufferListsComplete;

   pc.UninstallHandler = ProtocolUninstall0;

   // Set NDIS_PROTOCOL_TESTER flag (we can do unsolicited Open's and pass the reset handle down)
   pc.Flags = 0x20000000;
   
   //// NDIS 3.0 characteristics
   //pc.Ndis40Chars.Ndis30Chars.MajorNdisVersion = ucMajor;
   //pc.Ndis40Chars.Ndis30Chars.MinorNdisVersion = ucMinor;
   //pc.Ndis40Chars.Ndis30Chars.Flags = uiFlags;
   //NdisInitUnicodeString(&(pc.Ndis40Chars.Ndis30Chars.Name), szName);
   //pc.Ndis40Chars.Ndis30Chars.SendCompleteHandler = 
   //   CBinding::ProtocolSendComplete;
   //pc.Ndis40Chars.Ndis30Chars.TransferDataCompleteHandler = 
   //   CBinding::ProtocolTransferDataComplete;
   //pc.Ndis40Chars.Ndis30Chars.ResetCompleteHandler = 
   //   CBinding::ProtocolResetComplete;
   //pc.Ndis40Chars.Ndis30Chars.ReceiveHandler = 
   //   CBinding::ProtocolReceive;
   //pc.Ndis40Chars.Ndis30Chars.ReceiveCompleteHandler = 
   //   CBinding::ProtocolReceiveComplete;
   //pc.Ndis40Chars.Ndis30Chars.StatusHandler = 
   //   CBinding::ProtocolStatus;
   //pc.Ndis40Chars.Ndis30Chars.StatusCompleteHandler = 
   //   CBinding::ProtocolStatusComplete;

   //// NDIS 4.0 characteristics

   //
   // 
   //// Size for NDIS 4.0
   //cbPC = sizeof(NDIS40_PROTOCOL_CHARACTERISTICS);

   //// Did we want to support NDIS 5.x
   //if (ucMajor > 4) {

   //   pc.CoSendCompleteHandler = 
   //      CBinding::ProtocolCoSendComplete;
   //   pc.CoStatusHandler = 
   //      CBinding::ProtocolCoStatus;
   //   pc.CoReceivePacketHandler = 
   //      CBinding::ProtocolCoReceivePacket;
   //   pc.CoAfRegisterNotifyHandler = 
   //      CBinding::ProtocolCoAfRegisterNotify;

   //   cbPC = sizeof(NDIS50_PROTOCOL_CHARACTERISTICS);
   //}
    
   // Register protocol
   status = NdisRegisterProtocolDriver(this, &pc, &m_hProtocol);

   // Wait for binding complete
   if (status == NDIS_STATUS_SUCCESS)
   {
       NdisWaitEvent(&g_hBindsCompleteEvent, 0);
    //Reset the event so that the next registration can wait for it
    NdisResetEvent(&g_hBindsCompleteEvent);
   }

   // We are done
   return status;
}

//------------------------------------------------------------------------------

VOID CProtocol::DeregisterProtocol()
{
   // Stop heart beat
   SetHeartBeat(0);
   
   NdisDeregisterProtocolDriver(m_hProtocol);
   m_hProtocol = NULL;

   // We don't need list of devices....
   RemoveAllDeviceNamesFromList();
}

//------------------------------------------------------------------------------
//
// We don't open an adapter here so just save a device name for later use
//
NDIS_STATUS CProtocol::BindAdapter(
   IN NDIS_HANDLE  BindContext,
   IN PNDIS_BIND_PARAMETERS  BindParameters
)
{
    NDIS_STATUS status = NDIS_STATUS_FAILURE;

    if (this->m_pBindingRequest != NULL && NdisEqualString(BindParameters->AdapterName, &(this->m_pBindingRequest->m_nsAdapterName), TRUE))
    {
        status = this->m_pBindingRequest->CreateBinding(BindContext);
        this->m_pBindingRequest = NULL;
    }

    return status;
}

//------------------------------------------------------------------------------

void CProtocol::Unload()
{
   RemoveAllBindingsFromList();
   RemoveAllDeviceNamesFromList();
}

//------------------------------------------------------------------------------

NDIS_STATUS CProtocol::PnPEvent(CBinding *pBinding, NET_PNP_EVENT_NOTIFICATION *pPnPEvent)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;

  switch (pPnPEvent->NetPnPEvent.NetEvent) {
  case NetEventPause:
      NdisInterlockedIncrement((PLONG) &m_ulPauseEventsCounter);
    OnPause();
      break;
  case NetEventRestart:
    NdisInterlockedIncrement((PLONG) &m_ulRestartEventsCounter);
    OnRestart();
    break;
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

CBinding *CProtocol::GetBindingListHead()
{
    return (CBinding *) m_listBinding.GetHead();
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

void CProtocol::OnPause()
{
   //Acquire spin lock to serialize all pauses and restarts
   NdisAcquireSpinLock(&m_spinLockPauseRestart);
   m_ulPauseCount++;
   if (1 == m_ulPauseCount)
   {
       //We are the first thread trying to pause, lets do the pause
       //get the writer lock to make sure pending send/receives    have completed
       m_pPauseRestartLock->Write();
    m_bPaused = TRUE;
   }
   NdisReleaseSpinLock(&m_spinLockPauseRestart);
}

//------------------------------------------------------------------------------

void CProtocol::OnRestart()
{
   //Acquire spin lock to serialize all pauses and restarts
   NdisAcquireSpinLock(&m_spinLockPauseRestart);
   if (m_ulPauseCount>0)
       m_ulPauseCount--;
   if (0 == m_ulPauseCount)
   {
       //We were the last pause, lets restart now
       //get the writer lock to make sure pending send/receives    have completed
    m_bPaused = FALSE;
    NdisSetEvent(&m_hRestartedEvent);
    m_pPauseRestartLock->WriteComplete();
   }
   NdisReleaseSpinLock(&m_spinLockPauseRestart);
}

//------------------------------------------------------------------------------

