//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include <winioctl.h>
#include <ntddndis.h>
#include "Utils.h"
#include "Messages.h"
#include "NDT_CE.h"
#include "NDTError.h"
#include "NDTLog.h"

//------------------------------------------------------------------------------

typedef int NDIS_STATUS, *PNDIS_STATUS;
#define NDIS_STATUS_SUCCESS                  ((DWORD)0x00000000L)
#define NDIS_STATUS_PENDING                  ((DWORD)0x00000103L)

//------------------------------------------------------------------------------

typedef struct {
   HANDLE hShare;
   PVOID  pvShare;
   HANDLE hEvent;
   PVOID  pvOutBuffer;
   DWORD *pcbOutBuffer;
} NDT_IOCONTROL_OVERLAPPED;

//------------------------------------------------------------------------------

HANDLE g_hDevice = NULL;
HANDLE g_hDriver = NULL;

//------------------------------------------------------------------------------

HRESULT Open()
{
   DWORD rc = ERROR_SUCCESS;
   HRESULT hr = S_OK;

   // To load the protocol driver...
   g_hDevice = RegisterDevice(_T("ndt"), 1, _T("ndt.dll"), 0);
   if (g_hDevice == NULL) {
      rc = GetLastError();
      NDTLog(NDT_ERR_DRIVER_REGISTER, rc);
      hr = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }

   g_hDriver = CreateFile(
      _T("NDT1:"), GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL
   );
   
   if (g_hDriver == INVALID_HANDLE_VALUE) {
      g_hDriver = NULL;
      rc = GetLastError();
      NDTLog(NDT_ERR_DRIVER_OPEN, rc);
      hr = HRESULT_FROM_WIN32(rc);
   }

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT Close()
{
   HRESULT hr = S_OK;
   DWORD rc = ERROR_SUCCESS;

   if (!CloseHandle(g_hDriver)) {
      rc = GetLastError();
      NDTLog(NDT_ERR_DRIVER_CLOSE, rc);
      hr = HRESULT_FROM_WIN32(rc);
   }
   g_hDriver = NULL;

   if (!DeregisterDevice(g_hDevice)) {
      rc = GetLastError();
      NDTLog(NDT_ERR_DRIVER_DEREGISTER, rc);
      if (hr == S_OK) hr = HRESULT_FROM_WIN32(rc);
   }
   g_hDevice = NULL;
   return hr;    
}

//------------------------------------------------------------------------------

HRESULT StartIoControl(
   DWORD dwCode, PVOID pvInpBuffer, DWORD cbInpBuffer, PVOID pvOutBuffer, 
   DWORD cbOutBuffer, DWORD* pcbOutBuffer, PVOID* ppvOverlapped
)
{
   static DWORD dwIoControlCount = 0;
   NDT_IOCONTROL_OVERLAPPED *pOverlapped = NULL;
   NDT_IOCONTROL_REQUEST req;
   NDT_IOCONTROL_SHARE *pShare = NULL;
   BOOL bOk = FALSE;
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   HRESULT hr = S_OK;
   DWORD rc = 0;
   DWORD cb = 0;

   // Cleanup local structures
   memset(&req, 0, sizeof(req));

   // New call, so update counter
   dwIoControlCount++;

   // Create structure for overlapped info
   pOverlapped = new NDT_IOCONTROL_OVERLAPPED;
   if (pOverlapped == NULL) {
      rc = GetLastError();
      NDTLog(NDT_ERR_DEVICE_OVERLAPPED_ALLOC);
      hr = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }
   memset(pOverlapped, 0, sizeof(NDT_IOCONTROL_OVERLAPPED));
   
   // Share memory name and size
   wsprintfW(req.szShareName, L"NDTSM%08x", dwIoControlCount);
   req.dwShareSize = sizeof(NDT_IOCONTROL_SHARE);
   req.dwShareSize += cbInpBuffer + cbOutBuffer;

   // Map shared memory to share data
   pOverlapped->hShare = CreateFileMapping(
       INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | SEC_COMMIT, 
       0, req.dwShareSize, req.szShareName
   );
   if (pOverlapped->hShare == NULL) {
      rc = GetLastError();
      NDTLog(NDT_ERR_DEVICE_CREATEFILEMAPPING, req.szShareName, rc);
      hr = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }
    
   pOverlapped->pvShare = MapViewOfFile(
      pOverlapped->hShare, FILE_MAP_ALL_ACCESS, 0, 0, 0
   );
   if (pOverlapped->pvShare == NULL) {
      rc = GetLastError();
      NDTLog(NDT_ERR_DEVICE_MAPFILEVIEW, req.szShareName, rc);
      hr = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }
   pShare = (NDT_IOCONTROL_SHARE *)pOverlapped->pvShare;

   wsprintfW(pShare->szEventName, L"NDTEV%08x", dwIoControlCount);
   pShare->dwInputOffset = sizeof(NDT_IOCONTROL_SHARE);
   pShare->dwInputSize = cbInpBuffer;
   pShare->dwOutputOffset = pShare->dwInputOffset + pShare->dwInputSize;
   pShare->dwOutputSize = cbOutBuffer;
   pShare->dwActualOutputSize = 0;
   pShare->status = NDIS_STATUS_SUCCESS;

   pOverlapped->hEvent = CreateEvent(NULL, TRUE, FALSE, pShare->szEventName);
   if (pOverlapped->hEvent == NULL) {
      rc = GetLastError();
      NDTLog(NDT_ERR_DEVICE_CREATEEVENT, pShare->szEventName, rc);
      hr = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }

   // Copy input buffer
   memcpy(
      (PBYTE)pOverlapped->pvShare + pShare->dwInputOffset, 
      pvInpBuffer, cbInpBuffer
   );

   // Save an output buffer pointer
   pOverlapped->pvOutBuffer = pvOutBuffer;

   // There we will set actual returned size
   pOverlapped->pcbOutBuffer = pcbOutBuffer;

   // Start device
   bOk = DeviceIoControl(
      g_hDriver, dwCode, &req, sizeof(req), &status, sizeof(status), &cb, NULL
   );

   // Check result 
   if (!bOk) {
      rc = GetLastError();
      NDTLog(NDT_ERR_DEVICE_IOCONTROL, rc);
      hr = HRESULT_FROM_WIN32(rc);
      goto cleanUp;
   }

   // Check NDIS result
   if (status != NDIS_STATUS_SUCCESS && status != NDIS_STATUS_PENDING) {
      NDTLog(NDT_ERR_DEVICE_REQUEST, status);
      hr = HRESULT_FROM_NDIS(status);
      goto cleanUp;
   }

   if (status == NDIS_STATUS_SUCCESS) {
      memcpy(
         pvOutBuffer, (PBYTE)pOverlapped->pvShare + pShare->dwOutputOffset, 
         pShare->dwActualOutputSize
      );
      *pcbOutBuffer = pShare->dwActualOutputSize;
      CloseHandle(pOverlapped->hEvent);
      UnmapViewOfFile(pOverlapped->pvShare);
      CloseHandle(pOverlapped->hShare);
      delete pOverlapped;
      pOverlapped = NULL;
      hr = S_OK;
   } else {
      hr = HRESULT_FROM_NDIS(status);
   }
      
   // Return
   *ppvOverlapped = pOverlapped;
   
   // We are done...
   return hr;

cleanUp:
   if (pOverlapped->hEvent != NULL) CloseHandle(pOverlapped->hEvent);
   if (pOverlapped->pvShare != NULL) UnmapViewOfFile(pOverlapped->pvShare);
   if (pOverlapped->hShare != NULL) CloseHandle(pOverlapped->hShare);
   delete pOverlapped;
   *ppvOverlapped = NULL;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT StopIoControl(PVOID* ppvOverlapped)
{
   HRESULT hr = S_OK;
   NDT_IOCONTROL_OVERLAPPED* pOverlapped;

   // Get a pointer
   pOverlapped = (NDT_IOCONTROL_OVERLAPPED *)*ppvOverlapped;
   if (pOverlapped != NULL) {
      CloseHandle(pOverlapped->hEvent);
      UnmapViewOfFile(pOverlapped->pvShare);
      CloseHandle(pOverlapped->hShare);
      delete pOverlapped;
      *ppvOverlapped = NULL;
   }
   return hr;
}

//------------------------------------------------------------------------------

HRESULT WaitForIoControl(PVOID* ppvOverlapped, DWORD dwTimeout)
{
   NDT_IOCONTROL_OVERLAPPED *pOverlapped;
   NDT_IOCONTROL_SHARE *pShare;
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   DWORD rc = 0;
   HRESULT hr = S_OK;

   // Get a pointer
   pOverlapped = (NDT_IOCONTROL_OVERLAPPED *)*ppvOverlapped;
   pShare = (NDT_IOCONTROL_SHARE *)pOverlapped->pvShare;

   // Wait for complete
   rc = WaitForSingleObject(pOverlapped->hEvent, dwTimeout);
   switch (rc) {
   case WAIT_OBJECT_0:
      memcpy(
         pOverlapped->pvOutBuffer, (PBYTE)pShare + pShare->dwOutputOffset, 
         pShare->dwActualOutputSize
      );
      *pOverlapped->pcbOutBuffer = pShare->dwActualOutputSize;
      hr = HRESULT_FROM_NDIS(pShare->status);
      break;
   case WAIT_TIMEOUT:
      hr = NDT_STATUS_PENDING;
      break;
   default:
      rc = GetLastError();
      hr = HRESULT_FROM_WIN32(rc);
   }

   // If request isn't pending anymore just leave
   if (hr != NDT_STATUS_PENDING) {
      CloseHandle(pOverlapped->hEvent);
      UnmapViewOfFile(pOverlapped->pvShare);
      CloseHandle(pOverlapped->hShare);
      delete pOverlapped;
      *ppvOverlapped = NULL;
   }
   
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDISIOControl(
   DWORD dwCommand, LPVOID pInBuffer, DWORD cbInBuffer, LPVOID pOutBuffer,
   DWORD *pcbOutBuffer
)
{
   HANDLE hNDIS;
   DWORD rc = ERROR_SUCCESS;
   DWORD cbOutBuffer;

   hNDIS = CreateFile(
      DD_NDIS_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL
   );

   if (hNDIS == INVALID_HANDLE_VALUE) {
      rc = GetLastError();
      goto cleanUp;
   }

   cbOutBuffer = 0;
   if (pcbOutBuffer) cbOutBuffer = *pcbOutBuffer;

   if (!DeviceIoControl(
      hNDIS, dwCommand, pInBuffer, cbInBuffer, pOutBuffer, cbOutBuffer,
      &cbOutBuffer, NULL
   )) {
      rc = GetLastError();
   }

   if (pcbOutBuffer) *pcbOutBuffer = cbOutBuffer;
   CloseHandle(hNDIS);
   
cleanUp:
   return HRESULT_FROM_WIN32(rc);
}

//------------------------------------------------------------------------------

HRESULT LoadAdapter(LPCTSTR szAdapter)
{
   HRESULT hr = S_OK;
   TCHAR mszBuffer[256];
   DWORD cbBuffer = 0;
   TCHAR *pc = mszBuffer;

   // First adapter name without last character
   lstrcpy(pc, szAdapter);
   pc += lstrlen(szAdapter) - 1;
   *pc++ = _T('\0');
   cbBuffer += lstrlen(szAdapter);
   // Now full adapter name   
   lstrcpy(pc, szAdapter);
   pc += lstrlen(szAdapter) + 1;
   cbBuffer += lstrlen(szAdapter) + 1;
   *pc = _T('\0');
   cbBuffer = (cbBuffer + 1) * sizeof(TCHAR);

   hr = NDISIOControl(
      IOCTL_NDIS_REGISTER_ADAPTER, mszBuffer, cbBuffer, NULL, NULL 
   );
   return hr;
}

//------------------------------------------------------------------------------

HRESULT UnloadAdapter(LPCTSTR szAdapter)
{
   HRESULT hr = S_OK;
   TCHAR mszBuffer[256];
   DWORD cbBuffer = 0;
   TCHAR *pc = mszBuffer;

   // Full adapter name   
   lstrcpy(pc, szAdapter);
   pc += lstrlen(szAdapter) + 1;
   cbBuffer += lstrlen(szAdapter) + 1;
   *pc = _T('\0');
   cbBuffer = (cbBuffer + 1) * sizeof(TCHAR);

   hr = NDISIOControl(
      IOCTL_NDIS_DEREGISTER_ADAPTER, mszBuffer, cbBuffer, NULL, NULL 
   );
   return hr;
}

//------------------------------------------------------------------------------

HRESULT QueryAdapters(LPTSTR mszAdapters, DWORD dwSize)
{
   HRESULT hr = S_OK;
   DWORD cbAdapters = dwSize;

   hr = NDISIOControl(
      IOCTL_NDIS_GET_ADAPTER_NAMES, NULL, 0, (LPVOID)mszAdapters, &cbAdapters
   );
   return hr;
}

//------------------------------------------------------------------------------

HRESULT QueryProtocols(LPTSTR mszProtocols, DWORD dwSize)
{
   HRESULT hr = S_OK;
   DWORD cbProtocols = dwSize;

   hr = NDISIOControl(
      IOCTL_NDIS_GET_PROTOCOL_NAMES, NULL, 0, (LPVOID)mszProtocols, &cbProtocols
   );
   return hr;
}

//------------------------------------------------------------------------------

HRESULT QueryBindings(
   LPCTSTR szAdapter, LPTSTR mszProtocols, DWORD dwSize
)
{
   HRESULT hr = S_OK;
   DWORD cbProtocols = dwSize;

   hr = NDISIOControl(
      IOCTL_NDIS_GET_ADAPTER_BINDINGS, (LPVOID)szAdapter, 
      (lstrlen(szAdapter) + 1) * sizeof(TCHAR), (LPVOID)mszProtocols, 
      &cbProtocols
   );
   return hr;
}

//------------------------------------------------------------------------------

HRESULT BindProtocol(LPCTSTR szAdapterName, LPCTSTR szProtocol)
{
   HRESULT hr = S_OK;
   TCHAR mszBuffer[256];
   DWORD cbBuffer = 0;
   TCHAR *pc = mszBuffer;

   lstrcpy(pc, szAdapterName);
   pc += lstrlen(szAdapterName) + 1;
   cbBuffer += lstrlen(szAdapterName) + 1;
   if (szProtocol != NULL) {
      lstrcpy(pc, szProtocol);
      pc += lstrlen(szProtocol) + 1;
      cbBuffer += lstrlen(szProtocol) + 1;
   }
   *pc = _T('\0');
   cbBuffer = (cbBuffer + 1) * sizeof(TCHAR);
  
   hr = NDISIOControl(
      IOCTL_NDIS_BIND_ADAPTER, mszBuffer, cbBuffer, NULL, NULL 
   );
   return hr;
}

//------------------------------------------------------------------------------

HRESULT UnbindProtocol(LPCTSTR szAdapter, LPCTSTR szProtocol)
{
   HRESULT hr = S_OK;
   TCHAR mszBuffer[256];
   DWORD cbBuffer = 0;
   TCHAR *pc = mszBuffer;

   lstrcpy(pc, szAdapter);
   pc += lstrlen(szAdapter) + 1;
   cbBuffer += lstrlen(szAdapter) + 1;
   if (szProtocol != NULL) {
      lstrcpy(pc, szProtocol);
      pc += lstrlen(szProtocol) + 1;
      cbBuffer += lstrlen(szProtocol) + 1;
   }
   *pc = _T('\0');
   cbBuffer = (cbBuffer + 1) * sizeof(TCHAR);
  
   hr = NDISIOControl(
      IOCTL_NDIS_UNBIND_ADAPTER, mszBuffer, cbBuffer, NULL, NULL 
   );
   return hr;
}

//------------------------------------------------------------------------------

HRESULT WriteVerifyFlag(LPCTSTR szAdapter, DWORD dwFlag)
{
   HRESULT hr = S_OK;
   TCHAR szBuffer[256];
   LONG rc = ERROR_SUCCESS;
   HKEY hKey = NULL;

   StringCchCopy(szBuffer, 256, _T("Comm\\"));
   StringCchCat(szBuffer, 256, szAdapter);
   StringCchCat(szBuffer, 256, _T("\\Parms"));

   rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szBuffer, 0, 0, &hKey);
   if (rc != ERROR_SUCCESS) goto cleanUp;

   rc = RegSetValueEx(
      hKey, _T("NdisDriverVerifyFlags"), 0, REG_DWORD, (BYTE*)&dwFlag, 
      sizeof(dwFlag)
   );
   
cleanUp:
   if (hKey != NULL) RegCloseKey(hKey);
   if (rc != ERROR_SUCCESS) hr = HRESULT_FROM_WIN32(rc);
   return hr;
}

//------------------------------------------------------------------------------

HRESULT DeleteVerifyFlag(LPCTSTR szAdapter)
{
   HRESULT hr = S_OK;
   TCHAR szBuffer[256];
   LONG rc = ERROR_SUCCESS;
   HKEY hKey = NULL;

   StringCchCopy(szBuffer, 256, _T("Comm\\"));
   StringCchCat(szBuffer, 256, szAdapter);
   StringCchCat(szBuffer, 256, _T("\\Parms"));

   rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szBuffer, 0, 0, &hKey);
   if (rc != ERROR_SUCCESS) goto cleanUp;

   rc = RegDeleteValue(hKey, _T("NdisDriverVerifyFlags"));
   
cleanUp:
   if (hKey != NULL) RegCloseKey(hKey);
   if (rc != ERROR_SUCCESS) hr = HRESULT_FROM_WIN32(rc);
   return hr;}

//------------------------------------------------------------------------------
