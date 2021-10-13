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

    phsecure.h

Abstract:

    Handle Security section in Falcon Header

--*/

#ifndef __PHSECURE_H
#define __PHSECURE_H

#pragma pack(push, 1)
#pragma warning(disable: 4200)  //  zero-sized array in struct/union (enabeld later)

//
//  struct CSecurityHeader
//

struct CSecurityHeader {
public:

    inline CSecurityHeader();

    static ULONG CalcSectionSize(USHORT, USHORT, USHORT, ULONG, ULONG);
    inline PCHAR GetNextSection(void) const;

    inline void SetAuthenticated(BOOL);
    inline BOOL IsAuthenticated(void) const;

    inline void SetEncrypted(BOOL);
    inline BOOL IsEncrypted(void) const;

    inline void SetSenderIDType(USHORT wSenderID);
    inline USHORT GetSenderIDType(void) const;

    inline void SetSenderID(const UCHAR *pbSenderID, USHORT wSenderIDSize);
    inline const UCHAR* GetSenderID(USHORT* pwSize) const;

    inline void SetSenderCert(const UCHAR *pbSenderCert, ULONG ulSenderCertSize);
    inline const UCHAR* GetSenderCert(ULONG* pulSize) const;

    inline void SetEncryptedSymmetricKey(const UCHAR *pbEncryptedKey, USHORT wEncryptedKeySize);
    inline const UCHAR* GetEncryptedSymmetricKey(USHORT* pwSize) const;

    inline void SetSignature(const UCHAR *pbSignature, USHORT wSignatureSize);
    inline const UCHAR* GetSignature(USHORT* pwSize) const;

    inline void SetProvInfo(BOOL bDefProv, LPCWSTR wszProvName, ULONG dwPRovType);
    inline void GetProvInfo(BOOL *pbDefProv, LPCWSTR *wszProvName, ULONG *pdwPRovType) const;

private:
//
// BEGIN Network Monitor tag
//
    union {
        USHORT m_wFlags;
        struct {
            USHORT m_bfSenderIDType     : 4;
            USHORT m_bfAuthenticated    : 1;
            USHORT m_bfEncrypted        : 1;
            USHORT m_bfDefProv          : 1;
        };
    };
    USHORT m_wSenderIDSize;
    USHORT m_wEncryptedKeySize;
    USHORT m_wSignatureSize;
    ULONG  m_ulSenderCertSize;
    ULONG  m_ulProvInfoSize;
    UCHAR  m_abSecurityInfo[0];
//
// END Network Monitor tag
//
};

#pragma warning(default: 4200)  //  zero-sized array in struct/union
#pragma pack(pop)

/*=============================================================

 Routine Name:  CSecurityHeader::

 Description:

===============================================================*/
inline CSecurityHeader::CSecurityHeader():
    m_wFlags(0),
    m_wSenderIDSize(0),
    m_wEncryptedKeySize(0),
    m_wSignatureSize(0),
    m_ulSenderCertSize(0),
    m_ulProvInfoSize(0)
{
}

/*=============================================================

 Routine Name:  CSecurityHeader::

 Description:

===============================================================*/
inline
ULONG
CSecurityHeader::CalcSectionSize(
    USHORT wSenderIDSize,
    USHORT wEncryptedKeySize,
    USHORT wSignatureSize,
    ULONG  ulSenderCertSize,
    ULONG  ulProvInfoSize
    )
{
    if (!wSenderIDSize &&
        !wEncryptedKeySize &&
        !wSignatureSize &&
        !ulSenderCertSize &&
        !ulProvInfoSize)
    {
        return 0;
    }
    else
    {
        // If the message is signed, we must have also the user identity.
        return (
               sizeof(CSecurityHeader) +
               ALIGNUP4(wSenderIDSize) +
               ALIGNUP4(wEncryptedKeySize) +
               ALIGNUP4(wSignatureSize) +
               ALIGNUP4(ulSenderCertSize) +
               ALIGNUP4(ulProvInfoSize)
               );
    }
}

/*=============================================================

 Routine Name:  CSecurityHeader::

 Description:

===============================================================*/
inline PCHAR CSecurityHeader::GetNextSection(void) const
{
	// At least one of the security parameters should exist inorder to
    // have the security header, otherwise no need to include it in the
    // message.
    ASSERT(m_wSenderIDSize ||
           m_wEncryptedKeySize ||
           m_wSignatureSize ||
           m_ulSenderCertSize ||
           m_ulProvInfoSize);

    return (PCHAR)this +
                (
                sizeof(*this) +
                ALIGNUP4(m_wSenderIDSize) +
                ALIGNUP4(m_wEncryptedKeySize) +
                ALIGNUP4(m_wSignatureSize) +
                ALIGNUP4(m_ulSenderCertSize) +
                ALIGNUP4(m_ulProvInfoSize)
                );
}

/*=============================================================

 Routine Name:  CSecurityHeader::SetAuthenticated

 Description:   Set the authenticated bit

===============================================================*/
inline void CSecurityHeader::SetAuthenticated(BOOL f)
{
    m_bfAuthenticated = (USHORT)f;
}

/*=============================================================

 Routine Name:   CSecurityHeader::IsAuthenticated

 Description:    Returns TRUE if the msg is authenticated, False otherwise

===============================================================*/
inline BOOL
CSecurityHeader::IsAuthenticated(void) const
{
    return m_bfAuthenticated;
}

/*=============================================================

 Routine Name:  CSecurityHeader::SetEncrypted

 Description:   Set Encrypted message bit

===============================================================*/
inline void CSecurityHeader::SetEncrypted(BOOL f)
{
    m_bfEncrypted = (USHORT)f;
}

/*=============================================================

 Routine Name:   CSecurityHeader::IsEncrypted

 Description:    Returns TRUE if the msg is Encrypted, False otherwise

===============================================================*/
inline BOOL CSecurityHeader::IsEncrypted(void) const
{
    return m_bfEncrypted;
}

/*=============================================================

 Routine Name:  CSecurityHeader::SetSenderIDType

 Description:

===============================================================*/
inline void CSecurityHeader::SetSenderIDType(USHORT wSenderID)
{
    ASSERT(wSenderID < 16); // There are four bits for the user ID type.
    m_bfSenderIDType = wSenderID;
}

/*=============================================================

 Routine Name:  CSecurityHeader::GetSenderIDType

 Description:

===============================================================*/
inline USHORT CSecurityHeader::GetSenderIDType(void) const
{
    return m_bfSenderIDType;
}

/*=============================================================

 Routine Name:  CSecurityHeader::SetSenderID

 Description:

===============================================================*/
inline void CSecurityHeader::SetSenderID(const UCHAR *pbSenderID, USHORT wSenderIDSize)
{
    // Should set the user identity BEFORE setting the encription and
    // authentication sections.
    ASSERT(!m_wEncryptedKeySize &&
           !m_wSignatureSize &&
           !m_ulSenderCertSize &&
           !m_ulProvInfoSize);
    m_wSenderIDSize = wSenderIDSize;
    memcpy(m_abSecurityInfo, pbSenderID, wSenderIDSize);
}

/*=============================================================

 Routine Name:  CSecurityHeader::GetSenderID

 Description:

===============================================================*/
inline const UCHAR* CSecurityHeader::GetSenderID(USHORT* pwSize) const
{
    *pwSize = m_wSenderIDSize;
    return m_abSecurityInfo;
}

/*=============================================================

 Routine Name:

 Description:

===============================================================*/
inline
void
CSecurityHeader::SetEncryptedSymmetricKey(
    const UCHAR *pbEncryptedKey,
    USHORT wEncryptedKeySize
    )
{
    // Should set the encryption section BEFORE setting the authentication
    // section.
    ASSERT(m_wEncryptedKeySize ||
           (!m_wSignatureSize && !m_ulSenderCertSize && !m_ulProvInfoSize));
    ASSERT(!m_wEncryptedKeySize || (m_wEncryptedKeySize == wEncryptedKeySize));
    m_wEncryptedKeySize = wEncryptedKeySize;
    //
    // It is possible to call this function with no buffer for the encrypted
    // key. This is done by the device driver. the device driver only makes
    // room in the security header for the symmetric key. The QM writes
    // the symmetric key in the security header after encrypting the message
    // body.
    //
    if (pbEncryptedKey)
    {
        memcpy(
            &m_abSecurityInfo[ALIGNUP4(m_wSenderIDSize)],
            pbEncryptedKey,
            wEncryptedKeySize);
    }
}

/*=============================================================

 Routine Name:

 Description:

===============================================================*/
inline const UCHAR* CSecurityHeader::GetEncryptedSymmetricKey(USHORT* pwSize) const
{
    *pwSize = m_wEncryptedKeySize;
    return &m_abSecurityInfo[ALIGNUP4(m_wSenderIDSize)];
}
/*=============================================================

 Routine Name:   CSecurityHeader::SetSignature

 Description:

===============================================================*/
inline void CSecurityHeader::SetSignature(const UCHAR *pbSignature, USHORT wSignatureSize)
{
    ASSERT(!m_ulSenderCertSize && !m_ulProvInfoSize);
    m_wSignatureSize = wSignatureSize;
    memcpy(
        &m_abSecurityInfo[ALIGNUP4(m_wSenderIDSize) +
                          ALIGNUP4(m_wEncryptedKeySize)],
        pbSignature,
        wSignatureSize
        );
}

/*=============================================================

 Routine Name:  CSecurityHeader::GetSignature

 Description:

===============================================================*/
inline const UCHAR* CSecurityHeader::GetSignature(USHORT* pwSize) const
{
    *pwSize = m_wSignatureSize;
    return &m_abSecurityInfo[ALIGNUP4(m_wSenderIDSize) +
                             ALIGNUP4(m_wEncryptedKeySize)];
}

/*=============================================================

 Routine Name:  CSecurityHeader::SetSenderCert

 Description:

===============================================================*/
inline void CSecurityHeader::SetSenderCert(const UCHAR *pbSenderCert, ULONG ulSenderCertSize)
{
    // Should set the user identity BEFORE setting the encription and
    // authentication sections.
    ASSERT(!m_ulProvInfoSize);
    m_ulSenderCertSize = ulSenderCertSize;
    memcpy(&m_abSecurityInfo[ALIGNUP4(m_wSenderIDSize) +
                             ALIGNUP4(m_wEncryptedKeySize) +
                             ALIGNUP4(m_wSignatureSize)],
           pbSenderCert,
           ulSenderCertSize);
}

/*=============================================================

 Routine Name:  CSecurityHeader::GetSenderCert

 Description:

===============================================================*/
inline const UCHAR* CSecurityHeader::GetSenderCert(ULONG* pulSize) const
{
    *pulSize = m_ulSenderCertSize;
    return &m_abSecurityInfo[ALIGNUP4(m_wSenderIDSize) +
                             ALIGNUP4(m_wEncryptedKeySize) +
                             ALIGNUP4(m_wSignatureSize)];
}

/*=============================================================

 Routine Name:  CSecurityHeader::SetProvInfo

 Description:

===============================================================*/
inline
void
CSecurityHeader::SetProvInfo(
    BOOL bDefProv,
    LPCWSTR wszProvName,
    ULONG ulProvType)
{
    m_bfDefProv = (USHORT)bDefProv;
    if(!m_bfDefProv)
    {
        //
        // We fill the provider info only if this is not the default provider.
        //
        UCHAR *pProvInfo =
             &m_abSecurityInfo[ALIGNUP4(m_wSenderIDSize) +
                               ALIGNUP4(m_wEncryptedKeySize) +
                               ALIGNUP4(m_wSignatureSize) +
                               ALIGNUP4(m_ulSenderCertSize)];

        //
        // Write the provider type.
        //
        *(ULONG *)pProvInfo = ulProvType;
        pProvInfo += sizeof(ULONG);

        //
        // Write the provider name.
        //
        wcscpy((WCHAR*)pProvInfo, wszProvName);

        //
        // Compute the size of the provider information.
        //
        m_ulProvInfoSize = (wcslen(wszProvName) + 1) * sizeof(WCHAR) + sizeof(ULONG);
    }
}

/*=============================================================

 Routine Name:  CSecurityHeader::GetProvInfo

 Description:

===============================================================*/
inline
void
CSecurityHeader::GetProvInfo(
    BOOL *pbDefProv,
    LPCWSTR *wszProvName,
    ULONG *pulProvType) const
{
    *pbDefProv = m_bfDefProv;
    if(!m_bfDefProv)
    {
        //
        // We fill the provider type and name only if this is not the default
        // provider.
        //
        ASSERT(m_ulProvInfoSize);
        const UCHAR *pProvInfo =
             &m_abSecurityInfo[ALIGNUP4(m_wSenderIDSize) +
                               ALIGNUP4(m_wEncryptedKeySize) +
                               ALIGNUP4(m_wSignatureSize) +
                               ALIGNUP4(m_ulSenderCertSize)];

        //
        // Fill the provider type.
        //
        *pulProvType = *(ULONG *)pProvInfo;
        pProvInfo += sizeof(ULONG);

        //
        // Fill the provider name.
        //
        *wszProvName = (WCHAR*)pProvInfo;
    }
}

#endif // __PHSECURE_H
