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
#include "ShellProc.h"
#include "NDTNdis.h"
#include "NDTMsgs.h"
#include "NDTError.h"
#include "NDTLog.h"
#include "NDTLib.h"
#include "NDTLibEx.h"
#include "ndt_1c.h"
#include <ntddndis.h>
#include <ndis.h>

TEST_FUNCTION(TestOids)
{
   TEST_ENTRY;

   INT rc = TPR_PASS;
   HRESULT hr = S_OK;
   BOOL bForce30 = FALSE;
   NDIS_MEDIUM ndisMedium = g_ndisMedium;
   HANDLE hAdapter = NULL;
   UINT uiPhysicalMedium = 0;
   NDIS_OID aRequiredOids[256];
   NDIS_OID *aSupportedOids = NULL;
   UINT cbSupportedOids = 0;
   NDIS_OID aFddiOids[32];
   NDIS_OID aSkipOids[64];
   UINT cRequiredOids = 0;
   UINT cSupportedOids = 0;
   UINT cFddiOids = 0;
   UINT cSkipOids = 0;
   UINT cbUsed = 0;
   UINT cbRequired = 0;
   UINT ix1 = 0;
   UINT ix2 = 0;
   BOOL bError = FALSE;
   UCHAR * pacBuffer;
   DWORD cbacBuffSize = 8192;

   pacBuffer = (UCHAR *) new UCHAR[cbacBuffSize];
   if (!pacBuffer)
   {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }
   
   // Let start
   NDTLogMsg(
      _T("Start 1c_TestOidsDeviceIoControl test on the adapter %s"), 
      g_szTestAdapter
   );

   NDTLogMsg(_T("Open adapter"));
   hr = NDTOpen(g_szTestAdapter, &hAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
      bError = TRUE;
      goto cleanUp;
   }

   // Bind
   NDTLogMsg(_T("Bind to adapter"));
   hr = NDTBind(hAdapter, bForce30, ndisMedium);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailBind, hr);
      bError = TRUE;
      goto cleanUp;
   }

   hr = NDTGetPhysicalMedium(hAdapter, &uiPhysicalMedium);
   if (FAILED(hr)) {
      bError = TRUE;
      goto cleanUp;
   }

   aRequiredOids[cRequiredOids++] = OID_GEN_SUPPORTED_LIST;
   aRequiredOids[cRequiredOids++] = OID_GEN_HARDWARE_STATUS;
   aRequiredOids[cRequiredOids++] = OID_GEN_MEDIA_SUPPORTED;
   aRequiredOids[cRequiredOids++] = OID_GEN_MEDIA_IN_USE;
   aRequiredOids[cRequiredOids++] = OID_GEN_MAXIMUM_LOOKAHEAD;
   aRequiredOids[cRequiredOids++] = OID_GEN_MAXIMUM_FRAME_SIZE;
   aRequiredOids[cRequiredOids++] = OID_GEN_LINK_SPEED;
   aRequiredOids[cRequiredOids++] = OID_GEN_TRANSMIT_BUFFER_SPACE;
   aRequiredOids[cRequiredOids++] = OID_GEN_RECEIVE_BUFFER_SPACE;
   aRequiredOids[cRequiredOids++] = OID_GEN_TRANSMIT_BLOCK_SIZE;
   aRequiredOids[cRequiredOids++] = OID_GEN_RECEIVE_BLOCK_SIZE;
   aRequiredOids[cRequiredOids++] = OID_GEN_VENDOR_ID;
   aRequiredOids[cRequiredOids++] = OID_GEN_VENDOR_DESCRIPTION;
   aRequiredOids[cRequiredOids++] = OID_GEN_CURRENT_PACKET_FILTER;
   aRequiredOids[cRequiredOids++] = OID_GEN_CURRENT_LOOKAHEAD;
   aRequiredOids[cRequiredOids++] = OID_GEN_DRIVER_VERSION;
   aRequiredOids[cRequiredOids++] = OID_GEN_MAXIMUM_TOTAL_SIZE;
   aRequiredOids[cRequiredOids++] = OID_GEN_MAC_OPTIONS;
   aRequiredOids[cRequiredOids++] = OID_GEN_MEDIA_CONNECT_STATUS;
   aRequiredOids[cRequiredOids++] = OID_GEN_MAXIMUM_SEND_PACKETS;
   aRequiredOids[cRequiredOids++] = OID_GEN_VENDOR_DRIVER_VERSION;
   
   aRequiredOids[cRequiredOids++] = OID_GEN_XMIT_OK;
   aRequiredOids[cRequiredOids++] = OID_GEN_RCV_OK;
   aRequiredOids[cRequiredOids++] = OID_GEN_XMIT_ERROR;
   aRequiredOids[cRequiredOids++] = OID_GEN_RCV_ERROR;
   aRequiredOids[cRequiredOids++] = OID_GEN_RCV_NO_BUFFER;
   
   switch (ndisMedium) {
   case NdisMedium802_3:
      aRequiredOids[cRequiredOids++] = OID_802_3_PERMANENT_ADDRESS;
      aRequiredOids[cRequiredOids++] = OID_802_3_CURRENT_ADDRESS;
      aRequiredOids[cRequiredOids++] = OID_802_3_MULTICAST_LIST;
      aRequiredOids[cRequiredOids++] = OID_802_3_MAXIMUM_LIST_SIZE;
      aRequiredOids[cRequiredOids++] = OID_802_3_RCV_ERROR_ALIGNMENT;
      aRequiredOids[cRequiredOids++] = OID_802_3_XMIT_ONE_COLLISION;
      aRequiredOids[cRequiredOids++] = OID_802_3_XMIT_MORE_COLLISIONS;
      break;
   case NdisMedium802_5:   
      aRequiredOids[cRequiredOids++] = OID_802_5_PERMANENT_ADDRESS;
      aRequiredOids[cRequiredOids++] = OID_802_5_CURRENT_ADDRESS;
      aRequiredOids[cRequiredOids++] = OID_802_5_CURRENT_FUNCTIONAL;
      aRequiredOids[cRequiredOids++] = OID_802_5_CURRENT_GROUP;
      aRequiredOids[cRequiredOids++] = OID_802_5_LAST_OPEN_STATUS;
      aRequiredOids[cRequiredOids++] = OID_802_5_CURRENT_RING_STATUS;
      aRequiredOids[cRequiredOids++] = OID_802_5_CURRENT_RING_STATE;
      aRequiredOids[cRequiredOids++] = OID_802_5_LINE_ERRORS;
      aRequiredOids[cRequiredOids++] = OID_802_5_LOST_FRAMES;
      break;
   case NdisMediumFddi:
      aRequiredOids[cRequiredOids++] = OID_FDDI_LONG_PERMANENT_ADDR;
      aRequiredOids[cRequiredOids++] = OID_FDDI_LONG_CURRENT_ADDR;
      aRequiredOids[cRequiredOids++] = OID_FDDI_LONG_MULTICAST_LIST;
      aRequiredOids[cRequiredOids++] = OID_FDDI_LONG_MAX_LIST_SIZE;
      aRequiredOids[cRequiredOids++] = OID_FDDI_SHORT_PERMANENT_ADDR;
      aRequiredOids[cRequiredOids++] = OID_FDDI_SHORT_CURRENT_ADDR;
      aRequiredOids[cRequiredOids++] = OID_FDDI_SHORT_MULTICAST_LIST;
      aRequiredOids[cRequiredOids++] = OID_FDDI_SHORT_MAX_LIST_SIZE;
      aRequiredOids[cRequiredOids++] = OID_FDDI_ATTACHMENT_TYPE;
      aRequiredOids[cRequiredOids++] = OID_FDDI_UPSTREAM_NODE_LONG;
      aRequiredOids[cRequiredOids++] = OID_FDDI_DOWNSTREAM_NODE_LONG;
      aRequiredOids[cRequiredOids++] = OID_FDDI_FRAME_ERRORS;
      aRequiredOids[cRequiredOids++] = OID_FDDI_FRAMES_LOST;
      aRequiredOids[cRequiredOids++] = OID_FDDI_RING_MGT_STATE;
      aRequiredOids[cRequiredOids++] = OID_FDDI_LCT_FAILURES;
      aRequiredOids[cRequiredOids++] = OID_FDDI_LEM_REJECTS;
      aRequiredOids[cRequiredOids++] = OID_FDDI_LCONNECTION_STATE;
      break;
   }
   
   NDTLogMsg(_T("Get Supported OIDs List"));
   
   hr = NDTQueryInfo(
      hAdapter, OID_GEN_SUPPORTED_LIST, aSupportedOids, 0,
      &cbUsed, &cbRequired
   );
   if ((NDIS_FROM_HRESULT(hr)) != NDIS_STATUS_INVALID_LENGTH) {
      NDTLogErr(
          _T("Querying OID_GEN_SUPPORTED_LIST returned 0x%08x"),
          hr
      );
      bError = TRUE;
      goto cleanUp;
   }

   cbSupportedOids = cbRequired;
   aSupportedOids = new NDIS_OID[cbRequired/sizeof(NDIS_OID)];
   if (!aSupportedOids)
   {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }

   hr = NDTQueryInfo(
      hAdapter, OID_GEN_SUPPORTED_LIST, aSupportedOids, cbSupportedOids,
      &cbUsed, &cbRequired
   );
   if (FAILED(hr)) {
      NDTLogErr(
          _T("Querying OID_GEN_SUPPORTED_LIST returned 0x%08x"),
          hr
      );
      bError = TRUE;
      goto cleanUp;
   }

   NDTLogMsg(_T("Check for all required OIDs"));
   
   cSupportedOids = cbUsed/sizeof(NDIS_OID);

   for (ix1 = 0; ix1 < cRequiredOids; ix1++) {
      for (ix2 = 0; ix2 < cSupportedOids; ix2++) {
         if (aRequiredOids[ix1] == aSupportedOids[ix2]) break;
      }
      if (ix2 >= cSupportedOids) {
         NDTLogErr(
            _T("Supported list does not contain required OID (0x%08x)"), 
            aRequiredOids[ix1]
         );
         bError = TRUE;
      }
   }
   
   // The following OIDS are technically required, but will be
   // warned instead of generating a failure if they are not supported
   // (no protocol uses them as of this date)
   // At some later date this may be changed to a failure

   if (ndisMedium == NdisMediumFddi) {
      cFddiOids = 0;
      aFddiOids[cFddiOids++] = OID_FDDI_SHORT_PERMANENT_ADDR;
      aFddiOids[cFddiOids++] = OID_FDDI_SHORT_CURRENT_ADDR;
      aFddiOids[cFddiOids++] = OID_FDDI_SHORT_MULTICAST_LIST;
      aFddiOids[cFddiOids++] = OID_FDDI_SHORT_MAX_LIST_SIZE;
      for (ix1 = 0; ix1 < cFddiOids; ix1++) {
         for (ix2 = 0; ix2 < cSupportedOids; ix2++) {
            if (aFddiOids[ix1] == aSupportedOids[ix2]) break;
         }
         if (ix2 >= cSupportedOids) {
            NDTLogWrn(
               _T("Supported list does not contain required OID (0x%08x)"), 
               aFddiOids[ix1]
            );
         }
      }
   }
   //List of OIDs which are only settable
   aSkipOids[cSkipOids++] = OID_GEN_SUPPORTED_LIST;
   aSkipOids[cSkipOids++] = OID_GEN_PROTOCOL_OPTIONS;
   aSkipOids[cSkipOids++] = OID_GEN_NETWORK_LAYER_ADDRESSES;
   aSkipOids[cSkipOids++] = OID_PNP_SET_POWER;
   aSkipOids[cSkipOids++] = OID_PNP_QUERY_POWER;
   aSkipOids[cSkipOids++] = OID_PNP_ADD_WAKE_UP_PATTERN;
   aSkipOids[cSkipOids++] = OID_PNP_REMOVE_WAKE_UP_PATTERN;
   aSkipOids[cSkipOids++] = OID_PNP_ENABLE_WAKE_UP;
   aSkipOids[cSkipOids++] = OID_GEN_PHYSICAL_MEDIUM;
   aSkipOids[cSkipOids++] = OID_802_11_ADD_WEP;
   aSkipOids[cSkipOids++] = OID_802_11_REMOVE_WEP;
   aSkipOids[cSkipOids++] = OID_802_11_BSSID_LIST_SCAN;
   aSkipOids[cSkipOids++] = OID_802_11_DISASSOCIATE;
   aSkipOids[cSkipOids++] = OID_802_11_RELOAD_DEFAULTS;
   aSkipOids[cSkipOids++] = OID_802_11_ADD_KEY;
   aSkipOids[cSkipOids++] = OID_802_11_REMOVE_KEY;
   aSkipOids[cSkipOids++] = OID_802_11_TEST;
   
   
   NDTLogMsg(_T("Check that listed OIDs really are supported"));

   for (ix1 = 0; ix1 < cSupportedOids; ix1++) {

      NDTLogMsg(
         _T("Checking if an OID 0x%08x should be queried"), aSupportedOids[ix1]
      );
      for (ix2 = 0; ix2 < cSkipOids; ix2++) {
         if (aSupportedOids[ix1] == aSkipOids[ix2]) break;
      }
      if (ix2 < cSkipOids) {
        NDTLogMsg(_T("The OID 0x%08x was skippen"), aSkipOids[ix2]);
         continue;
      }

      for (ix2 = 0; ix2 < cRequiredOids; ix2++) {
         if (aSupportedOids[ix1] == aRequiredOids[ix2]) break;
      }
      if (ix2 >= cRequiredOids) {
         NDTLogMsg(
            _T("The OID 0x%08x isn't required, and hence not checked"), 
            aSupportedOids[ix1]
         );
         continue;
      }

      hr = NDTQueryInfo(
         hAdapter, aSupportedOids[ix1], pacBuffer, cbacBuffSize, &cbUsed, 
         &cbRequired
      );
      if (FAILED(hr) && hr != NDT_STATUS_BUFFER_TOO_SHORT) {
         NDTLogErr(
            _T("Unable to query the OID 0x%08x with hr=0x%08x"), 
            aSupportedOids[ix1], hr
         );
         bError = TRUE;
      }

   }   

   hr = NDTUnbind(hAdapter);
   if (FAILED(hr)) {
      NDTLogErr(g_szFailUnbind, hr);
      bError = TRUE;
      goto cleanUp;
   }
   
cleanUp:
   // We have deside about test pass/fail there
   rc = bError ? TPR_FAIL : TPR_PASS;

   if (pacBuffer)
       delete [] pacBuffer;

   if (aSupportedOids)
          delete [] aSupportedOids;

   NDTLogMsg(_T("Close adapter"));
   hr = NDTClose(&hAdapter);
   if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);

   return rc;
}

//------------------------------------------------------------------------------
