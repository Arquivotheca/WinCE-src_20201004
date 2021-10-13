//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    phinfo.h

Abstract:

    Falcon Packet header info, not passed on network, only stored on disk

--*/

#ifndef __PHINFO_H
#define __PHINFO_H

class CPacket;

//---------------------------------------------------------
//
// class CPacketInfo
//
//---------------------------------------------------------

class CPacketInfo {
public:
    CPacketInfo(CPacket* pPacket, ULONG ulSequentialID);

    CPacket* Packet() const;
    void Packet(CPacket* pPacket);

    ULONG SequentialID() const;
    void SequentialID(ULONG ulID);

    ULONG ArrivalTime() const;
    void ArrivalTime(ULONG ulArrivalTime);

    BOOL InSourceMachine() const;
    void InSourceMachine(BOOL);

    BOOL InTargetQueue() const;
    void InTargetQueue(BOOL);

    BOOL InJournalQueue() const;
    void InJournalQueue(BOOL);

    BOOL InMachineJournal() const;
    void InMachineJournal(BOOL);

    BOOL InDeadletterQueue() const;
    void InDeadletterQueue(BOOL);

    BOOL InMachineDeadxact() const;
    void InMachineDeadxact(BOOL);

    BOOL InConnectorQueue() const;
    void InConnectorQueue(BOOL);

    BOOL InTransaction() const;
    void InTransaction(BOOL);

    BOOL TransactSend() const;
    void TransactSend(BOOL);

    const XACTUOW* Uow() const;
    void Uow(const XACTUOW* pUow);

    BOOL KillTransaction() const;
    void KillTransaction(BOOL);

private:
    CPacket* m_pPacket;
    ULONG m_ulID;
    ULONG m_ulArrivalTime;
    union {
        ULONG m_ulFlags;
        struct {
            ULONG m_bfInSourceMachine   : 1;    // The packet was originaly send from this machine
            ULONG m_bfInTargetQueue     : 1;    // The packet has reached destination queue
            ULONG m_bfInJournalQueue    : 1;    // The packet is in a journal queue
            ULONG m_bfInMachineJournal  : 1;    // The packet is in the machine journal
            ULONG m_bfInDeadletterQueue : 1;    // The packet is in a deadletter queue
            ULONG m_bfInMachineDeadxact : 1;    // The packet is in the machine deadxact
            ULONG m_bfTransacted        : 1;    // The packet is under transaction control
            ULONG m_bfTransactSend      : 1;    // The transacted packet is sent (not received)
            ULONG m_bfInConnectorQueue  : 1;    // The packet has reached the Connector queue
                                                // used in recovery of transacted messages in Connector

            ULONG m_bfKillTransaction   : 1;    // Transacted message was killed. Don't store
                                                // it on order ack list
        };
    };
    XACTUOW m_uow;
};

inline CPacketInfo::CPacketInfo(CPacket* pPacket, ULONG ulSequentialID) :
    m_pPacket(pPacket),
    m_ulID(ulSequentialID),
    m_ulArrivalTime(0),
    m_ulFlags(0)
{
    memset(&m_uow, 0, sizeof(XACTUOW));
}

inline CPacket* CPacketInfo::Packet() const
{
    return m_pPacket;
}

inline void CPacketInfo::Packet(CPacket* pPacket)
{
    m_pPacket = pPacket;
}

inline ULONG CPacketInfo::SequentialID() const
{
    return m_ulID;
}

inline void CPacketInfo::SequentialID(ULONG ulID)
{
    m_ulID = ulID;
}

inline ULONG CPacketInfo::ArrivalTime() const
{
    return m_ulArrivalTime ;
}

inline void CPacketInfo::ArrivalTime(ULONG ulArrivalTime)
{
    m_ulArrivalTime = ulArrivalTime;
}

inline BOOL CPacketInfo::InSourceMachine() const
{
    return m_bfInSourceMachine;
}

inline void CPacketInfo::InSourceMachine(BOOL f)
{
    m_bfInSourceMachine = f;
}

inline BOOL CPacketInfo::InTargetQueue() const
{
    return m_bfInTargetQueue;
}

inline void CPacketInfo::InTargetQueue(BOOL f)
{
    m_bfInTargetQueue = f;
}

inline BOOL CPacketInfo::InJournalQueue() const
{
    return m_bfInJournalQueue;
}

inline void CPacketInfo::InJournalQueue(BOOL f)
{
    m_bfInJournalQueue = f;
}

inline BOOL CPacketInfo::InMachineJournal() const
{
    return m_bfInMachineJournal;
}

inline void CPacketInfo::InMachineJournal(BOOL f)
{
    m_bfInMachineJournal = f;
}

inline BOOL CPacketInfo::InDeadletterQueue() const
{
    return m_bfInDeadletterQueue;
}

inline void CPacketInfo::InDeadletterQueue(BOOL f)
{
    m_bfInDeadletterQueue = f;
}

inline BOOL CPacketInfo::InMachineDeadxact() const
{
    return m_bfInMachineDeadxact;
}

inline void CPacketInfo::InMachineDeadxact(BOOL f)
{
    m_bfInMachineDeadxact = f;
}

inline BOOL CPacketInfo::InConnectorQueue() const
{
    return m_bfInConnectorQueue;
}

inline void CPacketInfo::InConnectorQueue(BOOL f)
{
    m_bfInConnectorQueue = f;
}

inline BOOL CPacketInfo::InTransaction() const
{
    return m_bfTransacted;
}

inline void CPacketInfo::InTransaction(BOOL f)
{
    m_bfTransacted = f;
}

inline BOOL CPacketInfo::TransactSend() const
{
    return m_bfTransactSend;
}

inline void CPacketInfo::TransactSend(BOOL f)
{
   m_bfTransactSend = f;
}

inline const XACTUOW* CPacketInfo::Uow() const
{
    return &m_uow;
}

inline void CPacketInfo::Uow(const XACTUOW* pUow)
{
    memcpy(&m_uow, pUow, sizeof(XACTUOW));
}

inline BOOL CPacketInfo::KillTransaction() const
{
    return m_bfKillTransaction;
}

inline void CPacketInfo::KillTransaction(BOOL f)
{
   m_bfKillTransaction = f;
}

#endif // __PHINFO_H
