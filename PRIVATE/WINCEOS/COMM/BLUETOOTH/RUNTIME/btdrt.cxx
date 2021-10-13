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
//------------------------------------------------------------------------------
// 
//      Bluetooth Client API Layer
// 
// 
// Module Name:
// 
//      btdrt.cxx
// 
// Abstract:
// 
//      This file implements Bluetooth Client API Layer
// 
// 
//------------------------------------------------------------------------------
#include <windows.h>
#include <svsutil.hxx>
#include <bt_buffer.h>
#include <bt_ddi.h>
#include <winsock2.h>
#include <bt_api.h>
#include <bt_ioctl.h>
#include <service.h>

extern "C" const BOOL g_fBthApiCOM;


HANDLE hDev = NULL;

int Initialize (void) {
    if (hDev)
        return TRUE;

    hDev = CreateFile (L"BTD0:", GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL, OPEN_EXISTING, 0, NULL);

    return (hDev != INVALID_HANDLE_VALUE) ? TRUE : FALSE;
}

int BthGetCurrentMode
(
BT_ADDR            *pbt,
unsigned char    *pmode
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthCurrentMode_p.bt = *pbt;

    int iRes = ((DeviceIoControl (hDev, BT_IOCTL_BthGetCurrentMode, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
    if (iRes == ERROR_SUCCESS)
        *pmode = bc.BthCurrentMode_p.mode;

    return iRes;
}

int BthGetBasebandHandles
(
int                cHandles,
unsigned short    *pHandles,
int                *pcHandlesReturned
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthBasebandHandles_p.cMaxHandles = cHandles;
    bc.BthBasebandHandles_p.pHandles = pHandles;

    int iRes = ((DeviceIoControl (hDev, BT_IOCTL_BthGetBasebandHandles, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());

    *pcHandlesReturned = bc.BthBasebandHandles_p.cHandlesReturned;

    return iRes;
}

int BthGetBasebandConnections
(
int                        cConnections,
BASEBAND_CONNECTION        *pConnections,
int                        *pcConnectionsReturned
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;
    
    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    BASEBAND_CONNECTION_DATA buffer[15];
    BASEBAND_CONNECTION_DATA *pConnData = buffer;

    if (cConnections > 15) {
        PREFAST_SUPPRESS (419, "This allocation is safe.  If cConnections is too big that is handled.");
        pConnData = new BASEBAND_CONNECTION_DATA[cConnections];
        if (! pConnData)
            return ERROR_OUTOFMEMORY;
    }

    bc.BthBasebandConnections_p.cMaxConnections = cConnections;
    bc.BthBasebandConnections_p.pConnections = pConnData;
    bc.BthBasebandConnections_p.cConnectionsReturned = 0;

    int iRes = ((DeviceIoControl (hDev, BT_IOCTL_BthGetBasebandConnections, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());

    *pcConnectionsReturned = bc.BthBasebandConnections_p.cConnectionsReturned;

    if (iRes == ERROR_SUCCESS) {
        if (*pcConnectionsReturned > cConnections) {
            if (pConnData != buffer)
                delete[] pConnData;
            return ERROR_INSUFFICIENT_BUFFER;
        }

        // Copy back to a BASEBAND_CONNECTION structure
        for (int i = 0; i < *pcConnectionsReturned; i++) {
            memset(&pConnections[i], 0, sizeof(BASEBAND_CONNECTION));
            pConnections[i].baAddress =  SET_NAP_SAP(pConnData[i].baAddress.NAP, pConnData[i].baAddress.SAP);
            pConnections[i].cDataPacketsPending = pConnData[i].cDataPacketsPending;
            pConnections[i].fAuthenticated = pConnData[i].fAuthenticated;
            pConnections[i].fEncrypted = pConnData[i].fEncrypted;            
            pConnections[i].fMode = pConnData[i].mode;
            pConnections[i].hConnection = pConnData[i].hConnection;
            
            // We'll return 1 for ACL connections and 0 for all other connections (SCO/eSCO)
            // If users really need to distinguish SCO/eSCO they need to use the Ex version
            // of this API.
            pConnections[i].fLinkType = (pConnData[i].link_type == BTH_LINK_TYPE_ACL) ? 1 : 0;
        }
    }

    if (pConnData != buffer)
        delete[] pConnData;

    return iRes;
}


int BthGetBasebandConnectionsEx
(
DWORD                      dwApiVersion,
int                        cConnections,
BASEBAND_CONNECTION_EX     *pConnections,
int                        *pcConnectionsReturned
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    BASEBAND_CONNECTION_DATA buffer[15];
    BASEBAND_CONNECTION_DATA *pConnData = buffer;

    if (cConnections > 15) {
        PREFAST_SUPPRESS (419, "This allocation is safe.  If cConnections is too big that is handled.");
        pConnData = new BASEBAND_CONNECTION_DATA[cConnections];
        if (! pConnData)
            return ERROR_OUTOFMEMORY;
    }

    bc.BthBasebandConnections_p.cMaxConnections = cConnections;
    bc.BthBasebandConnections_p.pConnections = pConnData;
    bc.BthBasebandConnections_p.cConnectionsReturned = 0;

    int iRes = ((DeviceIoControl (hDev, BT_IOCTL_BthGetBasebandConnections, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());

    *pcConnectionsReturned = bc.BthBasebandConnections_p.cConnectionsReturned;

    if (iRes == ERROR_SUCCESS) {
        if (*pcConnectionsReturned > cConnections) {
            if (pConnData != buffer)
                delete[] pConnData;
            return ERROR_INSUFFICIENT_BUFFER;
        }

        // Copy back to a BASEBAND_CONNECTION_EX structure
        for (int i = 0; i < *pcConnectionsReturned; i++) {
            memset(&pConnections[i], 0, sizeof(BASEBAND_CONNECTION_EX));
            pConnections[i].baAddress =  SET_NAP_SAP(pConnData[i].baAddress.NAP, pConnData[i].baAddress.SAP);
            pConnections[i].cDataPacketsPending = pConnData[i].cDataPacketsPending;
            pConnections[i].fAuthenticated = pConnData[i].fAuthenticated;
            pConnections[i].fEncrypted = pConnData[i].fEncrypted;
            pConnections[i].link_type = pConnData[i].link_type;
            pConnections[i].mode = pConnData[i].mode;
            pConnections[i].hConnection = pConnData[i].hConnection;
        }
    }

    if (pConnData != buffer)
        delete[] pConnData;

    return iRes;
}

int BthGetAddress
(
unsigned short    handle,
BT_ADDR            *pba
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthGetAddress_p.handle = handle;

    int iRes = ((DeviceIoControl (hDev, BT_IOCTL_BthGetAddress, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
    if (iRes == ERROR_SUCCESS)
        *pba = bc.BthGetAddress_p.ba;

    return iRes;
}

int BthWriteScanEnableMask
(
unsigned char    mask
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthScanEnableMask_p.mask = mask;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthWriteScanEnableMask, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthReadScanEnableMask
(
unsigned char    *pmask
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    if (DeviceIoControl (hDev, BT_IOCTL_BthReadScanEnableMask, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) {
        *pmask = bc.BthScanEnableMask_p.mask;
        return ERROR_SUCCESS;
    }

    return GetLastError ();
}

int BthWriteAuthenticationEnable
(
unsigned char    ae
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthAuthenticationEnable_p.ae = ae;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthWriteAuthenticationEnable, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthReadAuthenticationEnable
(
unsigned char    *pae
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    if (DeviceIoControl (hDev, BT_IOCTL_BthReadAuthenticationEnable, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) {
        *pae = bc.BthAuthenticationEnable_p.ae;
        return ERROR_SUCCESS;
    }

    return GetLastError ();
}

int BthWriteLinkPolicySettings
(
BT_ADDR            *pbt,
unsigned short    lps
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthLinkPolicySettings_p.bt = *pbt;
    bc.BthLinkPolicySettings_p.lps = lps;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthWriteLinkPolicySettings, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthReadLinkPolicySettings
(
BT_ADDR            *pbt,
unsigned short    *plps
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthLinkPolicySettings_p.bt = *pbt;

    if (DeviceIoControl (hDev, BT_IOCTL_BthReadLinkPolicySettings, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) {
        *plps = bc.BthLinkPolicySettings_p.lps;
        return ERROR_SUCCESS;
    }

    return GetLastError ();
}

int BthEnterHoldMode
(
BT_ADDR            *pbt,
unsigned short    hold_mode_max,
unsigned short    hold_mode_min,
unsigned short    *pinterval
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthHold_p.ba = *pbt;
    bc.BthHold_p.hold_mode_max = hold_mode_max;
    bc.BthHold_p.hold_mode_min = hold_mode_min;

    int iRes = ((DeviceIoControl (hDev, BT_IOCTL_BthEnterHoldMode, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
    if ((iRes == ERROR_SUCCESS) && pinterval)
        *pinterval = bc.BthHold_p.interval;

    return iRes;
}

int BthEnterSniffMode
(
BT_ADDR            *pbt,
unsigned short    sniff_mode_max,
unsigned short    sniff_mode_min,
unsigned short    sniff_attempt,
unsigned short    sniff_timeout,
unsigned short    *pinterval
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthSniff_p.ba = *pbt;
    bc.BthSniff_p.sniff_mode_max = sniff_mode_max;
    bc.BthSniff_p.sniff_mode_min = sniff_mode_min;
    bc.BthSniff_p.sniff_attempt = sniff_attempt;
    bc.BthSniff_p.sniff_timeout = sniff_timeout;

    int iRes = ((DeviceIoControl (hDev, BT_IOCTL_BthEnterSniffMode, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());

    if ((iRes == ERROR_SUCCESS) && pinterval)
        *pinterval = bc.BthSniff_p.interval;

    return iRes;
}


int BthExitSniffMode
(
BT_ADDR            *pbt
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthSniff_p.ba = *pbt;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthExitSniffMode, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthEnterParkMode
(
BT_ADDR            *pbt,
unsigned short beacon_max,
unsigned short beacon_min,
unsigned short    *pinterval
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthPark_p.ba = *pbt;
    bc.BthPark_p.beacon_max = beacon_max;
    bc.BthPark_p.beacon_min = beacon_min;

    int  iRes = ((DeviceIoControl (hDev, BT_IOCTL_BthEnterParkMode, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());

    if ((iRes == ERROR_SUCCESS) && pinterval)
        *pinterval = bc.BthPark_p.interval;

    return iRes;
}

int BthExitParkMode
(
BT_ADDR            *pba
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthPark_p.ba = *pba;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthExitParkMode, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}


int BthWritePageTimeout
(
unsigned short    timeout
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthPageTimeout_p.timeout = timeout;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthWritePageTimeout, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthReadPageTimeout
(
unsigned short    *ptimeout
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    if (DeviceIoControl (hDev, BT_IOCTL_BthReadPageTimeout, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) {
        *ptimeout = bc.BthPageTimeout_p.timeout;
        return ERROR_SUCCESS;
    }

    return GetLastError ();
}

int BthWriteCOD
(
unsigned int    cod
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthCOD_p.cod = cod;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthWriteCOD, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthReadCOD
(
unsigned int    *pcod
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    if (DeviceIoControl (hDev, BT_IOCTL_BthReadCOD, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) {
        *pcod = bc.BthCOD_p.cod;
        return ERROR_SUCCESS;
    }

    return GetLastError ();
}

int BthGetRemoteCOD
(
BT_ADDR* pbt,
unsigned int* pcod
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthRemoteCOD_p.ba = *pbt;
    
    if (DeviceIoControl (hDev, BT_IOCTL_BthGetRemoteCOD, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) {
        *pcod = bc.BthRemoteCOD_p.cod;
        return ERROR_SUCCESS;
    }

    return GetLastError ();
}

int BthReadLocalVersion
(
unsigned char    *phci_version,
unsigned short    *phci_revision,
unsigned char    *plmp_version,
unsigned short    *plmp_subversion,
unsigned short    *pmanufacturer,
unsigned char    *plmp_features
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    if (DeviceIoControl (hDev, BT_IOCTL_BthReadLocalVersion, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) {
        *phci_version = bc.BthReadVersion_p.hci_version;
        *phci_revision = bc.BthReadVersion_p.hci_revision;
        *plmp_version = bc.BthReadVersion_p.lmp_version;
        *plmp_subversion = bc.BthReadVersion_p.lmp_subversion;
        *pmanufacturer = bc.BthReadVersion_p.manufacturer_name;
        memcpy (plmp_features, bc.BthReadVersion_p.lmp_features, 8);

        return ERROR_SUCCESS;
    }

    return GetLastError ();
}

int BthReadRemoteVersion
(
BT_ADDR            *pba,
unsigned char    *plmp_version,
unsigned short    *plmp_subversion,
unsigned short    *pmanufacturer,
unsigned char    *plmp_features
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthReadVersion_p.bt = *pba;

    if (DeviceIoControl (hDev, BT_IOCTL_BthReadRemoteVersion, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) {
        *plmp_version = bc.BthReadVersion_p.lmp_version;
        *plmp_subversion = bc.BthReadVersion_p.lmp_subversion;
        *pmanufacturer = bc.BthReadVersion_p.manufacturer_name;
        memcpy (plmp_features, bc.BthReadVersion_p.lmp_features, 8);

        return ERROR_SUCCESS;
    }

    return GetLastError ();
}


int BthPerformInquiry
(
unsigned int    LAP,
unsigned char    length,
unsigned char    num_responses,
unsigned int    cBuffer,
unsigned int    *pcDiscoveredDevices,
BthInquiryResult*inquiry_list
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthPerformInquiry_p.LAP = LAP;
    bc.BthPerformInquiry_p.length = length;
    bc.BthPerformInquiry_p.num_responses = num_responses;
    bc.BthPerformInquiry_p.cBuffer = cBuffer;
    bc.BthPerformInquiry_p.pcDiscoveredDevices = pcDiscoveredDevices;
    bc.BthPerformInquiry_p.inquiry_list = inquiry_list;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthPerformInquiry, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthCancelInquiry
(
void
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    return ((DeviceIoControl (hDev, BT_IOCTL_BthCancelInquiry, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthTerminateIdleConnections
(
void
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    return ((DeviceIoControl (hDev, BT_IOCTL_BthTerminateIdleConnections, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthRemoteNameQuery
(
BT_ADDR            *pba,
unsigned int    cBuffer,
unsigned int    *pcRequired,
WCHAR            *szString
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthRemoteNameQuery_p.ba = *pba;
    bc.BthRemoteNameQuery_p.cBuffer = cBuffer;
    bc.BthRemoteNameQuery_p.pcRequired = pcRequired;
    bc.BthRemoteNameQuery_p.pszString = szString;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthRemoteNameQuery, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthReadLocalAddr
(
BT_ADDR    *pba
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    int iErr = DeviceIoControl (hDev, BT_IOCTL_BthReadLocalAddr, &bc, sizeof(bc), NULL, NULL, NULL, NULL);
    if (iErr)
        *pba = bc.BthReadLocalAddr_p.ba;

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthGetHardwareStatus
(
int    *pStatus
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    int iErr = DeviceIoControl (hDev, BT_IOCTL_BthGetHardwareStatus, &bc, sizeof(bc), NULL, NULL, NULL, NULL);
    if (iErr)
        *pStatus = bc.BthGetHardwareStatus_p.iHwStatus;

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthSetPIN
(
BT_ADDR            *pba,
int                cPinLength,
unsigned char    *ppin
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    if ((cPinLength > 16) || (cPinLength < 1))
        return ERROR_INVALID_PARAMETER;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));
    bc.BthSecurity_p.ba = *pba;
    bc.BthSecurity_p.cDataLength = cPinLength;
    memcpy (bc.BthSecurity_p.data, ppin, cPinLength);

    int iErr = DeviceIoControl (hDev, BT_IOCTL_BthSetPIN, &bc, sizeof(bc), NULL, NULL, NULL, NULL);

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthSetLinkKey
(
BT_ADDR            *pba,
unsigned char    *pkey
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));
    bc.BthSecurity_p.ba = *pba;
    memcpy (bc.BthSecurity_p.data, pkey, 16);

    int iErr = DeviceIoControl (hDev, BT_IOCTL_BthSetLinkKey, &bc, sizeof(bc), NULL, NULL, NULL, NULL);

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthGetLinkKey
(
BT_ADDR            *pba,
unsigned char    *pkey
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));
    bc.BthSecurity_p.ba = *pba;

    int iErr = DeviceIoControl (hDev, BT_IOCTL_BthGetLinkKey, &bc, sizeof(bc), NULL, NULL, NULL, NULL);

    if (iErr)
        memcpy (pkey, bc.BthSecurity_p.data, 16);

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthRevokePIN
(
BT_ADDR            *pba
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));
    bc.BthSecurity_p.ba = *pba;

    int iErr = DeviceIoControl (hDev, BT_IOCTL_BthRevokePIN, &bc, sizeof(bc), NULL, NULL, NULL, NULL);

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthRevokeLinkKey
(
BT_ADDR            *pba
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));
    bc.BthSecurity_p.ba = *pba;

    int iErr = DeviceIoControl (hDev, BT_IOCTL_BthRevokeLinkKey, &bc, sizeof(bc), NULL, NULL, NULL, NULL);

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthAuthenticate
(
BT_ADDR            *pba
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));
    bc.BthAuthenticate_p.ba = *pba;

    int iErr = DeviceIoControl (hDev, BT_IOCTL_BthAuthenticate, &bc, sizeof(bc), NULL, NULL, NULL, NULL);

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthSetEncryption
(
BT_ADDR            *pba,
int                fOn
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));
    bc.BthSetEncryption_p.ba = *pba;
    bc.BthSetEncryption_p.fOn = fOn;

    int iErr = DeviceIoControl (hDev, BT_IOCTL_BthSetEncryption, &bc, sizeof(bc), NULL, NULL, NULL, NULL);

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthGetPINRequest
(
BT_ADDR    *pba
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    int iErr = DeviceIoControl (hDev, BT_IOCTL_BthGetPINRequest, &bc, sizeof(bc), NULL, NULL, NULL, NULL);
    if (iErr)
        *pba = bc.BthGetPINRequest_p.ba;

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthRefusePINRequest
(
BT_ADDR            *pba
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));
    bc.BthRefusePINRequest_p.ba = *pba;

    int iErr = DeviceIoControl (hDev, BT_IOCTL_BthRefusePINRequest, &bc, sizeof(bc), NULL, NULL, NULL, NULL);

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthAnswerPairRequest 
(
BT_ADDR *pba,
int cPinLength,
unsigned char *ppin
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    if ((cPinLength > 16) || (cPinLength < 1))
        return ERROR_INVALID_PARAMETER;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));
    bc.BthSecurity_p.ba = *pba;
    bc.BthSecurity_p.cDataLength = cPinLength;
    memcpy (bc.BthSecurity_p.data, ppin, cPinLength);

    int iErr = DeviceIoControl (hDev, BT_IOCTL_BthAnswerPairRequest, &bc, sizeof(bc), NULL, NULL, NULL, NULL);

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthPairRequest 
(
BT_ADDR *pba,
int cPinLength,
unsigned char *ppin
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    if ((cPinLength > 16) || (cPinLength < 1))
        return ERROR_INVALID_PARAMETER;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));
    bc.BthSecurity_p.ba = *pba;
    bc.BthSecurity_p.cDataLength = cPinLength;
    memcpy (bc.BthSecurity_p.data, ppin, cPinLength);

    int iErr = DeviceIoControl (hDev, BT_IOCTL_BthPairRequest, &bc, sizeof(bc), NULL, NULL, NULL, NULL);

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthSetSecurityUI
(
HANDLE        hEvent,
DWORD        dwStoreTimeout,
DWORD        dwProcTimeout
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));
    bc.BthSetSecurityUI_p.hEvent = hEvent;
    bc.BthSetSecurityUI_p.dwStoreTimeout = dwStoreTimeout;
    bc.BthSetSecurityUI_p.dwProcessTimeout = dwProcTimeout;

    int iErr = DeviceIoControl (hDev, BT_IOCTL_BthSetSecurityUI, &bc, sizeof(bc), NULL, NULL, NULL, NULL);

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}


int BthNsSetService(
LPWSAQUERYSET pSet, 
WSAESETSERVICEOP op, 
DWORD dwFlags
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthNsSetService_p.pSet    = pSet;
    bc.BthNsSetService_p.op      = op;
    bc.BthNsSetService_p.dwFlags = dwFlags;

    return ((DeviceIoControl(hDev, BT_IOCTL_BthNsSetService,&bc,sizeof(bc),NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : SOCKET_ERROR);
}

int BthNsLookupServiceBegin(
LPWSAQUERYSET pQuerySet, 
DWORD dwFlags, 
LPHANDLE lphLookup
)  {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthNsLookupServiceBegin_p.pQuerySet = pQuerySet;
    bc.BthNsLookupServiceBegin_p.dwFlags   = dwFlags;

    int iErr = DeviceIoControl(hDev, BT_IOCTL_BthNsLookupServiceBegin,&bc,sizeof(bc),NULL, NULL, NULL, NULL);
    if (iErr)
        *lphLookup = bc.BthNsLookupServiceBegin_p.hLookup;

    return ((iErr) ? ERROR_SUCCESS : SOCKET_ERROR);
}

int BthNsLookupServiceNext(
HANDLE hLookup, 
DWORD dwFlags, 
LPDWORD lpdwBufferLength, 
LPWSAQUERYSET pResults
)  {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthNsLookupServiceNext_p.hLookup          = hLookup;
    bc.BthNsLookupServiceNext_p.dwFlags          = dwFlags;
    bc.BthNsLookupServiceNext_p.dwBufferLength   = *lpdwBufferLength;
    bc.BthNsLookupServiceNext_p.pResults         = pResults;

    int iErr = DeviceIoControl(hDev, BT_IOCTL_BthNsLookupServiceNext,&bc,sizeof(bc),NULL, NULL, NULL, NULL);
    *lpdwBufferLength = bc.BthNsLookupServiceNext_p.dwBufferLength;

    return ((iErr) ? ERROR_SUCCESS : SOCKET_ERROR);
}

int BthNsLookupServiceEnd(
HANDLE hLookup
)  {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthNsLookupServiceEnd_p.hLookup = hLookup;
    return ((DeviceIoControl(hDev, BT_IOCTL_BthNsLookupServiceEnd,&bc,sizeof(bc),NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : SOCKET_ERROR);
}

int BthSetInquiryFilter
(
BT_ADDR            *pba
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthSetInquiry_p.ba = *pba;
    return ((DeviceIoControl(hDev, BT_IOCTL_BthSetInquiryFilter,&bc,sizeof(bc),NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthSetCODInquiryFilter
(
unsigned int cod,
unsigned int codMask
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthSetCODInquiry_p.cod = cod;
    bc.BthSetCODInquiry_p.codMask = codMask;
    return ((DeviceIoControl(hDev, BT_IOCTL_BthSetCODInquiryFilter,&bc,sizeof(bc),NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthClearInquiryFilter
(
void
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    return ((DeviceIoControl(hDev, BT_IOCTL_BthClearInquiryFilter,&bc,sizeof(bc),NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthSwitchRole
(
BT_ADDR* pbt,     
USHORT usRole     
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthSwitchRole_p.ba = *pbt;
    bc.BthSwitchRole_p.usRole = usRole;

    return ((DeviceIoControl(hDev, BT_IOCTL_BthSwitchRole,&bc,sizeof(bc),NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthGetRole
(
BT_ADDR* pbt,     
USHORT* pusRole
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthGetRole_p.ba = *pbt;

    int iErr = DeviceIoControl(hDev, BT_IOCTL_BthGetRole,&bc,sizeof(bc),NULL, NULL, NULL, NULL);
    if (iErr)
        *pusRole = bc.BthGetRole_p.usRole;

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthReadRSSI
(
BT_ADDR* pbt, 
BYTE* pbRSSI
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthReadRSSI_p.ba = *pbt;

    int iErr = DeviceIoControl(hDev, BT_IOCTL_BthReadRSSI,&bc,sizeof(bc),NULL, NULL, NULL, NULL);
    if (iErr)
        *pbRSSI = bc.BthReadRSSI_p.iRSSI;

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthCreateACLConnection
(
BT_ADDR            *pbt,
unsigned short    *phandle
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthCreateConnection_p.ba = *pbt;

    int iRes = ((DeviceIoControl (hDev, BT_IOCTL_BthCreateACLConnection, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
    if (iRes == ERROR_SUCCESS)
        *phandle = bc.BthCreateConnection_p.handle;

    return iRes;
}

int BthCreateSCOConnection
(
BT_ADDR            *pbt,
unsigned short    *phandle
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthCreateConnection_p.ba = *pbt;

    int iRes = ((DeviceIoControl (hDev, BT_IOCTL_BthCreateSCOConnection, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
    if (iRes == ERROR_SUCCESS)
        *phandle = bc.BthCreateConnection_p.handle;

    return iRes;
}

int BthCloseConnection
(
unsigned short    handle
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthCloseConnection_p.handle = handle;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthCloseConnection, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthAcceptSCOConnections
(
BOOL fAccept
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthAcceptSCOConnections_p.fAccept = fAccept;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthAcceptSCOConnections, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthActivatePAN
(
BOOL fActivate
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthActivatePAN_p.fActivate = fActivate;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthActivatePAN, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());       
}

int BthWritePageScanActivity
(
unsigned short pageScanInterval,
unsigned short pageScanWindow
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthScanActivity_p.scanInterval = pageScanInterval;
    bc.BthScanActivity_p.scanWindow = pageScanWindow;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthWritePageScanActivity, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthWriteInquiryScanActivity
(
unsigned short inquiryScanInterval,
unsigned short inquiryScanWindow
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthScanActivity_p.scanInterval = inquiryScanInterval;
    bc.BthScanActivity_p.scanWindow = inquiryScanWindow;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthWriteInquiryScanActivity, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthReadPageScanActivity
(
unsigned short* pPageScanInterval,
unsigned short* pPageScanWindow
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    int iErr = DeviceIoControl(hDev, BT_IOCTL_BthReadPageScanActivity,&bc,sizeof(bc),NULL, NULL, NULL, NULL);

    if (iErr) {
        *pPageScanInterval = bc.BthScanActivity_p.scanInterval;
        *pPageScanWindow = bc.BthScanActivity_p.scanWindow;
    }

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthReadInquiryScanActivity
(
unsigned short* pInquiryScanInterval,
unsigned short* pInquiryScanWindow
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    int iErr = DeviceIoControl(hDev, BT_IOCTL_BthReadInquiryScanActivity,&bc,sizeof(bc),NULL, NULL, NULL, NULL);

    if (iErr) {
        *pInquiryScanInterval = bc.BthScanActivity_p.scanInterval;
        *pInquiryScanWindow = bc.BthScanActivity_p.scanWindow;
    }

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthWritePageScanType
(
unsigned char pageScanType
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthScanType_p.scanType = pageScanType;    

    return ((DeviceIoControl (hDev, BT_IOCTL_BthWritePageScanType, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthWriteInquiryScanType
(
unsigned char inquiryScanType
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthScanType_p.scanType = inquiryScanType;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthWriteInquiryScanType, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}

int BthReadPageScanType
(
unsigned char* pPageScanType
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    int iErr = DeviceIoControl(hDev, BT_IOCTL_BthReadPageScanType,&bc,sizeof(bc),NULL, NULL, NULL, NULL);

    if (iErr) {
        *pPageScanType = bc.BthScanType_p.scanType;
    }

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthReadInquiryScanType
(
unsigned char* pInquiryScanType
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    int iErr = DeviceIoControl(hDev, BT_IOCTL_BthReadInquiryScanType,&bc,sizeof(bc),NULL, NULL, NULL, NULL);

    if (iErr) {
        *pInquiryScanType = bc.BthScanType_p.scanType;
    }

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthCreateSynchronousConnection
(
BT_ADDR         *pbt,
unsigned short  *pHandle,
unsigned int    txBandwidth,
unsigned int    rxBandwidth,
unsigned short  maxLatency,
unsigned short  voiceSetting,
unsigned char   retransmit
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));
    
    bc.BthCreateSynchronousConnection_p.ba = *pbt;
    bc.BthCreateSynchronousConnection_p.txBandwidth = txBandwidth;
    bc.BthCreateSynchronousConnection_p.rxBandwidth = rxBandwidth;
    bc.BthCreateSynchronousConnection_p.maxLatency = maxLatency;
    bc.BthCreateSynchronousConnection_p.voiceSetting = voiceSetting;
    bc.BthCreateSynchronousConnection_p.retransmit = retransmit;

    int iErr = DeviceIoControl(hDev, BT_IOCTL_BthCreateSynchronousConnection,&bc,sizeof(bc),NULL, NULL, NULL, NULL);

    if (iErr) {
        *pHandle = bc.BthCreateSynchronousConnection_p.handle;
    }

    return ((iErr) ? ERROR_SUCCESS : GetLastError());
}

int BthAcceptSynchronousConnections
(
BOOL fAccept
) {
    if (! Initialize())
        return ERROR_SERVICE_NOT_ACTIVE;

    BTAPICALL bc;
    memset (&bc, 0, sizeof(bc));

    bc.BthAcceptSynchronousConnections_p.fAccept = fAccept;

    return ((DeviceIoControl (hDev, BT_IOCTL_BthAcceptSynchronousConnections, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) ? ERROR_SUCCESS : GetLastError());
}


extern "C" 
#ifdef UNDER_CE
BOOL WINAPI BthApiDllMain(HANDLE hInstance, DWORD dwReason, LPVOID lpReserved);
#else
BOOL WINAPI BthApiDllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
#endif

STDAPI BthApiDllCanUnloadNow(void);
STDAPI BthApiDllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv);
STDAPI BthApiDllRegisterServer(void);
STDAPI BthApiDllUnregisterServer(void);

#ifdef UNDER_CE
BOOL WINAPI DllMain(HANDLE hInstance, DWORD dwReason, LPVOID lpReserved)
#else
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
#endif
{
    BOOL fRet = TRUE;
    
    if (g_fBthApiCOM)
        fRet = BthApiDllMain(hInstance,dwReason,lpReserved);

    if (hDev && (DLL_PROCESS_DETACH == dwReason)) {
        CloseHandle(hDev);        
        hDev = NULL;
    }

    return fRet;
}

STDAPI DllCanUnloadNow(void) {
    if (g_fBthApiCOM)
        return BthApiDllCanUnloadNow();

    // If we're not running with sdpuser COM component included, we should never be here.
    SVSUTIL_ASSERT(0);
    return S_OK;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (g_fBthApiCOM)
        return BthApiDllGetClassObject(rclsid, riid,ppv);

    return FALSE;
}

STDAPI DllRegisterServer(void) {
    if (g_fBthApiCOM)
        return BthApiDllRegisterServer();

    return FALSE;
}

STDAPI DllUnregisterServer(void) {
    if (g_fBthApiCOM)
        return BthApiDllUnregisterServer();

    return FALSE;
}

