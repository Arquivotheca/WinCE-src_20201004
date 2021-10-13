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

#include <diskio.h>
#include <cdioctl.h>

LRESULT CeOpenSGRequest (
    __out PSG_REQ* ppDuplicateSgReq,
    __in_bcount(InBufSize) const SG_REQ* pSgReq,
    DWORD InBufSize,
    DWORD ArgDescriptor, // ARG_I_PDW, ARG_O_PDW
    __out_ecount(PtrBackupCount) PBYTE* pPtrBackup,
    DWORD PtrBackupCount,
    DWORD BytesPerSector
    );

LRESULT CeCloseSGRequest (
    __in_bcount(InBufSize) SG_REQ* pDuplicateSgReq,
    __in_bcount(InBufSize) const SG_REQ* pSgReq,
    DWORD InBufSize,
    DWORD ArgDescriptor, // ARG_I_PDW, ARG_O_PDW
    __out_ecount(PtrBackupCount) PBYTE* pPtrBackup,
    DWORD PtrBackupCount
    );

LRESULT CeOpenSGXRequest (
    __out PCDROM_READ* ppDuplicateSgReq,
    __in_bcount(InBufSize) const CDROM_READ* pSgReq,
    DWORD InBufSize,
    DWORD ArgDescriptor, // ARG_I_PDW, ARG_O_PDW
    __out_ecount(PtrBackupCount) PBYTE* pPtrBackup,
    DWORD PtrBackupCount
    );

LRESULT CeCloseSGXRequest (
    __in_bcount(InBufSize) CDROM_READ* pDuplicateSgReq,
    __in_bcount(InBufSize) const CDROM_READ* pSgReq,
    DWORD InBufSize,
    DWORD ArgDescriptor, // ARG_I_PDW, ARG_O_PDW
    __out_ecount(PtrBackupCount) PBYTE* pPtrBackup,
    DWORD PtrBackupCount
    );

//
// Template classes for marshaling simple data types
//

// copy-in optional argument class
template <typename T>
class CopyInParam
{
public:

    CopyInParam(const T* pParam) :
        m_pParam(pParam)
    {
        if (m_pParam)
        {
            // exception may occur on copy-in
            m_Param = *m_pParam;
        }
    }

    const T* operator&() const
    {
        return m_pParam ? &m_Param : NULL;
    }

protected:

    T m_Param;
    const T* m_pParam;
};

// copy-out optional argument class
template <typename T>
class CopyOutParam
{
public:

    CopyOutParam(T* pParam) :
        m_pParam(pParam)
    {
        ;
    }

    ~CopyOutParam()
    {
        if (m_pParam)
        {
            // exception may occur on copy-out
            *m_pParam = m_Param;
        }
    }

    T* operator&()
    { 
        return m_pParam ? &m_Param : NULL;
    }

protected:
    T m_Param;
    T* m_pParam;
};

// copy-in/copy-out optional argument class, derived from copy-out class
template <typename T>
class CopyInOutParam : public CopyOutParam<T>
{
public:

    CopyInOutParam(T* pParam) :
        CopyOutParam(pParam)
    {
        if (m_pParam)
        {
            // exception may occur accessing on copy-in
            m_Param = *m_pParam;
        }
    }
};
