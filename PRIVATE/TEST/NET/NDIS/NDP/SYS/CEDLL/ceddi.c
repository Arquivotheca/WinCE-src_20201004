//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// File name: ceddi.c
//

#ifdef UNDER_CE

#include "NDP.h"
#include "CEddi.h"
#include "celog.h"

//------------------------------------------------------------------------------
//
//    Function:   ndp_Init
//
//    Arguments:  dwContext 
//
//    Returns:    Returns a handle to the device context created, or 0
//                if unsuccessful
//
//    Descript:   This function is called by the Device Manager to
//                initialize the device.  This handle is passed to the
//                ndt_Open, ndt_PowerDown, ndt_PowerUp, and ndt_Deinit
//                functions.
//
//------------------------------------------------------------------------------
extern DWORD ndp_Init(DWORD dwContext)
{
   NDP_PROTOCOL* pDevice = NULL;
   NDIS_STATUS status;   

   // Create a device object
   pDevice = (NDP_PROTOCOL*)LocalAlloc(LPTR, sizeof(NDP_PROTOCOL));
   if (pDevice == NULL) {
      goto cleanUp;
   }

   pDevice->magicCookie = NDP_PROTOCOL_COOKIE;
   NdisAllocateSpinLock(&pDevice->Lock);

   status = NDPInitProtocol(pDevice);

   if (status != NDIS_STATUS_SUCCESS) {
      LocalFree(pDevice); pDevice = NULL;
      goto cleanUp;
   }

cleanUp:   
   return (DWORD)pDevice;
}

//------------------------------------------------------------------------------
//
//    Function:   ndp_Deinit
//
//    Arguments:  hDevice - Handle to the device context. The ndp_Init function
//                          creates and returns this identifier. 
//
//    Returns:    TRUE indicates success. FALSE indicates failure.
//
//    Descript:   This function is called by the Device Manager to
//                deinitialize a device.
//
//------------------------------------------------------------------------------

extern BOOL ndp_Deinit(DWORD hDevice)
{
   BOOL ok = FALSE;
   NDP_PROTOCOL* pDevice = (NDP_PROTOCOL*)hDevice;
   NDIS_STATUS status;   


   // Check if we get device handler
   if ( (!pDevice) || (pDevice->magicCookie != NDP_PROTOCOL_COOKIE) )
	   goto cleanUp;

   status = NDPDeInitProtocol(pDevice);

   if (status != NDIS_STATUS_SUCCESS) {
   }

   NdisFreeSpinLock(&pDevice->Lock);

   // Delete device object
   LocalFree((HLOCAL)pDevice);

   // Anythink is ok
   ok = TRUE;
   
cleanUp:
   return ok;
}

//------------------------------------------------------------------------------
//
//    Function:   ndp_Open
//
//    Arguments:  hDeviceContext -- Handle to the device context.
//                                  The ndp_Init function creates and
//                                  returns this handle. 
//                    AccessCode -- Specifies the requested access code of
//                                  the device. The access is a combination
//                                  of read and write. 
//                     ShareMode -- Specifies the requested file share
//                                  mode of the PC Card device. The share
//                                  mode is a combination of file read
//                                  and write sharing. 
//
//    Returns:    This function returns a handle that identifies the open
//                context of the device to the calling application. If your
//                device can be opened multiple times, use this handle to
//                identify each open context. This identifier is passed into
//                the ndp_Read, ndp_Write, ndp_Seek, and ndp_IOControl
//                functions. If the device cannot be opened, this function
//                returns NULL.
//
//    Descript:   This function opens a the NDISTest Stream Interface
//                Driver for Windows CE.
//
//------------------------------------------------------------------------------

extern DWORD ndp_Open(DWORD hDevice, DWORD accessCode, DWORD shareMode)
{
   NDP_PROTOCOL* pDevice = (NDP_PROTOCOL*)hDevice;
   NDP_ADAPTER* pAdapter = NULL;


   // Check if we get device handler
   if ( (!pDevice) || (pDevice->magicCookie != NDP_PROTOCOL_COOKIE) )
	   goto cleanUp;

   // Allocate instance structure
   pAdapter = (NDP_ADAPTER*)LocalAlloc(LPTR, sizeof(NDP_ADAPTER));

   NdisZeroMemory(pAdapter, sizeof(NDP_ADAPTER));

   if (pAdapter == NULL) {
      goto cleanUp;
   }
   pAdapter->magicCookie = NDP_ADAPTER_COOKIE;
   
   // Link instance back to device
   pAdapter->pProtocol = pDevice;

   NDPInitAdapter(pAdapter);

cleanUp:
   return (DWORD)pAdapter;
}

//------------------------------------------------------------------------------
//
//    Function:   ndp_Close
//
//    Arguments:    hOpenContext -- Handle returned by the ndp_Open
//                                  function, used to identify the open 
//                                  context of the device.
//
//    Returns:    TRUE indicates success. FALSE indicates failure.
//
//    Descript:   This function closes the device context identified by 
//                hOpenContext.
//
//------------------------------------------------------------------------------

extern BOOL ndp_Close(DWORD hOpen) 
{
   BOOL ok = FALSE;
   NDP_ADAPTER* pAdapter = (NDP_ADAPTER*)hOpen;

   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   goto cleanUp;

   NDPDeInitAdapter(pAdapter);

   // Delete instance object
   LocalFree((HLOCAL)pAdapter);
   
   ok = TRUE;
   
cleanUp:   
   return ok;
}

//------------------------------------------------------------------------------
//
//    Function:   ndp_Write
//
//    Arguments:  hOpen -- Handle to the open context of the device. The call
//                         to the ndp_Open function returns this identifier. 
//                     
//                pBuffer -- Pointer to the buffer that contains the data 
//                           to write.
//                     
//                Count -- Specifies the number of bytes to write from 
//                         the pSourceBytes buffer into the device.
//
//    Returns:    The number of bytes written indicates success. A value
//                of -1 indicates failure.
//
//    Descript:   This function writes data to the device.
//
//------------------------------------------------------------------------------

extern DWORD ndp_Write(DWORD hOpen, LPCVOID pBuffer, DWORD count)
{
   DWORD result = -1;
   NDP_ADAPTER* pAdapter = (NDP_ADAPTER*)hOpen;

   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   goto cleanUp;

cleanUp:
   return result;
}

//------------------------------------------------------------------------------
//
//    Function:   ndp_Read
//
//    Arguments:  hOpenContext -- Handle to the open context of the
//                                device. The call to the ndp_Open
//                                function returns this identifier. 
//                     
//                     pBuffer -- Pointer to the buffer which stores
//                                the data read from the device. This
//                                buffer should be at least Count
//                                bytes long. 
//                     
//                       Count -- Specifies the number of bytes to
//                                read from the device into pBuffer.
//
//    Returns:    Returns 0 for the end-of-file, -1 for an error, or
//                the number of bytes read for success.
//
//    Descript:   This function reads data from the device identified
//                by the open context.
//
//------------------------------------------------------------------------------

extern DWORD ndp_Read(DWORD hOpen, LPVOID pBuffer, DWORD count)
{
   DWORD result = -1;
   NDP_ADAPTER* pAdapter = (NDP_ADAPTER*)hOpen;
   
   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   goto cleanUp;

cleanUp:
   return result;
}

//------------------------------------------------------------------------------
//
//    Function:   ndp_Seek
//
//    Arguments:  hOpenContext -- Handle to the open context of the
//                                device. The call to the ndp_Open
//                                function returns this identifier. 
//                     
//                      Amount -- Specifies the number of bytes to 
//                                move the data pointer in the device.
//                                A positive value moves the data
//                                pointer toward the end of the
//                                file, and a negative value moves
//                                it toward the beginning.
//                     
//                        Type -- Specifies the starting point for 
//                                the data pointer.
//
//    Returns:    The device’s new data pointer indicates success. A
//                value of -1 indicates failure.
//
//    Descript:   This function moves the data pointer in the device.
//
//                This function prototype, which is required to implement
//                a Windows CE stream driver interface, may not be changed.
//
//------------------------------------------------------------------------------

extern DWORD ndp_Seek(DWORD hOpen, long Amount, DWORD Type)
{
   return (DWORD)-1;
}

//------------------------------------------------------------------------------
//
//    Function:   ndp_IOControl
//
//    Arguments:   hOpenContext -- Handle to the open context of the
//                                 device. The ndp_Open function
//                                 creates and returns this identifier.          
//
//                       dwCode -- Specifies a value indicating the
//                                 I/O control operation to perform.
//                                 These codes are device-specific and
//                                 are usually exposed to programmers
//                                 by means of a header file. 
//
//                   pBufferInp -- Pointer to the buffer containing
//                                 data to be transferred to the device.           
//                          
//                  dwLengthInp -- Specifies the number of bytes of
//                                 data in the buffer specified for
//                                 pBufIn.   
//                          
//                   pBufferOut -- Pointer to the buffer used to transfer
//                                 the output data from the device.
//                          
//                  dwLengthOut -- Specifies the maximum number of bytes
//                                 in the buffer specified by pBufOut.
//                          
//                 pdwActualOut -- Pointer to the DWORD buffer that this
//                                 function uses to return the actual
//                                 number of bytes received from the
//                                 device. 
//                          
//    Returns:     TRUE indicates success. FALSE indicates failure.
//
//    Description: This function receives an IO Control Command via 
//                 shared memory area. Basic share memory parameters - name
//                 and size is in an input buffer. An output buffer
//                 contain return code. Asynchronous I/O is emulated
//                 through named event which is set when async I/O finish.
//                 The event name is passed in shared memory area.
//
//------------------------------------------------------------------------------

extern BOOL ndp_IOControl(
    DWORD hOpen, DWORD code, PBYTE pBufferIn, DWORD lengthIn, 
    PBYTE pBufferOut, DWORD lengthOut, PDWORD pActualLengthOut
)
{
   BOOL ok = FALSE;
   NDP_ADAPTER* pAdapter = (NDP_ADAPTER*)hOpen;
   UINT size;
   NDIS_STATUS status = NDIS_STATUS_INVALID_DATA;
   PNDP_OID_MP pOid;


   // By default we don't return data
   *pActualLengthOut = 0;
   
   // Check if we get instance handler
   if ( (!pAdapter) || (pAdapter->magicCookie != NDP_ADAPTER_COOKIE) )
	   goto cleanUp;

   switch (code) {
      case IOCTL_NDP_OPEN_MINIPORT:
         ok = ndp_OpenAdapter(pAdapter, (LPCTSTR)pBufferIn);
         break;
      case IOCTL_NDP_CLOSE_MINIPORT:
         ok = ndp_CloseAdapter(pAdapter);
         break;
      case IOCTL_NDP_SEND_PACKET:
         // Check input buffer size
         if (
            pAdapter->hAdapter == NULL ||
            lengthIn < sizeof(NDP_SEND_PACKET_INP) - 1
         ) break;
         size = sizeof(NDP_SEND_PACKET_INP) - 1;
         size += ((NDP_SEND_PACKET_INP*)pBufferIn)->dataSize;
         if (lengthIn < size) break;
         // Execute         
         ok = ndp_SendPacket(pAdapter, (NDP_SEND_PACKET_INP*)pBufferIn);
         break;
      case IOCTL_NDP_LISTEN:
         // Check input/output buffer size
         if (
            pAdapter->hAdapter == NULL ||
            lengthIn != sizeof(NDP_LISTEN_INP)
         ) break;
         // Execute         
         ok = ndp_Listen(pAdapter, (NDP_LISTEN_INP*)pBufferIn);
         break;
      case IOCTL_NDP_RECV_PACKET:
         // Check input/output buffer size
         if (
            pAdapter->hAdapter == NULL ||
            lengthIn != sizeof(NDP_RECV_PACKET_INP) ||
            lengthOut < sizeof(NDP_RECV_PACKET_OUT) - 1
         ) break;
         size = lengthOut - sizeof(NDP_RECV_PACKET_OUT) + 1;
         ((NDP_RECV_PACKET_OUT*)pBufferOut)->dataSize = size;
         // Execute         
         ok = ndp_RecvPacket(
            pAdapter, (NDP_RECV_PACKET_INP*)pBufferIn, 
            (NDP_RECV_PACKET_OUT*)pBufferOut
         );
         size = ((NDP_RECV_PACKET_OUT*)pBufferOut)->dataSize;
         size += sizeof(NDP_RECV_PACKET_OUT) - 1;
         *pActualLengthOut = size;
         break;
      case IOCTL_NDP_STRESS_SEND:
         // Check input/output buffer size
         if (
            pAdapter->hAdapter == NULL ||
            lengthIn != sizeof(NDP_STRESS_SEND_INP) ||
            lengthOut != sizeof(NDP_STRESS_SEND_OUT)
         ) break;
         // Execute         
         ok = ndp_StressSend(
            pAdapter, (NDP_STRESS_SEND_INP*)pBufferIn, 
            (NDP_STRESS_SEND_OUT*)pBufferOut
         );
         *pActualLengthOut = sizeof(NDP_STRESS_SEND_OUT);
         break;
      case IOCTL_NDP_STRESS_RECV:
         // Check input/output buffer size
         if (
            pAdapter->hAdapter == NULL ||
            lengthIn != sizeof(NDP_STRESS_RECV_INP) ||
            lengthOut < sizeof(NDP_STRESS_RECV_OUT) - 1
         ) break;
         // Set maximal data size
         size = lengthOut - sizeof(NDP_STRESS_RECV_OUT) + 1;
         ((NDP_STRESS_RECV_OUT*)pBufferOut)->dataSize = size;
         // Execute         
         ok = ndp_StressReceive(
            pAdapter, (NDP_STRESS_RECV_INP*)pBufferIn, 
            (NDP_STRESS_RECV_OUT*)pBufferOut
         );
         // Get actual return size         
         size = ((NDP_STRESS_RECV_OUT*)pBufferOut)->dataSize;
         size += sizeof(NDP_STRESS_RECV_OUT) - 1;
         *pActualLengthOut = size;
         break;

	  case IOCTL_NDP_MP_OID:
		  pOid = (PNDP_OID_MP) pBufferIn;
		  // Check input/output buffer size
		  if (
			  (pAdapter->hAdapter == NULL) ||
			  (lengthIn != sizeof(NDP_OID_MP)) ||
			  (pBufferIn == NULL)
			  ) break;
		  // Execute
		  if (pOid->eType == QUERY)
		  {//If its Query OID.
			  status = QueryOid(pAdapter,pOid->oid,pOid->lpInOutBuffer,pOid->pnInOutBufferSize);
			  if (status == NDIS_STATUS_SUCCESS) ok = TRUE;
		  }
		  else
		  {
			  if (pOid->eType == SET)
			  {//If its SET OID.
				  status = SetOid(pAdapter,pOid->oid,pOid->lpInOutBuffer,pOid->pnInOutBufferSize);
				  if (status == NDIS_STATUS_SUCCESS) ok = TRUE;
			  }
			  else
				  break;
		  }

		  pOid->status = status;
		  //Just for now.
		  *pActualLengthOut = sizeof(NDP_OID_MP);
		  break;
   }
   
cleanUp:   
   return ok;
}

//------------------------------------------------------------------------------
//
//    Function:   ndp_PowerUp
//
//    Arguments:  hOpenContext -- Handle to the open context of the
//                                device. The call to the ndp_Open
//                                function returns this identifier. 
//
//    Returns:    (none)
//
//    Descript:   This function restores power to a device.
//
//------------------------------------------------------------------------------

extern void ndp_PowerUp(DWORD hOpen)
{
   return;
}

//------------------------------------------------------------------------------
//
//    Function:   ndp_PowerDown
//
//    Arguments:  hOpenContext -- Handle to the open context of the
//                                device. The call to the ndp_Open
//                                function returns this identifier. 
//
//    Returns:    (none)
//
//    Descript:   This function suspends power to the device. It is
//                useful only with devices that can be shut off under
//                software control. Such devices are typically, but
//                not exclusively, PC Card devices.
//
//------------------------------------------------------------------------------

extern void ndp_PowerDown(DWORD hOpen)
{
   return;
}

//------------------------------------------------------------------------------

#endif //UNDER_CE