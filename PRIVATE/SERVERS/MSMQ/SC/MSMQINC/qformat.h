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
    qformat.h

Abstract:
    Falcon Internal Queue format represintation for FormatQueue string.

--*/

#ifndef __QFORMAT_H
#define __QFORMAT_H

#ifndef _QUEUE_FORMAT_DEFINED
#define _QUEUE_FORMAT_DEFINED

#ifdef __midl
cpp_quote("#ifndef __cplusplus")
cpp_quote("#ifndef _QUEUE_FORMAT_DEFINED")
cpp_quote("#define _QUEUE_FORMAT_DEFINED")
#endif // __midl

enum QUEUE_FORMAT_TYPE {
    QUEUE_FORMAT_TYPE_UNKNOWN = 0,
    QUEUE_FORMAT_TYPE_PUBLIC,
    QUEUE_FORMAT_TYPE_PRIVATE,
    QUEUE_FORMAT_TYPE_DIRECT,
    QUEUE_FORMAT_TYPE_MACHINE,
    QUEUE_FORMAT_TYPE_CONNECTOR,
};

enum QUEUE_SUFFIX_TYPE {
    QUEUE_SUFFIX_TYPE_NONE = 0,
    QUEUE_SUFFIX_TYPE_JOURNAL,
    QUEUE_SUFFIX_TYPE_DEADLETTER,
    QUEUE_SUFFIX_TYPE_DEADXACT,
    QUEUE_SUFFIX_TYPE_XACTONLY,
};

//---------------------------------------------------------
//
//  struct QUEUE_FORMAT
//
//  NOTE:   This structure should NOT contain virtual
//          functions. They are not RPC-able.
//
//---------------------------------------------------------
typedef struct QUEUE_FORMAT *PQUEUE_FORMAT;

struct QUEUE_FORMAT {

#ifndef __midl
public:

    BOOL Leagal() const;
    void Validate() const;

    QUEUE_FORMAT_TYPE GetType() const;
    QUEUE_SUFFIX_TYPE Suffix() const;
    void Suffix(QUEUE_SUFFIX_TYPE);

    QUEUE_FORMAT();
    void UnknownID(PVOID);

    QUEUE_FORMAT(const GUID&);
    void PublicID(const GUID&);
    const GUID& PublicID() const;

    QUEUE_FORMAT(const OBJECTID&);
    void PrivateID(const OBJECTID&);
    QUEUE_FORMAT(const GUID&, ULONG);
    void PrivateID(const GUID&, ULONG);
    const OBJECTID& PrivateID() const;

    void DisposeDirectID();
    QUEUE_FORMAT(LPWSTR);
    void DirectID(LPWSTR);
    LPCWSTR DirectID() const;

    QUEUE_FORMAT(PVOID, const GUID&);
    void MachineID(const GUID&);
    const GUID& MachineID() const;

    void  ConnectorID(const GUID&);
    const GUID& ConnectorID() const;

private:

#endif // !__midl

    UCHAR m_qft;
    UCHAR m_qst;
    USHORT m_reserved;

#ifdef __midl
    [switch_is(m_qft)]
#endif // __midl

    union {
#ifdef __midl
        [case(QUEUE_FORMAT_TYPE_UNKNOWN)]
#endif
            ;
#ifdef __midl
        [case(QUEUE_FORMAT_TYPE_PUBLIC)]
#endif // __midl
        GUID m_gPublicID;

#ifdef __midl
        [case(QUEUE_FORMAT_TYPE_PRIVATE)]
#endif // __midl

        OBJECTID m_oPrivateID;

#ifdef __midl
        [case(QUEUE_FORMAT_TYPE_DIRECT)]
#endif // __midl

        LPWSTR m_pDirectID;

#ifdef __midl
        [case(QUEUE_FORMAT_TYPE_MACHINE)]
#endif // __midl

        GUID m_gMachineID;

#ifdef __midl
        [case(QUEUE_FORMAT_TYPE_CONNECTOR)]
#endif // __midl

        GUID m_gConnectorID;
    };
};

#ifdef __midl
cpp_quote("#endif // _QUEUE_FORMAT_DEFINED")
cpp_quote("#endif // __cplusplus")
#endif // __midl

#endif // _QUEUE_FORMAT_DEFINED


#ifdef __cplusplus

inline BOOL QUEUE_FORMAT::Leagal() const
{
    switch(m_qst)
    {
        case QUEUE_SUFFIX_TYPE_NONE:
            return (m_qft != QUEUE_FORMAT_TYPE_MACHINE);

        case QUEUE_SUFFIX_TYPE_JOURNAL:
            return (m_qft != QUEUE_FORMAT_TYPE_CONNECTOR);

        case QUEUE_SUFFIX_TYPE_DEADLETTER:
        case QUEUE_SUFFIX_TYPE_DEADXACT:
            return (m_qft == QUEUE_FORMAT_TYPE_MACHINE);

        case QUEUE_SUFFIX_TYPE_XACTONLY:
            return (m_qft == QUEUE_FORMAT_TYPE_CONNECTOR);
    }
    return FALSE;
}

inline void QUEUE_FORMAT::Validate() const
{
    ASSERT((m_qft <= QUEUE_FORMAT_TYPE_CONNECTOR));
    ASSERT((m_qst <= QUEUE_SUFFIX_TYPE_XACTONLY));

    ASSERT((m_qft != QUEUE_FORMAT_TYPE_UNKNOWN));
    ASSERT(Leagal());
}

inline QUEUE_FORMAT_TYPE QUEUE_FORMAT::GetType() const
{
    return ((QUEUE_FORMAT_TYPE)m_qft);
}

inline QUEUE_SUFFIX_TYPE QUEUE_FORMAT::Suffix() const
{
    return ((QUEUE_SUFFIX_TYPE)m_qst);
}

inline void QUEUE_FORMAT::Suffix(QUEUE_SUFFIX_TYPE qst)
{
    m_qst = static_cast<UCHAR>(qst);
}

inline void QUEUE_FORMAT::UnknownID(PVOID)
{
    m_qft = QUEUE_FORMAT_TYPE_UNKNOWN;
    m_qst = QUEUE_SUFFIX_TYPE_NONE;
}

inline QUEUE_FORMAT::QUEUE_FORMAT()
{
    UnknownID(0);
}

inline void QUEUE_FORMAT::PublicID(const GUID& gPublicID)
{
    m_qft = QUEUE_FORMAT_TYPE_PUBLIC;
    m_qst = QUEUE_SUFFIX_TYPE_NONE;
    m_gPublicID = gPublicID;
}

inline QUEUE_FORMAT::QUEUE_FORMAT(const GUID& gPublicID)
{
    PublicID(gPublicID);
}

inline const GUID& QUEUE_FORMAT::PublicID() const
{
    ASSERT(GetType() == QUEUE_FORMAT_TYPE_PUBLIC);
    return m_gPublicID;
}

inline void QUEUE_FORMAT::PrivateID(const GUID& Lineage, ULONG Uniquifier)
{
    m_qft = QUEUE_FORMAT_TYPE_PRIVATE;
    m_qst = QUEUE_SUFFIX_TYPE_NONE;
    m_oPrivateID.Lineage = Lineage;
    m_oPrivateID.Uniquifier = Uniquifier;
    ASSERT(Uniquifier != 0);
}

inline QUEUE_FORMAT::QUEUE_FORMAT(const GUID& Lineage, ULONG Uniquifier)
{
    PrivateID(Lineage, Uniquifier);
}

inline void QUEUE_FORMAT::PrivateID(const OBJECTID& oPrivateID)
{
    PrivateID(oPrivateID.Lineage, oPrivateID.Uniquifier);
}

inline QUEUE_FORMAT::QUEUE_FORMAT(const OBJECTID& oPrivateID)
{
    PrivateID(oPrivateID);
}

inline const OBJECTID& QUEUE_FORMAT::PrivateID() const
{
    ASSERT(GetType() == QUEUE_FORMAT_TYPE_PRIVATE);
    return m_oPrivateID;
}

inline void QUEUE_FORMAT::DisposeDirectID()
{
    if(GetType() == QUEUE_FORMAT_TYPE_DIRECT)
    {
        delete[] m_pDirectID;
    }
}

inline void QUEUE_FORMAT::DirectID(LPWSTR pDirectID)
{
    m_qft = QUEUE_FORMAT_TYPE_DIRECT;
    m_qst = QUEUE_SUFFIX_TYPE_NONE;
    m_pDirectID = pDirectID;
}

inline QUEUE_FORMAT::QUEUE_FORMAT(LPWSTR pDirectID)
{
    DirectID(pDirectID);
}

inline LPCWSTR QUEUE_FORMAT::DirectID() const
{
    ASSERT(GetType() == QUEUE_FORMAT_TYPE_DIRECT);
    return m_pDirectID;
}

inline void QUEUE_FORMAT::MachineID(const GUID& gMachineID)
{
    m_qft = QUEUE_FORMAT_TYPE_MACHINE;
    m_qst = QUEUE_SUFFIX_TYPE_DEADLETTER;
    m_gMachineID = gMachineID;
}

inline QUEUE_FORMAT::QUEUE_FORMAT(PVOID, const GUID& gMachineID)
{
    MachineID(gMachineID);
}

inline const GUID& QUEUE_FORMAT::MachineID() const
{
    ASSERT(GetType() == QUEUE_FORMAT_TYPE_MACHINE);
    return m_gMachineID;
}

inline void QUEUE_FORMAT::ConnectorID(const GUID& gConnectorID)
{
    m_qft = QUEUE_FORMAT_TYPE_CONNECTOR;
    m_qst = QUEUE_SUFFIX_TYPE_NONE;
    m_gConnectorID = gConnectorID;
}

inline const GUID& QUEUE_FORMAT::ConnectorID() const
{
    ASSERT(GetType() == QUEUE_FORMAT_TYPE_CONNECTOR);
    return m_gConnectorID;
}

#endif

#endif // __QFORMAT_H
