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
// Supplicant.cpp : Implementation of CSupplicant
#include "stdafx.h"
#include "Supplicant.h"
#include "cryptlib.h"
#include "supplib.h"
#include "ndtlibwlan.h"

extern DWORD DebugLevel;

//IGlobalInterfaceTable   *pGIT                            = NULL;
//CLogSupp                *pLogSupp                        = NULL;
BYTE                    EtherType[SIZE_ETHERNET_TYPE]    = {0x88, 0x8E};
BYTE                    ProtocolVersion                  = 0x01;
AUTH_CONTEXT            AuthContext;
UINT                    LastMessage                      = 0;
//-----------------------------------------------------------------------------------------------//
/* 
 * This function will send a rekey message to the access point, requesting the access point to
 * restart the 4-way handshake.
 */
bool SendReKey(HANDLE hNdisuio, 
              BYTE* source, 
              BYTE* destination)
{
   BYTE*                pSendPacket = NULL;
   DWORD                nSendPktLen = sizeof(ETHERNET_HEADER) + FIELD_OFFSET(EAPOL_PACKET, PacketBody);
   ETHERNET_HEADER*     pEthernet   = NULL;
   EAPOL_PACKET*        pEapolPkt   = NULL;
   DWORD                nPktLen     = FIELD_OFFSET(EAPOL_PACKET, PacketBody);   
   DWORD                nBytesWritten  = 0;
  

   // Allocate the send packet
   pSendPacket = (BYTE*)malloc(nSendPktLen);
   if(pSendPacket == NULL)
   {
      NDTLogErr( _T("Failed to allocate memory for pSendPacket (Length = %d)\n"), nSendPktLen);
      return false;
   }
   
   ZeroMemory(pSendPacket, nSendPktLen);

   pEthernet = (ETHERNET_HEADER*)pSendPacket;

   // Copy the source and destination to the packet header
   memcpy(pEthernet->DestinationAddr,  destination,  MAC_ADDR_LEN);
   memcpy(pEthernet->SourceAddr,       source,     MAC_ADDR_LEN);
   
   pEapolPkt = (EAPOL_PACKET*)(pSendPacket + sizeof(ETHERNET_HEADER));  

   // Build EAPOL packet
   memcpy((BYTE*)pEapolPkt->EtherType, EtherType, SIZE_ETHERNET_TYPE);
   pEapolPkt->ProtocolVersion    = ProtocolVersion;
   pEapolPkt->PacketType         = EAPOL_Start;
   HostToWire16((WORD)(nPktLen - FIELD_OFFSET(EAPOL_PACKET,PacketBody)), (BYTE*)pEapolPkt->PacketBodyLength);

   BOOL bResult = WriteFile(hNdisuio, pSendPacket, nSendPktLen, &nBytesWritten, NULL);
   if (!bResult)
   {
      NDTLogErr( _T("WriteFile() failed (Error = %d)\n"), GetLastError());
      return false;
   }
   
   return true;
}

//-----------------------------------------------------------------------------------------------//
DWORD ProcessPacket(HANDLE hNdisuio, PBYTE pPkt)
{
    NDTLogVbs(  _T(": Enter ProcessPacket\n"));
    
    PEAPOL_PACKET        pEapolPkt      = NULL;
    PWPA_KEY_DESCRIPTOR  pKeyDescriptor = NULL;
    DWORD                status         = SUPPLICANT_FAILURE;
    
    //
    // Verify if the packet contains a 802.1P tag. If so, skip the last 4
    // bytes after the src+dest mac addresses and access the EAPOL packet
    //
    if((WireToHost16((PBYTE)(pPkt + sizeof(ETHERNET_HEADER))) == EAPOL_8021P_TAG_TYPE))
        pEapolPkt = (PEAPOL_PACKET)(pPkt + sizeof(ETHERNET_HEADER) + (sizeof(BYTE) * 4));
    else
        pEapolPkt = (PEAPOL_PACKET)(pPkt + sizeof(ETHERNET_HEADER));
    
    do
    {
        //
        // Verify this is a 1x packet
        //
        if(memcmp((BYTE*)pEapolPkt->EtherType, (BYTE*)EtherType, SIZE_ETHERNET_TYPE) != 0)
        {
            NDTLogErr( _T("Packet EtherType is not 802.1x, ignoring packet.\n"));                                                                                                      
            break;
        }
        
        //
        // Verify this is an EAPOL-Key message
        //
        if(pEapolPkt->PacketType != EAPOL_Key)
        {
            NDTLogErr( _T("Packet is not an EAPOL-Key message\n"));
            break;
        }
        
        //
        // Get a pointer to the key descriptor
        //
        pKeyDescriptor = (PWPA_KEY_DESCRIPTOR)((PBYTE)pEapolPkt + FIELD_OFFSET(EAPOL_PACKET, PacketBody));
        
        //
        // If this is a WPA2 Key Descriptor
        //
        switch(pKeyDescriptor->DescriptorType)
        {
        case EAPOL_KEY_DESCRIPTOR_TYPE_WPA2:
            status = ProcessRSNPacket(hNdisuio, pEapolPkt);
            break;
            
        case EAPOL_KEY_DESCRIPTOR_TYPE_WPA:
            status = ProcessWPAPacket(hNdisuio, pEapolPkt);
            break;
            
        default:
            NDTLogErr( _T("Key descriptor type not WPA or WPA2, ignoring packet\n"));                                                                                                      
            break;
        };
        
    }while(FALSE);
    
    NDTLogVbs(  _T(": Exit (Status = %d)\n"), status);
    return status;

}

//-----------------------------------------------------------------------------------------------//
/*
 * This is the receive thread used to post read IRPs to NdisUio and process the received packets
 */
DWORD __stdcall CSupplicant::RecvThread(LPVOID pPtr)
{  
   NDTLogVbs(  _T(": Enter RecvThread\n"));

   HRESULT                 hr             = S_OK;
   BOOL                    bRetval        = FALSE;
   DWORD                   nPacketLength  = sizeof(ETHERNET_HEADER) + sizeof(EAPOL_PACKET) + (1500 * sizeof(BYTE));
   DWORD                   nBytesRead     = 0;
   PBYTE                   pPacket        = NULL;
   THREADINFO*             pThreadInfo    = (THREADINFO*)pPtr;
   DWORD                   error          = 0;   
  
    do
    {
        pThreadInfo->IsThreadRunning = TRUE;
        NDTLogVbs(_T("Inside Recv Thread"));
        
        //
        // Allocate memory for receive packet
        //
        pPacket = (PBYTE)malloc(nPacketLength);
        if(pPacket == NULL)
        {
            NDTLogErr( _T("Failed to allocate memory for packet! (Error = %d)\n"), GetLastError());
            break;
        }
     
        //
        // Initialize context information
        //
        pThreadInfo->IsGroupKeyActive    = FALSE;
        pThreadInfo->HandshakeSuccessful = FALSE;

        while(TRUE)
        {  
            //
            // Check to see if the thread should terminate
            //
            if(pThreadInfo->KillThread == TRUE)
            {
                NDTLogVbs( _T("Exit receive thread flag set (First Check), bailing out\n"));
                break;
            }
            
            ZeroMemory(pPacket, nPacketLength);

            //
            // Do read and waiting for packets to be received
            //
            BOOL bResult = ReadFile(pThreadInfo->NdisUio, pPacket, nPacketLength, &nBytesRead, NULL);
            if (!bResult)
            {
       //                   NDTLogVbs(_T("Error reading from Ndisuio %x"),GetLastError());
                   continue;
               }      
              else
              {  

                  ETHERNET_HEADER*    pEthHeader  = (PETHERNET_HEADER)pPacket;
                  DWORD               retval      = 0;

                  NDTLogVbs(_T("Ndisuio Packet received"));
                  // Make sure the packet passed in is not NULL;
                  if(pEthHeader == NULL)
                  {
                      NDTLogErr( _T("Received packet pointer was NULL\n"));
                      break;
                  }
                
                  //
                  // Save supplicant and authenticator address
                  //
                  memcpy(AuthContext.SupplicantAddress,     pEthHeader->DestinationAddr,  MAC_ADDR_LEN);
                  memcpy(AuthContext.AuthenticatorAddress,  pEthHeader->SourceAddr,       MAC_ADDR_LEN);

                  // ISLP21 has a bug which corupts the authenticator address 
                  NDTLogDbg((_T("Authenticator Address %x::%x::%x::%x::%x::%x")), 
                     AuthContext.AuthenticatorAddress[0],
                     AuthContext.AuthenticatorAddress[1],
                     AuthContext.AuthenticatorAddress[2],
                     AuthContext.AuthenticatorAddress[3],
                     AuthContext.AuthenticatorAddress[4],
                     AuthContext.AuthenticatorAddress[5]
                   );

                  retval = ProcessPacket(pThreadInfo->NdisUio, pPacket);        
                  switch(retval)
                  {
                  case SUPPLICANT_HANDSHAKE_COMPLETE:
                      NDTLogDbg( _T("Handshake completed successfully\n"));
                      pThreadInfo->HandshakeSuccessful = TRUE;
                      break;
                      
                  case SUPPLICANT_FAILURE:
                      NDTLogDbg( _T("Handshake failure\n"));
                      pThreadInfo->HandshakeSuccessful = FALSE;
                      pThreadInfo->KillThread          = TRUE;
                      break;
                      
                  case SUPPLICANT_GROUP_KEY_ADDED:
                      NDTLogDbg( _T("Group key activated\n"));
                      pThreadInfo->IsGroupKeyActive    = TRUE;
                      pThreadInfo->KillThread          = TRUE;
                      pThreadInfo->HandshakeSuccessful = TRUE;
                      break;
                  
                  default:
                     NDTLogVbs(_T("Status from ProcessPacket %x"), retval);
                  }
                  //                  break;
               }
        }             
      
      free(pPacket);   
            
   }while(false);
   
   //
   // Cancel any outstannding IRP's
   //
   //CancelIo(pThreadInfo->NdisUio);
   
   // ::CoUninitialize();
   
   pThreadInfo->IsThreadRunning = FALSE;

   NDTLogDbg( _T("RecvThread(): Exiting\n"));
//   _endthreadex(0);
   
   return 0; 
}

//-----------------------------------------------------------------------------------------------//
HRESULT CSupplicant::FinalConstruct()
{  
   HRESULT  hr = S_OK;

   m_thread = INVALID_HANDLE_VALUE;

   //
   // Do this before we call any write functions 
//   pLogSupp = new CLogSupp;
 //  if(pLogSupp == NULL)
  // {
//      OutputDebugString(_T("CSupplicant::FinalConstruct: Failed to create CLogSupp.\n"));
//      return E_FAIL;
//   }

//   hr = pLogSupp->AttachToLog(SCRIPT_LOG, _T("NDT1XSUPP"));
//   if(FAILED(hr))
// {
//    OutputDebugString(_T("CSupplicant::FinalConstruct: Failed CLogSupp::AttachToLog!\n"));
//   }
   
   /*
   hr = CoCreateInstance(CLSID_StdGlobalInterfaceTable,
                         NULL,
                         CLSCTX_INPROC_SERVER,
                         IID_IGlobalInterfaceTable,
                         (void **)&pGIT);
   if(FAILED(hr))
   {
      pLogSupp->DBGSERIOUS( _T(": Failed to create Global Interface Table Object (hr = 0x%08x)\n"), hr);
      return E_FAIL;
   }

   pGIT->AddRef();
   */
   return S_OK;
}

//-----------------------------------------------------------------------------------------------//
VOID CSupplicant::FinalRelease()
{  
   BOOL retval = FALSE;
   
   if(m_thread != INVALID_HANDLE_VALUE)
   {
      retval = CloseSupplicant();
   }

   //pGIT->Release();

  //   delete pLogSupp;
}

//-----------------------------------------------------------------------------------------------//
/*
 * This property will return the current key index of the group key being used
 * it will and out any of the upper bits because the script does not need them
 */
void CSupplicant::get_GroupKeyIndex(LONG *pVal)
{
	*pVal = (AuthContext.GroupKeyIndex & 0xFF);
}

//-----------------------------------------------------------------------------------------------//
/* 
 * This method is exposed to script allowing the script to open the wireless device, the script
 * will pass in the GUID of the device and this method will attempt to open the device using
 * NDISUIO, if successful the method returns true, otherwise the method returns false
 */
//STDMETHODIMP CSupplicant::OpenSupplicant(BSTR guid, VARIANT_BOOL *status)
BOOL CSupplicant::OpenSupplicant(LPTSTR szAdapterName)
{
   BOOL status;
   NDTLogVbs(  _T(": Enter OpenSupplicant\n"));

   DWORD bytesReturned = 0;

   do
   {
      status = FALSE;

      ZeroMemory(&m_threadInfo, sizeof(THREADINFO));
   
      // Initialize our SNonce, which is always a constant value
      for(int i = 0; i < SIZE_NONCE; i++)
         AuthContext.SNonce[i] = 1;

      //m_threadInfo.AddGroupKey      = TRUE;
      // m_threadInfo.AddPairwiseKey   = TRUE;
      
      NDTLogDbg( _T("Device Name: %s\n"), szAdapterName);

      wcscpy(m_threadInfo.DeviceName,szAdapterName);
//      // if first char in the Guid is '\' then we assume the GUID format is
//      // '\DEVICE\{...}'. We do just the UNICODE conversion then
//      if(guid[0] == '\\')
//      {
//         wcscpy(m_threadInfo.DeviceName, guid);
//      }
//      else
//      {   
//         wcscpy(m_threadInfo.DeviceName, L"\\DEVICE\\");
//         wcsncat(m_threadInfo.DeviceName, guid, MAX_NDISUIO_DEVICE_NAME_LEN - 8);
//         m_threadInfo.DeviceName[MAX_NDISUIO_DEVICE_NAME_LEN - 1] = '\0';
//      }

      // Get a handle to NdisUio
      m_threadInfo.NdisUio = NdisUioCreate();
      if(m_threadInfo.NdisUio == INVALID_HANDLE_VALUE)
      {
         break;
      }

      // Open the device
      if(DeviceIoControl(m_threadInfo.NdisUio, 
                         IOCTL_NDISUIO_OPEN_DEVICE, 
                         m_threadInfo.DeviceName, 
                         (DWORD) wcslen(m_threadInfo.DeviceName) * sizeof(WCHAR), 
                         NULL, 
                         0, 
                         &bytesReturned, 
                         NULL) == FALSE)
      {
         NDTLogErr( _T("IOCTL_NDISUIO_OPEN_DEVICE failed (Device = %ws) (Error = %d)\n"), m_threadInfo.DeviceName, GetLastError());
         break;
      }
    
      // Set the Ether Type for the protocol (NdisUio)
      if(DeviceIoControl(m_threadInfo.NdisUio,
                         IOCTL_NDISUIO_SET_ETHER_TYPE,
                         (LPVOID)&EtherType,
                         sizeof(EtherType),
                         NULL,
                         0,
                         &bytesReturned,
                         NULL) == FALSE)
      {
         NDTLogErr( _T("Failed to set EtherType (Device = %ws) (Error = %d)\n"), m_threadInfo.DeviceName, GetLastError());
         break;
      } 
      
      status = TRUE;
    
   }while(false);
   
   NDTLogVbs(  _T(": Exit\n"));
   return status;
}

//-----------------------------------------------------------------------------------------------//
/*
 * This method will close the supplicant by stoping the authenticator, the listening thread, 
 * deregistering the event handlers.
 */
BOOL CSupplicant::CloseSupplicant()
{   
   BOOL status = FALSE;
   m_threadInfo.KillThread = TRUE;
   
   Sleep(200);

   if(m_threadInfo.NdisUio != INVALID_HANDLE_VALUE)
   {
      NdisUioClose(m_threadInfo.NdisUio);
      m_threadInfo.NdisUio = INVALID_HANDLE_VALUE;
   }

   status = TRUE;    
    
   return status;
}


//-----------------------------------------------------------------------------------------------//
/* 
 * This function is called to start the handshake thread
 */
BOOL CSupplicant::StartAuthenticator(char* password, NDIS_802_11_SSID ssid)
{
    BOOL status = FALSE;
    HRESULT hr;
    NDTLogVbs(  _T(": Enter StartAuthenticator\n"));

//    USES_CONVERSION;

    UCHAR*    buffer;
    INT      counter = 0;

    ZeroMemory(&AuthContext.PTK, sizeof(PAIRWISE_TEMPEROL_KEY));
    ZeroMemory(&AuthContext.PreSharedKey, sizeof(PRESHARED_KEY));

    // Reset thread specific flags
    m_threadInfo.IsThreadRunning  = FALSE;
    m_threadInfo.KillThread       = FALSE;
   
    // Initialize the thread info data
    AuthContext.GroupKeyIndex = 255; // 255 Indicate a bad key index

    do
    {
        TCHAR szSsidText [33];
        GetSsidText(szSsidText,  ssid);

#ifdef UNICODE
        NDTLogDbg( _T("password:    %S\n"),password);
#else
        NDTLogDbg( _T("password:    %s\n"),password);
#endif
        NDTLogDbg( _T("ssid:        %s\n"), szSsidText);
        
        status = FALSE;
        
        //
        // Generate PMK from password SSID
        //
        hr = GetPMKFromPassword(&buffer,password,ssid);
        if (E_FAIL == hr)
        {
           NDTLogErr(_T("Getting PMK from Password failed"));
           return FALSE;
        }

        AuthContext.PreSharedKey.KeyLength = MAX_KEY_MATERIAL_LEN;
        memcpy(AuthContext.PreSharedKey.KeyMaterial, buffer, AuthContext.PreSharedKey.KeyLength);

        m_thread = (HANDLE)CreateThread(NULL, 0, RecvThread, (VOID*)&m_threadInfo, 0, &m_threadID);
        if(m_thread == INVALID_HANDLE_VALUE)
        {
            NDTLogErr(_T(": CreateThread failed for RecvThread() (Error = %d)\n"), GetLastError());
            break;
        }

        
        //
        // Sleep for 100 ms intervals waiting for the trhead to signal ready
        //
        while(m_threadInfo.IsThreadRunning == FALSE)
        {
            Sleep(100);
            counter += 100;

            if(counter > 10000)
            {
                NDTLogErr( _T("Failed to start receive thread\n"));
                break;
            }
        }

        status = TRUE;

    }while(false);

    NDTLogVbs( _T(": Exit\n"));
    return status;
}

//-----------------------------------------------------------------------------------------------//
/*
 * This function is called after StartAuthenticator() to wait for the 4-way handshake to complete.
 * It will wait for the thread event to be signal upto the timeout value passed in. If the 
 * event is not signaled within the timeout value the thread will be terminated and false 
 * will be returned to indacte the 4-way handshake failed.
 */
BOOL CSupplicant::WaitAuthenticationComplete(LONG timeout)
{    
    BOOL status = FALSE;
    ULONG   retval          = 0;
    ULONG   threadWaitCount = 0;
   
    timeout *= 1000;    // convert timeout to ms
    
    //
    // Make an inital check to see if the handshake is complete because
    // it may have already complete before we entered this routine
    // Note: This does not nessacarily mean we completed the group key handshake
    //
    if(m_threadInfo.HandshakeSuccessful == FALSE)
    {
        retval = WaitForSingleObject(m_thread, timeout);
        if(retval != WAIT_OBJECT_0)
        {
            NDTLogErr( _T("WaitAuthenticationComplete(): Handshake timed out (Last Message = %d)\n"), LastMessage);
            
            // Set flag to kill thread and wait for thread to exit
            m_threadInfo.KillThread = TRUE;
            
            Sleep(100);             
        }
    }
    else
    {
        NDTLogDbg( _T("WaitAuthenticationComplete(): Handshake already completed\n"));
    }

    //
    // Wait for the thread to signal that it is exiting
    //
    while(m_threadInfo.IsThreadRunning == TRUE)
    {
        NDTLogVbs( _T("WaitAuthenticationComplete(): Waiting for thread to exit\n"));

        Sleep(100);
        threadWaitCount += 100;

        if(threadWaitCount == 2000)
        {
            NDTLogErr( _T("WaitAuthenticationComplete: Thread failed to exit, terminating thread\n"));

            TerminateThread(m_thread, 1);
            break;
        }
    }

    //
    // Cleanup the thread handle
    //
    if(m_thread != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_thread);
        m_thread = INVALID_HANDLE_VALUE;
    }

    
    if(m_threadInfo.HandshakeSuccessful == TRUE)
        status = TRUE;
    else
        status = FALSE;
    
    return status;
}

/*
STDMETHODIMP CSupplicant::get_DebugLevel(LONG *pVal)
{
	*pVal = DebugLevel;

	return S_OK;
}

STDMETHODIMP CSupplicant::put_DebugLevel(LONG newVal)
{
	DebugLevel = newVal;

	return S_OK;
}
*/
