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

    userhead.h

Abstract:

    SOAP fields not supported by XP currently - <from> and <relatesTo>


--*/

#ifndef __PHSOAPEXT_H
#define __PHSOAPEXT_H

/*+++

    Header fields.

+----------------+-------------------------------------------------------+----------+
| FIELD NAME     | DESCRIPTION                                           | SIZE     |
+----------------+-------------------------------------------------------+----------+
| size           | sizeof data (in bytes)                                |  4 bytes |
+----------------+-------------------------------------------------------+----------+
| relatesTo      | offset of relatesTo data                              |  4 bytes |
+----------------+-------------------------------------------------------+----------+
| Buffer         | packet data                                           | Variable |
+----------------+-------------------------------------------------------+----------+
---*/


#pragma pack(push, 1)
#pragma warning(disable: 4200)  //  zero-sized array in struct/union (enabeld later)

//
// struct CSoapExtSection
//

class CSoapExtSection {
public:
    inline CSoapExtSection(
               const WCHAR *szFrom,
               const ULONG ccFrom,
               const WCHAR *szRelatesTo,
               const ULONG ccRelatesTo
           );

    static ULONG CalcSectionSize(
               const ULONG ccFrom,
               const ULONG ccRelatesTo
            );

    inline PCHAR GetNextSection(void) const;

    inline void  GetFrom(WCHAR * pBuffer, ULONG BufferLengthInWCHARs) const;
    inline const WCHAR *GetFrom(void);
    inline ULONG GetFromLengthInWCHARs(void);

    inline void  GetRelatesTo(WCHAR * pBuffer, ULONG BufferLengthInWCHARs) const;
    inline const WCHAR *GetRelatesTo(void);
    inline ULONG GetRelatesToLengthInWCHARs(void);
private:

//
// BEGIN Network Monitor tag
//
	ULONG m_dataLen;
	ULONG m_relatesToOffset;
    WCHAR m_buffer[0];
//
// END Network Monitor tag
//
};

#pragma warning(default: 4200)  //  zero-sized array in struct/union
#pragma pack(pop)


inline 
CSoapExtSection::CSoapExtSection(
               const WCHAR *szFrom,
               const ULONG ccFrom,
               const WCHAR *szRelatesTo,
               const ULONG ccRelatesTo
       )
{
    m_relatesToOffset = ccFrom;
    if (ccFrom)
        memcpy(&m_buffer[0],szFrom,ccFrom*sizeof(WCHAR));

    if (ccRelatesTo)
        memcpy(&m_buffer[m_relatesToOffset],szRelatesTo,ccRelatesTo*sizeof(WCHAR));

	m_dataLen = ccFrom + ccRelatesTo;
}

inline ULONG CSoapExtSection::CalcSectionSize(
               const ULONG ccFrom,
               const ULONG ccRelatesTo
        )
{
    size_t cbSize = sizeof(CSoapExtSection) + (ccFrom+ccRelatesTo)*sizeof(WCHAR);

    //
    // Align the entire header size to 4 bytes boundaries
    //
    cbSize = ALIGNUP4(cbSize);
    return static_cast<ULONG>(cbSize);
}


inline void  CSoapExtSection::GetFrom(WCHAR * pBuffer, ULONG BufferLengthInWCHARs) const 
{
    ULONG length = min(BufferLengthInWCHARs, m_relatesToOffset+1);
    if (length != 0)
    {
        memcpy(pBuffer, &m_buffer[0], (length-1) * sizeof(WCHAR));
        pBuffer[length - 1] = L'\0';
    }
}

inline const WCHAR * CSoapExtSection::GetFrom(void)
{
	return m_buffer;
}

inline ULONG CSoapExtSection::GetFromLengthInWCHARs(void)
{
	return m_relatesToOffset;
}

inline void  CSoapExtSection::GetRelatesTo(WCHAR * pBuffer, ULONG BufferLengthInWCHARs) const 
{
    ULONG length = min(BufferLengthInWCHARs,m_dataLen-m_relatesToOffset+1);
    if (length != 0)
    {
        memcpy(pBuffer, &m_buffer[m_relatesToOffset], (length-1) * sizeof(WCHAR));
        pBuffer[length - 1] = L'\0';
    }
}

inline const WCHAR * CSoapExtSection::GetRelatesTo(void)
{
	return m_buffer+m_relatesToOffset;
}


inline ULONG CSoapExtSection::GetRelatesToLengthInWCHARs(void) 
{
	return m_dataLen-m_relatesToOffset;
}

inline PCHAR CSoapExtSection::GetNextSection(void) const
{
    size_t cbSize = sizeof(CSoapSection) + (m_dataLen * sizeof(WCHAR));
    cbSize = ALIGNUP4(cbSize);

    return (PCHAR)this + cbSize;

}


#endif // __PHSOAPEXT
