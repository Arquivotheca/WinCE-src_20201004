//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "NDTError.h"
#include "NDTLib.h"
#include "NDTLibEx.h"
#include "NDTNDIS.h"

//------------------------------------------------------------------------------

void NDTGetMediumInfo(NDIS_MEDIUM ndisMedium, UINT* pcbAddr, UINT* pcbHeader) 
{
   switch (ndisMedium) {
   case NdisMedium802_3:
      *pcbAddr = 6;
      *pcbHeader = 14;
      break;
   case NdisMedium802_5:
      *pcbAddr = 6;
      *pcbHeader = 14;
      break;
   case NdisMediumFddi:
      *pcbAddr = 6;
      *pcbHeader = 14;
      break;
   default:
      *pcbAddr = 0;
      *pcbHeader = 0;
      break;
   }
}

//------------------------------------------------------------------------------

UCHAR* NDTGetRandomAddr(NDIS_MEDIUM ndisMedium)
{
   UCHAR* pucAddr = NULL;
   
   switch (ndisMedium) {
   case NdisMedium802_3:
   case NdisMediumFddi:
   case NdisMedium802_5:
      pucAddr = new UCHAR[6];
      pucAddr[0] = 0x00; pucAddr[1] = 0x02; pucAddr[2] = 0x04;
      pucAddr[3] = 0x06; pucAddr[4] = 0x08; pucAddr[5] = 0x0A;
      break;
   }
   return pucAddr;
}

//------------------------------------------------------------------------------

UCHAR* NDTGetBroadcastAddr(NDIS_MEDIUM ndisMedium)
{
   UCHAR* pucAddr = NULL;
   
   switch (ndisMedium) {
   case NdisMedium802_3:
   case NdisMediumFddi:
      pucAddr = new UCHAR[6];
      pucAddr[0] = 0xFF; pucAddr[1] = 0xFF; pucAddr[2] = 0xFF;
      pucAddr[3] = 0xFF; pucAddr[4] = 0xFF; pucAddr[5] = 0xFF;
      break;
   case NdisMedium802_5:
      pucAddr = new UCHAR[6];
      pucAddr[0] = 0xC0; pucAddr[1] = 0x00; pucAddr[2] = 0xFF;
      pucAddr[3] = 0xFF; pucAddr[4] = 0xFF; pucAddr[5] = 0xFF;
      break;
   }
   return pucAddr;
}

//------------------------------------------------------------------------------

UCHAR* NDTGetMulticastAddr(NDIS_MEDIUM ndisMedium)
{
   UCHAR* pucAddr = NULL;
   
   switch (ndisMedium) {
   case NdisMedium802_3:
   case NdisMediumFddi:
      pucAddr = new UCHAR[6];
      pucAddr[0] = 0x01; pucAddr[1] = 0x02; pucAddr[2] = 0x03;
      pucAddr[3] = 0x04; pucAddr[4] = 0x05; pucAddr[5] = 0x06;
      break;
   case NdisMedium802_5:
      pucAddr = new UCHAR[6];
      pucAddr[0] = 0xC0; pucAddr[1] = 0x00; pucAddr[2] = 0x01;
      pucAddr[3] = 0x02; pucAddr[4] = 0x03; pucAddr[5] = 0x04;
      break;
   }
   return pucAddr;
}

//------------------------------------------------------------------------------

UCHAR* NDTGetMulticastAddrEx(NDIS_MEDIUM ndisMedium, UINT uiVar)
{
   UCHAR* pucAddr = NULL;
   
   switch (ndisMedium) {
   case NdisMedium802_3:
   case NdisMediumFddi:
      pucAddr = new UCHAR[6];
      pucAddr[0] = 0x01; pucAddr[1] = 0x02; pucAddr[2] = 0x03;
      pucAddr[3] = 0x04; pucAddr[4] = (UCHAR)(uiVar/256);
      pucAddr[5] = (UCHAR)uiVar;
      break;
   case NdisMedium802_5:
      pucAddr = new UCHAR[6];
      pucAddr[0] = 0xC0; pucAddr[1] = 0x00; pucAddr[2] = 0x01;
      pucAddr[3] = 0x02; pucAddr[4] = (UCHAR)(uiVar/256);
      pucAddr[5] = (UCHAR)uiVar;
      break;
   }
      
   return pucAddr;
}

//------------------------------------------------------------------------------

UCHAR* NDTGetBadMulticastAddr(NDIS_MEDIUM ndisMedium)
{
   UCHAR *pucAddr = NULL;
   
   switch (ndisMedium) {
   case NdisMedium802_3:
   case NdisMedium802_5:
   case NdisMediumFddi:
      pucAddr = new UCHAR[6];
      pucAddr[0] = 0x01; pucAddr[1] = 0x02; pucAddr[2] = 0x03;
      pucAddr[3] = 0x04; pucAddr[4] = 0x05; pucAddr[5] = 0x00;
      break;
   }
   return pucAddr;
}

//------------------------------------------------------------------------------

UCHAR* NDTGetGroupAddr(NDIS_MEDIUM ndisMedium)
{
   UCHAR* pucAddr = NULL;
   
   switch (ndisMedium) {
   case NdisMedium802_5:
      pucAddr = new UCHAR[6];
      pucAddr[0] = 0xC0; pucAddr[1] = 0x00; pucAddr[2] = 0x81;
      pucAddr[3] = 0x02; pucAddr[4] = 0x03; pucAddr[5] = 0x04;
      break;
   }
   return pucAddr;
}

//------------------------------------------------------------------------------

HRESULT NDTGetPermanentAddr(
   HANDLE hAdapter, NDIS_MEDIUM ndisMedium, UCHAR** ppucAddress
)
{
   HRESULT hr = S_OK;
   UINT cbAddr = 0;
   NDIS_OID ndisOid = 0;
   
   switch (ndisMedium) {
   case NdisMedium802_3:
      cbAddr = 6;
      ndisOid = OID_802_3_PERMANENT_ADDRESS;
      break;
   case NdisMedium802_5:
      cbAddr = 6;
      ndisOid = OID_802_5_PERMANENT_ADDRESS;
      break;
   case NdisMediumFddi:
      cbAddr = 6;
      ndisOid = OID_FDDI_LONG_CURRENT_ADDR;
      break;
   }

   *ppucAddress = new UCHAR[cbAddr];
   if (*ppucAddress == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }

   hr = NDTQueryInfo(hAdapter, ndisOid, *ppucAddress, cbAddr, NULL, NULL);
   if (FAILED(hr)) goto cleanUp;
   
cleanUp:   
   return hr;
}

//------------------------------------------------------------------------------

DWORD NDTGetReceiveDelay(NDIS_MEDIUM ndisMedium)
{
   DWORD dwDelay = 0;
   
   switch (ndisMedium) {
   case NdisMedium802_3:
      dwDelay = 2000;
      break;
   case NdisMedium802_5:
      dwDelay = 2000;
      break;
   case NdisMediumFddi:
      dwDelay = 400;
      break;
   }
   return dwDelay;
}

//------------------------------------------------------------------------------

HRESULT NDTGetMaximumFrameSize(HANDLE hAdapter, UINT* puiMaximumFrameSize)
{
   return NDTQueryInfo(
      hAdapter, OID_GEN_MAXIMUM_FRAME_SIZE, puiMaximumFrameSize, 
      sizeof(UINT), NULL, NULL
   );
}

//------------------------------------------------------------------------------

HRESULT NDTGetMaximumTotalSize(HANDLE hAdapter, UINT* puiMaximumTotalSize)
{
   return NDTQueryInfo(
      hAdapter, OID_GEN_MAXIMUM_TOTAL_SIZE, puiMaximumTotalSize, 
      sizeof(UINT), NULL, NULL
   );
}

//------------------------------------------------------------------------------

HRESULT NDTGetPhysicalMedium(HANDLE hAdapter, UINT* puiMedium)
{
   HRESULT hr = S_OK;

   hr = NDTQueryInfo(
      hAdapter, OID_GEN_PHYSICAL_MEDIUM, puiMedium, sizeof(UINT), NULL, NULL
   );
   if (hr == NDT_STATUS_INVALID_OID || hr == NDT_STATUS_NOT_SUPPORTED) {
      *puiMedium = 0;
      hr = S_OK;
   }
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTGetLinkSpeed(HANDLE hAdapter, UINT *puiLinkSpeed)
{
   return NDTQueryInfo(
      hAdapter, OID_GEN_LINK_SPEED, puiLinkSpeed, sizeof(UINT), NULL, NULL
   );
}

//------------------------------------------------------------------------------

HRESULT NDTSetPacketFilter(HANDLE hAdapter, UINT uiFilter)
{
   return NDTSetInfo(
      hAdapter, OID_GEN_CURRENT_PACKET_FILTER, &uiFilter, sizeof(uiFilter),
      NULL, NULL
   );
}

//------------------------------------------------------------------------------

HRESULT NDTGetFilters(HANDLE hAdapter, NDIS_MEDIUM ndisMedium, UINT *puiFilters)
{
   HRESULT hr = S_OK;
   UINT uiFilters = 0;
   
   switch (ndisMedium) {
   case NdisMedium802_3:
      uiFilters  = NDT_FILTER_DIRECTED | NDT_FILTER_BROADCAST;
      uiFilters |= NDT_FILTER_MULTICAST;
      hr = NDTSetPacketFilter(hAdapter, NDT_FILTER_ALL_MULTICAST);
      if (SUCCEEDED(hr)) uiFilters |= NDT_FILTER_ALL_MULTICAST;
      break;
   case NdisMedium802_5:
      uiFilters  = NDT_FILTER_DIRECTED | NDT_FILTER_BROADCAST;
      uiFilters |= NDT_FILTER_FUNCTIONAL | NDT_FILTER_GROUP_PKT;
      hr = NDTSetPacketFilter(hAdapter, NDT_FILTER_ALL_FUNCTIONAL);
      if (SUCCEEDED(hr)) uiFilters |= NDT_FILTER_ALL_FUNCTIONAL;
      hr = NDTSetPacketFilter(hAdapter, NDT_FILTER_SOURCE_ROUTING);
      if (SUCCEEDED(hr)) uiFilters |= NDT_FILTER_SOURCE_ROUTING;
      hr = NDTSetPacketFilter(hAdapter, NDT_FILTER_MAC_FRAME);
      if (SUCCEEDED(hr)) uiFilters |= NDT_FILTER_MAC_FRAME;
      break;
   case NdisMediumFddi:
      uiFilters  = NDT_FILTER_DIRECTED | NDT_FILTER_BROADCAST;
      uiFilters |= NDT_FILTER_MULTICAST;
      hr = NDTSetPacketFilter(hAdapter, NDT_FILTER_ALL_MULTICAST);
      if (SUCCEEDED(hr)) uiFilters |= NDT_FILTER_ALL_MULTICAST;
      hr = NDTSetPacketFilter(hAdapter, NDT_FILTER_SOURCE_ROUTING);
      if (SUCCEEDED(hr)) uiFilters |= NDT_FILTER_SOURCE_ROUTING;
      hr = NDTSetPacketFilter(hAdapter, NDT_FILTER_MAC_FRAME);
      if (SUCCEEDED(hr)) uiFilters |= NDT_FILTER_MAC_FRAME;
      hr = NDTSetPacketFilter(hAdapter, NDT_FILTER_SMT);
      if (SUCCEEDED(hr)) uiFilters |= NDT_FILTER_SMT;
      break;
   }

   hr = NDTSetPacketFilter(hAdapter, NDT_FILTER_PROMISCUOUS);
   if (SUCCEEDED(hr)) uiFilters |= NDT_FILTER_PROMISCUOUS;
   
   hr = NDTSetPacketFilter(hAdapter, NDT_FILTER_ALL_LOCAL);
   if (SUCCEEDED(hr)) uiFilters |= NDT_FILTER_ALL_LOCAL;

   *puiFilters = uiFilters;
   hr = NDTSetPacketFilter(hAdapter, 0);

   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTAddMulticastAddr(
   HANDLE hAdapter, NDIS_MEDIUM ndisMedium, UCHAR* pucAddr

)
{
   HRESULT hr = S_OK;
   UCHAR* pucMultiAddrs = NULL;
   UINT cMultiAddrs = 0;
   NDIS_OID oid = 0;
   NDIS_OID oidSize = 0;
   UINT cbAddr = 0;
   UINT uiSize = 0;
   UINT uiNeeded = 0;
   UINT ui = 0;

   switch (ndisMedium) {
   case NdisMedium802_3:
   case NdisMediumDix:
      cbAddr = 6;
      oid = OID_802_3_MULTICAST_LIST;
      oidSize = OID_802_3_MAXIMUM_LIST_SIZE;
      break;
   case NdisMediumFddi:
      cbAddr = 6;
      oid = OID_FDDI_LONG_MULTICAST_LIST;
      oidSize = OID_FDDI_LONG_MAX_LIST_SIZE;
      break;
   default:
      hr = E_UNEXPECTED;
      goto cleanUp;
   }

   // Get a list size
   hr = NDTQueryInfo(hAdapter, oidSize, &uiSize, sizeof(UINT), NULL, NULL);
   if (FAILED(hr)) goto cleanUp;
   
   // Allocate a memory
   pucMultiAddrs = new UCHAR[uiSize * cbAddr];
   if (pucMultiAddrs == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }
   
   // Get a multicast addresses
   hr = NDTQueryInfo(
      hAdapter, oid, pucMultiAddrs, uiSize * cbAddr, &uiNeeded, NULL
   );
   if (FAILED(hr)) goto cleanUp;

   // We get such amount of multicast address 
   cMultiAddrs = uiNeeded / cbAddr;

   // May be an address is already set
   for (ui = 0; ui < cMultiAddrs; ui++) {
      if (memcmp(pucAddr, pucMultiAddrs + ui*cbAddr, cbAddr) == 0) break;
   }

   // Address is already in a list, so we don't have to set it
   if (ui < cMultiAddrs) goto cleanUp;

   // There should be a room for next address
   if (cMultiAddrs >= uiSize) {
      hr = NDT_STATUS_RESOURCES;
      goto cleanUp;
   }

   // And now append address to current list
   memcpy(pucMultiAddrs + uiNeeded, pucAddr, cbAddr);
   uiNeeded += cbAddr;

   // And set it
   hr = NDTSetInfo(hAdapter, oid, pucMultiAddrs, uiNeeded, NULL, NULL);
   if (FAILED(hr)) goto cleanUp;
   
cleanUp:
   delete [] pucMultiAddrs;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTDeleteMulticastAddr(
   HANDLE hAdapter, NDIS_MEDIUM ndisMedium, UCHAR *pucAddr
)
{
   HRESULT hr = S_OK;
   UCHAR* pucMultiAddrs = NULL;
   UINT cMultiAddrs = 0;
   NDIS_OID oid = 0;
   NDIS_OID oidSize = 0;
   UINT cbAddr = 0;
   UINT uiSize = 0;
   UINT uiNeeded = 0;
   UINT ui = 0;


   switch (ndisMedium) {
   case NdisMedium802_3:
   case NdisMediumDix:
      cbAddr = 6;
      oid = OID_802_3_MULTICAST_LIST;
      oidSize = OID_802_3_MAXIMUM_LIST_SIZE;
      break;
   case NdisMediumFddi:
      cbAddr = 6;
      oid = OID_FDDI_LONG_MULTICAST_LIST;
      oidSize = OID_FDDI_LONG_MAX_LIST_SIZE;
      break;
   default:
      hr = E_UNEXPECTED;
      goto cleanUp;
   }

   // Get a list size
   hr = NDTQueryInfo(hAdapter, oidSize, &uiSize, sizeof(UINT), NULL, NULL);
   if (FAILED(hr)) goto cleanUp;

   // Allocate a memory
   pucMultiAddrs = new UCHAR[uiSize * cbAddr];
   if (pucMultiAddrs == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }
   
   // Get a multicast addresses
   hr = NDTQueryInfo(
      hAdapter, oid, pucMultiAddrs, uiSize * cbAddr, &uiNeeded, NULL
   );
   if (FAILED(hr)) goto cleanUp;

   // We get such amount of multicast address 
   cMultiAddrs = uiNeeded / cbAddr;

   // The address should be set
   for (ui = 0; ui < cMultiAddrs; ui++) {
      if (memcmp(pucAddr, pucMultiAddrs + ui*cbAddr, cbAddr) == 0) break;
   }

   // Address isn't in a list, so we don't have to remove it
   if (ui >= cMultiAddrs) goto cleanUp;

   // And now remove address from list (ok this can be unsafe in some cases)
   if ((ui + 1) < cMultiAddrs) memcpy(
      pucMultiAddrs + ui * cbAddr, pucMultiAddrs + (ui + 1) * cbAddr, 
      cbAddr * (cMultiAddrs - ui - 1)
   );
   uiNeeded -= cbAddr;

   // And set it
   hr = NDTSetInfo(hAdapter, oid, pucMultiAddrs, uiNeeded, NULL, NULL);
   if (FAILED(hr)) goto cleanUp;
      
cleanUp:
   delete [] pucMultiAddrs;
   return hr;
}

//------------------------------------------------------------------------------

ULONG NDTGetRandom(ULONG ulLow, ULONG ulHigh)
{
   static ULONG ulRandomNumber = -1;
   const  ULONG ulRandomMultiplier =  9301;
   const  ULONG ulRandomAddend = 49297;

   if (ulRandomNumber == -1) ulRandomNumber = GetTickCount();
   ulRandomNumber = (ulRandomNumber * ulRandomMultiplier) + ulRandomAddend;
   return ulRandomNumber % (ulHigh - ulLow + 1) + ulLow;
}

//------------------------------------------------------------------------------
