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
/*
 * ntp.h
 */
#ifndef __TUX_NTP_H__
#define __TUX_NTP_H__

/* max dns name length */
#if ! defined(DNS_MAX_NAME_BUFFER_LENGTH)
/* Length taken from help on getnameinfo */
#define DNS_MAX_NAME_BUFFER_LENGTH      (1025)
#endif

/* 
 * allowed max round trip delay between NTP request/response, in milliseconds 
 * the delay should less than 500ms in ordinary network traffic
 */ 
#define NTP_MAX_ROUND_TRIP_DELAY        (500) 

#define SECOND_DIV_100NS                (10000000)

#define MS_DIV_100NS                    (10000)


struct NtpTimeRecord
{
    FILETIME clSendTime;
    FILETIME svRecvTime;
    FILETIME svSendTime;
    FILETIME clRecvTime;
};

struct GtcTimeRecord
{
    DWORD gtcSendTime;
    DWORD gtcRecvTime;
};

/*
 * Translate an ip address.
 *
 * The inbound parameter is a string.  This value can either be a dns
 * name or a doted ip address.  The outbound paramter is the ip
 * address in host order.
 *
 * Currently only ipv4 is supported.  output value is 32 bits.
 *
 * Null for either parameter is not supported and will generate an error.
 *
 * On success the function returns true.  False on failure.
 */
BOOL
GetHostIPAddress (TCHAR * pszServer, ULONG *ulIPaddrHostOrder);

/*
 * Get and check the time stamps from NTP/SNTP server
 *
 * If pTimeRect != NULL, the time stamps will be returned.
 * Time stamps are stored in a NtpTimeRecord structure.
 * Return TRUE on success; Otherwise, return FALSE. 
 */
BOOL 
NtpGetValidTimeStamp (ULONG serverIP, NtpTimeRecord *pTimeRec, GtcTimeRecord *pGtcRec);


#endif
