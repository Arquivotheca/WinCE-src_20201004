//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __REMOTE_DRIVER_H
#define __REMOTE_DRIVER_H

//------------------------------------------------------------------------------

#include "NDT_Protocol.h"
#include "Driver.h"

//------------------------------------------------------------------------------

class CRemoteDriver : public CDriver
{
private:
   SOCKET m_socket;
   USHORT m_usPort;
   SOCKADDR_IN m_saLocal;
   SOCKADDR_IN m_saRemote;

   UCHAR  m_aucInpBuffer[sizeof(NDT_CMD_HEADER) + 1024];
   DWORD  m_cbInpBufferSize;
   UCHAR* m_pucInpBuffer;
   DWORD  m_cbInpBuffer;
   UCHAR  m_aucOutBuffer[sizeof(NDT_CMD_HEADER) + 1024];
   DWORD  m_cbOutBufferSize;
   UCHAR* m_pucOutBuffer;
   DWORD  m_cbOutBuffer;

private:
   HRESULT MarshalInpParameters(LPCTSTR szFormat, ...);
   HRESULT UnmarshalOutParameters(LPCTSTR szFormat, ...);
   HRESULT Execute(NDT_CMD eCommand);

public:
   CRemoteDriver(LPCTSTR szDeviceName);
   virtual ~CRemoteDriver();

   virtual HRESULT Open();
   virtual HRESULT Close();

   virtual HRESULT StartDriverIoControl(
      DWORD dwTimeout, NDT_ENUM_REQUEST_TYPE eRequest, PVOID pvInpBuffer, 
      DWORD cbInpBuffer, PVOID pvOutBuffer, DWORD* pcbOutBuffer, 
      PVOID* ppvOverlapped
   );
   virtual HRESULT StopDriverIoControl(
      PVOID *ppvOverlapped
   );
   virtual HRESULT WaitForDriverIoControl(
      PVOID *ppvOverlapped, DWORD dwTimeout, PVOID pvOutBuffer, 
      DWORD* pcbOutBuffer
   );

   virtual HRESULT LoadAdapter(LPCTSTR szAdapter);
   virtual HRESULT UnloadAdapter(LPCTSTR szAdapter);

   virtual HRESULT QueryAdapters(LPTSTR mszAdapters, DWORD dwSize);
   virtual HRESULT QueryProtocols(LPTSTR mszProtocols, DWORD dwSize);
   virtual HRESULT QueryBindings(
      LPCTSTR szAdapter, LPTSTR mszProtocols, DWORD dwSize
   );
   virtual HRESULT BindProtocol(LPCTSTR szAdapter, LPCTSTR szProtocol);
   virtual HRESULT UnbindProtocol(LPCTSTR szAdapter, LPCTSTR szProtocol);

   virtual HRESULT WriteVerifyFlag(LPCTSTR szAdapter, DWORD dwFlag);
   virtual HRESULT DeleteVerifyFlag(LPCTSTR szAdapter);
};

//------------------------------------------------------------------------------

#endif
