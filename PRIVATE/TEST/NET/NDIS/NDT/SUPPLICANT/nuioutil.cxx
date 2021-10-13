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
#include "stdafx.h"

//===============================================================================================//
HANDLE NdisUioCreate()
{
   NDTLogVbs(_T(": Enter\n"));

   DWORD                   nCreationDistribution   = OPEN_EXISTING;
   DWORD                   nFlagsAndAttributes     = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED;
   HANDLE                  hTemplateFile           = (HANDLE)INVALID_HANDLE_VALUE;
   DWORD                   nBytesReturned          = 0;
   DWORD                   nDesiredAccess          = GENERIC_READ|GENERIC_WRITE;;
   DWORD                   nShareMode              = 0;
   BOOL                    bRetval                 = FALSE;
   LPSECURITY_ATTRIBUTES   pSecurityAttributes     = NULL;
   HANDLE                  handle                  = INVALID_HANDLE_VALUE;

   // Create the handle to NdisUio
   handle = CreateFile(
                     NDISUIO_DEVICE_NAME, 
                       nDesiredAccess, 
                       nShareMode, 
                       pSecurityAttributes, 
                       nCreationDistribution,
                       nFlagsAndAttributes,
                       hTemplateFile);
   if(handle == INVALID_HANDLE_VALUE)
   {
       NDTLogErr( _T("CreateFile() failed (Error = %d)\n"), GetLastError());
       return INVALID_HANDLE_VALUE;
   }

//? WHy do this ?? if we are doing it on return
   //  Wait for NdisUio to complete binding to wireless drivers.
//   bRetval = DeviceIoControl(handle, 
//                             IOCTL_NDISUIO_BIND_WAIT,
//                             NULL,
//                             0,
//                             NULL,
//                             0,
//                             &nBytesReturned,
//                             NULL);
//   if(bRetval == FALSE)
//   {
//      NDTLogErr( _T("IOCTL_NDISIO_BIND_WAIT failed (Error %d)\n"), GetLastError());
//      return INVALID_HANDLE_VALUE;
//   }
   
   NDTLogVbs(_T(": Exit\n"));
   return handle;
}

//===============================================================================================//
BOOL NdisUioClose(HANDLE handle)
{   
   NDTLogVbs(_T(": Enter\n"));

   // If the handle is not already open, then just return success
   if(handle == INVALID_HANDLE_VALUE)
   {
      NDTLogErr( _T("Invalid handle specified\n"));
      return FALSE;
   }

   if(CloseHandle(handle) == FALSE)
   {
      NDTLogErr( _T("CloseHandle() failed (Error: %d)\n"), GetLastError());
      return FALSE;
   }
   
   NDTLogVbs(_T(": Exit\n"));
   return TRUE;
}

//===============================================================================================//
DWORD NdisUioQueryOIDValue(IN HANDLE hHandle, IN NDIS_OID Oid, IN BYTE *pbyOidData, IN ULONG *pnOidDataLen)
{
   NDTLogVbs(_T(": Enter\n"));

    PNDISUIO_QUERY_OID  pQueryOid       = NULL;
    DWORD               nBytesReturned  = 0;
    BOOL                bRetval         = TRUE;
    //BOOL                bReturn         = FALSE;
    DWORD               nErrCode        = ERROR_SUCCESS;

    // Allocate memory for the query buffer
    pQueryOid = (PNDISUIO_QUERY_OID)malloc(*pnOidDataLen + sizeof(NDISUIO_QUERY_OID)); 
    if(pQueryOid == NULL)
    {
        NDTLogErr( _T("Failed to allocate memory for pQueryOid\n"));
        goto CleanUp;
    }
    
    pQueryOid->Oid = Oid;

    // Added for CE
    pQueryOid->ptcDeviceName = NULL;

    // Query the OID
    bRetval = DeviceIoControl(hHandle, 
                              IOCTL_NDISUIO_QUERY_OID_VALUE,
                              (LPVOID)pQueryOid,
                              FIELD_OFFSET(NDISUIO_QUERY_OID, Data) + *pnOidDataLen,
                              (LPVOID)pQueryOid,
                              FIELD_OFFSET(NDISUIO_QUERY_OID, Data) + *pnOidDataLen,
                              &nBytesReturned,
                              NULL);
    if(bRetval == FALSE)
    {
        nErrCode = GetLastError();
        NDTLogErr( _T("IOCTL_NDISUIO_QUERY_OID_VALUE failed (BytesReturned = %ld) (Error = %d)\n"), nBytesReturned, nErrCode);
        *pnOidDataLen = nBytesReturned;
        goto CleanUp;
    }
    else
    {
        nBytesReturned -= FIELD_OFFSET(NDISUIO_QUERY_OID, Data);

        if(nBytesReturned > *pnOidDataLen)
        {
            nBytesReturned = *pnOidDataLen;
        }
        else
        {
            *pnOidDataLen = nBytesReturned;
        }

        memcpy(pbyOidData, &pQueryOid->Data[0], nBytesReturned);
        
    }
    
CleanUp:

    if(pQueryOid != NULL)
    {
        free(pQueryOid);
    }

   NDTLogVbs(_T(": Exit\n"));
    return nErrCode;
}

//===============================================================================================//
DWORD NdisuioSetOIDValue(IN HANDLE hHandle, IN NDIS_OID Oid, IN BYTE *pbyOidData, IN ULONG nOidDataLen)
{
   NDTLogVbs(_T(": Enter\n"));

   PNDISUIO_SET_OID    pSetOid         = NULL;
   DWORD               BytesReturned   = 0;
   BOOL                bRetval         = FALSE;
   DWORD               nErrCode        = 100;
    
   if(nOidDataLen > FIELD_OFFSET(NDIS_802_11_KEY,KeyMaterial) + 32)
   {
      NDTLogErr( _T("Length to large (Length = %d)\n"), nOidDataLen);
      DebugBreak();
   }

   do
   {
      pSetOid = (PNDISUIO_SET_OID) malloc(nOidDataLen + sizeof(NDISUIO_SET_OID)); 
      if(pSetOid == NULL)
      {
          NDTLogErr( _T("Failed to allocate memory for pSetOid\n"));
          break;
      }
        
      pSetOid->Oid = Oid;
      pSetOid->ptcDeviceName = NULL;

      memcpy(&pSetOid->Data[0], pbyOidData, nOidDataLen);

      bRetval = DeviceIoControl(hHandle, 
                                IOCTL_NDISUIO_SET_OID_VALUE,
                                (LPVOID)pSetOid,
                                FIELD_OFFSET(NDISUIO_SET_OID, Data) + nOidDataLen,
                                NULL,
                                0,
                                &BytesReturned,
                                NULL);
      if(bRetval == FALSE)
      {
          NDTLogErr( _T("IOCTL_NDISUIO_SET_OID_VALUE failed (Error = %d)\n"), GetLastError());
          break;
      }
         
      nErrCode = ERROR_SUCCESS;

   }while(FALSE);
 
   free(pSetOid);
   
   NDTLogVbs(_T(": Exit\n"));
   return nErrCode;
}
