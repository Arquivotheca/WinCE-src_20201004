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
#ifndef __BINDING_H
#define __BINDING_H

//------------------------------------------------------------------------------

#include "Object.h"
#include "ObjectList.h"
#include "Packet.h"
#include "Request.h"
#include "Log.h"
#define NDT6_TAG   '6TDN'
//------------------------------------------------------------------------------

class CRequestBind;
class CRequestUnbind;
class CRequestReset;
class CRequestRequest;
class CRequestReceive;
class CRequestReceiveEx;
class CRequestSend;

//------------------------------------------------------------------------------

class CBinding : public CObject
{
   friend class CProtocol;
   friend class CPacket;
   friend class CMedium;
   friend class CMedium802_3;

private:
   static LONG s_lInstanceCounter;        // How many instances we created

public:
   LONG m_lInstanceId;                    // This instance id
   
private:
   CObjectList m_listRequest;             // Waiting requests list
   
protected:
   NDIS_STRING m_nsAdapterName;           // Adapter name

   UINT m_uiPacketsSend;                  // How many packets for send alloc
   UINT m_uiPacketsRecv;                  // How many packets for recv alloc
   UINT m_uiBuffersPerPacket;             // Buffers/Packet allocate
   UINT m_uiRecvCompleteDelay;            //The delay for the Receive complete timer
   
   NDIS_HANDLE m_hNBLPool;
   NDIS_HANDLE m_hNBPool;
   BOOL m_bRecvCompleteQueued;        //Flag indicating if a Receive complete operation has already been queued
   NDIS_SPIN_LOCK m_spinLockRecvComplete; //Guard for queuing a receive complete operation
   NDIS_TIMER m_timerRecvComplete;    //The timer to launch the Receive complete operation asynchronously
   
public:   
   CProtocol* m_pProtocol;                // Parent protocol
   NDIS_HANDLE m_hAdapter;                // Binding handle
   CMedium* m_pMedium;                    // Medium object
   DWORD m_dwInternalTimeout;             // Internal command timeout

   ULONG m_ulUnexpectedEvents;            // Unexpected event counter
   ULONG m_ulUnexpectedOpenComplete;      // Unexpected open complete
   ULONG m_ulUnexpectedCloseComplete;     // Unexpected close complete
   ULONG m_ulUnexpectedResetComplete;
   ULONG m_ulUnexpectedSendComplete;
   ULONG m_ulUnexpectedRequestComplete;
   ULONG m_ulUnexpectedTransferComplete;
   ULONG m_ulUnexpectedReceiveIndicate;
   ULONG m_ulUnexpectedStatusIndicate;
   ULONG m_ulUnexpectedResetStart;
   ULONG m_ulUnexpectedResetEnd;
   ULONG m_ulUnexpectedMediaConnect;
   ULONG m_ulUnexpectedMediaDisconnect;
   ULONG m_ulUnexpectedBreakpoints;

   // Following are not unexpected status/events counters. These counters are maintained in ProtocolStatus function.
   ULONG m_ulTotalStatusIndicate;          // Total (Reset Start, Reset stop etc.) status indication
   ULONG m_ulStatusResetStart;                  // ResetStart indication
   ULONG m_ulStatusResetEnd;
   ULONG m_ulStatusMediaConnect;
   ULONG m_ulStatusMediaDisconnect;

   BOOL   m_bQueuePackets;                // Should be packets queued?
   UINT   m_cbQueueSrcAddr;               // Size of source address
   UCHAR* m_pucQueueSrcAddr;              // The source address to queue
   
   CObjectList m_listFreeSendPackets;
   CObjectList m_listFreeRecvPackets;
   CObjectList m_listSentPackets;
   CObjectList m_listReceivedPackets;
   CObjectList m_listTransferPackets;

   USHORT m_usLocalId;                    // ID used in a protocol header
   USHORT m_usRemoteId;                   // ID used in a protocol header
   ULONG  m_ulWindowSize;                 // Window size for responses
   ULONG  m_ulConversationId;            //ID to prevent collisions between tests   

   BOOL   m_bPoolsAllocated;              // Are memory pools allocted?
   BYTE*  m_pucStaticBody;                // Static packet body

   NDIS_EVENT m_hUnbindCompleteEvent;        //Set when unbind is complete for this binding.
   HANDLE m_hUnbindContext;        //Used to complete an unbind operation
private:
//
// Following static functions are exported by protocol and they contain 
// reference to a protocol bind context
//

   static NDIS_STATUS ProtocolUnbindAdapterEx(
      IN NDIS_HANDLE UnbindContext,
      IN NDIS_HANDLE  ProtocolBindingContext
   );

   static VOID ProtocolOpenAdapterCompleteEx(
      IN NDIS_HANDLE ProtocolBindingContext,
      IN NDIS_STATUS Status
   );

   static VOID ProtocolCloseAdapterCompleteEx(
      IN NDIS_HANDLE ProtocolBindingContext
   );

   static NDIS_STATUS ProtocolReceive(
      IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_HANDLE MacReceiveContext,
      IN PVOID HeaderBuffer, IN UINT HeaderBufferSize,
      IN PVOID LookAheadBuffer, IN UINT LookaheadBufferSize, IN UINT PacketSize
   );

   /*
   static VOID ProtocolTransferDataComplete(
      IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_PACKET Packet,
      IN NDIS_STATUS Status, IN UINT BytesTransferred
   );
   */

   /*
   static INT ProtocolReceivePacket(
      IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_PACKET Packet
   );
   */

   static VOID ProtocolSendNetBufferListsComplete(
      IN NDIS_HANDLE ProtocolBindingContext, IN PNET_BUFFER_LIST NetBufferLists,
      IN ULONG SendCompleteFlags
   );

   static VOID ProtocolReceiveNetBufferLists(
       IN NDIS_HANDLE ProtocolBindingContext, IN PNET_BUFFER_LIST NetBufferLists,
       IN NDIS_PORT_NUMBER PortNumber, IN ULONG NumberOfNetBufferLists,
       IN ULONG ReceiveFlags
   );

   static VOID ProtocolResetComplete(
      IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS Status
   );

   static VOID ProtocolOidRequestComplete(
      IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_OID_REQUEST OidRequest,
      IN NDIS_STATUS Status
   );

   static VOID ProtocolStatusEx(
      IN NDIS_HANDLE ProtocolBindingContext,
      IN PNDIS_STATUS_INDICATION StatusIndication
   );

   static VOID ProtocolStatusComplete(
      IN NDIS_HANDLE  ProtocolBindingContext
   );

   /*
   static VOID ProtocolCoSendComplete(
      IN NDIS_STATUS Status, IN NDIS_HANDLE ProtocolVcContext,
      IN PNDIS_PACKET Packet
   );
   */

   static VOID ProtocolCoStatus(
      IN NDIS_HANDLE ProtocolBindingContext,
      IN NDIS_HANDLE ProtocolVcContext OPTIONAL,
      IN NDIS_STATUS GeneralStatus, IN PVOID StatusBuffer,
      IN UINT StatusBufferSize
   );

   /*
   static UINT ProtocolCoReceivePacket(
      IN NDIS_HANDLE ProtocolBindingContext,
      IN NDIS_HANDLE ProtocolVcContext,
      IN PNDIS_PACKET Packet
    );
   */

   static VOID ProtocolCoAfRegisterNotify(
      IN NDIS_HANDLE ProtocolBindingContext,
      IN PCO_ADDRESS_FAMILY AddressFamily
   );

   static VOID ProtocolReceiveComplete(  
    IN PVOID SystemSpecific1, 
    IN PVOID FunctionContext,
    IN PVOID SystemSpecific2, 
    IN PVOID SystemSpecific3
   );

//
// There are internal function which are called from static function which
// are already in an object context
//

   void OpenAdapterComplete(
      NDIS_STATUS status
   );

   void CloseAdapterComplete();

   void ResetComplete(NDIS_STATUS status);

   NDIS_STATUS Receive(
      NDIS_HANDLE hMacReceiveContext, PVOID pvHeaderBuffer, 
      UINT uiHeaderBufferSize, PVOID pvLookAheadBuffer, 
      UINT uiLookaheadBufferSize, UINT uiPacketSize
   );

   void ReceiveComplete();

   void ResetComplete();

   /*
   void TransferDataComplete(
      PNDIS_PACKET pNdisPacket, NDIS_STATUS status, UINT uiBytesTransferred
   );
   */

   /*
   INT ReceivePacket(PNDIS_PACKET pNdisPacket);
   */

   void SendComplete(PNET_BUFFER_LIST pNBL);

   void ReceiveNetBufferLists(IN PNET_BUFFER_LIST NetBufferLists, IN ULONG NumberOfNetBufferLists,
       IN ULONG ReceiveFlags);

   void RequestComplete( PNDIS_OID_REQUEST pRequest, NDIS_STATUS status);

   void Status(
      PNDIS_STATUS_INDICATION StatusIndication
   );

   void StatusComplete();

   NDIS_STATUS PnPEvent(PNET_PNP_EVENT_NOTIFICATION pNetPnPEvent);

   NDIS_STATUS UnbindAdapter(NDIS_HANDLE hUnbindContext);

   void QueueReceiveComplete();

//
// Following function are internal
//
protected:

   NDIS_STATUS CheckProtocolHeader(
      PVOID pvLookAheadBuffer, UINT uiLookaheadBufferSize, UINT uiPacketSize
   );

   NDIS_STATUS BuildPacketBody(
      CPacket* pPacket, CMDLChain *pMDLChain, BYTE ucSizeMode, UINT uiSize, BYTE ucFirstByte
   );
   NDIS_STATUS CheckPacketBody(
      PVOID pvBody, BYTE ucSizeMode, UINT uiSize, BYTE ucFirstByte
   );

private:
   void AddPacketToList(CObjectList* pObjectList, CPacket* pPacket);
   void RemovePacketFromList(CObjectList* pPacketList, CPacket* pPacket);
   CPacket* GetPacketFromList(CObjectList* pObjectList, BOOL bRemove);
   BOOLEAN GetPacketsFromList(
      CObjectList* pPacketList, CPacket** apPackets, ULONG ulCount
   );
   BOOLEAN IsPacketListEmpty(CObjectList* pObjectList);

public:
   CBinding(CProtocol *pProtocol);
   virtual ~CBinding();

   NDIS_STATUS AllocatePools();
   NDIS_STATUS DeallocatePools();

   void OpenAdapter(NDIS_MEDIUM medium);
   void CloseAdapter();

   void AddRequestToList(CRequest* pRequest);
   void RemoveRequestFromList(CRequest* pRequest);
   CRequest* FindRequestByType(NDT_ENUM_REQUEST_TYPE eType, BOOL bRemove);
   CRequest* GetRequestFromList(BOOL bRemove);

   inline void AddPacketToFreeSend(CPacket* pPacket);
   inline void RemovePacketFromFreeSend(CPacket* pPacket);
   inline CPacket *GetPacketFromFreeSend(BOOL bRemove);
   inline BOOLEAN GetPacketsFromFreeSend(CPacket* apPackets[], ULONG ulCount);
   
   inline void AddPacketToFreeRecv(CPacket* pPacket);
   inline void RemovePacketFromFreeRecv(CPacket* pPacket);
   inline CPacket* GetPacketFromFreeRecv(BOOL bRemove);

   inline void AddPacketToSent(CPacket* pPacket);
   inline void RemovePacketFromSent(CPacket* pPacket);
   inline CPacket* GetPacketFromSent(BOOL bRemove);
   inline BOOLEAN IsEmptyPacketSent();
   
   inline void AddPacketToReceived(CPacket* pPacket);
   inline void RemovePacketFromReceived(CPacket* pPacket);
   inline CPacket *GetPacketFromReceived(BOOL bRemove);

   inline CRequestBind* FindBindRequest(BOOL bRemove);
   inline CRequestUnbind* FindUnbindRequest(BOOL bRemove);
   inline CRequestReset* FindResetRequest(BOOL bRemove);
   inline CRequestRequest* FindRequestRequest(BOOL bRemove);
   inline CRequestReceive* FindReceiveRequest(BOOL bRemove);
   inline CRequestSend* FindSendRequest(BOOL bRemove);
   inline CRequestReceiveEx* FindReceiveExRequest(BOOL bRemove);

//
// Function related to packet preparation
//
   void BuildNBLs(PNET_BUFFER_LIST *pNBLList, UINT uiNumDestinations, HANDLE hSource);
   void FreeNBLs(PNET_BUFFER_LIST pNBLList);
   NET_BUFFER_LIST *BuildNBL(HANDLE hSource);

   NDIS_STATUS BuildProtocolHeader(
      CPacket* pPacket, CMDLChain *pMDLChain, UINT uiSize, BYTE ucResponseMode, BYTE ucFirstByte, 
      ULONG ulSequenceNumber, ULONG ulConversationId, USHORT usReplyId
   );

   NDIS_STATUS BuildPacketToSend(
      CPacket* pPacket, BYTE* pucSrcAddr, BYTE* pucDestAddr, 
      UCHAR ucResponseMode, BYTE ucSizeMode, UINT uiSize, 
      ULONG ulSequenceNumber, ULONG ulConversationId, USHORT usReplyId
   );

   NDIS_STATUS BuildPacketForResponse(
      CPacket* pPacket, CPacket* pRecvPacket
   );
};

//------------------------------------------------------------------------------

inline void CBinding::AddPacketToFreeSend(CPacket *pPacket)
{
   pPacket->DestroyNetBufferMDLs();
   AddPacketToList(&m_listFreeSendPackets, pPacket);
}

inline void CBinding::RemovePacketFromFreeSend(CPacket *pPacket)
{
   RemovePacketFromList(&m_listFreeSendPackets, pPacket);
}

inline CPacket* CBinding::GetPacketFromFreeSend(BOOL bRemove)
{
   return GetPacketFromList(&m_listFreeSendPackets, bRemove);
}

inline BOOLEAN CBinding::GetPacketsFromFreeSend(
   CPacket** apPackets, ULONG ulCount
)
{
   return GetPacketsFromList(&m_listFreeSendPackets, apPackets, ulCount);
}

//------------------------------------------------------------------------------

inline void CBinding::AddPacketToFreeRecv(CPacket *pPacket)
{
   pPacket->DestroyNetBufferMDLs();
   AddPacketToList(&m_listFreeRecvPackets, pPacket);
}

inline void CBinding::RemovePacketFromFreeRecv(CPacket *pPacket)
{
   RemovePacketFromList(&m_listFreeRecvPackets, pPacket);
}

inline CPacket* CBinding::GetPacketFromFreeRecv(BOOL bRemove)
{
   return GetPacketFromList(&m_listFreeRecvPackets, bRemove);
}

//------------------------------------------------------------------------------

inline void CBinding::AddPacketToSent(CPacket *pPacket)
{
   AddPacketToList(&m_listSentPackets, pPacket);
}

inline void CBinding::RemovePacketFromSent(CPacket *pPacket)
{
   RemovePacketFromList(&m_listSentPackets, pPacket);
}

inline CPacket* CBinding::GetPacketFromSent(BOOL bRemove)
{
   return GetPacketFromList(&m_listSentPackets, bRemove);
}

inline BOOLEAN CBinding::IsEmptyPacketSent()
{
   return IsPacketListEmpty(&m_listSentPackets);
}

//------------------------------------------------------------------------------

inline void CBinding::AddPacketToReceived(CPacket *pPacket)
{
   AddPacketToList(&m_listReceivedPackets, pPacket);
}

inline void CBinding::RemovePacketFromReceived(CPacket *pPacket)
{
   RemovePacketFromList(&m_listReceivedPackets, pPacket);
}

inline CPacket* CBinding::GetPacketFromReceived(BOOL bRemove)
{
   return GetPacketFromList(&m_listReceivedPackets, bRemove);
}

//------------------------------------------------------------------------------

inline CRequestBind* CBinding::FindBindRequest(BOOL bRemove)
{
   return (CRequestBind*)FindRequestByType(NDT_REQUEST_BIND, bRemove);
}

inline CRequestUnbind* CBinding::FindUnbindRequest(BOOL bRemove)
{
   return (CRequestUnbind*)FindRequestByType(NDT_REQUEST_UNBIND, bRemove);
}

inline CRequestReset* CBinding::FindResetRequest(BOOL bRemove)
{
   return (CRequestReset*)FindRequestByType(NDT_REQUEST_RESET, bRemove);
}

inline CRequestRequest* CBinding::FindRequestRequest(BOOL bRemove)
{
   return (CRequestRequest*)FindRequestByType(NDT_REQUEST_REQUEST, bRemove);
}

inline CRequestSend* CBinding::FindSendRequest(BOOL bRemove)
{
   return (CRequestSend*)FindRequestByType(NDT_REQUEST_SEND, bRemove);
}

inline CRequestReceive* CBinding::FindReceiveRequest(BOOL bRemove)
{
   return (CRequestReceive*)FindRequestByType(NDT_REQUEST_RECEIVE, bRemove);
}

inline CRequestReceiveEx* CBinding::FindReceiveExRequest(BOOL bRemove)
{
   return (CRequestReceiveEx*)FindRequestByType(NDT_REQUEST_RECEIVE_EX, bRemove);
}

//------------------------------------------------------------------------------

#endif
