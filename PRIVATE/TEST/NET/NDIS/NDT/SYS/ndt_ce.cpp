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
#include "NDT_CE.h"
#include "Driver.h"
#include "Memory.h"
#include "Log.h"

//------------------------------------------------------------------------------

typedef struct {
   HANDLE hShare;
   PVOID  pvShare;
   HANDLE hEvent;
} NDT_IOCONTROL_OVERLAPPED;

//------------------------------------------------------------------------------
//
//    Function:   ndt_Init
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

extern "C" DWORD ndt_Init(DWORD dwContext)
{
   DWORD rc = 0;
   CDriver *pDriver = NULL;

   // First we need to initialize memory
   InitializeMemory();
   // Now create a device object
   pDriver = new CDriver;
   // Do initialization
   if (!pDriver->Init(dwContext)) {
      pDriver->Release(); pDriver = NULL;
      DeinitializeMemory();
   }
   return (DWORD)pDriver;
}

//------------------------------------------------------------------------------
//
//    Function:   ndt_Deinit
//
//    Arguments:  hDeviceContext - Handle to the device context. The
//                                 ndt_Init function creates and
//                                 returns this identifier. 
//
//    Returns:    TRUE indicates success. FALSE indicates failure.
//
//    Descript:   This function is called by the Device Manager to
//                deinitialize a device.
//
//------------------------------------------------------------------------------

extern "C" BOOL ndt_Deinit(DWORD hDeviceContext)
{
   // Deinitializce device and destroy the object
   CDriver* pDriver = (CDriver*)hDeviceContext;
   ASSERT(pDriver->m_dwMagic == NDT_MAGIC_DRIVER);
   pDriver->Deinit();
   pDriver->Release();
   // Deinitialize memory
   DeinitializeMemory();
   return TRUE;
}

//------------------------------------------------------------------------------
//
//    Function:   ndt_Open
//
//    Arguments:  hDeviceContext -- Handle to the device context.
//                                  The ndt_Init function creates and
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
//                the ndt_Read, ndt_Write, ndt_Seek, and ndt_IOControl
//                functions. If the device cannot be opened, this function
//                returns NULL.
//
//    Descript:   This function opens a the NDISTest Stream Interface
//                Driver for Windows CE.
//
//------------------------------------------------------------------------------

extern "C" DWORD ndt_Open(
    DWORD hDeviceContext, DWORD dwAccessCode, DWORD dwShareMode
)
{
   // We allow to open only one instance of device to be open
   return hDeviceContext;
}

//------------------------------------------------------------------------------
//
//    Function:   ndt_Close
//
//    Arguments:    hOpenContext -- Handle returned by the ndt_Open
//                                  function, used to identify the open 
//                                  context of the device.
//
//    Returns:    TRUE indicates success. FALSE indicates failure.
//
//    Descript:   This function closes the device context identified by 
//                hOpenContext.
//
//------------------------------------------------------------------------------

extern "C" BOOL ndt_Close(DWORD hOpenContext) 
{
   return TRUE;
}

//------------------------------------------------------------------------------
//
//    Function:   ndt_Write
//
//    Arguments:  hOpenContext -- Handle to the open context of the
//                                device. The call to the ndt_Open
//                                function returns this identifier. 
//                     
//                     pBuffer -- Pointer to the buffer that contains
//                                the data to write.
//                     
//                       Count -- Specifies the number of bytes to
//                                write from the pSourceBytes buffer
//                                into the device.
//
//    Returns:    The number of bytes written indicates success. A
//                value of -1 indicates failure.
//
//    Descript:   This function writes data to the device.
//
//------------------------------------------------------------------------------

extern "C" DWORD ndt_Write (DWORD hOpenContext, LPCVOID pBuffer, DWORD Count)
{
   return (DWORD)-1;
}

//------------------------------------------------------------------------------
//
//    Function:   ndt_Read
//
//    Arguments:  hOpenContext -- Handle to the open context of the
//                                device. The call to the ndt_Open
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

extern "C" DWORD ndt_Read (DWORD hOpenContext, LPVOID pBuffer, DWORD Count)
{
   return (DWORD)-1;
}

//------------------------------------------------------------------------------
//
//    Function:   ndt_Seek
//
//    Arguments:  hOpenContext -- Handle to the open context of the
//                                device. The call to the ndt_Open
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

extern "C" DWORD ndt_Seek (DWORD hOpenContext, long Amount, DWORD Type)
{
   return (DWORD)-1;
}

//------------------------------------------------------------------------------
//
//    Function:   ndt_IOControl
//
//    Arguments:   hOpenContext -- Handle to the open context of the
//                                 device. The ndt_Open function
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

extern "C" BOOL ndt_IOControl(
    DWORD hOpenContext, DWORD dwCode, PBYTE pBufIn, DWORD dwLenIn, 
    PBYTE pBufOut, DWORD dwLenOut, PDWORD pdwActualOut
)
{
   BOOL bOk = FALSE;
   NDIS_STATUS status = NDIS_STATUS_FAILURE;
   DWORD rc = ERROR_SUCCESS;
   NDT_IOCONTROL_OVERLAPPED *pOverlapped = NULL;
   NDT_IOCONTROL_REQUEST *pRequest = (NDT_IOCONTROL_REQUEST*)pBufIn;
   NDT_IOCONTROL_SHARE *pShare = NULL;
   PVOID pvInpBuffer = NULL;
   PVOID pvOutBuffer = NULL;
   NDT_ENUM_REQUEST_TYPE eRequest = NDT_REQUEST_UNKNOWN;
   CDriver *pDriver = (CDriver*)hOpenContext;

   // Check cast
   ASSERT(pDriver->m_dwMagic == NDT_MAGIC_DRIVER);
      
   // Check parameters
   if (pBufIn == NULL || dwLenIn < sizeof(NDT_IOCONTROL_REQUEST)) goto cleanUp;
   if (pBufOut == NULL || dwLenOut < sizeof(NDIS_STATUS)) goto cleanUp;

   pOverlapped = new NDT_IOCONTROL_OVERLAPPED;
   if (pOverlapped == NULL) goto cleanUp;
   
   // Map shared memory to get command 
   pOverlapped->hShare = CreateFileMapping(
      INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | SEC_COMMIT, 
      0, pRequest->dwShareSize, pRequest->szShareName
   );
   rc = GetLastError();
   if (pOverlapped->hShare == NULL || rc != ERROR_ALREADY_EXISTS) {
      status = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }
    
   pOverlapped->pvShare = MapViewOfFile(
      pOverlapped->hShare, FILE_MAP_ALL_ACCESS, 0, 0, 0
   );
   if (pOverlapped->pvShare == NULL) {
      rc = GetLastError();
      status = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }
   pShare = (NDT_IOCONTROL_SHARE *)pOverlapped->pvShare;

   // An event used for waiting on STATUS_PENDING
   pOverlapped->hEvent = CreateEvent(NULL, TRUE, FALSE, pShare->szEventName);
   if (pOverlapped->hEvent == NULL) {
      rc = GetLastError();
      status = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }

   // Get input & output buffers
   pvInpBuffer = (PVOID)((PBYTE)pShare + pShare->dwInputOffset);
   pvOutBuffer = (PVOID)((PBYTE)pShare + pShare->dwOutputOffset);

   // Get request code
   eRequest = (NDT_ENUM_REQUEST_TYPE)dwCode;
   
   // Issue a command
   status = pDriver->IOControl(
      eRequest, pvInpBuffer, pShare->dwInputSize, pvOutBuffer, 
      pShare->dwOutputSize, &pShare->dwActualOutputSize, (PVOID)pOverlapped
   );

   // We ware succesfull
   bOk = TRUE;
   
cleanUp:   
   // Save result
   if (pBufOut != NULL && pdwActualOut != NULL) {
      *(NDIS_STATUS *)pBufOut = status;
      *pdwActualOut = sizeof(NDIS_STATUS);
   }
   return bOk;
}

//------------------------------------------------------------------------------
//
//    Function:   ndt_PowerUp
//
//    Arguments:  hOpenContext -- Handle to the open context of the
//                                device. The call to the ndt_Open
//                                function returns this identifier. 
//
//    Returns:    (none)
//
//    Descript:   This function restores power to a device.
//
//------------------------------------------------------------------------------

extern "C" void ndt_PowerUp(DWORD hOpenContext)
{
   return;
}

//------------------------------------------------------------------------------
//
//    Function:   ndt_PowerDown
//
//    Arguments:  hOpenContext -- Handle to the open context of the
//                                device. The call to the ndt_Open
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

extern "C" void ndt_PowerDown(DWORD hOpenContext)
{
   return;
}

//------------------------------------------------------------------------------

void NDTCompleteRequest(PVOID pvComplete)
{
   NDT_IOCONTROL_OVERLAPPED *pOverlapped;

   pOverlapped = (NDT_IOCONTROL_OVERLAPPED *)pvComplete;
   SetEvent(pOverlapped->hEvent);
   CloseHandle(pOverlapped->hEvent);
   UnmapViewOfFile(pOverlapped->pvShare);
   CloseHandle(pOverlapped->hShare);
   delete pOverlapped; 
}

//------------------------------------------------------------------------------
