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
#include "NDP.h"

//------------------------------------------------------------------------------

#ifdef UNDER_CE
#include "celog.h"
#define NDP_USE_CELOG
#else
#include "ntddi.h"
#endif

//------------------------------------------------------------------------------

#ifdef NDP_USE_CELOG
#define CELOGUCHAR(c)   { \
   UCHAR cx=c; CELOGDATA(TRUE, CELID_RAW_UCHAR, &cx, 1, 1, CELZONE_MISC); \
}
#define CELOGLONGDATA(a, n)   CELOGDATA( \
   TRUE, CELID_RAW_LONG, (a), (WORD)((n) * sizeof(LONG)), 1, CELZONE_MISC \
)
#define CELOGULONGDATA(a, n)  CELOGDATA( \
   TRUE, CELID_RAW_ULONG, (a), (WORD)((n) * sizeof(ULONG)), 1, CELZONE_MISC \
)
#else
#define CELOGUCHAR(c)
#define CELOGLONGDATA(a, n)
#define CELOGULONGDATA(a, n)
#endif

//------------------------------------------------------------------------------

#define INITIALIZE_LIST(pList) {                                              \
   (pList)->pFirstPacket = (pList)->pLastPacket = NULL;                       \
   NdisAllocateSpinLock(&(pList)->spinLock);                                  \
   NdisInitializeEvent(&(pList)->hEvent);                                     \
}

//------------------------------------------------------------------------------

#define FREE_LIST(pList) {                                                    \
   ASSERT((pList)->pFirstPacket == NULL && (pList)->pLastPacket == NULL);     \
   NdisFreeSpinLock(&(pList)->spinLock);                                      \
   NdisFreeEvent(&(pList)->hEvent);                                           \
}

//------------------------------------------------------------------------------

#define INSERT_TAIL_LIST(pList, pPacket) {                                    \
   ASSERT(*(NDIS_PACKET**)pPacket->ProtocolReserved == NULL);                 \
   NdisAcquireSpinLock(&(pList)->spinLock);                                   \
   if ((pList)->pLastPacket == NULL) {                                        \
      (pList)->pFirstPacket = (pList)->pLastPacket = pPacket;                 \
      NdisSetEvent(&(pList)->hEvent);                                         \
   } else {                                                                   \
      *(NDIS_PACKET**)(pList)->pLastPacket->ProtocolReserved = pPacket;       \
      (pList)->pLastPacket = pPacket;                                         \
   }                                                                          \
   NdisReleaseSpinLock(&(pList)->spinLock);                                   \
}

//------------------------------------------------------------------------------

#define REMOVE_HEAD_LIST(pList, ppPacket) {                                   \
   NdisAcquireSpinLock(&(pList)->spinLock);                                   \
   *(ppPacket) = (pList)->pFirstPacket;                                       \
   if (*(ppPacket) != NULL) {                                                 \
      (pList)->pFirstPacket = *(NDIS_PACKET**)(*(ppPacket))->ProtocolReserved;\
      if ((pList)->pFirstPacket == NULL) (pList)->pLastPacket = NULL;         \
      *(NDIS_PACKET**)(*(ppPacket))->ProtocolReserved = NULL;                 \
      if ((pList)->pFirstPacket == NULL) NdisResetEvent(&(pList)->hEvent);    \
   }                                                                          \
   NdisReleaseSpinLock(&(pList)->spinLock);                                   \
}

//------------------------------------------------------------------------------

NDIS_MEDIUM g_aNdisMedium[] = { NdisMedium802_3 };

UCHAR g_aPacketHeader[] = { 
   0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x81, 0x37, 'N', 'D', 'P', 'X'
};

//------------------------------------------------------------------------------

VOID FreePackets(
   NDP_PACKET_QUEUE *pQueue, NDIS_HANDLE *phPacketPool, 
   NDIS_HANDLE *phBufferPool, BOOLEAN fFreeMemory
);
BOOL ndp_CloseAdapter(NDP_ADAPTER* pAdapter);

//------------------------------------------------------------------------------

#ifndef UNDER_CE

ULONG g_ulIdleCount = 0;
ULONG g_ulKNU = 0;
ULONG g_ulIndex = 0;

DWORD GetIdleTime ()
{
	NdisGetCurrentProcessorCounts(&g_ulIdleCount,&g_ulKNU,&g_ulIndex);
	return (DWORD) g_ulIdleCount;
}

//run at IRQL < DISPATCH_LEVEL
extern VOID NdisMSleep(ULONG MicrosecondsToSleep);

void Sleep(DWORD dwMilliseconds)
{
	NdisMSleep(dwMilliseconds * 1000);
}

VOID NdisFreeEvent(PNDIS_EVENT Event)
{
}

#endif

//------------------------------------------------------------------------------

NDIS_STATUS QueryOid(
   NDP_ADAPTER* pAdapter, NDIS_OID oid, PVOID pBuffer, PDWORD psize
) {
   NDIS_STATUS status;
   NDIS_REQUEST request;

   // Prepare request
   NdisZeroMemory(&request, sizeof(request));
   request.RequestType = NdisRequestQueryInformation;
   request.DATA.QUERY_INFORMATION.Oid = oid;
   request.DATA.QUERY_INFORMATION.InformationBuffer = pBuffer;
   request.DATA.QUERY_INFORMATION.InformationBufferLength = *psize;
   NdisResetEvent(&pAdapter->hPendingEvent);

   // Send it to adapter & wait if pending
   NdisRequest(&status, pAdapter->hAdapter, &request);
   if (status == NDIS_STATUS_PENDING) {
      NdisWaitEvent(&pAdapter->hPendingEvent, 0);
      status = pAdapter->status;
   }

   if (status == NDIS_STATUS_SUCCESS)
   {
	   //Lets note down the actual bytes written.
	   *psize = request.DATA.QUERY_INFORMATION.BytesWritten;
   }
   
   if ((status == NDIS_STATUS_INVALID_LENGTH) ||
	   (status == NDIS_STATUS_BUFFER_TOO_SHORT)
	   )
   {
	   //Lets note down the actual bytes required.
	   *psize = request.DATA.QUERY_INFORMATION.BytesNeeded;
   }

   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS SetOid(
   NDP_ADAPTER* pAdapter, NDIS_OID oid, PVOID pBuffer, PDWORD psize
) {
   NDIS_STATUS status;
   NDIS_REQUEST request;

   // Prepare request
   NdisZeroMemory(&request, sizeof(request));
   request.RequestType = NdisRequestSetInformation;
   request.DATA.QUERY_INFORMATION.Oid = oid;
   request.DATA.QUERY_INFORMATION.InformationBuffer = pBuffer;
   request.DATA.QUERY_INFORMATION.InformationBufferLength = *psize;
   NdisResetEvent(&pAdapter->hPendingEvent);

   // Send it to adapter & wait if pending
   NdisRequest(&status, pAdapter->hAdapter, &request);
   if (status == NDIS_STATUS_PENDING) {
      NdisWaitEvent(&pAdapter->hPendingEvent, 0);
      status = pAdapter->status;
   }

   if (status == NDIS_STATUS_SUCCESS)
   {
	   //Lets note down the actual bytes read.
	   *psize = request.DATA.SET_INFORMATION.BytesRead;
   }
   
   if ((status == NDIS_STATUS_INVALID_LENGTH) ||
	   (status == NDIS_STATUS_BUFFER_TOO_SHORT)
	   )
   {
	   //Lets note down the actual bytes required.
	   *psize = request.DATA.SET_INFORMATION.BytesNeeded;
   }

   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS AllocatePackets(
   NDP_PACKET_QUEUE *pQueue, NDIS_HANDLE *phPacketPool, 
   NDIS_HANDLE *phBufferPool, UINT count, PVOID pvBuffer, DWORD size
) {
   NDIS_STATUS status;
   NDIS_PACKET *pPacket;
   NDIS_BUFFER *pBuffer;
   BOOLEAN fAllocate = (pvBuffer == NULL);
   UINT i;
   
   // Allocate packet pool
   NdisAllocatePacketPool(&status, phPacketPool, count, 4*sizeof(PVOID));
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // Allocate a buffer pool (only one buffer per packet)
   NdisAllocateBufferPool(&status, phBufferPool, count);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   for (i = 0; i < count; i++) {
      // Allocate packet body
      if (fAllocate) NdisAllocateMemoryWithTag(
         &pvBuffer, size, NDP_PACKET_BODY_COOKIE
      );
      // Allocate buffer
      NdisAllocateBuffer(&status, &pBuffer, *phBufferPool, pvBuffer, size);
      if (status != NDIS_STATUS_SUCCESS) {
         if (fAllocate) NdisFreeMemory(pvBuffer, size, 0);
         goto cleanUp;
      }
      // Allocate packet
      NdisAllocatePacket(&status, &pPacket, *phPacketPool);
      if (status != NDIS_STATUS_SUCCESS) {
         if (fAllocate) NdisFreeMemory(pvBuffer, size, 0);
         NdisFreeBuffer(pBuffer);
         goto cleanUp;
      }
      // Chain buffer to packet
      NdisChainBufferAtFront(pPacket, pBuffer);
      // This flag is used to diferentiate between protocol & miniport packets
      pPacket->ProtocolReserved[4] = 0;
      // Insert packet to list
      INSERT_TAIL_LIST(pQueue, pPacket);
   }

cleanUp:
   if (status != NDIS_STATUS_SUCCESS) FreePackets(
      pQueue, phPacketPool, phBufferPool, fAllocate
   );
   return status;
}

//------------------------------------------------------------------------------

VOID FreePackets(
   NDP_PACKET_QUEUE *pQueue, NDIS_HANDLE *phPacketPool, 
   NDIS_HANDLE *phBufferPool, BOOLEAN fFreeBody
) {
   NDIS_PACKET *pPacket;
   NDIS_BUFFER *pBuffer;
   PVOID pvBuffer;
   UINT size;
   
   // Release all packets & its buffers in list
   for(;;) {
      REMOVE_HEAD_LIST(pQueue, &pPacket);
      if (pPacket == NULL) break;
      // Unchain and release all buffers from packet
      do {
         NdisUnchainBufferAtFront(pPacket, &pBuffer);
         if (pBuffer != NULL) {
            NdisQueryBuffer(pBuffer, &pvBuffer, &size);
            if (fFreeBody) NdisFreeMemory(pvBuffer, size, 0);
            NdisFreeBuffer(pBuffer);
         }
      } while (pBuffer != NULL);
      // Release packet
      NdisFreePacket(pPacket);
   }
   // Release buffer pool      
   if (*phBufferPool != NULL) {
      NdisFreeBufferPool(*phBufferPool);
      *phBufferPool = NULL;
   }
   // Release packet pool
   if (*phPacketPool != NULL) {
      NdisFreePacketPool(*phPacketPool);
      *phPacketPool = NULL;
   }
}

//------------------------------------------------------------------------------

VOID ndp_BindAdapter(
   OUT PNDIS_STATUS pStatus, NDIS_HANDLE hBind, PNDIS_STRING pnsDeviceName, 
   PVOID pvSystemSpecific1, PVOID pvSystemSpecific2
) {
   *pStatus = NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

VOID ndp_UnbindAdapter(
   OUT PNDIS_STATUS pStatus, IN NDIS_HANDLE hBinding, IN NDIS_HANDLE hUnbind
) {
   *pStatus = NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

VOID ndp_OpenAdapterComplete(
   IN NDIS_HANDLE hBinding, IN NDIS_STATUS status, 
   IN NDIS_STATUS openErrorStatus
) {
   NDP_ADAPTER* pAdapter = (NDP_ADAPTER*)hBinding;

   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   return;

   // Save open status
   pAdapter->status = status;

   // Set event
   NdisSetEvent(&pAdapter->hPendingEvent);
}

//------------------------------------------------------------------------------

VOID ndp_CloseAdapterComplete(
   IN NDIS_HANDLE hBinding, IN NDIS_STATUS status
) {
   NDP_ADAPTER* pAdapter = (NDP_ADAPTER*)hBinding;

   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   return;

   // Save close status
   pAdapter->status = status;

   // Set event
   NdisSetEvent(&pAdapter->hPendingEvent);
}

//------------------------------------------------------------------------------

NDIS_STATUS ndp_Receive(
   IN NDIS_HANDLE hBinding, IN NDIS_HANDLE hMacReceive,
   IN PVOID pvHeaderBuffer, IN UINT headerBufferSize, 
   IN PVOID pvLookAheadBuffer, IN UINT lookaheadBufferSize, IN UINT packetSize
) {
   NDP_ADAPTER* pAdapter = (NDP_ADAPTER*)hBinding;
   NDIS_STATUS status = NDIS_STATUS_NOT_ACCEPTED;
   NDIS_PACKET *pPacket;
   NDIS_BUFFER *pBuffer,*pTempBuffer;
   VOID *pvBuffer;
   UINT bufferSize, totalBufferSize, bytesTransferred;

   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   goto cleanUp;

   // We should be at receive mode (ok, this can be redundant, but...)
   if (!pAdapter->fReceiveMode) goto cleanUp;
   
   // If we don't get at least header ignore packet
   if (lookaheadBufferSize < sizeof(g_aPacketHeader)) goto cleanUp;

   // Only packets with our header are accepted
   if (!NdisEqualMemory(
      pvLookAheadBuffer, &g_aPacketHeader, sizeof(g_aPacketHeader)
   )) goto cleanUp;

   // If stress mode is active and first packet was received save time
   if (pAdapter->fStressMode && pAdapter->startTime == 0) {
      NdisGetSystemUpTime(&pAdapter->startTime);
      pAdapter->startIdleTime = GetIdleTime();
   }

   // Get packet from free packets queue
   REMOVE_HEAD_LIST(&pAdapter->recvQueue, &pPacket);

   // If there is no packet avaiable ignore incoming packet
   if (pPacket == NULL) goto cleanUp;

   // We have new packet
   NdisInterlockedIncrement((PLONG)&pAdapter->packetsInReceive);   

   // Query packet
   NdisGetFirstBufferFromPacket(
      pPacket, &pBuffer, &pvBuffer, &bufferSize, &totalBufferSize
   );

   // If packet doesn't fit to buffer ignore it. Theoreticaly totalBufferSize
   // should be used in condition, but it will make code more complicated and
   // it will not happen (we are allocate packets with only one buffer).
   if (bufferSize < packetSize) {
      // Return packet back between free packets
      NdisAdjustBufferLength(pBuffer, pAdapter->maxTotalSize);
      NdisRecalculatePacketCounts(pPacket);
      INSERT_TAIL_LIST(&pAdapter->recvQueue, pPacket);
      goto cleanUp;
   }
   
   // Copy media header to packet
   NdisMoveMemory(pvBuffer, pvHeaderBuffer, headerBufferSize);

   // Copy protocol header to packet
   NdisCopyLookaheadData(
      (UCHAR*)pvBuffer + headerBufferSize, pvLookAheadBuffer, 
      lookaheadBufferSize, pAdapter->macOptions
   );

   // Adjust buffer length
   NdisAdjustBufferLength(pBuffer, headerBufferSize + lookaheadBufferSize);
   NdisRecalculatePacketCounts(pPacket);

   // If there is more data 
   if (packetSize > lookaheadBufferSize) {
      // We should ask for a packet body
	  
	   //*********************************************************************************************
	  //I know allocating buffer descriptor is time consuming, critical especially if this protocol 
	  //driver is going to be used for finding performance. But since the original design has flaw and
	   // to work around the flaw in the given time is to use cheapest way.
	   //Note that since the miniport driver is showing part of data and then later transferring the rest of data
	   // is itself time consuming so allocating and deallocating buffer on the fly is OK in comparision with that.
	   //*********************************************************************************************
      
	  UINT iSize = pAdapter->maxTotalSize - (lookaheadBufferSize + headerBufferSize);
	  PBYTE pbBuff = (UCHAR*)pvBuffer + headerBufferSize + lookaheadBufferSize;
	  
	  if (iSize < (packetSize - lookaheadBufferSize))
	  {
         NdisAdjustBufferLength(pBuffer, pAdapter->maxTotalSize);
         NdisRecalculatePacketCounts(pPacket);
         INSERT_TAIL_LIST(&pAdapter->recvQueue, pPacket);
	     goto cleanUp;
	  }

	  iSize = packetSize - lookaheadBufferSize;
	  NdisAllocateBuffer(&status, &pTempBuffer,pAdapter->hRecvBufferPool,pbBuff,iSize);
	  ASSERT(pTempBuffer != NULL);
	  if (status != NDIS_STATUS_SUCCESS) {
		  //NdisFreeBuffer(pTempBuffer);
		  NdisAdjustBufferLength(pBuffer, pAdapter->maxTotalSize);
		  NdisRecalculatePacketCounts(pPacket);
		  INSERT_TAIL_LIST(&pAdapter->recvQueue, pPacket);
		  goto cleanUp;
      }
	  NdisChainBufferAtFront(pPacket,pTempBuffer);
	  //Assert that data can be contained in iSize.

      NdisTransferData(
         &status, pAdapter->hAdapter, hMacReceive, lookaheadBufferSize,// + 1,
         packetSize - lookaheadBufferSize, pPacket, &bytesTransferred
      );
      // If transfer is pending final 
      if (status == NDIS_STATUS_PENDING) {
         status = NDIS_STATUS_SUCCESS;
         goto cleanUp;
      }
      // Check transfer result
      if (status != NDIS_STATUS_SUCCESS) {
		 NdisUnchainBufferAtFront(pPacket,&pTempBuffer);
		 NdisFreeBuffer(pTempBuffer);
         NdisAdjustBufferLength(pBuffer, pAdapter->maxTotalSize);
         NdisRecalculatePacketCounts(pPacket);
         INSERT_TAIL_LIST(&pAdapter->recvQueue, pPacket);
         goto cleanUp;     
      }
	  NdisUnchainBufferAtFront(pPacket,&pTempBuffer);
	  NdisFreeBuffer(pTempBuffer);

      // Fix packet length
      NdisAdjustBufferLength(
         pBuffer, headerBufferSize + lookaheadBufferSize + bytesTransferred
      );
      NdisRecalculatePacketCounts(pPacket);
   }

   // Insert packet to list of received
   INSERT_TAIL_LIST(&pAdapter->workQueue, pPacket);

   // We are done with packet
   status = NDIS_STATUS_SUCCESS;
   
cleanUp:
   return status;
}

//------------------------------------------------------------------------------

VOID ndp_TransferDataComplete(
   IN NDIS_HANDLE hBinding, IN PNDIS_PACKET pPacket, IN NDIS_STATUS status, 
   IN UINT bytesTransferred
) {
   NDP_ADAPTER *pAdapter = (NDP_ADAPTER*)hBinding;
   NDIS_BUFFER *pBuffer, *pTempBuffer;
   UINT packetLength;

   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   return;

   // Query current packet length
   NdisQueryPacket(pPacket, NULL, NULL, &pTempBuffer, &packetLength);
   NdisGetNextBuffer(pTempBuffer,&pBuffer);

   ASSERT((pBuffer!=NULL)&&(pTempBuffer!=NULL));

   NdisUnchainBufferAtFront(pPacket,&pTempBuffer);
   NdisFreeBuffer(pTempBuffer);

   // Depending on transfer result
   if (status == NDIS_STATUS_SUCCESS) {
      // Fix packet length
      NdisAdjustBufferLength(pBuffer, packetLength); // + bytesTransferred);
      NdisRecalculatePacketCounts(pPacket);
      // Insert packet to list of received
      INSERT_TAIL_LIST(&pAdapter->workQueue, pPacket);
   } else {
      // Return packet between free
      NdisAdjustBufferLength(pBuffer, pAdapter->maxTotalSize);
      NdisRecalculatePacketCounts(pPacket);
      INSERT_TAIL_LIST(&pAdapter->recvQueue, pPacket);
   }
}

//------------------------------------------------------------------------------

INT ndp_ReceivePacket(IN NDIS_HANDLE hBinding, IN PNDIS_PACKET pPacket)
{
   NDP_ADAPTER *pAdapter = (NDP_ADAPTER*)hBinding;
   INT refCount = 0;
   NDIS_BUFFER *pBuffer;
   VOID *pvBuffer;
   UINT buffersCount, packetSize, bufferSize;

   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   return 0;

   // We should be at receive mode (ok, this can be redundant, but...)
   if (!pAdapter->fReceiveMode) goto cleanUp;
   
   // Get packet & first buffer info   
   NdisQueryPacket(pPacket, NULL, &buffersCount, &pBuffer, &packetSize);
   NdisQueryBuffer(pBuffer, &pvBuffer, &bufferSize);

   // If we don't get at least header ignore packet
   if (bufferSize < ETH_HEADER_SIZE + sizeof(g_aPacketHeader)) goto cleanUp;
   
   // Only packets with our header are accepted
   if (!NdisEqualMemory(
      (UCHAR*)pvBuffer + ETH_HEADER_SIZE, &g_aPacketHeader, 
      sizeof(g_aPacketHeader)
   )) goto cleanUp;

   // If stress mode is active and first packet was received save time
   if (pAdapter->fStressMode && pAdapter->startTime == 0) {
      NdisGetSystemUpTime(&pAdapter->startTime);
      pAdapter->startIdleTime = GetIdleTime();
   }

   // We have new packet in process
   NdisInterlockedIncrement((PLONG)&pAdapter->packetsInReceive);

   // Flag it as miniport packet
   pPacket->ProtocolReserved[4] = 1;

   // Insert packet to list of received
   INSERT_TAIL_LIST(&pAdapter->workQueue, pPacket);

   // And we are holding one packet reference
   refCount = 1;
   
cleanUp:
   return refCount;
}

//------------------------------------------------------------------------------

VOID ndp_ReceiveComplete(IN NDIS_HANDLE hBinding)
{
   NDP_ADAPTER* pAdapter = (NDP_ADAPTER*)hBinding;
   NDIS_PACKET *pPacket;
   NDIS_BUFFER *pBuffer;
   VOID *pvBuffer;
   UINT packetLength, packetType, bufferLength;
   
   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   return;

   // For all packets in queue
   for(;;) {

      // Get packet
      REMOVE_HEAD_LIST(&pAdapter->workQueue, &pPacket);
      // If there is no packet exit loop
      if (pPacket == NULL) break;

      // Get packet info and buffer
      NdisQueryPacket(pPacket, NULL, NULL, &pBuffer, &packetLength);
      NdisQueryBuffer(pBuffer, &pvBuffer, &bufferLength);

      // Get packet type
      NdisMoveMemory(
         &packetType, (UCHAR*)pvBuffer + ETH_HEADER_SIZE + 
         sizeof(g_aPacketHeader), sizeof(UINT)
      );

      // Depending on mode
      if (pAdapter->fStressMode) {
         if (packetType == pAdapter->packetType) {
            // Update counters
            pAdapter->packetsReceived++;
            pAdapter->bytesReceived += packetLength;
            // Set last receive time
            NdisGetSystemUpTime(&pAdapter->lastTime);
            pAdapter->lastIdleTime = GetIdleTime();
            // Return packet between free
            if (pPacket->ProtocolReserved[4] == 0) {
               NdisAdjustBufferLength(pBuffer, pAdapter->maxTotalSize);
               NdisRecalculatePacketCounts(pPacket);
               INSERT_TAIL_LIST(&pAdapter->recvQueue, pPacket);
            } else {
               NdisReturnPackets(&pPacket, 1);
            }   
            NdisInterlockedDecrement((PLONG)&pAdapter->packetsInReceive);
         } else {
            pAdapter->fStressMode = FALSE;
            INSERT_TAIL_LIST(&pAdapter->waitQueue, pPacket);
            NdisInterlockedDecrement((PLONG)&pAdapter->packetsInReceive);
         }
      } else {
         INSERT_TAIL_LIST(&pAdapter->waitQueue, pPacket);
         NdisInterlockedDecrement((PLONG)&pAdapter->packetsInReceive);
      }

   }
}

//------------------------------------------------------------------------------

VOID ndp_SendComplete(
   IN NDIS_HANDLE hBinding, IN PNDIS_PACKET pPacket, IN NDIS_STATUS status
) {
   NDP_ADAPTER *pAdapter = (NDP_ADAPTER*)hBinding;
   UINT packetLength;

   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   return;

   // Log send complete event
   CELOGULONGDATA((ULONG*)&pPacket, 1);

   // Update some counters if we are in stress mode
   if (pAdapter->fStressMode) {
      NdisQueryPacket(pPacket, NULL, NULL, NULL, &packetLength);
      NdisInterlockedIncrement((PLONG)&pAdapter->packetsSent);
      pAdapter->bytesSent += packetLength;
      NdisGetSystemUpTime(&pAdapter->lastTime);
      pAdapter->lastIdleTime = GetIdleTime();
   }   
      
   // Return packet to list
   INSERT_TAIL_LIST(&pAdapter->sendQueue, pPacket);
   NdisInterlockedDecrement((PLONG)&pAdapter->packetsInSend);

   NdisInterlockedDecrement(&pAdapter->sendCompleteCounter);
}

//------------------------------------------------------------------------------

VOID ndp_ResetComplete(
   IN NDIS_HANDLE hBinding, IN NDIS_STATUS status
) {
   NDP_ADAPTER* pAdapter = (NDP_ADAPTER*)hBinding;

   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   return;
}

//------------------------------------------------------------------------------

VOID ndp_RequestComplete(
   IN NDIS_HANDLE hBinding, IN PNDIS_REQUEST pNdisRequest, IN NDIS_STATUS status
) {
   NDP_ADAPTER* pAdapter = (NDP_ADAPTER*)hBinding;

   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   return;

   // Save request status
   pAdapter->status = status;

   // Set event
   NdisSetEvent(&pAdapter->hPendingEvent);
}

//------------------------------------------------------------------------------

VOID ndp_Status(
   IN NDIS_HANDLE hBinding, IN NDIS_STATUS generalStatus,
   IN PVOID pvStatusBuffer, IN UINT statusBufferSize
) {
   NDP_ADAPTER* pAdapter = (NDP_ADAPTER*)hBinding;

   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   return;
}

//------------------------------------------------------------------------------

VOID ndp_StatusComplete(
   IN NDIS_HANDLE hBinding
) {
   NDP_ADAPTER* pAdapter = (NDP_ADAPTER*)hBinding;

   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   return;
}

//------------------------------------------------------------------------------

NDIS_STATUS ndp_PnPEvent(
   IN NDIS_HANDLE hBinding, IN PNET_PNP_EVENT pNetPnPEvent
) {
   NDP_ADAPTER* pAdapter = (NDP_ADAPTER*)hBinding;

   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   return NDIS_STATUS_FAILURE;

   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

BOOL ndp_OpenAdapter(NDP_ADAPTER* pAdapter, LPCTSTR szAdapter)
{
   NDIS_STATUS status, openStatus;
   UINT mediumIndex, size;
   NDIS_STRING nsAdapter;
   DWORD dwBuffSize;

   // Initialize adapter name
   NdisInitUnicodeString(&nsAdapter, szAdapter);

   // Prepare event for pending
   NdisResetEvent(&pAdapter->hPendingEvent);
   
   // Open it (ok, this isn't officialy supported)
   NdisOpenAdapter(
      &status, &openStatus, &pAdapter->hAdapter, &mediumIndex,
      g_aNdisMedium, sizeof(g_aNdisMedium)/sizeof(NDIS_MEDIUM),
      pAdapter->pProtocol->hProtocol, (NDIS_HANDLE)pAdapter, &nsAdapter, 0, NULL
   );
   if (status == NDIS_STATUS_PENDING) {
      NdisWaitEvent(&pAdapter->hPendingEvent, 0);
      status = pAdapter->status;
   }

   // Check result for open
   if (status != NDIS_STATUS_SUCCESS) {
      pAdapter->hAdapter = NULL;
      goto cleanUp;
   }

   // Query packet maximum frame size
   dwBuffSize = sizeof(UINT);
   status = QueryOid(
      pAdapter, OID_GEN_MAXIMUM_TOTAL_SIZE, &pAdapter->maxTotalSize,
      &dwBuffSize
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // Get source address from adapter
   dwBuffSize = ETH_ADDR_SIZE;
   status = QueryOid(
      pAdapter, OID_802_3_CURRENT_ADDRESS, pAdapter->srcAddr, &dwBuffSize
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // Query how many packets can be send by NdisSendPackets call
   dwBuffSize = sizeof(UINT);
   status = QueryOid(
      pAdapter, OID_GEN_MAXIMUM_SEND_PACKETS, &pAdapter->maxSendPackets, 
      &dwBuffSize
   );
   if (status != NDIS_STATUS_SUCCESS) {
      pAdapter->maxSendPackets = 1;
      status = NDIS_STATUS_SUCCESS;
   }

   // Set minimal lookhead buffer size we want to get on receive
   size = sizeof(g_aPacketHeader) + sizeof(UINT);
   dwBuffSize = sizeof(UINT);
   status = SetOid(pAdapter, OID_GEN_CURRENT_LOOKAHEAD, &size, &dwBuffSize);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   
cleanUp:
   if (status != NDIS_STATUS_SUCCESS) ndp_CloseAdapter(pAdapter);
   return (status == NDIS_STATUS_SUCCESS);
}

//------------------------------------------------------------------------------

BOOL ndp_CloseAdapter(NDP_ADAPTER* pAdapter)
{
   BOOL ok = FALSE;
   NDIS_STATUS status;

   // Check if adapter is opened   
   if (pAdapter->hAdapter != NULL) {
      // Prepare event for pending
      NdisResetEvent(&pAdapter->hPendingEvent);
      // Close adapter
      NdisCloseAdapter(&status, pAdapter->hAdapter);
      if (status == NDIS_STATUS_PENDING) {
         NdisWaitEvent(&pAdapter->hPendingEvent, 0);
         status = pAdapter->status;
      }
      if (status == NDIS_STATUS_SUCCESS) pAdapter->hAdapter = NULL;
      // Result depend on status
      ok = (status == NDIS_STATUS_SUCCESS);
   }

   return ok;
}

//------------------------------------------------------------------------------

BOOL ndp_SendPacket(NDP_ADAPTER* pAdapter, NDP_SEND_PACKET_INP* pInp)
{
   BOOL ok = FALSE;
   NDIS_STATUS status;
   NDIS_PACKET *pPacket;
   VOID *pvBuffer = NULL;
   UINT size, offset;

  
   // Get required packet size and check if it fit to media
   size = ETH_HEADER_SIZE + sizeof(g_aPacketHeader) + sizeof(UINT);
   size += pInp->dataSize;
   if (size > pAdapter->maxTotalSize) goto cleanUp;

   // Alocate packet body
   NdisAllocateMemoryWithTag(&pvBuffer, size, NDP_PACKET_BODY_COOKIE);
   if (pvBuffer == NULL) goto cleanUp;

   // Build packet
   offset = 0;
   // Destination address
   NdisMoveMemory((UCHAR*)pvBuffer + offset, pInp->destAddr, ETH_ADDR_SIZE);
   offset += ETH_ADDR_SIZE;
   // Source address
   NdisMoveMemory((UCHAR*)pvBuffer + offset, pAdapter->srcAddr, ETH_ADDR_SIZE);
   offset += ETH_ADDR_SIZE;
   // Packet size
   ((UCHAR*)pvBuffer)[offset++] = (UCHAR)(size >> 8);
   ((UCHAR*)pvBuffer)[offset++] = (UCHAR)size;
   // Packet header
   NdisMoveMemory(
      (UCHAR*)pvBuffer + offset, g_aPacketHeader, sizeof(g_aPacketHeader)
   );
   offset += sizeof(g_aPacketHeader);
   // Packet type
   NdisMoveMemory((UCHAR*)pvBuffer + offset, &pInp->packetType, sizeof(UINT));
   offset += sizeof(UINT);
   // Packet body
   if (pInp->dataSize > 0) NdisMoveMemory(
      (UCHAR*)pvBuffer + offset, pInp->data, pInp->dataSize
   );
   offset += pInp->dataSize;

   // Allocate packet
   status = AllocatePackets(
      &pAdapter->sendQueue, &pAdapter->hSendPacketPool, 
      &pAdapter->hSendBufferPool, 1, pvBuffer, size
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // Get first and only packet in queue
   REMOVE_HEAD_LIST(&pAdapter->sendQueue, &pPacket);

   // This should never happen
   if (pPacket == NULL) goto cleanUp;

   // Send packet
   NdisInterlockedIncrement((PLONG)&pAdapter->packetsInSend);
   NdisSend(&status, pAdapter->hAdapter, pPacket);
   if (status != NDIS_STATUS_PENDING) {
      ndp_SendComplete((HANDLE)pAdapter, pPacket, status);
   }

   // Wait until it is sent
   NdisWaitEvent(&pAdapter->sendQueue.hEvent, 0);

   // We end succesfully
   ok = TRUE;
   
cleanUp:
   FreePackets(
      &pAdapter->sendQueue, &pAdapter->hSendPacketPool, 
      &pAdapter->hSendBufferPool, FALSE
   );
   if (pvBuffer != NULL) NdisFreeMemory(pvBuffer, size, 0);
   return ok;
}

//------------------------------------------------------------------------------

BOOL ndp_Listen(NDP_ADAPTER* pAdapter, NDP_LISTEN_INP* pInp)
{
   BOOL ok = FALSE;
   NDIS_STATUS status;
   UINT filter = 0;
   NDIS_PACKET *pPacket;
   NDIS_BUFFER *pBuffer;
   DWORD dwBuffSize;

   if (!pAdapter->fReceiveMode) {
      
      status = AllocatePackets(
         &pAdapter->recvQueue, &pAdapter->hRecvPacketPool, 
         &pAdapter->hRecvBufferPool, pInp->poolSize, NULL, 
         pAdapter->maxTotalSize
      );
      if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

      pAdapter->packetType = 0;
      pAdapter->fReceiveMode = TRUE;

      // Set receive filter
      if (pInp->fDirected) filter |= NDIS_PACKET_TYPE_DIRECTED;
      if (pInp->fBroadcast) filter |= NDIS_PACKET_TYPE_BROADCAST;
	  dwBuffSize = sizeof(UINT);
      status = SetOid(
         pAdapter, OID_GEN_CURRENT_PACKET_FILTER, &filter, &dwBuffSize
      );
      if (status != NDIS_STATUS_SUCCESS) goto cleanUp;


   } else {

      // Set receive filter to nothing
	  dwBuffSize = sizeof(UINT);
      status = SetOid(
         pAdapter, OID_GEN_CURRENT_PACKET_FILTER, &filter, &dwBuffSize
      );
      if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

      pAdapter->fReceiveMode = FALSE;
      pAdapter->packetType = 0;

      // Remove all received packets waiting in queue
      while ((ULONG)pAdapter->packetsInReceive > 0) NdisMSleep(10000);

      for(;;) {
         REMOVE_HEAD_LIST(&pAdapter->waitQueue, &pPacket);
         if (pPacket == NULL) break;
         if (pPacket->ProtocolReserved[4] == 0) {
            NdisQueryPacket(pPacket, NULL, NULL, &pBuffer, NULL);
            NdisAdjustBufferLength(pBuffer, pAdapter->maxTotalSize);
            NdisRecalculatePacketCounts(pPacket);
            INSERT_TAIL_LIST(&pAdapter->recvQueue, pPacket);
         } else {
            NdisReturnPackets(&pPacket, 1);
         }
      }

      // Free packets
      FreePackets(
         &pAdapter->recvQueue, &pAdapter->hRecvPacketPool, 
         &pAdapter->hRecvBufferPool, TRUE
      );
   }
   
   ok = TRUE;   

cleanUp:
   return ok;
}

//------------------------------------------------------------------------------

BOOL ndp_RecvPacket(
   NDP_ADAPTER* pAdapter, NDP_RECV_PACKET_INP* pInp, NDP_RECV_PACKET_OUT* pOut
) {
   BOOL ok = FALSE;
   NDIS_PACKET *pPacket = NULL;
   NDIS_BUFFER *pBuffer = NULL;
   VOID *pvBuffer;
   UINT size, offset, bufferSize, totalBufferSize;


   // First check if adapter is in correct state
   if (!pAdapter->fReceiveMode || pAdapter->fStressMode) goto cleanUp;

   // Try get packet
   REMOVE_HEAD_LIST(&pAdapter->waitQueue, &pPacket);

   // If there isn't packet wait
   if (pPacket == NULL) {
      if (NdisWaitEvent(&pAdapter->waitQueue.hEvent, pInp->timeout)) {
         REMOVE_HEAD_LIST(&pAdapter->waitQueue, &pPacket);
      }
   }

   // If there is no packet at this time fail request
   if (pPacket == NULL) goto cleanUp;

   // Query packet
   NdisGetFirstBufferFromPacket(
      pPacket, &pBuffer, &pvBuffer, &bufferSize, &totalBufferSize
   );

   // If this happen something is very bad
   if (bufferSize < totalBufferSize) goto cleanUp;

   // Save source address
   NdisMoveMemory(
      &pOut->srcAddr, (UCHAR*)pvBuffer + ETH_ADDR_SIZE, ETH_ADDR_SIZE
   );
   
   // Get size of data part
   offset = ETH_HEADER_SIZE + sizeof(g_aPacketHeader);

   // Again something bad happen
   if ((offset + sizeof(UINT)) > bufferSize) goto cleanUp;
   
   // Move packet type
   NdisMoveMemory(&pOut->packetType, (UCHAR*)pvBuffer + offset, sizeof(UINT));
   offset += sizeof(UINT);

   // Data part size (yes, we assume that packet has only one buffer...)
   size = bufferSize - offset;

   // Fix data size (ok, 
   if (size > pOut->dataSize) size = pOut->dataSize;

   if (size > 0) NdisMoveMemory(&pOut->data, (UCHAR*)pvBuffer + offset, size);
   pOut->dataSize = size;

   ok = TRUE;
   
cleanUp:

   // Return packet to packet queue
   if (pPacket != NULL) {
      if (pPacket->ProtocolReserved[4] == 0) {
         NdisAdjustBufferLength(pBuffer, pAdapter->maxTotalSize);
         NdisRecalculatePacketCounts(pPacket);
         INSERT_TAIL_LIST(&pAdapter->recvQueue, pPacket);
      } else {
         NdisReturnPackets(&pPacket, 1);
      }
   }      

   return ok;
}

//------------------------------------------------------------------------------

BOOL ndp_StressSend(
   NDP_ADAPTER* pAdapter, NDP_STRESS_SEND_INP* pInp, NDP_STRESS_SEND_OUT* pOut
) {
   BOOL ok = FALSE;
   NDIS_STATUS status;
   NDIS_PACKET *pPacket = NULL, **apPacket = NULL;
   UCHAR *pPacketBody = NULL;
   UINT size, sent, count, i;


   // Check improper state
   if (pAdapter->fReceiveMode) goto cleanUp;
  
   // Allocate array for NDIS_PACKET* used for NdisSendPackets
   size = pAdapter->maxSendPackets * sizeof(NDIS_PACKET*);
   NdisAllocateMemoryWithTag((VOID**)&apPacket, size, NDP_PACKET_ARRAY_COOKIE);
   if (apPacket == NULL) goto cleanUp;

   // Fix required packet size
   size = ETH_HEADER_SIZE + sizeof(g_aPacketHeader) + sizeof(UINT);
   if (pInp->packetSize < size) {
      pInp->packetSize = size;
   } else if (pInp->packetSize > pAdapter->maxTotalSize) {
      pInp->packetSize = pAdapter->maxTotalSize;
   }
   
   // Alocate packet body
   NdisAllocateMemoryWithTag(
      (VOID**)&pPacketBody, pInp->packetSize, NDP_PACKET_BODY_COOKIE
   );
   if (pPacketBody == NULL) goto cleanUp;

   // Set destination and source addresses
   NdisMoveMemory(pPacketBody, pInp->dstAddr, ETH_ADDR_SIZE);
   count = ETH_ADDR_SIZE;
   NdisMoveMemory(pPacketBody + count, pAdapter->srcAddr, ETH_ADDR_SIZE);
   count += ETH_ADDR_SIZE;
   
   // Set packet size
   pPacketBody[12] = (UCHAR)(pInp->packetSize >> 8);
   pPacketBody[13] = (UCHAR)pInp->packetSize;
   count += 2;
   
   // Copy packet protocol header
   NdisMoveMemory(
      pPacketBody + count, g_aPacketHeader, sizeof(g_aPacketHeader)
   );
   count += sizeof(g_aPacketHeader);
   
   // First char define packet type 
   NdisMoveMemory(pPacketBody + count, &pInp->packetType, sizeof(UINT));
   count += sizeof(pInp->packetType);
   
   // Put some date to packet rest
   while (count < pInp->packetSize) {
      pPacketBody[count] = (UCHAR)count;
      count++;
   }

   // Allocate packets pools etc.
   status = AllocatePackets(
      &pAdapter->sendQueue, &pAdapter->hSendPacketPool, 
      &pAdapter->hSendBufferPool, pInp->poolSize, pPacketBody, pInp->packetSize
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // Clear numbers
   pAdapter->packetsSent = 0;
   pAdapter->bytesSent = 0;

   // Start stress run
   pAdapter->fStressMode = TRUE;
   
   // Get start time
   NdisGetSystemUpTime(&pAdapter->startTime);
   pAdapter->startIdleTime = GetIdleTime();

   // Send packets
   sent = 0; 
   while (sent < pInp->packetsSend) {
	   UINT uiBurstPackets;
	   UINT uiCountsToCompare;

	   if (pInp->FlagControl)
		   uiBurstPackets = pInp->packetsInABurst;
	   else
		   uiBurstPackets = pAdapter->maxSendPackets;

	   while ( (uiBurstPackets > 0) && 
		       (sent < pInp->packetsSend)
			 )
	   {
		   if (uiBurstPackets < pAdapter->maxSendPackets)
			   uiCountsToCompare = uiBurstPackets;
		   else
			   uiCountsToCompare = pAdapter->maxSendPackets;
		   
		   count = 0;
		   
		   // Get as much packets as possible
		   while (
			   count < uiCountsToCompare && (sent + count) < pInp->packetsSend
			   ) {
			   REMOVE_HEAD_LIST(&pAdapter->sendQueue, &pPacket);
			   if (pPacket == NULL) break;
			   NdisInterlockedIncrement((PLONG)&pAdapter->packetsInSend);
			   apPacket[count++] = pPacket;
		   }
		   
		   if (count > 0) {
			   
			   if (pInp->fSendOnly) {
				   for (i = 0; i < count; i++) {
					   CELOGLONGDATA((LONG*)&apPacket[i], 1);
					   CELOGUCHAR(0x02);
					   NdisSend(&status, pAdapter->hAdapter, apPacket[i]);
					   CELOGUCHAR(0x82);
					   if (status != NDIS_STATUS_PENDING) {
						   ndp_SendComplete(pAdapter, apPacket[i], status);
					   }
				   }
			   } else {
				   CELOGLONGDATA((LONG*)apPacket, count);
				   CELOGUCHAR(0x02);
				   NdisSendPackets(pAdapter->hAdapter, apPacket, count);
				   CELOGUCHAR(0x82);
			   }
			   sent += count;
			   uiBurstPackets = uiBurstPackets - count;

		   } else {
			   
			   // Wait for packet be returned to queue
			   CELOGUCHAR(0x03);
			   NdisWaitEvent(&pAdapter->sendQueue.hEvent, 0);
			   CELOGUCHAR(0x83);
		   }
	   }
	   if (pInp->FlagControl)
		   Sleep(pInp->delayInABurst);
   }

   ok = TRUE;
   
cleanUp:

   // Wait for no packet in use
   while ((ULONG)pAdapter->packetsInSend > 0)  NdisMSleep(1000);

   ASSERT(pAdapter->packetsSent == pInp->packetsSend);
   
   pAdapter->fStressMode = FALSE;

   // Release packets & buffers
   FreePackets(
      &pAdapter->sendQueue, &pAdapter->hSendPacketPool, 
      &pAdapter->hSendBufferPool, FALSE
   );

   // And packet body
   if (pPacketBody != NULL) NdisFreeMemory(pPacketBody, pInp->packetSize, 0);

   // Finaly array used for packet sent
   if (apPacket != NULL) NdisFreeMemory(
      apPacket, pAdapter->maxSendPackets * sizeof(NDIS_PACKET*), 0
   );

   // Set results
   if (ok) {
      pOut->time = pAdapter->lastTime - pAdapter->startTime;
      pOut->idleTime = pAdapter->lastIdleTime - pAdapter->startIdleTime;
      pOut->packetsSent = pAdapter->packetsSent;
      pOut->bytesSent = pAdapter->bytesSent;
   }
   
   return ok;
}

//------------------------------------------------------------------------------

BOOL ndp_StressReceive(
   NDP_ADAPTER* pAdapter, NDP_STRESS_RECV_INP* pInp, NDP_STRESS_RECV_OUT* pOut
) {
   BOOL ok = FALSE;
   NDIS_STATUS status;
   NDIS_PACKET *pPacket = NULL;
   NDIS_BUFFER *pBuffer = NULL;
   VOID *pvBuffer;
   UINT filter, size, offset, bufferSize, totalBufferSize;
   DWORD dwBuffSize;


   // Check improper state
   if (pAdapter->fReceiveMode) goto cleanUp;

   // Allocate packets, buffers & other stuff
   status = AllocatePackets(
      &pAdapter->recvQueue, &pAdapter->hRecvPacketPool, 
      &pAdapter->hRecvBufferPool, pInp->poolSize, NULL, pAdapter->maxTotalSize
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // Stress receive ends with this packet
   pAdapter->packetType = pInp->packetType;
   
   // Clear numbers
   pAdapter->packetsReceived = 0;
   pAdapter->bytesReceived = 0;
   pAdapter->startTime = 0;
   pAdapter->startIdleTime = 0;
   
   // Set stress & receive mode
   pAdapter->fStressMode = TRUE;
   pAdapter->fReceiveMode = TRUE;

   // Enable direct packets receiving
   filter = NDIS_PACKET_TYPE_DIRECTED;
   dwBuffSize = sizeof(UINT);
   status = SetOid(
      pAdapter, OID_GEN_CURRENT_PACKET_FILTER, &filter, &dwBuffSize
   );

#define Minutes_5		(5 * 60 * 1000)

   // Wait until we don't receive final packet or 5 min. time expires.
   NdisWaitEvent(&pAdapter->waitQueue.hEvent,(UINT)Minutes_5);

   // Ok, no more receives
   pAdapter->fReceiveMode = FALSE;
   
   // Get packet which finish stress
   REMOVE_HEAD_LIST(&pAdapter->waitQueue, &pPacket);

   // This should not happen
   if (pPacket == NULL) goto cleanUp;

   // Query packet
   NdisGetFirstBufferFromPacket(
      pPacket, &pBuffer, &pvBuffer, &bufferSize, &totalBufferSize
   );
   
   // If this happen something is very bad
   if (bufferSize < totalBufferSize) goto cleanUp;

   // Save source address
   NdisMoveMemory(
      &pOut->srcAddr, (UCHAR*)pvBuffer + ETH_ADDR_SIZE, ETH_ADDR_SIZE
   );
   
   // Get size of data part
   offset = ETH_HEADER_SIZE + sizeof(g_aPacketHeader);
   
   // Again something bad happen
   if ((offset + sizeof(UINT)) > bufferSize) goto cleanUp;
   
   // Move packet type
   NdisMoveMemory(&pOut->packetType, (UCHAR*)pvBuffer + offset, sizeof(UINT));
   offset += sizeof(UINT);
   
   // Data part size (yes, we assume that packet has only one buffer...)
   size = bufferSize - offset;
   
   // Fix data size (ok, 
   if (size > pOut->dataSize) size = pOut->dataSize;
   
   if (size > 0) NdisMoveMemory(&pOut->data, (UCHAR*)pvBuffer + offset, size);
   pOut->dataSize = size;
   
   // When we are there it is success...
   ok = TRUE;
   
cleanUp:

   // If we hold packet return it to packet queue
   if (pPacket != NULL) {
      if (pPacket->ProtocolReserved[4] == 0) {
         NdisAdjustBufferLength(pBuffer, pAdapter->maxTotalSize);
         NdisRecalculatePacketCounts(pPacket);
         INSERT_TAIL_LIST(&pAdapter->recvQueue, pPacket);
      } else {
         NdisReturnPackets(&pPacket, 1);
      }
   }

   // Release all packets & its buffers in list
   FreePackets(
      &pAdapter->recvQueue, &pAdapter->hRecvPacketPool, 
      &pAdapter->hRecvBufferPool, TRUE
   );
   
   // Set output values
   if (ok) {
      pOut->time = pAdapter->lastTime - pAdapter->startTime;
      pOut->idleTime = pAdapter->lastIdleTime - pAdapter->startIdleTime;
      pOut->packetsReceived = pAdapter->packetsReceived;
      pOut->bytesReceived = pAdapter->bytesReceived;
   }

   return ok;
}

NDIS_STATUS NDPInitProtocol(NDP_PROTOCOL* pDevice)
{
   NDIS_PROTOCOL_CHARACTERISTICS pc;
   NDIS_STATUS status;   

   // Prepare protocol characteristics
   NdisZeroMemory(&pc, sizeof(pc));

   pc.MajorNdisVersion = NDIS_PROTOCOL_MAJOR_VERSION;
   pc.MinorNdisVersion = NDIS_PROTOCOL_MINOR_VERSION;
   NdisInitUnicodeString(&pc.Name, TEXT("NDP"));

   pc.BindAdapterHandler = ndp_BindAdapter;
   pc.UnbindAdapterHandler = ndp_UnbindAdapter;
   pc.OpenAdapterCompleteHandler = ndp_OpenAdapterComplete;
   pc.CloseAdapterCompleteHandler = ndp_CloseAdapterComplete;
   pc.ReceiveHandler = ndp_Receive;
   pc.TransferDataCompleteHandler = ndp_TransferDataComplete;
   pc.ReceivePacketHandler = ndp_ReceivePacket;
   pc.ReceiveCompleteHandler = ndp_ReceiveComplete;
   pc.SendCompleteHandler = ndp_SendComplete;
   pc.ResetCompleteHandler = ndp_ResetComplete;
   pc.RequestCompleteHandler = ndp_RequestComplete;
   pc.StatusHandler = ndp_Status;
   pc.StatusCompleteHandler = ndp_StatusComplete;
   pc.PnPEventHandler = ndp_PnPEvent;
 
   // Register protocol
   NdisRegisterProtocol(
      &status, &pDevice->hProtocol, &pc, sizeof(NDIS_PROTOCOL_CHARACTERISTICS)
   );

   return status;
}


NDIS_STATUS NDPDeInitProtocol(NDP_PROTOCOL* pDevice)
{
   NDIS_STATUS status;
   NDP_ADAPTER * pAdapt,* pNextAdapt;
   //
   // grab the global lock and Unbind each of the adapters.
   //
   NdisAcquireSpinLock(&pDevice->Lock);

   for(pAdapt = pDevice->pAdapterList; pAdapt; pAdapt = pNextAdapt)
   {
	  BOOL flag = FALSE;
      pNextAdapt = pAdapt->pAdapterNext;

      NdisReleaseSpinLock(&pDevice->Lock);
	  flag = ndp_CloseAdapter(pAdapt);
	  NDPDeInitAdapter(pAdapt);

      NdisAcquireSpinLock(&pDevice->Lock);
   }

   NdisReleaseSpinLock(&pDevice->Lock);

   NdisDeregisterProtocol(&status, pDevice->hProtocol);
   return status;
}

NDIS_STATUS NDPInitAdapter(NDP_ADAPTER* pAdapter)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   // Initialize events & lists
   NdisInitializeEvent(&pAdapter->hPendingEvent);

   INITIALIZE_LIST(&pAdapter->sendQueue);
   INITIALIZE_LIST(&pAdapter->recvQueue);
   INITIALIZE_LIST(&pAdapter->workQueue);
   INITIALIZE_LIST(&pAdapter->waitQueue);
   return status;
}

NDIS_STATUS NDPDeInitAdapter(NDP_ADAPTER* pAdapter)
{
	NDIS_STATUS status = NDIS_STATUS_SUCCESS;

	//If adapter is stuck in StressReceive then lets help it to stop StressReceive.
	if ((pAdapter->fStressMode) && (pAdapter->fReceiveMode))
	{
		//Lets set the event & force the StressReceive thread to leave
		NdisSetEvent(&pAdapter->waitQueue.hEvent);
		//Lets sleep for 10ms & let the stressreceive thread to clean off
		NdisMSleep(10000);
	}

   // Uninitialize lists
   FREE_LIST(&pAdapter->sendQueue);
   FREE_LIST(&pAdapter->recvQueue);
   FREE_LIST(&pAdapter->workQueue);
   FREE_LIST(&pAdapter->waitQueue);
   
   // Free events
   NdisFreeEvent(&pAdapter->hPendingEvent);
   return status;
}

