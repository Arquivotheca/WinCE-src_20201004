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

#if ! defined (__bthhid_H__)
#define __bthhid_H__        1

enum E_BTHID_TRANSACTION_TYPES 
{
    BTHID_HANDSHAKE, 
    BTHID_HID_CONTROL, 
    BTHID_GET_REPORT = 4, 
    BTHID_SET_REPORT, 
    BTHID_GET_PROTOCOL, 
    BTHID_SET_PROTOCOL, 
    BTHID_GET_IDLE, 
    BTHID_SET_IDLE, 
    BTHID_DATA, 
    BTHID_DATC
};

enum E_BTHID_HANDSHAKE_RESULT
{
    BTHID_HANDSHAKE_SUCCESSFUL = 0,
    BTHID_HANDSHAKE_NOT_READY,
    BTHID_HANDSHAKE_ERR_INVALID_REPORT_ID,
    BTHID_HANDSHAKE_ERR_UNSUPPORTED_REQUEST,
    BTHID_HANDSHAKE_ERR_INVALID_PARAMETER,
    BTHID_HANDSHAKE_ERR_UNKNOWN = 14,
    BTHID_HANDSHAKE_ERR_FATAL
};

enum E_BTHID_CONTROL_OPERATION
{
    BTHID_CONTROL_NOP = 0,
    BTHID_CONTROL_HARD_RESET,
    BTHID_CONTROL_SOFT_RESET,
    BTHID_CONTROL_SUSPEND,
    BTHID_CONTROL_EXIT_SUSPEND,
    BTHID_CONTROL_VIRTUAL_CABLE_UNPLUG
};

enum E_BTHID_REPORT_TYPES
{
    BTHID_REPORT_OTHER = 0,
    BTHID_REPORT_INPUT,
    BTHID_REPORT_OUTPUT,
    BTHID_REPORT_FEATURE
};

enum E_BTHID_PROTOCOLS
{
    BTHID_BOOT_PROTOCOL = 0,
    BTHID_REPORT_PROTOCOL
};

union BTHID_Header_Parameter
{
    struct S_BTHID_Handshake_Packet
    {
        unsigned char bResultCode       : 4;
    } handshake_p;

    struct S_BTHID_Control_Packet
    {
        unsigned char bControlOp        : 4;
    } control_p;

    struct S_BTHID_DATA_Header
    {
        unsigned char bReportType       : 2;
        unsigned char                   : 2;
    } data_p;

    struct S_BTHID_DATC_Header
    {
        unsigned char bReportType       : 2;
        unsigned char                   : 2;
    } datc_p;

    struct S_BTHIDSetProtocol_Request
    {
        unsigned char bProtocol         : 1;
        unsigned char                   : 3;
    } setprotocol_p;

    struct S_BTHIDGetReport_Request
    {
        unsigned char bReportType       : 2;
        unsigned char                   : 1; 
        unsigned char bSize             : 1;
    } getreport_p;

    struct S_BTHIDSetReport_Request
    {
        unsigned char bReportType       : 2;
        unsigned char                   : 2; 
    } setreport_p;

    struct S_BTHIDSetIdle_Request
    {
        unsigned char                   : 4;
    } setidle_p;

    unsigned char bRawHeader;
};

class IBTHHIDReportHandler
{
public:
    virtual int  SetIdle(unsigned char bIdle) = 0;
    virtual int  SetProtocol(E_BTHID_PROTOCOLS protocol) = 0;
    virtual int  GetReport(int iReportID, E_BTHID_REPORT_TYPES type, PCHAR pBuffer, int cbBuffer, PDWORD pcbTransfered, int iTimeout) = 0;
    virtual int  SetReport(E_BTHID_REPORT_TYPES type, PCHAR pBuffer, int cbBuffer, int iTimeout, BYTE bReportID) = 0;
};


#endif

