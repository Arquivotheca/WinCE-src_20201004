//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

//------------------------------------------------------------------------------

BOOL WPACapabled(HANDLE hAdapter);

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
   NDIS_OID aSupportedOids[256];
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
   UCHAR acBuffer[8192];
   
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
   
   // Some of the 802_11 Oids are mandatory
   if (uiPhysicalMedium == NdisPhysicalMediumWirelessLan) {
//    aRequiredOids[cRequiredOids++] = OID_802_11_BSSID;
      aRequiredOids[cRequiredOids++] = OID_802_11_SSID;
      aRequiredOids[cRequiredOids++] = OID_802_11_NETWORK_TYPE_IN_USE;
      aRequiredOids[cRequiredOids++] = OID_802_11_RSSI;
      aRequiredOids[cRequiredOids++] = OID_802_11_INFRASTRUCTURE_MODE;
      aRequiredOids[cRequiredOids++] = OID_802_11_SUPPORTED_RATES;
      aRequiredOids[cRequiredOids++] = OID_802_11_CONFIGURATION;
      aRequiredOids[cRequiredOids++] = OID_802_11_ADD_WEP;
      aRequiredOids[cRequiredOids++] = OID_802_11_REMOVE_WEP;
      aRequiredOids[cRequiredOids++] = OID_802_11_WEP_STATUS;
      aRequiredOids[cRequiredOids++] = OID_802_11_DISASSOCIATE;
      aRequiredOids[cRequiredOids++] = OID_802_11_BSSID_LIST;
      aRequiredOids[cRequiredOids++] = OID_802_11_BSSID_LIST_SCAN;
      aRequiredOids[cRequiredOids++] = OID_802_11_AUTHENTICATION_MODE;
      aRequiredOids[cRequiredOids++] = OID_802_11_RELOAD_DEFAULTS;

	  //WPA required OIDs
	  if (WPACapabled(hAdapter))
	  {
		  NDTLogMsg(_T("%s is WPA(Wi-Fi Protected Access) Capabled"),g_szTestAdapter);
		  aRequiredOids[cRequiredOids++] = OID_802_11_ADD_KEY;
		  aRequiredOids[cRequiredOids++] = OID_802_11_REMOVE_KEY;
		  aRequiredOids[cRequiredOids++] = OID_802_11_ASSOCIATION_INFORMATION;
		  aRequiredOids[cRequiredOids++] = OID_802_11_TEST;
		  aRequiredOids[cRequiredOids++] = OID_802_11_ENCRYPTION_STATUS;
		  aRequiredOids[cRequiredOids++] = OID_802_11_NETWORK_TYPES_SUPPORTED;
	  }
	  else
		  NDTLogWrn(_T("%s is not WPA(Wi-Fi Protected Access) Capabled"),g_szTestAdapter);

   }

   NDTLogMsg(_T("Get Supported OIDs List"));
   
   hr = NDTQueryInfo(
      hAdapter, OID_GEN_SUPPORTED_LIST, aSupportedOids, sizeof(aSupportedOids),
      &cbUsed, &cbRequired
   );
   if (FAILED(hr)) {
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
         hAdapter, aSupportedOids[ix1], acBuffer, sizeof(acBuffer), &cbUsed, 
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

   NDTLogMsg(_T("Close adapter"));
   hr = NDTClose(&hAdapter);
   if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);

   return rc;
}

BOOL WPACapabled(HANDLE hAdapter)
{
	//This function checks if the miniport driver + miniport device 
	//is WPA(Wi-Fi Protected Access) Capable.
	HRESULT hr = S_OK;
	UINT cbUsed, cbRequired;

	// 1.Set OID_802_11_AUTHENTICATION_MODE with Ndis802_11AuthModeWPA.
	NDIS_802_11_AUTHENTICATION_MODE e802_11_AuthMode = Ndis802_11AuthModeWPA;
	hr = NDTSetInfo(hAdapter, OID_802_11_AUTHENTICATION_MODE, &e802_11_AuthMode,
		sizeof(e802_11_AuthMode),&cbUsed,&cbRequired);

	if (FAILED(hr)) 
	{
		NDTLogWrn(_T("Unable to Set OID_802_11_AUTHENTICATION_MODE with hr=0x%08x"),hr);
		return FALSE;
	}

	// 2.Query OID_802_11_AUTHENTICATION_MODE
	hr = NDTQueryInfo(hAdapter, OID_802_11_AUTHENTICATION_MODE, &e802_11_AuthMode,
		sizeof(e802_11_AuthMode),&cbUsed,&cbRequired);

	if (FAILED(hr)) 
	{
		NDTLogWrn(_T("Unable to query OID_802_11_AUTHENTICATION_MODE with hr=0x%08x"),hr);
		return FALSE;
	}

	// 3.If mode = Ndis802_11AuthModeWPA then WPA Enabled.
	if (e802_11_AuthMode == Ndis802_11AuthModeWPA)
		return TRUE;
	else
		return FALSE;
}

//------------------------------------------------------------------------------
