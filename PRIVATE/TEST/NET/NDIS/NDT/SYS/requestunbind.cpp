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
#include "Protocol.h"
#include "Binding.h"
#include "RequestUnbind.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

CRequestUnbind::CRequestUnbind(CBinding *pBinding) : 
   CRequest(NDT_REQUEST_UNBIND, pBinding)
{
   m_dwMagic = NDT_MAGIC_REQUEST_UNBIND;
}

//------------------------------------------------------------------------------

void CRequestUnbind::Complete()
{
   // Close "adapter" in the binding object
   m_pBinding->CloseAdapter();
   // Default complete
   CRequest::Complete();   
}

//------------------------------------------------------------------------------
   
NDIS_STATUS CRequestUnbind::Execute()
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   
   // Add request to pending queue
   m_pBinding->AddRequestToList(this);
   
   // Execute command
   NdisCloseAdapter(&status, m_pBinding->m_hAdapter);

   // If request isn't pending remove it and complete
   if (status != NDIS_STATUS_PENDING) {
      m_status = status;
      m_pBinding->RemoveRequestFromList(this);
      Complete();
   }

   // And return
   return status;
}

//------------------------------------------------------------------------------

