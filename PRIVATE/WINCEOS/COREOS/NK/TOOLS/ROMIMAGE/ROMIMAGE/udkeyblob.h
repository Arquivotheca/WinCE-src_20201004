//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

#ifndef __UDKEYBLOB_H
#define __UDKEYBLOB_H

//-----------------------------------------------------------------------------
//
// File: UDKeyBlob.h
//
//
// Description:
//    Data structure to make manipulating the key blobs more maintainable.
//
// History:
// -@- 10/12/00 (mikemarr)  - created
// -@- 11/14/00 (mikemarr)  - added #include "uddbg.h"
// -@- 11/30/00 (mikemarr)  - removed dependency on uddbg.h
//                          - added InitRSAPublic
// -@- 01/20/01 (mikemarr)  - fixed IsValidDES to include legacy eBook
//
//-----------------------------------------------------------------------------

#include <windows.h>
#include <wincrypt.h>

// offsets into key blob
#define oBLOB           sizeof(BLOBHEADER)
#define oRSAMOD         (oBLOB + sizeof(RSAPUBKEY))
#define oDESHEADER      (oBLOB + sizeof(ALG_ID))

#define nKEY_SIZE_RSA512_PRIV   308
#define nKEY_SIZE_RSA512_PUB    84
#define nKEY_SIZE_RSA1024_PRIV  596
#define nKEY_SIZE_RSA1024_PUB   148
#define nKEY_SIZE_DES           (oDESHEADER + 8)
#define nMAX_KEY_SIZE           nKEY_SIZE_RSA1024_PRIV

// nMAGIC_RSAx - codes for the magic number in RSA key header
#define nMAGIC_RSA1     0x31415352      // public
#define nMAGIC_RSA2     0x32415352      // private

//
//	Comment on CALG_EBOOK_DES usage:
//  ===========================
//	Use uUSE_EBOOK_DES flag when calling UDCreateDESKey() and UDInitKeyFromBinary() 
//	to get non-standard legacy eBook DES encryption/decryption
//	In all other cases, use default uUSE_STANDARD_DES flag
//	CALG_EBOOK_DES / CALG_DES is written into the KeyBlob header 
//	and referenced by UDEncrypt() and UDDecrypt() functions

// Block cipher DES sub ids - defined similar to wincrypt.h
#define ALG_SID_EBOOK_DES 99
#define CALG_EBOOK_DES	    (ALG_CLASS_DATA_ENCRYPT|ALG_TYPE_BLOCK|ALG_SID_EBOOK_DES)

// default value (used in UDCreateDESKey, UDInitKeyFromBinary)
#define uUSE_STANDARD_DES   0x0000
// eBook legacy DES alternative (not compatible with the standard DES)
#define uUSE_EBOOK_DES      0x0001

class CKeyBlob
{
friend HRESULT UDWriteKeyToBinary(CKeyBlob *pKey, UINT uFlags, UINT *pcOut, BYTE *pbOut);
public:
                    CKeyBlob();
    HRESULT         InitDES(UINT cKeyBlob, BYTE *pbKeyBlob, UINT uFlags);
    HRESULT         InitRSA(UINT cKeyBlob, BYTE *pbKeyBlob);
    HRESULT         InitRSAPublic(UINT nExp, UINT nModBits, BYTE *pbModulus);

    BLOBHEADER *    Header()        { return (BLOBHEADER *) m_rgbData; }

    bool            IsValidRSAPublic();
    bool            IsValidRSAPrivate();
    bool            IsValidDES();
    
    // Treat key blob as RSA
    RSAPUBKEY *     RSAPubKey()     { return (RSAPUBKEY *) (m_rgbData + oBLOB); }
    UINT            RSAMagic()      { return RSAPubKey()->magic; }
    UINT            RSAModLen()     { return RSAPubKey()->bitlen/8; }
    UINT            RSAPubExp()     { return RSAPubKey()->pubexp; }
    BYTE *          RSAMod()        { return m_rgbData + oRSAMOD; }
    BYTE *          RSAPrime1()     { return RSAMod() +    RSAModLen(); }
    BYTE *          RSAPrime2()     { return RSAPrime1() + RSAModLen()/2; }
    BYTE *          RSAExp1()       { return RSAPrime2() + RSAModLen()/2; }
    BYTE *          RSAExp2()       { return RSAExp1() +   RSAModLen()/2; }
    BYTE *          RSACoeff()      { return RSAExp2() +   RSAModLen()/2; }
    BYTE *          RSAPrivExp()    { return RSACoeff() +  RSAModLen()/2; }

    // Treat key blob as DES
    ALG_ID *        DESEncryptAlg() { return (ALG_ID *) (m_rgbData + oBLOB); }
    BYTE *          DESKey()        { return m_rgbData + oDESHEADER; }

private:
    bool            IsValidHeader();
    void            WriteHeader(BYTE bType, ALG_ID aiKeyAlg);

public:
    BYTE            m_rgbData[nMAX_KEY_SIZE];
    UINT            m_cDataLen;
};

inline 
CKeyBlob::CKeyBlob()
{
    ::ZeroMemory(m_rgbData, nMAX_KEY_SIZE);
    m_cDataLen = 0;
}

inline void
CKeyBlob::WriteHeader(BYTE bType, ALG_ID aiKeyAlg)
{
    BLOBHEADER *pHeader = Header();
    pHeader->bType      = bType;
    pHeader->bVersion   = 0x02;
    pHeader->reserved   = 0;
    pHeader->aiKeyAlg   = aiKeyAlg;
}

inline HRESULT
CKeyBlob::InitDES(UINT cKeyBlob, BYTE *pbKeyBlob, UINT uFlags)
{
    if ((cKeyBlob == 0) || (pbKeyBlob == NULL))
        return E_INVALIDARG;

    HRESULT hr = S_OK;

    if (cKeyBlob == 8)
    {
        //
        // legacy 8 byte format
        //
        // write a CSP header into the blob
        // a flag to swith between EBOOK and STANDARD DES algorithm
        if (uFlags == uUSE_EBOOK_DES)
            this->WriteHeader(SIMPLEBLOB, CALG_EBOOK_DES);	// eBook variation of DES
        else
            this->WriteHeader(SIMPLEBLOB, CALG_DES);		// standard DES
      
        // we are not encrypting the DES key
        *(this->DESEncryptAlg()) = 0;
        // copy 56-bit DES key
        m_cDataLen = nKEY_SIZE_DES;
        ::CopyMemory(this->DESKey(), pbKeyBlob, 8);
    }
    else if (cKeyBlob == nKEY_SIZE_DES)
    {
        //
        // CSP blob header
        //
        m_cDataLen = nKEY_SIZE_DES;
        ::CopyMemory(m_rgbData, pbKeyBlob, cKeyBlob);
    }
    else
    {
        hr = NTE_BAD_KEY;
        goto e_Exit;
    }
    // verify the key
    if (!this->IsValidDES())
    {
        hr = NTE_BAD_KEY;
        goto e_Exit;
    }
e_Exit:
    return hr;
}

inline HRESULT
CKeyBlob::InitRSA(UINT cKeyBlob, BYTE *pbKeyBlob)
{
    if ((cKeyBlob < nKEY_SIZE_RSA512_PUB) || 
        (cKeyBlob > nKEY_SIZE_RSA1024_PRIV) || 
        (pbKeyBlob == NULL))
    {
        return E_INVALIDARG;
    }

    HRESULT hr = S_OK;

    // copy the blob
    m_cDataLen = cKeyBlob;
    ::CopyMemory(m_rgbData, pbKeyBlob, cKeyBlob);

    // verify the key
    if (!this->IsValidRSAPublic() && !this->IsValidRSAPrivate())
    {
        hr = NTE_BAD_KEY;
        goto e_Exit;
    }
e_Exit:
    return hr;
}

inline HRESULT
CKeyBlob::InitRSAPublic(UINT nExp, UINT nModBits, BYTE *pbModulus)
{
    switch(nModBits)
    {
    case 512:
        m_cDataLen = nKEY_SIZE_RSA512_PUB;
        break;
    case 1024:
        m_cDataLen = nKEY_SIZE_RSA1024_PUB;
        break;
    default:
        return E_INVALIDARG;
    }
    // write the blob header
    this->WriteHeader(PUBLICKEYBLOB, 0);

    // write the RSA public key header
    RSAPUBKEY *pPubKey = this->RSAPubKey();
    pPubKey->bitlen = nModBits;
    pPubKey->magic = nMAGIC_RSA1;
    pPubKey->pubexp = nExp;
    // copy the modulus
    ::CopyMemory(this->RSAMod(), pbModulus, nModBits/8);

    return S_OK;
}

inline bool
CKeyBlob::IsValidHeader()
{
    return ((Header()->bVersion == 0x02) && (Header()->reserved == 0));
}

inline bool
CKeyBlob::IsValidRSAPrivate()
{
    return (IsValidHeader() && 
            (m_cDataLen >= nKEY_SIZE_RSA512_PRIV) && (m_cDataLen <= nKEY_SIZE_RSA1024_PRIV) &&
            (RSAMagic() == nMAGIC_RSA2) &&
            (Header()->bType == PRIVATEKEYBLOB) &&
            ((RSAPubKey()->bitlen & 0x7) == 0) &&
            ((RSAModLen() == 128) || (RSAModLen() == 64)));
}

inline bool
CKeyBlob::IsValidRSAPublic()
{
    // RSA private keys are also valid public keys
    return (IsValidHeader() && 
            (m_cDataLen >= nKEY_SIZE_RSA512_PUB) && (m_cDataLen <= nKEY_SIZE_RSA1024_PRIV) &&
            (((RSAMagic() == nMAGIC_RSA1) && (Header()->bType == PUBLICKEYBLOB )) || 
             ((RSAMagic() == nMAGIC_RSA2) && (Header()->bType == PRIVATEKEYBLOB))) &&
            ((RSAPubKey()->bitlen & 0x7) == 0) &&
            ((RSAModLen() == 128) || (RSAModLen() == 64)));
}

inline bool
CKeyBlob::IsValidDES()
{
    return (IsValidHeader() && (m_cDataLen == nKEY_SIZE_DES) &&
            (Header()->bType == SIMPLEBLOB) &&
            ((Header()->aiKeyAlg == CALG_DES) || (Header()->aiKeyAlg == CALG_EBOOK_DES)));
}

#endif // #ifndef __UDKEYBLOB_H

