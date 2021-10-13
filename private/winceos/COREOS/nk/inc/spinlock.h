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
//
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#define MASTER_CPU      1

struct _SPINLOCK {
    DWORD       owner_cpu;              // which CPU own this spin lock
    DWORD       nestcount;              // nested count (for nested calls)
    DWORD       next_owner;              // can only be either 0 or MASTER_CPU, only updated by master CPU
};

void InitializeSpinLock (volatile SPINLOCK *psplock);

void AcquireSpinLock (volatile SPINLOCK *psplock);
void ReleaseSpinLock (volatile SPINLOCK *psplock);


void EnablePremption (volatile SPINLOCK *psplock);

typedef void (* PFN_SPINLOCKFUNC) (volatile SPINLOCK *psplock);

#ifdef IN_KERNEL
extern SPINLOCK g_schedLock;
extern SPINLOCK g_physLock;
extern SPINLOCK g_odsLock;
extern SPINLOCK g_oalLock;
#endif

#endif  // _SPINLOCK_H_
