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

