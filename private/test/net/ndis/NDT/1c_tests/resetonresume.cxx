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
#include <Pkfuncs.h>
#include <Pm.h>
#include <nkintr.h>
#include <ndis.h>

BOOL SetUpMcastAddrs(PBYTE pbListMacAddr,DWORD dwNosOfAddrs,NDIS_MEDIUM ndisMedium);
extern BOOL  g_bWirelessMiniport;

//------------------------------------------------------------------------------

TEST_FUNCTION(TestResetOnResume)
{
    TEST_ENTRY;
    
    HRESULT hr = S_OK;
    DWORD rc = TPR_FAIL;
    
    NDTLogMsg(
        _T("Start 1c_ResetOnResume (Reset on Resume) test on the adapter %s"), g_szTestAdapter
        );
    
    NDTLogMsg(
        _T("This test will test if %s restores its settings after Reset"), g_szTestAdapter
        );
    
    PBYTE pbMcastMacAddr = NULL;
    PBYTE pbMcastMacAddrVerify = NULL;
    HANDLE hAdapter = NULL;
#ifdef NDT6
    BYTE aStatusBuffer[4];
    UINT cbStatusBufferSize = 0;
#endif

    for(int i = 0; i < 1; i++)
    {
        hr = NDTOpen(g_szTestAdapter, &hAdapter);
        if (FAILED(hr)) 
        {
            NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
            break;
        }
        
        NDIS_MEDIUM ndisMedium = g_ndisMedium;
        UINT  cbAddr = 0; UINT  cbHeader = 0;
        
        // Get some information about a media
        NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);
        NDTLogVbs(_T("ProcessNIC: NDT Opened: cbAddr:%d cbHeader:%d"),cbAddr,cbHeader);
        
        BOOL bForce30 = FALSE;
        
        // Bind
        hr = NDTBind(hAdapter, bForce30, ndisMedium, &ndisMedium);
        if (FAILED(hr)) {
            NDTLogErr(g_szFailBind, hr);
            break;        
        }
        
        NDIS_PNP_CAPABILITIES sPNP = {{0}};
        DWORD dwSize=sizeof(sPNP);
        UINT cbUsed = 0; UINT cbRequired = 0;
        
        hr = NDTQueryInfo(
            hAdapter, OID_PNP_CAPABILITIES, PBYTE(&sPNP), dwSize,
            &cbUsed, &cbRequired
            );
        
        if (FAILED(hr)) {
            NDTLogMsg(_T("%s does not support Power Management"), g_szTestAdapter);
        }
        else
        {
            if (sPNP.Flags)
            {
                NDTLogMsg(_T("%s supports Power Management and")
                    _T(" one or more wake-up capabilities"), g_szTestAdapter);
            }
            else
                NDTLogMsg(_T("%s supports only Power Management, but")
                _T(" does not have wake-up capabilities"), g_szTestAdapter);
        }
        
        //Set the filter for binding.
        DWORD dwPktFilter = NDIS_PACKET_TYPE_MULTICAST|NDIS_PACKET_TYPE_DIRECTED;
        dwSize=sizeof(dwPktFilter);
        
        hr = NDTSetInfo(hAdapter, OID_GEN_CURRENT_PACKET_FILTER, PBYTE(&dwPktFilter),
            dwSize,&cbUsed,&cbRequired);
        
        if (FAILED(hr)) {
            NDTLogErr(_T("Error:Setting OID_GEN_CURRENT_PACKET_FILTER")
                _T(" for adapter %s failed:NDIS_STATUS= 0x%x"),g_szTestAdapter,hr);
            break;
        }
        
        NDTLogMsg(_T("Set CURRENT_PACKET_FILTER = NDIS_PACKET_TYPE_MULTICAST|NDIS_PACKET_TYPE_DIRECTED for adapter %s"),
            g_szTestAdapter);

        //Get the maximum list size of mcast addresses.
        DWORD dwMcastListSize;
        dwSize = sizeof(dwMcastListSize);
        
        hr = NDTQueryInfo(
            hAdapter, OID_802_3_MAXIMUM_LIST_SIZE, PBYTE(&dwMcastListSize), dwSize,
            &cbUsed, &cbRequired
            );
        
        if (FAILED(hr)) {
            NDTLogErr(_T("Error:Quering OID_802_3_MAXIMUM_LIST_SIZE")
                _T(" for adapter %s failed:NDIS_STATUS= 0x%x"),g_szTestAdapter,hr);
            break;
        }

        NDTLogMsg(_T("%s supports max %d Multicast addresses"),
            g_szTestAdapter, dwMcastListSize);

        if (dwMcastListSize < 32) {
            NDTLogWrn(_T("Miniport driver must support at least 32 multicast addresses"));
        }

        //Allocate the memory for the mcast mac addresses.
        DWORD dwMcastListSizeInBytes = dwMcastListSize * 6;
        pbMcastMacAddr = (PBYTE) new char[dwMcastListSizeInBytes];
        dwSize = dwMcastListSizeInBytes;
        
        //Get the current list of mcast addresses.
        hr = NDTQueryInfo(
            hAdapter, OID_802_3_MULTICAST_LIST, pbMcastMacAddr, dwSize,
            &cbUsed, &cbRequired
            );
        
        if (FAILED(hr)) {
            NDTLogErr(_T("Error:Quering OID_802_3_MULTICAST_LIST")
                _T(" for adapter %s failed:NDIS_STATUS= 0x%x"),g_szTestAdapter,hr);
            break;
        }

        //Get the current number of mcast addresses set.
        dwSize = cbUsed/6;

        NDTLogMsg(_T("Got the current complete list of multicast addresses for %s "),g_szTestAdapter);
        NDTLogMsg(_T("It has %d multicast addresses set"),dwSize);

        ASSERT(dwSize == 0);
        
        //Now Setup Mcast addresses in a buffer
        //If we setup Max nos. of Mcast addresses, chances are there that the list may overflow,
        //as the list is global for all the protocol bindings to a NIC.
        dwMcastListSize = dwMcastListSize - (2 + dwSize) ;
        
        // We should take care when the test is run with -nounbind.
        if (!SetUpMcastAddrs(pbMcastMacAddr,dwMcastListSize,ndisMedium)) 
        {
            NDTLogErr(_T("Failed to create multicast addresses in the array"));
            break;
        }
        
        dwSize = dwMcastListSize * 6; //This is the size in bytes reqd. for setting OID.
        
        //Now set the Mcast addresses in the list of current NIC.
        //The deal with "bOneMoreTime" is it helps to reissue set multicast list if it fails one time.
        //Note that Multicast list is global one & hence other protocol drivers can change it.
        BOOL bOneMoreTime = FALSE;
        do
        {
            hr = NDTSetInfo(hAdapter, OID_802_3_MULTICAST_LIST, pbMcastMacAddr,
                dwSize,&cbUsed,&cbRequired);
            
            if (NDIS_FROM_HRESULT(hr) == NDIS_STATUS_MULTICAST_FULL)
            {
                dwMcastListSize = 4;
                dwSize = dwMcastListSize * 6;
                bOneMoreTime = !bOneMoreTime;
            }
            else
                break;
            
        } while(bOneMoreTime);
        
        if (FAILED(hr)) {
            NDTLogErr(_T("Error:Setting OID_802_3_MULTICAST_LIST")
                _T(" for adapter %s failed:NDIS_STATUS= 0x%x"),g_szTestAdapter,hr);
            break;
        }
        
        NDTLogMsg(_T("Set %d multicast addresses for %s"),dwMcastListSize, g_szTestAdapter);
        NDTLogMsg(_T("We'll RESET the device & at the end of every RESET we'll verify if %s restores its settings or not\n"),
            g_szTestAdapter);

        for (INT i=0;i<5;++i)
        {
            HANDLE hStatus = NULL;

            //First start the waiting for event notification.
            hr = NDTStatus(hAdapter, NDIS_STATUS_RESET_END, &hStatus);
            
            if (FAILED(hr)) {
                NDTLogErr(_T("Failed to start receiving notifications on %s Error : 0x%x"), g_szTestAdapter,NDIS_FROM_HRESULT(hr));
                break;
            }

            NDTLogMsg(_T("Reseting the network adapter %s %d time"),g_szTestAdapter,i+1);

            hr = NDTReset(hAdapter);
            if (FAILED(hr)) {

                HRESULT hrErr = NDIS_FROM_HRESULT(hr);
                if (hrErr==NDIS_STATUS_NOT_RESETTABLE)
                {
                    NDTLogWrn(_T("%s adapter is ** NOT RESETTABLE **"), g_szTestAdapter);
                    rc = TPR_SKIP;
                }
                else
                {
                    NDTLogErr(_T("Failed to reset %s Error : 0x%x"), g_szTestAdapter,hrErr);
                    rc = TPR_FAIL;
                }
                break;
            }
            
            NDTLogMsg(_T("Waiting to receive NDIS_STATUS_RESET_END notification on the miniport adapter %s"), g_szTestAdapter);

#ifdef NDT6
            hr = NDTStatusWait(hAdapter, hStatus, 30000, &cbStatusBufferSize, aStatusBuffer);
#else
            hr = NDTStatusWait(hAdapter, hStatus, 30000);
#endif

            if ( FAILED(hr) || (hr == NDT_STATUS_PENDING) ) {
                NDTLogErr(_T("Failed to receive NDIS_STATUS_RESET_END Error : 0x%x"), NDIS_FROM_HRESULT(hr));
                break;
            }

            NDTLogMsg(_T("Received NDIS_STATUS_RESET_END notification on %s"), g_szTestAdapter);

            if (g_bWirelessMiniport)
            {
                UINT  uiConnectStatus = NdisMediaStateDisconnected;

                NDTLogMsg(_T("Querying connect status & waiting for NdisMediaStateConnected"));

                for (INT j=0;j<24;++j)
                {
                    Sleep(5000); //Sleep for 5 second
                    uiConnectStatus = NdisMediaStateDisconnected;
                    hr = NDTQueryInfo(
                        hAdapter, OID_GEN_MEDIA_CONNECT_STATUS, &uiConnectStatus,
                        sizeof(UINT), NULL, NULL
                        );
                    
                    if (FAILED(hr)) {
                        NDTLogErr(g_szFailQueryInfo, hr);
                        rc = TPR_FAIL;
                        break;
                    } else if (uiConnectStatus == NdisMediaStateConnected)
                        break;
                }
                
                if (uiConnectStatus != NdisMediaStateConnected)
                {
                    NDTLogErr(_T("Wireless adapter is not yet associated even waiting for 2 min after RESET complete"));
                    rc = TPR_FAIL;
                    break;
                }
            }

            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // Now its time to verify if "Packet filter settings" and "multicast list" are unchanged.
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            
            //Get the packet filter settings
            DWORD dwPktFilterVerify;
            dwSize=sizeof(dwPktFilterVerify);
            
            hr = NDTQueryInfo(
                hAdapter, OID_GEN_CURRENT_PACKET_FILTER, PBYTE(&dwPktFilterVerify), dwSize,
                &cbUsed, &cbRequired
                );
            
            if (FAILED(hr)) {
                //An Error.
                NDTLogErr(_T("Error:Quering OID_GEN_CURRENT_PACKET_FILTER")
                    _T(" for adapter %s failed:NDIS_STATUS= 0x%x"),g_szTestAdapter,hr);
                break;
            }
            
            ASSERT(dwPktFilterVerify == dwPktFilter);
            if (dwPktFilterVerify != dwPktFilter)
            {
                NDTLogErr(_T("%s failed to restore Packet Filter settings."),g_szTestAdapter);
                NDTLogErr(_T("%s has failed ResetOnResume test"),g_szTestAdapter);
                continue;
            }
            
            NDTLogMsg(_T("%s has restored/retained its Packet Filter settings."),g_szTestAdapter);

            //Now Query again for Mcast addresses.
            pbMcastMacAddrVerify = (PBYTE) new char[dwMcastListSizeInBytes];
            dwSize = dwMcastListSizeInBytes;
            
            //Get the current list of mcast addresses.
            hr = NDTQueryInfo(
                hAdapter, OID_802_3_MULTICAST_LIST, pbMcastMacAddrVerify, dwSize,
                &cbUsed, &cbRequired
                );
            
            if (FAILED(hr)) {
                //An Error.
                NDTLogErr(_T("Error:Quering OID_802_3_MULTICAST_LIST")
                    _T(" for adapter %s failed:NDIS_STATUS= 0x%x"),g_szTestAdapter,hr);
                break;
            }
            
            //Now lets get the current number of multicast addresses
            dwSize = cbUsed/6;

            ASSERT(dwSize != 0);
            
            if (dwSize != dwMcastListSize)
            {
                NDTLogMsg(_T("Number Multicast addresses: before Suspend/Resume = %d after = %d"),
                    dwMcastListSize,dwSize);
                NDTLogErr(_T("%s failed to restore size of Multicast Addresses list."),g_szTestAdapter);
                NDTLogErr(_T("%s has failed ResetOnResume test"),g_szTestAdapter);
                continue;
            }
            
            INT iRet = memcmp(pbMcastMacAddr,pbMcastMacAddrVerify,dwSize);
            ASSERT(iRet == 0);
            
            if (iRet!= 0)
            {
                NDTLogErr(_T("%s failed to restore %d Multicast Addresses"),g_szTestAdapter,dwMcastListSize);
                NDTLogErr(_T("%s has failed ResetOnResume test"),g_szTestAdapter);
                continue;
            }
            
            rc = TPR_PASS;
            NDTLogMsg(_T("%s has restored/retained its multicast addresses list."),g_szTestAdapter);
            NDTLogMsg(_T("%s has passed ResetOnResume test"),g_szTestAdapter);
        } // for { suspend/resume 5 times }

    }
    
    if (pbMcastMacAddr)
        delete [] pbMcastMacAddr;
    
    if (pbMcastMacAddrVerify)
        delete [] pbMcastMacAddrVerify;
    
    hr = NDTUnbind(hAdapter);
    if (FAILED(hr)) NDTLogErr(g_szFailUnbind, hr);
    
    hr = NDTClose(&hAdapter);
    if (FAILED(hr)) NDTLogErr(g_szFailClose, hr);

    return rc;
}

//------------------------------------------------------------------------------

BOOL FillUpMcastAddr(PBYTE pucAddr,DWORD dwVar,NDIS_MEDIUM ndisMedium)
{
    if (pucAddr == NULL)
        return FALSE;

   switch (ndisMedium) {
   case NdisMedium802_3:
   case NdisMediumFddi:
       pucAddr[0] = 0x01; pucAddr[1] = 0x02; pucAddr[2] = 0x03;
       pucAddr[3] = 0x04; pucAddr[4] = (UCHAR)(dwVar/256);
       pucAddr[5] = (UCHAR)dwVar;
       break;
   case NdisMedium802_5:
       pucAddr[0] = 0xC0; pucAddr[1] = 0x00; pucAddr[2] = 0x01;
       pucAddr[3] = 0x02; pucAddr[4] = (UCHAR)(dwVar/256);
       pucAddr[5] = (UCHAR)dwVar;
       break;
   default:
       return FALSE;
   }

    return TRUE;
}

//------------------------------------------------------------------------------

BOOL SetUpMcastAddrs(PBYTE pbListMacAddr,DWORD dwNosOfAddrs,NDIS_MEDIUM ndisMedium)
{
    if (pbListMacAddr == NULL)
        return FALSE;

    PBYTE pb = pbListMacAddr;

    for (DWORD i=0;i<dwNosOfAddrs;++i)
    {
        if (!FillUpMcastAddr(pb,i,ndisMedium))
            return FALSE;
        pb=pb+6;
    }
    return TRUE;
}

//------------------------------------------------------------------------------
