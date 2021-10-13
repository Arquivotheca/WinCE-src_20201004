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
#ifndef __NDT_ERROR_H
#define __NDT_ERROR_H

//------------------------------------------------------------------------------

// This is strict check for sucess, while FAILED macro allows status like Pending
#define NOT_SUCCEEDED(hr) ( S_OK != hr)

//------------------------------------------------------------------------------

#define FACILITY_NDIS_BIT     0x10000000
#define HRESULT_FROM_NDIS(x)  ((x==0)?0:(HRESULT)((x)|FACILITY_NDIS_BIT))
#define NDIS_FROM_HRESULT(x)  ((x==0)?0:(NDIS_STATUS)((x)&~FACILITY_NDIS_BIT))

//------------------------------------------------------------------------------
// Following NDT_STATUS values are HRESULT equivalents of NDIS_STATUS values
// 

#define NDT_STATUS_SUCCESS                   ((HRESULT)0x00000000L)
#define NDT_STATUS_PENDING                   ((HRESULT)0x10000103L)
#define NDT_STATUS_NOT_RECOGNIZED            ((HRESULT)0x10010001L)
#define NDT_STATUS_NOT_COPIED                ((HRESULT)0x10010002L)
#define NDT_STATUS_NOT_ACCEPTED              ((HRESULT)0x10010003L)
#define NDT_STATUS_CALL_ACTIVE               ((HRESULT)0x10010007L)

#define NDT_STATUS_ONLINE                    ((HRESULT)0x50010003L)
#define NDT_STATUS_RESET_START               ((HRESULT)0x50010004L)
#define NDT_STATUS_RESET_END                 ((HRESULT)0x50010005L)
#define NDT_STATUS_RING_STATUS               ((HRESULT)0x50010006L)
#define NDT_STATUS_CLOSED                    ((HRESULT)0x50010007L)
#define NDT_STATUS_WAN_LINE_UP               ((HRESULT)0x50010008L)
#define NDT_STATUS_WAN_LINE_DOWN             ((HRESULT)0x50010009L)
#define NDT_STATUS_WAN_FRAGMENT              ((HRESULT)0x5001000AL)
#define NDT_STATUS_MEDIA_CONNECT             ((HRESULT)0x5001000BL)
#define NDT_STATUS_MEDIA_DISCONNECT          ((HRESULT)0x5001000CL)
#define NDT_STATUS_HARDWARE_LINE_UP          ((HRESULT)0x5001000DL)
#define NDT_STATUS_HARDWARE_LINE_DOWN        ((HRESULT)0x5001000EL)
#define NDT_STATUS_INTERFACE_UP              ((HRESULT)0x5001000FL)
#define NDT_STATUS_INTERFACE_DOWN            ((HRESULT)0x50010010L)
#define NDT_STATUS_MEDIA_BUSY                ((HRESULT)0x50010011L)
#define NDT_STATUS_MEDIA_SPECIFIC_INDICATION ((HRESULT)0x50010012L)
#define NDT_STATUS_WW_INDICATION             ((HRESULT)0x50010012L)
#define NDT_STATUS_LINK_SPEED_CHANGE         ((HRESULT)0x50010013L)
#define NDT_STATUS_WAN_GET_STATS             ((HRESULT)0x50010014L)
#define NDT_STATUS_WAN_CO_FRAGMENT           ((HRESULT)0x50010015L)
#define NDT_STATUS_WAN_CO_LINKPARAMS         ((HRESULT)0x50010016L)

#define NDT_STATUS_NOT_RESETTABLE            ((HRESULT)0x90010001L)
#define NDT_STATUS_SOFT_ERRORS               ((HRESULT)0x90010003L)
#define NDT_STATUS_HARD_ERRORS               ((HRESULT)0x90010004L)
#define NDT_STATUS_BUFFER_OVERFLOW           ((HRESULT)0x90000005L)

#define NDT_STATUS_FAILURE                   ((HRESULT)0xD0000001L)
#define NDT_STATUS_RESOURCES                 ((HRESULT)0xD000009AL)
#define NDT_STATUS_CLOSING                   ((HRESULT)0xD0010002L)
#define NDT_STATUS_BAD_VERSION               ((HRESULT)0xD0010004L)
#define NDT_STATUS_BAD_CHARACTERISTICS       ((HRESULT)0xD0010005L)
#define NDT_STATUS_ADAPTER_NOT_FOUND         ((HRESULT)0xD0010006L)
#define NDT_STATUS_OPEN_FAILED               ((HRESULT)0xD0010007L)
#define NDT_STATUS_DEVICE_FAILED             ((HRESULT)0xD0010008L)
#define NDT_STATUS_MULTICAST_FULL            ((HRESULT)0xD0010009L)
#define NDT_STATUS_MULTICAST_EXISTS          ((HRESULT)0xD001000AL)
#define NDT_STATUS_MULTICAST_NOT_FOUND       ((HRESULT)0xD001000BL)
#define NDT_STATUS_REQUEST_ABORTED           ((HRESULT)0xD001000CL)
#define NDT_STATUS_RESET_IN_PROGRESS         ((HRESULT)0xD001000DL)
#define NDT_STATUS_CLOSING_INDICATING        ((HRESULT)0xD001000EL)
#define NDT_STATUS_NOT_SUPPORTED             ((HRESULT)0xD00000BBL)
#define NDT_STATUS_INVALID_PACKET            ((HRESULT)0xD001000FL)
#define NDT_STATUS_OPEN_LIST_FULL            ((HRESULT)0xD0010010L)
#define NDT_STATUS_ADAPTER_NOT_READY         ((HRESULT)0xD0010011L)
#define NDT_STATUS_ADAPTER_NOT_OPEN          ((HRESULT)0xD0010012L)
#define NDT_STATUS_NOT_INDICATING            ((HRESULT)0xD0010013L)
#define NDT_STATUS_INVALID_LENGTH            ((HRESULT)0xD0010014L)
#define NDT_STATUS_INVALID_DATA              ((HRESULT)0xD0010015L)
#define NDT_STATUS_BUFFER_TOO_SHORT          ((HRESULT)0xD0010016L)
#define NDT_STATUS_INVALID_OID               ((HRESULT)0xD0010017L)
#define NDT_STATUS_ADAPTER_REMOVED           ((HRESULT)0xD0010018L)
#define NDT_STATUS_UNSUPPORTED_MEDIA         ((HRESULT)0xD0010019L)
#define NDT_STATUS_GROUP_ADDRESS_IN_USE      ((HRESULT)0xD001001AL)
#define NDT_STATUS_FILE_NOT_FOUND            ((HRESULT)0xD001001BL)
#define NDT_STATUS_ERROR_READING_FILE        ((HRESULT)0xD001001CL)
#define NDT_STATUS_ALREADY_MAPPED            ((HRESULT)0xD001001DL)
#define NDT_STATUS_RESOURCE_CONFLICT         ((HRESULT)0xD001001EL)
#define NDT_STATUS_NO_CABLE                  ((HRESULT)0xD001001FL)

#define NDT_STATUS_INVALID_SAP               ((HRESULT)0xD0010020L)
#define NDT_STATUS_SAP_IN_USE                ((HRESULT)0xD0010021L)
#define NDT_STATUS_INVALID_ADDRESS           ((HRESULT)0xD0010022L)
#define NDT_STATUS_VC_NOT_ACTIVATED          ((HRESULT)0xD0010023L)
#define NDT_STATUS_DEST_OUT_OF_ORDER         ((HRESULT)0xD0010024L)
#define NDT_STATUS_VC_NOT_AVAILABLE          ((HRESULT)0xD0010025L)
#define NDT_STATUS_CELLRATE_NOT_AVAILABLE    ((HRESULT)0xD0010026L)
#define NDT_STATUS_INCOMPATABLE_QOS          ((HRESULT)0xD0010027L)
#define NDT_STATUS_AAL_PARAMS_UNSUPPORTED    ((HRESULT)0xD0010028L)
#define NDT_STATUS_NO_ROUTE_TO_DESTINATION   ((HRESULT)0xD0010029L)

#define NDT_STATUS_TOKEN_RING_OPEN_ERROR     ((HRESULT)0xD0011000L)
#define NDT_STATUS_INVALID_DEVICE_REQUEST    ((HRESULT)0xD0000010L)
#define NDT_STATUS_NETWORK_UNREACHABLE       ((HRESULT)0xD000023CL)

//------------------------------------------------------------------------------

#define NDT_STATUS_DEVICEIOCONTROL_FAILED    ((HRESULT)0xC0000001L)
#define NDT_STATUS_INVALID_STATE             ((HRESULT)0xC0000002L)
#define NDT_STATUS_SYNC_REQUEST_TIMEOUT      ((HRESULT)0xC0000003L)

//------------------------------------------------------------------------------
// Added for WPA
#define NDT_STATUS_ASSOCIATION_FAILED          ((HRESULT)0x00003001)
#define NDT_STATUS_PSK_AUTHENTICATION_FAILED   ((HRESULT)0x00003002)
#define NDT_STATUS_START_AUTHENTICATOR_FAILED  ((HRESULT)0x00003003)

//------------------------------------------------------------------------------

BOOL NotReceivedOK(ULONG ulPacketsReceived, ULONG ulPacketsSent, UINT ulPhysicalMedium, UINT ulPacketType);

//------------------------------------------------------------------------------

#endif

