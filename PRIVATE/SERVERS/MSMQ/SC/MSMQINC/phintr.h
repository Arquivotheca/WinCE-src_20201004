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

    qmpkt.h

Abstract:

    Handle packet in QM side

--*/

#ifndef __QM_INTERNAL_PACKET__
#define __QM_INTERNAL_PACKET__

#include "ph.h"

#define STORED_ACK_BITFIELD_SIZE 32
#define INTERNAL_SESSION_PACKET              1
#define INTERNAL_ESTABLISH_CONNECTION_PACKET 2
#define INTERNAL_CONNECTION_PARAMETER_PACKET 3

#define ESTABLISH_CONNECTION_BODY_SIZE       512
#define CONNECTION_PARAMETERS_BODY_SIZE      512
/*

==== Debug Packet =============

+-----------------------+------------------------------------------------+----------+
| Field Name            | Description                                    |Field Size|
+-----------------------+------------------------------------------------+----------+
|

*/

#pragma pack(push, 1)
#pragma warning(disable: 4200)  //  zero-sized array in struct/union (enabeld later)

struct CDebugSection {
public:
    CDebugSection(IN const QUEUE_FORMAT* pReportQueue);

    static ULONG CalcSectionSize(IN const QUEUE_FORMAT* pReportQueue);

    PCHAR GetNextSection(void) const;

    void SetReportQueue(IN const QUEUE_FORMAT* pReportQueue);

    BOOL GetReportQueue(OUT QUEUE_FORMAT* pReportQueue);


private:
    enum QType {
        qtNone      = 0,    //  0 - None                    ( 0 bytes)
        qtGUID      = 1,    //  1 - Public  Queue           (16 bytes)
        qtPrivate   = 2,    //  2 - Private Queue           (20 bytes)
        qtDirect    = 3     //  3 - Direct  Queue           (var size)
    };
//
// BEGIN Network Monitor tag
//
    union {
        USHORT   m_wFlags;
        struct {
            USHORT m_bfRQT: 2;
        };
    };
    WORD m_wReserved;
    UCHAR m_abReportQueue[0];
//
// END Network Monitor tag
//


};

#pragma warning(default: 4200)  //  zero-sized array in struct/union
#pragma pack(pop)

/*======================================================================

 Function:

 Description:

 =======================================================================*/
inline
CDebugSection::CDebugSection(
        IN const QUEUE_FORMAT* pReportQueue
        ) :
        m_wFlags(0),
        m_wReserved(0)
{
    SetReportQueue(pReportQueue);
}

/*======================================================================

 Function:

 Description:

 =======================================================================*/
inline ULONG
CDebugSection::CalcSectionSize(const QUEUE_FORMAT* pReportQueue)
{
    ULONG ulSize = sizeof(CDebugSection);

    switch (pReportQueue->GetType())
    {
        case QUEUE_FORMAT_TYPE_PUBLIC:
            ulSize += sizeof(GUID);
            break;

        case QUEUE_FORMAT_TYPE_PRIVATE:
            //
            //  Report Queue is private
            //
            ulSize += (sizeof(GUID) + sizeof(ULONG));
            break;

        case QUEUE_FORMAT_TYPE_DIRECT:
            //
            // Report Queue is Direct
            //
            ulSize += (sizeof(USHORT) +
                       (wcslen(pReportQueue->DirectID()) + 1) * sizeof(WCHAR));
            break;
    }

    return ALIGNUP4(ulSize);
}

/*======================================================================

 Function:

 Description:

 =======================================================================*/
 inline PCHAR CDebugSection::GetNextSection(void) const
 {
    int size = sizeof(*this);
    switch (m_bfRQT)
    {
    case qtNone:
        size += 0;
        break;
    case qtGUID:
        size += sizeof(GUID);
        break;
    case qtPrivate:
        size += (sizeof(GUID) + sizeof(ULONG));
        break;
    case qtDirect:
        size += (sizeof(USHORT) + *(PUSHORT)m_abReportQueue);
        break;
    default:
        ASSERT(0);
    }

    return (PCHAR)this + ALIGNUP4(size);
 }


/*======================================================================

 Function:

 Description:

 =======================================================================*/
inline void
CDebugSection::SetReportQueue(IN const QUEUE_FORMAT* pReportQueue)
{
    PUCHAR pQueue = m_abReportQueue;

    switch (pReportQueue->GetType())
    {
        case QUEUE_FORMAT_TYPE_PUBLIC:
            //
            //  Report Queue is PUBLIC
            //
            m_bfRQT = qtGUID;
            *(GUID*)pQueue = pReportQueue->PublicID();
            break;

        case QUEUE_FORMAT_TYPE_PRIVATE:
            //
            //  Report Queue is PRIVATE
            //
            m_bfRQT = qtPrivate;
            *(OBJECTID*)pQueue =  pReportQueue->PrivateID();
            break;

        case QUEUE_FORMAT_TYPE_DIRECT:
        {
            //
            // Report queue is direct
            //
            m_bfRQT = qtDirect;

            size_t size = (wcslen(pReportQueue->DirectID()) + 1) * sizeof(WCHAR);
            *reinterpret_cast<USHORT*>(pQueue) = static_cast<USHORT>(size);
            memcpy(pQueue + sizeof(USHORT), pReportQueue->DirectID(), size);
            break;
        }
        default:
            //
            //  Unexpected type, assert with no Warning level 4.
            //
            ASSERT(pReportQueue->GetType() == QUEUE_FORMAT_TYPE_DIRECT);
    }

}

inline BOOL CDebugSection::GetReportQueue(QUEUE_FORMAT* pReportQueue)
{
    PUCHAR pQueue = m_abReportQueue;

    switch (m_bfRQT)
    {
        case qtNone:
            pReportQueue->UnknownID(0);
            return FALSE;

        case qtGUID:
            //
            //  Report Queue is PUBLIC
            //
            pReportQueue->PublicID(*(GUID*)pQueue);
            break;

        case qtPrivate:
            //
            //  Report Queue is PRIVATE
            //
            pReportQueue->PrivateID(*(OBJECTID*)pQueue);
            break;

        case QUEUE_FORMAT_TYPE_DIRECT:
            //
            // Report queue is direct
            //
            pReportQueue->DirectID((WCHAR*)(pQueue + sizeof(USHORT)));
            break;

        default:
            //
            //  Unexpected type.
            //
            ASSERT(0);
            return FALSE;
    }

    return TRUE;
}

/*
================= Session Packet =====================

+-----------------------+------------------------------------------------+----------+
| Field Name            | Description                                    |Field Size|
+-----------------------+------------------------------------------------+----------+
|ACK Sequence number    | The transmitted packet sequence number.        | 2 bytes  |
+-----------------------+------------------------------------------------+----------+
|Store ACK sequence     | The reliable packet sequence number.           | 2 bytes  |
|number (ps)            |                                                |          |
+-----------------------+------------------------------------------------+----------+
|Storage Ack bits       | Bit i:refers to recoverable packet no. Ps+I+ 1 | 4 bytes  |
|                       |    0 - no acknowledgment.                      |          |
|                       |    1 - the packet is acknowledgment.           |          |
+-----------------------+------------------------------------------------+----------+
| Window size           | number of packet in specific priority that     | 2 bytes  |
|                       | can be sent before getting a new window size.  |          |
+-----------------------+------------------------------------------------+----------+
| window priority       |message priority, in which the window size refer| 1 byte   |         |
+-----------------------+------------------------------------------------+----------+
| Reserved              |                                                | 1 byte   |
+-----------------------+------------------------------------------------+----------+

*/

#pragma pack(push, 1)

struct  CSessionSection {
    public:
        CSessionSection(WORD     wAckSequenceNo,
                        WORD     wAckRecoverNo,
                        DWORD    wAckRecoverBitField,
                        WORD     wSyncAckSequenceNo,
                        WORD     wSyncAckRecoverNo,
                        WORD     wWindowSize
                       );

        CSessionSection() {};

        static ULONG CalcSectionSize(void);

        inline WORD GetAcknowledgeNo(void) const;
        inline WORD GetStorageAckNo(void) const;
        inline DWORD GetStorageAckBitField(void) const;
        inline void GetSyncNo(WORD* wSyncAckSequenceNo,
                              WORD* wSyncAckRecoverNo);
        WORD GetWindowSize(void) const;

    private:
//
// BEGIN Network Monitor tag
//
        WORD    m_wAckSequenceNo;
        WORD    m_wAckRecoverNo;
        DWORD   m_wAckRecoverBitField;
        WORD    m_wSyncAckSequenceNo;
        WORD    m_wSyncAckRecoverNo;
        WORD    m_wWinSize;
        UCHAR   m_bWinPriority;
        UCHAR   m_bReserve;
//
// END Network Monitor tag
//
};
#pragma pack(pop)


/*====================================================


 Routine Name: CSession::Csession

 Description: Constructor

 Arguments:  wAckSequenceNo - Acknowledge sequence number
             wAckRecoverNo  - Acknowledge Recover packet number
             wAckRecoverBitField - Acknowledge recover bit field

=====================================================*/
inline
CSessionSection::CSessionSection(WORD     wAckSequenceNo,
                                 WORD     wAckRecoverNo,
                                 DWORD    wAckRecoverBitField,
                                 WORD     wSyncAckSequenceNo,
                                 WORD     wSyncAckRecoverNo,
                                 WORD     wWindowSize
                                )
{
    m_wAckSequenceNo      = wAckSequenceNo;
    m_wAckRecoverNo       = wAckRecoverNo;
    m_wAckRecoverBitField = wAckRecoverBitField;
    m_wSyncAckSequenceNo  = wSyncAckSequenceNo;
    m_wSyncAckRecoverNo   = wSyncAckRecoverNo;
    m_wWinSize            = wWindowSize;
    m_bWinPriority        = 0x0;
    m_bReserve            = 0x0;
}


/*====================================================

RoutineName

Arguments:

Return Value:

=====================================================*/
inline ULONG
CSessionSection::CalcSectionSize(void)
{
    return sizeof(CSessionSection);
}
/*====================================================

RoutineName

Arguments:

Return Value:

=====================================================*/
inline WORD
CSessionSection::GetAcknowledgeNo(void) const
{
    return(m_wAckSequenceNo);
}

/*====================================================

RoutineName

Arguments:

Return Value:

=====================================================*/
inline WORD
CSessionSection::GetStorageAckNo(void) const
{
    return(m_wAckRecoverNo);
}

/*====================================================

RoutineName

Arguments:

Return Value:

=====================================================*/
inline void
CSessionSection::GetSyncNo(WORD* wSyncAckSequenceNo,
                           WORD* wSyncAckRecoverNo)
{
    *wSyncAckSequenceNo = m_wSyncAckSequenceNo;
    *wSyncAckRecoverNo  = m_wSyncAckRecoverNo;
}

/*====================================================

RoutineName

Arguments:

Return Value:

=====================================================*/
inline DWORD
CSessionSection::GetStorageAckBitField(void) const
{
    return(m_wAckRecoverBitField);
}

/*====================================================

RoutineName

Arguments:

Return Value:

=====================================================*/
inline WORD 
CSessionSection::GetWindowSize(void) const
{
    return m_wWinSize;
}
/*
======================== Establish Connection Section ================================

+-----------------------+------------------------------------------------+----------+
| Field Name            | Description                                    |Field Size|
+-----------------------+------------------------------------------------+----------+
|Client QM Guid         | The Client QM Identifier                       | 16 bytes |
+-----------------------+------------------------------------------------+----------+
|SErver QM Guid         | The Server QM Identifier                       | 16 bytes |
+-----------------------+------------------------------------------------+----------+
|Time Stamp             | send packet time stamp. use for determine the  | 4 bytes  |
|                       | line quality                                   |          |
+-----------------------+------------------------------------------------+----------+
|Body                   |                                                | 512 bytes|
+-----------------------+------------------------------------------------+----------+

*/
#pragma pack(push, 1)

struct CECSection {
    public:
        CECSection(const GUID* ClientQMId,
                   const GUID* ServerQMId,
                   BOOL  fServer
                  );

        CECSection(const GUID* ClientQMId,
                   const GUID* ServerQMId,
                   ULONG dwTime,
                   BOOL  fServer
                  );

        static ULONG CalcSectionSize(void);

        void CheckAllowNewSession(BOOL);
        BOOL CheckAllowNewSession() const;

        const GUID* GetServerQMGuid() const;
        const GUID* GetClientQMGuid() const;
        ULONG GetTimeStamp() const;
        DWORD GetVersion() const;
        BOOL  IsOtherSideServer()const;


    private:
//
// BEGIN Network Monitor tag
//
        GUID    m_guidClientQM;
        GUID    m_guidServerQM;
        ULONG   m_ulTimeStampe;
        union {
            ULONG m_ulFlags;
            struct {
                ULONG m_bVersion : 8;
                ULONG m_fCheckNewSession : 1;
                ULONG m_fServer : 1;
            };
        };

        UCHAR   m_abBody[ESTABLISH_CONNECTION_BODY_SIZE];
//
// END Network Monitor tag
//

};

#pragma pack(pop)

/*================================================================

 Routine Name: CECPacket::CECPacket

 Description: Constructor

==================================================================*/

inline  CECSection::CECSection(const GUID* ClientQMId,
                               const GUID* ServerQMId,
                               BOOL  fServer
                              ) : m_guidClientQM(*ClientQMId),
                                  m_guidServerQM(*ServerQMId),
                                  m_ulTimeStampe(GetTickCount()),
                                  m_ulFlags(0)
{
    m_bVersion = FALCON_PACKET_VERSION;
    m_fServer = fServer;
}

inline  CECSection::CECSection(const GUID* ClientQMId,
                               const GUID* ServerQMId,
                               ULONG dwTime,
                               BOOL  fServer
                              ) : m_guidClientQM(*ClientQMId),
                                  m_guidServerQM(*ServerQMId),
                                  m_ulTimeStampe(dwTime),
                                  m_ulFlags(0)
{
    m_bVersion = FALCON_PACKET_VERSION;
    m_fServer = fServer;
}

inline ULONG
CECSection::CalcSectionSize(void)
{
    return sizeof(CECSection);
}

inline void 
CECSection::CheckAllowNewSession(BOOL f)
{
    m_fCheckNewSession = f;
}

inline BOOL 
CECSection::CheckAllowNewSession() const
{
    return m_fCheckNewSession;
}

inline const GUID*
CECSection::GetClientQMGuid(void) const
{
    return &m_guidClientQM;
}

inline const GUID*
CECSection::GetServerQMGuid(void) const
{
    return &m_guidServerQM;
}

inline DWORD
CECSection::GetTimeStamp(void) const
{
    return m_ulTimeStampe;
}

inline DWORD
CECSection::GetVersion(void) const
{
    return m_bVersion;
}

inline BOOL  
CECSection::IsOtherSideServer()const
{
    return m_fServer;
}

/*
==========================  Connection Parameters Section =============================

+-----------------------+------------------------------------------------+----------+
| Field Name            | Description                                    |Field Size|
+-----------------------+------------------------------------------------+----------+
| Window size           | number of packet that  can be sent before      | 2 bytes  |
|                       | getting an acknowledge                         |          |
+-----------------------+------------------------------------------------+----------+
|ACK Timeout            | The Max time can be passed before getting an   |          |
|                       | acknowledge                                    | 2 bytes  |
+-----------------------+------------------------------------------------+----------+
|Store ACK Timeout      | The Max time can be passed before getting an   |          |
|                       | acknowledge on persistence packet              | 2 bytes  |
+-----------------------+------------------------------------------------+----------+
|Max Segmentaion size   |                                                | 2 bytes  |
+-----------------------+------------------------------------------------+----------+

*/
#pragma pack(push, 1)

struct CCPSection {
    public:
        CCPSection(USHORT wWindowSize,
                   DWORD  dwRecoverAckTimeout,
                   DWORD  dwAckTimeout,
                   USHORT wSegmentSize
                  );

        static ULONG CalcSectionSize(void);

        USHORT GetWindowSize(void) const;
        void   SetWindowSize(USHORT);

        DWORD  GetRecoverAckTimeout(void) const;
        DWORD  GetAckTimeout(void) const;
        USHORT GetSegmentSize(void) const;

    private:
//
// BEGIN Network Monitor tag
//
        DWORD   m_dwRecoverAckTimeout;
        DWORD   m_dwAckTimeout;
        USHORT  m_wSegmentSize;
        USHORT  m_wWindowSize;

#if 0
        UCHAR   m_abBody[ CONNECTION_PARAMETERS_BODY_SIZE ] ;
#endif
//
// END Network Monitor tag
//
};

#pragma pack(pop)


/*================================================================

 Routine Name: CCPSection::CCPSection

 Description: Constructor

==================================================================*/

inline  CCPSection::CCPSection(USHORT wWindowSize,
                               DWORD  dwRecoverAckTimeout,
                               DWORD  dwAckTimeout,
                               USHORT wSegmentSize
                              ) : m_wWindowSize(wWindowSize),
                                  m_dwRecoverAckTimeout(dwRecoverAckTimeout),
                                  m_dwAckTimeout(dwAckTimeout),
                                  m_wSegmentSize(wSegmentSize)
{

}

inline ULONG
CCPSection::CalcSectionSize(void)
{
    return sizeof(CCPSection);
}

inline USHORT
CCPSection::GetWindowSize(void) const
{
    return m_wWindowSize;
}

inline void
CCPSection::SetWindowSize(USHORT wWindowSize)
{
    m_wWindowSize = wWindowSize;
}


inline DWORD
CCPSection::GetRecoverAckTimeout(void) const
{
    return m_dwRecoverAckTimeout;
}

inline DWORD
CCPSection::GetAckTimeout(void) const
{
    return m_dwAckTimeout;
}

inline USHORT
CCPSection::GetSegmentSize(void) const
{
    return m_wSegmentSize;
}

/*
=============================== Internal Section ==========================

+-----------------------+------------------------------------------------+----------+
| Field Name            | Description                                    |Field Size|
+-----------------------+------------------------------------------------+----------+
|                          Falcon BASE HEADER                                       |
|                                                                                   |
+-----------------------+------------------------------------------------+----------+
|Flags                  | 0-3: Packet Type                               | 2 bytes  |
|                       | 4:   Refuse connection bit                     |          |
+-----------------------+------------------------------------------------+----------+
|Reserve                |                                                | 2 bytes  |
+-----------------------+------------------------------------------------+----------+
|                         Specific packet body                                      |
+-----------------------+------------------------------------------------+----------+

*/
#pragma pack(push, 1)

struct CInternalSection {
    public:
        CInternalSection(USHORT usPacketType);

        static ULONG CalcSectionSize(void);
        PCHAR GetNextSection(void) const;

        USHORT GetPacketType(void) const;
        USHORT GetRefuseConnectionFlag(void) const;

        void SetRefuseConnectionFlag(void);

    private:
//
// BEGIN Network Monitor tag
//
        USHORT            m_bReserved;
        union
        {
            USHORT m_wFlags;
            struct
            {
                USHORT m_bfType : 4;
                USHORT m_bfConnectionRefuse : 1;
            };
        };
//
// END Network Monitor tag
//
};

#pragma pack(pop)

inline
CInternalSection::CInternalSection(USHORT usPacketType
                                ) : m_bReserved(0),
                                    m_wFlags(0)
{
    m_bfType = usPacketType;
}

inline ULONG
CInternalSection::CalcSectionSize(void)
{
    return sizeof(CInternalSection);
}

inline PCHAR
CInternalSection::GetNextSection(void) const
{
    return (PCHAR)this + sizeof(*this);
}

inline USHORT
CInternalSection::GetPacketType(void) const
{
    return m_bfType;
}

inline USHORT
CInternalSection::GetRefuseConnectionFlag(void) const
{
    return m_bfConnectionRefuse;
}

inline void
CInternalSection::SetRefuseConnectionFlag(void)
{
    m_bfConnectionRefuse = (USHORT) TRUE;
}



#endif //__QM_INTERNAL_PACKET__
