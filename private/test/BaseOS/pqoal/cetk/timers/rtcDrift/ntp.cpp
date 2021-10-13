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
 * NTP.CPP
 *
 * Get timestamps from NTP server and calculate the clock offset between
 * time server and current machine
 */

#include <winsock2.h>
#include <tchar.h>
#include "ntp.h"

/* the define for Info(), Error(), etc. */
#include "..\..\..\common\commonUtils.h"

/* error code definitions */
#define NTP_ERR_SUCCESS                     (1 << 0)
#define NTP_ERR_FATAL_ERROR                 (1 << 1)
#define NTP_ERR_SOCKET_ERROR                (1 << 2)
#define NTP_ERR_SERVER_NO_RESPONSE          (1 << 3)
#define NTP_ERR_SERVER_INVALID_RESPONSE     (1 << 4)
#define NTP_ERR_SERVER_REJECT_REQUEST       (1 << 5)
#define NTP_ERR_SERVER_NOT_RELIABLE         (1 << 6)
#define NTP_ERR_TOO_MUCH_DELAY              (1 << 7)

#define NTP_PORT                            (123)

/* timeout time for NTP response, in seconds */
#define NTP_TIMEOUT                         (5)

#define NTP_REQUEST_MAX_TRIES               (3)

/* RFC recommends the minimum poll interval to be 15 seconds.*/
#define NTP_POLL_INTERVAL                   (15*1000)


/*
 * NTP timestamp structure
 */
struct NTPTIME
{
    UINT32 integer;     // integer part of the timestamp (in seconds)
    UINT32 fractional;  // fractional part of the timestamp
};

/*
 * NTP message body
 */
struct NtpHead
{
    UINT8   liVnMode;   // Leap Indicator (2bits) | Version Number (3bits) | Mode (3bits)
    UINT8   stratum;
    UINT8   poll;
    INT8    precision;
    INT32   rootDelay;
    UINT32  rootDispersion;
    UINT8   refID[4];   // reference ID
    NTPTIME refTS;      // reference timestamp
    NTPTIME oriTS;      // originate timestamp
    NTPTIME rcvTS;      // receive timestamp
    NTPTIME trsTS;      // transmit timestamp
};


/*
 * Authentication data for NTP protocol, not required for SNTP
 */
struct NtpAuthentication
{
    UINT32  keyID;
    UINT8   msgDigest[16];
};


/*
 * Set Version Number field for NTP message
 */
void
NtpSetVn (UINT8 *lvm, DWORD n)
{
    *lvm = (*lvm & 0xC7) | ((UINT8)(n & 0x7) << 3);
}


/*
 * Set Mode field for NTP message
 */
void
NtpSetMode (UINT8 *lvm, DWORD n)
{
    *lvm = (*lvm & 0xF8) | (UINT8)(n & 0x7);
}


/*
 * Get Leap Id filed
 */
DWORD
NtpGetLi (const UINT8 *lvm)
{
    return ((*lvm & 0xC0) >> 6);
}


/*
 * Get Mode field
 */
DWORD
NtpGetMode (const UINT8 *lvm)
{
    return (*lvm & 0x7);
}


/* Win32 FILETIME use Jan 1, 1600 UTC as start point (0), while NTP use Jan 1, 1900 UTC.
   NTP_FILETIME_OFFSET is the time offset (count with 100-nanosecond intervals) */
const UINT64 NTP_FILETIME_OFFSET = 0x14F373BFDE04000ULL;

/*
 * Calculates the NTPTIME based on Win32 FILETIME
 *
 * Win32 FILETIME values count 100-nanosecond intervals since January 1, 1600 UTC. It is a 64-bit number.
 * NTP timestamps are represented as a 64-bit unsigned fixed-point number, in seconds relative to 0h on 1 January 1900.
 * The integer part is in the first 32 bits, and the fraction part in the last 32 bits.
 * In the fraction part, the non-significant low-order bits are not specified and are ordinarily set to 0.
 */
void
FileTimeToNtpTime (const FILETIME *pFt, NTPTIME *pNt)
{
    ULARGE_INTEGER fileTime = { 0 }, ntpTime = { 0 };

    fileTime.HighPart = pFt->dwHighDateTime;
    fileTime.LowPart  = pFt->dwLowDateTime;

    /*
       NTPTIME = Y, FILETIME = X
       X -= OFFSET
       Y = (X / 10^7 * 2^32) | ((X % 10^7) << 32 / 10^7)
              Integer Part   |     Fractional Part
     */
    fileTime.QuadPart -= NTP_FILETIME_OFFSET;
    ntpTime.QuadPart = (fileTime.QuadPart / SECOND_DIV_100NS) << 32 | ((fileTime.QuadPart % SECOND_DIV_100NS) << 32) / SECOND_DIV_100NS;

    pNt->integer = ntpTime.HighPart;
    pNt->fractional = ntpTime.LowPart;
}


/*
 * Calculate the FILETIME based on NTPTIME
 */
void
NtpTimeToFileTime (const NTPTIME *pNt, FILETIME *pFt)
{
    ULARGE_INTEGER ntpTime = { 0 }, fileTime = { 0 };

    ntpTime.HighPart = pNt->integer;
    ntpTime.LowPart = pNt->fractional;

    /*
        NTPTIME = X, FILETIME = Y
        Y = (X >> 32 * 10^7) + (((X & 2^32) * 10^7)) >> 32)
        Y += OFFSET
     */
    fileTime.QuadPart = ((ntpTime.QuadPart >> 32) * SECOND_DIV_100NS) + (((ntpTime.QuadPart & (UINT32)(~0)) * SECOND_DIV_100NS) >> 32);
    fileTime.QuadPart += NTP_FILETIME_OFFSET;

    pFt->dwHighDateTime = fileTime.HighPart;
    pFt->dwLowDateTime  = fileTime.LowPart;
}


/*
 * Translate NTPTIME to millisecond based value
 */
void
NtpTimeToMs (NTPTIME *pNt, LARGE_INTEGER *pMs)
{
    INT64 high = { 0 };
    INT64 low = { 0 };

    pMs->HighPart = pNt->integer;
    pMs->LowPart = pNt->fractional;
    high = pMs->QuadPart >> 16;
    low  = pMs->QuadPart & 0xFFFF;
    pMs->QuadPart = ((high * 1000) + ((low * 1000) >> 16) ) >> 16;
}


/*
 * Translate SYSTEMTIME to NTPTIME
 *
 * If using hard-RTC, the resolution usually equals second. The precision is a little low.
 * To improve the precision of the system timer, add a millisecond value to it.
 * To skip the millisecond, pass a NULL pointer to pMillisecond
 */
void
SystemTimeToNtpTime (SYSTEMTIME *pSt, const DWORD *pMillisecond, NTPTIME *pNt)
{
    FILETIME ft = { 0 };

    /* translate SYSTEMTIME to FILETIME */
    SystemTimeToFileTime (pSt, &ft);

    if (pMillisecond != NULL) {
        /* add millsecond patch to FILETIME */
        LARGE_INTEGER cnt;
        cnt.HighPart = ft.dwHighDateTime;
        cnt.LowPart = ft.dwLowDateTime;
        cnt.QuadPart += *pMillisecond * MS_DIV_100NS;

        ft.dwHighDateTime = cnt.HighPart;
        ft.dwLowDateTime = cnt.LowPart;
    }

    /* translate FILETIME to NTPTIME */
    FileTimeToNtpTime (&ft, pNt);
}


/*
 * Calculate time offset based on received time server timestamps and local timestamps.
 *
 * The following table summarizes four timestamps.
 *
 *    Timestamp Name          ID   When Generated
 *    ------------------------------------------------------------
 *    Originate Timestamp     T1   time request sent by client
 *    Receive Timestamp       T2   time request received by server
 *    Transmit Timestamp      T3   time reply sent by server
 *    Destination Timestamp   T4   time reply received by client
 *
 * The roundtrip delay d and system clock offset t are defined as:
 *    d = (T4 - T1) - (T3 - T2)     t = ((T2 - T1) + (T3 - T4)) / 2.
 */
void
NtpCalcTimeOffset (
    NTPTIME *pClSend,
    NTPTIME *pSvRecv,
    NTPTIME *pSvSend,
    NTPTIME *pClRecv,
    LARGE_INTEGER *pOffset,
    LARGE_INTEGER *pDelay
    )
{
    /*
     * Can't use ULARGE_INTEGER here, because 't = ((T2 - T1) + (T3 - T4)) / 2',
     * if (T2-T1)+(T3-T4) == (ULONGLONG)-1, then t will be 0x7FFFFFFFFFFFFFFF
     */
    LARGE_INTEGER t1, t2, t3, t4, delay, offset;
    t1.HighPart = pClSend->integer;
    t1.LowPart  = pClSend->fractional;
    t2.HighPart = pSvRecv->integer;
    t2.LowPart  = pSvRecv->fractional;
    t3.HighPart = pSvSend->integer;
    t3.LowPart  = pSvSend->fractional;
    t4.HighPart = pClRecv->integer;
    t4.LowPart  = pClRecv->fractional;

    /* calculate round trip delay and time offset */
    delay.QuadPart = (t4.QuadPart - t1.QuadPart) - (t3.QuadPart - t2.QuadPart);
    offset.QuadPart = ( (t2.QuadPart - t1.QuadPart) + (t3.QuadPart - t4.QuadPart) ) / 2;

    /* translate NTPTIME value to ms-based value */
    NTPTIME nt;
    nt.integer = delay.HighPart;
    nt.fractional = delay.LowPart;
    NtpTimeToMs (&nt, &delay);
    pDelay->QuadPart = delay.QuadPart;

    nt.integer = offset.HighPart;
    nt.fractional = offset.LowPart;
    NtpTimeToMs (&nt, &offset);
    pOffset->QuadPart = offset.QuadPart;

    Info (_T("Get server response. The timestamps is:"));
    Info (_T("T1 second:%08X, fractional:%08X"), t1.HighPart, t1.LowPart);
    Info (_T("T2 second:%08X, fractional:%08X"), t2.HighPart, t2.LowPart);
    Info (_T("T3 second:%08X, fractional:%08X"), t3.HighPart, t3.LowPart);
    Info (_T("T4 second:%08X, fractional:%08X"), t4.HighPart, t4.LowPart);

    Info (_T("Round trip delay is %I64d ms"), delay.QuadPart);
    Info (_T("Offset is %I64d ms"), offset.QuadPart);

    /* blank line */
    Info (_T(""));
}


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
GetHostIPAddress (TCHAR * pszServer, ULONG *ulIPaddrHostOrder)
{
    char * hostname = NULL;
    BOOL returnVal = FALSE;
    size_t dwNumChars = 0;

    /* check in bound params */
    if (!pszServer || !ulIPaddrHostOrder) {
        goto cleanup;
    }

    /* get memory for ansi string */
    hostname = (char *) malloc (DNS_MAX_NAME_BUFFER_LENGTH);
    if (!hostname) {
        Error (_T("Out of memory"));
        goto cleanup;
    }

    /* convert hostname to ansi string */
#ifdef UNICODE
    wcstombs_s( &dwNumChars, hostname, DNS_MAX_NAME_BUFFER_LENGTH, pszServer, sizeof(char)*(_tcslen(pszServer)+1));
#else
    strncpy(hostname, pszServer, _tcslen(pszServer)+1);
#endif

    /* try to lookup in dns */
    hostent * he = gethostbyname (hostname);
    if (he) {
        /* we were successful looking it up in dns */
        *ulIPaddrHostOrder = ntohl(*(ULONG *)he->h_addr_list[0]);
        Info (_T("Found IP address of %s in DNS."), pszServer);
    } else {
        /* couldn't look up in dns.  Might be ip dot notation */
        ULONG inetAddrRval = inet_addr (hostname);

        if (inetAddrRval == INADDR_NONE) {
            /* couldn't find it */
            Error (_T("Couldn't find %s in DNS or convert from dot notation."), pszServer);
            goto cleanup;
        }
        Info (_T("Successfully converted address from dot notation."));
        *ulIPaddrHostOrder = ntohl(inetAddrRval);
    }
    returnVal = TRUE;

cleanup:
    if (hostname) {
        free (hostname);
    }
    return (returnVal);
}


/*
 * Get system time and GTC count at the beginning of a second
 *
 * The hard-RTC implementation usually doesn't give the sub-second count.
 * Using GTC as a high resolution timer to calculate the SNTP round trip time and offset.
 */
void
GetSystemTimeAtMs0 (SYSTEMTIME *pSystemTime, DWORD *pStartMs)
{
    DWORD lastSecond = 0, currSecond = 0;
    BOOL bFirstLoop = TRUE;
    do {
        Sleep (0);
        if (pStartMs != NULL) {
            *pStartMs = GetTickCount ();
        }
        lastSecond = currSecond;
        GetSystemTime (pSystemTime);
        currSecond = pSystemTime->wSecond;
        if (bFirstLoop) {
            /* lastSecond is -1 now, set it to a valid count*/
            lastSecond = currSecond;
            bFirstLoop = FALSE;
        }
        /* looping until RTC changes */
    } while (lastSecond == currSecond);
}


/*
 * sanity check the response packet according to SNTPv4
 */
DWORD
NtpCheckServerResponse (NtpHead *pMsg, NTPTIME *pOriTS)
{
    /* 0 = Kiss-o'-Death packet, client violates the server access control */
    if (pMsg->stratum == 0) {
        Error (_T("Time server reject the SNTP/NTP request"));
        return NTP_ERR_SERVER_REJECT_REQUEST;
    }

    /* check the SNTP response packet header */
    if (NtpGetLi (&pMsg->liVnMode) == 3         // 3 = (alarm condition, clock not synchronized)
        || NtpGetMode (&pMsg->liVnMode) != 4    // 4 = server
                                                // server should copy the trsTS of request packet to oriTS of response packet, if trsTS and oriTS didn't match, discard the packet.
        || (ntohl (pMsg->oriTS.integer) != pOriTS->integer)
        || (ntohl (pMsg->oriTS.fractional) != pOriTS->fractional)
                                                // the trsTS of response packet should NOT be 0
        || (pMsg->trsTS.integer == 0)
        || (pMsg->trsTS.fractional == 0) )
    {
        Error (_T("Time server reply with an invalid SNTP/NTP response"));
        return NTP_ERR_SERVER_INVALID_RESPONSE;
    }

    INT32 rootDelay = ntohl (pMsg->rootDelay);
    UINT32 rootDispersion = ntohl (pMsg->rootDispersion);

    /*
     * rootDelay and rootDispersion are fixed-point number with the fraction point between bits 15 and 16,
     * we care only the integer part, so translates it to integer
     */
    rootDelay >>= 16;
    rootDispersion >>= 16;

    /* if rootDelay or rootDispersion greater than 1 second, the server is not reliable */
    if (rootDelay < 0 || rootDelay > 1 || rootDispersion > 1) {
        Error (_T("Time server is NOT reliable"));
        return NTP_ERR_SERVER_NOT_RELIABLE;
    }
    return NTP_ERR_SUCCESS;
}


/*
 * Common UDP help function that send a UDP request to server and wait the response
 *
 * If successfully sent the request packet,return TRUE; Otherwise, return FALSE.
 * The bytes of received message is stored in *pBytesReceived. If the couldn't receive
 * any response from server, *pBytesReceived == 0.
 */
BOOL
SendUdpRequestAndGetResponse (
    ULONG serverIpAddress,
    WORD  serverUdpPort,
    DWORD maxWaitSeconds,
    __in_bcount(bytesToSend) void  *sendBuf,
    DWORD bytesToSend,
    __out_bcount(*pBytesReceived) void  *recvBuf,
    DWORD *pBytesReceived
    )
{
    SOCKET sock = INVALID_SOCKET;
    BOOL retVal = FALSE;

    /* check in bound parameters */
    if (!sendBuf || !recvBuf || !pBytesReceived) {
        goto cleanup;
    }

    /* create a UDP socket */
    sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        Error (_T("Failed to create a UDP socket"));
        goto cleanup;
    }

    SOCKADDR_IN serverAddr;

    /* init the server address */
    memset (&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.S_un.S_addr = htonl (serverIpAddress);
    serverAddr.sin_port = htons (serverUdpPort);

    Info (_T("Sent request, and awaiting the response"));

    /* send the SNTP request to the server */
    DWORD sendBytes = sendto (sock, (CHAR *) sendBuf, bytesToSend, 0, (sockaddr *) &serverAddr, sizeof(serverAddr));
    if (sendBytes != bytesToSend) {
        Error (_T("Failed to send the request"));
        goto cleanup;
    }

    CHAR *pRead = (CHAR *) recvBuf;
    DWORD bytesToRead = *pBytesReceived;
    INT bytesRead = 0;
    do {
        fd_set fdReadSet;
        FD_ZERO (&fdReadSet);
#pragma warning( suppress: 4127 )
        FD_SET (sock, &fdReadSet);
        timeval timeout;
        timeout.tv_sec = maxWaitSeconds;
        timeout.tv_usec = 0;

        /* waiting for the reply from time server */
        if (select (0, &fdReadSet, NULL, NULL, &timeout) > 0) {
            /* message arrives, read message from the socket */
            INT serverAddrLen = sizeof(serverAddr);
            bytesRead = recvfrom (sock, pRead, bytesToRead, 0, (SOCKADDR *) &serverAddr, &serverAddrLen);

            /* can't get the data of arrived packet */
            if (bytesRead == SOCKET_ERROR) {
                Error (_T("Recvfrom failed to get the response data"));
                goto cleanup;
            }
            if(bytesRead>=0 && bytesToRead >= (DWORD)bytesRead ) {
                bytesToRead -= bytesRead;
                pRead += bytesRead;
            }
            else {
                bytesToRead = 0;
            }
        }
    } while (bytesRead > 0 && bytesToRead > 0);

    *pBytesReceived -= bytesToRead;
    retVal = TRUE;
cleanup:
    /* close the UDP connection */
    if (sock != INVALID_SOCKET) {
        shutdown (sock, SD_BOTH);
        closesocket (sock);
    }
    return retVal;
}


/*
 * Get time stamps from specificed NTP/SNTP time server through SNTP request
 *
 * Passing in the host byte order IP address (serverIpAddr) of the server.
 * Return TRUE if successfully get the response, and time stamps (pTimeRec) and
 * elapsed time (pElapsed) will be calculated and stored. Otherwise return FALSE.
 */
DWORD
NtpGetTimeStamp (
    ULONG serverIpAddr,
    NtpTimeRecord *pTimeRec,
    GtcTimeRecord *pGtcRec,
    DWORD *pElapsed
    )
{
    NtpHead ntpMsg;
    NTPTIME clSendTime, svRecvTime, svSendTime; // SNTP packet send/receive time of Client and Server
    SYSTEMTIME sysStartTime, sysEndTime;
    DWORD dwGtcStartTime, dwGtcEndTime, startMs = 0;

    BOOL retVal = NTP_ERR_SUCCESS;

    /* check in bound parameters */
    if (!pElapsed) {
        retVal = NTP_ERR_FATAL_ERROR;
        goto cleanup;
    }

    memset (&ntpMsg, 0, sizeof(ntpMsg));

    /* set SNTP version, currently is 4 */
    NtpSetVn (&ntpMsg.liVnMode, 4);

    /* set Mode, 'client' == 3 */
    NtpSetMode (&ntpMsg.liVnMode, 3);

    /* get the request packet departing time */
    GetSystemTimeAtMs0 (&sysStartTime, &startMs);
    SystemTimeToNtpTime (&sysStartTime, NULL, &clSendTime);
    dwGtcStartTime = GetTickCount();

    /* fill the transmit timestamp field of SNTP request packet */
    ntpMsg.trsTS.integer = ntohl (clSendTime.integer);
    ntpMsg.trsTS.fractional = ntohl (clSendTime.fractional);

    DWORD receivedBytes = sizeof(ntpMsg);

    /* send a request to the time server, then wait the response packet */
    if (SendUdpRequestAndGetResponse (serverIpAddr, NTP_PORT, NTP_TIMEOUT, &ntpMsg, sizeof(ntpMsg), &ntpMsg, &receivedBytes) == FALSE) {
        /* failed on socket operations */
        retVal = NTP_ERR_SOCKET_ERROR;
        goto cleanup;
    }

    /* get the response packet arriving time */
    GetSystemTime (&sysEndTime);
    *pElapsed = GetTickCount () - startMs;
    dwGtcEndTime = GetTickCount();

    /* didn't receive the response from server */
    if (receivedBytes == 0) {
        Error (_T("Server is not available, or local NATs/proxies don't support UDP traffic, or SNTP packet is filtered by local firewall, etc."));
        Error (_T("Please check the server IP address and local network configuration"));
        retVal = NTP_ERR_SERVER_NO_RESPONSE;
        goto cleanup;
    }

    /* didn't receive all the expected data */
    if (receivedBytes < sizeof(ntpMsg)) {
        Error (_T("Server didn't send back all the expected data"));
        retVal = NTP_ERR_SERVER_INVALID_RESPONSE;
        goto cleanup;
    }

    /* sanity check the response packet */
    retVal = NtpCheckServerResponse (&ntpMsg, &clSendTime);
    if (retVal != NTP_ERR_SUCCESS) {
        goto cleanup;
    }

    /* translate timestamp data from net order to host order */
    svRecvTime.integer = ntohl (ntpMsg.rcvTS.integer);
    svRecvTime.fractional = ntohl (ntpMsg.rcvTS.fractional);
    svSendTime.integer = ntohl (ntpMsg.trsTS.integer);
    svSendTime.fractional = ntohl (ntpMsg.trsTS.fractional);

     /* return the FILETIME based time stamps */
    if (pTimeRec != NULL) {
        SystemTimeToFileTime (&sysStartTime, &pTimeRec->clSendTime);
        SystemTimeToFileTime (&sysEndTime, &pTimeRec->clRecvTime);

        NtpTimeToFileTime (&svRecvTime, &pTimeRec->svRecvTime);
        NtpTimeToFileTime (&svSendTime, &pTimeRec->svSendTime);
    }

    if(pGtcRec != NULL) {
        pGtcRec->gtcSendTime = dwGtcStartTime;
        pGtcRec->gtcRecvTime = dwGtcEndTime;
    }

cleanup:
    return retVal;
}


/*
 * Get and check the time stamps from NTP/SNTP server
 *
 * If pTimeRect != NULL, the time stamps will be returned.
 * Time stamps are stored in a NtpTimeRecord structure.
 * Return TRUE on success; Otherwise, return FALSE.
 */
BOOL
NtpGetValidTimeStamp (ULONG serverIP, NtpTimeRecord *pTimeRec, GtcTimeRecord *pGtcRec)
{
    BOOL bValid = FALSE;
    BOOL bStopTry = FALSE;
    UINT uiRand = 500;

    Info (_T("Time server IP address is %u.%u.%u.%u"),
        (serverIP >> 24) & 0xFF, (serverIP >> 16) & 0xFF, (serverIP >> 8) & 0xFF, serverIP & 0xFF);

    /* set the random seed */
    srand (GetTickCount ());

    /* Re-send the SNTP request if necessary */
    for (DWORD i = 0; !bStopTry && i < NTP_REQUEST_MAX_TRIES; i++) {
        DWORD delay = 0;
        DWORD status = NtpGetTimeStamp (serverIP, pTimeRec, pGtcRec, &delay);
        /* check the round-trip delay */
        if (status == NTP_ERR_SUCCESS &&
            delay >= NTP_MAX_ROUND_TRIP_DELAY) /* use '>=' instead of '>' to restrict the round-trip error */
        {
            status = NTP_ERR_TOO_MUCH_DELAY;
            if (i < NTP_REQUEST_MAX_TRIES - 1) {
                Info (_T("Too much round trip delay, try again"));
            } else {
                Error (_T("Always get too much round trip delay, use another server instead"));
            }
        }

        switch (status) {
        case NTP_ERR_SUCCESS:
            bValid = TRUE;
            /* since already got the offset, no other try is needed. go through*/
            __fallthrough;

        case NTP_ERR_SERVER_REJECT_REQUEST:
        case NTP_ERR_SERVER_NOT_RELIABLE:
            /* server isn't usable, stop pinging it */
            bStopTry = TRUE;
            break;
            /* other failure, should try again */
        case NTP_ERR_FATAL_ERROR:
        case NTP_ERR_SOCKET_ERROR:
        case NTP_ERR_SERVER_NO_RESPONSE:
        case NTP_ERR_SERVER_INVALID_RESPONSE:
        case NTP_ERR_TOO_MUCH_DELAY:
        default:
            break;
        }
        if (!bStopTry && i < NTP_REQUEST_MAX_TRIES - 1) {
            /* use exponential backoff algorithm to prevent multiple clients repeatly send requests at the same time */
            rand_s(&uiRand);
            double pollInterval = (double) uiRand / UINT_MAX * (((unsigned __int64)1) << i) * NTP_POLL_INTERVAL;
            Info (_T("Current poll interval is %dms, sleeping ..."), (DWORD) pollInterval);
            Sleep ((DWORD) pollInterval);
        }
    }
    return bValid;
}


