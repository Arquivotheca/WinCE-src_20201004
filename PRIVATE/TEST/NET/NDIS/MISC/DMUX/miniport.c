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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*++


Module Name:
   miniport.c

Abstract:
   The Ndis DMUX Intermediate Miniport driver sample. This is based on passthru driver 
   which doesn't touch packets at all. The miniport API is implemented
   in the following code.

Author:
   The original code was a NT/XP sample passthru driver.
   KarelD - add CE specific code and reformat text.

Environment:
   Windows CE .Net

Revision History:
   None.

--*/

//------------------------------------------------------------------------------

#include "precomp.h"
#pragma hdrstop

//------------------------------------------------------------------------------

NDIS_STATUS
MiniportInitialize(
   OUT PNDIS_STATUS pOpenStatus,
   OUT PUINT puiSelectedMediumIndex,
   IN PNDIS_MEDIUM aMediumArray,
   IN UINT uiMediumArraySize,
   IN NDIS_HANDLE hMPBinding,
   IN NDIS_HANDLE hWrapperConfiguration
)
/*++

Routine Description:
   This is the initialize handler which gets called as a result of
   the BindAdapter handler calling NdisIMInitializeDeviceInstanceEx.
   The context parameter which we pass there is the adapter structure
   which we retrieve here.

Arguments:
   pOpenStatus - Not used by us
   puiSelectedMediumIndex - Place-holder for what media we are using
   aMediumArray - The array of ndis media passed down to us
   uiMediumArraySize - Size of the array
   hMiniportAdapterHandle - The handle NDIS uses to refer to us
   hWrapperConfigurationContext - For use by NdisOpenConfiguration

Return Value:
   NDIS_STATUS_SUCCESS unless something goes wrong

--*/
{
   UINT i;
   BINDING* pBinding;
   NDIS_STATUS status;
   NDIS_MEDIUM medium;
   BOOLEAN bVMiniportDMUXed = FALSE;
   PsDMUX pDMUXIf = NULL;
   
   //
   // Start off by retrieving our adapter context and storing
   // the Miniport handle in it.
   //
   pBinding = NdisIMGetDeviceContext(hMPBinding);

   if (pBinding == NULL)
   {
	   ASSERT(0);
	   status = NDIS_STATUS_FAILURE;
	   goto cleanUp;
   }

   DBGPRINT(("==>DMUXMini::Miniport Initialize: pBinding %p\n", pBinding));

   if (pBinding->dwDMUXIfIndex < MAX_DMUX_INTERFACES)
   {
	   pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[pBinding->dwDMUXIfIndex];
	   if (!pDMUXIf)
	   {
		   status = NDIS_STATUS_ADAPTER_NOT_FOUND;
		   goto cleanUp;
	   }
   }
   else
   {
	   status = NDIS_STATUS_ADAPTER_NOT_FOUND;
	   goto cleanUp;
   }

   //
   // Usually we export the medium type of the adapter below as our
   // virtual miniport's medium type. However if the adapter below us
   // is a WAN device, then we claim to be of medium type 802.3.
   //
   medium = pBinding->medium;

   if (medium == NdisMediumWan) medium = NdisMedium802_3;

   for (i = 0; i < uiMediumArraySize; i++) {
      if (aMediumArray[i] == medium) {
         *puiSelectedMediumIndex = i;
         break;
      }
   }

   if (i == uiMediumArraySize) {
      status = NDIS_STATUS_UNSUPPORTED_MEDIA;
      goto cleanUp;
   }

   pDMUXIf->hMPBinding = hMPBinding;
   NdisInterlockedIncrement(&pBinding->nRef);

   //
   // Set the attributes now. NDIS_ATTRIBUTE_DESERIALIZE enables us
   // to make up-calls to NDIS without having to call NdisIMSwitchToMiniport
   // or NdisIMQueueCallBack. This also forces us to protect our data using
   // spinlocks where appropriate. Also in this case NDIS does not queue
   // packets on our behalf. Since this is a very simple pass-thru
   // miniport, we do not have a need to protect anything. However in
   // a general case there will be a need to use per-adapter spin-locks
   // for the packet queues at the very least.
   //
   NdisMSetAttributesEx(
      hMPBinding, pDMUXIf, 0,
         NDIS_ATTRIBUTE_IGNORE_PACKET_TIMEOUT |
         NDIS_ATTRIBUTE_IGNORE_REQUEST_TIMEOUT |
         NDIS_ATTRIBUTE_INTERMEDIATE_DRIVER |
         NDIS_ATTRIBUTE_DESERIALIZE |
         NDIS_ATTRIBUTE_NO_HALT_ON_SUSPEND,
      0
   );

   //
   // Initialize LastIndicatedStatus to be NDIS_STATUS_MEDIA_CONNECT
   //
   pDMUXIf->statusLastIndicated = NDIS_STATUS_MEDIA_CONNECT;
        
   //
   // Initialize the power states for both the lower binding (PTDeviceState)
   // and our miniport edge to Powered On.
   //
   pDMUXIf->MPDeviceState = NdisDeviceStateD0;
   pBinding->PTDeviceState = NdisDeviceStateD0;

#ifndef UNDER_CE
   //
   // Create an ioctl interface
   //
   (VOID)RegisterDevice();
#endif

   status = NDIS_STATUS_SUCCESS;
   ++pBinding->sPASSDMUXSet.dwDMUXnos;
   
cleanUp:
   DBGPRINT((
      "<==DMUXMini::MiniportInitialize: pBinding %p, status %x\n", 
      pBinding, status
   ));
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS
MiniportSend(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN PNDIS_PACKET pPacket,
   IN UINT uiFlags
)
/*++

Routine Description:
   Send Packet handler. Either this or our SendPackets (array) handler is called
   based on which one is enabled in our Miniport Characteristics.

Arguments:
   hMiniportAdapterContext - Pointer to the adapter
   pPacket - Packet to send
   uiFlags - Unused, passed down below

Return Value:
   Return code from NdisSend

--*/
{
   PsDMUX pDMUXIf = (PsDMUX)hMiniportAdapterContext;
   BINDING* pBinding = pDMUXIf->pBinding;
   NDIS_STATUS status;
   PNDIS_PACKET pSendPacket;
   PASSTHRU_PR_SEND* pProtocolSend;
   PVOID pvMediaSpecificInfo = NULL;
   ULONG uiMediaSpecificInfoSize = 0;

#ifdef NDIS51
   //
   // Use NDIS 5.1 packet stacking:
   //
   PNDIS_PACKET_STACK pStack;
   BOOLEAN bRemaining;

   //
   // Packet stacks: Check if we can use the same packet for sending down.
   //
   pStack = NdisIMGetCurrentPacketStack(pPacket, &bRemaining);
   if (bRemaining) {
      //
      // We can reuse packet
      //
      // NOTE: if we needed to keep per-packet information in packets
      // sent down, we can use pStack->IMReserved[].
      //
      ASSERT(pStack);
	  pStack->IMReserved[0] = (ULONG_PTR) pDMUXIf;

      NdisSend(&status, pBinding->hPTBinding, pPacket);
      goto cleanUp;
   }
#endif // NDIS51

   //
   // We are either not using packet stacks, or there isn't stack space
   // in the original packet passed down to us. Allocate a new packet
   // to wrap the data with.
   //
   NdisAllocatePacket(&status, &pSendPacket, pBinding->hSendPacketPool);
   if (status != NDIS_STATUS_SUCCESS) {
      //
      // We are out of packets. Silently drop it. Alternatively we can deal with it:
      // - By keeping separate send and receive pools
      // - Dynamically allocate more pools as needed and free them when not needed
      //
      goto cleanUp;
   }

   //
   // Save a pointer to the original packet in our reserved
   // area in the new packet. This is needed so that we can
   // get back to the original packet when the new packet's send
   // is completed.
   //
   pProtocolSend = (PASSTHRU_PR_SEND*)(pSendPacket->ProtocolReserved);
   pProtocolSend->pOriginalPacket = pPacket;

   NdisSetPacketFlags(pSendPacket, uiFlags);

   //
   // Set up the new packet so that it describes the same
   // data as the original packet.
   //
   pSendPacket->Private.Head = pPacket->Private.Head;
   pSendPacket->Private.Tail = pPacket->Private.Tail;

   //
   // Copy the OOB Offset from the original packet to the new
   // packet.
   //
   NdisMoveMemory(
      NDIS_OOB_DATA_FROM_PACKET(pSendPacket),
      NDIS_OOB_DATA_FROM_PACKET(pPacket), sizeof(NDIS_PACKET_OOB_DATA)
   );

   //
   // Copy the right parts of per packet info into the new packet.
   // This API is not available on Win9x since task offload is
   // not supported on that platform.
   //
   NdisIMCopySendPerPacketInfo(pSendPacket, pPacket);

   //
   // Copy the Media specific information
   //
   NDIS_GET_PACKET_MEDIA_SPECIFIC_INFO(
      pPacket, &pvMediaSpecificInfo, &uiMediaSpecificInfoSize
   );

   if (pvMediaSpecificInfo != NULL && uiMediaSpecificInfoSize > 0) {
      NDIS_SET_PACKET_MEDIA_SPECIFIC_INFO(
         pSendPacket, pvMediaSpecificInfo, uiMediaSpecificInfoSize
      );
   }

   NdisSend(&status, pBinding->hPTBinding, pSendPacket);
   if (status != NDIS_STATUS_PENDING) {
      NdisIMCopySendCompletePerPacketInfo(pPacket, pSendPacket);
      NdisFreePacket(pPacket);
   }
   
cleanUp:
   return(status);
}

//------------------------------------------------------------------------------

VOID
MiniportSendPackets(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN PPNDIS_PACKET apPacketArray,
   IN UINT uiNumberOfPackets
)
/*++

Routine Description:
   Send Packet Array handler. Either this or our SendPacket handler is called
   based on which one is enabled in our Miniport Characteristics.

Arguments:
   hMiniportAdapterContext - Pointer to our binding
   paPacketArray - Set of packets to send
   uiNumberOfPackets - Self-explanatory

Return Value:
   None

--*/
{
   PsDMUX pDMUXIf = (PsDMUX)hMiniportAdapterContext;
   BINDING* pBinding = pDMUXIf->pBinding;
   NDIS_STATUS status;
   UINT i;
   PVOID pvMediaSpecificInfo;
   UINT uiMediaSpecificInfoSize;
   PNDIS_PACKET pPacket, pSendPacket;
   PASSTHRU_PR_SEND* pProtocolSend;
#ifdef NDIS51
   PNDIS_PACKET_STACK pStack;
   BOOLEAN bRemaining;
#endif

   for (i = 0; i < uiNumberOfPackets; i++) {

      pPacket = apPacketArray[i];

#ifdef NDIS51
      //
      // Use NDIS 5.1 packet stacking:
      //
      // Packet stacks: Check if we can use the same packet for sending down.
      //
      pStack = NdisIMGetCurrentPacketStack(pPacket, &bRemaining);
      if (bRemaining) {
         //
         // We can reuse packet
         //
         // NOTE: if we needed to keep per-packet information in packets
         // sent down, we can use pStack->IMReserved[].
         //
         ASSERT(pStack);
		 pStack->IMReserved[0] = (ULONG_PTR)pDMUXIf;

         NdisSend(&status, pBinding->hPTBinding, pPacket);
         if (status != NDIS_STATUS_PENDING) {
            NdisMSendComplete(pDMUXIf->hMPBinding, pPacket, status);
         }
         continue;
      }
#endif

      NdisAllocatePacket(&status, &pSendPacket, pBinding->hSendPacketPool);
      if (status != NDIS_STATUS_SUCCESS) {
         // We are out of packets so silently drop
         if (status != NDIS_STATUS_PENDING) {
            NdisMSendComplete(pBinding->hMPBinding, pPacket, status);
         }
         continue;
      }
      
      pProtocolSend = (PPASSTHRU_PR_SEND)(pSendPacket->ProtocolReserved);
      pProtocolSend->pOriginalPacket = pPacket;

      NdisSetPacketFlags(pSendPacket, NdisGetPacketFlags(pPacket));

      pSendPacket->Private.Head = pPacket->Private.Head;
      pSendPacket->Private.Tail = pPacket->Private.Tail;

      //
      // Copy the OOB data from the original packet to the new
      // packet.
      //
      NdisMoveMemory(
         NDIS_OOB_DATA_FROM_PACKET(pSendPacket),
         NDIS_OOB_DATA_FROM_PACKET(pPacket), sizeof(NDIS_PACKET_OOB_DATA)
      );

      //
      // Copy relevant parts of the per packet info into the new packet
      //
      NdisIMCopySendPerPacketInfo(pSendPacket, pPacket);

      //
      // Copy the Media specific information
      //
      NDIS_GET_PACKET_MEDIA_SPECIFIC_INFO(
         pPacket, &pvMediaSpecificInfo, &uiMediaSpecificInfoSize
      );
      if (pvMediaSpecificInfo != NULL && uiMediaSpecificInfoSize > 0) {
         NDIS_SET_PACKET_MEDIA_SPECIFIC_INFO(
            pSendPacket, pvMediaSpecificInfo, uiMediaSpecificInfoSize
         );
      }

      NdisSend(&status, pBinding->hPTBinding, pSendPacket);

      if (status != NDIS_STATUS_PENDING) {
         NdisIMCopySendCompletePerPacketInfo(pPacket, pSendPacket);
         NdisFreePacket(pSendPacket);
      }

   }

}

//------------------------------------------------------------------------------

NDIS_STATUS
MiniportQueryInformation(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN NDIS_OID Oid,
   IN PVOID pvInformationBuffer,
   IN ULONG ulInformationBufferLength,
   OUT PULONG pulBytesWritten,
   OUT PULONG pulBytesNeeded
)
/*++

Routine Description:
   Entry point called by NDIS to query for the value of the specified OID.
   Typical processing is to forward the query down to the underlying miniport.

   The following OIDs are filtered here:

      OID_PNP_QUERY_POWER - return success right here

      OID_GEN_SUPPORTED_GUIDS - do not forward, otherwise we will show up
         multiple instances of private GUIDs supported by the underlying
         miniport.

      OID_PNP_CAPABILITIES - we do send this down to the lower miniport, but
         the values returned are postprocessed before we complete this request;
         see PtRequestComplete.

   NOTE on OID_TCP_TASK_OFFLOAD - if this IM driver modifies the contents
   of data it passes through such that a lower miniport may not be able
   to perform TCP task offload, then it should not forward this OID down,
   but fail it here with the status NDIS_STATUS_NOT_SUPPORTED. This is to
   avoid performing incorrect transformations on data.

   If our miniport edge (upper edge) is at a low-power state, fail the request.

   If our protocol edge (lower edge) has been notified of a low-power state,
   we pend this request until the miniport below has been set to D0. Since
   requests to miniports are serialized always, at most a single request will
   be pended.

Arguments:
   hMiniportAdapterContext - Pointer to the binding structure
   Oid - The Oid for this query
   pvInformationBuffer - Buffer for information
   ulInformationBufferLength - Size of this buffer
   pulBytesWritten - Specifies how much info is written
   pulBytesNeeded - In case the buffer is smaller than what we need, tell them 
      how much is needed

Return Value:
   Return code from the NdisRequest below.

--*/
{
   PsDMUX pDMUXIf = (PsDMUX)hMiniportAdapterContext;
   BINDING* pBinding = pDMUXIf->pBinding;
   NDIS_STATUS status = NDIS_STATUS_FAILURE;

   switch (Oid) {

   case OID_PNP_QUERY_POWER:
      //
      //  Do not forward this.
      //
      status = NDIS_STATUS_SUCCESS;
      break;

#ifdef UNDER_CE
   case OID_PNP_CAPABILITIES:
      //
      // Passthru can't support this for now because there is currently
      // no control on which adapter is powered up first 
      // i.e. between PASSTHRU virtual miniport or the underlying
      //      miniport passthru controls.
      //
      // PassThru's MiniportSetInformation() returns status pending 
      // if the underlying miniport has not been powered up.
      // This can potentially block the protocol driver that 
      // that waits for the completion.
      //
      // If the underlying miniport driver is powered up first then 
      // this is not a problem.
      //
      
      status = NDIS_STATUS_NOT_SUPPORTED;
      break;
#endif      

   case OID_GEN_SUPPORTED_GUIDS:
      //
      //  Do not forward this, otherwise we will end up with multiple
      //  instances of private GUIDs that the underlying miniport
      //  supports.
      //
      status = NDIS_STATUS_NOT_SUPPORTED;
      break;

   case OID_802_3_PERMANENT_ADDRESS:
   case OID_802_3_CURRENT_ADDRESS:

	   //
	   // We are being asked about the Mac address.
	   if (ulInformationBufferLength < 6)
	   {
		   *pulBytesWritten = 0;
		   *pulBytesNeeded = 6;
	   }
	   else
	   {
		   NdisMoveMemory((PBYTE)pvInformationBuffer,
			   pDMUXIf->pbMacAddr,
			   6);
		   *pulBytesWritten = 6;
		   *pulBytesNeeded = 6;
		   status = NDIS_STATUS_SUCCESS;
	   }
	   break;

   case OID_GEN_CURRENT_PACKET_FILTER:
	   //
	   // We are being asked about out filter setting.
	   //
	   if (ulInformationBufferLength < sizeof(ULONG))
	   {
		   *pulBytesWritten = 0;
		   *pulBytesNeeded = sizeof(ULONG);
	   }
	   else
	   {
		   *((PULONG)pvInformationBuffer) = pDMUXIf->ulFilter;
		   *pulBytesWritten = sizeof(ULONG);
		   *pulBytesNeeded = sizeof(ULONG);
		   status = NDIS_STATUS_SUCCESS;
	   }
	   break;

   case OID_TCP_TASK_OFFLOAD:
      //
      // Fail this -if- this driver performs data transformations
      // that can interfere with a lower driver's ability to offload
      // TCP tasks.
      //
   default:
      //
      // All other queries are failed, if the miniport is not at D0
      //
      if (pDMUXIf->MPDeviceState > NdisDeviceStateD0 || pDMUXIf->bStandingBy) {
         status = NDIS_STATUS_FAILURE;
         break;
      }

      pDMUXIf->request.RequestType = NdisRequestQueryInformation;
      pDMUXIf->request.DATA.QUERY_INFORMATION.Oid = Oid;
      pDMUXIf->request.DATA.QUERY_INFORMATION.InformationBuffer = pvInformationBuffer;
      pDMUXIf->request.DATA.QUERY_INFORMATION.InformationBufferLength = ulInformationBufferLength;
      pDMUXIf->pulBytesNeeded = pulBytesNeeded;
      pDMUXIf->pulBytesUsed = pulBytesWritten;
      pDMUXIf->bOutstandingRequests = TRUE;

      //
      // If the Protocol device state is OFF, mark this request as being
      // pended. We queue this until the device state is back to D0.
      //
      if (pBinding->PTDeviceState > NdisDeviceStateD0) {
         pDMUXIf->bQueuedRequest = TRUE;
         status = NDIS_STATUS_PENDING;
         break;
      }

      //
      // default case, most requests will be passed to the miniport below
      //
      NdisRequest(&status, pBinding->hPTBinding, &pDMUXIf->request);
      if (status != NDIS_STATUS_PENDING) {
         ProtocolRequestComplete(pBinding, &pDMUXIf->request, status);
         status = NDIS_STATUS_SUCCESS;
      }

   }

   return status;

}

//------------------------------------------------------------------------------

VOID
MiniportProcessSetPowerOid(
   IN OUT PNDIS_STATUS pStatus,
   IN PsDMUX pDMUXIf,
   IN PVOID pvInformationBuffer,
   IN ULONG ulInformationBufferLength,
   OUT PULONG pulBytesRead,
   OUT PULONG pulBytesNeeded
)
/*++

Routine Description:
   This routine does all the procssing for a request with a SetPower Oid.
   The miniport shoud accept the Set Power and transition to the new state.

   The Set Power should not be passed to the miniport below if the IM miniport 
   is going into a low power state, then there is no guarantee if it will ever
   be asked go back to D0, before getting halted. No requests should be pended
   or queued.

   
Arguments:
   pStatus - status of the operation
   pBinding - The binding structure
   pvInformationBuffer - Buffer for information
   ulInformationBufferLength - Size of this buffer
   pulBytesRead  - No of bytes read
   pulBytesNeeded -  No of bytes needed


Return Value:
   status  - NDIS_STATUS_SUCCESS if all the wait events succeed.

--*/
{
   NDIS_DEVICE_POWER_STATE deviceState;

   DBGPRINT((
      "==>DMUXMini::MiniportProcessSetPowerOid: pDMUXIf %p\n", pDMUXIf
   )); 

   ASSERT(pvInformationBuffer != NULL);

   //
   // Check for invalid length
   //
   if (ulInformationBufferLength < sizeof(NDIS_DEVICE_POWER_STATE)) {
      *pStatus = NDIS_STATUS_INVALID_LENGTH;
      goto cleanUp;
   }

    deviceState = *(NDIS_DEVICE_POWER_STATE*)pvInformationBuffer;

    //
    // Check for invalid device state
    //
    if (
      (pDMUXIf->MPDeviceState > NdisDeviceStateD0) && 
      (deviceState != NdisDeviceStateD0)
    ) {
       //
       // If the miniport is in a non-D0 state, the miniport can only receive
       // a Set Power to D0
       //
       *pStatus = NDIS_STATUS_FAILURE;
       goto cleanUp;
    }  

    //
    // Is the miniport transitioning from an ON state to an Low Power State
    // If so, then set the StandingBy Flag - (Block all incoming requests)
    //
    if (
      (pDMUXIf->MPDeviceState == NdisDeviceStateD0) && 
      (deviceState > NdisDeviceStateD0)
    ) {
      pDMUXIf->bStandingBy = TRUE;
    }

    //
    // If the miniport is transitioning from a Low Power state to ON (D0),
    // then clear the StandingBy flag. All incoming requests will be pended 
    // until the physical miniport turns ON.
    //
    if (
      (pDMUXIf->MPDeviceState > NdisDeviceStateD0) &&
      (deviceState == NdisDeviceStateD0)
    ) {
       pDMUXIf->bStandingBy = FALSE;
    }
      
    //
    // Now update the state in the pBinding structure;
    //
    pDMUXIf->MPDeviceState = deviceState;
    *pStatus = NDIS_STATUS_SUCCESS;
   
cleanUp:      
   if (*pStatus == NDIS_STATUS_SUCCESS) {
     //
     // The miniport resume from low power state
     // 
     if (!pDMUXIf->bStandingBy) {
         //
         // If we need to indicate the media connect state
         // 
         if (pDMUXIf->statusLastIndicated != pDMUXIf->statusLatestUnIndicate) {
            NdisMIndicateStatus(
               pDMUXIf->hMPBinding, pDMUXIf->statusLatestUnIndicate, NULL, 0
            );
            NdisMIndicateStatusComplete(pDMUXIf->hMPBinding);
            pDMUXIf->statusLastIndicated = pDMUXIf->statusLatestUnIndicate;
         }
     } else {
         //
         // Initialize LatestUnIndicatedStatus
         //
         pDMUXIf->statusLatestUnIndicate = pDMUXIf->statusLastIndicated;
     }
     *pulBytesRead = sizeof(NDIS_DEVICE_POWER_STATE);
     *pulBytesNeeded = sizeof(NDIS_DEVICE_POWER_STATE);

   } else {
      *pulBytesRead = 0;
      *pulBytesNeeded = sizeof(NDIS_DEVICE_POWER_STATE);
   }

   DBGPRINT((
      "<==DMUXMini::MiniportProcessSetPowerOid: pDMUXIf %p\n", pDMUXIf
   )); 
}

//------------------------------------------------------------------------------

NDIS_STATUS
MiniportSetInformation(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN NDIS_OID Oid,
   IN PVOID pvInformationBuffer,
   IN ULONG ulInformationBufferLength,
   OUT PULONG pulBytesRead,
   OUT PULONG pulBytesNeeded
)
/*++

Routine Description:
   Miniport SetInfo handler.

   In the case of OID_PNP_SET_POWER, record the power state and return the OID.  
   Do not pass below if the device is suspended, do not block the SET_POWER_OID 
   as it is used to reactivate the Passthru miniport.

   PM - If the Miniport is not ON (DeviceState > D0) return immediately
   (except for 'query power' and 'set power'). If Miniport is ON, but the PT 
   is not at D0, then queue the queue the request for later processing.

   Requests to miniports are always serialized

Arguments:
   hMiniportAdapterContext - Pointer to the adapter structure
   Oid - Oid for this query
   pvInformationBuffer - Buffer for information
   ulInformationBufferLength - Size of this buffer
   pulBytesRead - Specifies how much info is read
   pulBytesNeeded - In case the buffer is smaller than what we need, tell them 
      how much is needed

Return Value:
   Return code from the NdisRequest below.

--*/
{
   PsDMUX pDMUXIf = (PsDMUX)hMiniportAdapterContext;
   BINDING* pBinding = pDMUXIf->pBinding;
   NDIS_STATUS status;

   status = NDIS_STATUS_FAILURE;

   switch (Oid) {

   case OID_PNP_SET_POWER:
      //
      // The Set Power should not be sent to the miniport below the Passthru, but is handled internally
      //
      MiniportProcessSetPowerOid(
         &status, pDMUXIf, pvInformationBuffer, ulInformationBufferLength, 
         pulBytesRead, pulBytesNeeded
      );
      break;

   case OID_GEN_CURRENT_PACKET_FILTER:
	   {
		   static DWORD dwFirstTime = TRUE;

		   // We are getting a request to set our filter driver. This is indication that we can indicate to this protocol
		   // driver. Ideally we should do this in ProtocolRequestComplete or when request is completed.
		   pDMUXIf->dwState = 1;
		   
		   //
		   // We are being asked about out filter setting.
		   //
		   if (ulInformationBufferLength != sizeof(ULONG))
		   {
			   status = NDIS_STATUS_FAILURE;
			   *pulBytesRead = 0;
			   *pulBytesNeeded = sizeof(ULONG);
		   }
		   else
		   {
			   pDMUXIf->ulFilter = *((PULONG)pvInformationBuffer);
			   *pulBytesRead = sizeof(ULONG);
			   status = NDIS_STATUS_SUCCESS;
		   }
		   
		   if ((dwFirstTime) && (status == NDIS_STATUS_SUCCESS))
		   {
			   dwFirstTime = FALSE;
			   // Setting the miniport driver below us to basically PROMISCUOUS mode.
			   *((PULONG)pvInformationBuffer) = NDIS_PACKET_TYPE_PROMISCUOUS|NDIS_PACKET_TYPE_DIRECTED|
				   NDIS_PACKET_TYPE_MULTICAST|NDIS_PACKET_TYPE_ALL_MULTICAST|NDIS_PACKET_TYPE_BROADCAST;
			   // Fall through
		   }
		   else
			   break;
	   }


   default:
      //
      // All other Set Information requests are failed, if the miniport is
      // not at D0 or is transitioning to a device state greater than D0.
      //
      if (pDMUXIf->MPDeviceState > NdisDeviceStateD0 || pDMUXIf->bStandingBy) {
         status = NDIS_STATUS_FAILURE;
         break;
      }

      // Set up the Request and return the result
      pDMUXIf->request.RequestType = NdisRequestSetInformation;
      pDMUXIf->request.DATA.SET_INFORMATION.Oid = Oid;
      pDMUXIf->request.DATA.SET_INFORMATION.InformationBuffer = pvInformationBuffer;
      pDMUXIf->request.DATA.SET_INFORMATION.InformationBufferLength = ulInformationBufferLength;
      pDMUXIf->pulBytesNeeded = pulBytesNeeded;
      pDMUXIf->pulBytesUsed = pulBytesRead;
      pDMUXIf->bOutstandingRequests = TRUE;

      //
      // If the device below is at a low power state, we cannot send it the
      // request now, and must pend it.
      //
      if (pBinding->PTDeviceState > NdisDeviceStateD0) {
		 pDMUXIf->bQueuedRequest = TRUE;
         status = NDIS_STATUS_PENDING;
         break;
      }

      //
      // Forward the request to the device below.
      //
      NdisRequest(&status, pBinding->hPTBinding, &pDMUXIf->request);

      if (status == NDIS_STATUS_SUCCESS) {
         *pulBytesRead = pDMUXIf->request.DATA.SET_INFORMATION.BytesRead;
         *pulBytesNeeded = pDMUXIf->request.DATA.SET_INFORMATION.BytesNeeded;
      }

      if (status != NDIS_STATUS_PENDING)
      {
         pDMUXIf->bOutstandingRequests = FALSE;
      }
   }

   return status;
}

//------------------------------------------------------------------------------

VOID
MiniportReturnPacket(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN PNDIS_PACKET pPacket
)
/*++

Routine Description:
   NDIS Miniport entry point called whenever protocols are done with
   a packet that we had indicated up and they had queued up for returning
   later.

Arguments:
   hMiniportAdapterContext - pointer to BINDING structure
   pPacket - packet being returned

Return Value:
   None

--*/
{
   PsDMUX pDMUXIf = (PsDMUX)hMiniportAdapterContext;
   BINDING* pBinding = pDMUXIf->pBinding;

#ifdef NDIS51
   //
   // Packet stacking: Check if this packet belongs to us.
   //
   if (NdisGetPoolFromPacket(pPacket) != pBinding->hRecvPacketPool) {
      //
      // We reused the original packet in a receive indication.
      // Simply return it to the miniport below us.
      //
      NdisReturnPackets(&pPacket, 1);
   }
   else
#endif // NDIS51
   {
      //
      // This is a packet allocated from this IM's receive packet pool.
      // Reclaim our packet, and return the original to the driver below.
      //

      PNDIS_PACKET pOriginalPacket;
      PPASSTHRU_PR_RECV pProtocolRecv;
   
      pProtocolRecv = (PPASSTHRU_PR_RECV)(pPacket->MiniportReserved);
      pOriginalPacket = pProtocolRecv->pOriginalPacket;
   
      NdisFreePacket(pPacket);
      NdisReturnPackets(&pOriginalPacket, 1);
   }
}

//------------------------------------------------------------------------------

NDIS_STATUS
MiniportTransferData(
   OUT PNDIS_PACKET pPacket,
   OUT PUINT puiBytesTransferred,
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN NDIS_HANDLE hMiniportReceiveContext,
   IN UINT uiByteOffset,
   IN UINT uiBytesToTransfer
)
/*++

Routine Description:
   Miniport's transfer data handler.

Arguments:
   pPacket - Destination packet
   puiBytesTransferred - Place holder for how much data was copied
   hMiniportAdapterContext - Pointer to the binding structure
   hMiniportReceiveContext - Context
   uiByteOffset - Offset into the packet for copying data
   uiBytesToTransfer - How much to copy.

Return Value:
   status of transfer

--*/
{
   PsDMUX pDMUXIf = (PsDMUX)hMiniportAdapterContext;
   BINDING* pBinding = pDMUXIf->pBinding;
   NDIS_STATUS status;

   //
   // Return, if the device is OFF
   //
   if (
      (pDMUXIf->MPDeviceState != NdisDeviceStateD0) ||
      (pBinding->PTDeviceState != NdisDeviceStateD0) 
   ) {
     status = NDIS_STATUS_FAILURE;
     goto cleanUp;
   }

   NdisTransferData(
      &status, pBinding->hPTBinding, hMiniportReceiveContext, uiByteOffset,
      uiBytesToTransfer, pPacket, puiBytesTransferred
   );

   // We shd allocate the packet here & keep pDMUXIf in its context & when
   // our ProtocolTransferDataComplete is called we'll return the original packet back to
   // upper protocol layer based on pDMUXIf.

   // Right now we are not handling it.
   ASSERT(status != NDIS_STATUS_PENDING);

cleanUp:
   return status;
}

//------------------------------------------------------------------------------

VOID
MiniportHalt(
   IN NDIS_HANDLE hMiniportAdapterContext
)
/*++

Routine Description:
   Halt handler. All the hard-work for clean-up is done here.

Arguments:
   hMiniportAdapterContext - Pointer to the binding structure

Return Value:
   None

--*/
{
   PsDMUX pDMUXIf = (PsDMUX)hMiniportAdapterContext;
   BINDING* pBinding = pDMUXIf->pBinding;
   NDIS_STATUS status;
   BINDING** ppCursor;

   DBGPRINT(("==>DMUXMini::MiniportHalt: pBinding %p\n", pBinding));

#ifndef UNDER_CE
   //
   // Delete the ioctl interface that was created when the miniport
   // was created.
   //
   (VOID)DeregisterDevice();
#endif

   //
   // If we have a valid bind, close the miniport below the protocol
   //
   if (pBinding->hPTBinding != NULL) {
      //
      // Close the binding below. and wait for it to complete
      //
      NdisResetEvent(&pBinding->hEvent);
      NdisCloseAdapter(&status, pBinding->hPTBinding);
      if (status == NDIS_STATUS_PENDING) {
         NdisWaitEvent(&pBinding->hEvent, 0);
         status = pBinding->status;
      }

      ASSERT (status == NDIS_STATUS_SUCCESS);
      pBinding->hPTBinding = NULL;
   }

   //
   //  Free all resources on this adapter structure.
   //
   if (pBinding->hRecvPacketPool != NULL) {
      NdisFreePacketPool(pBinding->hRecvPacketPool);
      pBinding->hRecvPacketPool = NULL;
   }
   if (pBinding->hSendPacketPool != NULL) {
      NdisFreePacketPool(pBinding->hSendPacketPool);
      pBinding->hSendPacketPool = NULL;
   }

   //
   // Remove this adapter from the global list
   //
   NdisAcquireSpinLock(&g_spinLock);
   ppCursor = &g_pBindingList; 
   while (*ppCursor != NULL) {
      if (*ppCursor == pBinding) {
         *ppCursor = pBinding->pNext;
         break;
      }
      ppCursor = &(*ppCursor)->pNext;
   }
   NdisReleaseSpinLock(&g_spinLock);

   // Free instance structure
   NdisFreeMemory(pBinding, 0, 0);

   DBGPRINT(("<==DMUXMini::MiniportHalt: pBinding %p\n", pBinding));
}

//------------------------------------------------------------------------------

NDIS_STATUS
MiniportReset(
   OUT PBOOLEAN  pbAddressingReset,
   IN NDIS_HANDLE hMiniportAdapterContext
)
/*++

Routine Description:
   Reset Handler. We just don't do anything.

Arguments:
   pbAddressingReset - To let NDIS know whether we need help with our reset
   hMiniportAdapterContext - Pointer to our binding

Return Value:


--*/
{
   PsDMUX pDMUXIf = (PsDMUX)hMiniportAdapterContext;
   BINDING* pBinding = pDMUXIf->pBinding;
   *pbAddressingReset = FALSE;
   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

#ifdef NDIS51_MINIPORT

VOID
MiniportCancelSendPackets(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN PVOID pvCancelId
)
/*++

Routine Description:
   The miniport entry point to handle cancellation of all send packets
   that match the given CancelId. If we have queued any packets that match
   this, then we should dequeue them and call NdisMSendComplete for all
   such packets, with a status of NDIS_STATUS_REQUEST_ABORTED.

   We should also call NdisCancelSendPackets in turn, on each lower binding
   that this adapter corresponds to. This is to let miniports below cancel
   any matching packets.

Arguments:
   hMiniportAdapterContext - pointer to the binding structure
   pvCancelId - ID of packets to be cancelled.

Return Value:
   None

--*/
{
   PsDMUX pDMUXIf = (PsDMUX)hMiniportAdapterContext;
   BINDING* pBinding = pDMUXIf->pBinding;

   //
   // If we queue packets on our adapter structure, this would be 
   // the place to acquire a spinlock to it, unlink any packets whose
   // Id matches CancelId, release the spinlock and call NdisMSendComplete
   // with NDIS_STATUS_REQUEST_ABORTED for all unlinked packets.
   //

   //
   // Next, pass this down so that we let the miniport(s) below cancel
   // any packets that they might have queued.
   //
   NdisCancelSendPackets(pBinding->hPTBinding, pvCancelId);
}

#endif

//------------------------------------------------------------------------------

#ifdef NDIS51_MINIPORT

VOID
MiniportDevicePnPEvent(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN NDIS_DEVICE_PNP_EVENT  devicePnPEvent,
   IN PVOID pvInformationBuffer,
   IN ULONG ulInformationBufferLength
)
/*++

Routine Description:
   This handler is called to notify us of PnP events directed to
   our miniport device object.

Arguments:
   hMiniportAdapterContext  - pointer to the binding structure
   devicePnPEvent - the event
   pvInformationBuffer - points to additional event-specific information
   ulInformationBufferLength - length of above

Return Value:
   None

--*/
{
   // TBD - add code/comments about processing this.
   PsDMUX pDMUXIf = (PsDMUX)hMiniportAdapterContext;
   BINDING* pBinding = pDMUXIf->pBinding;

   DBGPRINT((
      "===DMUXMini::MiniportDevicePnPEvent: pBinding %p event %d\n", pBinding,
      devicePnPEvent
   ));
   return ;
}

#endif

//------------------------------------------------------------------------------

#ifdef NDIS51_MINIPORT

VOID
MiniportAdapterShutdown(
   IN NDIS_HANDLE hMiniportAdapterContext
)
/*++

Routine Description:
   This handler is called to notify us of an impending system shutdown.

Arguments:
   MiniportAdapterContext  - pointer to the binding structure

Return Value:
   None

--*/
{
   PsDMUX pDMUXIf = (PsDMUX)hMiniportAdapterContext;
   BINDING* pBinding = pDMUXIf->pBinding;

   DBGPRINT((
      "===DMUXMini::MiniportAdapterShutdown: pBinding %p\n", pBinding
   ));
   return;
}

#endif

//------------------------------------------------------------------------------
