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
#include "RequestBind.h"
#include "Medium802_3.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

CRequestBind::CRequestBind(CBinding *pBinding) : 
   CRequest(NDT_REQUEST_BIND, pBinding)
{
   m_dwMagic = NDT_MAGIC_REQUEST_BIND;

   m_pProtocol = NULL;
   m_uiSelectedMediaIndex = 0;

   m_uiMediumArraySize = 0;
   m_aMedium = NULL;
   NdisInitUnicodeString(&m_nsAdapterName, NULL);
   m_uiOpenOptions = 0;
   NdisInitUnicodeString(&m_nsDriverName, NULL);
}   

//------------------------------------------------------------------------------

CRequestBind::~CRequestBind()
{
   if (m_nsAdapterName.Buffer != NULL) NdisFreeString(m_nsAdapterName);
   if (m_nsDriverName.Buffer != NULL) NdisFreeString(m_nsDriverName);
   delete m_aMedium;
   if (m_pProtocol != NULL) m_pProtocol->Release();
}

//------------------------------------------------------------------------------

void CRequestBind::Complete()
{
   if (
      m_status == NDIS_STATUS_SUCCESS
   ) {
      m_pBinding->OpenAdapter(m_aMedium[m_uiSelectedMediaIndex]);
    m_pBinding->m_pProtocol->m_pBindingRequest->Release();
    m_pBinding->m_pProtocol->m_pBindingRequest = NULL;
   }
   CRequest::Complete();   
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestBind::Execute()
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   
   // When there is no binding let create it
   if (m_pBinding == NULL) {
      ASSERT(m_pProtocol);

      //BUGBUG: Assuming there is only one outstanding bind request at any time  
      m_pProtocol->m_pBindingRequest = this;
      AddRef();
      m_pBinding = new CBinding(m_pProtocol);
   }
   
   // Add request to pending queue
   m_pBinding->AddRequestToList(this);
      
   NdisReEnumerateProtocolBindings(m_pProtocol->m_hProtocol);

   //Re-binding is not synchronous, so we need to wait for completion
   // Legally we should wait till NDIS calls our ProtocolPnpEvent with
   // BindComplete. But since we have multiple protocol drivers, the 
   // ProtocolPnpEvent does not tell us if it is for this Protocol or 
   // another protocol.
   for(ULONG ulCount = 0; ulCount < 10; ulCount++)
       {
        if (NdisWaitEvent(&g_hBindsCompleteEvent, ulBIND_COMPLETE_WAIT))
        {
            NdisResetEvent(&g_hBindsCompleteEvent);
             status = NDIS_STATUS_SUCCESS;
            break;
        }
        else
        {
            status = NDT_STATUS_TIMEOUT;            
        }
        //TODO: Insert bind not complete: rewaiting log message here
       }

   if (status == NDIS_STATUS_SUCCESS)
       {
       // Bind has completed. Make sure binding is in running state before
          // returning to script. 
          for (ULONG ulCount = 0; ulCount < 10; ulCount++)
              {
                  if (!NdisWaitEvent(&(m_pBinding->m_pProtocol->m_hRestartedEvent), ulRESTART_COMPLETE_WAIT))
                  {
                      status = NDT_STATUS_TIMEOUT;
                  }
            else
            {
                NdisResetEvent(&(m_pBinding->m_pProtocol->m_hRestartedEvent));
                status = NDIS_STATUS_SUCCESS;
                break;
            }
            //TODO: Insert log message here: Restart did not arrrive -- rewaiting    
              }
       }

    // If the bind never happened
    if (NDT_STATUS_TIMEOUT == status)
    {
        DebugBreak();
    }
        
   return m_status;   
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestBind::CreateBinding(NDIS_HANDLE BindContext)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   NDIS_OPEN_PARAMETERS op;

   //Init the NDIS_OPEN_PARAMETERS structure
   op.Header.Type = NDIS_OBJECT_TYPE_OPEN_PARAMETERS;
   op.Header.Revision = NDIS_OPEN_PARAMETERS_REVISION_1;
   op.Header.Size = sizeof(NDIS_OPEN_PARAMETERS);
    op.AdapterName = &m_nsAdapterName;
    op.MediumArray = m_aMedium;
    op.MediumArraySize = m_uiMediumArraySize;
    op.SelectedMediumIndex = &m_uiSelectedMediaIndex;
    op.FrameTypeArray = NULL;
    op.FrameTypeArraySize = 0;

   // We need to add a reference because NDIS will hold a pointer
   m_pBinding->AddRef();

   // Execute command
   status = NdisOpenAdapterEx(
       m_pBinding->m_pProtocol->m_hProtocol, 
       m_pBinding, 
       &op,
        BindContext,
        &m_pBinding->m_hAdapter);

   // If open isn't pending remove it and complete
   m_status = status;
   
   if (status != NDIS_STATUS_PENDING) {
      // If command failed reference isn't hold anymore
      if (status != NDIS_STATUS_SUCCESS) m_pBinding->Release();
      m_pBinding->RemoveRequestFromList(this);
      Complete();
   }

   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestBind::CreateRegistryKey()
{
   TCHAR szUpperBindValueName[128];
   TCHAR valueString[256];
   DWORD valueStringSize = 256;
   StringCchPrintfW(szUpperBindValueName, 
       sizeof(szUpperBindValueName)/sizeof(szUpperBindValueName[0]),
       L"%s\\%s\\Parms\\UpperBind",
       REG_NDIS_KEY,
       m_nsAdapterName.Buffer);
   if (!GetRegMultiSZValue(HKEY_LOCAL_MACHINE, szUpperBindValueName, valueString, &valueStringSize))
   {
       //UpperBind key does not exist yet
       StringCchCopy(valueString, _countof(valueString), m_nsDriverName.Buffer);
    valueString[m_nsDriverName.Length/sizeof(valueString[0])+sizeof(valueString[0])] = '\0';
   }
   else
   {
          //convert bytes to characters
          valueStringSize /= sizeof(valueString[0]);
       StringCchCopy(valueString+valueStringSize-1, _countof(valueString)-valueStringSize, m_nsDriverName.Buffer);
       valueString[valueStringSize+m_nsDriverName.Length/sizeof(valueString[0])+sizeof(valueString[0])] = '\0';
   }
   if (!SetRegMultiSZValue(HKEY_LOCAL_MACHINE, szUpperBindValueName, valueString))
       return NDIS_STATUS_FAILURE;
   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

LPTSTR CRequestBind::GetAdapterName()
{
    LPTSTR szPos = m_nsAdapterName.Buffer;
    while (*szPos != _T('\0')) {
              if (*szPos == _T('\\')) break;
              szPos++;
   }
   if (*szPos == _T('\0')) szPos = m_nsAdapterName.Buffer;
   else szPos++;
   return (LPTSTR)szPos;

}

// ----------------------------------------------------------------
// GetRegMultiSZValue()  access the registry
//
//
BOOL CRequestBind::
GetRegMultiSZValue (HKEY hKey, LPTSTR lpValueName, PTSTR pData, LPDWORD pdwSize)
{
    DWORD    dwType;
    LONG    hRes;

    // We need to add an extra NULL, so shorten the requested length.
    *pdwSize -= sizeof(TCHAR);   
    hRes = RegQueryValueEx (hKey, lpValueName, 0, &dwType, (LPBYTE)pData,
                            pdwSize); 
    if ((hRes != ERROR_SUCCESS) ||
        ((dwType != REG_SZ) && (dwType != REG_MULTI_SZ))) {
        SetLastError (hRes);
        return FALSE;
    } else {
        if (dwType == REG_SZ) {
            // Now convert it to a MultiSz string.
            for (hRes=0; pData[hRes] != TEXT('\0'); hRes++) {
                if (TEXT(',') == pData[hRes]) {
                    pData[hRes++] = TEXT('\0');
                }
            }
            // Add an extra null....
            pData[++hRes] = TEXT('\0');
        }
        return TRUE;
    }
}

// ----------------------------------------------------------------
// SetRegMultiSZValue()  access the registry
//
//
BOOL CRequestBind::
SetRegMultiSZValue (HKEY hKey, LPTSTR lpValueName, PTSTR pData)
{
    LONG    hRes;
    PTSTR    pStr;
    DWORD    Len;

    // Null?
    if (NULL == pData) {
        hRes = RegSetValueEx (hKey, lpValueName, 0, REG_MULTI_SZ,
                              (LPBYTE)(L"\0\0"), 2*sizeof(TCHAR)); 
    } else {
        // Get the length of the string.
        // abc\0defg\0\0
        
        for (pStr = pData, Len = 0; *pStr != TEXT('\0'); pStr = pData + Len) {
            Len += _tcslen (pStr)+1;
        }
        Len++;    // Add one for the NULL.
        hRes = RegSetValueEx (hKey, lpValueName, 0, REG_MULTI_SZ,
                              (LPBYTE)pData, Len*sizeof(TCHAR));
    }

    if (ERROR_SUCCESS == hRes)
        return TRUE;

    SetLastError (hRes);
    return FALSE;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestBind::UnmarshalInpParams(
   LPVOID *ppvBuffer, DWORD *pcbBuffer
)
{
   UINT uiUnusedParameter;
   NDIS_STATUS status;
   
   status = UnmarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_INP_BIND, &uiUnusedParameter,
      &m_uiMediumArraySize, &m_aMedium, &m_nsAdapterName, &m_uiOpenOptions, &m_pProtocol
   );
   m_pProtocol->AddRef();

   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestBind::MarshalOutParams(
   LPVOID *ppvBuffer, DWORD *pcbBuffer
)
{
   //The fact that tests hold a reference is accounted for when the unbind request's input parameters are unmarshalled
   return MarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_OUT_BIND, m_status,
      m_uiSelectedMediaIndex, m_pBinding
   );
}

//------------------------------------------------------------------------------
