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
// Definitions and declarations for the EchoRequest_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_EchoRequest_t_
#define _DEFINED_EchoRequest_t_
#pragma once

#include "WiFUtils.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>   // DSCP_TRAFFIC_TYPE
#include <auto_xxx.hxx>
#include <vector.hxx>

namespace ce {
namespace qa {

class EchoRequest_t
{

public:
    typedef bool (*PFNCALLBACK)(const EchoRequest_t&, void*);

    enum Protocol_e
    {
        ProtocolUnknown = -1,
        ProtocolUDP,
        ProtocolTCP,
        ProtocolCount,
    };

    static const long DefaultEchoPort       = 7;
    static const long MinimumEchoTimeout    = 100;       // ms to await completion
    static const long DefaultEchoTimeout    = 5*1000;
    static const long MaximumEchoTimeout    = 24*60*60*1000;
    static const long MinimumEchoSendSize   = 32;        // packet size
    static const long DefaultEchoSendSize   = 1024;
    static const long MaximumEchoSendSize   = 64*1024;
    static const long MinimumEchoSendCount  = 1;         // number packets to send
    static const long DefaultEchoSendCount  = 5*1000;
    static const long MaximumEchoSendCount  = 10*1000*1000;
    static const long MinimumEchoQueueSize  = 8*1024;    // max un-echoed data
    static const long DefaultEchoQueueSize  = 64*1024;   // (only used for UDP)
    static const long MaximumEchoQueueSize  = 1024*1024;
    static const long DefaultEchoInterval   = 20;        // Default Voip packet interval
    static const long DefaultVoipEchoSize   = 184;       // Default Voip packet payload

    template<class _Value, class _Total>
    class TimingData_t
    {
    public:
        TimingData_t(void);
        ~TimingData_t();

        void
        Reset(void);

        DWORD
        Record(
            _Value value);

        DWORD
        GetStdDev(void) const;

        void
        Log(
            const TCHAR* pszHeader) const;

    public:
        DWORD              m_dwCount;
        DWORD              m_dwSlowestIndex;
        DWORD              m_dwFastestIndex;
        _Total             m_Total;
        _Value             m_SlowestValue;
        _Value             m_FastestValue;
        bool               m_fRecordValues;
        ce::vector<_Value> m_vecValues;
    };

private:

    // Packet encoding types and count:
    static const int  PktEncodingDefault    = 0;
    static const int  PktEncodingSystemTime = 1;
    static const int  PktEncodingCount      = 2;

    // Define the minimum packet size required for packet encoding types:
    struct PktEncoding_t
    {
        WORD wMinPacketSize;
    };
    static const PktEncoding_t     s_rgPktEncoding[PktEncodingCount];

    class TransferData_t
    {
    public:

        DWORD      TotalPackets;//Total packets transferred.
        DWORD      TotalBytes; // Total bytes transferred (multiple of packet size).
        DWORD      Packets;    // Internal count of packets sent.
        DWORD      Bytes;      // Partial packet bytes transferred.
        DWORD      PktNumber;  // Next packet number to transfer.
        BYTE*      Buffer;     // Transfer buffer.
        DWORD*     Flags;      // Bit array of which packets transferred.
        fd_set     FdSet;
        fd_set*    pFdSet;
        SYSTEMTIME Time;       // Time last packet was sent/received.
        bool       fComplete;  // A complete packet was transferred.

        TransferData_t(void);
        ~TransferData_t();

        DWORD
        Init(
            DWORD dwPacketSize,
            DWORD dwPacketCount);

        void
        Reset(
            bool fResetTotals = true);

    private:

        void
        FreeBuffers(void);

        DWORD
        AllocBuffers(
            DWORD dwPacketSize,
            DWORD dwPacketCount);

        HLOCAL
        AllocLocalBuffer(
            HLOCAL hMem,
            DWORD  dwAllocSize);
    };

    // Send/receive transfer data:
    TransferData_t    m_send;
    TransferData_t    m_recv;

    // Socket and socket address:
    ce::auto_socket   m_sock;
    addrinfo          m_sockaddr;

    // Hostname, port, protocol, and DSCP traffic type:
    ce::tstring       m_strHostName;
    DWORD             m_dwPort;
    Protocol_e        m_Protocol;
    DSCP_TRAFFIC_TYPE m_TrafficType;

    // Send timeout:
    DWORD             m_dwReplyTimeoutMs;

    // Packet queue and send window size for UDP:
    DWORD             m_dwQueueSize;
    DWORD             m_dwSendWindow;

    // Send start and finish time:
    DWORD             m_dwStartTicks;
    DWORD             m_dwFinishTicks;

    // Packet interval delay:
    DWORD             m_dwPacketIntervalMs;
    
    // Resent and duplicate received packet count:
    DWORD             m_dwResendCount;
    DWORD             m_dwDuplicateCount;

    // Callback function and parameter:
    PFNCALLBACK       m_pfnCallback;
    void*             m_pvCallbackParam;

    // Connected flag:
    bool              m_fConnected;

    // Packet encoding:
    int               m_iPktEncoding;

    // Latency/Jitter data:
    TimingData_t<DWORD, DWORD>      m_Latency;
    TimingData_t<int, int> m_Jitter;

    // Disable copy and assignment.
    EchoRequest_t(const EchoRequest_t&);
    EchoRequest_t& operator=(const EchoRequest_t&);

public:

    EchoRequest_t(void);

    virtual
    ~EchoRequest_t();

    DWORD
    Connect(
        const TCHAR *      pHostName,
        DWORD              HostPort,
        Protocol_e         eProtocol,
        DSCP_TRAFFIC_TYPE  TrafficType = DSCPTypeNotSet);

    DWORD
    Send(
        DWORD dwPacketSize,
        DWORD dwPacketCount);

    DWORD
    SendForTime(
        DWORD dwSeconds,
        DWORD dwPacketSize  = DefaultEchoSendSize,
        DWORD dwPacketCount = DefaultEchoSendCount)
    {
        SetCallback(_TimeCallback, reinterpret_cast<void*>(dwSeconds));

        // Add 5 seconds to the specified send time, to help ensure the
        // callback will cause us to exit first.
        SetTimeout((dwSeconds + 5) * 1000);
        return Send(dwPacketSize, dwPacketCount);
    }

    // Send until the supplied handle is signaled.
    // NOTE: Be sure to call SetTimeout before calling this function.
    DWORD
    SendUntilSignaled(
        HANDLE hEvent,
        DWORD  dwPacketSize = DefaultEchoSendSize,
        DWORD  dwPacketCount = DefaultEchoSendCount)
    {
        SetCallback(_SignalCallback, reinterpret_cast<void*>(hEvent));
        return Send(dwPacketSize, dwPacketCount);
    }

    // Set a callback to exit the send loop:
    void
    SetCallback(
        PFNCALLBACK pfnCallback,
        void*       pvParam)
    {
        m_pfnCallback = pfnCallback;
        m_pvCallbackParam = pvParam;
    }

    // Set a timeout in milliseconds to exit the send loop:
    void
    SetTimeout(
        DWORD dwMilliseconds)
    {
        m_dwReplyTimeoutMs = ValidateParam(dwMilliseconds,
                                           MinimumEchoTimeout,
                                           MaximumEchoTimeout);
    }

    // Set a send-queue size for UDP transfers:
    void
    SetQueueSize(
        DWORD dwQueueSize)
    {
        m_dwQueueSize = ValidateParam(dwQueueSize,
                                      MinimumEchoQueueSize,
                                      MaximumEchoQueueSize);
    }

    // Set the DSCP traffic type for QoS transfers:
    void
    SetTrafficType(
        DSCP_TRAFFIC_TYPE TrafficType)
    {
        m_TrafficType = TrafficType;
    }

    // Set the delay between packets, in milliseconds, for simulated VOIP
    // transfers.  Also sets whether per-packet latency/jitter data is
    // recorded.  This can consume memory, especially for long duration
    // sends.  Default is off:
    void
    SetPacketInterval(
        DWORD dwMilliseconds,
        bool  fRecordLatencyJitter = false)
    {
        m_dwPacketIntervalMs = dwMilliseconds;

        bool fRecord = fRecordLatencyJitter && (0 < dwMilliseconds);
        m_Latency.m_fRecordValues = fRecord;
        m_Jitter.m_fRecordValues  = fRecord;
        m_iPktEncoding  = fRecord ? PktEncodingSystemTime
                                  : PktEncodingDefault;
    }

    // Get the number of bytes sent:
    DWORD
    GetSentByteCount(void) const
    { return m_send.TotalBytes + m_send.Bytes; }

    // Get the number of bytes received:
    DWORD
    GetReceivedByteCount(void) const
    { return m_recv.TotalBytes + m_recv.Bytes; }

    // Get the tick-count at the beginning of the transfer:
    DWORD
    GetStartTicks(void) const
    { return m_dwStartTicks; }

    // Get the tick-count at the end of the transfer:
    DWORD
    GetFinishTicks(void) const
    { return m_dwFinishTicks; }

    // Get the tick-count duration of the transfer:
    DWORD
    GetDurationTicks(void) const
    { return WiFUtils::SubtractTickCounts(GetFinishTicks(), GetStartTicks()); }

    const TimingData_t<DWORD, DWORD>&
    GetLatencyData(void) const
    { return m_Latency; }

    const TimingData_t<int, int>&
    GetJitterData(void) const
    { return m_Jitter; }

private:

    static bool
    _SignalCallback(
        const EchoRequest_t& request,
        void*                pvParam);

    static bool
    _TimeCallback(
        const EchoRequest_t& request,
        void*                pvParam);

    void
    CheckForSendPacket(
        DWORD dwPacketSize,
        DWORD dwPacketCount,
        DWORD dwFirstPacket);

    int
    Select(
        DWORD dwTimeoutMs);

    void
    MarkResendPackets(void);

    DWORD
    ReceivePacket(
        DWORD dwPacketSize,
        DWORD dwPacketCount,
        DWORD dwFirstPacket);

    DWORD
    SendPacket(
        DWORD dwPacketSize,
        DWORD dwFirstPacket);

    DWORD
    ValidateParam(
        DWORD&      dwParam,
        const DWORD dwMinValue,
        const DWORD dwMaxValue,
        bool        fQuiet = false);

    DWORD
    GetLastSocketErrorOr(
        DWORD dwError) const;

    void
    DumpFlags(
        DWORD dwPacketSize,
        DWORD dwPacketCount) const;

    void
    BuildSendPacket(
  __out_ecount(PktSize) UCHAR *     pPacket,
                        int         PktSize,
                        int         PktNumber,
                  const SYSTEMTIME& Time) const;

    int
    CheckRecvPacket(
        const UCHAR *pPacket,
        DWORD        PktSize,
        SYSTEMTIME * pTime) const;

    DWORD
    RecordLatencyJitter(
        const SYSTEMTIME& TimeSent,
        const SYSTEMTIME& TimeRecv,
        const SYSTEMTIME& TimeLastRecv);
};

}; // qa
}; // ce
#endif /* _DEFINED_EchoRequest_t_ */
// ----------------------------------------------------------------------------
