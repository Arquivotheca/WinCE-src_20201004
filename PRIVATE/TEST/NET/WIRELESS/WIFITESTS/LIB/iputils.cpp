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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Implementation of the IPUtils class.
//
// ----------------------------------------------------------------------------

#include "IPUtils.hpp"
#include "WZCService_t.hpp"

#include <assert.h>
#include <netcmn.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <icmpapi.h>
#include <iphlpapi.h>

#include <auto_xxx.hxx>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Initializes or cleans up static resources.
//
void
IPUtils::
StartupInitialize(void)
{
    // nothing to do.
}

void
IPUtils::
ShutdownCleanup(void)
{
    // nothing to do.
}

// ----------------------------------------------------------------------------
//
// Gets the IP addresses for GetIPAddresses. Takes a few arguments 
// indicating how to handle GetAdaptersInfo errors.
//
static DWORD
LocalGetIPAddresses(
                             const TCHAR *pAdapterName,
  __out_ecount(AddressChars) TCHAR       *pIPAddress,
  __out_ecount(AddressChars) TCHAR       *pGatewayAddress, // optional
                             int          AddressChars,
                             bool         ShowGetAdaptersError, // else warn
                             DWORD       *pLastGetAdaptersError)
{
    HRESULT hr;
    DWORD result;
    
    IP_ADAPTER_INFO  *pAdapterInfo = NULL;
    ULONG              adapterSize = 0;
    ce::auto_local_mem adapterMemory;

    assert(NULL != pAdapterName);
    assert(NULL != pIPAddress);
    assert(NULL != pLastGetAdaptersError);

    // Get the adapter information table.
    result = GetAdaptersInfo(pAdapterInfo, &adapterSize);
    if (ERROR_BUFFER_OVERFLOW == result)
    {
        pAdapterInfo = (IP_ADAPTER_INFO *) LocalAlloc(LPTR, adapterSize);
        if (NULL == pAdapterInfo)
            result = ERROR_OUTOFMEMORY;
        else
        {
            adapterMemory = pAdapterInfo;
            result = GetAdaptersInfo(pAdapterInfo, &adapterSize);
        }
    }
    
    if (NO_ERROR != result)
    {
        void (*pLogFunc)(const TCHAR *,...) = LogDebug;
        if (ShowGetAdaptersError)
            pLogFunc = LogError;
        else
        if (*pLastGetAdaptersError != result)
        {
            pLogFunc = LogWarn;
           *pLastGetAdaptersError = result;
        }

        pLogFunc(TEXT("Can't GetAdaptersInfo(\"%s\"): %s"),
                 pAdapterName, Win32ErrorText(result));
        return result;
    }

    // Translate the adapter name to ASCII.
    char asciiName[128];
    hr = WiFUtils::ConvertString(asciiName, pAdapterName,
                         COUNTOF(asciiName), "adapter name");
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    // Find the adapter in the list.
    result = ERROR_NOT_CONNECTED;
    for (; NULL != pAdapterInfo ; pAdapterInfo = pAdapterInfo->Next)
    {
        if (strncmp(pAdapterInfo->AdapterName, asciiName,
                                       COUNTOF(asciiName)) != 0)
            continue;

        // Check each of its IP addresses.
        IP_ADDR_STRING *pAddressList = &(pAdapterInfo->IpAddressList);
        for (; NULL != pAddressList ; pAddressList = pAddressList->Next)
        {
            // Check whether it's a real IP address
            const char *pAddress = pAddressList->IpAddress.String;
            if (strncmp(pAddress, "0.",   2) == 0
             || strncmp(pAddress, "169.", 4) == 0)
                continue;
            result = NO_ERROR;

            // Get the IP address.
            hr = WiFUtils::ConvertString(pIPAddress, pAddress,
                                         AddressChars, "IP Address");
            if (FAILED(hr))
                result = HRESULT_CODE(hr);
        
            // If necessary, get the gateway address, too.
            else
            if (NULL != pGatewayAddress)
            {
                pAddressList = &(pAdapterInfo->GatewayList);
                pAddress = pAddressList->IpAddress.String;
                hr = WiFUtils::ConvertString(pGatewayAddress, pAddress,
                                             AddressChars, "Gateway Address");
                if (FAILED(hr))
                    result = HRESULT_CODE(hr);
            }
            
            break;
        }
        break;
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Gets the IP addresses assigned to the specified wireless adapter.
// Returns ERROR_NOT_CONNECTED if the adapter hasn't been connected
// and assigned a valid IP address yet.
//
DWORD
IPUtils::
GetIPAddresses(
                             const TCHAR *pAdapterName,
  __out_ecount(AddressChars) TCHAR       *pIPAddress,
  __out_ecount(AddressChars) TCHAR       *pGatewayAddress, // optional
                             int          AddressChars)
{
    DWORD lastGetAdaptersError = NO_ERROR;
    return LocalGetIPAddresses(pAdapterName,
                               pIPAddress,
                               pGatewayAddress,
                               AddressChars,
                               true, // GetAdaptersInfo failures are errors
                              &lastGetAdaptersError);
}

// ----------------------------------------------------------------------------
//
// Monitors the specified wireless adapter's connection status
// until either the specified time tuns out or it associates with
// the specified SSID and is assigned a valid IP address.
// Returns ERROR_TIMEOUT if the connection isn't essablished within
// the time-limit.
//
DWORD
IPUtils::
MonitorConnection(
    WZCService_t *pWZCService,
    const TCHAR  *pExpectedSSID,
    long          ConnectTimeMS,   // time to associate and assign IP
    long          StableTimeMS,    // time connection must remain stable
    long          CheckIntervalMS) // time/granularity between each state check
{
    DWORD result;
    
    assert(NULL != pExpectedSSID);
    assert(NULL != pWZCService);

    TCHAR ssidBuff[MAX_SSID_LEN+1];
    int   ssidChars = MAX_SSID_LEN;

    MACAddr_t macAddr;
    TCHAR     macBuff[sizeof(macAddr) * 4];
    int       macChars = COUNTOF(macBuff);

    const int MaxIPAddressChars = 64;
    TCHAR      ipAddress[MaxIPAddressChars+1];
    TCHAR gatewayAddress[MaxIPAddressChars+1];

    LogStatus(TEXT("Connecting to %s - waiting %ld seconds"),
              pExpectedSSID, ConnectTimeMS / 1000);

    assert(MinimumConnectTimeMS <= ConnectTimeMS
        && MaximumConnectTimeMS >= ConnectTimeMS);
    if (ConnectTimeMS < MinimumConnectTimeMS)
        ConnectTimeMS = MinimumConnectTimeMS;
    if (ConnectTimeMS > MaximumConnectTimeMS)
        ConnectTimeMS = MaximumConnectTimeMS;

    assert(MinimumStableTimeMS <= StableTimeMS
        && MaximumStableTimeMS >= StableTimeMS);
    if (StableTimeMS < MinimumStableTimeMS)
        StableTimeMS = MinimumStableTimeMS;
    if (StableTimeMS > MaximumStableTimeMS)
        StableTimeMS = MaximumStableTimeMS;

    assert(MinimumCheckIntervalMS <= CheckIntervalMS
        && MaximumCheckIntervalMS >= CheckIntervalMS);
    if (CheckIntervalMS < MinimumCheckIntervalMS)
        CheckIntervalMS = MinimumCheckIntervalMS;
    if (CheckIntervalMS > MaximumCheckIntervalMS)
        CheckIntervalMS = MaximumCheckIntervalMS;

    enum States_e { Disconnected, Associated, Connected };
    States_e state = Disconnected;

    DWORD lastGetAdaptersError = NO_ERROR;
    DWORD startTime = GetTickCount();
    DWORD stableStart = 0;
    long  runDuration = 0;
    WZCIntfEntry_t intf;
    do
    {
        Sleep(CheckIntervalMS);

        DWORD currentTime = GetTickCount();
        runDuration = WiFUtils::SubtractTickCounts(currentTime, startTime);

        // Get the current association status.
        result = pWZCService->QueryAdapterInfo(&intf);
        if (NO_ERROR != result)
            return result;

        // If associated, make sure it's the correct Access Point.
        if (NO_ERROR != intf.GetConnectedMAC(&macAddr)
         || NO_ERROR != intf.GetConnectedSSID(ssidBuff, ssidChars))
        {
            state = Disconnected;
            continue;
        }
        if (_tcsnicmp(ssidBuff, pExpectedSSID, MAX_SSID_LEN) != 0)
        {
            LogDebug(TEXT("  still associated with SSID %s - want %s"),
                        ssidBuff, pExpectedSSID);
            state = Disconnected;
            continue;
        }
        if (Associated > state)
        {
            macAddr.ToString(macBuff, macChars);
            LogDebug(TEXT("  associated with SSID %s at MAC %s")
                     TEXT(  " after %.2f seconds"),
                     ssidBuff, macBuff,
                     (double)runDuration / 1000.0);
            state = Associated;
        }

        // Get the current IP assignment.
        result = LocalGetIPAddresses(pWZCService->GetAdapterName(),
                                     ipAddress, gatewayAddress,
                                     MaxIPAddressChars,
                                     false, // GetAdapters failures are warnings
                                    &lastGetAdaptersError);
        if (NO_ERROR != result)
        {
            state = Associated;
            continue;
        }
        if (Connected > state)
        {
            LogDebug(TEXT("  connected with IP %s and gateway %s")
                     TEXT(  " after %.02f seconds"),
                     ipAddress, gatewayAddress,
                     (double)runDuration / 1000.0);
            state = Connected;

            stableStart = currentTime;
            continue;
        }

        // Check how long we've been connected.
        long stableDuration;
        stableDuration = WiFUtils::SubtractTickCounts(currentTime, stableStart);
        if (StableTimeMS <= stableDuration)
        {
            LogDebug(TEXT("  connection remained stable for %ld seconds"),
                     StableTimeMS / 1000);
            return NO_ERROR;
        }
    }
    while (ConnectTimeMS > runDuration);

    LogError(TEXT("Waited limit of %ld seconds for connection to SSID %s"),
             ConnectTimeMS / 1000, pExpectedSSID);
    return ERROR_TIMEOUT;
}

// ----------------------------------------------------------------------------
//
// Opens a UDP or TCP socket to the specified destination address.
//
static DWORD
ConnectToAddress(
    const TCHAR *pHostName,
    DWORD         HostPort,
    bool          ConnectTCP,
    SOCKET      *pSocket,   // can be NULL
    sockaddr    *pSockAddr) // can be NULL
{
    HRESULT hr;
    DWORD result;

    assert(NULL != pHostName && TEXT('\0') != pHostName[0]);

    // Translate the host and port to ASCII.
    char asciiName[MAX_PATH];
    char asciiPort[30];
    hr = WiFUtils::ConvertString(asciiName, pHostName, 
                         COUNTOF(asciiName), "host name");
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    hr = StringCchPrintfA(asciiPort, COUNTOF(asciiPort), "%d", HostPort);
    if (FAILED(hr))
    {
        LogError(TEXT("Can't format port number: %s"),
                 HRESULTErrorText(hr));
        return HRESULT_CODE(hr);
    }

    // Resolve the address.
    ADDRINFO hints, *pAddrInfo = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = ConnectTCP? SOCK_STREAM : SOCK_DGRAM;

    result = getaddrinfo(asciiName, asciiPort, &hints, &pAddrInfo);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Cannot resolve address \"%hs:%hs\": wsa %s"),
                 asciiName, asciiPort, Win32ErrorText(result));
    }
    else
    {
        // Attempt to connect each address in turn.
        result = WSANO_DATA;
        for (ADDRINFO *pAI = pAddrInfo ; NULL != pAI ; pAI = pAI->ai_next)
        {
            if (PF_INET == pAI->ai_family || PF_INET6 == pAI->ai_family)
            {
                ce::auto_socket sock = socket(pAI->ai_family,
                                              pAI->ai_socktype,
                                              pAI->ai_protocol); 
                if (!sock.valid()
                 || SOCKET_ERROR == connect(sock, pAI->ai_addr,
                                                  pAI->ai_addrlen))
                {
                    result = WSAGetLastError();
                }
                else
                {
                    result = NO_ERROR;
                    if (NULL != pSocket)
                       *pSocket = sock.release();
                    if (NULL != pSockAddr)
                       *pSockAddr = *pAI->ai_addr;
                    break;
                }
            }
        }

        if (NO_ERROR != result)
        {
            LogError(TEXT("Can't create %s socket for %hs:%hs: wsa %s"),
                    (ConnectTCP? TEXT("TCP") : TEXT("UDP")),
                     asciiName, asciiPort, Win32ErrorText(result));
        }
    }

    // Deallocate resources.
    if (NULL != pAddrInfo)
    {
        freeaddrinfo(pAddrInfo);
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// Generates a unique packet with the specified size and packet-number.
//
static void
BuildSendPacket(
  __out_ecount(PktSize) UCHAR *pPacket,
                        int    PktSize,
                        int    PktNumber)
{
    if (8 > PktSize)
    {
        assert(8 <= PktSize);
    }
    else
    {
        for (int hash = PktNumber, px = 8 ; --px >= 0 ; hash /= 10)
            pPacket[px] = (UCHAR)('0' + (hash % 10));
        for (int hash = PktNumber, px = 8 ; px < PktSize ; ++px, ++hash)
            pPacket[px] = (UCHAR)('A' + (hash % 23));
    }
}

// ----------------------------------------------------------------------------
//
// Checks the specified packet to verify it contains valid data.
// Returns the packet number. Returns -1 if packet data is corrupted.
//
static int
CheckRecvPacket(
    const UCHAR *pPacket,
    int          PktSize)
{
    if (8 > PktSize)
    {
        assert(8 <= PktSize);
        return -1;
    }

    int pktNumber = 0;
    for (int px = 0 ; px < 8 ; ++px)
    {
        char ch = (char)pPacket[px];
        if ('0' > ch || ch > '9')
            return -1;
        pktNumber *= 10;
        pktNumber += ch - '0';
    }

    for (int hash = pktNumber, px = 8 ; px < PktSize ; ++px, ++hash)
        if ((UCHAR)('A' + (hash % 23)) != pPacket[px])
            return -1;

    return pktNumber;
}

// ----------------------------------------------------------------------------
//
// Checks, sets or clears the specified packet-send/receive flag.
//
#undef  BITS_PER_DWORD
#define BITS_PER_DWORD (sizeof(DWORD) * 8)

inline bool
IsPacketFlagSet(
  __out_ecount(FlagIndex) const DWORD *pFlags,
                          long          FlagIndex)
{
    return 0 != (pFlags[FlagIndex / BITS_PER_DWORD] 
         & DWORD(1) << (FlagIndex % BITS_PER_DWORD));
}

inline void
SetPacketFlag(
  __out_ecount(FlagIndex) DWORD *pFlags,
                          long    FlagIndex)
{
    pFlags[FlagIndex / BITS_PER_DWORD] |= (DWORD(1) << (FlagIndex % BITS_PER_DWORD));
}


inline void
ClearPacketFlag(
  __out_ecount(FlagIndex) DWORD *pFlags,
                          long    FlagIndex)
{
    pFlags[FlagIndex / BITS_PER_DWORD] &= ~(DWORD(1) << (FlagIndex % BITS_PER_DWORD));
}

// ----------------------------------------------------------------------------
//
// Sends one or more UDP or TCP echo messages to the specified host.
// Returns ERROR_CONNECTION_INVALID to signal a failure which could
// be caused by the normal negative WZC API tests.
//
DWORD
IPUtils::
SendEchoMessages(
    const TCHAR *pHostName,
    DWORD         HostPort,
    long          ReplyTimeout, // ms to await completion
    bool          ConnectTCP,   // true=TCP, false=UDP
    long          SendSize,     // packet size
    long          SendCount,    // number packets to send
    long          QueueSize)    // max un-echoed data
{
    DWORD result;
    DWORD startTime = GetTickCount();
    DWORD allocSize;
    
    ce::auto_socket sock;

    UCHAR *sendBuffer, *recvBuffer;
    DWORD *sendFlags,  *recvFlags;

    assert(NULL != pHostName && TEXT('\0') != pHostName[0]);

    // Round the packet size up to the next 8-byte boundary.
    // This works around a problem in the AR5211 device-driver which
    // throws an exception when it tries to fragment an odd-sized packet.
    SendSize = ((SendSize + 7) / 8) * 8;

    assert(MinimumEchoTimeout <= ReplyTimeout
        && MaximumEchoTimeout >= ReplyTimeout);
    if (ReplyTimeout < MinimumEchoTimeout)
        ReplyTimeout = MinimumEchoTimeout;
    if (ReplyTimeout > MaximumEchoTimeout)
        ReplyTimeout = MaximumEchoTimeout;

    assert(MinimumEchoSendSize <= SendSize
        && MaximumEchoSendSize >= SendSize);
    if (SendSize < MinimumEchoSendSize)
        SendSize = MinimumEchoSendSize;
    if (SendSize > MaximumEchoSendSize)
        SendSize = MaximumEchoSendSize;
    
    assert(MinimumEchoSendCount <= SendCount
        && MaximumEchoSendCount >= SendCount);
    if (SendCount < MinimumEchoSendCount)
        SendCount = MinimumEchoSendCount;
    if (SendCount > MaximumEchoSendCount)
        SendCount = MaximumEchoSendCount;
    
    assert(MinimumEchoQueueSize <= QueueSize
        && MaximumEchoQueueSize >= QueueSize);
    if (QueueSize < MinimumEchoQueueSize)
        QueueSize = MinimumEchoQueueSize;
    if (QueueSize > MaximumEchoQueueSize)
        QueueSize = MaximumEchoQueueSize;

    LogDebug(TEXT("  waiting %u seconds for %d %d-byte %s echoes from %s"),
             ReplyTimeout / 1000, SendCount, SendSize,
             (ConnectTCP? TEXT("TCP") : TEXT("UDP")),
             pHostName);

    // Connect to the destination address.
    result = ConnectToAddress(pHostName, HostPort, ConnectTCP, &sock, NULL);
    if (NO_ERROR != result)
        return result;

    assert(sock.valid());

    // Allocate I/O buffers.
    allocSize = SendSize * 2 * sizeof(UCHAR);
    sendBuffer = (UCHAR *) LocalAlloc(LMEM_FIXED, allocSize);
    recvBuffer = &sendBuffer[SendSize];
    if (NULL == sendBuffer)
    {
        LogError(TEXT("Can't allocate %u bytes for I/O buffers"),
                 allocSize);
        return ERROR_OUTOFMEMORY;
    }
    ce::auto_local_mem sendBufferCloser(sendBuffer);

    // Allocate and initialize packet send/receive flags.
    allocSize = ((SendCount + BITS_PER_DWORD - 1) / BITS_PER_DWORD) * sizeof(DWORD);
    
    sendFlags = (DWORD *) LocalAlloc(LMEM_FIXED, allocSize);
    if (NULL == sendFlags)
    {
        LogError(TEXT("Can't allocate %u bytes for send flags"),
                 allocSize);
        return ERROR_OUTOFMEMORY;
    }
    ce::auto_local_mem sendFlagsCloser(sendFlags);
    memset(sendFlags, 0, allocSize);

    recvFlags = (DWORD *) LocalAlloc(LMEM_FIXED, allocSize);
    if (NULL == recvFlags)
    {
        LogError(TEXT("Can't allocate %u bytes for receive flags"),
                 allocSize);
        return ERROR_OUTOFMEMORY;
    }
    ce::auto_local_mem recvFlagsCloser(recvFlags);
    memset(recvFlags, 0, allocSize);

    // If we're using UDP, set up a send window so we only leave about
    // QueueSize packet data in the "pipe" at any one time.
    int sendWindowSize = ConnectTCP? SendCount : QueueSize / SendSize;
    int sendWindow = (SendCount < sendWindowSize)?
                      SendCount : sendWindowSize;

    // Send packets and get responses.
    long sentPackets = 0, sentBytes = 0, sendPktNumber = 0;
    long recvPackets = 0, recvBytes = 0, recvPktNumber = 0;
    long resendCount = 0, duplicateCount = 0;
    bool waitingForPackets = false;
    while (recvPackets < SendCount)
    {
        // Make sure we haven't timed out.
        DWORD currentTime = GetTickCount();
        long  runDuration = WiFUtils::SubtractTickCounts(currentTime, startTime);
        if (ReplyTimeout <= runDuration)
        {
            LogError(TEXT("Echo from host \"%s\" timed out after %.2f seconds"),
                     pHostName, (double)runDuration / 1000.0);
#if 0
            // Dump the send/recv flags to help diagnose packet
            // retransmision problems.
            for (long lx = 0, rx = 0 ; rx < SendCount ; ++lx)
            {
                BuildSendPacket(sendBuffer, SendSize, rx);
                UCHAR *pBuf = &sendBuffer[8];
                for (long bx = 0 ; bx < 50 && rx < SendCount ; ++bx, ++rx)
                {
                    *(pBuf++) = ' ';
                    if      (IsPacketFlagSet(recvFlags,rx) *(pBuf++) = 'R'; // R = recieved
                    else if (IsPacketFlagSet(sendFlags,rx) *(pBuf++) = 'S'; // S = sent
                    else                                   *(pBuf++) = '.'; // . = niether
                }
                *(pBuf++) = '\0';
                LogError(TEXT("%hs"), sendBuffer);
            }
#endif
            return ERROR_CONNECTION_INVALID;
        }

        static const long MinimumSelectTimeout = 200;
        static const long MaximumSelectTimeout = 500;
        long timeout = ReplyTimeout - runDuration;
        if (timeout < MinimumSelectTimeout)
            timeout = MinimumSelectTimeout;
        if (timeout > MaximumSelectTimeout)
            timeout = MaximumSelectTimeout;
        TIMEVAL timeval = { timeout / 1000,
                            timeout * 1000 };

        // If necessary, set up the next send packet.
        if (sentPackets < sendWindow && 0 == sentBytes)
        {
            for (; sentPackets < sendWindow ; ++sentPackets)
                if (!IsPacketFlagSet(sendFlags,sentPackets))
                    break;

            // If we reach the end, scan back over the list one
            // more time in case we've missed something.
            if (sentPackets >= sendWindow && !ConnectTCP)
            {
                sentPackets = 0;
                while (sentPackets < SendCount
                    && IsPacketFlagSet(sendFlags,sentPackets))
                    sentPackets++;
            }

            // If there's something to send, initialize the packet data.
            if (sentPackets < sendWindow)
            {
                sendPktNumber = sentPackets;
                BuildSendPacket(sendBuffer, SendSize, sendPktNumber);
            }
        }

        // Set up read and/or write fdsets.
        fd_set sendFdSet, *pSendFdSet = NULL;
        fd_set recvFdSet, *pRecvFdSet = NULL;
        if (sentPackets < sendWindow)
        {
            pSendFdSet = &sendFdSet;
            FD_ZERO(pSendFdSet);
            FD_SET(sock, pSendFdSet);
        }
        if (waitingForPackets)
        {
            pRecvFdSet = &recvFdSet;
            FD_ZERO(pRecvFdSet);
            FD_SET(sock, pRecvFdSet);
        }

        assert(NULL != pRecvFdSet || NULL != pSendFdSet);

        // Wait for room to write or messages to read.
        int numFds = select(0, pRecvFdSet, pSendFdSet, NULL, &timeval);
        if (SOCKET_ERROR == numFds)
        {
            result = WSAGetLastError();
            LogError(TEXT("Select error for %s echo from \"%s\": wsa %s"),
                    (ConnectTCP? TEXT("TCP") : TEXT("UDP")),
                     pHostName, Win32ErrorText(result));
            return result;
        }
        if (0 >= numFds)
        {
#if 0
            LogDebug(TEXT("    select timed out after %ld ms"), timeout);
#endif

            // If there is no input and we've finished writing packets,
            // assume any packets we haven't read yet need to be resent.
            if (sentPackets >= sendWindow && !ConnectTCP)
            {
                while (recvPackets < sentPackets)
                {
                    sentPackets--;
                    if (IsPacketFlagSet(sendFlags,sentPackets)
                    && !IsPacketFlagSet(recvFlags,sentPackets))
                    {
                        ClearPacketFlag(sendFlags,sentPackets);
                        resendCount++;
                    }
                }
            }
            continue;
        }

        // If there's anything to read, get the packet and compare it
        // against what we sent.
        if (NULL != pRecvFdSet && FD_ISSET(sock, pRecvFdSet))
        {
            char *inPtr = (char *)recvBuffer;
            int   inBytes = SendSize;
            inPtr   += recvBytes;
            inBytes -= recvBytes;

            int recvResult = recv(sock, inPtr, inBytes, 0);
            if (SOCKET_ERROR == recvResult)
            {
                result = WSAGetLastError();
                LogError(TEXT("Can't read %s packet from \"%s\": wsa %s"),
                        (ConnectTCP? TEXT("TCP") : TEXT("UDP")),
                         pHostName, Win32ErrorText(result));
                return result;
            }

            // If we've read part of the packet, go back for more data.
            if (inBytes > recvResult)
            {
#if 0
                LogDebug(TEXT("  tried to recv %d bytes - got %d instead"),
                         inBytes, recvResult);
#endif
                if (0 < recvResult)
                    recvBytes += recvResult;
            }

            // If we've read a complete packet...
            else
            {
#if 0
                LogDebug(TEXT("%20.8hs"), recvBuffer);
#endif

                // Validate the packet data.
                recvPktNumber = CheckRecvPacket(recvBuffer, SendSize);
                if (0 > recvPktNumber || recvPktNumber > SendCount)
                {
                    LogError(TEXT("Got corrupted %d-byte response from %s"),
                             SendSize, pHostName);
                    return ERROR_CONNECTION_INVALID;
                }
                recvBytes = 0;

                // If we're connected using TCP, the packets have to 
                // arrive in the proper order.
                if (ConnectTCP)
                {
                    if (recvPktNumber != recvPackets)
                    {
                        LogError(TEXT("Received out-of-order packet from %s:")
                                 TEXT(" expected %d, got %d"),
                                 pHostName, recvPackets, recvPktNumber);
                        return ERROR_CONNECTION_INVALID;
                    }
                    SetPacketFlag(recvFlags,recvPackets++);
                }

                // Otherwise, we're using an unreliable UDP transport, so
                // we have to handle retransmissions ourselves...
                else
                {
                    // If the packet was scheduled to be resent, unschedule it.
                    if (!IsPacketFlagSet(sendFlags,recvPktNumber))
                    {
                        SetPacketFlag(sendFlags,recvPktNumber);
                        resendCount--;
                    }

                    // If the packet has been recieved before, count it as a
                    // duplicate.
                    if (IsPacketFlagSet(recvFlags,recvPktNumber))
                    {
                        duplicateCount++;
                    }

                    // Otherwise, it's a new packet...
                    else
                    {
                        SetPacketFlag(recvFlags,recvPktNumber);

                        // If we've skipped one or more packets, mark all the
                        // lost packets to be resent.
                        if (recvPackets < recvPktNumber)
                        {
#if 0
                            LogDebug(TEXT("    received out-of-order packet:")
                                     TEXT(    " expected %d, got %d"),
                                     recvPackets, recvPktNumber);
#endif
                            for (int lostPacket = recvPackets ;
                                     lostPacket < recvPktNumber ;
                                   ++lostPacket)
                            {
                                if (IsPacketFlagSet(sendFlags,lostPacket))
                                {
                                    ClearPacketFlag(sendFlags,lostPacket);
                                    resendCount++;
                                }
                            }

                            recvPackets = recvPktNumber;
                        }

                        // Proceed to the next unrecieved packet.
                        while (recvPackets < SendCount
                            && IsPacketFlagSet(recvFlags,recvPackets))
                            recvPackets++;

                        // If we reach the end, scan back over the list one
                        // more time in case we're still missing something.
                        if (recvPackets >= SendCount)
                        {
                            recvPackets = 0;
                            while (recvPackets < SendCount
                                && IsPacketFlagSet(recvFlags,recvPackets))
                                recvPackets++;
                        }
                    }
                }

                // Move the send window.
                int newSendWindow = recvPackets + sendWindowSize;
                if (newSendWindow > sendWindow)
                {
                    sendWindow = newSendWindow;
                    if (sendWindow > SendCount)
                        sendWindow = SendCount;
                }
            }
        }

        // If there's room to write, send the remainder of the current packet.
        else
        if (NULL != pSendFdSet && FD_ISSET(sock, pSendFdSet))
        {
            const char *outPtr = (const char *)sendBuffer;
            int         outBytes = SendSize;
            outPtr   += sentBytes;
            outBytes -= sentBytes;

            int sendResult = send(sock, outPtr, outBytes, 0);
            if (SOCKET_ERROR == sendResult)
            {
                result = WSAGetLastError();
                LogError(TEXT("Can't send %s packet to \"%s\": wsa %s"),
                        (ConnectTCP? TEXT("TCP") : TEXT("UDP")),
                         pHostName, Win32ErrorText(result));
                return result;
            }

            // If we've sent part of the packet, go back and wait for
            // more space.
            if (outBytes > sendResult)
            {
#if 0
                LogDebug(TEXT("  tried to send %d bytes - sent %d instead"),
                         outBytes, sendResult);
#endif
                if (0 < sendResult)
                    sentBytes += sendResult;
            }

            // If we've sent a complete packet, just mark it so.
            else
            {
#if 0
                LogDebug(TEXT("%12.8hs"), sendBuffer);
#endif
                sentBytes = 0;
                SetPacketFlag(sendFlags,sendPktNumber);
                waitingForPackets = true;
            }
        }
    }

    DWORD currentTime = GetTickCount();
    long  runDuration = WiFUtils::SubtractTickCounts(currentTime, startTime);
    LogDebug(TEXT("    received %u echoes: avg round-trip time %.02f ms"),
             SendCount, (double)runDuration / (double)SendCount);
    if (resendCount)
        LogDebug(TEXT("    %d lost packets re-transmitted"),
                 resendCount);
    if (duplicateCount)
        LogDebug(TEXT("    %d duplicate packets received"),
                 duplicateCount);

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Sends one or more UDP or TCP echo messages to the specified host
// until the specified time-limit expires.
// Returns ERROR_CONNECTION_INVALID to signal a failure which could
// be caused by the normal negative WZC API tests.
//
DWORD
IPUtils::
SendEchoMessagesForTime(
    const TCHAR *pHostName,
    DWORD         HostPort,
    long          ReplyTimeout, // ms to await replies
    bool          ConnectTCP,   // true=TCP, false=UDP
    long          SendSize,     // packet size
    long          TimeDuration) // how long to send data in sec
{
    if (TimeDuration <= 0)
        return ERROR_INVALID_PARAMETER;
    
    DWORD startTime = GetTickCount();
    do
    {
        // Send 1024 packets at a time till end of time indicated
        DWORD result = SendEchoMessages(pHostName, HostPort, ReplyTimeout, 
                                        ConnectTCP, SendSize, 1024);
        if (result != ERROR_SUCCESS)
            return result;
    }
    while ((WiFUtils::SubtractTickCounts(GetTickCount(), startTime) / 1000) < TimeDuration);

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Sends one or more ICMP ping messages to the specified host.
// Returns ERROR_CONNECTION_INVALID to signal a failure which could
// be caused by the normal negative WZC API tests.
//
DWORD
IPUtils::
SendPingMessages(
    const TCHAR *pHostName,
    DWORD         HostPort,
    long          ReplyTimeout,  // ms to await completion
    long          SendSize,      // packet size
    long          SendCount,     // number packets to send
    UCHAR         TimeToLive,    // IP TTL value
    UCHAR         TypeOfService, // IP TOS value
    UCHAR         IPHeaderFlags, // IP header flags
    UCHAR       *pOptionsData,   // IP options
    UCHAR         OptionsSize)   // size of IP options
{
    DWORD result;
    DWORD startTime = GetTickCount();

    assert(NULL != pHostName && pHostName[0] != TEXT('\0'));
    assert(0 != TimeToLive);

    assert(MinimumPingTimeout <= ReplyTimeout
        && MaximumPingTimeout >= ReplyTimeout);
    if (ReplyTimeout < MinimumPingTimeout)
        ReplyTimeout = MinimumPingTimeout;
    if (ReplyTimeout > MaximumPingTimeout)
        ReplyTimeout = MaximumPingTimeout;

    assert(MinimumPingSendSize <= SendSize
        && MaximumPingSendSize >= SendSize);
    if (SendSize < MinimumPingSendSize)
        SendSize = MinimumPingSendSize;
    if (SendSize > MaximumPingSendSize)
        SendSize = MaximumPingSendSize;

    assert(MinimumPingSendCount <= SendCount
        && MaximumPingSendCount >= SendCount);
    if (SendCount < MinimumPingSendCount)
        SendCount = MinimumPingSendCount;
    if (SendCount > MaximumPingSendCount)
        SendCount = MaximumPingSendCount;

    static const long MinimumRecvSize = 16 * 1024;
    static const long MaximumRecvSize = 64 * 1024;
    long recvSize = SendCount * (SendSize + sizeof(ICMP_ECHO_REPLY));
    if (recvSize < MinimumRecvSize)
        recvSize = MinimumRecvSize;
    if (recvSize > MaximumRecvSize)
        recvSize = MaximumRecvSize;

    LogDebug(TEXT("  waiting %u seconds for %d %d-byte ICMP echoes from %s"),
             ReplyTimeout / 1000, SendCount, SendSize, pHostName);

    // Connect to the destination address.
    sockaddr hostAddr;
    result = ConnectToAddress(pHostName, HostPort, false, NULL, &hostAddr);
    if (NO_ERROR != result)
        return result;
    sockaddr_in *pInetAddr = (sockaddr_in *)&hostAddr;

    // Allocate send and receive buffers.
    UCHAR *sendBuffer = (UCHAR *) LocalAlloc(LMEM_FIXED, SendSize);
    if (NULL == sendBuffer)
    {
        LogError(TEXT("Can't allocate %d bytes for send buffer"),
                 SendSize);
        return ERROR_OUTOFMEMORY;
    }
    ce::auto_local_mem sendBufferCloser(sendBuffer);

    UCHAR *recvBuffer = (UCHAR *) LocalAlloc(LMEM_FIXED, recvSize);
    if (NULL == recvBuffer)
    {
        LogError(TEXT("Can't allocate %ld bytes for recv buffer"),
                 recvSize);
        return ERROR_OUTOFMEMORY;
    }
    ce::auto_local_mem recvBufferCloser(recvBuffer);

    // Initialize the send buffer with semi-random data.
    for (long bx = SendSize ; --bx >= 0 ;)
        sendBuffer[bx] = (UCHAR)('A' + (bx % 23));

    // Build the IP header options.
    IP_OPTION_INFORMATION sendOpts;
    memset(&sendOpts, 0, sizeof(IP_OPTION_INFORMATION));
    sendOpts.Ttl         = TimeToLive;
    sendOpts.Tos         = TypeOfService;
    sendOpts.Flags       = IPHeaderFlags;
    sendOpts.OptionsData = pOptionsData;
    sendOpts.OptionsSize =  OptionsSize;

    // Attach to ICMP.
    HANDLE hIcmp = IcmpCreateFile();
    if (INVALID_HANDLE_VALUE == hIcmp)
    {
        result = GetLastError();
        LogError(TEXT("Can't open handle to send ICMP packets: %s"),
                 Win32ErrorText(result));
        return result;
    }

    // Try the ping every few seconds. We do this because the pings tend to
    // time out if they start before the connection completes.
    long totalReplies = 0;
    long totalRoundTripTime = 0;
    while (totalReplies < SendCount)
    {
        // Make sure we haven't timed out.
        DWORD currentTime = GetTickCount();
        long  runDuration = WiFUtils::SubtractTickCounts(currentTime, startTime);
        if (ReplyTimeout <= runDuration)
        {
            LogError(TEXT("Ping from host \"%s\" timed out after %.2f seconds"),
                     pHostName, (double)runDuration / 1000.0);
            result = ERROR_CONNECTION_INVALID;
            break;
        }

        long timeout = ReplyTimeout - runDuration;
        static const long MinimumPingTimeout =    200;
        static const long MaximumPingTimeout = 2*1000;
        if (timeout < MinimumPingTimeout)
            timeout = MinimumPingTimeout;
        if (timeout > MaximumPingTimeout)
            timeout = MaximumPingTimeout;

        // Send the ping(s).
        DWORD numReplies = IcmpSendEcho(hIcmp,
                                        pInetAddr->sin_addr.s_addr,
                                        sendBuffer,
                                        SendSize,
                                       &sendOpts,
                                        recvBuffer,
                                        recvSize,
                                        timeout);
        if (numReplies == 0)
        {
            result = GetLastError();
            if (result == IP_REQ_TIMED_OUT)
                continue;
            else
            {
                LogError(TEXT("Ping from host \"%s\" failed: %s"),
                         pHostName, Win32ErrorText(result));
                break;
            }
        }

        // Check the response(s) for errors.
        const ICMP_ECHO_REPLY *pReply, *pReplyEnd;
        pReply    = (const ICMP_ECHO_REPLY *)&recvBuffer[0];
        pReplyEnd = (const ICMP_ECHO_REPLY *)&recvBuffer[recvSize];

        DWORD rx;
        for (result = IP_SUCCESS, rx = 0 ; rx < numReplies ; ++rx)
        {
            if (&pReply[rx+1] > pReplyEnd)
                break;
#if 0
            LogDebug(TEXT("  reply %u:"), rx);
            LogDebug(TEXT("    Status        = %u"), pReply[rx].Status);
            LogDebug(TEXT("    RoundTripTime = %u"), pReply[rx].RoundTripTime);
            LogDebug(TEXT("    DataSize      = %u"), pReply[rx].DataSize);
#endif
            result = pReply[rx].Status;
            if (IP_SUCCESS != result)
                break;

            totalReplies++;
            totalRoundTripTime += pReply[rx].RoundTripTime;
        }

        if (IP_SUCCESS != result)
        {
            LogError(TEXT("Ping #%u from host %s failed: %s"),
                     rx, pHostName, Win32ErrorText(result));
            break;
        }
    }

    // Detach from ICMP.
    IcmpCloseHandle(hIcmp);

    if (NO_ERROR == result)
    {
        LogDebug(TEXT("    received %u echoes: avg round-trip time %lu ms"),
                 totalReplies, totalRoundTripTime / totalReplies);
    }

    return result;
}

// ----------------------------------------------------------------------------
