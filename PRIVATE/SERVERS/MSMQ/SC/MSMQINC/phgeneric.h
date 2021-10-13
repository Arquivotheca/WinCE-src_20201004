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

    Handle of generic class definition, holds WCHAR data.


--*/

#ifndef __PHGENERIC_H
#define __PHGENERIC_H

/*+++

    Header fields.

+----------------+-------------------------------------------------------+----------+
| FIELD NAME     | DESCRIPTION                                           | SIZE     |
+----------------+-------------------------------------------------------+----------+
| size           | sizeof data (in bytes)                                |  4 bytes |
+----------------+-------------------------------------------------------+----------+
| Buffer         | packet data (null terminated)                         | Variable |
+----------------+-------------------------------------------------------+----------+
---*/


#pragma pack(push, 1)
#pragma warning(disable: 4200)  //  zero-sized array in struct/union (enabeld later)

//
// struct CDataHeader
//

class CDataHeader {
public:
    inline CDataHeader(
               const WCHAR *pwcsData,
               const ULONG numWChars
           );

    inline CDataHeader(
               const CHAR *pcsData,
               const ULONG numChars
           );

    static ULONG CalcSectionSize(
               const ULONG numChars,
               const BOOL  fUnicode
            );

    inline PCHAR GetNextSection(void) const;

    inline CHAR *GetData(void);
    inline ULONG GetDataLengthInWCHARs(void);
    inline void SetDataLengthInWCHARs(ULONG dataLen);
    inline ULONG GetDataLengthInBytes(void);
    inline VOID GetData(UCHAR * pBuffer, ULONG BufferSizeInBytes) const;

private:

//
// BEGIN Network Monitor tag
//
	ULONG m_dataLen;
    CHAR  m_buffer[0];
//
// END Network Monitor tag
//
};

#pragma warning(default: 4200)  //  zero-sized array in struct/union
#pragma pack(pop)


inline 
CDataHeader::CDataHeader(
           const WCHAR *pwcsData,
           const ULONG numChars
       ) 
{
    m_dataLen  = numChars*sizeof(WCHAR);

    if (numChars)
        memcpy(&m_buffer[0],pwcsData,numChars*sizeof(WCHAR));

}

inline 
CDataHeader::CDataHeader(
           const CHAR *pcsData,
           const ULONG numChars
       ) 
{
    m_dataLen  = numChars;
    if (numChars)
        memcpy(&m_buffer[0],pcsData,numChars);
}

inline ULONG CDataHeader::CalcSectionSize(
           const ULONG numChars,
           const BOOL  fUnicode
        )
{
    int iMult = fUnicode ? sizeof(WCHAR) : sizeof(CHAR);
    size_t cbSize = sizeof(CDataHeader) + (numChars)*iMult;

    //
    // Align the entire header size to 4 bytes boundaries
    //
    cbSize = ALIGNUP4(cbSize);
    return static_cast<ULONG>(cbSize);
}

inline PCHAR CDataHeader::GetNextSection(VOID) const
{
    size_t cbSize = sizeof(CDataHeader) + m_dataLen;
    cbSize = ALIGNUP4(cbSize);

    return (PCHAR)this + cbSize;
}

inline CHAR* CDataHeader::GetData(void) 
{
    return &m_buffer[0];
}

inline VOID CDataHeader::GetData(UCHAR * pBuffer, ULONG BufferSizeInBytes) const
{
    ULONG size = min(BufferSizeInBytes, m_dataLen);

    if (size != 0)
    {
        memcpy(pBuffer, &m_buffer[0], size);
    }
} // CCompoundMessageHeader::GetData


	
inline ULONG CDataHeader::GetDataLengthInWCHARs(void)
{
	return m_dataLen / sizeof(WCHAR);
}

inline void CDataHeader::SetDataLengthInWCHARs(ULONG numWChars)
{
	m_dataLen = numWChars*sizeof(WCHAR);
}

inline ULONG CDataHeader::GetDataLengthInBytes(void)
{
	return m_dataLen;
}


#endif // __PHGENERIC_H
