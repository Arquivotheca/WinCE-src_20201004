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
#include "Binding.h"
#include "RequestReset.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------
   
CRequestReset::CRequestReset(CBinding* pBinding) : 
   CRequest(NDT_REQUEST_RESET, pBinding)
{
   m_dwMagic = NDT_MAGIC_REQUEST_RESET;
}

//------------------------------------------------------------------------------
   
NDIS_STATUS CRequestReset::Execute()
{
   // Add request to pending queue
   m_pBinding->AddRequestToList(this);
   
   // Execute command
   NdisReset(&m_status, m_pBinding->m_hAdapter);

   // If request isn't pending remove it and complete
   if (m_status != NDIS_STATUS_PENDING) {
      m_pBinding->RemoveRequestFromList(this);
      Complete();
   }

   // And return
   return m_status;
}

//------------------------------------------------------------------------------
