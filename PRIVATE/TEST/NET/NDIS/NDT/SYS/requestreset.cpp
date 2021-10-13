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
