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

#if !defined (__FSDACL_H__)
#define __FSDACL_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef UNDER_NT
WINBASEAPI
FARPROC
WINAPI
GetProcAddressW(
    __in HMODULE hModule,
    __in LPCWSTR lpProcName
    );

#if defined(DEBUG) || defined(_DEBUG)
#define ASSERT(exp)  ((void)((exp)?1:          \
       DebugBreak(), \
       0  \
       ))

#else
#define ASSERT(exp)   ((void) 0)
#endif // defined(DEBUG) || defined(_DEBUG)

#else
#include <cepolicy.h>
#endif

#if defined (__cplusplus)
extern "C"
{
#endif

// Psuedo-handle used to track an instance of the volume security manager.
//
typedef LPVOID HACLVOL;

static const WCHAR* FSDACL_CREATE_VOLUME = L"FSDACL_CreateVolume";
typedef LRESULT (*PFSDACL_CREATE_VOLUME) (
    IN HANDLE hVolume,
    IN const WCHAR* pVolumeName,
    IN DWORD MountFlags,
    OUT HACLVOL* phSecurity
    );

static const WCHAR* FSDACL_DELETE_VOLUME = L"FSDACL_DeleteVolume";
typedef void (*PFSDACL_DELETE_VOLUME) (
    IN HACLVOL hVolume
    );

static const WCHAR* FSDACL_LOCK_VOLUME = L"FSDACL_LockVolume";
typedef void (*PFSDACL_LOCK_VOLUME) (
    IN HACLVOL hVolume
    );

static const WCHAR * FSDACL_UNLOCK_VOLUME = L"FSDACL_UnlockVolume";
typedef void (*PFSDACL_UNLOCK_VOLUME) (
    IN HACLVOL hVolume
    );

static const WCHAR* FSDACL_ADD_REFERENCE = L"FSDACL_AddReference";
typedef void (*PFSDACL_ADD_REFERENCE) (
    IN HACLVOL hVolume
    );

static const WCHAR* FSDACL_REMOVE_REFERENCE = L"FSDACL_RemoveReference";
typedef void (*PFSDACL_REMOVE_REFERENCE) (
    IN HACLVOL hVolume
    );

static const WCHAR* FSDACL_FREE_SECURITY_DESCRIPTOR = L"FSDACL_FreeSecurityDescriptor";
typedef void (*PFSDACL_FREE_SECURITY_DESCRIPTOR) (
    IN HACLVOL hVolume,
    IN SECURITY_ATTRIBUTES* pSecurityAttributes,
    IN PSECURITY_DESCRIPTOR pSecurityDescriptor
    );

static const WCHAR* FSDACL_GET_FILE_SECURITY = L"FSDACL_GetFileSecurity";
typedef LRESULT (*PFSDACL_GET_FILE_SECURITY) (
    IN HACLVOL hVolume,
    IN HANDLE hToken,
    IN LPCTSTR lpFileName,
    IN SECURITY_INFORMATION RequestedInformation,
    OUT PSECURITY_DESCRIPTOR pSecurityDescriptor,
    IN DWORD SecurityDescriptorBytes,
    OUT LPDWORD pSecurityDescriptorBytesNeeded
    );

static const WCHAR* FSDACL_SET_FILE_SECURITY = L"FSDACL_SetFileSecurity";
typedef LRESULT (*PFSDACL_SET_FILE_SECURITY) (
    IN HACLVOL hVolume,
    IN HANDLE hToken,
    IN LPCTSTR lpFileName,
    IN SECURITY_INFORMATION SecurityInformation,
    IN PSECURITY_DESCRIPTOR pSecurityDescriptor,
    IN DWORD Length
    );

static const WCHAR* FSDACL_VOLUME_ACCESS_CHECK = L"FSDACL_VolumeAccessCheck";
typedef LRESULT (*PFSDACL_VOLUME_ACCESS_CHECK) (
    IN HACLVOL hVolume,
    IN const WCHAR* pFileName,
    IN DWORD ObjectType, // CE_OBJECT_TYPE_FILE or CE_OBJECT_TYPE_DIRECTORY
    IN HANDLE hToken,
    IN DWORD AccessMask, // 0 if not required
    IN DWORD ParentAccessMask, // 0 if not required
    IN DWORD FileAttributes,
    IN const SECURITY_ATTRIBUTES* pSecurityAttributes,
    OUT PSECURITY_DESCRIPTOR* ppSecurityDescriptor,
    OUT DWORD* pSecurityDescriptorSize
    );

#if defined (__cplusplus)
};
#endif

#if defined (__cplusplus)

class VolumeSecurityManager
{
public:

    VolumeSecurityManager () :
        m_pCreateVolume (NULL),
        m_pDeleteVolume (NULL),
        m_pLockVolume (NULL),
        m_pUnlockVolume (NULL),
        m_pAddReference (NULL),
        m_pRemoveReference (NULL),
        m_pFreeSecurityDescriptor (NULL),
        m_pGetFileSecurity (NULL),
        m_pSetFileSecurity (NULL),
        m_pVolumeAccessCheck (NULL),
        m_hDll (NULL)
    {
        ;
    }

    ~VolumeSecurityManager ()
    {
        if (m_hDll)
        {
            ::FreeLibrary (m_hDll);
            m_hDll = NULL;
        }
    }

    // Initialize the file security manager engine using the named DLL.
    inline LRESULT Initialize (
        const WCHAR* DllName
        )
    {
        if (m_hDll)
        {
            return ERROR_ALREADY_EXISTS;
        }

        m_hDll = ::LoadDriver (DllName);
        if (!m_hDll)
        {
            return ERROR_MOD_NOT_FOUND;
        }

        m_pCreateVolume = reinterpret_cast<PFSDACL_CREATE_VOLUME> (GetProcAddressW (m_hDll, FSDACL_CREATE_VOLUME));
        m_pDeleteVolume = reinterpret_cast<PFSDACL_DELETE_VOLUME> (GetProcAddressW (m_hDll, FSDACL_DELETE_VOLUME));
        m_pLockVolume = reinterpret_cast<PFSDACL_LOCK_VOLUME> (GetProcAddressW (m_hDll, FSDACL_LOCK_VOLUME));
        m_pUnlockVolume = reinterpret_cast<PFSDACL_UNLOCK_VOLUME> (GetProcAddressW (m_hDll, FSDACL_UNLOCK_VOLUME));
        m_pAddReference = reinterpret_cast<PFSDACL_ADD_REFERENCE> (GetProcAddressW (m_hDll, FSDACL_ADD_REFERENCE));
        m_pRemoveReference = reinterpret_cast<PFSDACL_REMOVE_REFERENCE> (GetProcAddressW (m_hDll, FSDACL_REMOVE_REFERENCE));
        m_pFreeSecurityDescriptor = reinterpret_cast<PFSDACL_FREE_SECURITY_DESCRIPTOR> (GetProcAddressW (m_hDll, FSDACL_FREE_SECURITY_DESCRIPTOR));
        m_pGetFileSecurity = reinterpret_cast<PFSDACL_GET_FILE_SECURITY> (GetProcAddressW (m_hDll, FSDACL_GET_FILE_SECURITY));
        m_pSetFileSecurity = reinterpret_cast<PFSDACL_SET_FILE_SECURITY> (GetProcAddressW (m_hDll, FSDACL_SET_FILE_SECURITY));
        m_pVolumeAccessCheck = reinterpret_cast<PFSDACL_VOLUME_ACCESS_CHECK> (GetProcAddressW (m_hDll, FSDACL_VOLUME_ACCESS_CHECK));

        if (!m_pCreateVolume || !m_pDeleteVolume || !m_pLockVolume ||
            !m_pUnlockVolume || !m_pAddReference || !m_pRemoveReference ||
            !m_pFreeSecurityDescriptor || !m_pGetFileSecurity ||
            !m_pSetFileSecurity || !m_pVolumeAccessCheck)
        {
            ::FreeLibrary (m_hDll);
            m_hDll = NULL;
            return ERROR_PROC_NOT_FOUND;
        }

        return NO_ERROR;
    }

    inline LRESULT CreateVolume (
        HANDLE hVolume,
        __in const WCHAR* pVolumeName,
        DWORD MountFlags,
        __out HACLVOL* phSecurity
        )
    {
        LRESULT lResult = ERROR_SUCCESS;
        if (m_hDll && m_pCreateVolume)
        {
            lResult = m_pCreateVolume (hVolume, pVolumeName, MountFlags, phSecurity);
        }
        return lResult;
    }

    inline void DeleteVolume (
        HACLVOL hVolumeSecurity
        )
    {
        if (IsSecurityEnabled (hVolumeSecurity))
        {
            ASSERT (m_hDll);
            ASSERT (m_pDeleteVolume);
            m_pDeleteVolume (hVolumeSecurity);
        }
    }

    inline void LockVolume (
        HACLVOL hVolumeSecurity
        )
    {
        if (IsSecurityEnabled (hVolumeSecurity))
        {
            ASSERT (m_hDll);
            ASSERT (m_pLockVolume);
            m_pLockVolume (hVolumeSecurity);
        }
    }

    inline void UnlockVolume (
        HACLVOL hVolumeSecurity
        )
    {
        if (IsSecurityEnabled (hVolumeSecurity))
        {
            ASSERT (m_hDll);
            ASSERT (m_pUnlockVolume);
            m_pUnlockVolume (hVolumeSecurity);
        }
    }

    inline void Acquire (
        HACLVOL hVolumeSecurity
        )
    {
        if (IsSecurityEnabled (hVolumeSecurity))
        {
            ASSERT (m_hDll);
            ASSERT (m_pAddReference);
            m_pAddReference (hVolumeSecurity);
        }
    }

    inline void Release (
        HACLVOL hVolumeSecurity
        )
    {
        if (IsSecurityEnabled (hVolumeSecurity))
        {
            ASSERT (m_hDll);
            ASSERT (m_pRemoveReference);
            m_pRemoveReference (hVolumeSecurity);
        }
    }

    inline void FreeSecurityDescriptor (
        HACLVOL hVolumeSecurity, 
        __in SECURITY_ATTRIBUTES* pSecurityAttributes,
        __in PSECURITY_DESCRIPTOR pSecurityDescriptor
        )
    {
        if (IsSecurityEnabled (hVolumeSecurity))
        {
            ASSERT (m_hDll);
            ASSERT (m_pFreeSecurityDescriptor);
            m_pFreeSecurityDescriptor (hVolumeSecurity, pSecurityAttributes, pSecurityDescriptor);
        }
    }

    inline LRESULT GetFileSecurity (
        HACLVOL hVolumeSecurity,
        HANDLE hToken,
        __in LPCTSTR lpFileName,
        SECURITY_INFORMATION RequestedInformation,
        __out_bcount_opt (SecurityDescriptorBytes) PSECURITY_DESCRIPTOR pSecurityDescriptor,
        DWORD SecurityDescriptorBytes,
        __out LPDWORD pSecurityDescriptorBytesNeeded
        )
    {
        LRESULT lResult = ERROR_NOT_SUPPORTED;
        if (IsSecurityEnabled (hVolumeSecurity))
        {
            ASSERT (m_hDll);
            ASSERT (m_pGetFileSecurity);
            lResult = m_pGetFileSecurity (hVolumeSecurity, hToken, lpFileName, RequestedInformation,
                pSecurityDescriptor, SecurityDescriptorBytes, pSecurityDescriptorBytesNeeded);
        }
        return lResult;
    }

    inline LRESULT SetFileSecurity (
        HACLVOL hVolumeSecurity,
        HANDLE hToken,
        __in LPCTSTR lpFileName,
        SECURITY_INFORMATION SecurityInformation,
        __in_bcount (Length) PSECURITY_DESCRIPTOR pSecurityDescriptor,
        DWORD Length
        )
    {
        LRESULT lResult = ERROR_NOT_SUPPORTED;
        if (IsSecurityEnabled (hVolumeSecurity))
        {
            ASSERT (m_hDll);
            ASSERT (m_pSetFileSecurity);
            lResult = m_pSetFileSecurity (hVolumeSecurity, hToken, lpFileName, SecurityInformation,
                pSecurityDescriptor, Length);
        }
        return lResult;
    }

    inline LRESULT VolumeAccessCheck (
        HACLVOL hVolumeSecurity,
        __in const WCHAR* pFileName,
        DWORD ObjectType,
        HANDLE hToken,
        DWORD AccessMask, // 0 if not required
        DWORD ParentAccessMask, // 0 if not required
        DWORD FileAttributes = INVALID_FILE_ATTRIBUTES,
        __in_opt const SECURITY_ATTRIBUTES* pSecurityAttributes = NULL,
        __out_opt PSECURITY_DESCRIPTOR* ppSecurityDescriptor = NULL,
        __out_opt DWORD* pSecurityDescriptorSize = NULL
        )
    {
        LRESULT lResult = NO_ERROR;
        if (IsSecurityEnabled (hVolumeSecurity))
        {
            ASSERT (m_hDll);
            ASSERT (m_pVolumeAccessCheck);
            lResult = m_pVolumeAccessCheck (hVolumeSecurity, pFileName, ObjectType, hToken, AccessMask, ParentAccessMask,
                FileAttributes, pSecurityAttributes, ppSecurityDescriptor, pSecurityDescriptorSize);
        }
        return lResult;
    }

    inline BOOL IsSecurityEnabled (
        HACLVOL hVolumeSecurity
        ) const
    {
        return hVolumeSecurity ? TRUE : FALSE;
    }

protected:
    HMODULE m_hDll;
    PFSDACL_CREATE_VOLUME m_pCreateVolume;
    PFSDACL_DELETE_VOLUME m_pDeleteVolume;
    PFSDACL_LOCK_VOLUME m_pLockVolume;
    PFSDACL_UNLOCK_VOLUME m_pUnlockVolume;
    PFSDACL_ADD_REFERENCE m_pAddReference;
    PFSDACL_REMOVE_REFERENCE m_pRemoveReference;
    PFSDACL_FREE_SECURITY_DESCRIPTOR m_pFreeSecurityDescriptor;
    PFSDACL_GET_FILE_SECURITY m_pGetFileSecurity;
    PFSDACL_SET_FILE_SECURITY m_pSetFileSecurity;
    PFSDACL_VOLUME_ACCESS_CHECK m_pVolumeAccessCheck;
};
#endif // __cplusplus

#endif // __FSDACL_H__
